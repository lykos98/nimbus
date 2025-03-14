#include "esp32s2/ulp.h"
#include "driver/rtc_io.h"
#include "hal/gpio_types.h"
#include "soc/rtc_io_reg.h"
#include "esp32_utilities.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"

#define GPIO_SENSOR_PIN GPIO_NUM_12
#define RTC_GPIO_IDX rtc_io_number_get(GPIO_SENSOR_PIN)

// Define the ULP counter variable in RTC memory

enum {
  COUNTER,
  OLD_STATE,
  PROGRAM_OFFSET,
};

enum {
  LABEL_MAIN,
  LABEL_INCREMENT_COUNTER,
  LABEL_NO_INCREMENT,
  LABEL_DELAY_START,
  LABEL_DELAY_LOOP,
  LABEL_DELAY_END
};

const ulp_insn_t ulp_program[] = {
  // Load ulp_wakeup_counter address into R0

  //prevent restart after halt
  //I_WR_REG_BIT(RTC_CNTL_STATE0_REG, 24,0),

  //init state 
  I_RD_REG( RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX, RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX),
  //set old state == new state
  I_MOVR(R1,R0),

  M_LABEL(LABEL_MAIN),

  //read state into R0
  I_RD_REG( RTC_GPIO_IN_REG, 
            RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX, 
            RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX),
  

  
  //compare with R1
  I_SUBR(R1,R0,R1),
  //jump if old state == new state
  M_BXZ(LABEL_NO_INCREMENT),
  
  //if not increment counter and, loop back

  M_LABEL(LABEL_INCREMENT_COUNTER),
  // Save in R2 the pointer
  I_MOVI(R2, COUNTER),
  // Load the value of the counter from memory to R3
  I_LD(R3, R2, 0),
  // Increment the counter
  I_ADDI(R3, R3, 1),
  // Store the updated counter back to memory
  I_ST(R3, R2, 0),
  // Update the state
  I_MOVR(R1,R0),
    //wait for current stabilization//
  M_BX(LABEL_DELAY_START),
  M_LABEL(LABEL_DELAY_END),
  
  M_BX(LABEL_MAIN),

  M_LABEL(LABEL_NO_INCREMENT),
  I_MOVR(R1,R0),
  M_BX(LABEL_MAIN),


  M_LABEL(LABEL_DELAY_START),
  I_MOVR(R3,R0),
  I_MOVI(R0,0),
  M_LABEL(LABEL_DELAY_LOOP),
  I_ADDI(R0,R0,1),
  I_DELAY(0xFFFF),
  //loop 100 times
  M_BL(LABEL_DELAY_LOOP,5),
  I_MOVR(R0,R3),
  M_BX(LABEL_DELAY_END)
};


const ulp_insn_t ulp_program_d[] = {
  //init state 
  // read state
  M_LABEL(1),
  I_RD_REG( RTC_GPIO_IN_REG, 
          RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX, 
          RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX),
  I_DELAY(0xFFFF),

  
   
  I_MOVI(R2, OLD_STATE),
  // old state
  I_LD(R1, R2, 0),

  I_SUBR(R1, R1, R0),
  M_BXZ(0),
  I_ST(R0, R2, 0),
  I_MOVI(R2, COUNTER),
  I_LD(R0, R2, 0),
  I_ADDI(R0, R0, 1),
  I_ST(R0,R2,0),
  //I_HALT(),
  M_BX(1),

  M_LABEL(0),
  //I_HALT()
  M_BX(1)
};

const ulp_insn_t ulp_program_e[] = {
  //init state 
  // read state
  I_RD_REG( RTC_GPIO_IN_REG, 
          RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX, 
          RTC_GPIO_IN_NEXT_S + RTC_GPIO_IDX),
  I_DELAY(0xFFFF),

  
   
  I_MOVI(R2, OLD_STATE),
  I_MOVI(R1,1),
  I_SUBR(R1,R1,R0),

  I_ST(R0,R2,0),


};
// Function to start the ULP program
void startULP() {

  // Load ULP program
  
  size_t size = sizeof(ulp_program) / sizeof(ulp_insn_t);
  auto err = ulp_process_macros_and_load(PROGRAM_OFFSET, ulp_program, &size);
  //Serial.println(err);
  // Set ULP wake-up period (e.g., every 1000ms)

  //ulp_set_wakeup_period(0, 1); // Wake up every second

  // Start the ULP program
  ulp_run(PROGRAM_OFFSET);

}
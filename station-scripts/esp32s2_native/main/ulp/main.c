#include "../config.h"
#include <stdint.h>
#include <stdbool.h>
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "ulp_riscv_gpio.h"

volatile uint32_t gpio_level = false;
volatile uint32_t gpio_level_previous = false;
volatile uint32_t rotations = 0;
volatile uint32_t is_active = 0;
volatile uint32_t debounce = 0;
volatile uint32_t ulp_initialized = 0;

// version 1
int main (void)
{
    gpio_level = (bool)ulp_riscv_gpio_get_level(ANEMOMETER_PIN);
    gpio_level_previous = gpio_level;

    while(1) {

        is_active = 117;
        gpio_level = (bool)ulp_riscv_gpio_get_level(ANEMOMETER_PIN);

        if(gpio_level != gpio_level_previous) {
            gpio_level_previous = gpio_level;
            rotations++;
        }
        ulp_riscv_delay_cycles(40000);
    }
    /* ulp_riscv_halt() is called automatically when main exits */
    return 0;
}

// version 2
// int main (void) {
//     uint32_t current_level = ulp_riscv_gpio_get_level(ANEMOMETER_PIN);
//     is_active = 117;
//
//     if (ulp_initialized == 1) {
//         if (current_level != gpio_level_previous) {
//             rotations++;
//         }
//     } else {
//         ulp_initialized = 1;
//     }
//     gpio_level_previous = current_level;
//
//     return 0; // Trigger Halt
// }


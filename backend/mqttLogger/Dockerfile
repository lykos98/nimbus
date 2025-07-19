FROM python:3.12-slim-bookworm

WORKDIR /usr/local/app

# Install the application dependencies
COPY ./mqttLogger.py ./mqttLogger.py
COPY ./requirements.txt ./requirements.txt
RUN pip install --no-cache-dir -r requirements.txt
RUN mkdir logs

# Setup an app user so the container doesn't run as the root user
RUN useradd app
USER app

CMD ["python3", "-u", "mqttLogger.py"]

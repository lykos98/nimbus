FROM python:3.12-slim-bookworm

WORKDIR /usr/local/app

# Install the application dependencies
COPY ./dash ./dash
RUN pip install --no-cache-dir -r dash/requirements.txt

EXPOSE 8050

# Setup an app user so the container doesn't run as the root user
RUN useradd app
USER app
WORKDIR dash

CMD ["gunicorn", "-b", "0.0.0.0:8050", "app:server"]

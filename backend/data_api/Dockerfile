# Use an official Python runtime as a base image
FROM python:3.9-slim

# Set the working directory in the container
WORKDIR /app

# Copy the requirements file and install dependencies
COPY requirements.txt requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

# Copy the Flask app and certificate into the container
COPY *.py /app/.

# Expose port 5000 for the Flask app
EXPOSE 5555

# Run the Flask app
CMD ["gunicorn", "--bind", "0.0.0.0:5555","--log-level", "debug", "app:app"]


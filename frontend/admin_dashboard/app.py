from flask import Flask, render_template, request
import requests
import os

app = Flask(__name__, static_folder='static', template_folder='static')

# The internal URL for the data-api service
DATA_API_URL = os.getenv("DATA_API_INTERNAL_URL", "http://data-api:5555")

@app.route('/api/<path:path>', methods=['GET', 'POST', 'PUT', 'DELETE'])
def proxy_api(path):
    """A reverse proxy to forward API requests to the data-api service."""
    
    # Construct the full URL for the data-api
    url = f"{DATA_API_URL}/api/{path}"
    
    # Copy relevant headers from the incoming request
    headers = {key: value for (key, value) in request.headers if key != 'Host'}
    
    # Forward the request to the data-api
    try:
        resp = requests.request(
            method=request.method,
            url=url,
            headers=headers,
            data=request.get_data(),
            cookies=request.cookies,
            allow_redirects=False
        )
        
        # Exclude certain headers from the response
        excluded_headers = ['content-encoding', 'content-length', 'transfer-encoding', 'connection']
        headers = [(name, value) for (name, value) in resp.raw.headers.items()
                   if name.lower() not in excluded_headers]

        # Return the response from the data-api to the client
        return (resp.content, resp.status_code, headers)

    except requests.exceptions.RequestException as e:
        # Handle connection errors or other request issues
        raise
        app.logger.error(f"Proxy request failed: {e}")
        return ("Proxy Error: Could not connect to the backend service.", 502)

@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')

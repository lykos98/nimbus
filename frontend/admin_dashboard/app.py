from flask import Flask, render_template, request, Response
import requests
import os

app = Flask(__name__, static_folder='static', template_folder='static')

# The internal URL for the data-api service
DATA_API_URL = os.getenv("DATA_API_INTERNAL_URL", "http://data-api:5555")

@app.route('/api/<path:path>', methods=['GET', 'POST', 'PUT', 'DELETE'])
def proxy_api(path):
    """A reverse proxy to forward API requests to the data-api service."""
    
    url = f"{DATA_API_URL}/api/{path}"
    
    # Forward headers from the client, except for the Host header.
    fwd_headers = {key: value for (key, value) in request.headers if key.lower() != 'host'}

    try:
        # Use stream=True to get the raw response without auto-decompression
        resp = requests.request(
            method=request.method,
            url=url,
            headers=fwd_headers,
            data=request.get_data(),
            cookies=request.cookies,
            allow_redirects=False,
            stream=True
        )

        # Exclude hop-by-hop headers that should not be forwarded.
        excluded_headers = ['transfer-encoding', 'connection']
        headers = [(name, value) for (name, value) in resp.raw.headers.items()
                   if name.lower() not in excluded_headers]

        # Create a Flask response that streams the raw content and forwards the original headers.
        # This correctly passes through gzipped content to the browser.
        response = Response(resp.iter_content(chunk_size=1024), resp.status_code, headers)
        
        return response

    except requests.exceptions.RequestException as e:
        # Handle connection errors to the backend service.
        app.logger.error(f"Proxy request failed: {e}")
        return ("Proxy Error: Could not connect to the backend service.", 502)


@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0')

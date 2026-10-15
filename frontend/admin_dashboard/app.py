from flask import Flask, render_template, request, Response
import requests
import os

app = Flask(__name__, static_folder='static', template_folder='static')

# The internal URL for the data-api service
DATA_API_URL = os.getenv("DATA_API_INTERNAL_URL", "http://data-api:5555")
# Upstream timeout (connect, read) — prevents hanging workers on backend outage.
PROXY_TIMEOUT = float(os.getenv("PROXY_TIMEOUT_SECONDS", "10"))

# Hop-by-hop + framing headers that must never be forwarded verbatim.
# content-encoding/content-length are excluded because requests already decodes
# gzip/deflate in iter_content(); forwarding them would corrupt the body.
EXCLUDED_HEADERS = {
    'connection', 'transfer-encoding', 'content-encoding',
    'content-length', 'keep-alive', 'proxy-authenticate',
    'proxy-authorization', 'te', 'trailer', 'upgrade',
}


@app.route('/api/<path:path>', methods=['GET', 'POST', 'PUT', 'DELETE', 'PATCH', 'OPTIONS'])
def proxy_api(path):
    """A reverse proxy to forward API requests to the data-api service."""

    # Preserve the client's query string (was silently dropped before).
    qs = request.query_string.decode('latin-1')
    url = f"{DATA_API_URL}/api/{path}"
    if qs:
        url = f"{url}?{qs}"

    # Forward headers from the client, except Host (and Content-Length, which
    # requests recalculates from the body we pass).
    # NOTE: 'cookie' is excluded here because cookies are passed explicitly via
    # the cookies= param below — forwarding both would duplicate the header.
    # Set-Cookie on the way back is preserved (not in EXCLUDED_HEADERS).
    fwd_headers = {
        key: value for (key, value) in request.headers
        if key.lower() not in ('host', 'content-length', 'cookie')
    }

    try:
        resp = requests.request(
            method=request.method,
            url=url,
            headers=fwd_headers,
            params=None,  # query string already appended above
            data=request.get_data(),
            cookies=request.cookies,
            allow_redirects=False,
            stream=True,
            timeout=PROXY_TIMEOUT,
        )

        headers = [(name, value) for (name, value) in resp.raw.headers.items()
                   if name.lower() not in EXCLUDED_HEADERS]

        # Stream the (already-decoded) body with the original status code.
        return Response(resp.iter_content(chunk_size=8192), status=resp.status_code, headers=headers)

    except requests.exceptions.Timeout:
        app.logger.error("Proxy request timed out: %s %s", request.method, path)
        return ("Proxy Error: backend service timed out.", 504)
    except requests.exceptions.RequestException as e:
        # Handle connection errors to the backend service.
        app.logger.error(f"Proxy request failed: {e}")
        return ("Proxy Error: Could not connect to the backend service.", 502)


@app.route('/')
def index():
    return render_template('index.html')

if __name__ == '__main__':
    # Never enable debug in a container entrypoint; use FLASK_DEBUG=1 locally if needed.
    app.run(debug=os.getenv("FLASK_DEBUG", "0") == "1", host='0.0.0.0')

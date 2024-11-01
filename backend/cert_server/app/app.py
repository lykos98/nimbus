from flask import Flask, jsonify, send_from_directory, abort
import hashlib

app = Flask(__name__)

CERTIFICATE_PATH = '/app/certs'
CERTIFICATE_NAME = 'ca.crt'

def get_certificate_hash():
    try:
        with open(f"{CERTIFICATE_PATH}/{CERTIFICATE_NAME}", "rb") as cert_file:
            cert_data = cert_file.read()
            return hashlib.sha256(cert_data).hexdigest()
    except FileNotFoundError:
        return None

@app.route('/ca-cert')
def serve_ca_cert():
    try:
        return send_from_directory(CERTIFICATE_PATH, CERTIFICATE_NAME, as_attachment=True)
    except FileNotFoundError:
        abort(404)

@app.route('/ca-cert-hash')
def serve_ca_cert_hash():
    cert_hash = get_certificate_hash()
    if cert_hash:
        return jsonify({"hash": cert_hash})
    else:
        abort(404)

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True)

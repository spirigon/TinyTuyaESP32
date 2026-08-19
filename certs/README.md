# HTTPS root CA

Production uses the public `148-253-213-23.sslip.io` hostname and a Let's
Encrypt certificate. Download the self-signed ISRG Root X1 PEM from
`https://letsencrypt.org/certs/isrgrootx1.pem` to
`certs/isrg-root-x1.pem`. The certificate is embedded into the ESP32 firmware by
`scripts/load_env.py`; local certificate files are ignored by Git.

For a different deployment, place that endpoint's trusted root CA in this
directory and point `FORWARD_CA_CERT_FILE` at it. Never copy a CA private key.

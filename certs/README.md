# Caddy root CA

Place the public root certificate from the production Caddy instance at
`certs/caddy-root-ca.crt`. The certificate is embedded into the ESP32 firmware
by `scripts/load_env.py`; the local certificate file is ignored by Git.

Do not copy Caddy's root private key.

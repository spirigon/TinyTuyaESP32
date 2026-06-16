# Tests

The requested host-side regression tests are not wired yet. Recommended next tests:

- `test_protocol.c`: 55AA CRC framing and 6699 length/AAD checks with captured packets.
- `test_crypto.c`: AES-ECB padding, AES-GCM tag verification, HMAC-SHA256, MD5 slice for v3.1 control.
- `test_session.c`: v3.4 and v3.5 session-key negotiation with fixed local/remote nonces.
- `test_json.cpp`: compact JSON payload generation for status and control.

Real-device captures should be placed in `tests/captures/` with expected decoded JSON.

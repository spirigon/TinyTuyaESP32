# Tests

Run host-side regression tests:

```bash
python tests/run_host_tests.py
```

The host runner currently covers:

- `test_crypto.c`: CRC32 known vectors via the library CRC implementation.
- `test_protocol.c`: 55AA plain framing and unpacking.

Recommended next tests:

- 6699 length/AAD checks with captured packets.
- AES-ECB padding, AES-GCM tag verification, HMAC-SHA256, MD5 slice for v3.1 control.
- v3.4 and v3.5 session-key negotiation with fixed local/remote nonces.
- compact JSON payload generation for status and control.

Real-device captures should be placed in `tests/captures/` with expected decoded JSON.

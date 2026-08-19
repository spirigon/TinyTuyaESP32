# TinyTuyaESP32

TinyTuyaESP32 is an ESP32 Arduino LAN client for Tuya Wi-Fi devices. It is a C/C++ port scaffold based on `jasonacox/tinytuya` v1.18.1, focused on local discovery, `DP_QUERY`, `CONTROL`, heartbeat, and protocol revisions 3.1, 3.2, 3.3, 3.4, and 3.5.

## Status

Implemented:

- 55AA framing with CRC32 and v3.4 HMAC-SHA256.
- 6699 framing with AES-GCM and authenticated AAD.
- AES-ECB, AES-GCM, HMAC-SHA256, MD5, base64, and UDP discovery key via mbedTLS.
- v3.4/v3.5 session-key negotiation helpers.
- Sync Arduino API: `tuya_status()`, `tuya_set_bool()`, `tuya_set_int()`, `tuya_set_string()`, `tuya_heartbeat()`.
- UDP discovery listeners on 6666, 6667, and 7000, plus v3.5 `REQ_DEVINFO` solicitation.
- Arduino wrappers: `TinyTuya`, `TuyaScanner`, `TinyTuyaFsm`, `TinyTuyaAsync`, and `TinyTuyaMulti`.
- Cooperative multi-device polling with independent sockets/session keys per device.
- Task-backed single-device async queue with `TinyTuyaAsync`.
- Host-side regression test runner and GitHub Actions CI build matrix.

Known gaps:

- `TinyTuyaFsm` and `TinyTuyaMulti` schedule sync operations from `loop()`; they are cooperative schedulers, not true non-blocking TCP state machines. Use `TinyTuyaAsync` when the Arduino loop task must not run Tuya TCP roundtrips.
- `TinyTuyaAsync` runs the existing sync protocol implementation on a worker task; it is not an AsyncTCP/lwIP callback-level transport rewrite.
- Zigbee gateway child-device support is reserved but not implemented.
- Hardware regression captures and generated Doxygen HTML are not included yet.

## Install

Copy this folder into your Arduino libraries directory, or use it as a PlatformIO project/library.

PlatformIO dependency:

```ini
lib_deps =
    bblanchon/ArduinoJson @ ^7
```

## Usage

```cpp
#include <WiFi.h>
#include <tinytuya.h>

TinyTuya dev("DEVICE_ID", "192.168.1.42", "0123456789abcdef");

void setup() {
  Serial.begin(115200);
  WiFi.begin("SSID", "PASS");
  while (WiFi.status() != WL_CONNECTED) delay(250);

  dev.setVersion(3.3);
  String status;
  if (dev.status(status) == TUYA_OK) {
    Serial.println(status);
  }
  dev.setBool(1, true);
}

void loop() {}
```

Cooperative multi-device polling:

```cpp
TinyTuyaMulti tuya(2);

void onTuya(size_t index, const char *id, tuya_event_t event,
            const char *payload, tuya_err_t err, void *user) {
  if (event == TUYA_EVENT_STATUS_RECEIVED && payload) Serial.println(payload);
}

void setup() {
  WiFi.begin("SSID", "PASS");
  while (WiFi.status() != WL_CONNECTED) delay(250);

  tuya.setCallback(onTuya);
  tuya.addDevice("DEVICE_ID_1", "192.168.1.42", "0123456789abcdef", 3.4);
  tuya.addDevice("DEVICE_ID_2", "192.168.1.43", "fedcba9876543210", 3.4);
}

void loop() {
  tuya.loop();
}
```

Examples:

- `examples/discovery.ino`
- `examples/poll_status.ino`
- `examples/control_relay.ino`
- `examples/verify_relay.ino`
- `examples/multi_device.ino`
- `examples/async_fsm.ino`
- `examples/async_task.ino`
- `examples/push_updates.ino`
- `examples/validate_device.ino`
- `examples/http_forward.ino`

Generic HTTPS forwarding example:

`examples/http_forward.ino` polls one Tuya device and POSTs a JSON envelope to
`TINYTUYA_FORWARD_SERVER_URL`. It validates the server against the configured
Caddy root CA and sends the receiver token in `X-ESP32-Token`. It is intentionally
self-contained example code, not a library API. To try the receiver locally, run:

```bash
python examples/tools/http_json_receiver.py --port 28080
```

For production, copy the public Caddy root certificate to
`certs/caddy-root-ca.crt`, then set these `.env` values:

```text
FORWARD_SERVER_URL=https://VPS_IP/tuya
FORWARD_TLS_HOSTNAME=esp32-receiver.invalid
FORWARD_TOKEN=replace_with_receiver_token
FORWARD_CA_CERT_FILE=certs/caddy-root-ca.crt
FORWARD_INTERVAL_MS=30000
```

Arduino-ESP32 2.x cannot validate an `iPAddress` subjectAltName. The forwarding
client therefore connects to the IP in `FORWARD_SERVER_URL` while validating
the Caddy certificate against `FORWARD_TLS_HOSTNAME`. This does not perform a
DNS lookup. Configure Caddy to issue an internal certificate for the same
logical hostname and to accept the IP-valued HTTP Host header.

Run host-side tests:

```bash
python tests/run_host_tests.py
```

## Notes

- Set the correct protocol version from discovery output or your device inventory.
- Protocol 3.2 is treated as a device22-style 3.3 variant.
- Protocol 3.4 and 3.5 negotiate a per-connection session key; reconnects renegotiate.
- `local_key` and `session_key` are never logged by this library.

## Credits

The protocol behavior follows TinyTuya by Jason A. Cox:

https://github.com/jasonacox/tinytuya

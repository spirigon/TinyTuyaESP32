# Tested Devices

| Model | Product key | Protocol | Discovery | Status | Control | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Tuya Wi-Fi relay/switch, model not recorded | keyjup78v54myhan | 3.4 | Pass | Pass | Not tested | Validated 2026-06-18 with ESP32-C3-DevKitM-1. `discovery` found the device on LAN, `validate_device` returned `tuya_status err=0`, and `async_task` returned repeated DPS JSON through `TinyTuyaAsync`. DPS included relay and metering fields. Control was not toggled to avoid switching a connected load. |

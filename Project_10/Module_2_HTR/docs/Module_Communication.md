Module Communication - MQTT Interface

Overview

This document describes the MQTT topics, payload formats, and UI behaviour used to read and control modules running the ESP32 firmware in this project.

Topic namespace

All topics use the base prefix `home` followed by the module id, for example:

- `home/esp_module_1/...`
- `home/esp_module_2/...`

Each device (physical module) publishes and subscribes under its module id. The module id is derived from GPIO35 at boot:
- GPIO35 HIGH -> `esp_module_1` (Cluster 1: Soil moisture, ADC)
- GPIO35 LOW  -> `esp_module_2` (Cluster 2: SHT30 + BH1750)

MQTT client ID

- The device uses the `module_id` as its MQTT client id (e.g. `esp_module_1`).

Topics and payloads

1) Relay control (commands sent to device to toggle relays)
- Control topics:
  - `home/<module_id>/relay1/control`
  - `home/<module_id>/relay2/control`
  - `home/<module_id>/relay3/control`
  - `home/<module_id>/relay4/control`
- Payloads: simple text `ON`, `OFF` or `1`, `0`.

2) Relay status (device publishes current relay states)
- Status topics:
  - `home/<module_id>/relay1/status`
  - `home/<module_id>/relay2/status`
  - `home/<module_id>/relay3/status`
  - `home/<module_id>/relay4/status`
- Payloads: `ON` or `OFF` (retained = true). The dashboard subscribes to these to update UI.

3) Moisture sensor (Cluster 1)
- Topic: `home/<module_id>/sensors/moisture`
- Payload: JSON
  {
    "module_id": "esp_module_1",
    "adc": 3100,
    "percentage": 42
  }

4) SHT30 sensor (Cluster 2)
- Topic: `home/<module_id>/sensors/sht30`
- Payload: JSON
  {
    "module_id": "esp_module_2",
    "temperature": 24.3,
    "humidity": 55.2
  }

5) Light sensor (BH1750 on Cluster 2)
- Topic: `home/<module_id>/sensors/light`
- Payload: JSON
  {
    "module_id": "esp_module_2",
    "lux": 123.4
  }

6) Module ID and WiFi status
- Module id topic (device publishes current id): `home/<module_id>/status/module_id` (payload: e.g. `esp_module_1`)
- WiFi status topic: `home/<module_id>/status/wifi` (payload: `online`/`offline`)

Dashboard behaviour

- The web dashboard can run in two modes:
  - Single-module: choose `esp_module_1` or `esp_module_2` and the dashboard subscribes only to that module's topics.
  - Both-modules view: subscribe to both modules' topics and show two cards simultaneously.
- The dashboard determines module identity from the topic path (`home/<module_id>/...`) and from the `module_id` field in JSON payloads (as fallback).
- When toggling relays from the dashboard, commands are sent to `home/<selected_module>/relayN/control` where `<selected_module>` is the currently selected module in the UI (or chosen per-control if the UI is extended).

Best practices

- Keep `retain=true` for status topics so new subscribers get current states.
- Use module-specific topics to avoid cross-control between multiple physical devices.
- If multiple modules share a broker, use distinct `module_id` values and avoid overlapping topic prefixes.

Conversion to DOCX

To convert this Markdown to a Word document locally, use `pandoc`:

```bash
pandoc docs/Module_Communication.md -o Module_Communication.docx
```

If you want, I can generate a `.docx` file content here and add it to the repo; tell me if you prefer that and I'll create it.

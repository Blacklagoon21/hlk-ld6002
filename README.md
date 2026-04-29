> **Source code was written with vibe coding using Claude Sonnet 4.6.**  
> **Tested on ESP32-C3.**

# ESPHome External Component ? HLK-LD6002

ESPHome external component for the HLK-LD6002 60 GHz FMCW radar sensor.  
Receives Heart Rate, Breath Rate, and Distance measurements over UART.

---

## Hardware Specifications

| Item | Value |
|---|---|
| Product | HI-LINK HLK-LD6002 |
| Frequency | 57?64 GHz FMCW |
| Power | 3.3 V (separate DCDC recommended: ￣1 A, ripple ¯50 mV) |
| Communication | UART 115200 bps |
| Measurements | Heart Rate (bpm), Breath Rate (bpm), Distance (cm) |

---

## Wiring

| LD6002 Pin | ESP32 Pin | Description |
|---|---|---|
| 3V3 | 3.3V | Power supply (separate DCDC recommended) |
| GND | GND | Common ground |
| **P19** | **GND** | **Must be tied to GND** (must be LOW at power-on) |
| TX0 | RX (e.g. GPIO4) | Sensor ⊥ MCU data |
| RX0 | TX (e.g. GPIO5) | MCU ⊥ Sensor (optional) |

> **Warning:** If P19 is not connected to GND, the sensor will not transmit any data.

---

## Communication Protocol

### Frame Structure

```
Byte   Field         Description
式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式式
[0]    Start         Fixed value 0x01
[1-2]  Reserved      0x00 0x00 (always 0)
[3-4]  DataLen       Payload length (Big-Endian, 4 or 8)
[5-6]  FrameType     Frame type ID (Big-Endian)
[7]    HeaderXor     Header checksum (XOR-Inverse of [0..6])
[8..]  Payload       Data payload
[last] PayloadXor    Payload checksum (XOR-Inverse of payload)
```

### Frame Types

| Type Code | Description | Payload |
|---|---|---|
| `0x0A15` | Heart Rate | 4-byte IEEE 754 float (bpm) |
| `0x0A14` | Breath Rate | 4-byte IEEE 754 float (bpm) |
| `0x0A16` | Distance | 4-byte uint32 flag + 4-byte float (cm) |

---

## ESPHome Configuration

### 1. Copy the Component

Place the component files in your project directory.

```
your-project/
戍式式 your-device.yaml
戌式式 external_components/
    戌式式 ld6002/
        戍式式 __init__.py
        戍式式 ld6002.h
        戌式式 ld6002.cpp
```

### 2. Minimal Configuration

```yaml
external_components:
  - source:
      type: local
      path: external_components
    components: [ld6002]

esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: arduino

uart:
  id: uart_ld6002
  rx_pin: GPIO4       # Connect to LD6002 TX0
  tx_pin: GPIO5       # Connect to LD6002 RX0 (optional)
  baud_rate: 115200

ld6002:
  uart_id: uart_ld6002
  heart_rate:
    name: "Heart Rate"
  breath_rate:
    name: "Breath Rate"
  distance:
    name: "Distance"
```

## Configuration Parameters

### `ld6002:` Block

| Parameter | Required | Description |
|---|---|---|
| `uart_id` | Required | UART component ID |
| `heart_rate` | Optional | Heart rate sensor configuration |
| `breath_rate` | Optional | Breath rate sensor configuration |
| `distance` | Optional | Distance sensor configuration (cm) |

All sensors support the full ESPHome [Sensor schema](https://esphome.io/components/sensor/index.html) (`name`, `id`, `filters`, `on_value`, etc.).

---

## Troubleshooting

| Symptom | Cause | Solution |
|---|---|---|
| No data (5s warning) | Baud rate mismatch or P19 not grounded | Change `baud_rate` / connect P19 to GND |
| Repeated checksum errors | UART noise or baud rate mismatch | Check wiring, verify baud rate |
| HR/BR spikes | Motion interference | Ensure stable power supply and minimize vibration |

---

## References

- HLK-LD6002 original library: [phuongnamzz/HLK-LD6002](https://github.com/phuongnamzz/HLK-LD6002)

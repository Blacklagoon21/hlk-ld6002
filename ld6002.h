#pragma once

// HLK-LD6002 ESPHome External Component
// 57-64GHz FMCW Radar: Heart Rate / Breath Rate / Distance
// Based on: https://github.com/phuongnamzz/HLK-LD6002

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"
#include <queue>

namespace esphome {
namespace ld6002 {

// Frame type codes (from protocol documentation)
static constexpr uint16_t FRAME_TYPE_HEART_RATE  = 0x0A15;
static constexpr uint16_t FRAME_TYPE_BREATH_RATE = 0x0A14;
static constexpr uint16_t FRAME_TYPE_DISTANCE    = 0x0A16;

// Maximum frame length: 17 bytes (8 header + 8 payload + 1 checksum)
static constexpr uint8_t MAX_FRAME_LEN = 17;

class LD6002Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Sensor registration setters (called by generated code from __init__.py)
  void set_heart_rate_sensor(sensor::Sensor *s) { heart_rate_sensor_ = s; }
  void set_breath_rate_sensor(sensor::Sensor *s) { breath_rate_sensor_ = s; }
  void set_distance_sensor(sensor::Sensor *s) { distance_sensor_ = s; }
  void set_running_sensor(sensor::Sensor *s) { running_sensor_ = s; }

 protected:
  // Frame parsing state
  std::queue<uint8_t> rx_queue_;    // UART 수신 바이트 큐
  uint8_t frame_[MAX_FRAME_LEN];   // Receive frame buffer
  uint8_t pos_{0};                  // Current buffer write position
  bool syncing_{false};             // True while accumulating a frame
  uint16_t expected_frame_len_{0};  // Expected total frame length (uint16 to avoid overflow)
  uint32_t last_rx_ms_{0};          // Timestamp of last received byte (for no-data watchdog)

  // Sensor pointers (nullptr if not configured)
  sensor::Sensor *heart_rate_sensor_{nullptr};
  sensor::Sensor *breath_rate_sensor_{nullptr};
  sensor::Sensor *distance_sensor_{nullptr};
  sensor::Sensor *running_sensor_{nullptr};

  // running flag: true when distance <= 90 cm
  bool running_{false};

  // Last time any valid HR/BR/Distance packet was received
  uint32_t last_valid_ms_{0};
  // True after sensors have been cleared; prevents repeated 0-publish
  bool sensors_cleared_{false};

  // --- Parsing helpers ---
  // 4 bytes little-endian -> IEEE 754 float
  float bytes_to_float(const uint8_t *data) const;
  // 4 bytes little-endian -> uint32_t
  uint32_t bytes_to_uint32(const uint8_t *data) const;
  // XOR-inverse checksum over len bytes
  uint8_t calc_xor_inverse(const uint8_t *data, int len) const;
  // Validate and dispatch a complete frame
  void parse_frame(const uint8_t *frame);
  // Publish 0 to all sensors and log
  void reset_all_sensors();
};

}  // namespace ld6002
}  // namespace esphome
// HLK-LD6002 ESPHome External Component Implementation
//
// Frame structure:
//   [0]     0x01         - Start byte
//   [1-2]   0x00 0x00    - Reserved (must be 0x00)
//   [3-4]   dataLen      - Payload length (big-endian)
//   [5-6]   frameType    - Frame type ID (big-endian)
//   [7]     headerXor    - XOR-inverse checksum of bytes[0..6]
//   [8..]   payload      - Data payload
//   [last]  payloadXor   - XOR-inverse checksum of payload bytes
//
// Frame types handled:
//   0x0A15  Heart Rate  - payload: 4-byte float (bpm)
//   0x0A14  Breath Rate - payload: 4-byte float (bpm)
//   0x0A16  Distance    - payload: 4-byte uint32 flag + 4-byte float (cm)

#include "ld6002.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace ld6002 {

static const char *const TAG = "ld6002";

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void LD6002Component::setup() {
  ESP_LOGI(TAG, "Setting up HLK-LD6002 radar sensor...");
  pos_ = 0;
  syncing_ = false;
  expected_frame_len_ = 0;
  last_rx_ms_ = millis();
  last_valid_ms_ = millis();
  sensors_cleared_ = false;
  this->check_uart_settings(115200, 1, uart::UART_CONFIG_PARITY_NONE, 8);
}

// ---------------------------------------------------------------------------
// loop  - Read all available UART bytes into the queue first, then process frames
// ---------------------------------------------------------------------------
void LD6002Component::loop() {
  // -- 1) Top priority: drain all available UART bytes into the queue ----------
  while (this->available()) {
    uint8_t b;
    if (!this->read_byte(&b)) break;
    last_rx_ms_ = millis();
    rx_queue_.push(b);
  }

  uint32_t now = millis();

  // running_ flag: clear to 0 if no HR/BR received for more than 3 seconds
  if (running_ && (now - last_valid_ms_ > 3000)) {
    ESP_LOGW(TAG, "No HR/BR for > 3s!");
  }

  // No-data watchdog: warn once if no bytes received for more than 5 seconds
  if (rx_queue_.empty()) {
    if (now - last_rx_ms_ > 5000) {
      ESP_LOGW(TAG, "No UART data for >5s - check baud rate, RX pin, 3.3V power, P19 pull-low");
      last_rx_ms_ = now;
    }
    
    return;
  }

  // -- 2) Dequeue bytes and assemble frames using the state machine -----------
  while (!rx_queue_.empty()) {
    uint8_t b = rx_queue_.front();
    rx_queue_.pop();

    if (!syncing_) {
      // Search for SOF byte (0x01)
      if (b == 0x01) {
        frame_[0] = b;
        pos_ = 1;
        syncing_ = true;
        expected_frame_len_ = 0;
      }
    } else {
      if (pos_ < MAX_FRAME_LEN) {
        frame_[pos_++] = b;

        // pos==7: determine total frame length from dataLen field
        if (pos_ == 7) {
          uint16_t data_len =
              (static_cast<uint16_t>(frame_[3]) << 8) | frame_[4];

          if (data_len != 4 && data_len != 8) {
            // Unknown dataLen - silently re-sync
            syncing_ = false;
            pos_ = 0;
            expected_frame_len_ = 0;
            continue;
          }
          // total = 8 (header) + dataLen (payload) + 1 (payload checksum)
          expected_frame_len_ = static_cast<uint16_t>(8 + data_len + 1);
        }

        // Frame complete -> parse
        if (expected_frame_len_ > 0 && pos_ >= expected_frame_len_) {
          parse_frame(frame_);
          syncing_ = false;
          pos_ = 0;
          expected_frame_len_ = 0;
        }
      } else {
        // Buffer overflow - silently re-sync
        syncing_ = false;
        pos_ = 0;
        expected_frame_len_ = 0;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// dump_config
// ---------------------------------------------------------------------------
void LD6002Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HLK-LD6002 Radar Sensor:");
  LOG_SENSOR("  ", "Heart Rate",  heart_rate_sensor_);
  LOG_SENSOR("  ", "Breath Rate", breath_rate_sensor_);
  LOG_SENSOR("  ", "Distance",    distance_sensor_);
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------
float LD6002Component::bytes_to_float(const uint8_t *data) const {
  float f;
  memcpy(&f, data, sizeof(float));
  return f;
}

uint32_t LD6002Component::bytes_to_uint32(const uint8_t *data) const {
  uint32_t val;
  memcpy(&val, data, sizeof(uint32_t));
  return val;
}

uint8_t LD6002Component::calc_xor_inverse(const uint8_t *data, int len) const {
  uint8_t cksum = 0;
  for (int i = 0; i < len; i++) cksum ^= data[i];
  return static_cast<uint8_t>(~cksum);
}

// ---------------------------------------------------------------------------
// parse_frame  - Handles Heart Rate / Breath Rate / Distance only; ignores all other frame types
// ---------------------------------------------------------------------------
void LD6002Component::parse_frame(const uint8_t *frame) {
  // Validate header checksum
  uint8_t hdr_calc = calc_xor_inverse(frame, 7);
  if (hdr_calc != frame[7]) {
    ESP_LOGW(TAG, "Header checksum error: calc=0x%02X got=0x%02X (type=0x%02X%02X)",
             hdr_calc, frame[7], frame[5], frame[6]);
    return;
  }

  uint16_t frame_type =
      (static_cast<uint16_t>(frame[5]) << 8) | frame[6];

  switch (frame_type) {

    // -- Heart Rate (0x0A15) ------------------------------------------------
    case FRAME_TYPE_HEART_RATE: {
      uint8_t hr_calc = calc_xor_inverse(frame + 8, 4);
      if (hr_calc != frame[12]) {
        ESP_LOGW(TAG, "Heart Rate payload checksum error: calc=0x%02X got=0x%02X", hr_calc, frame[12]);
        return;
      }
      float hr = bytes_to_float(&frame[8]);
      ESP_LOGI(TAG, "Heart Rate : %.1f bpm", hr);
      last_valid_ms_ = millis();
      sensors_cleared_ = false;
      if (heart_rate_sensor_ != nullptr)
        heart_rate_sensor_->publish_state(hr);
      // HR received -> set running_ = 1
      if (!running_) {
        running_ = true;
        ESP_LOGI(TAG, "Running: 1 (HR received)");
        if (running_sensor_ != nullptr)
          running_sensor_->publish_state(1.0f);
      }
      break;
    }

    // -- Breath Rate (0x0A14) -----------------------------------------------
    case FRAME_TYPE_BREATH_RATE: {
      uint8_t br_calc = calc_xor_inverse(frame + 8, 4);
      if (br_calc != frame[12]) {
        ESP_LOGW(TAG, "Breath Rate payload checksum error: calc=0x%02X got=0x%02X", br_calc, frame[12]);
        return;
      }
      float br = bytes_to_float(&frame[8]);
      ESP_LOGI(TAG, "Breath Rate: %.1f bpm", br);
      last_valid_ms_ = millis();
      sensors_cleared_ = false;
      if (breath_rate_sensor_ != nullptr)
        breath_rate_sensor_->publish_state(br);
      // BR received -> set running_ = 1
      if (!running_) {
        running_ = true;
        ESP_LOGI(TAG, "Running: 1 (BR received)");
        if (running_sensor_ != nullptr)
          running_sensor_->publish_state(1.0f);
      }
      break;
    }

    // -- Distance (0x0A16) --------------------------------------------------
    //   payload: 4-byte uint32 flag @ frame[8..11]
    //            4-byte float dist  @ frame[12..15] (cm)
    //   flag == 1 -> valid target present
    case FRAME_TYPE_DISTANCE: {
      uint8_t dist_calc = calc_xor_inverse(frame + 8, 8);
      if (dist_calc != frame[16]) {
        ESP_LOGW(TAG, "Distance payload checksum error: calc=0x%02X got=0x%02X", dist_calc, frame[16]);
        return;
      }
      uint32_t flag = bytes_to_uint32(&frame[8]);
      if (flag == 1) {
        float dist = bytes_to_float(&frame[12]);
        ESP_LOGI(TAG, "Distance   : %.1f cm", dist);
        if (distance_sensor_ != nullptr)
          distance_sensor_->publish_state(dist);
      }
      break;
    }

    default:
      // Silently ignore all packets other than Heart Rate / Breath Rate / Distance
      break;
  }
}

void LD6002Component::reset_all_sensors() {
  sensors_cleared_ = true;
  if (heart_rate_sensor_ != nullptr)
    heart_rate_sensor_->publish_state(0.0f);
  if (breath_rate_sensor_ != nullptr)
    breath_rate_sensor_->publish_state(0.0f);
}

}  // namespace ld6002
}  // namespace esphome
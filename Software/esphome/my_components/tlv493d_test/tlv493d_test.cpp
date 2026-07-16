#include "tlv493d_test.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace tlv493d_test {

static const char *const TAG = "tlv493d_test";

void TLV493DTestComponent::set_zero_angle(float value) {
  this->zero_angle_deg_ = this->normalize_deg_(value);
  ESP_LOGI(TAG, "Zero angle set to %.2f deg", this->zero_angle_deg_);
}

void TLV493DTestComponent::set_calibration_leaf(int value) {
  this->calibration_leaf_ = this->normalize_leaf_(value);
  ESP_LOGI(TAG, "Calibration leaf set to %d", this->calibration_leaf_);
}

void TLV493DTestComponent::set_target_leaf(int value) {
  this->target_leaf_ = this->normalize_leaf_(value);
  ESP_LOGI(TAG, "Target leaf set to %d", this->target_leaf_);
}

void TLV493DTestComponent::set_pages(int value) {
  if (value < 1) value = 1;
  this->pages_ = value;
  this->calibration_leaf_ = this->normalize_leaf_(this->calibration_leaf_);
  this->target_leaf_ = this->normalize_leaf_(this->target_leaf_);
  this->current_leaf_ = this->normalize_leaf_(this->current_leaf_);
  this->pending_leaf_ = -1;
  this->pending_leaf_count_ = 0;
  ESP_LOGI(TAG, "Pages set to %d", this->pages_);
}

void TLV493DTestComponent::set_mode(const std::string &value) {
  this->mode_ = value;
  this->last_clock_target_ = -1;
  this->last_clock_second_ = -1;
  ESP_LOGI(TAG, "Mode set to %s", this->mode_.c_str());
}

void TLV493DTestComponent::set_sensor_samples(int value) {
  if (value < 1) value = 1;
  if (value > 20) value = 20;
  this->sensor_samples_ = value;
  ESP_LOGI(TAG, "Sensor samples set to %d", this->sensor_samples_);
}

void TLV493DTestComponent::set_motor_pin(GPIOPin *pin) {
  this->motor_pin_ = pin;
}

void TLV493DTestComponent::set_motor_pulse_ms(int value) {
  if (value < 1) value = 1;
  this->motor_pulse_ms_ = value;
}

void TLV493DTestComponent::set_motor_settle_ms(int value) {
  if (value < 1) value = 1;
  this->motor_settle_ms_ = value;
}

void TLV493DTestComponent::set_move_timeout_ms(int value) {
  if (value < 100) value = 100;
  this->move_timeout_ms_ = value;
}

void TLV493DTestComponent::set_max_pulses_per_move(int value) {
  if (value < 1) value = 1;
  this->max_pulses_per_move_ = value;
}

void TLV493DTestComponent::set_unchanged_limit(int value) {
  if (value < 1) value = 1;
  this->unchanged_limit_ = value;
}

void TLV493DTestComponent::set_move_stop_lead(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 0.95f) value = 0.95f;
  this->move_stop_lead_ = value;
  ESP_LOGI(TAG, "Move stop lead set to %.2f leaves", this->move_stop_lead_);
}

void TLV493DTestComponent::set_seconds_stop_lead(float value) {
  if (value < 0.0f) value = 0.0f;
  if (value > 0.95f) value = 0.95f;
  this->seconds_stop_lead_ = value;
  ESP_LOGI(TAG, "Seconds stop lead set to %.2f leaves", this->seconds_stop_lead_);
}

float TLV493DTestComponent::normalize_deg_(float a) const {
  while (a < 0.0f) a += 360.0f;
  while (a >= 360.0f) a -= 360.0f;
  return a;
}

float TLV493DTestComponent::circular_diff_deg_(float a, float b) const {
  float d = this->normalize_deg_(a - b);
  if (d > 180.0f) d -= 360.0f;
  return d;
}

float TLV493DTestComponent::calc_angle_(float x, float y) const {
  float angle = std::atan2(y, x) * 180.0f / 3.14159265358979323846f;
  return this->normalize_deg_(angle);
}

int TLV493DTestComponent::normalize_leaf_(int leaf) const {
  if (this->pages_ <= 0) return 0;
  while (leaf < 0) leaf += this->pages_;
  while (leaf >= this->pages_) leaf -= this->pages_;
  return leaf;
}

int TLV493DTestComponent::forward_leaf_distance_(int from, int to) const {
  if (this->pages_ <= 0) return 0;
  return this->normalize_leaf_(to - from);
}

bool TLV493DTestComponent::target_was_passed_(int previous_leaf, int new_leaf) const {
  int moved_forward = this->forward_leaf_distance_(previous_leaf, new_leaf);
  if (moved_forward <= 0 || moved_forward > (this->pages_ / 2)) {
    return false;
  }

  int target_distance = this->forward_leaf_distance_(previous_leaf, this->target_leaf_);
  return target_distance > 0 && target_distance <= moved_forward;
}

float TLV493DTestComponent::angle_to_leaf_position_(float angle_deg) const {
  if (this->pages_ <= 0) return 0.0f;
  float adjusted = std::fmod(this->zero_angle_deg_ - angle_deg + 360.0f, 360.0f);
  if (adjusted < 0.0f) adjusted += 360.0f;

  const float degrees_per_page = 360.0f / static_cast<float>(this->pages_);
  return adjusted / degrees_per_page;
}

float TLV493DTestComponent::forward_leaf_position_distance_(float from_position, int to_leaf) const {
  if (this->pages_ <= 0) return 0.0f;
  float distance = static_cast<float>(this->normalize_leaf_(to_leaf)) - from_position;
  while (distance < 0.0f) distance += static_cast<float>(this->pages_);
  while (distance >= static_cast<float>(this->pages_)) distance -= static_cast<float>(this->pages_);
  return distance;
}

int TLV493DTestComponent::clock_value_to_leaf_(int value) const {
  if (value < 0) value = 0;
  if (value > 59) value = 59;
  if (value <= 30) return this->normalize_leaf_(value);
  return this->normalize_leaf_(value + 1);
}

int TLV493DTestComponent::angle_to_leaf_raw_(float angle_deg) const {
  int leaf = static_cast<int>(this->angle_to_leaf_position_(angle_deg));
  if (leaf < 0) leaf = 0;
  if (leaf >= this->pages_) leaf = this->pages_ - 1;
  return leaf;
}

int TLV493DTestComponent::angle_to_leaf_hysteretic_(float angle_deg) {
  const float degrees_per_page = 360.0f / static_cast<float>(this->pages_);
  const float hysteresis_deg = degrees_per_page * this->leaf_hysteresis_fraction_;

  int raw_leaf = this->angle_to_leaf_raw_(angle_deg);
  if (raw_leaf == this->current_leaf_) {
    return raw_leaf;
  }

  float current_leaf_center_adjusted =
      (static_cast<float>(this->current_leaf_) + 0.5f) * degrees_per_page;

  float current_leaf_center_angle =
      this->normalize_deg_(this->zero_angle_deg_ - current_leaf_center_adjusted);

  float dist_to_current_center =
      std::fabs(this->circular_diff_deg_(angle_deg, current_leaf_center_angle));

  if (dist_to_current_center <= (degrees_per_page * 0.5f + hysteresis_deg)) {
    return this->current_leaf_;
  }

  return raw_leaf;
}

int TLV493DTestComponent::stabilize_idle_leaf_(int candidate_leaf) {
  if (candidate_leaf == this->current_leaf_) {
    this->pending_leaf_ = -1;
    this->pending_leaf_count_ = 0;
    return this->current_leaf_;
  }

  if (candidate_leaf == this->pending_leaf_) {
    this->pending_leaf_count_++;
  } else {
    this->pending_leaf_ = candidate_leaf;
    this->pending_leaf_count_ = 1;
  }

  if (this->pending_leaf_count_ >= 4) {
    this->current_leaf_ = candidate_leaf;
    this->pending_leaf_ = -1;
    this->pending_leaf_count_ = 0;
  }

  return this->current_leaf_;
}

bool TLV493DTestComponent::read_frame_(uint8_t *data, size_t len) {
  auto err = this->read(data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Read failed: %d", err);
    this->status_text_ = "I2C read error";
    return false;
  }
  return true;
}

bool TLV493DTestComponent::decode_xyz_(const uint8_t *data, int16_t &x, int16_t &y, int16_t &z) {
  x = ((int16_t) data[0] << 4) | ((data[4] >> 4) & 0x0F);
  y = ((int16_t) data[1] << 4) | (data[4] & 0x0F);
  z = ((int16_t) data[2] << 4) | (data[5] & 0x0F);

  if (x & 0x800) x -= 4096;
  if (y & 0x800) y -= 4096;
  if (z & 0x800) z -= 4096;

  return true;
}

bool TLV493DTestComponent::init_sensor_() {
  uint8_t init10[10] = {0};
  auto err = this->read(init10, sizeof(init10));
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Initial read failed: %d", err);
    this->status_text_ = "Initial read failed";
    return false;
  }

  ESP_LOGI(TAG, "Init10: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
           init10[0], init10[1], init10[2], init10[3], init10[4],
           init10[5], init10[6], init10[7], init10[8], init10[9]);

  uint8_t payload[4] = {0x00, 0x81, 0x00, 0x00};
  err = this->bus_->write_readv(this->address_, payload, sizeof(payload), nullptr, 0);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Raw config write failed: %d", err);
    this->status_text_ = "Config write failed";
    return false;
  }

  ESP_LOGI(TAG, "Raw config write sent: %02X %02X %02X %02X",
           payload[0], payload[1], payload[2], payload[3]);

  delay(20);
  this->status_text_ = "OK";
  return true;
}

bool TLV493DTestComponent::read_averaged_xyz_(float &x_ut, float &y_ut, float &z_ut, float &angle_deg,
                                              bool moving) {
  float sx = 0.0f;
  float sy = 0.0f;
  float sz = 0.0f;
  float angle_sin = 0.0f;
  float angle_cos = 0.0f;
  int samples = this->sensor_samples_;
  if (moving && samples > 3) samples = 3;

  for (int i = 0; i < samples; i++) {
    uint8_t data[7] = {0};
    if (!this->read_frame_(data, sizeof(data))) {
      return false;
    }

    int16_t raw_x = 0;
    int16_t raw_y = 0;
    int16_t raw_z = 0;
    this->decode_xyz_(data, raw_x, raw_y, raw_z);

    const float scale = 0.098f;
    float sample_x = raw_x * scale;
    float sample_y = raw_y * scale;
    float sample_z = raw_z * scale;
    sx += sample_x;
    sy += sample_y;
    sz += sample_z;

    float sample_angle_rad = std::atan2(sample_y, sample_x);
    angle_sin += std::sin(sample_angle_rad);
    angle_cos += std::cos(sample_angle_rad);

    delay(1);
  }

  float measured_x = sx / static_cast<float>(samples);
  float measured_y = sy / static_cast<float>(samples);
  float measured_z = sz / static_cast<float>(samples);
  float measured_angle = this->normalize_deg_(std::atan2(angle_sin, angle_cos) * 180.0f / 3.14159265358979323846f);

  if (moving) {
    this->filtered_x_ut_ = measured_x;
    this->filtered_y_ut_ = measured_y;
    this->filtered_z_ut_ = measured_z;
    this->filtered_angle_deg_ = measured_angle;
    this->filter_initialized_ = true;
    this->angle_filter_initialized_ = true;
  } else if (!this->filter_initialized_) {
    this->filtered_x_ut_ = measured_x;
    this->filtered_y_ut_ = measured_y;
    this->filtered_z_ut_ = measured_z;
    this->filtered_angle_deg_ = measured_angle;
    this->filter_initialized_ = true;
    this->angle_filter_initialized_ = true;
  } else {
    float delta = std::fabs(this->circular_diff_deg_(measured_angle, this->filtered_angle_deg_));

    float alpha = 0.10f;
    if (delta > 3.0f) alpha = 0.22f;
    if (delta > 8.0f) alpha = 0.45f;
    if (delta > 15.0f) alpha = 0.65f;

    this->filtered_x_ut_ = this->filtered_x_ut_ + alpha * (measured_x - this->filtered_x_ut_);
    this->filtered_y_ut_ = this->filtered_y_ut_ + alpha * (measured_y - this->filtered_y_ut_);
    this->filtered_z_ut_ = this->filtered_z_ut_ + alpha * (measured_z - this->filtered_z_ut_);
    this->filtered_angle_deg_ = this->normalize_deg_(
        this->filtered_angle_deg_ + alpha * this->circular_diff_deg_(measured_angle, this->filtered_angle_deg_));
  }

  x_ut = this->filtered_x_ut_;
  y_ut = this->filtered_y_ut_;
  z_ut = this->filtered_z_ut_;
  angle_deg = this->filtered_angle_deg_;
  return true;
}

bool TLV493DTestComponent::refresh_measurement_(bool moving) {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float angle = 0.0f;

  if (!this->read_averaged_xyz_(x, y, z, angle, moving)) {
    return false;
  }

  this->x_ut_ = x;
  this->y_ut_ = y;
  this->z_ut_ = z;
  this->angle_deg_ = angle;
  int candidate_leaf = moving ? this->angle_to_leaf_raw_(this->angle_deg_)
                              : this->angle_to_leaf_hysteretic_(this->angle_deg_);
  this->current_leaf_ = moving ? candidate_leaf : this->stabilize_idle_leaf_(candidate_leaf);
  return true;
}

void TLV493DTestComponent::motor_on_() {
  if (this->motor_pin_ != nullptr) {
    this->motor_pin_->digital_write(true);
  }
}

void TLV493DTestComponent::motor_off_() {
  if (this->motor_pin_ != nullptr) {
    this->motor_pin_->digital_write(false);
  }
}

void TLV493DTestComponent::begin_pulse_() {
  this->motor_on_();
  this->pulse_started_ms_ = millis();
  this->move_state_ = MOVE_PULSE_ON;
}

void TLV493DTestComponent::finish_move_(const std::string &status_text) {
  this->motor_off_();
  this->move_active_ = false;
  this->move_state_ = MOVE_IDLE;
  this->status_text_ = status_text;
  this->post_move_settle_until_ms_ = millis() + 220;
}

void TLV493DTestComponent::pulse_once() {
  if (this->move_active_) {
    ESP_LOGW(TAG, "Pulse ignored, move active");
    return;
  }
  this->status_text_ = "Single pulse";
  this->move_active_ = true;
  this->move_started_ms_ = millis();
  this->move_pulses_ = 0;
  this->unchanged_counter_ = 0;
  this->last_move_leaf_ = this->current_leaf_;
  this->target_leaf_ = this->normalize_leaf_(this->current_leaf_ + 1);
  this->begin_pulse_();
}

void TLV493DTestComponent::advance_one_leaf() {
  this->target_leaf_ = this->normalize_leaf_(this->current_leaf_ + 1);
  this->start_move_to_target();
}

void TLV493DTestComponent::update_clock_time(int hour, int minute, int second) {
  if (!this->initialized_ || this->mode_ == "Manuell" || this->mode_ == "Kalibrierung") {
    return;
  }

  if ((int32_t) (millis() - this->post_move_settle_until_ms_) < 0) {
    return;
  }

  int clock_target = -1;
  if (this->mode_ == "Sekunden") {
    if (second == this->last_clock_second_) {
      return;
    }
    this->last_clock_second_ = second;

    if (this->move_active_) {
      ESP_LOGW(TAG, "Second tick skipped, move still active");
      return;
    }

    clock_target = this->clock_value_to_leaf_(second);
  } else if (this->mode_ == "Minuten") {
    clock_target = this->clock_value_to_leaf_(minute);
  } else if (this->mode_ == "Stunden") {
    clock_target = hour % 24;
  } else if (this->mode_ == "Leerblatt") {
    clock_target = this->pages_ > 61 ? 61 : this->pages_ - 1;
  } else {
    return;
  }

  clock_target = this->normalize_leaf_(clock_target);

  if (clock_target != this->last_clock_target_) {
    ESP_LOGI(TAG, "Clock target: mode=%s time=%02d:%02d:%02d target=%d",
             this->mode_.c_str(), hour, minute, second, clock_target);
    this->last_clock_target_ = clock_target;
  }

  if (this->move_active_ || this->current_leaf_ == clock_target) {
    return;
  }

  this->target_leaf_ = clock_target;
  this->start_move_to_target_(this->mode_ == "Sekunden" ? this->seconds_stop_lead_ : this->move_stop_lead_);
}

void TLV493DTestComponent::start_move_to_target() {
  this->start_move_to_target_(this->move_stop_lead_);
}

void TLV493DTestComponent::start_move_to_target_(float stop_lead) {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Move ignored, sensor not initialized");
    return;
  }

  if (!this->refresh_measurement_()) {
    this->status_text_ = "Measure error";
    ESP_LOGW(TAG, "Move ignored, no fresh sensor data");
    return;
  }

  this->target_leaf_ = this->normalize_leaf_(this->target_leaf_);
  if (this->current_leaf_ == this->target_leaf_) {
    this->status_text_ = "Already at target";
    return;
  }

  this->move_active_ = true;
  this->move_started_ms_ = millis();
  this->last_progress_ms_ = this->move_started_ms_;
  this->move_pulses_ = 0;
  this->unchanged_counter_ = 0;
  this->last_move_leaf_ = this->angle_to_leaf_raw_(this->angle_deg_);
  this->status_text_ = "Moving";
  this->move_state_ = MOVE_RUN;
  this->active_stop_lead_ = stop_lead;
  this->motor_on_();

  ESP_LOGI(TAG, "Start move: current=%d target=%d stop_lead=%.2f",
           this->current_leaf_, this->target_leaf_, this->active_stop_lead_);
}

void TLV493DTestComponent::stop_move() {
  this->finish_move_("Stopped");
  ESP_LOGI(TAG, "Move stopped");
}

void TLV493DTestComponent::handle_move_state_() {
  if (!this->move_active_) {
    return;
  }

  uint32_t now = millis();

  if ((int32_t) (now - this->move_started_ms_) > this->move_timeout_ms_) {
    this->finish_move_("Move timeout");
    ESP_LOGW(TAG, "Move timeout");
    return;
  }

  switch (this->move_state_) {
    case MOVE_IDLE:
      break;

    case MOVE_RUN: {
      int previous_leaf = this->last_move_leaf_;
      if (!this->refresh_measurement_(true)) {
        this->finish_move_("Measure error");
        return;
      }

      float move_position = this->angle_to_leaf_position_(this->angle_deg_);
      float target_distance = this->forward_leaf_position_distance_(move_position, this->target_leaf_);
      int move_leaf = this->angle_to_leaf_raw_(this->angle_deg_);

      if (this->active_stop_lead_ > 0.0f && target_distance > 0.0f && target_distance <= this->active_stop_lead_) {
        this->finish_move_("Coast to target");
        ESP_LOGI(TAG, "Coast to target: position=%.2f target=%d lead=%.2f",
                 move_position, this->target_leaf_, target_distance);
        return;
      }

      if (move_leaf == this->target_leaf_) {
        this->current_leaf_ = this->target_leaf_;
        this->finish_move_("Target reached");
        ESP_LOGI(TAG, "Target reached: leaf=%d", this->current_leaf_);
        return;
      }

      if (previous_leaf >= 0 && this->target_was_passed_(previous_leaf, move_leaf)) {
        this->current_leaf_ = move_leaf;
        this->finish_move_("Target passed");
        ESP_LOGW(TAG, "Target passed between leaf %d and %d: target=%d",
                 previous_leaf, move_leaf, this->target_leaf_);
        return;
      }

      if (move_leaf == previous_leaf) {
        if ((int32_t) (now - this->last_progress_ms_) > 2000) {
          this->finish_move_("Blocked/no progress");
          ESP_LOGW(TAG, "Blocked: no leaf progress");
          return;
        }
      } else {
        this->last_progress_ms_ = now;
        this->last_move_leaf_ = move_leaf;
      }
      break;
    }

    case MOVE_PULSE_ON:
      if ((int32_t) (now - this->pulse_started_ms_) >= this->motor_pulse_ms_) {
        this->motor_off_();
        this->settle_started_ms_ = now;
        this->move_state_ = MOVE_SETTLE;
      }
      break;

    case MOVE_SETTLE:
      if ((int32_t) (now - this->settle_started_ms_) >= this->motor_settle_ms_) {
        int previous_leaf = this->last_move_leaf_;
        if (!this->refresh_measurement_()) {
          this->finish_move_("Measure error");
          return;
        }

        this->move_pulses_++;

        if (this->current_leaf_ == this->target_leaf_) {
          this->finish_move_("Target reached");
          ESP_LOGI(TAG, "Target reached: leaf=%d pulses=%d", this->current_leaf_, this->move_pulses_);
          return;
        }

        if (previous_leaf >= 0 && this->target_was_passed_(previous_leaf, this->current_leaf_)) {
          this->finish_move_("Target passed");
          ESP_LOGW(TAG, "Target passed between leaf %d and %d: target=%d pulses=%d",
                   previous_leaf, this->current_leaf_, this->target_leaf_, this->move_pulses_);
          return;
        }

        if (this->current_leaf_ == this->last_move_leaf_) {
          this->unchanged_counter_++;
        } else {
          this->unchanged_counter_ = 0;
          this->last_move_leaf_ = this->current_leaf_;
        }

        if (this->unchanged_counter_ >= this->unchanged_limit_) {
          this->finish_move_("Blocked/no progress");
          ESP_LOGW(TAG, "Blocked: no leaf progress");
          return;
        }

        if (this->move_pulses_ >= this->max_pulses_per_move_) {
          this->finish_move_("Pulse limit reached");
          ESP_LOGW(TAG, "Pulse limit reached");
          return;
        }

        this->begin_pulse_();
      }
      break;
  }
}

void TLV493DTestComponent::calibrate_here() {
  if (!this->initialized_) {
    ESP_LOGW(TAG, "Calibration ignored, sensor not initialized");
    return;
  }

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float angle = 0.0f;

  if (!this->read_averaged_xyz_(x, y, z, angle, false)) {
    ESP_LOGW(TAG, "Calibration failed, no fresh sensor data");
    return;
  }

  const float degrees_per_page = 360.0f / static_cast<float>(this->pages_);
  float zero = std::fmod(angle + (static_cast<float>(this->calibration_leaf_) * degrees_per_page), 360.0f);
  if (zero < 0.0f) zero += 360.0f;

  this->zero_angle_deg_ = zero;
  this->current_leaf_ = this->calibration_leaf_;
  this->pending_leaf_ = -1;
  this->pending_leaf_count_ = 0;

  ESP_LOGI(TAG, "Calibrated here: angle=%.2f deg, leaf=%d, new zero=%.2f deg",
           angle, this->calibration_leaf_, this->zero_angle_deg_);
  this->status_text_ = "Calibrated";
}

void TLV493DTestComponent::reinitialize_sensor() {
  ESP_LOGI(TAG, "Reinitializing TLV493D...");
  this->initialized_ = this->init_sensor_();
  this->filter_initialized_ = false;
  this->angle_filter_initialized_ = false;
  this->move_active_ = false;
  this->move_state_ = MOVE_IDLE;
  this->pending_leaf_ = -1;
  this->pending_leaf_count_ = 0;
  this->motor_off_();
}

void TLV493DTestComponent::setup() {
  ESP_LOGI(TAG, "Setup TLV493D test component...");

  if (this->motor_pin_ != nullptr) {
    this->motor_pin_->setup();
    this->motor_off_();
  }

  this->initialized_ = this->init_sensor_();
  if (this->initialized_) {
    this->refresh_measurement_();
  }
}

void TLV493DTestComponent::update() {
  if (!this->initialized_) {
    return;
  }

  if (this->move_active_) {
    this->handle_move_state_();
  } else {
    this->refresh_measurement_();
  }

  ESP_LOGD(TAG, "X=%.1f uT Y=%.1f uT Z=%.1f uT Angle=%.1f deg Leaf=%d Target=%d Zero=%.1f Pulses=%d State=%s",
           this->x_ut_, this->y_ut_, this->z_ut_, this->angle_deg_,
           this->current_leaf_, this->target_leaf_, this->zero_angle_deg_,
           this->move_pulses_, this->status_text_.c_str());
}

void TLV493DTestComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TLV493D Test Component");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Pages: %d", this->pages_);
  ESP_LOGCONFIG(TAG, "  Calibration leaf: %d", this->calibration_leaf_);
  ESP_LOGCONFIG(TAG, "  Target leaf: %d", this->target_leaf_);
  ESP_LOGCONFIG(TAG, "  Zero angle: %.2f", this->zero_angle_deg_);
  ESP_LOGCONFIG(TAG, "  Mode: %s", this->mode_.c_str());
  ESP_LOGCONFIG(TAG, "  Sensor samples: %d", this->sensor_samples_);
  ESP_LOGCONFIG(TAG, "  Motor pin configured");
  ESP_LOGCONFIG(TAG, "  Pulse ms: %d", this->motor_pulse_ms_);
  ESP_LOGCONFIG(TAG, "  Settle ms: %d", this->motor_settle_ms_);
  ESP_LOGCONFIG(TAG, "  Move timeout ms: %d", this->move_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Max pulses per move: %d", this->max_pulses_per_move_);
  ESP_LOGCONFIG(TAG, "  Unchanged limit: %d", this->unchanged_limit_);
  ESP_LOGCONFIG(TAG, "  Move stop lead: %.2f leaves", this->move_stop_lead_);
  ESP_LOGCONFIG(TAG, "  Seconds stop lead: %.2f leaves", this->seconds_stop_lead_);
}

}  // namespace tlv493d_test
}  // namespace esphome

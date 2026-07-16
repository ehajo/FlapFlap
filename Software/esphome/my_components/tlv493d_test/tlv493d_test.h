#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include <string>

namespace esphome {
namespace tlv493d_test {

enum MoveState : uint8_t {
  MOVE_IDLE = 0,
  MOVE_RUN,
  MOVE_PULSE_ON,
  MOVE_SETTLE
};

class TLV493DTestComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_zero_angle(float value);
  void set_calibration_leaf(int value);
  void set_target_leaf(int value);
  void set_pages(int value);
  void set_mode(const std::string &value);
  void set_sensor_samples(int value);

  void set_motor_pin(GPIOPin *pin);
  void set_motor_pulse_ms(int value);
  void set_motor_settle_ms(int value);
  void set_move_timeout_ms(int value);
  void set_max_pulses_per_move(int value);
  void set_unchanged_limit(int value);
  void set_move_stop_lead(float value);
  void set_seconds_stop_lead(float value);

  void calibrate_here();
  void reinitialize_sensor();

  void pulse_once();
  void advance_one_leaf();
  void start_move_to_target();
  void stop_move();
  void update_clock_time(int hour, int minute, int second);

  float get_x_ut() const { return this->x_ut_; }
  float get_y_ut() const { return this->y_ut_; }
  float get_z_ut() const { return this->z_ut_; }
  float get_angle_deg() const { return this->angle_deg_; }
  float get_zero_angle() const { return this->zero_angle_deg_; }
  int get_current_leaf() const { return this->current_leaf_; }
  int get_target_leaf() const { return this->target_leaf_; }
  int get_move_pulses() const { return this->move_pulses_; }
  std::string get_status_text() const { return this->status_text_; }

 protected:
  bool initialized_{false};

  float x_ut_{0.0f};
  float y_ut_{0.0f};
  float z_ut_{0.0f};
  float angle_deg_{0.0f};
  float zero_angle_deg_{0.0f};

  float filtered_x_ut_{0.0f};
  float filtered_y_ut_{0.0f};
  float filtered_z_ut_{0.0f};
  float filtered_angle_deg_{0.0f};
  bool filter_initialized_{false};
  bool angle_filter_initialized_{false};

  int current_leaf_{0};
  int pending_leaf_{-1};
  int pending_leaf_count_{0};
  int calibration_leaf_{0};
  int target_leaf_{0};
  int pages_{62};
  int sensor_samples_{8};

  float leaf_hysteresis_fraction_{0.22f};

  std::string mode_{"Manuell"};
  std::string status_text_{"Start"};

  GPIOPin *motor_pin_{nullptr};
  int motor_pulse_ms_{40};
  int motor_settle_ms_{35};
  int move_timeout_ms_{15000};
  int max_pulses_per_move_{120};
  int unchanged_limit_{8};
  float move_stop_lead_{0.35f};
  float seconds_stop_lead_{0.12f};

  MoveState move_state_{MOVE_IDLE};
  bool move_active_{false};
  uint32_t pulse_started_ms_{0};
  uint32_t settle_started_ms_{0};
  uint32_t move_started_ms_{0};
  uint32_t last_progress_ms_{0};
  uint32_t post_move_settle_until_ms_{0};
  int move_pulses_{0};
  int unchanged_counter_{0};
  int last_move_leaf_{-1};
  int last_clock_target_{-1};
  int last_clock_second_{-1};

  bool init_sensor_();
  bool read_frame_(uint8_t *data, size_t len);
  bool read_averaged_xyz_(float &x_ut, float &y_ut, float &z_ut, float &angle_deg, bool moving);
  bool refresh_measurement_(bool moving = false);
  bool decode_xyz_(const uint8_t *data, int16_t &x, int16_t &y, int16_t &z);
  float calc_angle_(float x, float y) const;
  float circular_diff_deg_(float a, float b) const;
  float normalize_deg_(float a) const;
  float angle_to_leaf_position_(float angle_deg) const;
  float forward_leaf_position_distance_(float from_position, int to_leaf) const;
  int clock_value_to_leaf_(int value) const;
  int angle_to_leaf_raw_(float angle_deg) const;
  int angle_to_leaf_hysteretic_(float angle_deg);
  int stabilize_idle_leaf_(int candidate_leaf);
  int normalize_leaf_(int leaf) const;
  int forward_leaf_distance_(int from, int to) const;
  bool target_was_passed_(int previous_leaf, int new_leaf) const;

  void motor_on_();
  void motor_off_();
  void begin_pulse_();
  void start_move_to_target_(float stop_lead);
  void finish_move_(const std::string &status_text);
  void handle_move_state_();

  float active_stop_lead_{0.35f};
};

}  // namespace tlv493d_test
}  // namespace esphome

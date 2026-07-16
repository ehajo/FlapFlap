import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]

tlv493d_test_ns = cg.esphome_ns.namespace("tlv493d_test")
TLV493DTestComponent = tlv493d_test_ns.class_(
    "TLV493DTestComponent",
    cg.PollingComponent,
    i2c.I2CDevice,
)

CONF_ZERO_ANGLE = "zero_angle"
CONF_CALIBRATION_LEAF = "calibration_leaf"
CONF_TARGET_LEAF = "target_leaf"
CONF_PAGES = "pages"
CONF_MODE = "mode"
CONF_SENSOR_SAMPLES = "sensor_samples"
CONF_MOTOR_PIN = "motor_pin"
CONF_MOTOR_PULSE_MS = "motor_pulse_ms"
CONF_MOTOR_SETTLE_MS = "motor_settle_ms"
CONF_MOVE_TIMEOUT_MS = "move_timeout_ms"
CONF_MAX_PULSES_PER_MOVE = "max_pulses_per_move"
CONF_UNCHANGED_LIMIT = "unchanged_limit"
CONF_MOVE_STOP_LEAD = "move_stop_lead"
CONF_SECONDS_STOP_LEAD = "seconds_stop_lead"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TLV493DTestComponent),
            cv.Optional(CONF_ZERO_ANGLE, default=0.0): cv.float_,
            cv.Optional(CONF_CALIBRATION_LEAF, default=0): cv.int_range(min=0, max=200),
            cv.Optional(CONF_TARGET_LEAF, default=0): cv.int_range(min=0, max=200),
            cv.Optional(CONF_PAGES, default=62): cv.int_range(min=1, max=200),
            cv.Optional(CONF_MODE, default="Manuell"): cv.string,
            cv.Optional(CONF_SENSOR_SAMPLES, default=8): cv.int_range(min=1, max=20),
            cv.Required(CONF_MOTOR_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_MOTOR_PULSE_MS, default=40): cv.int_range(min=1, max=1000),
            cv.Optional(CONF_MOTOR_SETTLE_MS, default=35): cv.int_range(min=1, max=2000),
            cv.Optional(CONF_MOVE_TIMEOUT_MS, default=15000): cv.int_range(min=100, max=120000),
            cv.Optional(CONF_MAX_PULSES_PER_MOVE, default=120): cv.int_range(min=1, max=1000),
            cv.Optional(CONF_UNCHANGED_LIMIT, default=8): cv.int_range(min=1, max=100),
            cv.Optional(CONF_MOVE_STOP_LEAD, default=0.35): cv.float_range(min=0.0, max=0.95),
            cv.Optional(CONF_SECONDS_STOP_LEAD, default=0.12): cv.float_range(min=0.0, max=0.95),
        }
    )
    .extend(cv.polling_component_schema("20ms"))
    .extend(i2c.i2c_device_schema(0x5E))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_zero_angle(config[CONF_ZERO_ANGLE]))
    cg.add(var.set_calibration_leaf(config[CONF_CALIBRATION_LEAF]))
    cg.add(var.set_target_leaf(config[CONF_TARGET_LEAF]))
    cg.add(var.set_pages(config[CONF_PAGES]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_sensor_samples(config[CONF_SENSOR_SAMPLES]))

    motor_pin = await cg.gpio_pin_expression(config[CONF_MOTOR_PIN])
    cg.add(var.set_motor_pin(motor_pin))

    cg.add(var.set_motor_pulse_ms(config[CONF_MOTOR_PULSE_MS]))
    cg.add(var.set_motor_settle_ms(config[CONF_MOTOR_SETTLE_MS]))
    cg.add(var.set_move_timeout_ms(config[CONF_MOVE_TIMEOUT_MS]))
    cg.add(var.set_max_pulses_per_move(config[CONF_MAX_PULSES_PER_MOVE]))
    cg.add(var.set_unchanged_limit(config[CONF_UNCHANGED_LIMIT]))
    cg.add(var.set_move_stop_lead(config[CONF_MOVE_STOP_LEAD]))
    cg.add(var.set_seconds_stop_lead(config[CONF_SECONDS_STOP_LEAD]))

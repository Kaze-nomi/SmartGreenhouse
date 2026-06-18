#include "wokwi-api.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint32_t temperature_attr;
  uint32_t humidity_attr;
  uint32_t address_attr;
  uint8_t command[2];
  uint8_t command_len;
  uint8_t response[6];
  uint8_t response_len;
  uint8_t response_pos;
} chip_state_t;

static float clamp_float(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static uint8_t sht30_crc8(uint8_t msb, uint8_t lsb) {
  uint8_t crc = 0xff;
  uint8_t data[2] = { msb, lsb };

  for (int i = 0; i < 2; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      if (crc & 0x80) {
        crc = (uint8_t)((crc << 1) ^ 0x31);
      } else {
        crc <<= 1;
      }
    }
  }

  return crc;
}

static void prepare_measurement(chip_state_t *chip) {
  float temperature = clamp_float(attr_read_float(chip->temperature_attr), -45.0f, 130.0f);
  float humidity = clamp_float(attr_read_float(chip->humidity_attr), 0.0f, 100.0f);

  uint16_t raw_temperature = (uint16_t)(((temperature + 45.0f) * 65535.0f / 175.0f) + 0.5f);
  uint16_t raw_humidity = (uint16_t)((humidity * 65535.0f / 100.0f) + 0.5f);

  chip->response[0] = (uint8_t)(raw_temperature >> 8);
  chip->response[1] = (uint8_t)(raw_temperature & 0xff);
  chip->response[2] = sht30_crc8(chip->response[0], chip->response[1]);
  chip->response[3] = (uint8_t)(raw_humidity >> 8);
  chip->response[4] = (uint8_t)(raw_humidity & 0xff);
  chip->response[5] = sht30_crc8(chip->response[3], chip->response[4]);
  chip->response_len = 6;
  chip->response_pos = 0;
}

static void prepare_status(chip_state_t *chip) {
  chip->response[0] = 0x00;
  chip->response[1] = 0x00;
  chip->response[2] = sht30_crc8(chip->response[0], chip->response[1]);
  chip->response_len = 3;
  chip->response_pos = 0;
}

static void process_command(chip_state_t *chip) {
  uint16_t command = ((uint16_t)chip->command[0] << 8) | chip->command[1];

  switch (command) {
    case 0x2c06:
    case 0x2c0d:
    case 0x2c10:
    case 0x2400:
    case 0x240b:
    case 0x2416:
    case 0xe000:
      prepare_measurement(chip);
      break;
    case 0xf32d:
      prepare_status(chip);
      break;
    case 0x3041:
    case 0x30a2:
    case 0x306d:
    case 0x3066:
      chip->response_len = 0;
      chip->response_pos = 0;
      break;
    default:
      chip->response_len = 0;
      chip->response_pos = 0;
      break;
  }
}

static bool on_i2c_connect(void *user_data, uint32_t address, bool read) {
  (void)address;
  chip_state_t *chip = (chip_state_t *)user_data;

  if (read) {
    chip->response_pos = 0;
  } else {
    chip->command_len = 0;
  }

  return true;
}

static uint8_t on_i2c_read(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->response_pos < chip->response_len) {
    return chip->response[chip->response_pos++];
  }

  return 0xff;
}

static bool on_i2c_write(void *user_data, uint8_t data) {
  chip_state_t *chip = (chip_state_t *)user_data;

  if (chip->command_len < 2) {
    chip->command[chip->command_len++] = data;
  }

  if (chip->command_len == 2) {
    process_command(chip);
  }

  return true;
}

static void on_i2c_disconnect(void *user_data) {
  (void)user_data;
}

void chip_init(void) {
  chip_state_t *chip = (chip_state_t *)malloc(sizeof(chip_state_t));
  chip->temperature_attr = attr_init_float("temperature", 25.0f);
  chip->humidity_attr = attr_init_float("humidity", 60.0f);
  chip->address_attr = attr_init("i2cAddress", 0x44);
  chip->command_len = 0;
  chip->response_len = 0;
  chip->response_pos = 0;

  pin_init("VCC", INPUT);
  pin_init("GND", INPUT);
  pin_init("ADDR", INPUT_PULLDOWN);

  i2c_config_t i2c_config = {
    .user_data = chip,
    .address = attr_read(chip->address_attr),
    .scl = pin_init("SCL", INPUT_PULLUP),
    .sda = pin_init("SDA", INPUT_PULLUP),
    .connect = on_i2c_connect,
    .read = on_i2c_read,
    .write = on_i2c_write,
    .disconnect = on_i2c_disconnect
  };

  i2c_init(&i2c_config);
}

#ifndef WOKWI_API_H
#define WOKWI_API_H

#include <stdbool.h>
#include <stdint.h>

enum pin_value { LOW = 0, HIGH = 1 };
enum pin_mode {
  INPUT = 0,
  OUTPUT = 1,
  INPUT_PULLUP = 2,
  INPUT_PULLDOWN = 3,
  ANALOG = 4,
  OUTPUT_LOW = 16,
  OUTPUT_HIGH = 17
};
enum edge { RISING = 1, FALLING = 2, BOTH = 3 };

int __attribute__((export_name("__wokwi_api_version_1"))) __attribute__((weak)) __wokwi_api_version_1(void) {
  return 1;
}

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t pin_t;

typedef struct {
  void *user_data;
  uint32_t edge;
  void (*pin_change)(void *user_data, pin_t pin, uint32_t value);
} pin_watch_config_t;

extern __attribute__((export_name("chipInit"))) void chip_init(void);
extern __attribute__((import_name("pinInit"))) pin_t pin_init(const char *name, uint32_t mode);
extern __attribute__((import_name("pinRead"))) uint32_t pin_read(pin_t pin);
extern __attribute__((import_name("pinWrite"))) void pin_write(pin_t pin, uint32_t value);
extern __attribute__((import_name("pinWatch"))) bool pin_watch(pin_t pin, const pin_watch_config_t *config);
extern __attribute__((import_name("pinWatchStop"))) void pin_watch_stop(pin_t pin);
extern __attribute__((import_name("pinMode"))) void pin_mode(pin_t pin, uint32_t value);
extern __attribute__((import_name("attrInit"))) uint32_t attr_init(const char *name, uint32_t default_value);
extern __attribute__((import_name("attrInit"))) uint32_t attr_init_float(const char *name, float default_value);
extern __attribute__((import_name("attrRead"))) uint32_t attr_read(uint32_t attr_id);
extern __attribute__((import_name("attrReadFloat"))) float attr_read_float(uint32_t attr_id);

typedef struct {
  void *user_data;
  uint32_t address;
  pin_t scl;
  pin_t sda;
  bool (*connect)(void *user_data, uint32_t address, bool read);
  uint8_t (*read)(void *user_data);
  bool (*write)(void *user_data, uint8_t data);
  void (*disconnect)(void *user_data);
  uint32_t reserved[8];
} i2c_config_t;

typedef uint32_t i2c_dev_t;
extern __attribute__((import_name("i2cInit"))) i2c_dev_t i2c_init(const i2c_config_t *config);

#ifdef __cplusplus
}
#endif

#endif

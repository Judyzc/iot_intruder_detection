#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Arduino.h>
#include "esp_timer.h"
#include "driver/gpio.h"

#pragma once
#include <Wire.h>
#include <Adafruit_MLX90614.h>

extern Adafruit_MLX90614 mlx;
extern TwoWire I2C;


// ---------- GPIO Configuration ----------
#ifndef INTRUDER_LED_GPIO
#define INTRUDER_LED_GPIO GPIO_NUM_21
#endif

#ifndef PIR_LED_GPIO
#define PIR_LED_GPIO GPIO_NUM_48
#endif

#ifndef PIR_GPIO
#define PIR_GPIO GPIO_NUM_47
#endif



// ---------- Data Structures ----------
typedef struct {
    gpio_num_t pin;
    esp_timer_handle_t timer;
    volatile bool state;
    const char *name;
} hw_led_t;


// after typedef hw_led_t ...
extern hw_led_t intruder_led;   // expose the global intruder LED object
extern hw_led_t pir_led;        // optional: expose pir_led too

extern volatile bool pir_triggered;           // set by ISR
extern volatile bool pir_active;              // set by hardware_poll() to enable face detection window
extern int64_t pir_active_until_ms; 

#if CONFIG_ESP_FACE_RECOGNITION_ENABLED
extern int8_t recognition_enabled;
extern int8_t is_enrolling;
#endif

// ---------- Initialization ----------
/**
 * @brief Initialize LEDs, PIR sensor input, and attach interrupts.
 */
void hardware_init(void);

// ---------- Generic LED Control ----------
/**
 * @brief Initialize a single LED (called internally).
 */
esp_err_t hardware_led_init(hw_led_t *led);

/**
 * @brief Pulse the given LED for the specified duration (ms).
 */
void hardware_led_pulse(hw_led_t *led, uint32_t ms);

/**
 * @brief Turn the LED off immediately.
 */
void hardware_led_off(hw_led_t *led);

/**
 * @brief Returns true if the LED is currently ON.
 */
bool hardware_led_is_on(hw_led_t *led);

// ---------- PIR Sensor Handling ----------
/**
 * @brief Run logic for for PIR events and handle them in non-ISR context.
 *        Should be called frequently from loop() or a task.
 */
void hardware_main(void);


void hardware_buzz(void);

void send_to_database(bool intruder_status, int face_id, float confidence);

void sendIntruderAlert(void);

#endif // HARDWARE_CONTROL_H

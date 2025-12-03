#ifndef HARDWARE_CONTROL_H
#define HARDWARE_CONTROL_H

#include <Arduino.h>
#include "esp_timer.h"
#include "driver/gpio.h"

// GPIO pins
#ifndef INTRUDER_LED_GPIO
#define INTRUDER_LED_GPIO GPIO_NUM_21
#endif

#ifndef PIR_LED_GPIO
#define PIR_LED_GPIO GPIO_NUM_48
#endif

#ifndef PIR_GPIO
#define PIR_GPIO GPIO_NUM_47
#endif


// LED struct
typedef struct {
    gpio_num_t pin;
    esp_timer_handle_t timer;
    volatile bool state;
    const char *name;
} hw_led_t;


extern hw_led_t intruder_led; 
extern hw_led_t pir_led;      
extern volatile bool pir_triggered;       
extern volatile bool pir_active;             
extern int64_t pir_active_until_ms; 
extern volatile int8_t recognition_enabled;
extern volatile int8_t detection_enabled;

/* Initialize LEDs, PIR sensor input, and attach interrupts */
void hardware_init(void);

/* Initialize a single LED*/
esp_err_t hardware_led_init(hw_led_t *led);

/* Pulse the given LED for the given amount of time*/
void hardware_led_pulse(hw_led_t *led, uint32_t ms);

/* Turn the LED off */
void hardware_led_off(hw_led_t *led);

/* Returns true if the LED is ON */
bool hardware_led_is_on(hw_led_t *led);

/* Poll for PIR events and handles corresponding action (not in ISR) */
void hardware_control(void);

/* Turns on buzzer for a second */
void hardware_buzz(void);

/* Sends intruder status, face id and confidence to EC2 database */
void send_to_database(bool intruder_status, int face_id, float confidence);

/* Sends whatsapp message through chat me bot to notify owner*/
void sendIntruderAlert(void);

#endif

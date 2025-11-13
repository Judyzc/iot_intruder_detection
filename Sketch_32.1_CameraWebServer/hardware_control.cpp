/* hardware_control.cpp
   Used to setup and control different hardware peripherals
*/

#include "esp_timer.h"
#include "driver/gpio.h"
#include <Arduino.h>
#include "esp_log.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


#ifndef INTRUDER_LED_GPIO
#define INTRUDER_LED_GPIO GPIO_NUM_21
#endif

#ifndef PIR_LED_GPIO
#define PIR_LED_GPIO GPIO_NUM_48
#endif

#ifndef PIR_GPIO
#define PIR_GPIO GPIO_NUM_47
#endif

#ifndef BUZZ_GPIO
#define BUZZ_GPIO GPIO_NUM_45
#endif

typedef struct {
  gpio_num_t pin;
  esp_timer_handle_t timer;
  volatile bool state;
  const char *name;
} hw_led_t;


hw_led_t intruder_led = {
  .pin = INTRUDER_LED_GPIO,
  .timer = NULL,
  .state = false,
  .name = "intruder_led"
};

hw_led_t pir_led = {
  .pin = PIR_LED_GPIO,
  .timer = NULL,
  .state = false,
  .name = "pir_led"
};



// pir trigger
static volatile bool pir_triggered = false;
volatile bool alert_pending = false;

// text message variables
static volatile unsigned long lastAlertTime = 0;
#define ALERT_INTERVAL_MS 60000

const char* serverName = "https://api.callmebot.com/whatsapp.php";
const char* phoneNumber = "%2B19175103025";
const char* apiKey = "2047937";


// led timer
static void led_timer_callback(void* arg){
  if (arg == NULL) return;
  hw_led_t *led = (hw_led_t*)arg;
  digitalWrite((int)led->pin, LOW);
  led->state = false;
  ESP_LOGI("hw_led", "%s timer expired -> OFF (pin %d)", led->name, (int)led->pin);
}


// initialize led
esp_err_t hardware_led_init(hw_led_t *led) {
  if (!led) return ESP_ERR_INVALID_ARG;

  pinMode((int)led->pin, OUTPUT);
  digitalWrite((int)led->pin, LOW);
  led->state = false;

  esp_timer_create_args_t timerargs = {
    .callback = &led_timer_callback,
    .arg = led,
    .name = led->name
  };

  return esp_timer_create(&timerargs, &led->timer);
}


// pulse led
void hardware_led_pulse(hw_led_t *led, uint32_t ms){
  if (!led) return;

  digitalWrite((int)led->pin, HIGH);
  led->state = true;

  if (led->timer) {
    esp_timer_stop(led->timer);
    esp_timer_start_once(led->timer, (int64_t)ms * 1000);
  } else {
    ESP_LOGW("hw_led", "%s has no timer (pin %d). Keeping it ON.", led->name, (int)led->pin);
  }
}


// turn led off
void hardware_led_off(hw_led_t *led)
{
  if (!led) return;

  if (led->timer) {
    esp_timer_stop(led->timer);
  }
  digitalWrite((int)led->pin, LOW);
  led->state = false;
}


// led state
bool hardware_led_is_on(hw_led_t *led)
{
  if (!led) return false;
  return led->state;
}
// PIR
void IRAM_ATTR pir_isr() {
  pir_triggered = true;
}



static WiFiClientSecure alertClient;
static HTTPClient alertHttp;
static bool alertClientInitialized = false;

// text message
void sendIntruderAlert() {
  if (millis() - lastAlertTime < ALERT_INTERVAL_MS) return;

 if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // skip certificate check
    HTTPClient http;

    String message = "intruder+alert";
    String fullURL = String(serverName) +
                     "?phone=" + phoneNumber +
                     "&text=" + message +
                     "&apikey=" + apiKey;
     http.begin(client, fullURL);

      
      int httpResponseCode = http.GET();

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      if (httpResponseCode > 0) {
        String payload = http.getString();
        Serial.println(payload);
      } else {
        Serial.printf("GET failed: %s\n", http.errorToString(httpResponseCode).c_str());
      }

      http.end(); 
    }
    else {
      Serial.println("WiFi Disconnected");
    }

  lastAlertTime = millis();
}

void hardware_poll(void) {
  if (pir_triggered) {
    pir_triggered = false;

    hardware_led_pulse(&pir_led, 5000);

    ESP_LOGI("hw_poll", "PIR event processed");
  }

  if (alert_pending) {
    alert_pending = false;               
    Serial.println("hw_poll: alert_pending -> sending alert");
    sendIntruderAlert(); 
  }
}

void hardware_buzz(void){
  digitalWrite((int)BUZZ_GPIO, HIGH);
  delay(1000);
  digitalWrite((int)BUZZ_GPIO, LOW);
}


// hardware init
void hardware_init(void){
  esp_err_t err;

  err = hardware_led_init(&intruder_led);
  if (err != ESP_OK) {
    ESP_LOGE("hw_init", "Failed to create timer for intruder_led (err=%d)", err);
  }

  err = hardware_led_init(&pir_led);
  if (err != ESP_OK) {
    ESP_LOGE("hw_init", "Failed to create timer for pir_led (err=%d)", err);
  }

  // PIR Sensor
  pinMode((int)PIR_GPIO, INPUT);
  attachInterrupt(digitalPinToInterrupt((int)PIR_GPIO), pir_isr, RISING);

  pinMode((int)BUZZ_GPIO, OUTPUT);

  ESP_LOGI("hw_init", "Hardware initialized: PIR pin %d, intruder LED %d, pir LED %d",
           (int)PIR_GPIO, (int)INTRUDER_LED_GPIO, (int)PIR_LED_GPIO);
}



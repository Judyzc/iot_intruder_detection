#include "esp_camera.h"
#include <WiFi.h>
#include "hardware_control.h"
#include "intruder_task.h"



// Camera module
#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#include "camera_pins.h"
#define TAG "camera: "

//WiFi Credentials
const char* ssid     = "DukeVisitor";
const char* password = "";


void startCameraServer(); // Defined in app_httpd.cpp


void setup() {
  Serial.begin(115200);
  delay(200); // give UART a moment
  Serial.setDebugOutput(true);

  // To view ESP logs in Arudino, set core debug level to verbose and uncomment
  // esp_log_level_set("*", ESP_LOG_VERBOSE);
  // esp_log_level_set("app", ESP_LOG_VERBOSE);
  // esp_log_level_set("camera", ESP_LOG_VERBOSE);
  // esp_log_level_set("hardware", ESP_LOG_VERBOSE);
  // Serial.println("esp_log_level_set calls done");


  //Camera Pins on ESP32S3
  Serial.println("Setting up camera");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  // for PSRAM (pseudo static ram to add more memory to esp32)
  config.jpeg_quality = 10;
  config.fb_count = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;

  // camera init 
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 0); // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, 0); // lower the saturation

  // Start Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  // Allow for Wi-Fi to tIMEOUT
  unsigned long start = millis();
  const unsigned long wifiTimeout = 15000; // 15 s timeout
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < wifiTimeout) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection failed or timed out!");
  } else {
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP().toString()); 
  }

  startCameraServer();

  // give server a moment to start up and then print the camera URL
  delay(1000);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP().toString());
    Serial.println("' to connect");
    // Post that wifi is up
  } else {
    Serial.println("Camera server started but no WiFi IP assigned.");
  }

  hardware_init();
  intruder_task_init(); 
}

// Constantly check to see if PIR has been triggered - "motion detected"
unsigned long lastPoll = 0;
const unsigned long pollInterval = 500; // ms

void loop() {
  unsigned long now = millis();
  if (now - lastPoll >= pollInterval) {
    lastPoll = now;
    hardware_control();
  }
}

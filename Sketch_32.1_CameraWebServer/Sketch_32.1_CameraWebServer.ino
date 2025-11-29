#include "esp_camera.h"
#include <WiFi.h>
#include "hardware_control.h"
#include "intruder_task.h"

// camera module
#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#include "camera_pins.h"
//WiFi Credentials
const char* ssid     = "DukeVisitor";
const char* password = "";

//Text Messages
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;


void startCameraServer();

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.setDebugOutput(true);
  Serial.println();
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
  
  // need PSRAM in order to due facial detection (included in our board)
  if(psramFound()){
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // limit frame size when PSRAM is not available
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

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

  unsigned long start = millis();
  const unsigned long wifiTimeout = 15000; // 15 seconds
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

  delay(1000);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP().toString());
    Serial.println("' to connect");
  } else {
    Serial.println("Camera server started but no WiFi IP assigned.");
  }

  // init other components
  hardware_init();
  intruder_task_init(); 
}

void loop() {
  // PIR polling
  hardware_poll();
  delay(10);
}


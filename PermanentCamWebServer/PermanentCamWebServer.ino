/**********************************************************************
  Filename    : Camera Web Server
  Description : The camera images captured by the ESP32S3 are displayed on the web page.
  Auther      : www.freenove.com
  Modification: 2024/07/01
**********************************************************************/
#include "esp_camera.h"
#include <WiFi.h>
#include "hardware_control.h"

#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
#include "camera_pins.h"

const char* ssid     = "DukeVisitor";
const char* password = "";
const char* serverName = "https://api.callmebot.com/whatsapp.php";
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;


// initial declaration 
void startCameraServer(); 


void setup() {
  // set up serial
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  // set up camera configs
  camera_config_t camera_config;
  camera_config.ledc_channel = LEDC_CHANNEL_0;
  camera_config.ledc_timer = LEDC_TIMER_0;
  camera_config.pin_d0 = Y2_GPIO_NUM;
  camera_config.pin_d1 = Y3_GPIO_NUM;
  camera_config.pin_d2 = Y4_GPIO_NUM;
  camera_config.pin_d3 = Y5_GPIO_NUM;
  camera_config.pin_d4 = Y6_GPIO_NUM;
  camera_config.pin_d5 = Y7_GPIO_NUM;
  camera_config.pin_d6 = Y8_GPIO_NUM;
  camera_config.pin_d7 = Y9_GPIO_NUM;
  camera_config.pin_xclk = XCLK_GPIO_NUM;
  camera_config.pin_pclk = PCLK_GPIO_NUM;
  camera_config.pin_vsync = VSYNC_GPIO_NUM;
  camera_config.pin_href = HREF_GPIO_NUM;
  camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
  camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
  camera_config.pin_pwdn = PWDN_GPIO_NUM;
  camera_config.pin_reset = RESET_GPIO_NUM;
  camera_config.xclk_freq_hz = 10000000;
  camera_config.frame_size = FRAMESIZE_QVGA;
  camera_config.pixel_format = PIXFORMAT_JPEG; // for streaming
  camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  camera_config.fb_location = CAMERA_FB_IN_PSRAM;
  camera_config.jpeg_quality = 12;
  camera_config.fb_count = 2;
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality for larger pre-allocated frame buffer.
  if(psramFound()){
    camera_config.jpeg_quality = 10;
    camera_config.fb_count = 2;
    camera_config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    // Limit the frame size when PSRAM is not available
    Serial.println("ERROR: PSRAM not available");
    camera_config.fb_count = 1;
    camera_config.fb_location = CAMERA_FB_IN_DRAM;
  }
  // camera init
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  // get esp-cam
  sensor_t * s = esp_camera_sensor_get();   // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 0);                       // flip it back
  s->set_brightness(s, 1);                  // up the brightness just a bit
  s->set_saturation(s, 0);                  // lower the saturation
  // start wifi 
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  while (WiFi.STA.hasIP() != true) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("");
  Serial.println("WiFi connected");
  // start camera web server
  startCameraServer();
  delay(10000);
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  delay(10000);
  hardware_init();
}


void loop() {
  camera_task_loop();
  delay(10);
}


void camera_task_loop(){
    hardware_poll();
    delay(10);
}

  // // Do nothing. Everything is done in another task by the web server
  // if ((millis() - lastTime) > timerDelay && bool hardware_intruder_is_on() == 1) {
  //   if(WiFi.status() == WL_CONNECTED){
  //     WiFiClientSecure client;   
  //     client.setInsecure();     
  //     HTTPClient http;

  //     // change phone number in the phone field to change the number the message gets sent to
  //     // I believe you might need your own api key which you can get by messaging the number +34 611 01 16 37 on whatsapp the message 'I allow callmebot to send me messages' in whatsapp
  //     String fullURL = String(serverName) + "?phone=%2B19175103025&text=intruder+alert&apikey=2047937";

      
  //     http.begin(client, fullURL);

      
  //     int httpResponseCode = http.GET();

  //     Serial.print("HTTP Response code: ");
  //     Serial.println(httpResponseCode);

  //     if (httpResponseCode > 0) {
  //       String payload = http.getString();
  //       Serial.println(payload);
  //     } else {
  //       Serial.printf("GET failed: %s\n", http.errorToString(httpResponseCode).c_str());
  //     }

  //     http.end(); 
  //   }
  //   else {
  //     Serial.println("WiFi Disconnected");
  //   }
  //   lastTime = millis();
  // }
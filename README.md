# iot_intruder_detection

All Files:
- **CameraWebServer.ino**
  - Sets up ESP32 connection to Wifi
  - Defines camera configs
  - Displays status to serial if camera is configured properly
- **app_httpd.cpp**
  - Runs facial recognition algorithm
  - Outputs intruder detection status and confidence
  - Intruder detection status is sent to database and hardware peripherals
  - Sends GET request to CallMeBot to send an intruder status whatsapp message 
  - Sends image buffers to Camera streaming UI
  - Enrolls Face IDs
- **camera_index.h**
  - Compressed html file as a C array, came with CameraWebServer code developed by FreeNove
- **camera_pins.h**
  - All camera pins defined, came with CameraWebServer code developed by FreeNove
- **hardware_control.cpp** + header file
  - Functions receiving I/O from hardware peripherals (LEDs, PIR, buzzer)
  - Sending to database the heartbeat and intruder status
- **index.html**
  - Full-stack JavaScript Web App visualizing data from the Postgres database
- **app_intruder_detector.py**
  - Flask server processing GET and POST requests from the ESP32 and Javascript web app
  - Updates Postgres database with new entries 
    

#include <WiFi.h>
#include <WiFiClientSecure.h> 
#include <HTTPClient.h>

const char* ssid = "DukeVisitor";
const char* password = "";

const char* serverName = "https://api.callmebot.com/whatsapp.php";

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
 
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");
}

void loop() {
  if ((millis() - lastTime) > timerDelay) {
    if(WiFi.status() == WL_CONNECTED){
      WiFiClientSecure client;   
      client.setInsecure();     
      HTTPClient http;

      // change phone number in the phone field to change the number the message gets sent to
      // I believe you might need your own api key which you can get by messaging the number +34 611 01 16 37 on whatsapp the message 'I allow callmebot to send me messages' in whatsapp
      String fullURL = String(serverName) + "?phone=%2B19175103025&text=message+from+the+esp32&apikey=2047937";

      
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
    lastTime = millis();
  }
}
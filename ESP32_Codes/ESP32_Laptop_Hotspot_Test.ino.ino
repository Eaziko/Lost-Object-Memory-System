
#include <WiFi.h>

const char* ssid = "LostObject_Laptop";
const char* password = "LostObject123";

unsigned long lastStatusCheck = 0;

void connectWiFi() {
  Serial.println();
  Serial.println("Connecting to Laptop Hotspot...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WIFI CONNECTED SUCCESSFULLY!");
    Serial.print("Network: ");
    Serial.println(WiFi.SSID());

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway: ");
    Serial.println(WiFi.gatewayIP());
  } else {
    Serial.println("WIFI CONNECTION FAILED!");
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("LOST OBJECT MEMORY SYSTEMS");
  connectWiFi();
}

void loop() {
  if (millis() - lastStatusCheck >= 5000) {
    lastStatusCheck = millis();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Laptop hotspot connection active.");
    } else {
      Serial.println("WiFi disconnected. Reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }
}

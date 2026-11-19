
#include <WiFi.h>
#include <WiFiManager.h>

WiFiManager wifiManager;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("LOST OBJECT MEMORY SYSTEM");
  Serial.println("Wi-Fi Setup Portal");
  Serial.println("==============================");

  bool connected = wifiManager.autoConnect(
    "LostObject_Setup"
  );

  if (!connected) {
    Serial.println("Wi-Fi connection failed.");
    delay(3000);
    ESP.restart();
  }

  Serial.println("Wi-Fi connected successfully!");
  Serial.print("Network: ");
  Serial.println(WiFi.SSID());

  Serial.print("ESP32 IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  delay(3000);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connection active.");
  } else {
    Serial.println("Wi-Fi disconnected.");
  }
}

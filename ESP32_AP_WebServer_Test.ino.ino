
#include <WiFi.h>
#include <WebServer.h>

const char* apName = "LostObject_Test";
const char* apPassword = "lost12345";

WebServer server(80);

void handleRoot() {
  Serial.println("ROOT PAGE REQUEST RECEIVED");

  String webpage = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Lost Object Memory System</title>
  </head>

  <body style="font-family:Arial;text-align:center;padding:25px;background:#f2f4f7;">

    <div style="background:white;padding:25px;border-radius:15px;max-width:500px;margin:auto;">

      <h1 style="color:#087f8c;">ESP32-S3 Connected!</h1>

      <h2>Lost Object Memory System</h2>

      <p>Your ESP32-S3 web server is working successfully.</p>

      <hr>

      <p><b>Wi-Fi:</b> Connected</p>
      <p><b>Web Server:</b> Running</p>
      <p><b>Device:</b> ESP32-S3</p>

      <hr>

      <p style="color:green;">SYSTEM TEST SUCCESSFUL</p>

    </div>

  </body>
  </html>
  )rawliteral";

  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.send(200, "text/html", webpage);
}

void handleStatus() {
  String status = "ESP32-S3 ONLINE\n";
  status += "WiFi Clients: ";
  status += String(WiFi.softAPgetStationNum());
  status += "\nIP Address: ";
  status += WiFi.softAPIP().toString();

  server.send(200, "text/plain", status);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("================================");
  Serial.println("LOST OBJECT MEMORY SYSTEM");
  Serial.println("ESP32 WEB SERVER TEST");
  Serial.println("================================");

  WiFi.mode(WIFI_AP);

  bool result = WiFi.softAP(apName, apPassword);

  if (!result) {
    Serial.println("ERROR: WiFi Access Point failed!");
    return;
  }

  delay(500);

  Serial.println("Access Point started!");
  Serial.print("WiFi Name: ");
  Serial.println(apName);

  Serial.print("Password: ");
  Serial.println(apPassword);

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);

  server.onNotFound([]() {
    Serial.println("Unknown page requested");
    server.send(404, "text/plain", "404 - Page Not Found");
  });

  server.begin();

  Serial.println("HTTP Server Started!");
  Serial.println("Open: http://192.168.4.1/");
  Serial.println("Status: http://192.168.4.1/status");
}

void loop() {
  server.handleClient();

  static unsigned long lastCheck = 0;

  if (millis() - lastCheck >= 5000) {
    lastCheck = millis();

    Serial.print("Connected phones: ");
    Serial.println(WiFi.softAPgetStationNum());
  }

  delay(2);
}

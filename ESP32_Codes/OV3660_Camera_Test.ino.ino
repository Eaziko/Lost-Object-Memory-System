
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

// =====================================================
// ESP32-S3-CAM N16R8 + OV3660 CAMERA PIN CONFIGURATION
// Previously tested successfully on your board
// =====================================================

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11

#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// ====================================================
// LAPTOP MOBILE HOTSPOT
// ====================================================

const char* ssid = "LostObject_Laptop";
const char* password = "LostObject123";

// ====================================================
// FLASK SERVER ADDRESS
// ====================================================

const char* serverURL =
  "http://192.168.137.1:5000/upload";

// Capture interval: 5 seconds
const unsigned long captureInterval = 5000;

unsigned long lastCaptureTime = 0;

// ====================================================
// WIFI CONNECTION
// ====================================================

bool connectWiFi() {

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  WiFi.begin(ssid, password);

  Serial.println();
  Serial.print("Connecting to laptop hotspot");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("WIFI CONNECTED!");

    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

    Serial.print("Gateway Address: ");
    Serial.println(WiFi.gatewayIP());

    Serial.print("Flask Server: ");
    Serial.println(serverURL);

    return true;
  }

  Serial.println("WIFI CONNECTION FAILED!");

  return false;
}

// ==================================================
// CAMERA INITIALIZATION
// ==================================================

bool initializeCamera() {

  if (!psramFound()) {

    Serial.println("ERROR: PSRAM NOT DETECTED!");

    return false;
  }

  Serial.println("PSRAM DETECTED!");

  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  // Camera data pins

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  // Camera control pins

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  // Camera settings

  config.xclk_freq_hz = 10000000;

  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;

  config.jpeg_quality = 12;

  config.fb_count = 1;

  config.fb_location = CAMERA_FB_IN_PSRAM;

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  Serial.println("Initializing OV3660 camera...");

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {

    Serial.printf(
      "CAMERA INITIALIZATION FAILED: 0x%x\n",
      err
    );

    return false;
  }

  Serial.println("CAMERA INITIALIZED SUCCESSFULLY!");

  sensor_t* sensor = esp_camera_sensor_get();

  if (sensor != NULL) {

    Serial.printf(
      "Camera Sensor PID: 0x%04X\n",
      sensor->id.PID
    );
  }

  return true;
}

// ==================================================
// UPLOAD IMAGE TO FLASK SERVER
// ==================================================

bool uploadImage(camera_fb_t* fb) {

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WIFI DISCONNECTED!");

    return false;
  }

  WiFiClient client;

  HTTPClient http;

  const char* boundary = "----ESP32CameraBoundary";

  String header =
    String("--") + boundary + "\r\n" +
    "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n" +
    "Content-Type: image/jpeg\r\n\r\n";

  String footer =
    String("\r\n--") + boundary + "--\r\n";

  size_t totalLength =
    header.length() + fb->len + footer.length();

  uint8_t* payload =
    (uint8_t*)malloc(totalLength);

  if (payload == NULL) {

    Serial.println("ERROR: NOT ENOUGH MEMORY FOR UPLOAD!");

    return false;
  }

  size_t position = 0;

  // Copy HTTP multipart header

  memcpy(
    payload + position,
    header.c_str(),
    header.length()
  );

  position += header.length();

  // Copy JPEG image data

  memcpy(
    payload + position,
    fb->buf,
    fb->len
  );

  position += fb->len;

  // Copy HTTP multipart footer

  memcpy(
    payload + position,
    footer.c_str(),
    footer.length()
  );

  Serial.println("Sending image to laptop...");

  http.setTimeout(20000);

  if (!http.begin(client, serverURL)) {

    Serial.println("HTTP CONNECTION SETUP FAILED!");

    free(payload);

    return false;
  }

  http.addHeader(
    "Content-Type",
    String("multipart/form-data; boundary=") + boundary
  );

  int httpCode = http.POST(payload, totalLength);

  Serial.print("HTTP Response: ");

  Serial.println(httpCode);

  bool uploadSuccess = false;

  if (httpCode > 0) {

    String response = http.getString();

    Serial.println("Flask Server Response:");

    Serial.println(response);

    if (httpCode == 200) {

      Serial.println("IMAGE UPLOADED SUCCESSFULLY!");

      uploadSuccess = true;

    } else {

      Serial.println("SERVER REJECTED IMAGE!");
    }

  } else {

    Serial.print("UPLOAD FAILED: ");

    Serial.println(http.errorToString(httpCode));
  }

  http.end();

  free(payload);

  return uploadSuccess;
}

// ==================================================
// SETUP
// ==================================================

void setup() {

  Serial.begin(115200);

  delay(2000);

  Serial.println();

  Serial.println("======================================");

  Serial.println("LOST OBJECT MEMORY SYSTEM");

  Serial.println("ESP32-S3-CAM + OV3660");

  Serial.println("AUTOMATIC IMAGE UPLOAD SYSTEM");

  Serial.println("======================================");

  // Connect to laptop hotspot

  if (!connectWiFi()) {

    Serial.println("WIFI CONNECTION FAILED!");

    Serial.println("Restarting in 5 seconds...");

    delay(5000);

    ESP.restart();
  }

  // Initialize camera

  if (!initializeCamera()) {

    Serial.println("CAMERA INITIALIZATION FAILED!");

    Serial.println("Please check camera connection.");

    return;
  }

  Serial.println();

  Serial.println("SYSTEM READY!");

  Serial.println("Camera: OV3660");

  Serial.println("Resolution: 640 x 480");

  Serial.println("Capture Interval: 5 Seconds");

  Serial.println("Upload Destination: Laptop Flask Server");

  Serial.println();

  Serial.println("Starting automatic image capture...");
}

// ==================================================
// MAIN LOOP
// ==================================================

void loop() {

  // Reconnect Wi-Fi if disconnected

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WIFI DISCONNECTED!");

    Serial.println("Attempting reconnection...");

    if (!connectWiFi()) {

      delay(3000);

      return;
    }
  }

  // Capture an image every 5 seconds

  if (millis() - lastCaptureTime >= captureInterval) {

    lastCaptureTime = millis();

    Serial.println();

    Serial.println("--------------------------------------");

    Serial.println("Capturing image...");

    camera_fb_t* fb = esp_camera_fb_get();

    if (fb == NULL) {

      Serial.println("IMAGE CAPTURE FAILED!");

      delay(1000);

      return;
    }

    Serial.printf(
      "Image Size: %u bytes\n",
      fb->len
    );

    Serial.printf(
      "Resolution: %u x %u\n",
      fb->width,
      fb->height
    );

    // Upload captured image

    bool result = uploadImage(fb);

    // Release camera frame buffer

    esp_camera_fb_return(fb);

    if (result) {

      Serial.println("CAPTURE AND UPLOAD COMPLETED!");

    } else {

      Serial.println("CAPTURE COMPLETED, BUT UPLOAD FAILED!");
    }

    Serial.println("--------------------------------------");
  }

  delay(100);
}

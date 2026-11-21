
#include "esp_camera.h"

// Candidate pin mapping for an ESP32-S3 N16R8
// with an OV3660 camera.

#define PWDN_GPIO_NUM   38
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5

#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM     8
#define Y3_GPIO_NUM     9
#define Y2_GPIO_NUM     11

#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM   13

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("LOST OBJECT MEMORY SYSTEM");
  Serial.println("ESP32-S3 CAMERA TEST");
  Serial.println("==============================");

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

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  Serial.println("Initializing camera...");

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {
    Serial.printf("Camera initialization FAILED: 0x%x\n", result);
    Serial.println("Check camera ribbon and pin mapping.");
    return;
  }

  Serial.println("CAMERA INITIALIZED SUCCESSFULLY!");

  sensor_t *sensor = esp_camera_sensor_get();

  if (sensor != NULL) {
    Serial.printf("Camera PID: 0x%04X\n", sensor->id.PID);
  }
}

void loop() {
  camera_fb_t *frame = esp_camera_fb_get();

  if (!frame) {
    Serial.println("Image capture FAILED");
    delay(3000);
    return;
  }

  Serial.println("----------------------------------");
  Serial.println("IMAGE CAPTURE SUCCESSFUL!");
  Serial.printf("Image size: %u bytes\n", frame->len);
  Serial.printf("Resolution: %u x %u\n",
                frame->width, frame->height);
  Serial.println("----------------------------------");

  esp_camera_fb_return(frame);

  delay(3000);
}

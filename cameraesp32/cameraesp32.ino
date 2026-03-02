#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ================= WIFI =================
const char* ssid     = "San";
const char* password = "222777111";

// ================= CAMERA PINS =================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      21
#define SIOD_GPIO_NUM      26
#define SIOC_GPIO_NUM      27
#define Y9_GPIO_NUM        35
#define Y8_GPIO_NUM        34
#define Y7_GPIO_NUM        39
#define Y6_GPIO_NUM        36
#define Y5_GPIO_NUM        19
#define Y4_GPIO_NUM        18
#define Y3_GPIO_NUM         5
#define Y2_GPIO_NUM         4
#define VSYNC_GPIO_NUM     25
#define HREF_GPIO_NUM      23
#define PCLK_GPIO_NUM      22

httpd_handle_t server = NULL;

// ================= STREAM HANDLER =================
static esp_err_t stream_handler(httpd_req_t *req)
{
  camera_fb_t* fb  = NULL;
  esp_err_t    res = ESP_OK;

  // Allow browser canvas to capture frames (needed for recording)
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store");

  res = httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Frame capture failed");
      res = ESP_FAIL;
      break;
    }

    char header[80];
    int  header_len = snprintf(header, sizeof(header),
      "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      (uint32_t)fb->len);

    res = httpd_resp_send_chunk(req, header, header_len);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, "\r\n", 2);

    esp_camera_fb_return(fb);

    // ~20fps cap — prevents flooding the network buffer
    vTaskDelay(pdMS_TO_TICKS(50));

    if (res != ESP_OK) break;
  }
  return res;
}

// ================= START SERVER =================
void startCameraServer()
{
  httpd_config_t config    = HTTPD_DEFAULT_CONFIG();
  config.max_open_sockets  = 2;   // limit concurrent connections
  config.recv_wait_timeout = 10;
  config.send_wait_timeout = 10;

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_uri_t stream_uri = {
      .uri      = "/",
      .method   = HTTP_GET,
      .handler  = stream_handler,
      .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &stream_uri);
    Serial.println("Stream server started");
  }
}

// ================= SETUP =================
void setup()
{
  Serial.begin(115200);

  // Run ESP32 at full speed for best camera performance
  setCpuFrequencyMhz(240);

  camera_config_t config;
  config.ledc_channel  = LEDC_CHANNEL_0;
  config.ledc_timer    = LEDC_TIMER_0;
  config.pin_d0        = Y2_GPIO_NUM;
  config.pin_d1        = Y3_GPIO_NUM;
  config.pin_d2        = Y4_GPIO_NUM;
  config.pin_d3        = Y5_GPIO_NUM;
  config.pin_d4        = Y6_GPIO_NUM;
  config.pin_d5        = Y7_GPIO_NUM;
  config.pin_d6        = Y8_GPIO_NUM;
  config.pin_d7        = Y9_GPIO_NUM;
  config.pin_xclk      = XCLK_GPIO_NUM;
  config.pin_pclk      = PCLK_GPIO_NUM;
  config.pin_vsync     = VSYNC_GPIO_NUM;
  config.pin_href      = HREF_GPIO_NUM;
  config.pin_sccb_sda  = SIOD_GPIO_NUM;
  config.pin_sccb_scl  = SIOC_GPIO_NUM;
  config.pin_pwdn      = PWDN_GPIO_NUM;
  config.pin_reset     = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QVGA;  // 320x240 — best balance for hotspot
    config.jpeg_quality = 12;              // 10=best quality, 20=smallest file
    config.fb_count     = 3;              // 3 buffers = smoother capture
    config.grab_mode    = CAMERA_GRAB_LATEST; // always grab the LATEST frame
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  // Fine-tune sensor for better low-light and fast movement
  sensor_t* s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_quality(s, 12);
  s->set_brightness(s, 1);     // slight brightness boost
  s->set_saturation(s, 0);     // neutral saturation
  s->set_sharpness(s, 1);      // slight sharpness for motion clarity
  s->set_denoise(s, 1);        // reduce noise
  s->set_ae_level(s, 0);       // auto exposure neutral
  s->set_aec2(s, true);        // advanced auto exposure ON
  s->set_agc_gain(s, 0);       // auto gain control
  s->set_gainceiling(s, (gainceiling_t)6); // max gain ceiling
  s->set_awb_gain(s, true);    // auto white balance ON
  s->set_whitebal(s, true);
  s->set_lenc(s, true);        // lens correction ON

  WiFi.setSleep(false);        // disable WiFi sleep = lower latency
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("Camera stream: http://");
  Serial.println(WiFi.localIP());
  Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());

  startCameraServer();
}

// ================= LOOP =================
void loop()
{
  // Print WiFi signal strength every 10s so you can monitor connection quality
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000) {
    lastCheck = millis();
    Serial.printf("RSSI: %d dBm | Heap: %d bytes free\n",
      WiFi.RSSI(), ESP.getFreeHeap());
  }
  delay(100);
}

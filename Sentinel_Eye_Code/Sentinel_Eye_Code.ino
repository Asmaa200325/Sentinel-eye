#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "esp_http_server.h"

// ===========================
// WIFI SETTINGS
// ===========================
const char *ssid =:"YOUR_WIFI_SSID";
const char *password = "TOUR_WIFI_PASSWORD";

// ===========================
// TELEGRAM SETTINGS
// ===========================
const char* BOTtoken = "YOUR_TELEGRAM_BOT_TOKEM";
const char* CHAT_ID = "YOUR_TELEGRAM_CHAT_ID";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// ===========================
// SECURITY PASSWORD
// ===========================
const String systemPassword = "1234"; 
bool awaitingPassword = false;         

// ===========================
// PIN DEFINITIONS
// ===========================
#define PIR_PIN 13
#define RED_LED_PIN 12
#define GREEN_LED_PIN 2   
#define BUZZER_PIN 14      
#define FLASH_LED 4

// ===========================
// SYSTEM VARIABLES
// ===========================
bool systemActive = false;
bool motionDetected = false;
bool manualBuzzerMode = false; 

unsigned long lastCheckTime = 0;
int botRequestDelay = 300;

unsigned long lastBuzzerMillis = 0;
bool buzzerState = false;

unsigned long lastPhotoMillis = 0;
const unsigned long photoInterval = 4000; 

httpd_handle_t stream_httpd = NULL;

// ===========================
// CAMERA PINS (AI THINKER)
// ===========================
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM    5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ===========================
// STREAM SERVER HANDLER
// ===========================
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed for stream");
      res = ESP_FAIL;
    } else {
      _jpg_buf_len = fb->len;
      _jpg_buf = fb->buf;
    }

    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } 
    else if (res != ESP_OK) {
      break;
    }

    delay(50); 
  }

  return res;
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };
  
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

// ===========================
// SEND PHOTO TO TELEGRAM
// ===========================
void sendPhotoTelegram() {

  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, HIGH); 
  delay(150); 

  camera_fb_t * fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Camera capture failed");
    digitalWrite(FLASH_LED, LOW);
    return;
  }

  digitalWrite(FLASH_LED, LOW); 
  Serial.println("Sending photo...");

  WiFiClientSecure telegramClient;
  telegramClient.setInsecure();
  
  if (telegramClient.connect("api.telegram.org", 443)) {

    String head = "--boundary\r\nContent-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + 
                  String(CHAT_ID) + 
                  "\r\n--boundary\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"motion.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";

    String tail = "\r\n--boundary--\r\n";
    
    size_t total_len = head.length() + fb->len + tail.length();
    
    telegramClient.print("POST /bot" + String(BOTtoken) + "/sendPhoto HTTP/1.1\r\n");
    telegramClient.print("Host: api.telegram.org\r\n");
    telegramClient.print("Content-Type: multipart/form-data; boundary=boundary\r\n");
    telegramClient.print("Content-Length: " + String(total_len) + "\r\n\r\n");
    
    telegramClient.print(head);
    
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;

    for (size_t n = 0; n < fbLen; n = n + 1024) {

      if (n + 1024 < fbLen) {
        telegramClient.write(fbBuf, 1024);
        fbBuf += 1024;
      } 
      else if (fbLen % 1024 > 0) {
        telegramClient.write(fbBuf, fbLen % 1024);
      }
    }
    
    telegramClient.print(tail);

    Serial.println("Photo transmitted successfully!");

  } else {

    Serial.println("Connection to Telegram failed");
  }

  esp_camera_fb_return(fb); 
}

// ===========================
// HANDLE TELEGRAM COMMANDS
// ===========================
void handleNewMessages(int numNewMessages) {

  for (int i = 0; i < numNewMessages; i++) {

    String chat_id = String(bot.messages[i].chat_id);

    if (chat_id != CHAT_ID) continue;

    String text = bot.messages[i].text;

    Serial.println("Received text: " + text);

    if (awaitingPassword) {

      if (text == systemPassword) {

        systemActive = false;
        manualBuzzerMode = false;
        awaitingPassword = false;

        digitalWrite(RED_LED_PIN, LOW);     
        digitalWrite(GREEN_LED_PIN, HIGH);  
        digitalWrite(BUZZER_PIN, LOW); 

        String msg = "🔓 [ SENTINEL EYE ] 🔓\n";
        msg += "-------------------------------------\n";
        msg += "🟢 Status: DISARMED (Safe Mode)\n";
        msg += "🔑 Authorization: Password Verified\n";
        msg += "💡 Action: Security System Deactivated.";

        bot.sendMessage(CHAT_ID, msg, "");

      } else {

        awaitingPassword = false; 

        String msg = "🚫 [ SENTINEL EYE ] 🚫\n";
        msg += "-------------------------------------\n";
        msg += "❌ Status: ACCESS DENIED\n";
        msg += "🔒 Security: Invalid Password Attempt\n";
        msg += "🔴 Action: System Remains Active & Armed.";

        bot.sendMessage(CHAT_ID, msg, "");
      }

      continue; 
    }

    if (text == "/activate") {

      systemActive = true;
      motionDetected = false;
      manualBuzzerMode = false;
      awaitingPassword = false;

      digitalWrite(RED_LED_PIN, HIGH);   
      digitalWrite(GREEN_LED_PIN, LOW);  
      digitalWrite(BUZZER_PIN, LOW); 

      String msg = "🔴 [ SENTINEL EYE : ACTIVATED ] 🔴\n";
      msg += "-------------------------------------\n";
      msg += "🛰️ System Status: ARMED & RUNNING\n";
      msg += "👀 Surveillance: Scanning Environment...\n";
      msg += "📢 Protection: Active Security Monitoring Enabled.";

      bot.sendMessage(CHAT_ID, msg, "");
    }

    else if (text == "/deactivate") {

      if (!systemActive) {

        String msg = "ℹ️ [ SENTINEL EYE ] ℹ️\n";
        msg += "-------------------------------------\n";
        msg += "🟢 Status: Already Standby/Inactive.";

        bot.sendMessage(CHAT_ID, msg, "");

      } else {

        awaitingPassword = true;

        String msg = "🔑 [ AUTHENTICATION REQUIRED ] 🔑\n";
        msg += "-------------------------------------\n";
        msg += "🔒 System is currently Locked.\n";
        msg += "⌨️ Please enter the security password:";

        bot.sendMessage(CHAT_ID, msg, "");
      }
    }

    else if (text == "/buzzer_on") {

      manualBuzzerMode = true;
      digitalWrite(BUZZER_PIN, HIGH);
      
      String msg = "🔊 [ SYSTEM COMMAND ] 🔊\n";
      msg += "-------------------------------------\n";
      msg += "⚡ Action: Audio Siren Overridden\n";
      msg += "📢 Status: Security Alarm Forced ON.";

      bot.sendMessage(CHAT_ID, msg, "");
    }

    else if (text == "/buzzer_off") {

      manualBuzzerMode = false;
      digitalWrite(BUZZER_PIN, LOW);
      
      String msg = "🔇 [ SYSTEM COMMAND ] 🔇\n";
      msg += "-------------------------------------\n";
      msg += "⚡ Action: Audio Siren Muted\n";
      msg += "📴 Status: Security Alarm Turned OFF.";

      bot.sendMessage(CHAT_ID, msg, "");
    }

    else if (text == "لا") {

      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);

      manualBuzzerMode = true; 
      digitalWrite(BUZZER_PIN, HIGH);   

      String msg = "🚨 [ SECURITY BREACH CONFIRMED ] 🚨\n";
      msg += "-------------------------------------\n";
      msg += "🔴 Operator Input: Threat Verified\n";
      msg += "🔥 Alarm State: Maximum Intensity Triggered!";

      bot.sendMessage(CHAT_ID, msg, "");
    }

    else if (text == "نعم") {

      manualBuzzerMode = false;
      digitalWrite(BUZZER_PIN, LOW);     

      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH); 

      String msg = "🟢 [ SECURITY STATUS ] 🟢\n";
      msg += "-------------------------------------\n";
      msg += "✅ Operator Input: Authorized Person Confirmed\n";
      msg += "🔓 Area Status: Environment Secured.";

      bot.sendMessage(CHAT_ID, msg, "");
    }
  }
}

// ===========================
// SETUP
// ===========================
void setup() {

  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FLASH_LED, OUTPUT);

  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(FLASH_LED, LOW);

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

  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {

    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 14;
    config.fb_count = 2; 

  } else {

    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 14;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);

  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  client.setInsecure(); 

  WiFi.begin(ssid, password);

  Serial.print("WiFi connecting");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");

  startCameraServer(); 

  String welcomeMsg = "🖥️ [ SENTINEL EYE : CORE ONLINE ] 🖥️\n";
  welcomeMsg += "-------------------------------------\n";
  welcomeMsg += "🔹 /activate   : Enable Security Monitoring\n";
  welcomeMsg += "🔹 /deactivate : Disable Security System (Password Required)\n";
  welcomeMsg += "🔹 /buzzer_off : Mute Security Siren\n";
  welcomeMsg += "-------------------------------------\n";
  welcomeMsg += "🔗 Stream: http://" + WiFi.localIP().toString() + "/stream";
  
  bot.sendMessage(CHAT_ID, welcomeMsg, "");
}

// ===========================
// LOOP
// ===========================
void loop() {

  if (millis() - lastCheckTime > botRequestDelay) {

    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {

      handleNewMessages(numNewMessages);

      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    lastCheckTime = millis();
  }

  if (systemActive) {

    int motion = digitalRead(PIR_PIN); 

    if (motion == HIGH) {

      if (!manualBuzzerMode) {

        if (millis() - lastBuzzerMillis > 300) { 

          buzzerState = !buzzerState;

          digitalWrite(BUZZER_PIN, buzzerState);

          lastBuzzerMillis = millis();
        }
      }
      
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);

      if (!motionDetected) {

        Serial.println("🚨 Motion Detected - First Photo!");
        
        String msg = "⚠️ [ ALERT : MOTION DETECTED ] ⚠️\n";
        msg += "-------------------------------------\n";
        msg += "🚨 Security Zone: Movement Detected\n";
        msg += "📸 Capture: Recording Visual Evidence...";

        bot.sendMessage(CHAT_ID, msg, "");
        
        sendPhotoTelegram();

        motionDetected = true;

        lastPhotoMillis = millis();
      } 

      else if (millis() - lastPhotoMillis > photoInterval) {

        Serial.println("📸 Continuous Capture - Next Photo!");

        sendPhotoTelegram();

        lastPhotoMillis = millis();
      }
    }

    else {

      motionDetected = false;

      if (!manualBuzzerMode) {
        digitalWrite(BUZZER_PIN, LOW);
      }
    }
  }
  
  if (manualBuzzerMode) {
    digitalWrite(BUZZER_PIN, HIGH);
  }
}
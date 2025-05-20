#define LED_PIN 48
#define BUTTON_PIN 0  // Using GPIO0 as the button pin
#define SDA_PIN GPIO_NUM_11
#define SCL_PIN GPIO_NUM_12
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "Wire.h"
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT broker settings
constexpr char MQTT_BROKER[] = "test.mosquitto.org";
constexpr int MQTT_PORT = 1883;
constexpr char MQTT_TOPIC[] = "nhatminh/data";

// Default Wi-Fi credentials
constexpr char DEFAULT_WIFI_SSID[] = "TRUC ANH";
constexpr char DEFAULT_WIFI_PASSWORD[] = "23230903";

// Global variables for Wi-Fi
char WIFI_SSID[32] = "";
char WIFI_PASSWORD[64] = "";

Preferences preferences;

volatile uint32_t blinkInterval = 500;

// Task handles
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
PubSubClient client(wifiClient);

void InitWiFi();
void connectMQTT();
void loadWiFiNVS();
void mqttCallback(char* topic, byte* payload, unsigned int length);

//Task xử lý đèn LED

void ledTask(void *parameter) {
  // Khởi tạo chân LED
  pinMode(LED_PIN, OUTPUT);
  bool ledState = false;
  while (true) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    vTaskDelay(pdMS_TO_TICKS(blinkInterval));
  }
}

//Task xử lý nút nhấn

void buttonTask(void *parameter) {
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  bool lastState = HIGH; 
  unsigned long lastChangeTime = 0;  
  
  while (true) {
    bool currentState = digitalRead(BUTTON_PIN);
    if (currentState == LOW && lastState == HIGH) {
      unsigned long currentTime = millis();
      if (currentTime - lastChangeTime > 200) { 
        if (blinkInterval == 500) {
          blinkInterval = 1000;  
          Serial.println("Đã chuyển sang tốc độ chậm: 1000ms");
        } else if (blinkInterval == 1000) {
          blinkInterval = 100;   
          Serial.println("Đã chuyển sang tốc độ nhanh: 100ms");
        } else {
          blinkInterval = 500;  
          Serial.println("Đã chuyển sang tốc độ trung bình: 500ms");
        }
        
        // Send blink interval to MQTT broker
        if (client.connected()) {
          String message = "{\"blinkInterval\":" + String(blinkInterval) + "}";
          client.publish(MQTT_TOPIC, message.c_str());
        }
        
        lastChangeTime = currentTime;
      }
    }
    lastState = currentState;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// MQTT callback cho nhận tin
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  
  char message[length + 1];
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println();
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Update wifi credentials mới vào nvs
  if (doc.containsKey("wifi_config") && doc["wifi_config"].as<bool>()) {
    if (doc.containsKey("ssid") && doc.containsKey("password")) {
      preferences.begin("wifi-config", false);
      
      const char* newSsid = doc["ssid"];
      const char* newPassword = doc["password"];
      
      preferences.putString("ssid", newSsid);
      preferences.putString("password", newPassword);
      
      preferences.end();
      
      // Update current variables
      strncpy(WIFI_SSID, newSsid, sizeof(WIFI_SSID));
      strncpy(WIFI_PASSWORD, newPassword, sizeof(WIFI_PASSWORD));
      
      // thông báo (debug)
      String response = "{\"status\":\"success\",\"message\":\"WiFi credentials updated\"}";
      client.publish(MQTT_TOPIC, response.c_str());
      
      // Reconnect
      WiFi.disconnect();
      delay(1000);
      InitWiFi();
    }
  }
  
  // Test để kiểm tra thông tin đã lưu
  else if (doc.containsKey("show_wifi") && doc["show_wifi"].as<bool>()) {
    String response = "{\"status\":\"info\",\"ssid\":\"";
    response += WIFI_SSID;
    response += "\",\"connected\":";
    response += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
    response += "}";
    client.publish(MQTT_TOPIC, response.c_str());
  }
  
  // Xóa thông tin Wi-Fi trong NVS
  else if (doc.containsKey("clear_wifi") && doc["clear_wifi"].as<bool>()) {
    preferences.begin("wifi-config", false);
    preferences.clear();
    preferences.end();
    
    // Cập nhật biến trong bộ nhớ với thông tin mặc định
    strncpy(WIFI_SSID, DEFAULT_WIFI_SSID, sizeof(WIFI_SSID));
    strncpy(WIFI_PASSWORD, DEFAULT_WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    String response = "{\"status\":\"success\",\"message\":\"WiFi credentials cleared, default values restored\"}";
    client.publish(MQTT_TOPIC, response.c_str());
    
    // Tùy chọn: Kết nối lại với thông tin mặc định
    WiFi.disconnect();
    delay(1000);
    InitWiFi();
  }
}

// Load Wi-Fi từ NVS
void loadWiFiNVS() {
  preferences.begin("wifi-config", false);
  preferences.getString("ssid", WIFI_SSID, sizeof(WIFI_SSID));
  preferences.getString("password", WIFI_PASSWORD, sizeof(WIFI_PASSWORD));

  if (strlen(WIFI_SSID) == 0) {
    Serial.println("Ko tìm thấy thông tin Wi-Fi trong NVS, sử dụng thông tin mặc định");
    strncpy(WIFI_SSID, DEFAULT_WIFI_SSID, sizeof(WIFI_SSID));
    strncpy(WIFI_PASSWORD, DEFAULT_WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
    
    preferences.putString("ssid", WIFI_SSID);
    preferences.putString("password", WIFI_PASSWORD);
  }
  
  preferences.end();
  Serial.println("WiFi load từ NVS");
}

// Kết nối tới MQTT broker
void connectMQTT() {
  client.setServer(MQTT_BROKER, MQTT_PORT);
  client.setCallback(mqttCallback);
  
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("nhatminh/control");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

//Hàm khởi tạo Wi-Fi
void InitWiFi() {
  Serial.println("Connecting to AP ...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi!");
    // Nếu không kết nối được với Wi-Fi, thử lại với thông tin mặc định
    if (strcmp(WIFI_SSID, DEFAULT_WIFI_SSID) != 0) {
      Serial.println("Thử load wifi mặc định");
      strncpy(WIFI_SSID, DEFAULT_WIFI_SSID, sizeof(WIFI_SSID));
      strncpy(WIFI_PASSWORD, DEFAULT_WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      
      startAttemptTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
        delay(500);
        Serial.print(".");
      }
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Đã connect");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Connect fail");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Khởi tạo các task FreeRTOS cho LED và nút nhấn
  xTaskCreate(ledTask, "LED Task", 1024, NULL, 1, &ledTaskHandle);
  
  xTaskCreate(buttonTask,"Button Task", 2048, NULL, 1, &buttonTaskHandle);
  
  Serial.println("LED and Button tasks created");
  
  loadWiFiNVS();
  InitWiFi();
  Wire.begin(SDA_PIN, SCL_PIN);
  if (WiFi.status() == WL_CONNECTED) {
    connectMQTT();
  }
  if (client.connected()) {
    String message = "{\"status\":\"online\",\"blinkInterval\":" + String(blinkInterval) + "}";
    client.publish(MQTT_TOPIC, message.c_str());
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      connectMQTT();
    }
    client.loop();
  } else {
    static unsigned long lastWifiRetry = 0;
    if (millis() - lastWifiRetry > 30000) { 
      lastWifiRetry = millis();
      InitWiFi();
    }
  }
}
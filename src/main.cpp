#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Định nghĩa các chân
#define LED_PIN 48
#define BUTTON_PIN 0

volatile uint32_t blinkInterval = 500;  // Tốc độ nhấp nháy ban đầu (ms)

// Task handles
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t buttonTaskHandle = NULL;

/**
 * Task xử lý đèn LED
 */
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

/**
 * Task xử lý nút nhấn
 */
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
        
        lastChangeTime = currentTime;
      }
    }
    lastState = currentState;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("FreeRTOS Button and LED Tasks Demo");
  delay(1000); 
  
  // Tạo task điều khiển LED
  xTaskCreate(
    ledTask, 
    "LED Task",    
    1024,   
    NULL,        
    1,              
    &ledTaskHandle  
  );
  Serial.println("LED task created");
  
  // Tạo task xử lý nút nhấn
  xTaskCreate(
    buttonTask,
    "Button Task", 
    2048,        
    NULL,           
    1,              
    &buttonTaskHandle 
  );
  Serial.println("Button task created");
  
  // Thông báo khởi động hoàn tất
  Serial.println("Setup complete - system running...");
}

void loop() {
}

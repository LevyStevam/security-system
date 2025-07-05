#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

#define LED1 5
#define FLAME_SENSOR_PIN 15
#define BUZZER_PIN 13  
#define BOTtoken "7775986434:AAEoANBcpjdM8R7VMiP6PWNmCrJyeoHgWLk"
#define CHAT_ID "2013267800"
const char* ssid = "brisa-2475083";
const char* password = "anppveoe";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

volatile bool fireDetected = false;
volatile unsigned long fireDetectedTime = 0;

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }

    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (text == "/start") {
      String welcome = "Welcome, " + from_name + ".\n";
      welcome += "Use the following commands to control your outputs.\n\n";
      bot.sendMessage(chat_id, welcome, "");
    }
  }
}

void flameMonitorTask(void *parameter) {
  pinMode(FLAME_SENSOR_PIN, INPUT);
  ledcSetup(0, 2000, 8);           
  ledcAttachPin(BUZZER_PIN, 0); 

  while (true) {
    int flame = digitalRead(FLAME_SENSOR_PIN);
    
    if (flame == LOW && !fireDetected) {
      fireDetected = true;
      fireDetectedTime = millis();
      Serial.println("🔥 FOGO DETECTADO!");
      ledcWrite(0, 127);  
    }

    
    if (fireDetected && millis() - fireDetectedTime > 5000) {
      fireDetected = false;
      ledcWrite(0, 0);  
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  pinMode(LED1, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  
  xTaskCreate(
    flameMonitorTask,      
    "Flame Monitor",       
    10000,                 
    NULL,                  
    1,                     
    NULL                   
  );
}

void loop() {
  
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  static bool alertSent = false;
  if (fireDetected && !alertSent) {
    bot.sendMessage(CHAT_ID, "🔥 Casa pegando fogo!", "");
    alertSent = true;
  } else if (!fireDetected) {
    alertSent = false;  
  }
}

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ----- DEFINIÇÕES -----
#define LED1 5
#define FLAME_SENSOR_PIN 15
#define BUZZER_PIN 13  
#define BOTtoken "7775986434:AAEoANBcpjdM8R7VMiP6PWNmCrJyeoHgWLk"
#define CHAT_ID "2013267800"
const char* ssid = "brisa-2475083";
const char* password = "anppveoe";

// ----- OBJETOS -----
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

// Variável compartilhada entre tasks
volatile bool fireDetected = false;
volatile unsigned long fireDetectedTime = 0;

// ----- FUNÇÃO DE MENSAGENS -----
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

// ----- TASK DE MONITORAMENTO DO SENSOR -----
void flameMonitorTask(void *parameter) {
  pinMode(FLAME_SENSOR_PIN, INPUT);
  ledcSetup(0, 2000, 8);           // Canal 0, 2kHz, 8 bits
  ledcAttachPin(BUZZER_PIN, 0); 

  while (true) {
    int flame = digitalRead(FLAME_SENSOR_PIN);
    
    if (flame == LOW && !fireDetected) {
      fireDetected = true;
      fireDetectedTime = millis();
      Serial.println("🔥 FOGO DETECTADO!");
      ledcWrite(0, 127);  // Ativa buzzer
    }

    // Se já detectou fogo, aguarda 500ms antes de resetar
    if (fireDetected && millis() - fireDetectedTime > 500) {
      fireDetected = false;
      ledcWrite(0, 0);  // Desliga buzzer
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); // Verifica a cada 50ms
  }
}

// ----- SETUP -----
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

  // Cria a task para monitorar fogo e buzzer
  xTaskCreate(
    flameMonitorTask,      // Função da task
    "Flame Monitor",       // Nome da task
    10000,                 // Tamanho da stack
    NULL,                  // Parâmetro
    1,                     // Prioridade
    NULL                   // Handle da task
  );
}

// ----- LOOP -----
void loop() {
  // Verifica comandos no bot
  if (millis() > lastTimeBotRan + botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // Se o fogo for detectado, envia a mensagem (uma vez por ocorrência)
  static bool alertSent = false;
  if (fireDetected && !alertSent) {
    bot.sendMessage(CHAT_ID, "🔥 Casa pegando fogo!", "");
    alertSent = true;
  } else if (!fireDetected) {
    alertSent = false;  // Reset para próxima ocorrência
  }
}

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <HTTPClient.h> 

#define LED1 5
#define FLAME_SENSOR_PIN 15
#define BUZZER_PIN 13  
#define BOTtoken "7775986434:AAEoANBcpjdM8R7VMiP6PWNmCrJyeoHgWLk"
#define CHAT_ID "2013267800"
const char* ssid = "iPhone Jorge";
const char* password = "julia123";

// =====================
// Configura o IP local do servidor com a imagem
const char* endpoint_url = "http://172.20.10.8:8000/human"; // <-- Troque isso!
// =====================

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

volatile bool fireDetected = false;
volatile unsigned long fireDetectedTime = 0;

// ----- Função para baixar a imagem -----
bool baixarImagem(const char* url, std::vector<uint8_t>& imagemBytes) {
  HTTPClient http;
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == 200) {
    WiFiClient* stream = http.getStreamPtr();
    while (http.connected() && stream->available()) {
      imagemBytes.push_back(stream->read());
    }
    http.end();
    return true;
  }

  Serial.printf("Erro HTTP GET imagem: %d\n", httpCode);
  http.end();
  return false;
}

// ----- Função para enviar imagem ao Telegram -----
bool enviarImagemTelegram(const std::vector<uint8_t>& imagemBytes) {
  WiFiClientSecure clientTelegram;
  clientTelegram.setInsecure();  // Cuidado em produção

  String boundary = "boundaryESP32";
  String url = "/bot" + String(BOTtoken) + "/sendPhoto";

  if (!clientTelegram.connect("api.telegram.org", 443)) {
    Serial.println("Falha ao conectar no Telegram");
    return false;
  }

  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
    String(CHAT_ID) + "\r\n" +
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"photo\"; filename=\"foto.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";

  String tail = "\r\n--" + boundary + "--\r\n";

  int contentLength = head.length() + imagemBytes.size() + tail.length();

  clientTelegram.println("POST " + url + " HTTP/1.1");
  clientTelegram.println("Host: api.telegram.org");
  clientTelegram.println("Content-Type: multipart/form-data; boundary=" + boundary);
  clientTelegram.println("Content-Length: " + String(contentLength));
  clientTelegram.println();
  clientTelegram.print(head);

  for (size_t i = 0; i < imagemBytes.size(); i++) {
    clientTelegram.write(imagemBytes[i]);
  }

  clientTelegram.print(tail);

  while (clientTelegram.connected()) {
    String line = clientTelegram.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = clientTelegram.readString();
  Serial.println("Resposta do Telegram:");
  Serial.println(response);
  return response.indexOf("\"ok\":true") >= 0;
}

// ----- Função para lidar com mensagens -----
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
      String welcome = "Bem-vindo, " + from_name + ".\n";
      welcome += "Comandos disponíveis:\n";
      welcome += "/pessoas - Capturar imagem da câmera\n /pet - Verificar presença do pet\n /pessoas_qtd - Contar pessoas na imagem\n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/pessoas") {
      bot.sendMessage(chat_id, "📷 Capturando imagem...", "");
      std::vector<uint8_t> imagem;
      if (baixarImagem(endpoint_url, imagem)) {
        if (enviarImagemTelegram(imagem)) {
          Serial.println("Imagem enviada com sucesso.");
        } else {
          bot.sendMessage(chat_id, "❌ Falha ao enviar imagem.", "");
        }
      } else {
        bot.sendMessage(chat_id, "❌ Não consegui baixar a imagem.", "");
      }
    }

    if (text == "/pet") {
      bot.sendMessage(chat_id, "🔍 Verificando presença do pet...", "");

      HTTPClient http;
      WiFiClient clientHttp;
      http.begin(clientHttp, "http://172.20.10.8:8000/pet");  // Corrigido

      int httpCode = http.GET();

      if (httpCode == 200) {
        String payload = http.getString();

        // Parse do JSON
        DynamicJsonDocument doc(2048); // pode ajustar o tamanho se necessário
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          bot.sendMessage(chat_id, "❌ Erro ao interpretar a resposta do servidor.", "");
        } else {
          JsonArray predictions = doc["predictions"];
          if (!predictions.isNull() && predictions.size() > 0) {
            bot.sendMessage(chat_id, "🐾 Seu pet está no alcance da câmera!", "");
          } else {
            bot.sendMessage(chat_id, "📷 Seu pet **não** está no alcance da câmera.", "");
          }
        }
      } else {
        bot.sendMessage(chat_id, "❌ Falha ao acessar o servidor pet.", "");
        Serial.printf("Erro HTTP GET /pet: %d\n", httpCode);
      }

      http.end();
    }
    if (text == "/pessoas_qtd") {
      bot.sendMessage(chat_id, "🔍 Contando humanos na imagem...", "");

      HTTPClient http;
      WiFiClient clientHttp;
      http.begin(clientHttp, "http://172.20.10.8:8000/quantity");  // mesma estrutura do /pet

      int httpCode = http.GET();

      if (httpCode == 200) {
        String qtd = http.getString();
        qtd.trim(); // remove \n ou espaço

        if (qtd == "0") {
          bot.sendMessage(chat_id, "🚫 Nenhum humano foi detectado na imagem.", "");
        } else if (qtd == "1") {
          bot.sendMessage(chat_id, "🚶‍♂️ Foi detectado 1 humano na imagem.", "");
        } else {
          bot.sendMessage(chat_id, "🚶‍♂️ Foram detectados " + qtd + " humanos na imagem.", "");
        }

      } else {
        bot.sendMessage(chat_id, "❌ Não consegui obter a quantidade de pessoas.", "");
        Serial.printf("Erro HTTP GET /quantity: %d\n", httpCode);
      }

      http.end();
    }

  }
}


// ----- Task de monitoramento -----
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

// ----- Setup -----
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

  Serial.println("\nConectado ao Wi-Fi");
  Serial.print("IP local: ");
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

// ----- Loop -----
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


#include <WiFi.h>
#include <PubSubClient.h> // Biblioteca MQTT para fins educacionais

// Macro para escolher entre simulação e hardware real
#define SIMULATION_MODE 0  // 1 para simulação, 0 para hardware real

// Configurações WiFi
const char* ssid = "Alan Kardek";
const char* password = "87651234";

// Configurações MQTT
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic_led = "CAT341/LEDControl";
const char* mqtt_topic_status = "CAT341/status";
const char* mqtt_topic_adc = "CAT341/ADC"; // Novo tópico para dados ADC

// Configurações da Serial2 - ESP32 DevKit C
#define RX2_PIN 16  // GPIO16 é o pino RX2 no ESP32 DevKit C
#define TX2_PIN 17  // GPIO17 é o pino TX2 no ESP32 DevKit C
#define SERIAL2_BAUD 9600

// Pin do LED
#define LedPin 5

// Cliente WiFi para o servidor web
WiFiClient espClient;
WiFiServer server(80);

// Cliente MQTT
PubSubClient mqttClient(espClient);

// Variáveis para controle de tempo
unsigned long lastMsgTime = 0;
unsigned long lastCheckTime = 0;
char msg[50];
bool mqttConnected = false;

// Variáveis para leitura da porta serial
String serialBuffer = "";      // Buffer para Serial principal (depuração)
String serial2Buffer = "";     // Buffer para Serial2 (dados ADC)
bool stringComplete = false;   // Flag para Serial principal
bool string2Complete = false;  // Flag para Serial2
String adcValue = "";          // Para armazenar o valor extraído do ADC

// Esta função é chamada quando uma mensagem MQTT é recebida
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem MQTT recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  // Controla o LED com base na mensagem
  if (message == "1") {
    digitalWrite(LedPin, HIGH);
    Serial.println("LED LIGADO via MQTT");
  } else if (message == "2" || message == "0") {
    digitalWrite(LedPin, LOW);
    Serial.println("LED DESLIGADO via MQTT");
  }
}

// Função para conectar/reconectar ao broker MQTT
void reconnect() {
  // Evita tentativas repetidas muito rápidas
  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();
  
  if (!mqttConnected && (now - lastReconnectAttempt > 5000)) {
    lastReconnectAttempt = now;
    
    Serial.print("Tentando conexão MQTT com ");
    Serial.print(mqtt_server);
    Serial.println("...");
    
    // Cria um ID de cliente aleatório
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Tenta conectar
    if (SIMULATION_MODE == 0 && mqttClient.connect(clientId.c_str())) {
      mqttConnected = true;
      Serial.println("Conectado ao broker MQTT!");
      
      // Publica mensagem de status
      mqttClient.publish(mqtt_topic_status, "ESP32 online");
      Serial.print("Publicando no topico [");
      Serial.print(mqtt_topic_status);
      Serial.println("]: ESP32 online");
      
      // Inscreve no tópico de controle
      mqttClient.subscribe(mqtt_topic_led);
      Serial.print("Inscrito no topico: ");
      Serial.println(mqtt_topic_led);
    } else if (SIMULATION_MODE == 1) {
      // Em modo de simulação, apenas fingimos estar conectados
      mqttConnected = true;
      Serial.println("Simulando conexão MQTT!");
    } else {
      Serial.print("Falha na conexão MQTT, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" tentando novamente em 5 segundos");
    }
  }
}

void setup() {
  // Configura o LED
  pinMode(LedPin, OUTPUT);
  
  // Inicializa a Serial principal (USB) para depuração
  Serial.begin(115200);
  Serial.println("Inicializando...");
  
  // Inicializa a Serial2 para receber dados ADC
  Serial2.begin(SERIAL2_BAUD, SERIAL_8N1, RX2_PIN, TX2_PIN);
  
  // Inicializa os buffers seriais
  serialBuffer.reserve(50);
  serial2Buffer.reserve(50);
  adcValue.reserve(10);
  
  // Conecta ao WiFi
  Serial.println();
  Serial.print("Conectando ao WiFi ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
  
  // Configura o cliente MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
  
  // Inicia o servidor web (usado em ambos os modos para diagnóstico)
  server.begin();
  
  // Inicializa o gerador de números aleatórios
  randomSeed(micros());
  
  Serial.println(SIMULATION_MODE ? "Rodando em modo de SIMULAÇÃO" : "Rodando em modo de HARDWARE REAL");
  Serial.println("Servidor HTTP iniciado na porta 80");
  Serial.println("Endpoint ADC JSON disponível em /adc-json");
  
  // Informações sobre as portas seriais
  Serial.println("Configuração das portas seriais:");
  Serial.println("Serial (USB): Depuração apenas");
  Serial.println("Serial2: Recebendo dados ADC");
  Serial.print("  - Pino RX2: GPIO");
  Serial.println(RX2_PIN);
  Serial.print("  - Pino TX2: GPIO");
  Serial.println(TX2_PIN);
  Serial.print("  - Baud Rate: ");
  Serial.println(SERIAL2_BAUD);
  Serial.println("Esperando dados ADC no formato \"/\\*1234/\" na Serial2...");
}

void loop() {
  // Modo de hardware real: usa MQTT diretamente
  if (SIMULATION_MODE == 0) {
    if (!mqttClient.connected()) {
      reconnect();
    }
    mqttClient.loop();
  } else {
    // Modo simulação: verifica a conexão MQTT simulada
    if (!mqttConnected) {
      reconnect();
    }
  }
  
  // Lê a porta serial principal (apenas para depuração)
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    serialBuffer += inChar;
    
    // Se encontramos um final de linha, marque a string como completa
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
  
  // Processa a string recebida da serial principal
  if (stringComplete) {
    Serial.print("Comando de depuração recebido: ");
    Serial.println(serialBuffer);
    
    // Limpa o buffer para a próxima leitura
    serialBuffer = "";
    stringComplete = false;
  }
  
  // Lê a porta Serial2 (dados ADC)
  while (Serial2.available()) {
    char inChar = (char)Serial2.read();
    serial2Buffer += inChar;
    
    // Se encontramos um final de linha, marque a string como completa
    if (inChar == '\n') {
      string2Complete = true;
    }
  }
  
  // Processa a string recebida da Serial2
  if (string2Complete) {
    // Verifica se a string está no formato "/\*1002/"
    if (serial2Buffer.startsWith("/") && serial2Buffer.indexOf("*") > 0) {
      Serial.print("Dados ADC recebidos na Serial2: ");
      Serial.println(serial2Buffer);
      
      // Remove o '\n' do final
      serial2Buffer.trim();
      
      // Extrai apenas os 4 caracteres numéricos após o asterisco
      int asteriskPos = serial2Buffer.indexOf("*");
      int slashPos = serial2Buffer.lastIndexOf("/");
      
      if (asteriskPos >= 0 && slashPos > asteriskPos) {
        // Extrair os caracteres após o asterisco e antes da última barra
        adcValue = serial2Buffer.substring(asteriskPos + 1, slashPos);
        Serial.print("Valor ADC extraído: ");
        Serial.println(adcValue);
        
        // Atualiza o valor na web
        Serial.println("Valor ADC disponível no endpoint /adc-json");
      }
      
      // Publica no tópico ADC via MQTT
      if (SIMULATION_MODE == 0) {
        // Publicação MQTT real
        mqttClient.publish(mqtt_topic_adc, adcValue.c_str());
      } else {
        // Simulação de publicação MQTT via console
        Serial.print("Publicando no topico [");
        Serial.print(mqtt_topic_adc);
        Serial.print("]: ");
        Serial.println(adcValue);
      }
    }
    
    // Limpa o buffer para a próxima leitura
    serial2Buffer = "";
    string2Complete = false;
  }
  
  // Publica uma mensagem de status a cada 30 segundos
  unsigned long now = millis();
  if (now - lastMsgTime > 30000) {
    lastMsgTime = now;
    
    snprintf(msg, 50, "LED está %s", digitalRead(LedPin) ? "ligado" : "desligado");
    Serial.print("Publicando status: ");
    Serial.println(msg);
    
    if (SIMULATION_MODE == 0) {
      // Publicação MQTT real
      mqttClient.publish(mqtt_topic_status, msg);
    } else {
      // Simulação de publicação MQTT
      Serial.print("Publicando no topico [");
      Serial.print(mqtt_topic_status);
      Serial.print("]: ");
      Serial.println(msg);
    }
  }
  
  // Código do servidor web (esta parte funciona em ambos os modos)
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("Novo cliente web");
    String currentLine = "";
    bool isJsonRequest = false;
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (c == '\n') {
          // Verifica se a solicitação é para o endpoint JSON
          if (currentLine.indexOf("GET /adc-json") >= 0) {
            isJsonRequest = true;
          }
          
          if (currentLine.length() == 0) {
            // Se for solicitação JSON, responda com JSON
            if (isJsonRequest) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: application/json");
              client.println("Access-Control-Allow-Origin: *"); // Permitir CORS
              client.println("Connection: close");
              client.println();
              
              // Resposta JSON simples
              client.print("{\"adc_value\":\"");
              client.print(adcValue);
              client.print("\",\"timestamp\":");
              client.print(millis()/1000);
              client.println("}");
            } else {
              // Resposta HTML normal
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              
              // HTML do servidor web com valor ADC incluído
              client.println("<html><head>");
              client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<meta http-equiv=\"refresh\" content=\"5\">"); // Recarrega a cada 5 segundos
              client.println("<style>body { font-family: Arial; text-align: center; }");
              client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px;");
              client.println("text-align: center; font-size: 16px; margin: 4px 2px; cursor: pointer; }");
              client.println(".button-off { background-color: #f44336; }");
              client.println(".adc-value { font-size: 24px; font-weight: bold; margin: 20px 0; padding: 10px; background-color: #f0f0f0; border-radius: 5px; }");
              client.println("</style></head><body>");
              client.println("<h1>ESP32 MQTT Demo</h1>");
              client.println("<p>Controle o LED pelo navegador ou via MQTT (test.mosquitto.org)</p>");
              client.println("<a href=\"/LEDControl/1\"><button class=\"button\">LIGAR LED</button></a>");
              client.println("<a href=\"/LEDControl/0\"><button class=\"button button-off\">DESLIGAR LED</button></a>");
              
              // Exibe o valor ADC na página web se disponível
              if (adcValue.length() > 0) {
                client.println("<div class=\"adc-value\">Último valor ADC: ");
                client.println(adcValue);
                client.println("</div>");
                client.println("<p><small>Atualizado em: " + String(millis()/1000) + "s</small></p>");
              } else {
                client.println("<div class=\"adc-value\">Aguardando leitura ADC...</div>");
              }
              
              client.println("<p>Topico MQTT: CAT341/LEDControl</p>");
              client.println("<p>Envie '1' para ligar, '0' para desligar</p>");
              client.println("<p>Topico ADC: CAT341/ADC</p>");
              client.println("<p>Dados ADC disponíveis em JSON: <a href=\"/adc-json\">/adc-json</a></p>");
              client.println("<p><small>Serial2 RX: GPIO" + String(RX2_PIN) + ", TX: GPIO" + String(TX2_PIN) + "</small></p>");
              client.println("</body></html>");
            }
            
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
        
        // Processa comandos do servidor web
        if (currentLine.endsWith("GET /LEDControl/1")) {
          digitalWrite(LedPin, HIGH);
          Serial.println("LED LIGADO via Web");
          
          if (SIMULATION_MODE == 0) {
            // Publica mensagem MQTT real quando LED é ligado
            mqttClient.publish(mqtt_topic_led, "1");
          } else {
            // Simula a publicação de uma mensagem MQTT
            Serial.print("Publicando no topico [");
            Serial.print(mqtt_topic_led);
            Serial.println("]: 1");
          }
        }
        if (currentLine.endsWith("GET /LEDControl/0")) {
          digitalWrite(LedPin, LOW);
          Serial.println("LED DESLIGADO via Web");
          
          if (SIMULATION_MODE == 0) {
            // Publica mensagem MQTT real quando LED é desligado
            mqttClient.publish(mqtt_topic_led, "0");
          } else {
            // Simula a publicação de uma mensagem MQTT
            Serial.print("Publicando no topico [");
            Serial.print(mqtt_topic_led);
            Serial.println("]: 0");
          }
        }
      }
    }
    
    client.stop();
    Serial.println("Cliente web desconectado");
  }
}

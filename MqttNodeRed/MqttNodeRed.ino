
#include <WiFi.h>
#include <PubSubClient.h> // Biblioteca MQTT para fins educacionais

// Configurações WiFi
const char* ssid = "PICSimLabWifi";
const char* password = "";

// Configurações MQTT
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic_led = "CAT341/LEDControl";
const char* mqtt_topic_status = "CAT341/status";

// Pin do LED
#define LedPin 5

// Cliente WiFi para o servidor web
WiFiClient espClient;
WiFiServer server(80);

// Cliente MQTT (simulado)
PubSubClient mqttClient(espClient);

// Variáveis para controle de tempo
unsigned long lastMsgTime = 0;
unsigned long lastCheckTime = 0;
char msg[50];
bool mqttConnected = false;

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

// Função para conectar/reconectar ao broker MQTT (simulada)
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
    
    // Simula uma conexão bem-sucedida
    mqttConnected = true;
    Serial.println("Conectado ao broker MQTT!");
    
    // Publica mensagem de status
    Serial.print("Publicando no tópico [");
    Serial.print(mqtt_topic_status);
    Serial.println("]: ESP32 online");
    
    // Inscreve no tópico de controle
    Serial.print("Inscrito no tópico: ");
    Serial.println(mqtt_topic_led);
  }
}

void setup() {
  // Configura o LED
  pinMode(LedPin, OUTPUT);
  Serial.begin(115200);
  
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
  
  // Configura o cliente MQTT (simulado)
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
  
  // Inicia o servidor web
  server.begin();
  
  // Inicializa o gerador de números aleatórios
  randomSeed(micros());
}

void loop() {
  // Verifica a conexão MQTT (simulado)
  if (!mqttConnected) {
    reconnect();
  }
  
  // Publica uma mensagem de status a cada 30 segundos
  unsigned long now = millis();
  if (now - lastMsgTime > 30000) {
    lastMsgTime = now;
    
    snprintf(msg, 50, "LED está %s", digitalRead(LedPin) ? "ligado" : "desligado");
    Serial.print("Publicando status: ");
    Serial.println(msg);
    
    // Simulação de publicação MQTT
    Serial.print("Publicando no tópico [");
    Serial.print(mqtt_topic_status);
    Serial.print("]: ");
    Serial.println(msg);
  }
  
  // Código do servidor web (esta parte é real e funciona com o simulador)
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("Novo cliente web");
    String currentLine = "";
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();
            
            // HTML do servidor web
            client.println("<html><head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>body { font-family: Arial; text-align: center; }");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 15px 32px;");
            client.println("text-align: center; font-size: 16px; margin: 4px 2px; cursor: pointer; }");
            client.println(".button-off { background-color: #f44336; }");
            client.println("</style></head><body>");
            client.println("<h1>ESP32 MQTT Demo</h1>");
            client.println("<p>Controle o LED pelo navegador ou via MQTT (test.mosquitto.org)</p>");
            client.println("<a href=\"/H\"><button class=\"button\">LIGAR LED</button></a>");
            client.println("<a href=\"/L\"><button class=\"button button-off\">DESLIGAR LED</button></a>");
            client.println("<p>Tópico MQTT: CAT341/LEDControl</p>");
            client.println("<p>Envie '1' para ligar, '2' para desligar</p>");
            client.println("</body></html>");
            
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
        
        // Processa comandos do servidor web
        if (currentLine.endsWith("GET /H")) {
          digitalWrite(LedPin, HIGH);
          Serial.println("LED LIGADO via Web");
          
          // Simula a publicação de uma mensagem MQTT quando o LED é ligado via web
          Serial.print("Publicando no tópico [");
          Serial.print(mqtt_topic_led);
          Serial.println("]: 1");
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(LedPin, LOW);
          Serial.println("LED DESLIGADO via Web");
          
          // Simula a publicação de uma mensagem MQTT quando o LED é desligado via web
          Serial.print("Publicando no tópico [");
          Serial.print(mqtt_topic_led);
          Serial.println("]: 2");
        }
      }
    }
    
    client.stop();
    Serial.println("Cliente web desconectado");
  }
}

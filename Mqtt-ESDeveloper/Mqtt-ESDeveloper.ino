#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

// =====================================================
// CONFIGURACOES
// =====================================================

// 0 = MQTT real
// 1 = MQTT simulado, mantendo Wi-Fi e servidor web
#define SIMULATION_MODE 0

// Wi-Fi
const char* ssid = "AulaESP32";
const char* password = "87651234";

// MQTT
const char* mqtt_server = "broker.hivemq.com";
const uint16_t mqtt_port = 1883;

const char* mqtt_topic_led = "CAT341/LEDControl";
const char* mqtt_topic_status = "CAT341/status";

// LED: mantido conforme seu exemplo funcional
#define LedPin 17

// TFT da ES32Lab
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1

#define TFT_SCK   18
#define TFT_MISO  19
#define TFT_MOSI  23

// Intervalos
const unsigned long STATUS_INTERVAL = 30000;
const unsigned long MQTT_RETRY_INTERVAL = 5000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;
const unsigned long PAGE_INTERVAL = 6000;
const unsigned long SCREEN_INTERVAL = 1000;

// =====================================================
// OBJETOS
// =====================================================

Adafruit_ST7735 tela(TFT_CS, TFT_DC, TFT_RST);

WiFiClient espClient;
WiFiServer server(80);
PubSubClient mqttClient(espClient);

// =====================================================
// VARIAVEIS
// =====================================================

unsigned long lastMsgTime = 0;
unsigned long lastReconnectAttempt = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastScreenTime = 0;
unsigned long lastPageTime = 0;

unsigned long recebidas = 0;
unsigned long publicadas = 0;

bool mqttSimuladoConectado = false;
bool telaPendente = true;
bool wifiEstavaConectado = false;

uint8_t pagina = 0;

String clientId;
String ultimaRx = "--";
String ultimaTx = "--";
String origemLED = "Inicial";
String eventoMQTT = "Aguardando conexao";

// =====================================================
// ESTADO DA CONEXAO
// =====================================================

bool mqttAtivo() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (SIMULATION_MODE) {
    return mqttSimuladoConectado;
  }

  return mqttClient.connected();
}

// =====================================================
// DISPLAY
// =====================================================

// Fonte padrao: aproximadamente 6 x 8 pixels.
// O texto e limitado a largura do display.
void escreverLinha(int16_t y,
                   const String& texto,
                   uint16_t cor) {
  int colunas = tela.width() / 6;

  tela.setCursor(0, y);
  tela.setTextColor(cor);
  tela.print(texto.substring(0, colunas));
}

void atualizarTela() {
  unsigned long agora = millis();

  if (agora - lastPageTime >= PAGE_INTERVAL) {
    lastPageTime = agora;
    pagina = (pagina + 1) % 2;
    telaPendente = true;
  }

  if (!telaPendente &&
      agora - lastScreenTime < SCREEN_INTERVAL) {
    return;
  }

  lastScreenTime = agora;
  telaPendente = false;

  tela.fillScreen(ST77XX_BLACK);
  tela.setTextSize(1);
  tela.setTextWrap(false);

  bool wifiOK = WiFi.status() == WL_CONNECTED;
  bool mqttOK = mqttAtivo();

  escreverLinha(
    0,
    "ESP32 - MQTT / CAT341",
    ST77XX_CYAN
  );

  if (pagina == 0) {
    escreverLinha(
      12,
      SIMULATION_MODE
        ? "MODO: MQTT SIMULADO"
        : "MODO: MQTT REAL",
      ST77XX_YELLOW
    );

    escreverLinha(
      24,
      wifiOK
        ? "WiFi: CONECTADO"
        : "WiFi: CONECTANDO",
      wifiOK ? ST77XX_GREEN : ST77XX_RED
    );

    escreverLinha(
      36,
      "Rede: " + String(ssid),
      ST77XX_WHITE
    );

    escreverLinha(
      48,
      "IP: " +
        (wifiOK
          ? WiFi.localIP().toString()
          : String("--")),
      ST77XX_WHITE
    );

    escreverLinha(
      60,
      "Broker: " + String(mqtt_server),
      ST77XX_WHITE
    );

    escreverLinha(
      72,
      "Porta: " + String(mqtt_port),
      ST77XX_WHITE
    );

    escreverLinha(
      84,
      mqttOK
        ? "MQTT: CONECTADO"
        : "MQTT: DESCONECTADO",
      mqttOK ? ST77XX_GREEN : ST77XX_RED
    );

    escreverLinha(
      96,
      digitalRead(LedPin)
        ? "LED: LIGADO"
        : "LED: DESLIGADO",
      ST77XX_YELLOW
    );

    escreverLinha(
      108,
      "Origem: " + origemLED,
      ST77XX_WHITE
    );

    escreverLinha(
      120,
      "1/2 Web: acesse o IP",
      ST77XX_CYAN
    );
  } else {
    escreverLinha(
      12,
      "Topico de controle:",
      ST77XX_WHITE
    );

    escreverLinha(
      24,
      String(mqtt_topic_led),
      ST77XX_CYAN
    );

    escreverLinha(
      36,
      "1=LIGA  0 ou 2=DESLIGA",
      ST77XX_YELLOW
    );

    escreverLinha(
      48,
      "RX: " + ultimaRx,
      ST77XX_WHITE
    );

    escreverLinha(
      60,
      "Topico de status:",
      ST77XX_WHITE
    );

    escreverLinha(
      72,
      String(mqtt_topic_status),
      ST77XX_CYAN
    );

    escreverLinha(
      84,
      "TX: " + ultimaTx,
      ST77XX_WHITE
    );

    escreverLinha(
      96,
      "RX:" + String(recebidas) +
      " TX:" + String(publicadas),
      ST77XX_WHITE
    );

    escreverLinha(
      108,
      eventoMQTT,
      ST77XX_YELLOW
    );

    escreverLinha(
      120,
      "2/2 Status a cada 30s",
      ST77XX_CYAN
    );
  }
}

// =====================================================
// CONTROLE DO LED
// =====================================================

void controlarLED(bool ligado, const char* origem) {
  digitalWrite(LedPin, ligado ? HIGH : LOW);

  origemLED = origem;
  telaPendente = true;

  Serial.print("LED ");
  Serial.print(ligado ? "LIGADO" : "DESLIGADO");
  Serial.print(" via ");
  Serial.println(origem);
}

// =====================================================
// PUBLICACAO MQTT
// =====================================================

// O contador TX indica publicacoes aceitas pela
// biblioteca, ou simuladas. Nao confirma recebimento
// pelo broker.
bool publicar(const char* topico,
              const char* mensagem) {
  bool sucesso = false;

  if (SIMULATION_MODE) {
    sucesso = mqttAtivo();
  } else if (mqttAtivo()) {
    sucesso = mqttClient.publish(topico, mensagem);
  }

  if (sucesso) {
    publicadas++;
    ultimaTx = mensagem;

    eventoMQTT = SIMULATION_MODE
      ? "TX simulada"
      : "TX enviada";

    Serial.print(
      SIMULATION_MODE
        ? "[SIM] Publicando ["
        : "Publicando ["
    );

    Serial.print(topico);
    Serial.print("]: ");
    Serial.println(mensagem);
  } else {
    eventoMQTT = "Falha ao publicar";

    Serial.print("Publicacao nao enviada [");
    Serial.print(topico);
    Serial.println("]");
  }

  telaPendente = true;
  return sucesso;
}

// =====================================================
// RECEPCAO MQTT
// =====================================================

void callback(char* topic,
              byte* payload,
              unsigned int length) {
  String mensagem;
  mensagem.reserve(length);

  for (unsigned int i = 0; i < length; i++) {
    mensagem += (char)payload[i];
  }

  Serial.print("Mensagem MQTT recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(mensagem);

  recebidas++;
  ultimaRx = mensagem.substring(0, 22);
  eventoMQTT = "Mensagem recebida";
  telaPendente = true;

  if (String(topic) == mqtt_topic_led) {
    if (mensagem == "1") {
      controlarLED(true, "MQTT");
    } else if (mensagem == "0" ||
               mensagem == "2") {
      controlarLED(false, "MQTT");
    }
  }
}

// =====================================================
// CONEXAO / RECONEXAO MQTT
// =====================================================

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    mqttSimuladoConectado = false;
    return;
  }

  if (SIMULATION_MODE) {
    if (!mqttSimuladoConectado) {
      mqttSimuladoConectado = true;

      Serial.println("Conexao MQTT simulada");
      publicar(mqtt_topic_status, "ESP32 online");
    }

    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  unsigned long agora = millis();

  if (agora - lastReconnectAttempt <
      MQTT_RETRY_INTERVAL) {
    return;
  }

  lastReconnectAttempt = agora;

  eventoMQTT = "Conectando MQTT...";
  telaPendente = true;
  atualizarTela();

  Serial.print("Conectando ao broker: ");
  Serial.println(mqtt_server);

  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("MQTT conectado!");

    bool inscricaoEnviada =
      mqttClient.subscribe(mqtt_topic_led);

    publicar(mqtt_topic_status, "ESP32 online");

    if (inscricaoEnviada) {
      eventoMQTT = "Inscricao enviada";

      Serial.print("Inscricao enviada: ");
      Serial.println(mqtt_topic_led);
    } else {
      eventoMQTT = "Falha na inscricao";
      Serial.println("Falha ao enviar inscricao");
    }
  } else {
    eventoMQTT =
      "MQTT rc=" + String(mqttClient.state());

    Serial.print("Falha MQTT, rc=");
    Serial.println(mqttClient.state());
  }

  telaPendente = true;
}

// =====================================================
// MONITORAMENTO WI-FI
// =====================================================

void verificarWiFi() {
  bool conectado =
    WiFi.status() == WL_CONNECTED;

  if (conectado != wifiEstavaConectado) {
    wifiEstavaConectado = conectado;
    telaPendente = true;

    if (conectado) {
      Serial.println();
      Serial.println("WiFi conectado!");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi desconectado");

      mqttSimuladoConectado = false;

      // Descarta a conexao TCP antiga.
      // A inscricao sera refeita na reconexao MQTT.
      espClient.stop();
    }
  }

  if (!conectado) {
    unsigned long agora = millis();

    if (agora - lastWifiAttempt >=
        WIFI_RETRY_INTERVAL) {
      lastWifiAttempt = agora;

      Serial.println("Tentando reconectar WiFi...");
      WiFi.reconnect();
    }
  }
}

// =====================================================
// PAGINA WEB
// =====================================================

void enviarPagina(WiFiClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println(
    "Content-Type: text/html; charset=utf-8"
  );
  client.println("Connection: close");
  client.println();

  client.println(
    "<!DOCTYPE html>"
    "<html lang='pt-br'>"
    "<head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' "
    "content='width=device-width,initial-scale=1'>"
    "<title>ESP32 MQTT</title>"
    "<style>"
    "body{font-family:Arial;text-align:center;"
    "margin:30px;background:#eef2f6;color:#182536}"
    ".painel{max-width:520px;margin:auto;"
    "background:white;padding:24px;"
    "border-radius:14px}"
    "button{padding:16px;font-size:18px;"
    "border:0;border-radius:8px;color:white;"
    "margin:8px;cursor:pointer}"
    ".on{background:#198754}"
    ".off{background:#dc3545}"
    "</style>"
    "</head>"
    "<body><div class='painel'>"
    "<h1>ESP32 MQTT Demo</h1>"
  );

  client.print("<h2>LED: ");
  client.print(
    digitalRead(LedPin) ? "LIGADO" : "DESLIGADO"
  );
  client.println("</h2>");

  client.print("<p>Origem do comando: ");
  client.print(origemLED);
  client.println("</p>");

  client.println(
    "<a href='/LEDControl/1'>"
    "<button class='on'>LIGAR LED</button></a>"
    "<a href='/LEDControl/0'>"
    "<button class='off'>DESLIGAR LED</button></a>"
  );

  client.print("<p>Modo: ");
  client.print(
    SIMULATION_MODE ? "MQTT simulado" : "MQTT real"
  );
  client.println("</p>");

  client.print("<p>Broker: ");
  client.print(mqtt_server);
  client.print(":");
  client.print(mqtt_port);
  client.println("</p>");

  client.print("<p>MQTT: ");
  client.print(
    mqttAtivo() ? "conectado" : "desconectado"
  );
  client.println("</p>");

  client.print("<p>Topico de controle: <b>");
  client.print(mqtt_topic_led);
  client.println("</b></p>");

  client.print("<p>Topico de status: <b>");
  client.print(mqtt_topic_status);
  client.println("</b></p>");

  client.println(
    "<p>Envie <b>1</b> para ligar;"
    " <b>0</b> ou <b>2</b> para desligar.</p>"
  );

  client.print("<p>Mensagens RX: ");
  client.print(recebidas);
  client.print(" | TX: ");
  client.print(publicadas);
  client.println("</p>");

  client.println(
    "<p><a href='/'>Atualizar pagina</a></p>"
    "</div></body></html>"
  );
}

// =====================================================
// ATENDIMENTO WEB
// =====================================================

void atenderWeb() {
  WiFiClient client = server.available();

  if (!client) {
    return;
  }

  String requisicao;
  requisicao.reserve(256);

  String cabecalho;
  cabecalho.reserve(256);

  bool primeiraLinha = true;
  bool requisicaoCompleta = false;

  unsigned long inicio = millis();

  // Aguarda os cabecalhos HTTP por ate 1 segundo.
  while (client.connected() &&
         millis() - inicio < 1000) {
    if (!SIMULATION_MODE) {
      mqttClient.loop();
    }

    if (!client.available()) {
      delay(1);
      continue;
    }

    char c = client.read();

    if (c == '\n') {
      if (primeiraLinha) {
        requisicao = cabecalho;
        primeiraLinha = false;
      } else if (cabecalho.length() == 0) {
        requisicaoCompleta = true;
        break;
      }

      cabecalho = "";
    } else if (c != '\r') {
      if (cabecalho.length() >= 256) {
        client.stop();
        return;
      }

      cabecalho += c;
    }
  }

  if (!requisicaoCompleta) {
    client.stop();
    return;
  }

  if (requisicao.startsWith(
        "GET /LEDControl/1 ")) {
    controlarLED(true, "Web");
    publicar(mqtt_topic_led, "1");

  } else if (requisicao.startsWith(
               "GET /LEDControl/0 ")) {
    controlarLED(false, "Web");
    publicar(mqtt_topic_led, "0");
  }

  enviarPagina(client);
  client.stop();

  telaPendente = true;
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(LedPin, OUTPUT);
  digitalWrite(LedPin, LOW);

  // Inicializacao do display
  SPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI);

  tela.initR(INITR_BLACKTAB);
  tela.setRotation(1);
  tela.setTextWrap(false);
  tela.setTextSize(1);
  tela.fillScreen(ST77XX_BLACK);

  atualizarTela();

  // Inicializacao do Wi-Fi
  WiFi.mode(WIFI_STA);

  // A reconexao sera controlada pelo nosso codigo.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  delay(100);

  // Diagnostico: verifica se o ESP32 encontra o hotspot.
  Serial.println("\nBuscando redes Wi-Fi...");

  int quantidade = WiFi.scanNetworks();
  bool encontrou = false;

  for (int i = 0; i < quantidade; i++) {
    Serial.print("Rede: ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" | Sinal: ");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" dBm | Canal: ");
    Serial.println(WiFi.channel(i));

    if (WiFi.SSID(i) == String(ssid)) {
      encontrou = true;
    }
  }

  Serial.println(
    encontrou
      ? "Rede configurada ENCONTRADA."
      : "Rede configurada NAO encontrada."
  );

  WiFi.scanDelete();

  Serial.print("Conectando a: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  lastWifiAttempt = millis();

  Serial.print("Conectando ao WiFi: ");
  Serial.println(ssid);

  // Identificador diferente para cada placa
  clientId = "ESP32-";

  String mac = WiFi.macAddress();
  mac.replace(":", "");
  clientId += mac;

  Serial.print("ID MQTT: ");
  Serial.println(clientId);

  // Inicializacao do MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);
  mqttClient.setSocketTimeout(2);

  // Inicializacao do servidor web
  server.begin();

  Serial.println(
    SIMULATION_MODE
      ? "Rodando com MQTT SIMULADO"
      : "Rodando com MQTT REAL"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  verificarWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    reconnectMQTT();

    if (!SIMULATION_MODE) {
      mqttClient.loop();
    }
  }

  unsigned long agora = millis();

  // Publica status a cada 30 segundos
  if (agora - lastMsgTime >= STATUS_INTERVAL) {
    lastMsgTime = agora;

    const char* mensagem =
      digitalRead(LedPin)
        ? "LED esta ligado"
        : "LED esta desligado";

    publicar(mqtt_topic_status, mensagem);
  }

  atenderWeb();
  atualizarTela();
}
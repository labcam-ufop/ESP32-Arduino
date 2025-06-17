
// ESP32 - Implementação de timer usando millis()
// É necessário selecionar a placa "DOIT ESP32 DEVKIT V1"

// Constantes para os pinos
const int ledPin = 14;   // LED principal
const int ledPin2 = 16;  // LED secundário

// Variáveis para controle dos "timers" virtuais
unsigned long timer1LastTrigger = 0;
const unsigned long timer1Interval = 1000;  // 1 segundo
int timer1Counter = 0;

unsigned long timer2LastTrigger = 0;
const unsigned long timer2Interval = 500;   // 0.5 segundo
int timer2Counter = 0;

// Estados dos LEDs
bool ledState = false;
bool led2State = false;

void setup() {
  // Inicializa pinos dos LEDs
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(ledPin2, LOW);
  
  // Inicializa comunicação serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP32 Timers Baseados em millis()");
  
  // Inicializa os tempos de referência
  timer1LastTrigger = millis();
  timer2LastTrigger = millis();
  
  Serial.println("Sistema iniciado - Usando timers por software");
}

// Função para verificar se um timer "disparou"
bool checkTimer(unsigned long &lastTrigger, unsigned long interval) {
  unsigned long currentTime = millis();
  
  // Verifica se passou o intervalo desejado
  if (currentTime - lastTrigger >= interval) {
    lastTrigger = currentTime;  // Atualiza o tempo da última verificação
    return true;                // Timer "disparou"
  }
  
  return false;  // Timer não disparou ainda
}

void loop() {
  // Verifica o "Timer 1" (1 segundo)
  if (checkTimer(timer1LastTrigger, timer1Interval)) {
    // Ações que seriam executadas pela interrupção do Timer 1
    timer1Counter++;
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    
    // Informação sobre o "disparo" do timer
    Serial.print("Timer 1 disparou! Contagem: ");
    Serial.println(timer1Counter);
    
    // Podemos consultar o tempo exato em que o timer disparou
    Serial.print("Momento do disparo: ");
    Serial.print(timer1LastTrigger);
    Serial.println(" ms desde o início");
  }
  
  // Verifica o "Timer 2" (0.5 segundo)
  if (checkTimer(timer2LastTrigger, timer2Interval)) {
    // Ações que seriam executadas pela interrupção do Timer 2
    timer2Counter++;
    led2State = !led2State;
    digitalWrite(ledPin2, led2State);
    
    // Mostra informação apenas a cada 10 disparos para não sobrecarregar o serial
    if (timer2Counter % 10 == 0) {
      Serial.print("Timer 2 disparou 10x! Contagem total: ");
      Serial.println(timer2Counter);
    }
  }
  
  // Podemos ler o valor "atual" do timer a qualquer momento
  // calculando quanto tempo se passou desde o último disparo
  unsigned long timer1CurrentValue = millis() - timer1LastTrigger;
  unsigned long timer2CurrentValue = millis() - timer2LastTrigger;
  
  // Mostrar os valores dos timers a cada segundo (usando um terceiro "timer")
  static unsigned long statusLastUpdate = 0;
  if (millis() - statusLastUpdate >= 3000) {  // A cada 3 segundos
    statusLastUpdate = millis();
    
    Serial.println("\n--- Status dos Timers ---");
    Serial.print("Timer 1: ");
    Serial.print(timer1CurrentValue);
    Serial.print(" / ");
    Serial.print(timer1Interval);
    Serial.print(" ms (");
    Serial.print((timer1CurrentValue * 100) / timer1Interval);
    Serial.println("%)");
    
    Serial.print("Timer 2: ");
    Serial.print(timer2CurrentValue);
    Serial.print(" / ");
    Serial.print(timer2Interval);
    Serial.print(" ms (");
    Serial.print((timer2CurrentValue * 100) / timer2Interval);
    Serial.println("%)");
    Serial.println("------------------------\n");
  }
}

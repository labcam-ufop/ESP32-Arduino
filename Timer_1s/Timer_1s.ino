
// ESP32 Timer para Versão 3.2.0
// É necessário selecionar a placa "DOIT ESP32 DEVKIT V1"

// Constantes para os pinos
const int ledPin = 14;  // LED principal

// Variáveis globais
hw_timer_t * timer = NULL;
volatile bool ledState = false;
volatile int interruptCounter = 0;

// Função de interrupção do timer
void IRAM_ATTR onTimer() {
  interruptCounter++;
  ledState = !ledState;
  digitalWrite(ledPin, ledState);
}

void setup() {
  // Inicializa pino do LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  
  // Inicializa comunicação serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nESP32 Timer para Versão 3.2.0");

  // Versão 3.2.0: timerBegin aceita apenas frequência
  // Frequência 1 = timer dispara a cada 1 segundo
  timer = timerBegin(1);  // 1Hz = timer de 1 segundo
  
  // Anexa a função de interrupção (sem o terceiro parâmetro na versão 3.2.0)
  timerAttachInterrupt(timer, &onTimer);
  
  // Versão 3.2.0: use timerAlarm em vez de timerAlarmWrite + timerAlarmEnable
  // Parâmetros: timer, valor do alarme, auto-reload, valor de recarga
  timerAlarm(timer, 1, true, 0);  // Alarme após 1 contagem, com auto-reload
  
  // Inicia o timer (necessário na versão 3.2.0)
  timerStart(timer);
  
  Serial.println("Timer configurado e iniciado");
}

void loop() {
  // Exibe contador de interrupções a cada 3 segundos
  static unsigned long lastMillis = 0;
  static int lastInterruptCount = 0;
  
  if (millis() - lastMillis > 3000) {
    lastMillis = millis();
    
    // Mostra informações sobre o contador
    Serial.print("Contador de interrupções: ");
    Serial.print(interruptCounter);
    Serial.print(" (Novas: ");
    Serial.print(interruptCounter - lastInterruptCount);
    Serial.println(")");
    
    lastInterruptCount = interruptCounter;
  }
}

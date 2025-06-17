
// É necessário selecionar a placa "DOIT ESP32 DEVKIT V1" para simular o projeto no PICSimLab.
// No PICSimLab, selecione a placa Esp32 DevkitC.

// constants 
const int buttonPin = 13;  // o número do pino do botão
const int ledPin = 14;     // o número do pino do LED

// variáveis para controle do estado
volatile bool ledState = false;  // estado do LED (volátil porque é alterado em ISR)

// Função de callback para a interrupção
void IRAM_ATTR buttonISR() {
  // Inverter o estado do LED quando o botão for pressionado
  ledState = !ledState;
  digitalWrite(ledPin, ledState);
}

void setup() {
  // inicializa o pino do LED como saída:
  pinMode(ledPin, OUTPUT);
  
  // inicializa o pino do botão como entrada com pull-down:
  pinMode(buttonPin, INPUT);
  
  // Configura a interrupção externa no pino do botão
  // RISING detecta quando o botão é pressionado (transição de LOW para HIGH)
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, RISING);
}

void loop() {
  // Com interrupções, não precisamos verificar o estado do botão no loop principal
  // O loop principal pode ficar vazio ou executar outras tarefas
  
  // Aqui você pode adicionar outras funcionalidades se necessário
}

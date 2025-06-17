
// É necessário selecionar a placa "DOIT ESP32 DEVKIT V1" para simular o projeto no PICSimLab.
// No PICSimLab, selecione a placa Esp32 DevkitC.

// constants 
const int buttonPin = 13;  // o número do pino do botão
const int ledPin = 14;     // o número do pino do LED

/*
No ESP32, todos os pinos GPIO podem ser configurados como pinos de interrupção externa. Esta é uma das grandes vantagens desta placa em comparação com outros microcontroladores que têm um número limitado de pinos de interrupção.

De acordo com a documentação, o ESP32 oferece:
- Interrupções em todos os pinos GPIO
- Até 32 slots de interrupção para cada núcleo do processador
- Cinco modos diferentes de interrupção (LOW, HIGH, CHANGE, FALLING e RISING)

Ao usar interrupções externas no ESP32, é importante lembrar que:

1. As funções de manipulação de interrupções (ISR) devem ter o atributo `IRAM_ATTR` para garantir que sejam armazenadas na RAM interna, para execução mais rápida.

2. Alguns pinos têm comportamentos especiais durante a inicialização (boot):
   - GPIO 0, 2, 4, 5, 12 e 15 são pinos de strapping, usados durante o boot
   - GPIO 1, 3 e 5 ficam em estado HIGH durante o boot ou podem gerar sinais PWM

3. É recomendável evitar os pinos conectados ao flash SPI interno (geralmente GPIO 6-11) para interrupções.

### Pinos Recomendados (Melhores Opções)
- **GPIO 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39**: São seguros para uso como interrupções e não têm comportamentos inesperados no boot

### Pinos a Evitar ou Usar com Cautela
- **GPIO 0**: Deve estar LOW para entrar no modo de boot, usado como pino de strapping
- **GPIO 1 e 3**: Usados como TX e RX para comunicação serial de debug
- **GPIO 2**: Conectado ao LED onboard, usado como pino de strapping
- **GPIO 5**: Gera sinal PWM durante o boot, usado como pino de strapping
- **GPIO 6-11**: Geralmente conectados ao flash SPI interno, não disponíveis para uso geral
- **GPIO 12**: Deve estar LOW durante o boot, usado como pino de strapping
*/

// variáveis para controle do estado
volatile bool ledState = false;  // estado do LED (volátil porque é alterado em ISR)
volatile unsigned long lastDebounceTime = 0;  // último tempo para debounce
const unsigned long debounceDelay = 300;     // tempo de debounce em ms

// Função de callback para a interrupção
void IRAM_ATTR buttonISR() {
  // Implementa debounce por software
  unsigned long currentTime = millis();
  if ((currentTime - lastDebounceTime) > debounceDelay) {
    ledState = !ledState;
    digitalWrite(ledPin, ledState);
    lastDebounceTime = currentTime;
  }
}

void setup() {
  // inicializa o pino do LED como saída:
  pinMode(ledPin, OUTPUT);

  // Garante que o LED começa apagado
  digitalWrite(ledPin, LOW);
  
  // inicializa o pino do botão como entrada com pull-down:
  pinMode(buttonPin, INPUT);
  
  // Configura a interrupção externa no pino do botão  
  /*
  LOW: A interrupção é disparada continuamente enquanto o pino estiver em nível baixo (0V).
  HIGH: A interrupção é disparada continuamente enquanto o pino estiver em nível alto (3.3V).
  CHANGE: A interrupção é disparada sempre que houver uma mudança no estado do pino (tanto de LOW para HIGH quanto de HIGH para LOW).
  RISING: A interrupção é disparada na borda de subida (quando o pino muda de LOW para HIGH - botão solto).
  FALLING: A interrupção é disparada na borda de descida (quando o pino muda de HIGH para LOW - botão pressinado).
  */
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, FALLING); 

  // Inicializa comunicação serial para depuração
  Serial.begin(115200);
  Serial.println("Inicialização completa - Configuração com pull-up");
}

void loop() {
  // Com interrupções, não precisamos verificar o estado do botão no loop principal
  // O loop principal pode ficar vazio ou executar outras tarefas
  // Ou adicione um código de depuração para verificar o estado
  delay(5000);  // A cada 5 segundos
  Serial.print("Estado do LED: ");
  Serial.println(ledState ? "ACESO" : "APAGADO");
}



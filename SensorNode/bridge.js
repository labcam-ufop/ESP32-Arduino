
const express = require('express');
const mqtt = require('mqtt');
const axios = require('axios');
const app = express();

app.use(express.json());

// Configurações do bridge
const ESP_URL = 'http://localhost:16555';
const MQTT_BROKER = 'mqtt://test.mosquitto.org:1883';
const LED_TOPIC = 'CAT341/LEDControl';
const STATUS_TOPIC = 'CAT341/status';
const ADC_TOPIC = 'CAT341/ADC';  // Tópico para dados ADC
const PORT = 8080;

// Variável para armazenar o último valor ADC
let lastAdcValue = null;

// Intervalo para buscar o valor ADC (em ms)
const ADC_POLLING_INTERVAL = 5000; // 5 segundos

// Conecta ao broker MQTT
const mqttClient = mqtt.connect(MQTT_BROKER);

// Quando conectar ao MQTT
mqttClient.on('connect', () => {
  console.log(`Conectado ao broker MQTT ${MQTT_BROKER}`);
  
  // Inscreve em todos os tópicos do prefixo CAT341 usando curinga
  mqttClient.subscribe('CAT341/#', (err) => {
    if (!err) {
      console.log(`Inscrito em todos os tópicos CAT341/#`);
      // Publica mensagem informando que está online
      mqttClient.publish(STATUS_TOPIC, 'Bridge online, ESP32 disponível');
    } else {
      console.error(`Erro ao inscrever nos tópicos: ${err}`);
    }
  });
});

// Quando receber mensagem do MQTT
mqttClient.on('message', (topic, message) => {
  const msg = message.toString();
  console.log(`Mensagem recebida do MQTT no tópico ${topic}: ${msg}`);

  // Trata mensagens conforme o tópico
  if (topic === LED_TOPIC) {
    // Processa comandos do LED
    if (msg === '1') {
      // Liga o LED
      axios.get(`${ESP_URL}/LEDControl/1`)
        .then(response => {
          console.log('Comando para ligar o LED enviado com sucesso');
          mqttClient.publish(STATUS_TOPIC, 'LED ligado via MQTT');
        })
        .catch(error => {
          console.error('Erro ao enviar comando para ligar o LED:', error.message);
          mqttClient.publish(STATUS_TOPIC, 'Erro ao ligar o LED: ' + error.message);
        });
    } else if (msg === '2' || msg === '0') {
      // Desliga o LED
      axios.get(`${ESP_URL}/LEDControl/0`)
        .then(response => {
          console.log('Comando para desligar o LED enviado com sucesso');
          mqttClient.publish(STATUS_TOPIC, 'LED desligado via MQTT');
        })
        .catch(error => {
          console.error('Erro ao enviar comando para desligar o LED:', error.message);
          mqttClient.publish(STATUS_TOPIC, 'Erro ao desligar o LED: ' + error.message);
        });
    }
  } else if (topic === ADC_TOPIC) {
    // Processa dados do ADC
    console.log(`Dados ADC recebidos via MQTT: ${msg}`);
    lastAdcValue = msg;
  }
});

// Função para buscar o valor ADC do ESP32
async function fetchAdcValue() {
  try {
    // Busca dados ADC do endpoint JSON do ESP32
    const response = await axios.get(`${ESP_URL}/adc-json`);
    
    if (response.data && response.data.adc_value) {
      const adcValue = response.data.adc_value;
      console.log(`Valor ADC obtido do ESP32: ${adcValue}`);
      
      // Atualiza o valor ADC local
      lastAdcValue = adcValue;
      
      // Publica o valor no tópico MQTT
      mqttClient.publish(ADC_TOPIC, adcValue);
      console.log(`Publicado no tópico ${ADC_TOPIC}: ${adcValue}`);
      
      return adcValue;
    } else {
      console.log('Nenhum valor ADC disponível no ESP32');
      return null;
    }
  } catch (error) {
    console.error('Erro ao buscar valor ADC do ESP32:', error.message);
    return null;
  }
}

// Configura um intervalo para buscar o valor ADC periodicamente
setInterval(fetchAdcValue, ADC_POLLING_INTERVAL);

// API REST para interagir com o bridge
app.post('/publish', (req, res) => {
  const { topic, message } = req.body;
  
  if (!topic || !message) {
    return res.status(400).json({ error: 'Tópico e mensagem são obrigatórios' });
  }
  
  mqttClient.publish(topic, message);
  console.log(`Publicado no tópico ${topic}: ${message}`);
  res.json({ status: 'success', message: 'Mensagem publicada' });
});

// Endpoint específico para publicar dados ADC
app.post('/adc', (req, res) => {
  const { value } = req.body;
  
  if (!value) {
    return res.status(400).json({ error: 'Valor ADC é obrigatório' });
  }
  
  mqttClient.publish(ADC_TOPIC, value);
  lastAdcValue = value;
  console.log(`Publicado no tópico ${ADC_TOPIC}: ${value}`);
  res.json({ status: 'success', message: 'Dados ADC publicados' });
});

// Novo endpoint para buscar o valor ADC sob demanda
app.get('/fetch-adc', async (req, res) => {
  try {
    const adcValue = await fetchAdcValue();
    if (adcValue) {
      res.json({ status: 'success', adc_value: adcValue });
    } else {
      res.json({ status: 'warning', message: 'Não foi possível obter o valor ADC' });
    }
  } catch (error) {
    res.status(500).json({ status: 'error', message: error.message });
  }
});

// Endpoint para obter o último valor ADC conhecido
app.get('/adc', (req, res) => {
  if (lastAdcValue) {
    res.json({ status: 'success', adc_value: lastAdcValue });
  } else {
    res.json({ status: 'warning', message: 'Nenhum valor ADC disponível ainda' });
  }
});

// Endpoint para verificar o status do bridge
app.get('/status', (req, res) => {
  res.json({
    status: 'online',
    mqtt_connected: mqttClient.connected,
    esp_url: ESP_URL,
    mqtt_broker: MQTT_BROKER,
    subscribed_topics: ['CAT341/#'],
    status_topic: STATUS_TOPIC,
    adc_topic: ADC_TOPIC,
    last_adc_value: lastAdcValue,
    adc_polling_interval: ADC_POLLING_INTERVAL + 'ms'
  });
});

// Inicia o servidor
app.listen(PORT, () => {
  console.log(`Bridge invertido rodando na porta ${PORT}`);
  console.log(`Monitorando ESP32 em ${ESP_URL}`);
  console.log(`Conectado ao MQTT em ${MQTT_BROKER}`);
  console.log(`Inscrito em todos os tópicos CAT341/#`);
  console.log(`Tópico de controle LED: ${LED_TOPIC}`);
  console.log(`Tópico de dados ADC: ${ADC_TOPIC}`);
  console.log(`Tópico de status: ${STATUS_TOPIC}`);
  console.log(`Buscando dados ADC do ESP32 a cada ${ADC_POLLING_INTERVAL}ms`);
  console.log(`Valores aceitos para LED: "1" (ligar) e "2" ou "0" (desligar)`);
  
  // Busca valor ADC imediatamente na inicialização
  fetchAdcValue();
});

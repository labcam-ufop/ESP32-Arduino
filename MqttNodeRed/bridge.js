
const express = require('express');
const mqtt = require('mqtt');
const axios = require('axios');

const app = express();
app.use(express.json());

// Configurações do bridge
const ESP_URL = 'http://localhost:16555';
const MQTT_BROKER = 'mqtt://test.mosquitto.org:1883';
const MQTT_TOPIC = 'CAT341/LEDControl';
const STATUS_TOPIC = 'CAT341/status';
const PORT = 8080;

// Conecta ao broker MQTT
const mqttClient = mqtt.connect(MQTT_BROKER);

// Quando conectar ao MQTT
mqttClient.on('connect', () => {
  console.log(`Conectado ao broker MQTT ${MQTT_BROKER}`);

  // Inscreve no tópico para controlar o LED
  mqttClient.subscribe(MQTT_TOPIC, (err) => {
    if (!err) {
      console.log(`Inscrito no tópico ${MQTT_TOPIC}`);

      // Publica mensagem informando que está online
      mqttClient.publish(STATUS_TOPIC, 'Bridge online, ESP32 disponível');
    } else {
      console.error(`Erro ao inscrever no tópico: ${err}`);
    }
  });
});

// Quando receber mensagem do MQTT
mqttClient.on('message', (topic, message) => {
  const msg = message.toString();
  console.log(`Mensagem recebida do MQTT no tópico ${topic}: ${msg}`);

  // Formata o comando no estilo MQTT para o ESP32
  const topicParts = topic.split('/');
  const command = topicParts[topicParts.length - 1];
  
  // Encaminha para o ESP32 no formato MQTT-like
  if (msg === '1') {
    // Liga o LED usando formato MQTT-like
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
    // Desliga o LED usando formato MQTT-like
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
});

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

// Endpoint para verificar o status do bridge
app.get('/status', (req, res) => {
  res.json({
    status: 'online',
    mqtt_connected: mqttClient.connected,
    esp_url: ESP_URL,
    mqtt_broker: MQTT_BROKER,
    subscribed_topics: [MQTT_TOPIC],
    status_topic: STATUS_TOPIC
  });
});

// Inicia o servidor
app.listen(PORT, () => {
  console.log(`Bridge invertido rodando na porta ${PORT}`);
  console.log(`Monitorando ESP32 em ${ESP_URL}`);
  console.log(`Conectado ao MQTT em ${MQTT_BROKER}`);
  console.log(`Tópico de controle: ${MQTT_TOPIC}`);
  console.log(`Tópico de status: ${STATUS_TOPIC}`);
  console.log(`Valores aceitos: "1" (ligar) e "2" ou "0" (desligar)`);
});

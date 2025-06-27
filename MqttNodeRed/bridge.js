
const express = require('express');
const mqtt = require('mqtt');
const axios = require('axios');
const bodyParser = require('body-parser');
const fs = require('fs');
const path = require('path');

const app = express();
app.use(bodyParser.json());
app.use(bodyParser.urlencoded({ extended: true }));

// Configurações
const ESP_URL = 'http://localhost:16555';
const MQTT_BROKER = 'mqtt://test.mosquitto.org:1883';
const PORT = 8080;
const CONFIG_FILE = path.join(__dirname, 'mqtt_config.json');

// Configuração padrão
let config = {
  topics: {
    // Configuração padrão para o LED
    "CAT341/LEDControl": {
      type: "input",
      actions: {
        "1": { endpoint: "/H", description: "Ligar LED" },
        "2": { endpoint: "/L", description: "Desligar LED" },
        "0": { endpoint: "/L", description: "Desligar LED" }
      }
    },
    "CAT341/status": {
      type: "output",
      description: "Tópico para mensagens de status"
    }
  },
  custom_handlers: {}
};

// Carrega configuração existente se disponível
if (fs.existsSync(CONFIG_FILE)) {
  try {
    const savedConfig = JSON.parse(fs.readFileSync(CONFIG_FILE, 'utf8'));
    config = { ...config, ...savedConfig };
    console.log("Configuração carregada do arquivo");
  } catch (err) {
    console.error("Erro ao carregar configuração:", err.message);
  }
}

// Conecta ao broker MQTT
const mqttClient = mqtt.connect(MQTT_BROKER);

// Quando conectar ao MQTT
mqttClient.on('connect', () => {
  console.log("Conectado ao broker MQTT " + MQTT_BROKER);
  
  // Inscreve em todos os tópicos de entrada
  Object.keys(config.topics).forEach(topic => {
    if (config.topics[topic].type === "input") {
      mqttClient.subscribe(topic, (err) => {
        if (!err) {
          console.log("Inscrito no tópico " + topic);
        } else {
          console.error("Erro ao inscrever no tópico " + topic + ": " + err);
        }
      });
    }
  });
  
  // Publica mensagem informando que está online
  const statusTopics = Object.keys(config.topics).filter(topic => 
    config.topics[topic].type === "output" && 
    config.topics[topic].description.includes("status")
  );
  
  if (statusTopics.length > 0) {
    mqttClient.publish(statusTopics[0], 'Bridge online, ESP32 disponível');
  }
});

// Quando receber mensagem do MQTT
mqttClient.on('message', (topic, message) => {
  const msg = message.toString();
  console.log("Mensagem recebida do MQTT no tópico " + topic + ": " + msg);
  
  // Verifica se temos uma configuração para este tópico
  if (config.topics[topic] && config.topics[topic].type === "input") {
    const topicConfig = config.topics[topic];
    
    // Verifica se temos uma ação para esta mensagem
    if (topicConfig.actions && topicConfig.actions[msg]) {
      const action = topicConfig.actions[msg];
      
      // Executa a ação
      axios.get(ESP_URL + action.endpoint)
        .then(() => {
          console.log(`Comando "${action.description}" enviado com sucesso`);
          
          // Publica status se houver um tópico de status
          const statusTopics = Object.keys(config.topics).filter(t => 
            config.topics[t].type === "output" && 
            config.topics[t].description.includes("status")
          );
          
          if (statusTopics.length > 0) {
            mqttClient.publish(statusTopics[0], `${action.description} via MQTT`);
          }
        })
        .catch(err => {
          console.error(`Erro ao enviar comando "${action.description}": ${err.message}`);
        });
    } 
    // Verifica se há um handler personalizado para este tópico
    else if (config.custom_handlers[topic]) {
      // Aqui implementaríamos lógica personalizada se necessário
      console.log(`Handler personalizado para tópico ${topic} não implementado`);
    }
    else {
      console.log(`Nenhuma ação definida para mensagem "${msg}" no tópico ${topic}`);
    }
  }
});

// API para gerenciar tópicos e ações MQTT

// Adicionar ou atualizar um tópico MQTT
app.post('/api/topics', (req, res) => {
  const { topic, type, description } = req.body;
  
  if (!topic || !type) {
    return res.status(400).json({ error: 'Tópico e tipo são obrigatórios' });
  }
  
  // Adiciona o tópico à configuração
  config.topics[topic] = {
    type,
    description: description || '',
    actions: config.topics[topic]?.actions || {}
  };
  
  // Se for um tópico de entrada, inscreve nele
  if (type === 'input' && mqttClient.connected) {
    mqttClient.subscribe(topic);
  }
  
  // Salva a configuração
  fs.writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2));
  
  res.json({ success: true, message: `Tópico ${topic} configurado com sucesso` });
});

// Adicionar ou atualizar uma ação para um tópico
app.post('/api/topics/:topic/actions', (req, res) => {
  const { topic } = req.params;
  const { message, endpoint, description } = req.body;
  
  if (!message || !endpoint) {
    return res.status(400).json({ error: 'Mensagem e endpoint são obrigatórios' });
  }
  
  // Verifica se o tópico existe
  if (!config.topics[topic]) {
    return res.status(404).json({ error: 'Tópico não encontrado' });
  }
  
  // Adiciona a ação
  if (!config.topics[topic].actions) {
    config.topics[topic].actions = {};
  }
  
  config.topics[topic].actions[message] = {
    endpoint,
    description: description || `Ação para mensagem ${message}`
  };
  
  // Salva a configuração
  fs.writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2));
  
  res.json({ success: true, message: `Ação para mensagem ${message} configurada com sucesso` });
});

// Endpoint para publicar mensagem no MQTT
app.post('/api/publish', (req, res) => {
  const { topic, message } = req.body;
  
  if (!topic || !message) {
    return res.status(400).json({ error: 'Tópico e mensagem são obrigatórios' });
  }
  
  mqttClient.publish(topic, message);
  console.log(`Publicado no tópico ${topic}: ${message}`);
  
  res.json({ success: true, message: 'Mensagem publicada com sucesso' });
});

// Interface web para configuração
app.get('/', (req, res) => {
  res.send(`
    <html>
      <head>
        <title>Bridge MQTT Configuração</title>
        <style>
          body { font-family: Arial; margin: 20px; }
          .container { max-width: 800px; margin: 0 auto; }
          h1 { color: #333; }
          .topic { margin-bottom: 20px; padding: 10px; border: 1px solid #ddd; }
          .action { margin-left: 20px; margin-bottom: 10px; }
          form { margin-top: 20px; padding: 10px; border: 1px solid #ddd; }
          label { display: block; margin-bottom: 5px; }
          input, select { margin-bottom: 10px; padding: 5px; width: 100%; }
          button { padding: 8px 15px; background: #4CAF50; color: white; border: none; cursor: pointer; }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>Bridge MQTT Configuração</h1>
          
          <h2>Tópicos Configurados</h2>
          ${Object.keys(config.topics).map(topic => `
            <div class="topic">
              <h3>${topic}</h3>
              <p>Tipo: ${config.topics[topic].type}</p>
              <p>Descrição: ${config.topics[topic].description || 'Nenhuma'}</p>
              
              ${config.topics[topic].type === 'input' ? `
                <h4>Ações:</h4>
                ${config.topics[topic].actions ? 
                  Object.keys(config.topics[topic].actions).map(msg => `
                    <div class="action">
                      <p>Mensagem: "${msg}" => ${config.topics[topic].actions[msg].description} (${config.topics[topic].actions[msg].endpoint})</p>
                    </div>
                  `).join('') : 'Nenhuma ação configurada'
                }
              ` : ''}
            </div>
          `).join('')}
          
          <h2>Adicionar Novo Tópico</h2>
          <form action="/api/topics" method="post">
            <label for="topic">Tópico:</label>
            <input type="text" id="topic" name="topic" required>
            
            <label for="type">Tipo:</label>
            <select id="type" name="type" required>
              <option value="input">Entrada (Receber Comandos)</option>
              <option value="output">Saída (Publicar Status)</option>
            </select>
            
            <label for="description">Descrição:</label>
            <input type="text" id="description" name="description">
            
            <button type="submit">Adicionar Tópico</button>
          </form>
          
          <h2>Adicionar Nova Ação</h2>
          <form action="/api/topics/:topic/actions" method="post" onsubmit="this.action = '/api/topics/' + document.getElementById('actionTopic').value + '/actions'">
            <label for="actionTopic">Tópico:</label>
            <select id="actionTopic" name="actionTopic" required>
              ${Object.keys(config.topics)
                .filter(topic => config.topics[topic].type === 'input')
                .map(topic => `<option value="${topic}">${topic}</option>`)
                .join('')
              }
            </select>
            
            <label for="message">Mensagem:</label>
            <input type="text" id="message" name="message" required>
            
            <label for="endpoint">Endpoint:</label>
            <input type="text" id="endpoint" name="endpoint" required placeholder="/sua-rota">
            
            <label for="actionDescription">Descrição:</label>
            <input type="text" id="actionDescription" name="description">
            
            <button type="submit">Adicionar Ação</button>
          </form>
          
          <h2>Publicar Mensagem MQTT</h2>
          <form action="/api/publish" method="post">
            <label for="pubTopic">Tópico:</label>
            <input type="text" id="pubTopic" name="topic" required>
            
            <label for="pubMessage">Mensagem:</label>
            <input type="text" id="pubMessage" name="message" required>
            
            <button type="submit">Publicar</button>
          </form>
        </div>
      </body>
    </html>
  `);
});

// Inicia o servidor
app.listen(PORT, () => {
  console.log("Bridge MQTT Inteligente rodando na porta " + PORT);
  console.log("Interface de configuração: http://localhost:" + PORT);
  console.log("Monitorando ESP32 em " + ESP_URL);
  console.log("Conectado ao MQTT em " + MQTT_BROKER);
});

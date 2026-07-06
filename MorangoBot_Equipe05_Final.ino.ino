/*
  =============================================================================
 PROJETO: MORANGOBOT - Sistema Embarcado de Diagnóstico Multiespectral
 CEEP Olegário Macedo - Castro - Paraná
 EQUIPE: EMANUELLA / KAUANE / LAURA / PAOLA / EMANUELA
 
 CONCURSO: Agrinho 2026 - Categoria AgroRobótica
 TEMA: Agro forte, futuro sustentável: equilíbrio entre produção e meio ambiente
 EMPRESA PARCEIRA: Luiz Antonio Lopes - Produtor Rural de Morangos (Pirai do Sul/PR)
 
 ORIENTADORA: Prof.ª MARLI BURDA 
 =============================================================================
 
 ARQUITETURA TÉCNICA:
 - Edge Computing: Processamento local no ESP32-S3 (sem dependência de nuvem)
 - Dual I2C Bus: Segregação física de sensores para evitar conflitos de barramento
 - Máquina de Estados Finita (FSM): Substitui delay() bloqueantes por millis()
 - Interface Web Offline: Servidor HTTP embutido com charset UTF-8 forçado
 
 ALGORITMOS AGRONÔMICOS IMPLEMENTADOS:
 - VPD (Déficit de Pressão de Vapor): Fórmula de Tetens para risco transpiratório
 - Ponto de Orvalho: Fórmula de Magnus para previsão de condensação/fungos
 - NDVI Proxy: Índice de vegetação por reflectância espectral (Green/Red)
 - Matriz de Decisão Cruzada: 7 regras de inferência solo+folha+ar
 
 SUSTENTABILIDADE:
 - LED do TCS acionado apenas durante leitura (economia energética)
 - Sensor capacitivo S12 sem corrosão eletrolítica (longevidade/zero descarte)
 - Diagnóstico preciso reduz aplicação desnecessária de agroquímicos
 =============================================================================
*/

// --- BIBLIOTECAS ---
#include <WiFi.h>          // Conexão Wi-Fi (modo Access Point)
#include <WebServer.h>     // Servidor HTTP embutido no ESP32
#include <Wire.h>          // Comunicação I2C (barramentos Wire e Wire1)
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>    // Sensor ambiental (Temp/UR/Pressão)
#include <Adafruit_TCS34725.h>  // Sensor de cor/clorofila (RGB+Clear)
#include <math.h>          // Funções matemáticas (exp, log, pow)

// --- CREDENCIAIS DA REDE WI-FI LOCAL (ACCESS POINT) ---
// O ESP32 cria sua própria rede; não depende de internet externa
const char* ssid = "EQUIPE05-Morango";
const char* password = "12345678";
WebServer server(80);      // Servidor HTTP na porta padrão 80

// --- INSTANCIAÇÃO DOS SENSORES ---
Adafruit_BME280 bme; 
// Configuração otimizada para folhas de morango:
// Integração 154ms + Ganho 4X = melhor relação sinal/ruído para superfícies verdes escuras
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_154MS, TCS34725_GAIN_4X);

// --- DEFINIÇÃO DE PINOS (Declaração centralizada via #define) ---
#define PINO_BOTAO    4    // Push button com pull-up interno
#define PINO_LED_TCS  10   // Controle independente do LED branco do TCS
#define PINO_UMIDADE  3    // ADC para sensor capacitivo S12

// --- PARÂMETROS DE CALIBRAÇÃO DO SOLO (Ajustáveis sem alterar lógica) ---
// Valores obtidos empiricamente em substrato comercial de morango
const int SOLO_SECO = 3800;     // Valor ADC com sensor no ar seco
const int SOLO_MOLHADO = 1500;  // Valor ADC com sensor saturado em água

// --- THRESHOLDS AGRONÔMICOS PARA MORANGO (Fragaria × ananassa) ---
// Calibrados com folhas em 3 estágios de sanidade na propriedade parceira
const float MORANGO_SAUDAVEL_MIN = 1.8;   // Índice G/R saudável
const float MORANGO_ESTRESSE_MIN = 1.3;   // Início de clorose/nutricional
const float MORANGO_CRITICO = 1.0;        // Clorose severa / possível ácaro

// --- VARIÁVEIS DE CONTROLE DE TEMPO (millis) ---
// Substituem delay() para manter o servidor web responsivo
unsigned long tempoAnteriorBME = 0;
const long intervaloBME = 10000;  // Leitura ambiental a cada 10s

// --- VARIÁVEIS DO BOTÃO (Debounce por software) ---
bool estadoAnteriorBotao = HIGH;

// --- MÁQUINA DE ESTADOS FINITA (FSM) PARA DIAGNÓSTICO ---
// Estados: 0=IDLE, 1=LED_LIGADO, 2=TCS_LIDO, 3=FINALIZADO
bool fazerDiagnostico = false;
int estadoDiagnostico = 0; 
unsigned long tempoDiagnostico = 0;

// --- VARIÁVEIS DE DADOS AMBIENTAIS (BME280) ---
float ultimaTemp = 0, ultimaHum = 0, ultimaPress = 0;
float vpd = 0, pontoOrvalho = 0;
String riscoFungo = "Normal";

// --- VARIÁVEIS DE DADOS ESPECTRAIS (TCS34725) ---
float clorofilaGR = 0, indiceNDVI = 0;
uint16_t ultimaLuz = 0;

// --- VARIÁVEIS DE DADOS EDÁFICOS (SENSOR CAPACITIVO) ---
int ultimaUmidadeSolo = 0;

// --- VARIÁVEIS DE DIAGNÓSTICO INTEGRADO ---
String diagnosticoFinal = "Aguardando...";
String classeDiagnostico = "alert-info";
String recomendacao = "";

// ===========================================================================
// FUNÇÃO: calcularVPD_e_PontoOrvalho()
// PROPÓSITO: Calcular indicadores agrometeorológicos em tempo real
// FÓRMULAS: Tetens (SVP/VPD) e Magnus (Ponto de Orvalho)
// RELEVÂNCIA: VPD é o parâmetro mais importante para manejo de estufas;
//             Ponto de Orvalho prevê condensação foliar (risco de Botrytis)
// ===========================================================================
void calcularVPD_e_PontoOrvalho() {
  // Pressão de Vapor Saturada (SVP) pela fórmula de Tetens (kPa)
  float svp = 0.6108 * exp((17.27 * ultimaTemp) / (ultimaTemp + 237.3));
  
  // VPD = SVP × (1 - UR/100) → Força motriz da transpiração vegetal
  vpd = svp * (1.0 - (ultimaHum / 100.0));
  
  // Ponto de Orvalho pela fórmula de Magnus-Tetens
  float gamma = log(ultimaHum / 100.0) + (17.27 * ultimaTemp) / (ultimaTemp + 237.3);
  pontoOrvalho = (237.3 * gamma) / (17.27 - gamma);
  
  // Classificação de risco fúngico baseada em VPD e delta T-Ta
  float delta = ultimaTemp - pontoOrvalho;
  if (vpd < 0.4 || delta < 2.0) {
    riscoFungo = "ALTO (Botrytis)";      // Condições de saturação
  } else if (vpd < 0.8 || delta < 3.0) {
    riscoFungo = "Moderado";              // Atenção preventiva
  } else {
    riscoFungo = "Baixo";                 // Faixa segura
  }
}

// ===========================================================================
// FUNÇÃO: gerarDiagnosticoIntegrado()
// PROPÓSITO: Cruzar dados de 3 domínios sensoriais (solo+folha+ar) para
//            gerar diagnóstico acionável específico para morango
// LÓGICA: Matriz de decisão com 7 regras priorizadas por severidade
// INOVAÇÃO: Substitui análise unica por uma multivariada.
// ===========================================================================
void gerarDiagnosticoIntegrado() {
  // Variáveis booleanas para legibilidade da matriz de decisão
  bool soloSeco = (ultimaUmidadeSolo < 40);
  bool soloEncharcado = (ultimaUmidadeSolo > 85);
  bool folhaEstressada = (clorofilaGR < MORANGO_ESTRESSE_MIN);
  bool folhaSaudavel = (clorofilaGR >= MORANGO_SAUDAVEL_MIN);
  bool vpdAlto = (vpd > 1.5);
  bool vpdBaixo = (vpd < 0.5);
  bool riscoFungos = (riscoFungo == "ALTO (Botrytis)");
  
  // REGRA 1: Condições ideais (todas variáveis dentro da faixa ótima)
  if (folhaSaudavel && !soloSeco && !riscoFungos && vpd >= 0.8 && vpd <= 1.2) {
    diagnosticoFinal = "PERFEITO! Planta em condições ideais.";
    classeDiagnostico = "alert-success";
    recomendacao = "Manter manejo atual. Colheita em boas condições.";
  } 
  // REGRA 2: Estresse hídrico severo (folha + solo seco + VPD alto)
  else if (folhaEstressada && soloSeco && vpdAlto) {
    diagnosticoFinal = "ESTRESSE HÍDRICO SEVERO";
    classeDiagnostico = "alert-danger";
    recomendacao = "Irrigar imediatamente. Acionar nebulização para baixar VPD. Fechar cortinas.";
  }
  // REGRA 3: Risco fitossanitário crítico (fungos + VPD baixo)
  else if (riscoFungos && vpdBaixo) {
    diagnosticoFinal = "ALERTA DE BOTRYTIS (Mofo Cinzento)";
    classeDiagnostico = "alert-danger";
    recomendacao = "Abrir cortinas para ventilação. Reduzir irrigação foliar. Aplicar fungicida preventivo.";
  }
  // REGRA 4: Deficiência nutricional (folha estressada + solo NÃO seco)
  else if (folhaEstressada && !soloSeco) {
    diagnosticoFinal = "DEFICIÊNCIA NUTRICIONAL";
    classeDiagnostico = "alert-warning";
    recomendacao = "Verificar Nitrogênio (amarelo uniforme) ou Magnésio (amarelo entre nervuras). Fazer fertirrigação.";
  }
  // REGRA 5: Excesso de água no solo (risco de Pythium/apodrecimento radicular)
  else if (soloEncharcado) {
    diagnosticoFinal = "SOLO ENCHARCADO (Risco de Pythium)";
    classeDiagnostico = "alert-danger";
    recomendacao = "Suspender irrigação. Verificar drenagem dos canteiros. Risco de apodrecimento radicular.";
  }
  // REGRA 6: Clorose severa ou infestação por ácaro-rajado
  else if (clorofilaGR < MORANGO_CRITICO) {
    diagnosticoFinal = "CLOROSE SEVERA / POSSÍVEL ÁCARO";
    classeDiagnostico = "alert-danger";
    recomendacao = "Inspecionar face inferior das folhas. Coloração bronzeada = Tetranychus urticae. Aplicar acaricida.";
  }
  // REGRA 7: VPD alto isolado (transpiração excessiva sem outros sintomas)
  else if (vpdAlto) {
    diagnosticoFinal = "VPD ALTO - Transpiração Excessiva";
    classeDiagnostico = "alert-warning";
    recomendacao = "Aumentar umidade do ar. Acionar nebulizadores. Sombrear a estufa nas horas quentes.";
  }
  // REGRA PADRÃO: Condições aceitáveis
  else {
    diagnosticoFinal = "Condições aceitáveis, monitore.";
    classeDiagnostico = "alert-info";
    recomendacao = "Continuar acompanhando os parâmetros.";
  }
}

// ===========================================================================
// INTERFACE WEB EMBUTIDA (HTML + CSS + JS)
// Codificação UTF-8 forçada via meta-tag e cabeçalho HTTP
// Design responsivo para celulares e tablets em campo
// ===========================================================================
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MORANGOBOT - Sistema Embarcado de Diagnóstico Multiespectral</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; background: #fff5f5; color: #333; margin: 0; padding: 20px; }
    .container { max-width: 900px; margin: 0 auto; }
    h1 { text-align: center; color: #c0392b; margin-bottom: 10px; }
    .subtitle { text-align: center; color: #7f8c8d; margin-bottom: 30px; font-style: italic; }
    .card { background: white; border-radius: 15px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.08); }
    .card-env { border-left: 5px solid #3498db; }
    .card-adv { border-left: 5px solid #9b59b6; }
    .card-plant { border-left: 5px solid #27ae60; }
    h2 { margin-top: 0; font-size: 18px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 15px; text-align: center; }
    .stat-value { font-size: 26px; font-weight: bold; color: #2c3e50; }
    .stat-label { font-size: 11px; color: #7f8c8d; margin-top: 3px; text-transform: uppercase; letter-spacing: 0.5px; }
    .vpd-ok { color: #27ae60; }
    .vpd-alert { color: #f39c12; }
    .vpd-danger { color: #c0392b; }
    button { width: 100%; padding: 16px; background: linear-gradient(135deg, #c0392b, #e74c3c); color: white; border: none; border-radius: 10px; font-size: 18px; font-weight: bold; cursor: pointer; box-shadow: 0 4px 15px rgba(231, 76, 60, 0.3); }
    button:active { transform: scale(0.98); }
    button:disabled { background: #bdc3c7; }
    .status { text-align: center; color: #e67e22; margin-top: 10px; display: none; font-weight: bold; }
    .alert { padding: 15px; border-radius: 8px; margin-top: 15px; font-weight: bold; display: none; }
    .alert-danger { background: #fadbd8; color: #c0392b; border: 1px solid #e6b0aa; }
    .alert-success { background: #d5f5e3; color: #27ae60; border: 1px solid #abebc6; }
    .alert-warning { background: #fcf3cf; color: #f39c12; border: 1px solid #f9e79f; }
    .alert-info { background: #d6eaf8; color: #2874a6; border: 1px solid #aed6f1; }
    .recomendacao { background: #fef9e7; padding: 15px; border-radius: 8px; margin-top: 10px; font-size: 14px; border-left: 4px solid #f39c12; }
    .recomendacao strong { color: #d35400; }
    .footer { text-align: center; font-size: 11px; color: #95a5a6; margin-top: 30px; }
  </style>
</head>
<body>
  <div class="container">
    <h1>O Médico de Plantas.</h1>
    <div class="subtitle"><n>Sistema de Diagnóstico Agronômico para Cultivo Protegido</n></div>
    
    <div class="card card-env">
      <h2>🌡️ Monitoramento do Ar (BME280)</h2>
      <div class="grid">
        <div><div class="stat-value" id="temp">--</div><div class="stat-label">Temp (°C)</div></div>
        <div><div class="stat-value" id="hum">--</div><div class="stat-label">Umid Ar (%)</div></div>
        <div><div class="stat-value" id="press">--</div><div class="stat-label">Pressão (hPa)</div></div>
        <div><div class="stat-value" id="alt">--</div><div class="stat-label">Altitude (m)</div></div>
      </div>
    </div>

    <div class="card card-adv">
      <h2>🔬 Indicadores Agronômicos Avançados</h2>
      <div class="grid">
        <div><div class="stat-value" id="vpd">--</div><div class="stat-label">VPD (kPa)</div></div>
        <div><div class="stat-value" id="orvalho">--</div><div class="stat-label">Pto Orvalho (°C)</div></div>
        <div><div class="stat-value" id="fungo">--</div><div class="stat-label">Risco Fúngico</div></div>
      </div>
    </div>

    <div class="card card-plant">
      <h2>🌱 Diagnóstico da Planta e Solo</h2>
      <div class="grid">
        <div><div class="stat-value" id="cloro">--</div><div class="stat-label">Índice Verde (G/R)</div></div>
        <div><div class="stat-value" id="ndvi">--</div><div class="stat-label">NDVI Aprox.</div></div>
        <div><div class="stat-value" id="solo">--</div><div class="stat-label">Umidade Solo</div></div>
        <div><div class="stat-value" id="luz">--</div><div class="stat-label">Luz (lux)</div></div>
      </div>
      <div id="msg" class="alert"></div>
      <div id="recomendacao" class="recomendacao" style="display:none;"></div>
      <button id="btn" onclick="iniciar()">🔍 INICIAR DIAGNÓSTICO COMPLETO</button>
      <div id="status" class="status">🔄 Analisando planta... Mantenha o sensor na folha.</div>
    </div>
    
    <div class="footer">Projeto Equipe 05 | CEEP Olegário Macedo |  Agrinho2026</div>
      </div>

  <script>
    let processando = false;

    async function atualizarAmbiente() {
      try {
        const res = await fetch('/ambiente');
        const data = await res.json();
        document.getElementById('temp').innerText = data.temp.toFixed(1);
        document.getElementById('hum').innerText = data.hum.toFixed(1);
        document.getElementById('press').innerText = data.press.toFixed(0);
        document.getElementById('alt').innerText = data.alt.toFixed(0);
        
        const vpdEl = document.getElementById('vpd');
        vpdEl.innerText = data.vpd.toFixed(2);
        vpdEl.className = 'stat-value ' + (data.vpd >= 0.8 && data.vpd <= 1.2 ? 'vpd-ok' : (data.vpd < 0.5 || data.vpd > 1.5 ? 'vpd-danger' : 'vpd-alert'));
        
        document.getElementById('orvalho').innerText = data.orvalho.toFixed(1);
        document.getElementById('fungo').innerText = data.fungo;
      } catch (e) {}
    }

    async function iniciar() {
      if(processando) return;
      processando = true;
      document.getElementById('btn').disabled = true;
      document.getElementById('status').style.display = 'block';
      document.getElementById('msg').style.display = 'none';
      document.getElementById('recomendacao').style.display = 'none';
      
      try {
        await fetch('/iniciar');
        checkStatus();
      } catch (e) {
        alert("Erro de conexão");
        resetUI();
      }
    }

    async function checkStatus() {
      try {
        const res = await fetch('/status');
        const data = await res.json();
        if(data.status === 'pronto') {
          document.getElementById('cloro').innerText = data.cloro.toFixed(2);
          document.getElementById('ndvi').innerText = data.ndvi.toFixed(2);
          document.getElementById('solo').innerText = data.solo + "%";
          document.getElementById('luz').innerText = data.luz;
          
          const msgDiv = document.getElementById('msg');
          msgDiv.style.display = 'block';
          msgDiv.innerText = data.diag;
          msgDiv.className = 'alert ' + data.class;
          
          const recDiv = document.getElementById('recomendacao');
          recDiv.style.display = 'block';
          recDiv.innerHTML = '<strong>🎯 Recomendação:</strong> ' + data.rec;
          
          resetUI();
        } else {
          setTimeout(checkStatus, 500);
        }
      } catch(e) { setTimeout(checkStatus, 500); }
    }

    function resetUI() {
      document.getElementById('status').style.display = 'none';
      document.getElementById('btn').disabled = false;
      processando = false;
    }

    setInterval(atualizarAmbiente, 5000);
    atualizarAmbiente();
  </script>
</body>
</html>
)rawliteral";

// ===========================================================================
// ROTAS DO SERVIDOR WEB (REST API)
// Cada endpoint retorna JSON com charset UTF-8 forçado
// ===========================================================================

// Rota raiz: serve a interface HTML completa
void handleRoot() { 
  server.send(200, "text/html; charset=utf-8", htmlPage); 
}

// Rota /ambiente: dados do BME280 + cálculos derivados (VPD, orvalho, fungo)
void handleAmbiente() {
  // Altitude estimada pela fórmula barométrica internacional
  float alt = 44330.0 * (1.0 - pow(ultimaPress / 1013.25, 0.1903));
  
  String json = "{\"temp\":" + String(ultimaTemp) + 
                ",\"hum\":" + String(ultimaHum) + 
                ",\"press\":" + String(ultimaPress) + 
                ",\"alt\":" + String(alt) +
                ",\"vpd\":" + String(vpd) + 
                ",\"orvalho\":" + String(pontoOrvalho) + 
                ",\"fungo\":\"" + riscoFungo + "\"}";
  server.send(200, "application/json; charset=utf-8", json);
}

// Rota /iniciar: dispara a máquina de estados do diagnóstico
void handleIniciar() {
  // Proteção contra disparos duplicados enquanto diagnóstico está em andamento
  if (!fazerDiagnostico && estadoDiagnostico == 0) {
    fazerDiagnostico = true;
    server.send(200, "text/plain; charset=utf-8", "OK");
  } else {
    server.send(200, "text/plain; charset=utf-8", "BUSY");
  }
}

// Rota /status: polling assíncrono do resultado do diagnóstico
void handleStatus() {
  if (estadoDiagnostico == 3) {
    // Estado 3 = diagnóstico finalizado; envia resultados e reseta FSM
    String json = "{\"status\":\"pronto\",\"cloro\":" + String(clorofilaGR) + 
                  ",\"ndvi\":" + String(indiceNDVI) + 
                  ",\"solo\":" + String(ultimaUmidadeSolo) + 
                  ",\"luz\":" + String(ultimaLuz) +
                  ",\"diag\":\"" + diagnosticoFinal + 
                  "\",\"class\":\"" + classeDiagnostico + 
                  "\",\"rec\":\"" + recomendacao + "\"}";
    server.send(200, "application/json; charset=utf-8", json);
    estadoDiagnostico = 0;  // Reseta FSM para permitir novo ciclo
  } else {
    // Estados 0-2 = diagnóstico em andamento
    server.send(200, "application/json; charset=utf-8", "{\"status\":\"processando\"}");
  }
}

// ===========================================================================
// SETUP: Inicialização de hardware, sensores e servidor
// ===========================================================================
void setup() {
  Serial.begin(115200);
  
  // --- CONFIGURAÇÃO DE PINOS ---
  pinMode(PINO_BOTAO, INPUT_PULLUP);   // Pull-up interno elimina resistor externo
  pinMode(PINO_LED_TCS, OUTPUT);
  digitalWrite(PINO_LED_TCS, LOW);     // LED inicia DESLIGADO (economia energética)
  pinMode(PINO_UMIDADE, INPUT);        // ADC do sensor capacitivo

  // =======================================================================
  // ROTINA DE RECUPERAÇÃO I2C (Anti-Travamento ESP32-S3)
  // PROBLEMA: O ESP32-S3 pode travar o barramento I2C após reset ou brownout,
  //           mantendo SDA em nível BAIXO e impedindo comunicação.
  // SOLUÇÃO: Enviar 9 pulsos de clock manuais no pino SCL força qualquer
  //          dispositivo preso a liberar a linha SDA (conforme errata Espressif).
  // REFERÊNCIA: ESP32-S3 Technical Reference Manual, Seção I2C Known Issues
  // =======================================================================
  
  // Recuperação do barramento Wire (GPIO 9 = SCL do TCS34725)
  pinMode(9, OUTPUT);
  for(int i = 0; i < 9; i++) {
    digitalWrite(9, LOW);
    delayMicroseconds(100);
    digitalWrite(9, HIGH);
    delayMicroseconds(100);
  }
  pinMode(9, INPUT);  // Retorna ao estado de alta impedância
  
  // Recuperação do barramento Wire1 (GPIO 2 = SCL do BME280)
  pinMode(2, OUTPUT);
  for(int i = 0; i < 9; i++) {
    digitalWrite(2, LOW);
    delayMicroseconds(100);
    digitalWrite(2, HIGH);
    delayMicroseconds(100);
  }
  pinMode(2, INPUT);
  // =======================================================================

  // --- INICIALIZAÇÃO DUAL I2C (Solução de Ouro) ---
  // Wire1 (GPIOs 1/2): BME280 → Barramento exclusivo evita conflito de clock stretching
  Wire1.begin(1, 2); 
  Wire1.setClock(100000);  // 100kHz: frequência segura para compatibilidade máxima
  delay(1500);             // Tempo de estabilização pós-power-on do BME280
  
  // Validação explícita de inicialização com feedback serial
  if (!bme.begin(0x76, &Wire1)) {
    Serial.println("ERRO CRITICO: BME280 nao encontrado no Wire1!");
    diagnosticoFinal = "ERRO: Sensor BME280 offline";
    classeDiagnostico = "alert-danger";
  } else {
    Serial.println("BME280 OK (Wire1 - GPIOs 1/2)");
  }
  
  delay(500);  // Intervalo de segurança entre inicializações de sensores I2C
  
  // Wire (GPIOs 8/9): TCS34725 → Barramento independente
  Wire.begin(8, 9); 
  Wire.setClock(100000); 
  
  if (!tcs.begin(0x29, &Wire)) {
    Serial.println("ERRO CRITICO: TCS34725 nao encontrado no Wire!");
    diagnosticoFinal = "ERRO: Sensor TCS34725 offline";
    classeDiagnostico = "alert-danger";
  } else {
    Serial.println("TCS34725 OK (Wire - GPIOs 8/9)");
  }

  // --- INICIALIZAÇÃO DO ACCESS POINT ---
  WiFi.softAP(ssid, password);
  Serial.print("Access Point ativo | IP: "); 
  Serial.println(WiFi.softAPIP());
  
  // --- REGISTRO DAS ROTAS HTTP ---
  server.on("/", handleRoot);
  server.on("/ambiente", handleAmbiente);
  server.on("/iniciar", handleIniciar);
  server.on("/status", handleStatus);
  server.begin();
  Serial.println("Servidor HTTP iniciado na porta 80");
}

// ===========================================================================
// LOOP PRINCIPAL: Arquitetura não-bloqueante baseada em millis()
// IMPORTANTE: Nenhum delay() longo neste escopo para manter o servidor
//             HTTP responsivo durante todo o ciclo de operação
// ===========================================================================
void loop() {
  // Processa requisições HTTP pendentes (deve ser chamado em TODA iteração)
  server.handleClient(); 
  
  // -----------------------------------------------------------------------
  // TAREFA 1: Leitura periódica do BME280 (não-bloqueante via millis)
  // Frequência: A cada 10 segundos (definido por intervaloBME)
  // -----------------------------------------------------------------------
  unsigned long tempoAtual = millis();
  if (tempoAtual - tempoAnteriorBME >= intervaloBME) {
    tempoAnteriorBME = tempoAtual;
    
    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0F;  // Conversão Pa → hPa
    
    // Proteção contra leituras NaN (falha esporádica de comunicação I2C)
    if (!isnan(t)) { 
      ultimaTemp = t; 
      ultimaHum = h; 
      ultimaPress = p; 
      calcularVPD_e_PontoOrvalho();  // Atualiza indicadores derivados
    }
  }

  // -----------------------------------------------------------------------
  // TAREFA 2: Leitura do botão físico com debounce por software
  // Detecção de borda de descida (HIGH→LOW) para disparo único
  // -----------------------------------------------------------------------
  bool estadoAtualBotao = digitalRead(PINO_BOTAO);
  if (estadoAnteriorBotao == HIGH && estadoAtualBotao == LOW) {
    delay(50);  // Debounce: espera mecânica de 50ms
    // Confirmação dupla: verifica se botão ainda está pressionado após debounce
    if (digitalRead(PINO_BOTAO) == LOW && !fazerDiagnostico && estadoDiagnostico == 0) {
      fazerDiagnostico = true;  // Dispara a FSM de diagnóstico
    }
  }
  estadoAnteriorBotao = estadoAtualBotao;

  // -----------------------------------------------------------------------
  // TAREFA 3: MÁQUINA DE ESTADOS FINITA (FSM) DO DIAGNÓSTICO
  // Substitui delay(3000) bloqueante por verificação temporal não-bloqueante
  // Permite que server.handleClient() continue respondendo durante a leitura
  // -----------------------------------------------------------------------
  if (fazerDiagnostico) {
    
    // ESTADO 0: IDLE → Liga LED do TCS e registra timestamp de início
    if (estadoDiagnostico == 0) {
      digitalWrite(PINO_LED_TCS, HIGH);  // LED ligado APENAS durante medição
      tempoDiagnostico = millis();       // Marca tempo de referência
      estadoDiagnostico = 1;             // Transiciona para ESTADO 1
    } 
    
    // ESTADO 1: AGUARDA ESTABILIZAÇÃO → Lê TCS após 3000ms (sem delay!)
    else if (estadoDiagnostico == 1 && millis() - tempoDiagnostico >= 3000) {
      uint16_t r, g, b, c;
      tcs.getRawData(&r, &g, &b, &c);
      ultimaLuz = c;  // Canal Clear = intensidade luminosa total
      
      // Cálculo dos índices espectrais (proteção contra divisão por zero)
      if (r > 0) {
        clorofilaGR = (float)g / (float)r;                    // Índice Verde/Vermelho
        indiceNDVI = (float)(g - r) / (float)(g + r);         // NDVI aproximado
      }
      
      tempoDiagnostico = millis();  // Reinicia contador para próxima etapa
      estadoDiagnostico = 2;        // Transiciona para ESTADO 2
    }
    
    // ESTADO 2: LEITURA DO SOLO + FINALIZAÇÃO → Após 500ms de pausa
    else if (estadoDiagnostico == 2 && millis() - tempoDiagnostico >= 500) {
      // Média móvel de 10 amostras para filtrar ruído do ADC do ESP32-S3
      long somaSolo = 0;
      for(int i = 0; i < 10; i++) { 
        somaSolo += analogRead(PINO_UMIDADE); 
        delay(2);  // Delay curto aceitável aqui (total: 20ms, não impacta HTTP)
      }
      int valorSolo = somaSolo / 10;
      
      // Mapeamento linear ADC → porcentagem com proteção de limites
      ultimaUmidadeSolo = constrain(map(valorSolo, SOLO_SECO, SOLO_MOLHADO, 0, 100), 0, 100);
      
      // Desliga LED imediatamente após conclusão das leituras
      digitalWrite(PINO_LED_TCS, LOW);
      
      // Gera diagnóstico cruzado com todos os dados coletados
      gerarDiagnosticoIntegrado();
      
      fazerDiagnostico = false;   // Libera flag para novo acionamento
      estadoDiagnostico = 3;      // Sinaliza resultado pronto para a rota /status
    }
  }
}
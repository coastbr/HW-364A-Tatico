#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266Ping.h>
#include <time.h>
#include <user_interface.h> 

ESP8266WebServer server(80);

// ==========================================
// CONFIGURAÇÃO DOS PINOS (BOTÕES FÍSICOS)
// ==========================================
#define PINO_BOTAO_EVIL  5  // D1 
#define PINO_BOTAO_SNIFF 4  // D2 
#define PINO_BOTAO_RESET 13 // D7 

#define LED_PIN LED_BUILTIN 
bool ledLigado = false; 

// ==========================================
// CONFIGURAÇÃO DO OLED E ANIMAÇÃO
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define OLED_SDA 14 
#define OLED_SCL 12 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

unsigned long ultimaAtualizacaoOLED = 0;
unsigned long ultimaAnimacaoCachorro = 0;
unsigned long ultimoDebounce = 0; 
int estadoCachorro = 0; 
bool modoFixo = false; 

// ==========================================
// VARIÁVEIS DO BACKGROUND SCANNER
// ==========================================
bool scanAtivo = false;
int scanIpAtual = 1;
int scanIpLimite = 254;
String scanResultados = "";
bool scanAchouAlguem = false;

// ------------------------------------------
// FUNÇÕES DE SISTEMA E REDE 
// ------------------------------------------
IPAddress getSystemIP() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) return WiFi.softAPIP(); 
  return WiFi.localIP();
}

String getSystemSSID() {
  if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) return WiFi.softAPSSID();
  String ssid = WiFi.SSID();
  if (ssid.length() == 0 || ssid.indexOf("unknown") != -1) return "Desconectado";
  return ssid;
}

String getUptime() {
  unsigned long seg = millis() / 1000;
  char tempoStr[20];
  sprintf(tempoStr, "%02lu:%02lu:%02lu", seg / 3600, (seg % 3600) / 60, seg % 60);
  return String(tempoStr);
}

String getHoraNTP() {
  time_t now = time(nullptr);
  if (now < 100000) return "--:--"; 
  struct tm* timeinfo = localtime(&now);
  char horaStr[10];
  sprintf(horaStr, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  return String(horaStr);
}

// ------------------------------------------
// MASCOTE E TELA
// ------------------------------------------
void desenhaCachorro(int estado) {
  int x = 90; int y = 25; 
  display.fillTriangle(x+2, y+5, x+8, y-5, x+14, y+2, SSD1306_WHITE); 
  display.fillTriangle(x+28, y+5, x+22, y-5, x+16, y+2, SSD1306_WHITE); 
  display.fillRoundRect(x, y, 30, 24, 6, SSD1306_WHITE);
  display.fillCircle(x+15, y+17, 4, SSD1306_BLACK); 
  if (estado == 3) { 
    display.drawLine(x+5, y+8, x+12, y+8, SSD1306_BLACK); display.drawLine(x+17, y+8, x+24, y+8, SSD1306_BLACK);
  } else if (estado == 4) { 
    display.fillRect(x+2, y+4, 14, 8, SSD1306_BLACK); display.fillRect(x+14, y+4, 14, 8, SSD1306_BLACK); 
    display.drawLine(x, y+6, x+2, y+6, SSD1306_BLACK); display.drawLine(x+28, y+6, x+30, y+6, SSD1306_BLACK); 
  } else if (estado == 5) { 
    display.fillRoundRect(x+5, y+6, 8, 8, 2, SSD1306_BLACK); display.fillRoundRect(x+17, y+6, 8, 8, 2, SSD1306_BLACK);
    display.fillRect(x+7, y+8, 2, 2, SSD1306_WHITE); display.fillRect(x+19, y+8, 2, 2, SSD1306_WHITE); 
    display.drawLine(x+4, y+3, x+13, y+6, SSD1306_BLACK); display.drawLine(x+4, y+2, x+13, y+5, SSD1306_BLACK);
    display.drawLine(x+26, y+3, x+17, y+6, SSD1306_BLACK); display.drawLine(x+26, y+2, x+17, y+5, SSD1306_BLACK);
  } else { 
    display.fillRoundRect(x+5, y+5, 8, 10, 2, SSD1306_BLACK); display.fillRoundRect(x+17, y+5, 8, 10, 2, SSD1306_BLACK);
    int pupilaX = (estado == 1) ? 0 : ((estado == 2) ? 4 : 2); 
    display.fillRect(x+5 + pupilaX, y+9, 2, 2, SSD1306_WHITE); display.fillRect(x+17 + pupilaX, y+9, 2, 2, SSD1306_WHITE); 
  }
}

void atualizaOLED(String mensagemExtra = "") {
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setCursor(0,0);
  if (WiFi.getMode() == WIFI_AP) { display.print("AP: "); display.println(getSystemIP()); } 
  else { display.print("IP: "); display.println(getSystemIP()); }
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  if (mensagemExtra != "") {
    display.setCursor(0, 20); display.println(mensagemExtra); desenhaCachorro(estadoCachorro); 
  } else {
    display.setCursor(0, 20); display.print("Hora:"); display.println(getHoraNTP());
    display.setCursor(0, 30); display.print("LED :");
    if(ledLigado) { display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); display.println(" ON"); } 
    else { display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); display.println("OFF"); }
    display.setTextColor(SSD1306_WHITE); 
    display.setCursor(0, 40); display.print("Up:"); display.println(getUptime());
    display.setCursor(0, 50); display.print("RAM:"); display.print(ESP.getFreeHeap() / 1024); display.println("KB");
    desenhaCachorro(estadoCachorro);
  }
  display.display();
}

// ------------------------------------------
// LÓGICA DO SNIFFER E IDS (BLUE TEAM)
// ------------------------------------------
volatile uint32_t pacotesCapturados = 0;
volatile uint32_t deauthCapturados = 0;

void IRAM_ATTR sniffer_callback(uint8_t *buf, uint16_t len) { 
  pacotesCapturados++; 
  // O cabeçalho 802.11 geralmente começa no byte 12 da estrutura RxControl do ESP8266
  if (len > 12) {
    // 0xC0 = Frame de Desautenticação | 0xA0 = Frame de Desassociação
    if (buf[12] == 0xC0 || buf[12] == 0xA0) {
      deauthCapturados++;
    }
  }
}

// ==========================================
// FUNÇÕES TÁTICAS (Botões e Web)
// ==========================================
String executarEvilTwin() {
  modoFixo = true; estadoCachorro = 4; atualizaOLED("Analisando o\nEspectro WiFi\n(2.4GHz)...");
  int modoAtual = WiFi.getMode(); if(modoAtual == WIFI_AP) WiFi.mode(WIFI_AP_STA);
  int n = WiFi.scanNetworks(); 
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#0f0; font-family:monospace; text-align:center;'><h2>Relatório Rogue AP</h2><ul style='list-style:none; padding:0;'>";
  bool clone = false;
  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      if (WiFi.SSID(i) == WiFi.SSID(j)) {
        clone = true; html += "<li style='border:1px solid red; margin:10px; padding:10px;'>ALERTA! Clone:<br>SSID: <b>" + WiFi.SSID(i) + "</b><br>Original: " + WiFi.BSSIDstr(i) + "<br>Suspeito: " + WiFi.BSSIDstr(j) + "</li>";
      }
    }
  }
  if (clone) { estadoCachorro = 5; atualizaOLED("ALERTA!\nEvil Twin\nDetectado!"); } 
  else { estadoCachorro = 0; atualizaOLED("Espectro\nLimpo."); html += "<p>Tudo Limpo. Nenhuma clonagem detectada.</p>"; }
  html += "</ul><br><a href='/'><button style='background:#333; color:#0f0; padding:10px;'>Voltar</button></a></body></html>";
  WiFi.mode((WiFiMode_t)modoAtual); 
  return html; 
}

void executarSniffer() {
  int modoAtual = WiFi.getMode(); 
  modoFixo = true; estadoCachorro = 4; 
  WiFi.disconnect(true); WiFi.softAPdisconnect(true); delay(200);
  WiFi.mode(WIFI_STA); delay(200);
  wifi_promiscuous_enable(0); wifi_set_promiscuous_rx_cb(sniffer_callback); wifi_promiscuous_enable(1);

  int trafegoCanal[14] = {0}; int picoMaximo = 10; 
  for(int ciclos = 0; ciclos < 4; ciclos++) {
    for(int canal = 1; canal <= 11; canal++) {
      wifi_set_channel(canal); pacotesCapturados = 0; delay(250); trafegoCanal[canal] += pacotesCapturados; 
      if (trafegoCanal[canal] > picoMaximo) picoMaximo = trafegoCanal[canal];
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setCursor(0,0);
      display.print("Sniffer CH:"); display.println(canal); display.drawLine(0, 10, 80, 10, SSD1306_WHITE);
      for(int i = 1; i <= 11; i++) {
        int alturaBarra = map(trafegoCanal[i], 0, picoMaximo, 0, 45);
        if (alturaBarra > 45) alturaBarra = 45; if (alturaBarra == 0 && trafegoCanal[i] > 0) alturaBarra = 1; 
        display.fillRect((i - 1) * 7, 64 - alturaBarra, 5, alturaBarra, SSD1306_WHITE);
      }
      desenhaCachorro(estadoCachorro); display.display();
    }
  }

  wifi_promiscuous_enable(0); WiFi.mode(WIFI_OFF); delay(200); 
  if (modoAtual == WIFI_STA) {
    WiFi.mode(WIFI_STA); WiFi.begin(); atualizaOLED("Reconectando\nao WiFi...");
    int tentativas = 0; while (WiFi.status() != WL_CONNECTED && tentativas < 30) { delay(500); tentativas++; }
  } else { 
    WiFi.mode(WIFI_AP); delay(200); WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0)); WiFi.softAP("HW-364A_Tatico", "12345678"); 
  }
  modoFixo = false; estadoCachorro = 0; ultimaAtualizacaoOLED = millis() - 2000;
}

void executarDeauthDetector() {
  int modoAtual = WiFi.getMode(); 
  modoFixo = true; estadoCachorro = 4; // Começa modo alerta
  
  WiFi.disconnect(true); WiFi.softAPdisconnect(true); delay(200);
  WiFi.mode(WIFI_STA); delay(200);

  wifi_promiscuous_enable(0); 
  wifi_set_promiscuous_rx_cb(sniffer_callback); 
  wifi_promiscuous_enable(1);

  deauthCapturados = 0; 
  
  for(int ciclos = 0; ciclos < 3; ciclos++) {
    for(int canal = 1; canal <= 11; canal++) {
      wifi_set_channel(canal); 
      delay(300); 
      
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setCursor(0,0);
      display.print("Blue Team IDS - CH:"); display.println(canal); 
      display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
      
      display.setCursor(0, 20);
      display.print("Deauths: "); display.println(deauthCapturados);
      
      if (deauthCapturados > 5) {
        estadoCachorro = 5; // Bravo!
        display.fillRect(0, 35, 128, 20, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
        display.setCursor(10, 41);
        display.println("ATAQUE DETECTADO!");
        digitalWrite(LED_PIN, !digitalRead(LED_PIN)); // Pisca sirene
      } else {
        display.setCursor(0, 40);
        display.println("Area Segura...");
      }
      
      desenhaCachorro(estadoCachorro); 
      display.display();
    }
  }

  wifi_promiscuous_enable(0); WiFi.mode(WIFI_OFF); delay(200); 
  digitalWrite(LED_PIN, HIGH); // Garante que o LED desligue

  if (modoAtual == WIFI_STA) {
    WiFi.mode(WIFI_STA); WiFi.begin(); atualizaOLED("Reconectando\nao WiFi...");
    int tentativas = 0; while (WiFi.status() != WL_CONNECTED && tentativas < 30) { delay(500); tentativas++; }
  } else { 
    WiFi.mode(WIFI_AP); delay(200); WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0)); WiFi.softAP("HW-364A_Tatico", "12345678"); 
  }
  modoFixo = false; estadoCachorro = 0; ultimaAtualizacaoOLED = millis() - 2000;
}

void executarReset() {
  atualizaOLED("Limpando...\nReiniciando"); 
  delay(2000); WiFi.disconnect(true); WiFi.softAPdisconnect(true); delay(500); ESP.restart(); 
}

// ==========================================
// PÁGINAS HTML (Com botão IDS e UTF-8)
// ==========================================
const char paginaPrincipal[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HW-364A Tático</title>
<style>
  body { font-family: 'Courier New', Courier, monospace; text-align: center; background-color: #111; color: #0f0; margin: 0; padding: 20px;}
  .card { background: #222; border: 1px solid #0f0; padding: 20px; border-radius: 5px; margin-bottom: 20px;}
  .btn { background-color: #0f0; border: none; color: #000; padding: 15px; font-weight: bold; margin: 5px; cursor: pointer; border-radius: 3px; width: 45%; max-width: 150px;}
  .btn-off { background-color: #555; color: #fff;}
  .btn-tool { background-color: #333; color: #0f0; border: 1px solid #0f0; width: 90%; max-width: 300px; margin-top: 10px;}
  .btn-tool:hover { background-color: #0f0; color: #000; }
  .btn-danger { background-color: #d32f2f; color: white; border: none;}
  .btn-sniff { background-color: #9c27b0; color: white; border: none;}
  .btn-wifi { border-color: #ff9800; color: #ff9800;}
  .btn-wifi:hover { background-color: #ff9800; color: #000;}
  .btn-ids { border-color: #00bcd4; color: #00bcd4;}
  .btn-ids:hover { background-color: #00bcd4; color: #000;}
  h2 { margin-top: 0; border-bottom: 1px solid #0f0; padding-bottom: 10px;}
</style>
<script>
  function toggleLed(estado) { fetch('/' + estado).then(r => { if(r.ok) fetchStatus(); }); }
  function fetchStatus() {
    fetch('/status').then(r => r.json()).then(data => {
      document.getElementById('ledState').innerText = data.led ? "ON" : "OFF";
      document.getElementById('ssidInfo').innerText = data.ssid;
      document.getElementById('ram').innerText = data.ram + " KB";
      document.getElementById('mac').innerText = data.mac;
    });
  }
  setInterval(fetchStatus, 3000); window.onload = fetchStatus;
</script></head><body>
  <div class="card">
    <h2>Sistema Base</h2>
    <p>LED Status: <span id="ledState">--</span></p>
    <button class="btn" onclick="toggleLed('ligar')">Ligar</button>
    <button class="btn btn-off" onclick="toggleLed('desligar')">Desligar</button>
    <p>Rede: <span id="ssidInfo">--</span></p>
    <a href="/scanwifi"><button class="btn btn-tool btn-wifi">Trocar Rede Wi-Fi</button></a><br>
    <p>RAM: <span id="ram">--</span></p>
    <p style="font-size: 12px; color: #888;">MAC: <span id="mac">--</span></p>
  </div>
  <div class="card">
    <h2>Arsenal (Recon & Evasão)</h2>
    <a href="/scanui"><button class="btn btn-tool">Escanear IPs Locais</button></a><br>
    <a href="/eviltwin"><button class="btn btn-tool">Caçar Evil Twins</button></a><br>
    <a href="/macspoof"><button class="btn btn-tool">Injetar Novo MAC</button></a><br>
    <a href="/sniffer"><button class="btn btn-tool btn-sniff">Packet Sniffer (Gráfico)</button></a><br>
    <a href="/deauth"><button class="btn btn-tool btn-ids">IDS: Detector de Deauth</button></a><br>
    <a href="/resetarwifi"><button class="btn btn-tool btn-danger" style="margin-top:30px;">Limpar WiFi / Forçar AP</button></a>
  </div>
</body></html>
)rawliteral";

const char paginaScanner[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Radar de IPs</title>
<style>
  body { font-family: 'Courier New', Courier, monospace; text-align: center; background-color: #111; color: #0f0; margin: 0; padding: 20px;}
  .card { background: #222; border: 1px solid #0f0; padding: 20px; border-radius: 5px; margin-bottom: 20px; text-align: left;}
  .btn { background-color: #0f0; border: none; color: #000; padding: 15px; font-weight: bold; margin: 5px; cursor: pointer; border-radius: 3px; width: 45%; max-width: 150px;}
  .btn-danger { background-color: #d32f2f; color: white;}
  input { padding: 10px; font-size: 16px; width: 80px; background: #333; color: #0f0; border: 1px solid #0f0; text-align: center;}
</style>
<script>
  let timer;
  function startScan() {
    let lim = document.getElementById('limite').value;
    fetch('/api/startscan?limit=' + lim).then(() => {
      document.getElementById('statusMsg').innerText = "Iniciando...";
      document.getElementById('resultados').innerHTML = "";
      timer = setInterval(pollScan, 1000); 
    });
  }
  function stopScan() {
    fetch('/api/stopscan').then(() => { clearInterval(timer); document.getElementById('statusMsg').innerText = "Cancelado pelo usuário."; });
  }
  function pollScan() {
    fetch('/api/scanstatus').then(r => r.json()).then(d => {
      document.getElementById('statusMsg').innerText = "Escaneando IP final " + d.ipAtual + " de " + d.limite;
      document.getElementById('resultados').innerHTML = d.resultados;
      if (!d.ativo) { clearInterval(timer); document.getElementById('statusMsg').innerText += " (CONCLUÍDO)"; }
    });
  }
</script></head><body>
  <h2>Radar Tático</h2>
  <p>Limite de Varredura (1 a 254 IPs):</p>
  <input type="number" id="limite" value="50" min="1" max="254"><br><br>
  <button class="btn" onclick="startScan()">INICIAR</button>
  <button class="btn btn-danger" onclick="stopScan()">CANCELAR</button>
  <div class="card" style="margin-top: 20px;"><h3 id="statusMsg" style="border-bottom: 1px dashed #0f0; padding-bottom: 10px;">Aguardando comando...</h3><ul id="resultados" style="list-style:none; padding:0;"></ul></div>
  <a href="/"><button class="btn" style="background:#555; color:white; width:90%; max-width:300px;">Voltar à Base</button></a>
</body></html>
)rawliteral";

// ==========================================
// INICIALIZAÇÃO
// ==========================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, HIGH); 
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) for(;;);
  
  pinMode(PINO_BOTAO_EVIL, INPUT_PULLUP); pinMode(PINO_BOTAO_SNIFF, INPUT_PULLUP); pinMode(PINO_BOTAO_RESET, INPUT_PULLUP);
  
  estadoCachorro = 4; modoFixo = true; atualizaOLED("Iniciando OS...");
  
  WiFi.persistent(false); WiFi.mode(WIFI_STA); delay(500);
  
  WiFiManager wifiManager; wifiManager.setConnectTimeout(15); wifiManager.setConfigPortalTimeout(15); 
  if (!wifiManager.autoConnect("HW-364A_Config")) { 
    WiFi.disconnect(true); WiFi.softAPdisconnect(true); delay(500);
    WiFi.mode(WIFI_AP); delay(200); WiFi.softAP("HW-364A_Tatico", "12345678"); delay(2000);
  } else { configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); }

  modoFixo = false; estadoCachorro = 0;

  server.on("/", []() { server.send(200, "text/html", paginaPrincipal); });
  server.on("/scanui", []() { server.send(200, "text/html", paginaScanner); });
  server.on("/status", []() {
    String json = "{\"led\":" + String(ledLigado ? "true" : "false") + ",\"ssid\":\"" + getSystemSSID() + "\",\"ram\":" + String(ESP.getFreeHeap() / 1024) + ",\"mac\":\"" + WiFi.macAddress() + "\"}";
    server.send(200, "application/json", json);
  });
  
  server.on("/ligar", []() { ledLigado = true; digitalWrite(LED_PIN, LOW); atualizaOLED(); server.send(200, "text/plain", "OK"); });
  server.on("/desligar", []() { ledLigado = false; digitalWrite(LED_PIN, HIGH); atualizaOLED(); server.send(200, "text/plain", "OK"); });
  server.on("/resetarwifi", []() { server.send(200, "text/plain", "Limpando configuracoes..."); executarReset(); });
  server.on("/eviltwin", []() { String html = executarEvilTwin(); server.send(200, "text/html", html); delay(4000); modoFixo = false; });
  
  server.on("/sniffer", []() { server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#0f0; text-align:center;'><h2>Sniffer Ativado!</h2><p>Placa offline por 15s. Olhe no OLED!</p></body></html>"); delay(1000); executarSniffer(); });
  
  // NOVA ROTA DO IDS (BLUE TEAM)
  server.on("/deauth", []() { server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#00bcd4; text-align:center;'><h2>IDS Ativado!</h2><p>Placa offline por 10s. Olhe no OLED!</p></body></html>"); delay(1000); executarDeauthDetector(); });

  server.on("/scanwifi", []() {
    modoFixo = true; estadoCachorro = 4; atualizaOLED("Buscando\nRedes WiFi...");
    int modoAtual = WiFi.getMode(); if(modoAtual == WIFI_AP) WiFi.mode(WIFI_AP_STA);
    int n = WiFi.scanNetworks();
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#0f0; font-family:monospace; text-align:center;'><h2>Selecionar Alvo</h2>";
    if (n == 0) html += "<p>Nenhuma rede encontrada.</p>";
    else {
      html += "<form action='/connectwifi' method='POST'><select name='ssid' style='padding:10px; background:#222; color:#0f0; border:1px solid #0f0; width:80%; max-width:300px; margin-bottom:10px;'>";
      for (int i = 0; i < n; ++i) html += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + " (" + WiFi.RSSI(i) + "dBm)</option>";
      html += "</select><br><input type='text' name='senha' placeholder='Senha da Rede' style='padding:10px; background:#222; color:#0f0; border:1px solid #0f0; width:80%; max-width:300px; margin-bottom:10px;'><br><button type='submit' style='background:#0f0; color:#000; padding:15px 30px; font-weight:bold; border:none; cursor:pointer;'>CONECTAR</button></form>";
    }
    html += "<br><a href='/'><button style='background:#333; color:#0f0; padding:10px 20px; border:1px solid #0f0;'>Cancelar</button></a></body></html>";
    server.send(200, "text/html", html); WiFi.mode((WiFiMode_t)modoAtual); modoFixo = false; estadoCachorro = 0;
  });

  server.on("/connectwifi", []() {
    if (server.hasArg("ssid")) {
      String n_ssid = server.arg("ssid"); String n_pass = server.arg("senha");
      server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#0f0; text-align:center; padding-top:50px;'><h2>Conectando...</h2><p>A placa vai reiniciar e conectar em <b>" + n_ssid + "</b>.</p><p>Olhe a tela OLED para ver o novo IP.</p></body></html>");
      modoFixo = true; estadoCachorro = 4; atualizaOLED("Salvando rede\ne Reiniciando..."); delay(2000);
      WiFi.begin(n_ssid.c_str(), n_pass.c_str()); delay(1000); ESP.restart();
    }
  });

  server.on("/api/startscan", []() {
    if (server.hasArg("limit")) scanIpLimite = server.arg("limit").toInt(); if (scanIpLimite < 1 || scanIpLimite > 254) scanIpLimite = 254;
    scanAtivo = true; scanIpAtual = 1; scanResultados = ""; scanAchouAlguem = false; modoFixo = true; estadoCachorro = 1; server.send(200, "text/plain", "Started");
  });
  server.on("/api/stopscan", []() { scanAtivo = false; modoFixo = false; estadoCachorro = 0; atualizaOLED("Scan\nCancelado!"); server.send(200, "text/plain", "Stopped"); });
  server.on("/api/scanstatus", []() {
    String json = "{\"ativo\":" + String(scanAtivo ? "true" : "false") + ",\"ipAtual\":" + String(scanIpAtual) + ",\"limite\":" + String(scanIpLimite) + ",\"resultados\":\"" + scanResultados + "\"}";
    server.send(200, "application/json", json);
  });

  server.on("/macspoof", []() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#0f0; font-family:monospace; text-align:center;'><h2>MAC Spoofing (Evasão)</h2><p>Atual: " + WiFi.macAddress() + "</p><form action='/applymac' method='POST'><input type='text' name='newmac' style='padding:10px; background:#222; color:#0f0;'><br><br><button type='submit' style='background:#d32f2f; color:white; padding:15px;'>INJETAR</button></form></body></html>";
    server.send(200, "text/html", html);
  });
  server.on("/applymac", []() {
    if (server.hasArg("newmac")) {
      String macStr = server.arg("newmac"); 
      server.send(200, "text/html", "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'></head><body style='background:#111; color:#0f0; text-align:center;'><h2>Derrubando rede para injetar...</h2><p>Olhe no OLED o novo IP.</p><br><p>(Nota: Em modo AP Off-Grid, a placa apenas pisca e volta para 192.168.4.1)</p></body></html>");
      modoFixo = true; estadoCachorro = 4; atualizaOLED("Trocando MAC..."); delay(2000); 
      int m[6];
      if (6 == sscanf(macStr.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5])) {
        uint8_t newMac[6]; for (int i = 0; i < 6; ++i) newMac[i] = (uint8_t)m[i];
        int modoAtual = WiFi.getMode(); WiFi.disconnect(); delay(100); 
        if(modoAtual == WIFI_STA) { wifi_set_macaddr(STATION_IF, newMac); WiFi.begin(); atualizaOLED("Reconectando\nao WiFi...");
          int tentativas = 0; while (WiFi.status() != WL_CONNECTED && tentativas < 30) { delay(500); tentativas++; }
        } else { wifi_set_macaddr(SOFTAP_IF, newMac); WiFi.softAP("HW-364A_Tatico", "12345678"); }
      }
      modoFixo = false; estadoCachorro = 0; ultimaAtualizacaoOLED = millis() - 2000;
    }
  });

  server.begin();
}

// ------------------------------------------
// LOOP PRINCIPAL 
// ------------------------------------------
void loop() {
  server.handleClient();
  
  if (!scanAtivo && millis() - ultimoDebounce > 500) {
    if (digitalRead(PINO_BOTAO_EVIL) == LOW) { ultimoDebounce = millis(); String s = executarEvilTwin(); delay(4000); modoFixo = false; }
    else if (digitalRead(PINO_BOTAO_SNIFF) == LOW) { ultimoDebounce = millis(); executarSniffer(); }
    else if (digitalRead(PINO_BOTAO_RESET) == LOW) { ultimoDebounce = millis(); executarReset(); }
  }
  
  if (scanAtivo) {
    IPAddress meuIP = getSystemIP(); IPAddress ipAlvo(meuIP[0], meuIP[1], meuIP[2], scanIpAtual);
    if (millis() - ultimaAnimacaoCachorro >= 300) {
      ultimaAnimacaoCachorro = millis(); estadoCachorro = (estadoCachorro == 1) ? 2 : 1;
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE); display.setCursor(0,0);
      display.println("Radar Ligado"); display.drawLine(0, 10, 128, 10, SSD1306_WHITE); display.setCursor(0, 25); display.println("Testando IP:"); display.setTextSize(2); display.setCursor(0, 40); display.print(scanIpAtual); desenhaCachorro(estadoCachorro); display.display();
    }
    if (ipAlvo != meuIP) {
      if (Ping.ping(ipAlvo, 1)) {
        scanAchouAlguem = true; String portasAbertas = ""; int portasAlvo[] = {80, 22, 443};
        for (int p = 0; p < 3; p++) { WiFiClient client; if (client.connect(ipAlvo, portasAlvo[p])) { portasAbertas += String(portasAlvo[p]) + " "; client.stop(); } }
        scanResultados += "<li style='border:1px solid #0f0; padding:10px; margin-top:5px;'>IP: <strong>" + ipAlvo.toString() + "</strong> <span style='color:white;'>(Ativo)</span><br>";
        if (portasAbertas != "") scanResultados += "Portas: <span style='color:#fff;'>" + portasAbertas + "</span></li>"; else scanResultados += "Nenhuma porta mapeada aberta.</li>";
      }
    }
    scanIpAtual++;
    if (scanIpAtual > scanIpLimite || scanIpAtual > 254) {
      scanAtivo = false; modoFixo = false; estadoCachorro = 0;
      if (!scanAchouAlguem) scanResultados += "<li>Nenhum dispositivo encontrado no raio definido.</li>"; atualizaOLED("Radar\nConcluido!");
    }
  } else {
    if (!modoFixo && millis() - ultimaAnimacaoCachorro >= random(1500, 4000)) {
      ultimaAnimacaoCachorro = millis(); int sorteio = random(0, 100);
      if (sorteio < 20) estadoCachorro = 3; else if (sorteio < 45) estadoCachorro = 1; else if (sorteio < 70) estadoCachorro = 2; else estadoCachorro = 0;                   
      atualizaOLED(); 
    }
    if (!modoFixo && millis() - ultimaAtualizacaoOLED >= 1000) { ultimaAtualizacaoOLED = millis(); atualizaOLED(); }
  }
}
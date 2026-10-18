#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "esp_sleep.h"

// T-Embed CC1101 pin map from LilyGO's public board documentation.
static constexpr int USER_BUTTON_PIN = 6;
static constexpr int DISPLAY_BACKLIGHT_PIN = 21;
static constexpr char AP_SSID[] = "PocketChat";
// Change this to your own password (8+ characters) before building.
static constexpr char AP_PASSWORD[] = "Pc-7mK9-xQ2v";
static constexpr uint32_t LONG_PRESS_MS = 3000;
static constexpr uint16_t DNS_PORT = 53;

TFT_eSPI tft = TFT_eSPI();
WebServer server(80);
DNSServer dnsServer;
IPAddress localIP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
bool dnsStarted = false;
uint32_t buttonPressedAt = 0;
bool buttonHandled = false;

static const char INDEX_HTML[] PROGMEM = R"POCKETCHATHTML(
<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><meta name="theme-color" content="#111827"><title>PocketChat</title><style>
:root{color-scheme:dark;--bg:#0b1120;--panel:#121c2e;--panel2:#1a2940;--line:#283952;--blue:#59d0ff;--text:#f3f7ff;--muted:#9badc5}*{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#172941 0,#0b1120 45%);color:var(--text);font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;min-height:100vh}.app{max-width:760px;margin:auto;min-height:100vh;display:flex;flex-direction:column}.top{position:sticky;top:0;background:#101a2aee;backdrop-filter:blur(12px);border-bottom:1px solid var(--line);padding:15px 16px;display:flex;align-items:center;gap:12px;z-index:2}.logo{width:42px;height:42px;border-radius:14px;background:linear-gradient(135deg,#59d0ff,#7c7cff);display:grid;place-items:center;color:#071321;font-size:22px;font-weight:900}.brand{font-weight:800;font-size:19px;letter-spacing:.2px}.sub{font-size:12px;color:var(--muted);margin-top:2px}.online{margin-left:auto;font-size:12px;color:#8ff0bd;display:flex;align-items:center;gap:6px}.dot{width:7px;height:7px;border-radius:50%;background:#4de39b;box-shadow:0 0 12px #4de39b}.profile{margin:14px 12px 0;padding:12px;background:var(--panel);border:1px solid var(--line);border-radius:18px;display:flex;align-items:center;gap:11px}.avatar{width:48px;height:48px;border-radius:50%;object-fit:cover;background:#273a53;display:grid;place-items:center;font-size:22px;flex-shrink:0}.profile-fields{display:flex;gap:8px;flex:1;min-width:0}.profile-fields input{width:100%;min-width:0}.input{border:1px solid #31445f;border-radius:12px;background:#0d1727;color:var(--text);padding:12px;font:inherit;outline:none}.input:focus{border-color:var(--blue)}button{font:inherit;border:0;cursor:pointer}.btn{border-radius:12px;padding:11px 13px;background:var(--blue);color:#06131f;font-weight:750;white-space:nowrap}.btn.secondary{background:#24364d;color:#e9f4ff;border:1px solid #344a65}.messages{padding:16px 12px 100px;flex:1;display:flex;flex-direction:column;gap:10px}.empty{text-align:center;color:var(--muted);padding:44px 20px}.bubble{max-width:88%;display:flex;gap:9px;align-items:flex-end;animation:pop .15s ease-out}.bubble.mine{align-self:flex-end;flex-direction:row-reverse}.mini{width:32px;height:32px;border-radius:50%;background:#263a55;object-fit:cover;display:grid;place-items:center;flex-shrink:0;font-size:14px}.content{background:var(--panel2);border:1px solid #2a3e58;border-radius:15px 15px 15px 4px;padding:10px 12px;min-width:70px;overflow-wrap:anywhere}.mine .content{background:#143c56;border-color:#1e5674;border-radius:15px 15px 4px 15px}.who{font-size:12px;color:var(--blue);font-weight:750;margin-bottom:4px}.body{white-space:pre-wrap;line-height:1.4}.time{font-size:10px;color:#a9bdd4;text-align:right;margin-top:5px}.composer{position:sticky;bottom:0;background:#0e1828ee;backdrop-filter:blur(12px);border-top:1px solid var(--line);padding:12px;display:flex;gap:8px;padding-bottom:max(12px,env(safe-area-inset-bottom))}.composer input{flex:1;min-width:0}.hint{font-size:11px;color:var(--muted);text-align:center;padding:0 12px 10px}.hidden{display:none!important}input[type=file]{display:none}@keyframes pop{from{transform:translateY(4px);opacity:.4}to{transform:none;opacity:1}}@media(min-width:600px){.profile{margin-left:18px;margin-right:18px}.messages{padding-left:18px;padding-right:18px}}
</style></head><body><div class="app"><header class="top"><div class="logo">P</div><div><div class="brand">PocketChat</div><div class="sub">Твоя локальная комната · без интернета</div></div><div class="online"><i class="dot"></i><span id="state">Подключено</span></div></header>
<section class="profile"><img id="avatar" class="avatar" alt="Аватар" src=""><div class="profile-fields"><input id="nick" class="input" maxlength="24" placeholder="Твой ник" autocomplete="nickname"></div><button class="btn secondary" id="save">Сохранить</button><button class="btn secondary" id="pick">＋</button><input id="file" type="file" accept="image/*"></section>
<main id="messages" class="messages"><div class="empty">👋 Добро пожаловать в PocketChat<br><small>Напиши первое сообщение!</small></div></main><div class="hint">Сообщения хранятся на T-Embed, пока не заполнится лимит истории.</div>
<form id="composer" class="composer"><input id="message" class="input" maxlength="500" placeholder="Написать сообщение…" autocomplete="off"><button class="btn" type="submit">➤</button></form></div>
<script>
const $=id=>document.getElementById(id);let avatarData='',lastCount=-1,sending=false;
try{$('nick').value=localStorage.getItem('pc_nick')||'';avatarData=localStorage.getItem('pc_avatar')||'';if(avatarData)$('avatar').src=avatarData}catch(e){}
function initials(n){return (n||'?').trim().slice(0,1).toUpperCase()||'?'}
function safeAvatar(el,src,n){if(src){el.src=src;el.style.display='block'}else{el.removeAttribute('src');el.style.display='grid';el.alt=initials(n);el.onerror=()=>{el.removeAttribute('src');el.alt=initials(n)}}}
function setState(t){$('state').textContent=t}
async function api(path,opts={}){const r=await fetch(path,{cache:'no-store',...opts,headers:{'Content-Type':'application/json',...(opts.headers||{})}});if(!r.ok)throw Error('HTTP '+r.status);return r.json()}
async function saveProfile(){const nick=$('nick').value.trim();if(!nick){alert('Сначала введи ник');return}try{await api('/api/profile',{method:'POST',body:JSON.stringify({nick,avatar:avatarData})});try{localStorage.setItem('pc_nick',nick);localStorage.setItem('pc_avatar',avatarData)}catch(e){}setState('Профиль сохранён')}catch(e){setState('Ошибка сервера');alert('Не удалось сохранить профиль. Проверь подключение к PocketChat.')}}
$('save').onclick=saveProfile;$('pick').onclick=()=>$('file').click();
$('file').onchange=e=>{const f=e.target.files&&e.target.files[0];if(!f)return;if(!f.type.startsWith('image/')){alert('Выбери изображение');return}const reader=new FileReader();reader.onload=()=>{const im=new Image();im.onload=()=>{const c=document.createElement('canvas');c.width=96;c.height=96;const ctx=c.getContext('2d');const side=Math.min(im.width,im.height);ctx.drawImage(im,(im.width-side)/2,(im.height-side)/2,side,side,0,0,96,96);avatarData=c.toDataURL('image/jpeg',.72);$('avatar').src=avatarData;saveProfile()};im.src=reader.result};reader.readAsDataURL(f)};
function render(items){if(!Array.isArray(items))return;const box=$('messages');if(items.length===lastCount)return;lastCount=items.length;box.replaceChildren();if(!items.length){const d=document.createElement('div');d.className='empty';d.textContent='👋 Добро пожаловать в PocketChat — напиши первое сообщение!';box.append(d);return}for(const m of items){const row=document.createElement('div');row.className='bubble'+(m.nick===($('nick').value.trim())?' mine':'');const img=document.createElement('img');img.className='mini';img.alt='';safeAvatar(img,m.avatar,m.nick);const content=document.createElement('div');content.className='content';const who=document.createElement('div');who.className='who';who.textContent=m.nick||'Гость';const body=document.createElement('div');body.className='body';body.textContent=m.text||'';const time=document.createElement('div');time.className='time';time.textContent=m.time||'';content.append(who,body,time);row.append(img,content);box.append(row)}box.scrollTop=box.scrollHeight}
async function refresh(){try{const data=await api('/api/messages');render(data.messages||[]);setState('В сети')}catch(e){setState('Нет связи')}}
$('composer').addEventListener('submit',async e=>{e.preventDefault();const text=$('message').value.trim();const nick=$('nick').value.trim();if(!nick){alert('Сначала введи ник и нажми «Сохранить»');$('nick').focus();return}if(!text||sending)return;sending=true;try{await api('/api/message',{method:'POST',body:JSON.stringify({nick,text})});$('message').value='';await refresh()}catch(e){alert('Сообщение не отправлено: сервер недоступен')}finally{sending=false}});
refresh();setInterval(refresh,1800);
</script></body></html>
)POCKETCHATHTML";

String readFile(const char *path) {
  File f = LittleFS.open(path, "r");
  if (!f) return String();
  String s = f.readString(); f.close(); return s;
}
void writeJson(const char *path, JsonDocument &doc) {
  File f = LittleFS.open(path, "w");
  if (f) { serializeJson(doc, f); f.close(); }
}
void drawScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("POCKETCHAT", 8, 8, 4);
  tft.drawFastHLine(8, 40, 304, TFT_DARKGREY);
  tft.drawString("Wi-Fi: ON", 8, 49, 2);
  tft.drawString(String("SSID: ") + AP_SSID, 8, 72, 2);
  tft.drawString(String("PASS: ") + AP_PASSWORD, 8, 95, 2);
  tft.drawFastHLine(8, 121, 304, TFT_DARKGREY);
  tft.drawString("SITE:", 8, 132, 2);
  tft.drawString("http://192.168.4.1", 66, 132, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Hold TOP 3s: sleep", 8, 153, 1);
}
void handleMessages() {
  JsonDocument out;
  String raw = readFile("/messages.json");
  JsonDocument data;
  if (raw.length()) deserializeJson(data, raw);
  if (!data.is<JsonArray>()) data.to<JsonArray>();
  out["messages"] = data.as<JsonArray>();
  String response; serializeJson(out, response);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", response);
}
void handleNewMessage() {
  JsonDocument req;
  if (deserializeJson(req, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"bad_json\"}"); return; }
  String nick = req["nick"] | ""; String msg = req["text"] | "";
  nick.trim(); msg.trim();
  if (!nick.length() || !msg.length() || nick.length() > 24 || msg.length() > 500) { server.send(400, "application/json", "{\"error\":\"invalid\"}"); return; }
  JsonDocument doc;
  String raw = readFile("/messages.json");
  if (raw.length()) deserializeJson(doc, raw);
  JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();
  while (arr.size() >= 80) arr.remove(0);
  JsonObject item = arr.add<JsonObject>(); item["nick"] = nick; item["text"] = msg;
  char timebuf[16]; snprintf(timebuf, sizeof(timebuf), "%lu", millis()/1000); item["time"] = String("+") + timebuf + "s";
  // Add saved avatar, if the profile exists.
  JsonDocument users; String uraw = readFile("/users.json"); if (uraw.length()) deserializeJson(users, uraw);
  if (users.is<JsonArray>()) for (JsonObject u : users.as<JsonArray>()) if (String((const char*)(u["nick"] | "")) == nick) { item["avatar"] = u["avatar"] | ""; break; }
  writeJson("/messages.json", doc);
  server.send(200, "application/json", "{\"ok\":true}");
}
void handleProfile() {
  JsonDocument req;
  if (deserializeJson(req, server.arg("plain"))) { server.send(400, "application/json", "{\"error\":\"bad_json\"}"); return; }
  String nick = req["nick"] | ""; String avatar = req["avatar"] | ""; nick.trim();
  if (!nick.length() || nick.length() > 24 || avatar.length() > 120000) { server.send(400, "application/json", "{\"error\":\"invalid\"}"); return; }
  JsonDocument doc; String raw = readFile("/users.json"); if (raw.length()) deserializeJson(doc, raw);
  JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc.to<JsonArray>();
  JsonObject found;
  for (JsonObject u : arr) if (String((const char*)(u["nick"] | "")) == nick) { found = u; break; }
  if (found.isNull()) { if (arr.size() >= 40) { server.send(507, "application/json", "{\"error\":\"user_limit\"}"); return; } found = arr.add<JsonObject>(); }
  found["nick"] = nick; found["avatar"] = avatar;
  writeJson("/users.json", doc);
  server.send(200, "application/json", "{\"ok\":true}");
}
void startPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(localIP, gateway, subnet);
  bool ok = WiFi.softAP(AP_SSID, AP_PASSWORD, 6, false, 8);
  if (!ok) Serial.println("Wi-Fi AP start failed");
  dnsStarted = dnsServer.start(DNS_PORT, "*", localIP);
  server.on("/", HTTP_GET, [](){ server.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  server.on("/api/messages", HTTP_GET, handleMessages);
  server.on("/api/message", HTTP_POST, handleNewMessage);
  server.on("/api/profile", HTTP_POST, handleProfile);
  server.onNotFound([](){
    // Captive-portal fallback. HTTPS sites cannot be transparently redirected.
    server.sendHeader("Location", String("http://") + localIP.toString() + "/", true);
    server.send(302, "text/plain", "Open PocketChat");
  });
  server.begin();
}
void enterDeepSleep() {
  // Avoid immediate wake: wait for the physical button to be released first.
  server.stop(); dnsServer.stop(); WiFi.softAPdisconnect(true); WiFi.mode(WIFI_OFF);
  tft.fillScreen(TFT_BLACK);
  digitalWrite(DISPLAY_BACKLIGHT_PIN, LOW);
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  while (digitalRead(USER_BUTTON_PIN) == LOW) delay(20);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)USER_BUTTON_PIN, 0);
  delay(100);
  esp_deep_sleep_start();
}
void setup() {
  Serial.begin(115200);
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DISPLAY_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(DISPLAY_BACKLIGHT_PIN, HIGH);
  tft.init(); tft.setRotation(1); drawScreen();
  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  if (!LittleFS.exists("/messages.json")) { File f=LittleFS.open("/messages.json","w"); if(f){f.print("[]");f.close();} }
  if (!LittleFS.exists("/users.json")) { File f=LittleFS.open("/users.json","w"); if(f){f.print("[]");f.close();} }
  startPortal();
  Serial.printf("PocketChat ready: %s, IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}
void loop() {
  if (dnsStarted) dnsServer.processNextRequest();
  server.handleClient();
  if (digitalRead(USER_BUTTON_PIN) == LOW) {
    if (!buttonPressedAt) { buttonPressedAt = millis(); buttonHandled = false; }
    else if (!buttonHandled && millis() - buttonPressedAt >= LONG_PRESS_MS) {
      buttonHandled = true;
      enterDeepSleep();
    }
  } else { buttonPressedAt = 0; buttonHandled = false; }
  delay(2);
}

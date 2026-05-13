/*
 * ============================================
 *   ESP32 THERMAL TOUCH DETECTOR
 *   Touch = your finger warms the chip sensor
 *   No GPIO pins, no wires — pure thermal!
 * ============================================
 *
 * HOW TOUCH IS DETECTED:
 *   The ESP32's built-in temp sensor sits inside
 *   the chip itself. When you press your finger
 *   onto the chip, body heat (37°C) conducts into
 *   the chip and raises the sensor reading.
 *
 *   This code watches for a fast temperature RISE.
 *   If temp climbs more than TOUCH_RISE_THRESHOLD
 *   degrees in one second → "Touch Detected!"
 *   Once temp stops rising and starts falling back,
 *   the touch is cleared after TOUCH_HOLD_SECONDS.
 *
 * HOW TO SEE IT:
 *   1. Flash this code
 *   2. Connect to WiFi: ESP32 TempGauge  (pass: tempwatch)
 *   3. Open browser → 192.168.4.1
 *   4. Press finger firmly on the ESP32 chip (big black square)
 * ============================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ─────────────────────────────────────────────
// ✏️  TUNE THESE if detection is too sensitive
//     or not sensitive enough
// ─────────────────────────────────────────────

// How many seconds of history to look back across.
// A 6-second window means we compare "now" vs "6 seconds ago".
// Increase if you press slowly; decrease for a quicker response.
#define WINDOW_SECONDS        6

// How many °C the temp must shift across that window to count as a touch.
// A single random ±0.56 °C noise blip stays well below 1.0 over 6 seconds.
// A real finger press causing a gradual 2–5 °C drift will easily exceed it.
#define TOUCH_WINDOW_DELTA    1.0f

// How long (seconds) the touch flag stays on after the shift stops.
#define TOUCH_HOLD_SECONDS    3

const char* WIFI_NAME = "ESP32 TempGauge";
const char* WIFI_PASS = "tempwatch";

// ─────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────
WebServer server(80);

float         cachedTempC    = 0.0f;
float         prevTempC      = 0.0f;
float         lastDeltaC     = 0.0f;
// Circular buffer storing the last WINDOW_SECONDS readings
float         tempHistory[WINDOW_SECONDS];
int           histIdx        = 0;       // where to write next
bool          historyReady   = false;   // false until buffer is fully populated
bool          cachedTouch    = false;
int           touchHoldCount = 0;
unsigned long lastMs         = 0;

// ─────────────────────────────────────────────
// Temperature — already °C on modern ESP32 core
// ─────────────────────────────────────────────
float getTemperatureC() {
  return temperatureRead();
}

// ─────────────────────────────────────────────
// Thermal touch logic
// Call every second with the fresh reading.
// ─────────────────────────────────────────────
void updateTouchDetection(float currentC) {
  lastDeltaC = currentC - prevTempC;   // kept for serial display only
  prevTempC  = currentC;

  // Write current reading into circular buffer
  tempHistory[histIdx] = currentC;
  histIdx = (histIdx + 1) % WINDOW_SECONDS;
  if (histIdx == 0) historyReady = true;   // buffer has wrapped once = full

  if (!historyReady) return;   // don't judge until we have a full window

  // histIdx now points at the OLDEST value in the circular buffer
  // (the slot we're about to overwrite next second)
  float oldest     = tempHistory[histIdx];
  float windowDiff = currentC - oldest;   // how much temp changed over the window

  // Example from your data:
  //   Oldest (6s ago): 64.4 °C   Current: 62.2 °C
  //   windowDiff = -2.2 °C  →  fabsf = 2.2  ≥  1.0  →  TOUCH!
  //
  // Random noise over 6s stays within ±0.56 °C → fabsf < 1.0 → ignored.

  if (fabsf(windowDiff) >= TOUCH_WINDOW_DELTA) {
    touchHoldCount = TOUCH_HOLD_SECONDS;
    cachedTouch    = true;
  } else {
    if (touchHoldCount > 0) touchHoldCount--;
    else                    cachedTouch = false;
  }
}

// ─────────────────────────────────────────────
// Build the HTML gauge page
// ─────────────────────────────────────────────
String buildGaugePage() {
  String p = "";
  p.reserve(8192);   // pre-allocate — prevents heap fragmentation crash
  p += "<!DOCTYPE html><html><head>";
  p += "<meta charset='UTF-8'>";
  p += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  p += "<title>ESP32 Temp Gauge</title>";
  p += "<style>";
  p += "* { margin:0; padding:0; box-sizing:border-box; }";
  p += "body { background:#0e0e12; font-family:Georgia,serif;";
  p += "  display:flex; flex-direction:column; align-items:center;";
  p += "  justify-content:center; min-height:100vh; color:#eee; }";
  p += ".title { font-size:0.75em; letter-spacing:5px; color:#888;";
  p += "  text-transform:uppercase; margin-bottom:30px; }";
  p += ".gauge-wrap { position:relative; width:260px; height:160px; }";
  p += "svg { overflow:visible; }";
  p += ".needle { transition: transform 1.2s cubic-bezier(.4,2,.6,1); }";
  p += ".temp-val { text-align:center; margin-top:10px; }";
  p += ".temp-big { font-size:3.5em; color:#fff; font-weight:bold; letter-spacing:-2px; }";
  p += ".temp-c { font-size:1.2em; color:#ff6b35; vertical-align:super; }";
  p += ".temp-f { font-size:0.75em; color:#555; margin-top:4px; letter-spacing:2px; }";
  p += ".delta       { font-size:0.7em; margin-top:6px; letter-spacing:2px; }";
  p += ".delta.up    { color:#ef4444; }";
  p += ".delta.down  { color:#3b82f6; }";
  p += ".delta.flat  { color:#444; }";
  p += ".status { margin-top:20px; font-size:0.65em; letter-spacing:3px;";
  p += "  color:#333; text-transform:uppercase; }";
  p += ".dot { display:inline-block; width:6px; height:6px; background:#00cc88;";
  p += "  border-radius:50%; margin-right:6px; animation:blink 1.4s infinite; }";
  p += "@keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.2} }";
  p += ".scale { font-size:0.55em; fill:#444; }";
  p += ".touch-banner { margin-top:22px; padding:12px 32px; border-radius:32px;";
  p += "  font-size:0.75em; letter-spacing:3px; text-transform:uppercase;";
  p += "  border:1px solid #222; transition:all 0.4s ease; }";
  p += ".touch-banner.active {";
  p += "  background:#ff6b35; color:#fff; border-color:#ff6b35;";
  p += "  animation:pulse 0.8s infinite alternate; }";
  p += ".touch-banner.idle { background:transparent; color:#333; }";
  p += "@keyframes pulse {";
  p += "  from { box-shadow:0 0 10px #ff6b3555; }";
  p += "  to   { box-shadow:0 0 30px #ff6b35cc; } }";
  p += ".chip-note { margin-top:20px; font-size:0.62em; color:#2a2a3a;";
  p += "  letter-spacing:1px; text-align:center; line-height:2; padding:0 30px; }";
  p += "</style></head><body>";

  p += "<div class='title'>ESP32 Thermal Touch Sensor</div>";

  // SVG gauge — 20°C (left) to 80°C (right)
  p += "<div class='gauge-wrap'>";
  p += "<svg width='260' height='160' viewBox='0 0 260 160'>";
  p += "<path d='M 20 140 A 110 110 0 0 1 240 140'";
  p += " fill='none' stroke='#1e1e2a' stroke-width='18' stroke-linecap='round'/>";
  p += "<path d='M 20 140 A 110 110 0 0 1 75 47'";
  p += " fill='none' stroke='#3b82f6' stroke-width='18' stroke-linecap='round' opacity='0.6'/>";
  p += "<path d='M 75 47 A 110 110 0 0 1 185 47'";
  p += " fill='none' stroke='#f59e0b' stroke-width='18' opacity='0.6'/>";
  p += "<path d='M 185 47 A 110 110 0 0 1 240 140'";
  p += " fill='none' stroke='#ef4444' stroke-width='18' stroke-linecap='round' opacity='0.6'/>";
  p += "<text x='14'  y='158' class='scale'>20&#xB0;</text>";
  p += "<text x='108' y='25'  class='scale' text-anchor='middle'>50&#xB0;</text>";
  p += "<text x='230' y='158' class='scale'>80&#xB0;</text>";
  p += "<g id='needle' class='needle' style='transform-origin:130px 140px;transform:rotate(-90deg)'>";
  p += "<line x1='130' y1='140' x2='130' y2='42' stroke='#ff6b35' stroke-width='3' stroke-linecap='round'/>";
  p += "<circle cx='130' cy='140' r='8' fill='#ff6b35'/>";
  p += "<circle cx='130' cy='140' r='4' fill='#0e0e12'/>";
  p += "</g>";
  p += "</svg>";
  p += "</div>";

  p += "<div class='temp-val'>";
  p += "<span class='temp-big' id='tempC'>--</span>";
  p += "<span class='temp-c'>&#xB0;C</span>";
  p += "<div class='temp-f'  id='tempF'>-- &#xB0;F</div>";
  p += "<div class='delta flat' id='deltaLbl'>&#x2508; stable</div>";
  p += "</div>";

  p += "<div class='status'><span class='dot'></span>Live &middot; updates every second</div>";

  p += "<div class='touch-banner idle' id='touchBanner'>&#x1F321; Touch idle</div>";

  p += "<div class='chip-note'>";
  p += "Press finger onto the ESP32 chip.<br>";
  p += "Body heat (37&#xB0;C) raises the sensor &rarr; touch detected.";
  p += "</div>";

  // JavaScript — polls /temp every second
  p += "<script>";
  p += "var prevC = null;";
  p += "function update() {";
  p += "  fetch('/temp').then(r => r.json()).then(d => {";

  // Temp display
  p += "    document.getElementById('tempC').textContent = d.c.toFixed(1);";
  p += "    document.getElementById('tempF').textContent = d.f.toFixed(1) + ' \u00B0F';";

  // Needle
  p += "    var pct = Math.min(Math.max((d.c - 20) / 60, 0), 1);";
  p += "    document.getElementById('needle').style.transform = 'rotate(' + (-90 + pct * 180) + 'deg)';";

  // Delta label (JS-side, for smooth display)
  p += "    var lbl = document.getElementById('deltaLbl');";
  p += "    if (prevC !== null) {";
  p += "      var diff = d.c - prevC;";
  p += "      if (diff >= 0.2) {";
  p += "        lbl.innerHTML = '\u25B2 +' + diff.toFixed(2) + '\u00B0C \u2014 rising';";
  p += "        lbl.className = 'delta up';";
  p += "      } else if (diff <= -0.2) {";
  p += "        lbl.innerHTML = '\u25BC ' + diff.toFixed(2) + '\u00B0C \u2014 falling';";
  p += "        lbl.className = 'delta down';";
  p += "      } else {";
  p += "        lbl.innerHTML = '\u2508 stable';";
  p += "        lbl.className = 'delta flat';";
  p += "      }";
  p += "    }";
  p += "    prevC = d.c;";

  // Touch banner
  p += "    var banner = document.getElementById('touchBanner');";
  p += "    if (d.touch) {";
  p += "      banner.textContent = '🔥 TOUCH DETECTED \u2014 finger on chip!';";
  p += "      banner.className = 'touch-banner active';";
  p += "    } else {";
  p += "      banner.textContent = '🌡 Touch idle';";
  p += "      banner.className = 'touch-banner idle';";
  p += "    }";
  p += "  }).catch(() => {});";
  p += "}";
  p += "setInterval(update, 1000);";
  p += "update();";
  p += "</script>";

  p += "</body></html>";
  return p;
}

// ─────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Thermal Touch Detector ===");
  Serial.println("Press finger firmly on the ESP32 chip to trigger touch.");
  Serial.print("Window: "); Serial.print(WINDOW_SECONDS);
  Serial.print("s  |  Trigger: "); Serial.print(TOUCH_WINDOW_DELTA, 1);
  Serial.println(" C shift across window");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_NAME, WIFI_PASS);
  Serial.print("WiFi: \"");
  Serial.print(WIFI_NAME);
  Serial.print("\"  →  http://");
  Serial.println(WiFi.softAPIP());

  // Fill history buffer with real first reading so window starts clean
  float initTemp = getTemperatureC();
  for (int i = 0; i < WINDOW_SECONDS; i++) tempHistory[i] = initTemp;
  prevTempC   = initTemp;
  historyReady = true;   // buffer is pre-filled, no need to wait

  server.on("/", []() {
    server.send(200, "text/html", buildGaugePage());
  });

  server.on("/temp", []() {
    String json = "{\"c\":"      + String(cachedTempC, 1)
                + ",\"f\":"      + String((cachedTempC * 9.0f / 5.0f) + 32.0f, 1)
                + ",\"delta\":"  + String(lastDeltaC, 2)
                + ",\"touch\":"  + (cachedTouch ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  Serial.println("Server ready.\n");
  Serial.println("  Temp(C)  Temp(F)   Delta/s   Touch");
  Serial.println("  -------  -------   -------   -----");
}

// ─────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastMs >= 1000) {
    lastMs = now;

    cachedTempC = getTemperatureC();
    updateTouchDetection(cachedTempC);   // updates prevTempC and lastDeltaC

    float tempF = (cachedTempC * 9.0f / 5.0f) + 32.0f;

    // Neat columnar serial output
    Serial.printf("  %6.1f C  %6.1f F   %+.2f C/s   %s\n",
      cachedTempC, tempF, lastDeltaC,
      cachedTouch ? "*** TOUCH ***" : "idle");
  }
}

/*
 * ============================================================
 *  ESP32-S3 Gateway Diagnostic  v2.0
 *  WiFi Access-Point  +  Web GUI  +  Module Tests  +  OTA
 * ============================================================
 *
 *  HOW TO USE
 *  1. Flash this firmware via Arduino IDE or esptool.
 *  2. On your phone / laptop, connect to Wi-Fi:
 *       SSID : GatewayDiag
 *       Pass : gateway123
 *  3. Open browser → http://192.168.4.1
 *  4. Click "Run All Tests" or individual module buttons.
 *  5. OTA: drop a .bin in the OTA section → Flash Firmware.
 *
 *  REQUIRED LIBRARIES (Arduino Library Manager)
 *    • RTClib  (Adafruit)
 *  BUILT-IN (ESP32 Arduino core – no install needed)
 *    • WiFi, WebServer, Update, SPI, Wire
 * ============================================================
 */

#include "RTClib.h"
#include "web_ui.h"        // PROGMEM HTML dashboard
#include "winbond_flash.h" // SPI flash stub / real lib
#include <Arduino.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>

// ============================================================
//  BOARD PIN MAP  (edit to match your hardware)
// ============================================================
#define MUX_A0 36 // MUX select bit 0
#define MUX_A1 37 // MUX select bit 1

// RS232 port  (MUX_A0=HIGH, MUX_A1=LOW → Serial2)
#define RS232_RX 15
#define RS232_TX 14

// RS485 port  (MUX_A0=LOW,  MUX_A1=LOW → Serial2)
#define RS485_RX 18
#define RS485_TX 17

// GPRS / LTE modem  (Serial1)
#define GPRS_RX 1
#define GPRS_TX 2
#define GSM_EN_PIN 21
#define GSM_PWRKEY_PIN 5

// I2C  (-1 = use board default SDA/SCL)
#define I2C_SDA -1
#define I2C_SCL -1

// Digital Inputs  (change to match your board)
#define DI_COUNT 4
const uint8_t DI_PINS[DI_COUNT] = {38, 39, 40, 41};

// ============================================================
//  MODBUS / SERIAL CONFIG
// ============================================================
#define MODBUS_BAUD 9600
#define MODBUS_CFG SERIAL_8N1
#define MODBUS_TIMEOUT_MS 1200
#define MODBUS_GAP_MS 40
#define MODBUS_FC03 0x03 // Read Holding Registers

// FR-meter slave
#define FR_SLAVE_ID 1
#define FR_START_REG 0
#define FR_REG_COUNT 2

// GPRS modem
#define GPRS_BAUD_RATE 115200
#define GPRS_AT_TIMEOUT_MS 2000

// ============================================================
//  WI-FI ACCESS POINT
// ============================================================\

#define AP_SSID "Esp32_Channel_Network's"
#define AP_PASS "esp32"

// ============================================================
//  TEST IDs
// ============================================================
enum TestID : int {
  T_RS232 = 0,
  T_RS485 = 1,
  T_GPRS = 2,
  T_DI = 3,
  T_PSRAM = 4,
  T_RTC = 5,
  T_WINBOND = 6,
  T_COUNT = 7
};

enum TestStatus : uint8_t { S_PENDING, S_PASS, S_WARN, S_FAIL, S_SKIP };

struct TestResult {
  const char *name;
  TestStatus status;
  String detail;
};

// ============================================================
//  GLOBALS
// ============================================================
RTC_DS1307 rtc;
WebServer server(80);

TestResult results[T_COUNT] = {
    {"RS232", S_PENDING, "Not tested"},   {"RS485", S_PENDING, "Not tested"},
    {"GPRS", S_PENDING, "Not tested"},    {"DI", S_PENDING, "Not tested"},
    {"PSRAM", S_PENDING, "Not tested"},   {"RTC", S_PENDING, "Not tested"},
    {"Winbond", S_PENDING, "Not tested"},
};

volatile bool testRunning = false;
volatile bool pendingAll = false;
volatile int pendingTestID = -1;

// Circular log buffer (last LOG_MAX_BYTES of output)
#define LOG_MAX_BYTES 4096
String logBuf;

// ============================================================
//  LOGGING HELPERS
// ============================================================
void logAdd(const String &s) {
  Serial.print(s);
  logBuf += s;
  if (logBuf.length() > LOG_MAX_BYTES)
    logBuf = logBuf.substring(logBuf.length() - LOG_MAX_BYTES);
}
void logLn(const String &s = "") { logAdd(s + "\n"); }
void logLine() {
  logLn("------------------------------------------------------------");
}

void logFmt(const char *fmt, ...) {
  char buf[256];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof(buf), fmt, a);
  va_end(a);
  logAdd(String(buf));
}

// ============================================================
//  RESULT HELPERS
// ============================================================
const char *statusStr(TestStatus s) {
  switch (s) {
  case S_PASS:
    return "PASS";
  case S_WARN:
    return "WARN";
  case S_FAIL:
    return "FAIL";
  case S_SKIP:
    return "SKIP";
  default:
    return "PENDING";
  }
}

void setResult(TestID id, TestStatus status, const String &detail) {
  results[id].status = status;
  results[id].detail = detail;
  logFmt("[%s] %s: %s\n", statusStr(status), results[id].name, detail.c_str());
}

// ============================================================
//  SERIAL HELPERS
// ============================================================
void drain(HardwareSerial &p) {
  while (p.available())
    p.read();
}

String hexStr(const uint8_t *d, size_t n) {
  String s;
  char h[4];
  for (size_t i = 0; i < n; i++) {
    if (i)
      s += ' ';
    snprintf(h, sizeof(h), "%02X", d[i]);
    s += h;
  }
  return s;
}

// Read bytes until inter-frame gap or timeout
size_t readFrame(HardwareSerial &p, uint8_t *buf, size_t maxLen,
                 uint32_t timeoutMs, uint32_t gapMs) {
  size_t n = 0;
  bool seen = false;
  uint32_t t0 = millis();
  uint32_t tl = t0;
  while (millis() - t0 < timeoutMs) {
    while (p.available()) {
      int b = p.read();
      if (b >= 0 && n < maxLen)
        buf[n++] = (uint8_t)b;
      seen = true;
      tl = millis();
    }
    if (seen && (millis() - tl) >= gapMs)
      break;
    delay(1);
  }
  return n;
}

// ============================================================
//  MUX CONTROL
// ============================================================
void muxRS232() {
  digitalWrite(MUX_A0, HIGH);
  digitalWrite(MUX_A1, LOW);
  delay(80);
}
void muxRS485() {
  digitalWrite(MUX_A0, LOW);
  digitalWrite(MUX_A1, LOW);
  delay(80);
}

// ============================================================
//  GSM POWER-ON SEQUENCE
// ============================================================
void gsmPowerOn() {
  pinMode(GSM_EN_PIN, OUTPUT);
  pinMode(GSM_PWRKEY_PIN, OUTPUT);
  digitalWrite(GSM_EN_PIN, HIGH);
  delay(100);
  digitalWrite(GSM_PWRKEY_PIN, HIGH);
  delay(1200);
  digitalWrite(GSM_PWRKEY_PIN, LOW);
}

// ============================================================
//  MODBUS CRC-16
// ============================================================
uint16_t crc16(const uint8_t *d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int j = 0; j < 8; j++)
      c = (c & 1) ? ((c >> 1) ^ 0xA001) : (c >> 1);
  }
  return c;
}

bool crcOK(const uint8_t *f, size_t n) {
  if (n < 4)
    return false;
  uint16_t rx = f[n - 2] | ((uint16_t)f[n - 1] << 8);
  return rx == crc16(f, n - 2);
}

// ============================================================
//  TEST — RS232  (Modbus RTU via FR Meter on Serial2)
// ============================================================
void testRS232() {
  // testRunning = true;
  // setResult(T_RS232, S_PENDING, "Running…");
  // logLine();
  // logLn("RS232 · MODBUS RTU TEST");

  muxRS232();
  // Serial2.end();
  // delay(20);
  // Serial2.begin(9600, SERIAL_8N1, RS232_RX, RS232_TX);
  // Serial2.flush();
  // delay(10);

  // // Build FC03 request
  // uint8_t req[8] = {(uint8_t)FR_SLAVE_ID,
  //                   MODBUS_FC03,
  //                   highByte(FR_START_REG),
  //                   lowByte(FR_START_REG),
  //                   highByte(FR_REG_COUNT),
  //                   lowByte(FR_REG_COUNT),
  //                   0,
  //                   0};
  // uint16_t c = crc16(req, 6);
  // req[6] = lowByte(c);
  // req[7] = highByte(c);

  // logFmt("Req : %s\n", hexStr(req, 8).c_str());
  // drain(Serial2);
  // Serial2.write(req, 8);
  // Serial2.flush();

  // uint8_t resp[128];
  // size_t rn =
  //     readFrame(Serial2, resp, sizeof(resp), MODBUS_TIMEOUT_MS,
  //     MODBUS_GAP_MS);

  // if (rn == 0) {
  //   setResult(T_RS232, S_FAIL, "No response — check wiring & slave power");
  //   testRunning = false;
  //   return;
  // }
  // logFmt("Resp: %s (%u bytes)\n", hexStr(resp, rn).c_str(), (unsigned)rn);

  // // Scan for valid Modbus frame
  // for (size_t o = 0; o + 5 <= rn; o++) {
  //   if (resp[o] != (uint8_t)FR_SLAVE_ID)
  //     continue;
  //   uint8_t fc = resp[o + 1];
  //   if (fc == (MODBUS_FC03 | 0x80)) {
  //     if (crcOK(resp + o, 5))
  //       setResult(T_RS232, S_FAIL, "Exception code=" + String(resp[o + 2]));
  //     continue;
  //   }
  //   if (fc != MODBUS_FC03)
  //     continue;
  //   uint8_t bc = resp[o + 2];
  //   if (o + 5 + bc > rn || !crcOK(resp + o, 5 + bc))
  //     continue;
  //   String d;
  //   for (uint16_t i = 0; i < bc / 2; i++) {
  //     if (i)
  //       d += ", ";
  //     uint16_t v = ((uint16_t)resp[o + 3 + i * 2] << 8) | resp[o + 4 + i *
  //     2]; d += "R" + String(FR_START_REG + i) + "=" + String(v);
  //   }
  //   setResult(T_RS232, S_PASS, "slave=" + String(FR_SLAVE_ID) + " | " + d);
  //   testRunning = false;
  //   return;
  // }
  // setResult(T_RS232, S_FAIL, "Invalid / corrupt response");
  // testRunning = false;
}

// ============================================================
//  TEST — RS485  (Modbus Diagnostics loopback on Serial2)
// ============================================================
void testRS485() {
  testRunning = true;
  setResult(T_RS485, S_PENDING, "Running…");
  logLine();
  logLn("RS485 · MODBUS BUS TEST");

  muxRS485();
  Serial2.end();
  delay(20);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  delay(100);

  // FC08 sub-function 0 — "Return Query Data" (echo loopback)
  uint8_t req[8] = {0x01, 0x08, 0x00, 0x00, 0xA5, 0x37, 0, 0};
  uint16_t c = crc16(req, 6);
  req[6] = lowByte(c);
  req[7] = highByte(c);

  logFmt("Req : %s\n", hexStr(req, 8).c_str());
  drain(Serial2);
  Serial2.write(req, 8);
  Serial2.flush();

  uint8_t resp[128];
  size_t rn =
      readFrame(Serial2, resp, sizeof(resp), MODBUS_TIMEOUT_MS, MODBUS_GAP_MS);

  if (rn == 0) {
    // No device, but MUX switching was OK
    setResult(T_RS485, S_WARN, "MUX switched OK · no device responded on bus");
    testRunning = false;
    return;
  }
  logFmt("Resp: %s (%u bytes)\n", hexStr(resp, min(rn, (size_t)16)).c_str(),
         (unsigned)rn);
  setResult(T_RS485, S_PASS,
            "Got " + String(rn) + "B: " + hexStr(resp, min(rn, (size_t)8)));
  testRunning = false;
}

// ============================================================
//  TEST — GPRS / LTE  (AT commands on Serial1)
// ============================================================
static String atRead(uint32_t ms) {
  String r;
  uint32_t t0 = millis();
  bool seen = false;
  while (millis() - t0 < ms) {
    while (Serial1.available()) {
      r += (char)Serial1.read();
      seen = true;
    }
    if (seen && millis() - t0 > 150)
      break;
    delay(1);
  }
  r.trim();
  return r;
}

static String AT(const char *cmd, uint32_t ms = 1200) {
  drain(Serial1);
  Serial1.print(cmd);
  Serial1.print("\r\n");
  return atRead(ms);
}

static int parseCSQ(const String &r) {
  int s = r.indexOf("+CSQ:");
  if (s < 0)
    return -1;
  int p = s + 5;
  while (p < (int)r.length() && (r[p] < '0' || r[p] > '9'))
    p++;
  int e = p;
  while (e < (int)r.length() && r[e] >= '0' && r[e] <= '9')
    e++;
  return (e == p) ? -1 : r.substring(p, e).toInt();
}

void testGPRS() {
  testRunning = true;
  setResult(T_GPRS, S_PENDING, "Running…");
  logLine();
  logLn("GPRS / LTE · AT COMMAND TEST");

  Serial1.end();
  delay(20);
  Serial1.begin(GPRS_BAUD_RATE, SERIAL_8N1, GPRS_RX, GPRS_TX);
  delay(300);

  // Wait for modem ready
  bool alive = false;
  for (int i = 0; i < 3 && !alive; i++) {
    String r = AT("AT", 1000);
    logFmt("AT -> %s\n", r.c_str());
    if (r.indexOf("OK") >= 0)
      alive = true;
    else
      delay(400);
  }
  if (!alive) {
    setResult(T_GPRS, S_FAIL, "No AT response — check modem power/wiring");
    testRunning = false;
    return;
  }

  AT("ATE0", 500); // echo off

  String sim = AT("AT+CPIN?", GPRS_AT_TIMEOUT_MS);
  String csqR = AT("AT+CSQ", GPRS_AT_TIMEOUT_MS);
  String creg = AT("AT+CREG?", GPRS_AT_TIMEOUT_MS);
  String icc = AT("AT+ICCID", GPRS_AT_TIMEOUT_MS);

  logFmt("CPIN : %s\n", sim.c_str());
  logFmt("CSQ  : %s\n", csqR.c_str());
  logFmt("CREG : %s\n", creg.c_str());

  int csqV = parseCSQ(csqR);
  bool simOK = sim.indexOf("READY") >= 0;
  bool regOK = creg.indexOf(",1") >= 0 || creg.indexOf(",5") >= 0;
  const char *sigQ = (csqV < 0 || csqV == 99) ? "??"
                     : csqV < 10              ? "LOW"
                     : csqV < 15              ? "OK"
                     : csqV < 20              ? "GOOD"
                                              : "STRONG";

  char d[120];
  snprintf(d, sizeof(d), "SIM=%s | CSQ=%d(%ddBm,%s) | NET=%s",
           simOK ? "OK" : "FAIL", csqV,
           (csqV >= 0 && csqV != 99) ? -113 + 2 * csqV : 0, sigQ,
           regOK ? "REG" : "UNREG");

  if (!simOK)
    setResult(T_GPRS, S_FAIL, String(d));
  else if (!regOK || (csqV >= 0 && csqV < 10))
    setResult(T_GPRS, S_WARN, String(d));
  else
    setResult(T_GPRS, S_PASS, String(d));

  testRunning = false;
}

// ============================================================
//  TEST — Digital Inputs
// ============================================================
void testDI() {
  testRunning = true;
  setResult(T_DI, S_PENDING, "Running…");
  logLine();
  logLn("DIGITAL INPUT TEST");

  String d;
  for (int i = 0; i < DI_COUNT; i++) {
    pinMode(DI_PINS[i], INPUT_PULLUP);
    delay(5);
    int v = digitalRead(DI_PINS[i]);
    if (i)
      d += " | ";
    d += "DI" + String(i + 1) + "(G" + String(DI_PINS[i]) +
         ")=" + (v ? "H" : "L");
    logFmt("DI%d  GPIO%u = %s\n", i + 1, (unsigned)DI_PINS[i],
           v ? "HIGH" : "LOW");
  }
  setResult(T_DI, S_PASS, d);
  testRunning = false;
}

// ============================================================
//  TEST — PSRAM
// ============================================================
void testPSRAM() {
  testRunning = true;
  setResult(T_PSRAM, S_PENDING, "Running…");
  logLine();
  logLn("PSRAM TEST");

  if (!psramInit()) {
    setResult(T_PSRAM, S_WARN,
              "PSRAM not present or not enabled in this build");
    testRunning = false;
    return;
  }

  size_t freeMem = ESP.getFreePsram();
  logFmt("Free PSRAM: %u bytes\n", (unsigned)freeMem);

  const size_t PROBE = 2048;
  uint8_t *probe = (uint8_t *)ps_malloc(PROBE);
  if (!probe) {
    setResult(T_PSRAM, S_FAIL, "ps_malloc(" + String(PROBE) + ") failed");
    testRunning = false;
    return;
  }

  // Write pattern
  for (size_t i = 0; i < PROBE; i++)
    probe[i] = (uint8_t)(i & 0xFF);
  // Verify
  bool ok = true;
  for (size_t i = 0; i < PROBE; i++) {
    if (probe[i] != (uint8_t)(i & 0xFF)) {
      ok = false;
      break;
    }
  }
  free(probe);

  char d[72];
  snprintf(d, sizeof(d), "%u B free | W/R %s", (unsigned)freeMem,
           ok ? "PASS" : "FAIL");
  setResult(T_PSRAM, ok ? S_PASS : S_FAIL, String(d));
  testRunning = false;
}

// ============================================================
//  TEST — RTC DS1307
// ============================================================
void testRTC() {
  testRunning = true;
  setResult(T_RTC, S_PENDING, "Running…");
  logLine();
  logLn("RTC DS1307 · I2C TEST");

#if I2C_SDA >= 0 && I2C_SCL >= 0
  Wire.begin(I2C_SDA, I2C_SCL);
#else
  Wire.begin();
#endif

  // Scan I2C bus
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      logFmt("I2C device @ 0x%02X\n", a);
      found++;
    }
  }
  logFmt("I2C devices found: %u\n", (unsigned)found);

  if (!rtc.begin()) {
    setResult(T_RTC, S_FAIL,
              "DS1307 not detected | I2C devices=" + String(found));
    testRunning = false;
    return;
  }

  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    setResult(T_RTC, S_WARN, "Oscillator was stopped — set to compile time");
    testRunning = false;
    return;
  }

  DateTime now = rtc.now();
  char d[40];
  snprintf(d, sizeof(d), "%04u-%02u-%02u %02u:%02u:%02u", now.year(),
           now.month(), now.day(), now.hour(), now.minute(), now.second());
  setResult(T_RTC, S_PASS, String(d));
  testRunning = false;
}

// ============================================================
//  TEST — Winbond SPI Flash
// ============================================================
void testWinbond() {
  testRunning = true;
  setResult(T_WINBOND, S_PENDING, "Running…");
  logLine();
  logLn("WINBOND SPI FLASH TEST");

  winbondInit();
  bool ok = runWinbondTest();

  if (ok)
    setResult(T_WINBOND, S_PASS, "JEDEC=" + winbondJedecString() + " | R/W OK");
  else
    setResult(T_WINBOND, S_FAIL,
              "JEDEC=" + winbondJedecString() + " | Not Winbond / no response");

  testRunning = false;
}

// ============================================================
//  RUN ALL / DISPATCH
// ============================================================
void runAllTests() {
  logBuf = "";
  logLine();
  logLn("=== GATEWAY DIAGNOSTIC — RUN ALL TESTS ===");
  logLine();
  testRS232();
  testRS485();
  testGPRS();
  testDI();
  testPSRAM();
  testRTC();
  testWinbond();
  logLine();
  logLn("=== ALL TESTS COMPLETE ===");
  logLine();
}

void dispatchTest(int id) {
  switch (id) {
  case T_RS232:
    testRS232();
    break;
  case T_RS485:
    testRS485();
    break;
  case T_GPRS:
    testGPRS();
    break;
  case T_DI:
    testDI();
    break;
  case T_PSRAM:
    testPSRAM();
    break;
  case T_RTC:
    testRTC();
    break;
  case T_WINBOND:
    testWinbond();
    break;
  default:
    break;
  }
}

// ============================================================
//  JSON BUILDER  (for /results endpoint)
// ============================================================
String buildJSON() {
  String j = "{\"running\":";
  j += testRunning ? "true" : "false";
  j += ",\"tests\":[";
  for (int i = 0; i < T_COUNT; i++) {
    if (i)
      j += ",";
    String d = results[i].detail;
    d.replace("\\", "\\\\");
    d.replace("\"", "\\\"");
    d.replace("\n", " ");
    d.replace("\r", "");
    j += "{\"name\":\"" + String(results[i].name) +
         "\","
         "\"status\":\"" +
         String(statusStr(results[i].status)) +
         "\","
         "\"detail\":\"" +
         d + "\"}";
  }
  j += "]}";
  return j;
}

// ============================================================
//  ROUTE HANDLERS
// ============================================================

// GET /  → serve dashboard HTML from PROGMEM
void onRoot() { server.send_P(200, "text/html", index_html); }

// GET /results  → JSON test results
void onResults() {
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "application/json", buildJSON());
}

// GET /log  → raw diagnostic log text
void onLog() {
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "text/plain; charset=utf-8",
              logBuf.length() ? logBuf : "No log yet.");
}

// GET /run?test=<name>  → queue a test (async, returns immediately)
void onRun() {
  if (testRunning) {
    server.send(429, "application/json", "{\"error\":\"busy\"}");
    return;
  }
  String t = server.arg("test");
  t.toLowerCase();

  if (t == "all") {
    pendingAll = true;
  } else if (t == "rs232") {
    pendingTestID = T_RS232;
  } else if (t == "rs485") {
    pendingTestID = T_RS485;
  } else if (t == "gprs") {
    pendingTestID = T_GPRS;
  } else if (t == "di") {
    pendingTestID = T_DI;
  } else if (t == "psram") {
    pendingTestID = T_PSRAM;
  } else if (t == "rtc") {
    pendingTestID = T_RTC;
  } else if (t == "winbond") {
    pendingTestID = T_WINBOND;
  } else {
    server.send(400, "application/json", "{\"error\":\"unknown test\"}");
    return;
  }
  server.send(200, "application/json", "{\"status\":\"queued\"}");
}

// OTA — upload handler (called for each chunk)
void onOTAUpload() {
  HTTPUpload &u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    logFmt("OTA start: %s\n", u.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
      logLn("OTA begin error: " + String(Update.errorString()));
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (Update.write(u.buf, u.currentSize) != u.currentSize)
      logLn("OTA write error: " + String(Update.errorString()));
  } else if (u.status == UPLOAD_FILE_END) {
    if (Update.end(true))
      logFmt("OTA done: %u bytes written\n", (unsigned)u.totalSize);
    else
      logLn("OTA end error: " + String(Update.errorString()));
  }
}

// OTA — completion handler (called once after upload finishes)
void onOTADone() {
  bool ok = !Update.hasError();
  if (ok) {
    server.send(200, "text/plain", "OTA OK");
    logLn("OTA success — rebooting…");
    delay(1500);
    ESP.restart();
  } else {
    String err = Update.errorString();
    server.send(500, "text/plain", "OTA FAIL: " + err);
    logLn("OTA failed: " + err);
  }
}

// GET /info  → board/firmware info
void onInfo() {
  char buf[300];
  snprintf(buf, sizeof(buf),
           "{\"fw\":\"2.0\",\"chip\":\"ESP32-S3\","
           "\"heap\":%u,\"psram\":%u,\"clients\":%u}",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
           (unsigned)WiFi.softAPgetStationNum());
  server.send(200, "application/json", String(buf));
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  delay(1000);

  pinMode(MUX_A0, OUTPUT);
  pinMode(MUX_A1, OUTPUT);
  muxRS485(); // safe default

  Serial.begin(115200);
  delay(500);

  logLine();
  logLn("ESP32-S3 Gateway Diagnostic v2.0");
  logLn("Booting WiFi AP + Web Server…");
  logLine();

  // ── Wi-Fi Access Point ───────────────────────────────────
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  logFmt("AP  : SSID=%s  PASS=%s\n", AP_SSID, AP_PASS);
  logFmt("URL : http://%s\n", ip.toString().c_str());

  // ── Web routes ───────────────────────────────────────────
  server.on("/", HTTP_GET, onRoot);
  server.on("/results", HTTP_GET, onResults);
  server.on("/log", HTTP_GET, onLog);
  server.on("/run", HTTP_GET, onRun);
  server.on("/info", HTTP_GET, onInfo);
  server.on("/ota", HTTP_POST, onOTADone, onOTAUpload);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();

  logLn("WebServer running on port 80");
  logLine();
  logLn("Ready — open http://192.168.4.1 in your browser");
  logLine();

  // Power on GSM module in background
  gsmPowerOn();
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  server.handleClient();
  int i = 0;
  while (i <= 20) {
    muxRS485();
    i++;
  }
  // Process queued test requests (tests run in main loop, not in HTTP handler)
  if (!testRunning) {
    if (pendingAll) {
      pendingAll = false;
      runAllTests();
    } else if (pendingTestID >= 0) {
      int id = pendingTestID;
      pendingTestID = -1;
      dispatchTest(id);
    }
  }

  delay(1);
}

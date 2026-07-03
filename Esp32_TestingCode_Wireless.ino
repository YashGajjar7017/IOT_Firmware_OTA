/*
 * ============================================================
 *  ESP32-S3 Gateway Diagnostic  v3.0
 *  WiFi Access-Point  +  Web GUI  +  Module Tests  +  OTA
 * ============================================================
 *
 *  HOW TO USE
 *  1. Flash this firmware via Arduino IDE or esptool.
 *  2. On your phone / laptop, connect to Wi-Fi:
 *       SSID : Esp32_Channel_Network's
 *       Pass : esp32
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

// ── Switch inputs  (edit GPIOs to match your PCB) ────────────
// Up to 8 switches; set SW_COUNT to however many you have.
#define SW_COUNT 4
const uint8_t SW_PINS[SW_COUNT] = {42, 45, 46, 47};
const char *SW_LABELS[SW_COUNT] = {"SW1", "SW2", "SW3", "SW4"};
bool swState[SW_COUNT] = {false, false, false, false}; // last read state

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

// Continuous-send test duration (ms) – RS232 & RS485
#define CONT_TEST_MS 5000 // 5 seconds
#define CONT_INTERVAL 500 // send every 500 ms

// GPRS modem
#define GPRS_BAUD_RATE 115200
#define GPRS_AT_TIMEOUT_MS 2000

// ============================================================
//  WI-FI ACCESS POINT
// ============================================================
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
  T_FR = 7,
  T_SWITCH = 8,
  T_COUNT = 9
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
    {"Winbond", S_PENDING, "Not tested"}, {"FR", S_PENDING, "Not tested"},
    {"Switch", S_PENDING, "Not tested"},
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
//  TEST — RS232
//  Sends a known loopback probe for CONT_TEST_MS.
//  The TX byte is echoed back only when the cable is physically
//  connected (full loopback). If no echo is received → FAIL.
//  Ensures a removed cable cannot produce a false PASS.
// ============================================================
void testRS232() {
  testRunning = true;
  setResult(T_RS232, S_PENDING, "Running…");
  logLine();
  logLn("RS232 · LOOPBACK + MODBUS TEST (5 s)");

  muxRS232();
  Serial2.end();
  delay(20);
  Serial2.begin(MODBUS_BAUD, MODBUS_CFG, RS232_RX, RS232_TX);
  Serial2.flush();
  delay(10);

  // ── Phase 1: Pure loopback probe ──────────────────────────
  // Send a single unique byte; if cable is connected the UART
  // TX is wired back to RX (hardware loopback) and we get it
  // back. Without cable there is no return path → no echo.
  const uint8_t PROBE_BYTE = 0xA5;
  drain(Serial2);
  Serial2.write(PROBE_BYTE);
  Serial2.flush();

  bool cableOK = false;
  uint32_t lbEnd = millis() + 300; // 300 ms for echo
  while (millis() < lbEnd) {
    if (Serial2.available()) {
      uint8_t b = Serial2.read();
      if (b == PROBE_BYTE) {
        cableOK = true;
        break;
      }
    }
    delay(1);
  }

  if (!cableOK) {
    logLn("Loopback probe: no echo → cable not connected");
    setResult(T_RS232, S_FAIL,
              "Cable not connected — no loopback echo received");
    testRunning = false;
    return;
  }
  logLn("Loopback probe: echo OK → cable present");

  // ── Phase 2: Continuous Modbus FC03 sends for 5 s ─────────
  uint8_t req[8] = {(uint8_t)FR_SLAVE_ID,
                    MODBUS_FC03,
                    highByte(FR_START_REG),
                    lowByte(FR_START_REG),
                    highByte(FR_REG_COUNT),
                    lowByte(FR_REG_COUNT),
                    0,
                    0};
  uint16_t c = crc16(req, 6);
  req[6] = lowByte(c);
  req[7] = highByte(c);

  int attempts = 0, passes = 0;
  uint32_t deadline = millis() + CONT_TEST_MS;

  while (millis() < deadline) {
    attempts++;
    logFmt("Attempt %d  Req: %s\n", attempts, hexStr(req, 8).c_str());
    drain(Serial2);
    Serial2.write(req, 8);
    Serial2.flush();

    uint8_t resp[128];
    size_t rn = readFrame(Serial2, resp, sizeof(resp), MODBUS_TIMEOUT_MS,
                          MODBUS_GAP_MS);
    if (rn > 0) {
      logFmt("       Resp: %s (%u B)\n", hexStr(resp, rn).c_str(),
             (unsigned)rn);
      // Validate frame
      for (size_t o = 0; o + 5 <= rn; o++) {
        if (resp[o] != (uint8_t)FR_SLAVE_ID)
          continue;
        uint8_t fc = resp[o + 1];
        if (fc != MODBUS_FC03)
          continue;
        uint8_t bc = resp[o + 2];
        if (o + 5 + bc > rn || !crcOK(resp + o, 5 + bc))
          continue;
        passes++;
        break;
      }
    } else {
      logLn("       No response");
    }
    // Pace sends
    uint32_t nextSend = millis() + CONT_INTERVAL;
    while (millis() < nextSend && millis() < deadline) {
      server.handleClient();
      delay(1);
    }
  }

  if (passes > 0) {
    setResult(T_RS232, S_PASS,
              "Loopback OK | Modbus responses: " + String(passes) + "/" +
                  String(attempts));
  } else {
    setResult(T_RS232, S_WARN,
              "Cable OK (loopback echo) | No Modbus device replied (" +
                  String(attempts) + " attempts)");
  }
  testRunning = false;
}

// ============================================================
//  TEST — RS485  (continuous 5-second bus probe)
// ============================================================
void testRS485() {
  testRunning = true;
  setResult(T_RS485, S_PENDING, "Running…");
  logLine();
  logLn("RS485 · MODBUS BUS TEST (5 s continuous)");

  muxRS485();
  Serial2.end();
  delay(20);
  Serial2.begin(9600, SERIAL_8N1, RS485_RX, RS485_TX);
  delay(100);

  // FC08 sub-function 0 — "Return Query Data" (echo/diagnostic)
  uint8_t req[8] = {0x01, 0x08, 0x00, 0x00, 0xA5, 0x37, 0, 0};
  uint16_t c = crc16(req, 6);
  req[6] = lowByte(c);
  req[7] = highByte(c);

  int attempts = 0, passes = 0;
  uint32_t deadline = millis() + CONT_TEST_MS;

  while (millis() < deadline) {
    attempts++;
    logFmt("Attempt %d  Req: %s\n", attempts, hexStr(req, 8).c_str());
    drain(Serial2);
    Serial2.write(req, 8);
    Serial2.flush();

    uint8_t resp[128];
    size_t rn = readFrame(Serial2, resp, sizeof(resp), MODBUS_TIMEOUT_MS,
                          MODBUS_GAP_MS);
    if (rn > 0) {
      logFmt("       Resp: %s (%u B)\n",
             hexStr(resp, min(rn, (size_t)16)).c_str(), (unsigned)rn);
      passes++;
    } else {
      logLn("       No response");
    }

    // Pace sends
    uint32_t nextSend = millis() + CONT_INTERVAL;
    while (millis() < nextSend && millis() < deadline) {
      server.handleClient();
      delay(1);
    }
  }

  if (passes > 0) {
    setResult(T_RS485, S_PASS,
              "MUX OK | Device responded " + String(passes) + "/" +
                  String(attempts) + " times");
  } else {
    setResult(T_RS485, S_WARN,
              "MUX switched OK · no device responded on bus (" +
                  String(attempts) + " attempts)");
  }
  testRunning = false;
}

// ============================================================
//  TEST — GPRS / LTE  (AT commands on Serial1)
//  PASS criteria: modem replies OK to AT  (cable/power present)
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

  // ── Step 1: Basic AT handshake ─────────────────────────────
  // If ANY attempt gets OK → modem is alive → PASS base
  bool alive = false;
  String atResp;
  for (int i = 0; i < 3 && !alive; i++) {
    atResp = AT("AT", 1000);
    logFmt("AT[%d] -> %s\n", i + 1, atResp.c_str());
    if (atResp.indexOf("OK") >= 0) {
      alive = true;
      break;
    }
    delay(400);
  }

  if (!alive) {
    setResult(T_GPRS, S_FAIL,
              "No AT response — check modem power/wiring | "
              "Baud=" +
                  String(GPRS_BAUD_RATE));
    testRunning = false;
    return;
  }

  // Modem responded — already qualifies as PASS
  // Gather extra detail (best-effort, failures don't change status)
  AT("ATE0", 500); // echo off

  String simR = AT("AT+CPIN?", GPRS_AT_TIMEOUT_MS);
  String csqR = AT("AT+CSQ", GPRS_AT_TIMEOUT_MS);
  String cregR = AT("AT+CREG?", GPRS_AT_TIMEOUT_MS);
  String cgmiR = AT("AT+CGMI", GPRS_AT_TIMEOUT_MS); // manufacturer
  String cgsn = AT("AT+CGSN", GPRS_AT_TIMEOUT_MS);  // IMEI

  logFmt("CPIN : %s\n", simR.c_str());
  logFmt("CSQ  : %s\n", csqR.c_str());
  logFmt("CREG : %s\n", cregR.c_str());
  logFmt("CGMI : %s\n", cgmiR.c_str());

  int csqV = parseCSQ(csqR);
  bool simOK = simR.indexOf("READY") >= 0;
  bool regOK = cregR.indexOf(",1") >= 0 || cregR.indexOf(",5") >= 0;

  const char *sigQ = (csqV < 0 || csqV == 99) ? "??"
                     : csqV < 10              ? "LOW"
                     : csqV < 15              ? "OK"
                     : csqV < 20              ? "GOOD"
                                              : "STRONG";

  // Build detail string
  String detail = "Modem:OK";
  if (simOK)
    detail += " | SIM:READY";
  else
    detail += " | SIM:NOT-READY";
  if (csqV >= 0 && csqV != 99)
    detail += " | CSQ:" + String(csqV) + "(" + String(-113 + 2 * csqV) +
              "dBm," + sigQ + ")";
  else
    detail += " | CSQ:N/A";
  detail += regOK ? " | NET:REG" : " | NET:UNREG";

  // PASS as long as modem responded (simOK and regOK are bonuses)
  setResult(T_GPRS, S_PASS, detail);
  testRunning = false;
}

// ============================================================
//  TEST — FR Meter  (Modbus RTU on RS232 / Serial2)
//  Dedicated section separate from raw RS232 loopback test.
// ============================================================
void testFR() {
  testRunning = true;
  setResult(T_FR, S_PENDING, "Running…");
  logLine();
  logLn("FR METER · MODBUS RTU TEST");

  muxRS232();
  Serial2.end();
  delay(20);
  Serial2.begin(MODBUS_BAUD, MODBUS_CFG, RS232_RX, RS232_TX);
  Serial2.flush();
  delay(10);

  uint8_t req[8] = {(uint8_t)FR_SLAVE_ID,
                    MODBUS_FC03,
                    highByte(FR_START_REG),
                    lowByte(FR_START_REG),
                    highByte(FR_REG_COUNT),
                    lowByte(FR_REG_COUNT),
                    0,
                    0};
  uint16_t c = crc16(req, 6);
  req[6] = lowByte(c);
  req[7] = highByte(c);

  logFmt("Slave ID: %d  Regs: %d..%d\n", FR_SLAVE_ID, FR_START_REG,
         FR_START_REG + FR_REG_COUNT - 1);
  logFmt("Req : %s\n", hexStr(req, 8).c_str());
  drain(Serial2);
  Serial2.write(req, 8);
  Serial2.flush();

  uint8_t resp[128];
  size_t rn =
      readFrame(Serial2, resp, sizeof(resp), MODBUS_TIMEOUT_MS, MODBUS_GAP_MS);

  if (rn == 0) {
    setResult(T_FR, S_FAIL, "No response — check wiring & FR slave power");
    testRunning = false;
    return;
  }
  logFmt("Resp: %s (%u bytes)\n", hexStr(resp, rn).c_str(), (unsigned)rn);

  for (size_t o = 0; o + 5 <= rn; o++) {
    if (resp[o] != (uint8_t)FR_SLAVE_ID)
      continue;
    uint8_t fc = resp[o + 1];
    if (fc == (MODBUS_FC03 | 0x80)) {
      if (crcOK(resp + o, 5))
        setResult(T_FR, S_FAIL, "Exception code=" + String(resp[o + 2]));
      continue;
    }
    if (fc != MODBUS_FC03)
      continue;
    uint8_t bc = resp[o + 2];
    if (o + 5 + bc > rn || !crcOK(resp + o, 5 + bc))
      continue;
    String d;
    for (uint16_t i = 0; i < bc / 2; i++) {
      if (i)
        d += ", ";
      uint16_t v = ((uint16_t)resp[o + 3 + i * 2] << 8) | resp[o + 4 + i * 2];
      d += "R" + String(FR_START_REG + i) + "=" + String(v);
    }
    setResult(T_FR, S_PASS, "slave=" + String(FR_SLAVE_ID) + " | " + d);
    testRunning = false;
    return;
  }
  setResult(T_FR, S_FAIL, "Invalid / corrupt response");
  testRunning = false;
}

// ============================================================
//  TEST — Switch Inputs
//  Reads each SW_PIN with INPUT_PULLUP, reports ON/OFF state.
//  PASS = at least one switch detected (pin LOW = closed).
//  WARN = all switches open (pullup HIGH) — normal if nothing pressed.
// ============================================================
void testSwitch() {
  testRunning = true;
  setResult(T_SWITCH, S_PENDING, "Reading…");
  logLine();
  logLn("SWITCH INPUT TEST");

  String detail;
  int closed = 0;
  for (int i = 0; i < SW_COUNT; i++) {
    pinMode(SW_PINS[i], INPUT_PULLUP);
    delay(5);
    int v = digitalRead(SW_PINS[i]);
    swState[i] = (v == LOW); // LOW = switch closed/ON
    if (i)
      detail += " | ";
    detail += String(SW_LABELS[i]) + "(G" + String(SW_PINS[i]) +
              ")=" + (swState[i] ? "ON" : "OFF");
    logFmt("%s  GPIO%u = %s\n", SW_LABELS[i], (unsigned)SW_PINS[i],
           swState[i] ? "ON (closed)" : "OFF (open)");
    if (swState[i])
      closed++;
  }

  if (closed > 0)
    setResult(T_SWITCH, S_PASS, detail);
  else
    setResult(T_SWITCH, S_WARN, detail + " · all open");

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

  for (size_t i = 0; i < PROBE; i++)
    probe[i] = (uint8_t)(i & 0xFF);
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
  testSwitch();
  testRS232();
  testRS485();
  testGPRS();
  testDI();
  testPSRAM();
  testRTC();
  testWinbond();
  testFR();
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
  case T_FR:
    testFR();
    break;
  case T_SWITCH:
    testSwitch();
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

void onRoot() { server.send_P(200, "text/html", index_html); }

void onResults() {
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "application/json", buildJSON());
}

void onLog() {
  server.sendHeader("Cache-Control", "no-cache, no-store");
  server.send(200, "text/plain; charset=utf-8",
              logBuf.length() ? logBuf : "No log yet.");
}

void onRun() {
  if (testRunning) {
    server.send(429, "application/json", "{\"error\":\"busy\"}");
    return;
  }
  String t = server.arg("test");
  t.toLowerCase();

  if (t == "all")
    pendingAll = true;
  else if (t == "rs232")
    pendingTestID = T_RS232;
  else if (t == "rs485")
    pendingTestID = T_RS485;
  else if (t == "gprs")
    pendingTestID = T_GPRS;
  else if (t == "di")
    pendingTestID = T_DI;
  else if (t == "psram")
    pendingTestID = T_PSRAM;
  else if (t == "rtc")
    pendingTestID = T_RTC;
  else if (t == "winbond")
    pendingTestID = T_WINBOND;
  else if (t == "fr")
    pendingTestID = T_FR;
  else if (t == "switch")
    pendingTestID = T_SWITCH;
  else {
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

// OTA — completion handler
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
           "{\"fw\":\"3.0\",\"chip\":\"ESP32-S3\","
           "\"heap\":%u,\"psram\":%u,\"clients\":%u}",
           (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram(),
           (unsigned)WiFi.softAPgetStationNum());
  server.send(200, "application/json", String(buf));
}

// GET /switch-state  → live switch GPIO readings (JSON)
// Re-reads pins every time; does NOT run the test or change results.
void onSwitchState() {
  server.sendHeader("Cache-Control", "no-cache, no-store");
  String j = "{\"switches\":[";
  for (int i = 0; i < SW_COUNT; i++) {
    pinMode(SW_PINS[i], INPUT_PULLUP);
    bool on = (digitalRead(SW_PINS[i]) == LOW);
    swState[i] = on;
    if (i)
      j += ",";
    j += "{\"label\":\"" + String(SW_LABELS[i]) +
         "\",\"gpio\":" + String(SW_PINS[i]) +
         ",\"on\":" + (on ? "true" : "false") + "}";
  }
  j += "]}";
  server.send(200, "application/json", j);
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
  logLn("ESP32-S3 Gateway Diagnostic v3.0");
  logLn("Booting WiFi AP + Web Server…");
  logLine();

  // ── Wi-Fi Access Point ─────────────────────────────────────
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  logFmt("AP  : SSID=%s  PASS=%s\n", AP_SSID, AP_PASS);
  logFmt("URL : http://%s\n", ip.toString().c_str());

  // ── Web routes ─────────────────────────────────────────────
  server.on("/", HTTP_GET, onRoot);
  server.on("/results", HTTP_GET, onResults);
  server.on("/log", HTTP_GET, onLog);
  server.on("/run", HTTP_GET, onRun);
  server.on("/info", HTTP_GET, onInfo);
  server.on("/switch-state", HTTP_GET, onSwitchState);
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

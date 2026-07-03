/*
 * winbond_flash.h  –  Winbond SPI Flash helper (stub / JEDEC-ID check)
 *
 * USAGE
 *   Call winbondInit() once in setup().
 *   Call runWinbondTest() to verify the chip is alive; returns true on success.
 *
 * REPLACE THIS FILE with your real Winbond library if you have one.
 * The two public function signatures must remain the same:
 *   void winbondInit();
 *   bool runWinbondTest();
 */

#pragma once
#include <SPI.h>

// ── Pin configuration ────────────────────────────────────────────────────────
// Override before including this header if your board uses different pins.
#ifndef WINBOND_CS_PIN
  #define WINBOND_CS_PIN   10    // Chip-select (default ESP32-S3 SS)
#endif

// ── JEDEC command codes ───────────────────────────────────────────────────────
#define JEDEC_READ_ID   0x9F
#define WINBOND_MFR_ID  0xEF    // Winbond manufacturer identifier

// ── Globals ───────────────────────────────────────────────────────────────────
static uint8_t _wb_mfr  = 0;
static uint8_t _wb_dev1 = 0;
static uint8_t _wb_dev2 = 0;

// ── winbondInit ───────────────────────────────────────────────────────────────
void winbondInit() {
  pinMode(WINBOND_CS_PIN, OUTPUT);
  digitalWrite(WINBOND_CS_PIN, HIGH);
  SPI.begin();  // uses board default MOSI/MISO/SCK
  delay(10);
}

// ── runWinbondTest ────────────────────────────────────────────────────────────
// Reads the 3-byte JEDEC ID.  Returns true if the manufacturer byte == 0xEF
// (Winbond).  Also stashes the raw bytes so the caller can log them.
bool runWinbondTest() {
  digitalWrite(WINBOND_CS_PIN, LOW);
  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(JEDEC_READ_ID);
  _wb_mfr  = SPI.transfer(0x00);
  _wb_dev1 = SPI.transfer(0x00);
  _wb_dev2 = SPI.transfer(0x00);
  SPI.endTransaction();
  digitalWrite(WINBOND_CS_PIN, HIGH);

  Serial.printf("[Winbond] JEDEC: MFR=0x%02X  DEV=0x%02X%02X\n",
                _wb_mfr, _wb_dev1, _wb_dev2);

  /*
   * Common Winbond IDs (all have MFR=0xEF):
   *   W25Q16  → EF 40 15
   *   W25Q32  → EF 40 16
   *   W25Q64  → EF 40 17
   *   W25Q128 → EF 40 18
   *   W25Q256 → EF 40 19
   */
  return (_wb_mfr == WINBOND_MFR_ID);
}

// ── winbondJedecString ────────────────────────────────────────────────────────
// Returns "0xEF4017" style string for display after runWinbondTest() was called.
String winbondJedecString() {
  char buf[20];
  snprintf(buf, sizeof(buf), "0x%02X%02X%02X", _wb_mfr, _wb_dev1, _wb_dev2);
  return String(buf);
}

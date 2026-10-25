// Pulzusindukcios betonvas-kereso - kozos beallitasok es labkiosztas
#pragma once

#include <Arduino.h>

// ---- Labkiosztas ----
// D4  = gomb: nullazas / DD beallito   A4/A5 = OLED I2C
// D5  = gomb: kalibralas               D11 = csipogo (OC2A, BC547-en keresztul)
// D2  = IRLZ44N gate (47R soros)       A1 = akku osztó (100k/33k)
// A0  = jel (U1D kimenet, x100)        A2 = U1A kimenet (x10), tartalek
// A3  = nyers Rx jel (R15 10k-n at), DD beallito mod (csatolas-meres)
static const uint8_t PIN_BTN_ZERO = 4;
static const uint8_t PIN_BTN_CAL = 5;
#define GATE_BIT PD2

// ---- Idozites (us) - oszcilloszkoppal finomhangolando ----
static const uint16_t T_ON = 60;         // gate bekapcsolt ido
static const uint16_t T_PERIOD = 1000;   // impulzus periodus (1 kHz)
static const uint16_t T_SAMPLE1 = 60;    // 1. mintavetel a kikapcsolas utan
static const uint16_t T_SAMPLE2 = 600;   // referencia mintavetel (alapvonal)
static const uint16_t T_COUPLE = 40;     // DD beallitas: mintavetel a bekapcsolas utan
static const uint16_t N_PULSES = 100;    // impulzus / meres (kb. 100 ms)
static const uint16_t TICKS_PER_US = 2;  // Timer1: 16 MHz / 8

// ---- Erzekelesi / kijelzesi parameterek (ADC egyseg, 0..1023) ----
static const float DETECT_TH = 1.5f;      // ennel nagyobb jel = talalat
static const float FULL_SCALE = 60.0f;    // kalibralatlan oszlop teljes kitereses
static const float SETUP_FULL = 500.0f;   // DD beallito mod: log skala teljes kitereses
static const float SETUP_GAIN_RATIO = 10.0f;  // A0 (x100) / A2 (U1A kimenet, x10)
static const float CAL_DEPTH_CM = 5.0f;   // kalibralasi melyseg
static const float BATT_DIV = (100.0f + 33.0f) / 33.0f;
static const uint16_t ZERO_UPDATES = 15;  // inditasi nullazas hossza (x ~140 ms)
static const uint32_t LONG_PRESS_MS = 1000;

// A nullpont lassu kovetese a kuszob alatti jelnel (ha kisebb, lassabban kovet)
static const float DRIFT_FOLLOW = 0.04f;

// Simitas (EMA) sulya: normal / DD beallito mod; ennel nagyobb ugrasnal ujrainditja
static const float SMOOTH_RUN = 0.3f;
static const float SMOOTH_SETUP = 0.1f;
static const float SMOOTH_JUMP = 60.0f;

// Telites / DC hiba hatarok (ADC egyseg)
static const float OVERLOAD_HIGH = 950.0f;
static const float OVERLOAD_LOW = 70.0f;
static const uint16_t IDLE_HIGH = 900;
static const uint16_t IDLE_LOW = 120;

#define USE_WATCHDOG 1

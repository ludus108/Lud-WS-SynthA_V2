#pragma once

// =========================================================================
// synthConfig.h — Configurazione per chip e chiavi LWS del SynthA
// =========================================================================
// Contiene:
//   - macro MCU_ID / NUM_VOCI / VOCE_BASE / porte fisiche
//   - parametri fisici (PWM, baud, pin)
//   - chiavi dei comandi LWS (K_*)
//   - estensione di sicurezza dei CMD se serial_protocol.h non aggiornato
//   - costanti del motore (PIx2, NOTE_STACK_MAX, ecc.)
//
// NOTA: MCU_ID va definito PRIMA dell'include di questo file (oppure viene
// default a 'a'). In pratica: nel .ino mettere
//     #define MCU_ID 'a'
//     #include "synthConfig.h"
// =========================================================================

// -------------------------------------------------------------------------
// 1. IDENTITÀ DEL CHIP
// -------------------------------------------------------------------------
#ifndef MCU_ID
  #define MCU_ID 'a'      // fallback di sicurezza
#endif

#if   MCU_ID == 'a'
  #define NUM_VOCI      1   // SynthA_M:  1 voce locale
  #define VOCE_BASE     0   //   voice globali 0..0
#elif MCU_ID == 'b'
  #define NUM_VOCI      2   // SynthA_V_A: 2 voci locali
  #define VOCE_BASE     1   //   voice globali 1..2
#elif MCU_ID == 'c'
  #define NUM_VOCI      2   // SynthA_V_B: 2 voci locali
  #define VOCE_BASE     3   //   voice globali 3..4
#else
  #error "MCU_ID non riconosciuto (usa 'a', 'b' o 'c')"
#endif

#define SUBVOCI_PER_VOCE  2
#define TOTAL_OSC         (NUM_VOCI * SUBVOCI_PER_VOCE)

// -------------------------------------------------------------------------
// 2. PORTE FISICHE LWS
// -------------------------------------------------------------------------
// Topologia a cascata:
//   Router ── SerialPIO@115200 ──▶ Master 'a'
//   Master 'a' ── Serial1@1Mbps ──▶ V_A 'b'
//   V_A 'b'   ── Serial2@1Mbps ──▶ V_B 'c'
//
// Ogni chip ha:
//   LWS_PORT_UP   : porta da cui arrivano i frame dal Router (o dal nodo
//                   precedente nella catena)
//   LWS_PORT_DOWN : porta verso il nodo successivo (definita solo se
//                   HAS_DOWNSTREAM == 1)
// -------------------------------------------------------------------------

#if MCU_ID == 'a'
  // ---- Master ----
  #include <SerialPIO.h>
  extern SerialPIO lwsPortUp;         // definito nel .ino

  #define LWS_PORT_UP    lwsPortUp
  #define LWS_PORT_DOWN  Serial1
  #define HAS_DOWNSTREAM 1
  #define LWS_BAUD_UP    115200UL
  #define LWS_BAUD_DOWN  1000000UL

#elif MCU_ID == 'b'
  // ---- V_A ----
  #define LWS_PORT_UP    Serial1
  #define LWS_PORT_DOWN  Serial2
  #define HAS_DOWNSTREAM 1
  #define LWS_BAUD_UP    1000000UL
  #define LWS_BAUD_DOWN  1000000UL

#elif MCU_ID == 'c'
  // ---- V_B ----
  #define LWS_PORT_UP    Serial1
  #define LWS_PORT_DOWN  Serial1     // dummy, non usato
  #define HAS_DOWNSTREAM 0
  #define LWS_BAUD_UP    1000000UL
  #define LWS_BAUD_DOWN  1000000UL
#endif

// -------------------------------------------------------------------------
// 3. PIN HARDWARE DEL MOTORE AUDIO
// -------------------------------------------------------------------------
#define OUTPUT_A_PIN   5    // PWM audio
#define OUTPUT_ON_PIN  4    // enable out (LOW all'avvio, HIGH dopo init)

// -------------------------------------------------------------------------
// 4. COSTANTI MOTORE
// -------------------------------------------------------------------------
#define PWM_IRQ_RATE_HZ   30000.0f  // ~frequenza IRQ con clkdiv=4, wrap=1023

#define NOTE_STACK_MAX    8         // max note in stack per voce

#define MAX_PITCH         127
#define MAX_NOTEARRAY_IDX 60        // noteArr ha 61 elementi (0..60)

// Costante matematica (non tutti i core la espongono come M_PI)
#ifndef PIx2
  #define PIx2  6.28318530718f
#endif

// -------------------------------------------------------------------------
// 5. SICUREZZA — CMD estesi (se serial_protocol.h non è ancora aggiornato)
// -------------------------------------------------------------------------
#ifndef CMD_PARAM_VOCE
  #define CMD_PARAM_VOCE   'V'
#endif
#ifndef CMD_PARAM_I32
  #define CMD_PARAM_I32    'I'
#endif
#ifndef CMD_PARAM_I32_V
  #define CMD_PARAM_I32_V  'J'
#endif
#ifndef CMD_MIDI_NOTE_V
  #define CMD_MIDI_NOTE_V  'N'
#endif
#ifndef CMD_MIDI_BEND_V
  #define CMD_MIDI_BEND_V  'M'
#endif
#ifndef CMD_MIDI_CC_V
  #define CMD_MIDI_CC_V    'K'
#endif

// -------------------------------------------------------------------------
// 6. CHIAVI LWS — per-voce uint8 (CMD_PARAM_VOCE)
// -------------------------------------------------------------------------
// Formato: [target][voice][key][value]
// -------------------------------------------------------------------------

// MODE / WAVEFORM
#define K_MODE            'm'   // 0=WF, 1=FM, 2=AM
#define K_WAVEFORM        'w'   // 0..8 → sub_voce 0
#define K_SUB1_WAVEFORM   'c'   // 0..8 → sub_voce 1
#define K_FM_SELECT       'F'   // 0..7 → fmSel_loc

// TUNING
#define K_OTTAVA          'o'   // 1..3
#define K_ATTENUA         'a'   // 0..9 (indice attenuaNumArr)
#define K_TRACKING        'k'   // 0..128
#define K_DETUNE          'd'   // 0..100
#define K_TEMPO_SLIDE     't'   // 0..99

// VCF LFO
#define K_VCF_WAVE        'v'   // 0..4
#define K_VCF_LFO_LEV     'l'   // 0..255
#define K_VCF_LFO_RATE    's'   // 0..255
#define K_VCF_LFO_SYNC    '1'   // 0/1
#define K_VCF_MULTI       '3'   // 0..12
#define K_VCF_START       '4'   // 0..99

// BENDER / MOD
#define K_BEND_UP         'u'   // 0..4
#define K_BEND_DOWN       'n'   // 0..4
#define K_SEND_BEND       'B'   // 0/1
#define K_SEND_MOD        'M'   // 0/1

// MONO / SLIDE
#define K_SLIDE           'S'   // 0/1 (glide on/off)

// LFO SYNC
#define K_LFO_MOD_SYNC    'y'   // 0/1

// DSP
#define K_DSP             'D'   // 0..15

// ARPEGGIATOR
#define K_ARP_ON          'r'
#define K_ARP_MODE        'R'
#define K_ARP_MULTI       'A'
#define K_ARP_OCTAVES     'T'

// GATER
#define K_GATER_ON        'g'
#define K_GATER_NUM       'G'
#define K_GATE_MULTI      'H'
#define K_GATE_LUNG       'L'

// OFFSET / MIDI ROUTING
#define K_CONTA_OFFSET    'O'
#define K_MIDI_CH         'C'
#define K_NOTA_SPLIT      'Z'
#define K_MIDI_MODE       'X'

// SUB_VOCE 1
#define K_SUB1_LEVEL      'e'   // 0..20 (indice levArr)

// VOLUME voce
#define K_VOICE_VOL       '9'   // 0..20 (indice levArr)

// ADSR
#define K_ADSR_A          '5'   // 0..255
#define K_ADSR_D          '6'   // 0..255
#define K_ADSR_S          '7'   // 0..255
#define K_ADSR_R          '8'   // 0..255

// -------------------------------------------------------------------------
// 7. CHIAVI LWS — per-voce int32 (CMD_PARAM_I32_V)
// -------------------------------------------------------------------------
// Formato: [target][voice][key][i32_le]
// -------------------------------------------------------------------------

#define K_MOD_IN_B        'i'   // 0..1023
#define K_MOD_LEV         'I'   // 0..1023
#define K_LFO_RATE        'f'   // 350..80000 (µs, inverso)
#define K_PITCH_LEV       'j'   // 0..550
#define K_PITCH_RATE      'J'   // 350..80000 (µs, inverso)

// FM operator parameters (agiscono su fmSetSin/Div[fmSel_loc[v]][op])
#define K_FM_SIN_0        'x'   // float*100
#define K_FM_SIN_1        'X'
#define K_FM_SIN_2        'Y'
#define K_FM_DIV_0        'q'   // int, 1..600
#define K_FM_DIV_1        'Q'
#define K_FM_DIV_2        'W'

// SUB_VOCE 1 interval (semitoni, -24..+24)
#define K_SUB1_INTERVAL   'h'

// -------------------------------------------------------------------------
// 8. CHIAVI LWS — globali chip (CMD_PARAM_I32)
// -------------------------------------------------------------------------
#define K_CALIB_SEL       'e'   // 0..60
#define K_CALIB_VAL       'E'   // freq * 10

// -------------------------------------------------------------------------
// 9. CHIAVI LWS — sistema (CMD_PARAM, solo Master)
// -------------------------------------------------------------------------
#define K_SYNTH_MODE      'Y'   // 0=Poly, 1=MultiMono
#define K_PRESET_SEL      'P'   // 0..29
#define K_PRESET_SAVE     'Q'   // 0..29
#define K_VOICE_MUTE      'U'   // 0/1
#define K_PANIC           '!'
#define K_ALL_NOTES_OFF   'O'

// -------------------------------------------------------------------------
// 10. UTILITY COMPILE-TIME
// -------------------------------------------------------------------------
// Verifica coerenza
#if (NUM_VOCI < 1) || (NUM_VOCI > 2)
  #error "NUM_VOCI deve essere 1 o 2 per SynthA"
#endif

// Macro per evitare warning su parametri non usati
#define UNUSED(x)  ((void)(x))
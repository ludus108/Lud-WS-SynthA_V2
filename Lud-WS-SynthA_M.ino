// =====================================================================
// Lud-WS-SynthA_M — RP2040 synth a 1 voce (+2 sub_voci), nodo LWS
// =====================================================================
// REV 1 (LWS integration)
//
// ARCHITETTURA
// ---------------------------------------------------------------------
//   Display ── 1 Mbps ──▶ Router ── 115200 ──▶ [a] SynthA_M (1 voce)
//                                                  │
//                                                  │ Serial1 1 Mbps
//                                                  ▼
//                                              [b] SynthA_V_A (2 voci)
//                                                  │
//                                                  │ Serial2 1 Mbps
//                                                  ▼
//                                              [c] SynthA_V_B (2 voci)
//
// VOCI:
//   'a' → voce globale 0 (VOCE_BASE=0, NUM_VOCI=1)
//   'b' → voci globali 1, 2 (VOCE_BASE=1, NUM_VOCI=2)
//   'c' → voci globali 3, 4 (VOCE_BASE=3, NUM_VOCI=2)
//
// SUB_VOCI:
//   Ogni voce ha 2 sub_voci:
//     sub_voce 0: nota base
//     sub_voce 1: nota base + interval (semitoni), con volume subLevel
//
// MODALITÀ:
//   Poly      : tutte le voci eseguono lo stesso preset.
//               Il Master broadcasta i comandi voce-specific.
//   MultiMono : ogni voce ha il proprio preset.
//               Il Display indirizza esplicitamente ogni voce.
//
// MIDI:
//   Il MIDI hardware è gestito solo dal Router.
//   I comandi MIDI arrivano qui come CMD_MIDI_*_V via LWS.
//
// COSA È STATO RIMOSSO rispetto all'originale
// ---------------------------------------------------------------------
//   - #include <MIDI.h>, MIDI_CREATE_INSTANCE, MIDI.read(), ecc.
//   - Encoder, bottoni, potenziometri, LED (pin e oggetti)
//   - Display companion legacy (Serial2 = protocollo UI)
//   - POLIMAX=6 (parafonia): sostituito da 2 sub_voci per voce
//   - forward Vermona, waldorfCC, midiMode split ecc.
//   - ctrlFuncA.h, pagineA.h, sendFuncA.h (interi)
//
// =====================================================================

// ---------------------------------------------------------------------
// 0. MCU_ID — va PRIMA di ogni altro include
// ---------------------------------------------------------------------
// Cambia questa riga (o passala come -D) per compilare i tre chip.
#ifndef MCU_ID
  #define MCU_ID 'a'          // 'a'=Master, 'b'=V_A, 'c'=V_B
#endif

// ---------------------------------------------------------------------
// 1. Include
// ---------------------------------------------------------------------
#include <Arduino.h>
#include <hardware/pwm.h>
#include <EEPROM.h>

#include "synthConfig.h"       // deps: MCU_ID
#include "ottaveA.h"           // tabelle noteArr, voctpow, bendMaxUp/DownArr

// Porta fisica per il Master (SerialPIO verso il Router)
#if MCU_ID == 'a'
  SerialPIO lwsPortUp(2, 3);   // TX=GP2, RX=GP3
#endif

#include "serial_protocol.h"   // protocollo LWS (condiviso)
#include "comunicazioni_mcu.h" // callbacks e parser LWS (condiviso)

#include "synthState.h"        // variabili globali + extern

// ---------------------------------------------------------------------
// 2. Definizione delle variabili dichiarate in synthState.h
// ---------------------------------------------------------------------
// (Vedi i file synthEngine.h / synthControl.h / synthLws.h per le funzioni.)

// --- Tabelle FM (globali al chip) ---
float fmSetSin[8][3] = {};
int   fmSetDiv[8][3] = {};

// --- Pattern gater (8 pattern x 16 step) ---
byte gaterArr[8][16] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},   // 0 = sempre on
    {1,1,1,0,1,1,0,1,0,1,1,0,1,1,1,1},   // 1
    {1,1,1,1,0,1,0,1,0,1,1,1,1,1,1,1},   // 2
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},   // 3
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},   // 4
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},   // 5
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},   // 6
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}    // 7
};

// --- Tabelle arp / VCF / attenua ---
const byte lfoVcfMultiArr[13] = {1,2,3,4,5,6,7,8,10,12,16,24,32};
const int  arpMultiArr[5]     = {128, 64, 32, 16, 8};
const int  attenuaNumArr[10]  = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};

// --- Curva volume (21 livelli da 0.0 a 1.0) ---
const float levArr[21] = {
    0.0f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f,
    0.35f, 0.40f, 0.45f, 0.50f, 0.55f, 0.60f, 0.65f,
    0.70f, 0.75f, 0.80f, 0.85f, 0.90f, 0.95f, 1.00f
};

// --- Costanti globali ---
const float masterFreq    = 4.0f;
const float f0            = 30.0f;
float       calb          = 6.58f;
const float sampleLev     = 551.0f;
float       freq_table[2048];
int         sinePitchModArr[256];
int         sineModArr[256];
float       maxModPitchLev = 300.0f;

// --- Stato PWM ---
int slice_num = 0;

// --- Oscillatori (per voce, per sub_voce) ---
float  f_loc[NUM_VOCI][SUBVOCI_PER_VOCE] = {};
float  osc_freq_loc[NUM_VOCI][SUBVOCI_PER_VOCE] = {};

// --- Sub_voce 1 ---
int16_t sub_interval[NUM_VOCI] = {};
uint8_t sub_level_idx[NUM_VOCI] = {};

// --- Waveform / Mode ---
uint8_t mode_loc[NUM_VOCI] = {};
uint8_t waveform_loc[NUM_VOCI][SUBVOCI_PER_VOCE] = {};
uint8_t fmSel_loc[NUM_VOCI] = {};

// --- Tuning ---
uint8_t ottava_loc[NUM_VOCI]    = {};
uint8_t oct_sw_loc[NUM_VOCI]    = {};
uint8_t attenua_loc[NUM_VOCI]   = {};
int     trackingLev_loc[NUM_VOCI] = {};
uint8_t detune_loc[NUM_VOCI]    = {};
float   detuneFlo_loc[NUM_VOCI] = {};

// --- Volume ---
uint8_t voiceVol_idx[NUM_VOCI] = {};

// --- Wavetable ---
int   wavetable_loc    [NUM_VOCI][SUBVOCI_PER_VOCE][256] = {};
float mod_wavetable_loc[NUM_VOCI][SUBVOCI_PER_VOCE][256] = {};
int   mod2_wavetable_loc[NUM_VOCI][SUBVOCI_PER_VOCE][256] = {};

// --- Modulazione ---
float         mod_loc[NUM_VOCI]         = {};
int           modIn_loc[NUM_VOCI]       = {};
int           modInB_loc[NUM_VOCI]      = {};
int           modLev_loc[NUM_VOCI]      = {};
int           modLevA_loc[NUM_VOCI]     = {};
unsigned long speedMod_loc[NUM_VOCI]    = {};
unsigned long prevTimeMod_loc[NUM_VOCI] = {};
int           contaMod_loc[NUM_VOCI]    = {};

// --- LFO pitch ---
int           modPitchLev_loc[NUM_VOCI]      = {};
int           modPitchLevA_loc[NUM_VOCI]     = {};
unsigned long speedPitchMod_loc[NUM_VOCI]    = {};
unsigned long prevTimePitchMod_loc[NUM_VOCI] = {};
int           contaPitchMod_loc[NUM_VOCI]    = {};

// --- Bend / mod ---
float   frBend_offset_loc[NUM_VOCI] = {};
float   frMod_offset_loc[NUM_VOCI]  = {};
uint8_t bendMaxUp_loc[NUM_VOCI]     = {};
uint8_t bendMaxDown_loc[NUM_VOCI]   = {};
uint8_t sendBend_loc[NUM_VOCI]      = {};
uint8_t sendMod_loc[NUM_VOCI]       = {};

// --- Glide ---
uint8_t       slide_loc[NUM_VOCI]        = {};
int           tempoSlide_loc[NUM_VOCI]   = {};
unsigned long speedSlide_loc[NUM_VOCI]   = {};
float         glideTarget_loc[NUM_VOCI]  = {};
float         glideCurrent_loc[NUM_VOCI] = {};
float         glideStep_loc[NUM_VOCI]    = {};

// --- VCF LFO ---
uint8_t lfoVcfLev_loc[NUM_VOCI]      = {};
uint8_t speedVcfLfo_loc[NUM_VOCI]    = {};
uint8_t vcfWave_loc[NUM_VOCI]        = {};
uint8_t lfoVcf1Syn_loc[NUM_VOCI]     = {};
uint8_t lfoVcfMulti_loc[NUM_VOCI]    = {};
uint8_t lfoVcfLev2B_loc[NUM_VOCI]    = {};
uint8_t lfoVcf2Syn_loc[NUM_VOCI]     = {};

// --- DSP ---
uint8_t dspNum_loc[NUM_VOCI] = {};

// --- MIDI routing per voce ---
uint8_t midiCh_loc[NUM_VOCI]    = {};
uint8_t notaSplit_loc[NUM_VOCI] = {};
uint8_t midiMode_loc[NUM_VOCI]  = {};

// --- LFO mod sync ---
uint8_t lfoMod2Syn_loc[NUM_VOCI] = {};
uint8_t contaOffset_loc[NUM_VOCI] = {};

// --- Note stack ---
uint8_t noteStack_loc[NUM_VOCI][NOTE_STACK_MAX] = {};
uint8_t noteCount_loc[NUM_VOCI]   = {};
uint8_t currentNote_loc[NUM_VOCI] = {};

// --- ADSR ---
uint8_t adsr_a_loc[NUM_VOCI] = {};
uint8_t adsr_d_loc[NUM_VOCI] = {};
uint8_t adsr_s_loc[NUM_VOCI] = {};
uint8_t adsr_r_loc[NUM_VOCI] = {};
uint8_t envState_loc[NUM_VOCI]     = {};
float   envPhase_loc[NUM_VOCI]     = {};
float   envLevel_loc[NUM_VOCI]     = {};
float   envRelStart_loc[NUM_VOCI]  = {};

// --- Arp ---
uint8_t arp_on_loc[NUM_VOCI]       = {};
uint8_t arp_mode_loc[NUM_VOCI]     = {};
uint8_t arp_multi_loc[NUM_VOCI]    = {};
uint8_t arp_octaves_loc[NUM_VOCI]  = {};
int8_t  arp_step_loc[NUM_VOCI]     = {};
uint8_t arp_dir_loc[NUM_VOCI]      = {};
uint8_t arp_octave_loc[NUM_VOCI]   = {};
uint8_t arp_oct_dir_loc[NUM_VOCI]  = {};

// --- Gater ---
uint8_t gater_on_loc[NUM_VOCI]     = {};
uint8_t gater_num_loc[NUM_VOCI]    = {};
uint8_t gater_multi_loc[NUM_VOCI]  = {};
uint8_t gater_step_loc[NUM_VOCI]   = {};
uint8_t gater_lung_loc[NUM_VOCI]   = {};

// --- AM ---
int      am_k_loc[NUM_VOCI]     = {};
uint32_t am_timer_loc[NUM_VOCI] = {};

// --- Sistema ---
uint8_t synthMode  = 0;   // 0=Poly, 1=MultiMono (Master only)
uint8_t presetSel  = 0;
uint8_t presetSave = 0;

// --- Interval multipliers ---
float intervalMul[49] = {};

// ---------------------------------------------------------------------
// 3. Include moduli funzionali
// ---------------------------------------------------------------------
#include "synthEngine.h"    // IRQ, wavetable, ADSR, note stack, freq
#include "synthControl.h"   // LFO tick, arp, gater per-voce
#include "synthLws.h"       // callback LWS, bridge, poly broadcast

// ---------------------------------------------------------------------
// 4. SETUP
// ---------------------------------------------------------------------
void setup() {
    // --- Debug USB ---
    Serial.begin(115200);
    delay(200);
    Serial.printf("\n=== Lud-WS-SynthA_%c ===\n", MCU_ID);
    Serial.printf("NUM_VOCI=%u  VOCE_BASE=%u\n", NUM_VOCI, VOCE_BASE);

    // --- LWS porte ---
    LWS_PORT_UP.begin(LWS_BAUD_UP);
#if HAS_DOWNSTREAM
    LWS_PORT_DOWN.begin(LWS_BAUD_DOWN);
#endif

    // --- LWS callbacks ---
    lws_set_callbacks(on_param, on_error);
    lws_set_synth_callbacks(on_param_voce, on_param_i32, on_param_i32_voce,
                            on_midi_note_v, on_midi_bend_v, on_midi_cc_v);

    // --- Motore audio ---
    pinMode(OUTPUT_ON_PIN, OUTPUT);
    digitalWrite(OUTPUT_ON_PIN, LOW);   // out OFF durante init

    EEPROM.begin(512);

    // Inizializza tabelle globali
    for (int i = 0; i < 256; i++) {
        sinePitchModArr[i] = (int)(sinf(PIx2 * i / 256.0f) * 511.0f);
        sineModArr[i]      = (int)(sinf(PIx2 * i / 256.0f) * 511.0f);
    }
    for (int i = 0; i < 1230; i++)
        freq_table[i] = f0 * powf(2.0f, voctpow[i]);
    for (int i = 1230; i < 2048; i++) freq_table[i] = 6.0f;

    init_intervalMul();      // definita in synthEngine.h

    // Inizializza stato per ogni voce locale
    for (uint8_t v = 0; v < NUM_VOCI; v++) {
        mode_loc[v]          = 0;    // wavefold
        waveform_loc[v][0]   = 0;    // SAW
        waveform_loc[v][1]   = 0;    // SAW
        fmSel_loc[v]         = 0;
        ottava_loc[v]        = 2;
        attenua_loc[v]       = 5;
        voiceVol_idx[v]      = 20;   // full
        sub_level_idx[v]     = 20;   // full
        sub_interval[v]      = 0;
        detune_loc[v]        = 0;
        detuneFlo_loc[v]     = 0.0f;
        trackingLev_loc[v]   = 0;
        voiceVol_idx[v]      = 20;
        // ADSR iniziale
        adsr_a_loc[v] = 10;
        adsr_d_loc[v] = 30;
        adsr_s_loc[v] = 200;
        adsr_r_loc[v] = 40;
        envState_loc[v] = ENV_IDLE;
        envLevel_loc[v] = 0.0f;
        // Oscillatori
        for (uint8_t s = 0; s < SUBVOCI_PER_VOCE; s++) {
            f_loc[v][s] = 0.0f;
            osc_freq_loc[v][s] = 0.0f;
        }
        // LFO default
        modLev_loc[v]    = 0;
        modLevA_loc[v]   = 0;
        speedMod_loc[v]  = 12000UL;
        modPitchLev_loc[v] = 0;
        modPitchLevA_loc[v] = 0;
        speedPitchMod_loc[v] = 500UL;

        setOttava_loc(v, ottava_loc[v]);   // synthEngine.h
        wavetable_setup_voce(v);            // synthEngine.h
    }

    // --- PWM audio ---
    gpio_set_function(OUTPUT_A_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(OUTPUT_A_PIN);
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);
    pwm_set_enabled(slice_num, true);

    // Clock PWM iniziale
    pwm_set_clkdiv(slice_num, masterFreq);
    pwm_set_wrap(slice_num, 1023);

    digitalWrite(OUTPUT_ON_PIN, HIGH);   // out ON

    Serial.println("SynthA ready.");
}

// ---------------------------------------------------------------------
// 5. LOOP
// ---------------------------------------------------------------------
void loop() {
    // --- LWS: poll delle due porte ---
    pollRouter();       // synthLws.h
    pollChain();        // synthLws.h

    // --- LWS: retry pending ACK (sulla porta UP) ---
    // NB: l'header usa LWS_SERIAL (definita come LWS_PORT_UP in config).
    // Non abbiamo bisogno di chiamarlo perché gestiamo tutto via pollRouter()
    // e il pending è gestito internamente dall'header — ma per pulizia:
    // lws_mcu_poll();

    // --- Calcolo "mod" per ciascuna voce locale ---
    for (uint8_t v = 0; v < NUM_VOCI; v++) {
        int tmpmod = modIn_loc[v] + modInB_loc[v];
        tmpmod = (tmpmod > 1023) ? 1023 : tmpmod;
        switch (mode_loc[v]) {
            case 0:  // wavefold
                if (waveform_loc[v][0] != 2)
                    mod_loc[v] = (float)tmpmod * 0.0036f + 0.90f;
                else
                    mod_loc[v] = (float)(tmpmod >> 3);
                break;
            case 1:  // FM
                mod_loc[v] = (float)(tmpmod >> 3);
                break;
            case 2:  // AM
                mod_loc[v] = (float)(1023 - tmpmod);
                break;
        }
    }

    // --- LFO tick (mod + pitch + arp + gater) ---
    lfoTick_all();      // synthControl.h

    // --- VCF LFO (per-voce) ---
    vcfTick_all();      // synthControl.h
}
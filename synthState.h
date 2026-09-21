#pragma once
#include "synthConfig.h"

// =========================================================================
// synthState.h — Stato globale del SynthA
// =========================================================================
// Contiene:
//   - tabelle costanti (noteArr, voctpow, freq_table, ecc.)
//   - tabelle FM condivise
//   - pattern gater, arp
//   - stato per-voce (tutti gli array _loc[NUM_VOCI])
//   - stack note, ADSR, glide
//
// Include synthConfig.h per NUM_VOCI e le costanti.
// =========================================================================

// -------------------------------------------------------------------------
// 1. TABELLE COSTANTI (una sola copia per chip)
// -------------------------------------------------------------------------
// Le tabelle noteArr, voctpow, bendMaxUpArr/DownArr sono definite in
// "ottaveA.h" (invariato dall'originale). Vanno incluse nel .ino PRIMA
// di synthState.h. Qui dichiariamo solo quelle che il motore usa.

extern const float voctpow[1230];
extern float noteArr[3][61];
extern const int bendMaxUpArr[5];
extern const int bendMaxDownArr[5];

// -------------------------------------------------------------------------
// 2. PARAMETRI GLOBALI CHIP (invariati dall'originale)
// -------------------------------------------------------------------------
extern const float masterFreq;      // 4.0
extern const float f0;              // 30.0 (base freq)
extern float       calb;            // 6.58 (fine tune)
extern const float sampleLev;       // 551 (ampiezza wavetable)
extern float       freq_table[2048];
extern int         sinePitchModArr[256];
extern int         sineModArr[256];
extern float       maxModPitchLev;  // 300.0

// -------------------------------------------------------------------------
// 3. TABELLE FM (globali al chip, selezionate per-voce via fmSel_loc)
// -------------------------------------------------------------------------
extern float fmSetSin[8][3];
extern int   fmSetDiv[8][3];

// -------------------------------------------------------------------------
// 4. TABELLE GATER / ARP / VCF MULTI (globali al chip)
// -------------------------------------------------------------------------
extern byte gaterArr[8][16];
extern const byte lfoVcfMultiArr[13];
extern const int  arpMultiArr[5];
extern const int  attenuaNumArr[10];

// -------------------------------------------------------------------------
// 5. CURVA VOLUME (levArr, da mem.h originale)
// -------------------------------------------------------------------------
// 21 livelli 0.0 .. 1.0
extern const float levArr[21];

// -------------------------------------------------------------------------
// 6. STATO PWM (condiviso chip)
// -------------------------------------------------------------------------
extern int slice_num;

// -------------------------------------------------------------------------
// 7. STATO PER-VOCE (locale, indicizzato 0..NUM_VOCI-1)
// -------------------------------------------------------------------------

// --- Oscillatori ---
extern float  f_loc[NUM_VOCI][SUBVOCI_PER_VOCE];        // accumulatore fase 0..255
extern float  osc_freq_loc[NUM_VOCI][SUBVOCI_PER_VOCE]; // incremento per IRQ

// --- Sub_voce 1 ---
extern int16_t sub_interval[NUM_VOCI];      // -24..+24 semitoni
extern uint8_t sub_level_idx[NUM_VOCI];     // 0..20 (indice levArr)

// --- Waveform / Mode ---
extern uint8_t mode_loc[NUM_VOCI];                       // 0=WF, 1=FM, 2=AM
extern uint8_t waveform_loc[NUM_VOCI][SUBVOCI_PER_VOCE]; // 0..8 per sub_voce
extern uint8_t fmSel_loc[NUM_VOCI];                      // 0..7

// --- Tuning ---
extern uint8_t ottava_loc[NUM_VOCI];        // 1..3
extern uint8_t oct_sw_loc[NUM_VOCI];        // 1, 2, 4 (derivato da ottava)
extern uint8_t attenua_loc[NUM_VOCI];       // 1..10 (indice attenuaNumArr)
extern int     trackingLev_loc[NUM_VOCI];
extern uint8_t detune_loc[NUM_VOCI];
extern float   detuneFlo_loc[NUM_VOCI];

// --- Volume ---
extern uint8_t voiceVol_idx[NUM_VOCI];      // 0..20 (indice levArr)

// --- Wavetable (per voce, per sub_voce) ---
extern int   wavetable_loc    [NUM_VOCI][SUBVOCI_PER_VOCE][256];
extern float mod_wavetable_loc[NUM_VOCI][SUBVOCI_PER_VOCE][256];
extern int   mod2_wavetable_loc[NUM_VOCI][SUBVOCI_PER_VOCE][256];

// --- Modulazione (per voce) ---
extern float          mod_loc[NUM_VOCI];            // "mod" del loop
extern int            modIn_loc[NUM_VOCI];
extern int            modInB_loc[NUM_VOCI];
extern int            modLev_loc[NUM_VOCI];
extern int            modLevA_loc[NUM_VOCI];
extern unsigned long  speedMod_loc[NUM_VOCI];
extern unsigned long  prevTimeMod_loc[NUM_VOCI];
extern int            contaMod_loc[NUM_VOCI];

// --- LFO pitch (per voce) ---
extern int            modPitchLev_loc[NUM_VOCI];
extern int            modPitchLevA_loc[NUM_VOCI];
extern unsigned long  speedPitchMod_loc[NUM_VOCI];
extern unsigned long  prevTimePitchMod_loc[NUM_VOCI];
extern int            contaPitchMod_loc[NUM_VOCI];

// --- Bend / Mod modulation ---
extern float   frBend_offset_loc[NUM_VOCI];
extern float   frMod_offset_loc[NUM_VOCI];
extern uint8_t bendMaxUp_loc[NUM_VOCI];
extern uint8_t bendMaxDown_loc[NUM_VOCI];
extern uint8_t sendBend_loc[NUM_VOCI];
extern uint8_t sendMod_loc[NUM_VOCI];

// --- Glide (slide) ---
extern uint8_t       slide_loc[NUM_VOCI];
extern int           tempoSlide_loc[NUM_VOCI];
extern unsigned long speedSlide_loc[NUM_VOCI];
extern float         glideTarget_loc[NUM_VOCI];
extern float         glideCurrent_loc[NUM_VOCI];
extern float         glideStep_loc[NUM_VOCI];

// --- VCF LFO per-voce ---
extern uint8_t lfoVcfLev_loc[NUM_VOCI];
extern uint8_t speedVcfLfo_loc[NUM_VOCI];
extern uint8_t vcfWave_loc[NUM_VOCI];
extern uint8_t lfoVcf1Syn_loc[NUM_VOCI];
extern uint8_t lfoVcfMulti_loc[NUM_VOCI];
extern uint8_t lfoVcfLev2B_loc[NUM_VOCI];
extern uint8_t lfoVcf2Syn_loc[NUM_VOCI];    // (flag separato, se serve)

// --- DSP ---
extern uint8_t dspNum_loc[NUM_VOCI];

// --- MIDI routing (per voce, non-chip) ---
extern uint8_t midiCh_loc[NUM_VOCI];
extern uint8_t notaSplit_loc[NUM_VOCI];
extern uint8_t midiMode_loc[NUM_VOCI];

// --- LFO mod sync ---
extern uint8_t lfoMod2Syn_loc[NUM_VOCI];
extern uint8_t contaOffset_loc[NUM_VOCI];

// -------------------------------------------------------------------------
// 8. NOTE STACK (per voce)
// -------------------------------------------------------------------------
extern uint8_t noteStack_loc  [NUM_VOCI][NOTE_STACK_MAX];
extern uint8_t noteCount_loc  [NUM_VOCI];      // 0 = nessuna nota premuta
extern uint8_t currentNote_loc[NUM_VOCI];      // MIDI pitch (0..127)

// -------------------------------------------------------------------------
// 9. ADSR (per voce)
// -------------------------------------------------------------------------
enum EnvState : uint8_t {
    ENV_IDLE = 0,
    ENV_ATTACK,
    ENV_DECAY,
    ENV_SUSTAIN,
    ENV_RELEASE
};

extern uint8_t adsr_a_loc[NUM_VOCI];
extern uint8_t adsr_d_loc[NUM_VOCI];
extern uint8_t adsr_s_loc[NUM_VOCI];
extern uint8_t adsr_r_loc[NUM_VOCI];

extern uint8_t envState_loc  [NUM_VOCI];   // EnvState
extern float   envPhase_loc  [NUM_VOCI];   // 0.0..1.0 nella fase corrente
extern float   envLevel_loc  [NUM_VOCI];   // 0.0..1.0
extern float   envRelStart_loc[NUM_VOCI];  // livello al momento del release

// -------------------------------------------------------------------------
// 10. ARPEGGIATOR per-voce
// -------------------------------------------------------------------------
extern uint8_t arp_on_loc[NUM_VOCI];
extern uint8_t arp_mode_loc[NUM_VOCI];      // 0=">>", 1="<<", 2="><"
extern uint8_t arp_multi_loc[NUM_VOCI];     // 0..4 (indice arpMultiArr)
extern uint8_t arp_octaves_loc[NUM_VOCI];   // 0..2
extern int8_t  arp_step_loc[NUM_VOCI];      // indice nello stack
extern uint8_t arp_dir_loc[NUM_VOCI];       // direzione per ><
extern uint8_t arp_octave_loc[NUM_VOCI];    // ottava corrente
extern uint8_t arp_oct_dir_loc[NUM_VOCI];

// -------------------------------------------------------------------------
// 11. GATER per-voce
// -------------------------------------------------------------------------
extern uint8_t gater_on_loc[NUM_VOCI];
extern uint8_t gater_num_loc[NUM_VOCI];     // 0..7
extern uint8_t gater_multi_loc[NUM_VOCI];   // 0..4
extern uint8_t gater_step_loc[NUM_VOCI];    // 0..15
extern uint8_t gater_lung_loc[NUM_VOCI];    // 0..99

// -------------------------------------------------------------------------
// 12. AM (mode 2) per-voce
// -------------------------------------------------------------------------
extern int      am_k_loc[NUM_VOCI];         // contatore 0..63
extern uint32_t am_timer_loc[NUM_VOCI];     // micros()

// -------------------------------------------------------------------------
// 13. STATO SISTEMA (globale, gestito solo dal Master)
// -------------------------------------------------------------------------
// synthMode è definito solo nel Master, ma per uniformità lo dichiariamo
// come extern in tutti (le slave non lo usano).
extern uint8_t synthMode;    // 0=Poly, 1=MultiMono (Master only)
extern uint8_t presetSel;    // preset attivo (0..29)
extern uint8_t presetSave;   // slot di salvataggio

// -------------------------------------------------------------------------
// 14. INTERVAL MULTIPLIERS (per sub_voce 1)
// -------------------------------------------------------------------------
// intervalMul[i] = 2^((i-24)/12)   per i in 0..48   (interval -24..+24)
extern float intervalMul[49];

// -------------------------------------------------------------------------
// 15. STATE DEFINITION — la parte di "istanza" va nel .ino
// -------------------------------------------------------------------------
// Nel .ino vanno definite le variabili (non extern). Questo header
// le dichiara soltanto. In questo modo tutti i moduli possono
// accedervi senza duplicazione.
// =========================================================================
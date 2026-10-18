#pragma once
#include "synthConfig.h"
#include "synthState.h"
#include "synthEngine.h"
#include "synthControl.h"

// =========================================================================
// synthLws.h — Layer LWS: callback, bridge, Poly broadcast
// =========================================================================
// Contiene:
//   - utility di routing (ownerOfVoice, voiceToLocal, frameVoiceGlobal)
//   - dispatch per CMD (param, param_voce, i32, midi, preset)
//   - bridge Router <-> Chain
//   - Poly broadcast (solo Master 'a')
//   - pollRouter / pollChain
//   - tracking passivo "last seen" per dumpStatus (no heartbeat attivo)
//
// NON usa lws_mcu_poll() dell'header perché gestiamo 2 porte e CMD estesi.
// Usa solo le utility lws_send_frame() e lws_next_seq() dell'header.
// =========================================================================
#include "synthPreset.h"

// -------------------------------------------------------------------------
// PRESET ENTRY HANDLER — chiamato per ogni K-V del preset
// -------------------------------------------------------------------------
// La voce corrente è presetVoice (globale 0..4). Il target è già
// filtrato a monte: se non è nostro, non arriviamo qui.
// -------------------------------------------------------------------------
// Forward declaration (Arduino IDE la generava automaticamente)
static inline int voiceToLocal(uint8_t vGlobal);
// Variabile di stato per la calibrazione (era nel .ino originale)
static int calibraNota = -1;
// Contatori per dumpStatus
static uint32_t g_frameFromRouter = 0;
static uint32_t g_frameFromChain  = 0;
static uint32_t g_pongSent        = 0;
static uint32_t g_presetApplied   = 0;

// --- Tracking passivo "last seen": aggiornato SOLO in ricezione ---
// Zero traffico generato. Serve solo a dumpStatus() per diagnostica.
// Indice = MCU_ID - 'a'  →  0='a', 1='b', 2='c'
static uint32_t g_lastSeen_ms[3] = {0, 0, 0};

static inline void trackDownstream(uint8_t senderId) {
    int idx = (int)senderId - (int)'a';
    if (idx >= 0 && idx < 3) g_lastSeen_ms[idx] = millis();
}

static void dumpStatus();

static void synthA_presetEntry(uint8_t key, uint8_t len, const uint8_t* value) {
    int v = voiceToLocal(presetVoice);
    if (v < 0) return;   // sicurezza: non è per noi

    switch (key) {
        // ---------- uint8 ----------
        case K_MODE:          if (len>=1) { mode_loc[v]=presetRd8(value); wavetable_setup_voce(v); } break;
        case K_WAVEFORM:      if (len>=1) { waveform_loc[v][0]=presetRd8(value); wavetable_setup_voce(v); } break;
        case K_SUB1_WAVEFORM: if (len>=1) { waveform_loc[v][1]=presetRd8(value); wavetable_setup_voce(v); } break;
        case K_FM_SELECT:     if (len>=1) fmSel_loc[v]       = presetRd8(value); break;
        case K_OTTAVA:        if (len>=1) { ottava_loc[v]=presetRd8(value); setOttava_loc(v, ottava_loc[v]); } break;
        case K_ATTENUA:       if (len>=1) attenua_loc[v]     = presetRd8(value); break;
        case K_TRACKING:      if (len>=1) trackingLev_loc[v] = presetRd8(value); break;
        case K_DETUNE:        if (len>=1) { detune_loc[v]=presetRd8(value);
                                            detuneFlo_loc[v]=(float)detune_loc[v]/90.0f; } break;
        case K_TEMPO_SLIDE:   if (len>=1) { tempoSlide_loc[v]=presetRd8(value); updateGlideStep(v); } break;
        case K_VCF_WAVE:      if (len>=1) vcfWave_loc[v]     = presetRd8(value); break;
        case K_VCF_LFO_LEV:   if (len>=1) lfoVcfLev_loc[v]   = presetRd8(value); break;
        case K_VCF_LFO_RATE:  if (len>=1) speedVcfLfo_loc[v] = presetRd8(value); break;
        case K_VCF_LFO_SYNC:  if (len>=1) lfoVcf1Syn_loc[v]  = presetRd8(value); break;
        case K_VCF_MULTI:     if (len>=1) lfoVcfMulti_loc[v] = presetRd8(value); break;
        case K_VCF_START:     if (len>=1) lfoVcfLev2B_loc[v] = presetRd8(value); break;
        case K_BEND_UP:       if (len>=1) bendMaxUp_loc[v]   = presetRd8(value); break;
        case K_BEND_DOWN:     if (len>=1) bendMaxDown_loc[v] = presetRd8(value); break;
        case K_SEND_BEND:     if (len>=1) sendBend_loc[v]    = presetRd8(value); break;
        case K_SEND_MOD:      if (len>=1) sendMod_loc[v]     = presetRd8(value); break;
        case K_SLIDE:         if (len>=1) { slide_loc[v]=presetRd8(value); updateGlideStep(v); } break;
        case K_LFO_MOD_SYNC:  if (len>=1) lfoMod2Syn_loc[v]  = presetRd8(value); break;
        case K_DSP:           if (len>=1) dspNum_loc[v]      = presetRd8(value); break;
        case K_ARP_ON:        if (len>=1) { arp_on_loc[v]=presetRd8(value);
                                            if (!arp_on_loc[v]) { arp_step_loc[v]=0; arp_octave_loc[v]=0; } } break;
        case K_ARP_MODE:      if (len>=1) arp_mode_loc[v]    = presetRd8(value); break;
        case K_ARP_MULTI:     if (len>=1) arp_multi_loc[v]   = presetRd8(value); break;
        case K_ARP_OCTAVES:   if (len>=1) arp_octaves_loc[v] = presetRd8(value); break;
        case K_GATER_ON:      if (len>=1) { gater_on_loc[v]=presetRd8(value);
                                            if (!gater_on_loc[v]) gater_step_loc[v]=0; } break;
        case K_GATER_NUM:     if (len>=1) gater_num_loc[v]   = presetRd8(value); break;
        case K_GATE_MULTI:    if (len>=1) gater_multi_loc[v] = presetRd8(value); break;
        case K_GATE_LUNG:     if (len>=1) gater_lung_loc[v]  = presetRd8(value); break;
        case K_CONTA_OFFSET:  if (len>=1) contaOffset_loc[v] = presetRd8(value); break;
        case K_MIDI_CH:       if (len>=1) midiCh_loc[v]      = presetRd8(value); break;
        case K_NOTA_SPLIT:    if (len>=1) notaSplit_loc[v]   = presetRd8(value); break;
        case K_MIDI_MODE:     if (len>=1) midiMode_loc[v]    = presetRd8(value); break;
        case K_SUB1_LEVEL:    if (len>=1) sub_level_idx[v]   = presetRd8(value); break;
        case K_VOICE_VOL:     if (len>=1) voiceVol_idx[v]    = presetRd8(value); break;
        case K_ADSR_A:        if (len>=1) adsr_a_loc[v] = presetRd8(value); break;
        case K_ADSR_D:        if (len>=1) adsr_d_loc[v] = presetRd8(value); break;
        case K_ADSR_S:        if (len>=1) adsr_s_loc[v] = presetRd8(value); break;
        case K_ADSR_R:        if (len>=1) adsr_r_loc[v] = presetRd8(value); break;

        default: break;
    }   // ← chiude il primo switch (uint8)

    // =====================================================================
    // Secondo switch: parametri non-uint8 (uint16, uint32, FM operator)
    // =====================================================================
    switch (key) {

        // ---------- uint16 ----------
        case K_SUB1_INTERVAL: if (len>=2) {
                                  int16_t x=(int16_t)presetRd16(value);
                                  x=constrain(x,-24,24);
                                  sub_interval[v]=x;
                              } break;
        case K_MOD_IN_B:      if (len>=2) modInB_loc[v]  = presetRd16(value); break;
        case K_MOD_LEV:       if (len>=2) {
                                  modLev_loc[v]  = presetRd16(value);
                                  modLevA_loc[v] = modLev_loc[v] - (modLev_loc[v]*2);
                              } break;
        case K_PITCH_LEV:     if (len>=2) {
                                  modPitchLev_loc[v]  = presetRd16(value);
                                  modPitchLevA_loc[v] = modPitchLev_loc[v] - (modPitchLev_loc[v]*2);
                              } break;

        // ---------- uint32 ----------
        case K_LFO_RATE:      if (len>=4) speedMod_loc[v]      = presetRd32(value); break;
        case K_PITCH_RATE:    if (len>=4) speedPitchMod_loc[v] = presetRd32(value); break;

        // ---------- FM operator (globali chip) ----------
        case K_FM_SIN_0:      if (len>=2) fmSetSin[fmSel_loc[v]][0] = (float)(int16_t)presetRd16(value)/100.0f; break;
        case K_FM_SIN_1:      if (len>=2) fmSetSin[fmSel_loc[v]][1] = (float)(int16_t)presetRd16(value)/100.0f; break;
        case K_FM_SIN_2:      if (len>=2) fmSetSin[fmSel_loc[v]][2] = (float)(int16_t)presetRd16(value)/100.0f; break;
        case K_FM_DIV_0:      if (len>=2) fmSetDiv[fmSel_loc[v]][0] = presetRd16(value); break;
        case K_FM_DIV_1:      if (len>=2) fmSetDiv[fmSel_loc[v]][1] = presetRd16(value); break;
        case K_FM_DIV_2:      if (len>=2) fmSetDiv[fmSel_loc[v]][2] = presetRd16(value); break;

        default:
            // Chiave sconosciuta: ignora silenziosamente (compatibilità avanti)
            break;
    }
}

// -------------------------------------------------------------------------
// 1. UTILITY DI ROUTING
// -------------------------------------------------------------------------
static inline char ownerOfVoice(uint8_t vGlobal) {
    if (vGlobal == 0)                return 'a';
    if (vGlobal == 1 || vGlobal == 2) return 'b';
    if (vGlobal == 3 || vGlobal == 4) return 'c';
    return 'a';
}

// Ritorna la voce LOCALE o -1 se non appartiene a questo chip.
static inline int voiceToLocal(uint8_t vGlobal) {
    if (vGlobal <  VOCE_BASE)              return -1;
    if (vGlobal >= VOCE_BASE + NUM_VOCI)   return -1;
    return (int)(vGlobal - VOCE_BASE);
}

// -------------------------------------------------------------------------
// 2. CLASSIFICAZIONE FRAME
// -------------------------------------------------------------------------

// Estrae la voce globale (0..4) o -1 se il CMD non ha campo voice.
//
// MIDI_V è DUAL-FORMAT (auto-rilevato da f.len) per compatibilità con
// Display/Router che possono usare la convenzione "target-first":
//   voice-first  (interno SynthA): [voice][dati...]        NOTE=4 BEND=5 CC=3
//   target-first (Display/Router): [target][voice][dati...] NOTE=5 BEND=6 CC=4
// Le due lunghezze sono sempre distinte, quindi non c'è ambiguità.
static inline int frameVoiceGlobal(const LwsFrame& f) {
    switch ((char)f.cmd) {
        // --- target in data[0], voice in data[1] ---
        case CMD_PARAM_VOCE:
        case CMD_PARAM_I32_V:
        case CMD_PRESET_BEGIN:
        case CMD_PRESET_CHUNK:
        case CMD_PRESET_END:
            if (f.len < 2) return -1;
            return (int)f.data[1];

        // --- MIDI_V: dual-format ---
        case CMD_MIDI_NOTE_V:
            if (f.len == 4) return (int)f.data[0];   // voice-first
            if (f.len == 5) return (int)f.data[1];   // target-first
            return -1;
        case CMD_MIDI_BEND_V:
            if (f.len == 5) return (int)f.data[0];   // voice-first
            if (f.len == 6) return (int)f.data[1];   // target-first
            return -1;
        case CMD_MIDI_CC_V:
            if (f.len == 3) return (int)f.data[0];   // voice-first
            if (f.len == 4) return (int)f.data[1];   // target-first
            return -1;

        default:
            return -1;
    }
}

// CMD con target globale (no voice): PING, PARAM, PARAM_REL, PARAM_I32.
// Questi vengono instradati per data[0] (Fase 2 di handleFrameFromRouter).
static inline bool frameIsGlobalTargeted(uint8_t cmd) {
    switch ((char)cmd) {
        case CMD_PING:
        case CMD_PARAM:
        case CMD_PARAM_REL:
        case CMD_PARAM_I32:      // i32 globale (calibrazione)
            return true;
        default:
            return false;
    }
}

// I CMD che sono "voice-specific" e in Poly vanno replicati.
// (Non include CMD_PRESET_*: i preset viaggiano per-voce espliciti.)
static inline bool frameIsVoiceSpecific(uint8_t cmd) {
    switch ((char)cmd) {
        case CMD_PARAM_VOCE:
        case CMD_PARAM_I32_V:
        case CMD_MIDI_NOTE_V:
        case CMD_MIDI_BEND_V:
        case CMD_MIDI_CC_V:
            return true;
        default:
            return false;
    }
}

// -------------------------------------------------------------------------
// 3. FORWARDING HELPERS
// -------------------------------------------------------------------------
static inline void sendToChain(const LwsFrame& f) {
#if HAS_DOWNSTREAM
    lws_send_frame(LWS_PORT_DOWN, f.sender, f.seq, f.cmd, f.data, f.len);
#else
    UNUSED(f);
#endif
}

static inline void sendToRouter(const LwsFrame& f) {
    lws_send_frame(LWS_PORT_UP, f.sender, f.seq, f.cmd, f.data, f.len);
}

// -------------------------------------------------------------------------
// 4. CALLBACK APPLICATIVI — versione 1 (stub + parametri chiave)
// -------------------------------------------------------------------------

static inline void on_param(char target, char key, uint8_t value) {
    if (target != MCU_ID) return;   // già filtrato a monte
    LWS_DEBUG.printf("[%c] PARAM key=%c val=%u\n", MCU_ID, key, value);

    switch (key) {
#if MCU_ID == 'a'
        // Solo il Master applica la modalità di sistema
        case K_SYNTH_MODE:
            synthMode = (value != 0) ? 1 : 0;
            LWS_DEBUG.printf("[a] synthMode=%u\n", synthMode);
            break;
#endif
        case K_PANIC:
            // Applica localmente
            for (uint8_t v = 0; v < NUM_VOCI; v++) onAllNotesOff(v);

#if MCU_ID == 'a'
            // Broadcast panic ai V (solo il Master)
            {
                LwsFrame pf;
                pf.sender  = MCU_ID;
                pf.seq     = lws_next_seq();
                pf.cmd     = CMD_PARAM;
                pf.len     = 3;
                pf.data[0] = 'b';       // target
                pf.data[1] = K_PANIC;   // key
                pf.data[2] = 0;         // value
                sendToChain(pf);
            }
            {
                LwsFrame pf;
                pf.sender  = MCU_ID;
                pf.seq     = lws_next_seq();
                pf.cmd     = CMD_PARAM;
                pf.len     = 3;
                pf.data[0] = 'c';
                pf.data[1] = K_PANIC;
                pf.data[2] = 0;
                sendToChain(pf);
            }
            LWS_DEBUG.println("[a] panic broadcast");
#endif
            break;

        case K_PRESET_SEL:  presetSel  = value; break;
        case K_PRESET_SAVE: presetSave = value; break;

        default:
            LWS_DEBUG.printf("[%c] key sys non gestita: %c\n", MCU_ID, key);
            break;
    }
}

static inline void on_param_voce(char target, uint8_t vGlobal,
                                 char key, uint8_t value) {
    if (target != MCU_ID) return;
    int v = voiceToLocal(vGlobal);
    if (v < 0) return;

    switch (key) {
        // ---------- MODE / WAVEFORM ----------
        case K_MODE:
            mode_loc[v] = value;
            wavetable_setup_voce(v);
            break;
        case K_WAVEFORM:
            waveform_loc[v][0] = value;
            wavetable_setup_voce(v);
            break;
        case K_SUB1_WAVEFORM:
            waveform_loc[v][1] = value;
            wavetable_setup_voce(v);
            break;
        case K_FM_SELECT:
            fmSel_loc[v] = value;
            break;

        // ---------- TUNING ----------
        case K_OTTAVA:
            ottava_loc[v] = value;
            setOttava_loc(v, value);
            break;
        case K_ATTENUA:
            attenua_loc[v] = value;
            break;
        case K_TRACKING:
            trackingLev_loc[v] = value;
            break;
        case K_DETUNE:
            detune_loc[v]    = value;
            detuneFlo_loc[v] = (float)value / 90.0f;
            break;
        case K_TEMPO_SLIDE:
            tempoSlide_loc[v] = value;
            updateGlideStep(v);
            break;

        // ---------- VCF ----------
        case K_VCF_WAVE:     vcfWave_loc[v]      = value; break;
        case K_VCF_LFO_LEV:  lfoVcfLev_loc[v]    = value; break;
        case K_VCF_LFO_RATE: speedVcfLfo_loc[v]  = value; break;
        case K_VCF_LFO_SYNC: lfoVcf1Syn_loc[v]   = value; break;
        case K_VCF_MULTI:    lfoVcfMulti_loc[v]  = value; break;
        case K_VCF_START:    lfoVcfLev2B_loc[v]  = value; break;

        // ---------- BENDER / MOD ----------
        case K_BEND_UP:    bendMaxUp_loc[v]    = value; break;
        case K_BEND_DOWN:  bendMaxDown_loc[v]  = value; break;
        case K_SEND_BEND:  sendBend_loc[v]     = value; break;
        case K_SEND_MOD:   sendMod_loc[v]      = value; break;

        // ---------- SLIDE ----------
        case K_SLIDE:
            slide_loc[v] = value;
            updateGlideStep(v);
            break;

        // ---------- LFO SYNC ----------
        case K_LFO_MOD_SYNC:
            lfoMod2Syn_loc[v] = value;
            break;

        // ---------- DSP ----------
        case K_DSP: dspNum_loc[v] = value; break;

        // ---------- ARP ----------
        case K_ARP_ON:
            arp_on_loc[v] = value;
            if (!value) {
                arp_step_loc[v]   = 0;
                arp_octave_loc[v] = 0;
            }
            break;
        case K_ARP_MODE:    arp_mode_loc[v]    = value; break;
        case K_ARP_MULTI:   arp_multi_loc[v]   = value; break;
        case K_ARP_OCTAVES: arp_octaves_loc[v] = value; break;

        // ---------- GATER ----------
        case K_GATER_ON:
            gater_on_loc[v] = value;
            if (!value) gater_step_loc[v] = 0;
            break;
        case K_GATER_NUM:  gater_num_loc[v]   = value; break;
        case K_GATE_MULTI: gater_multi_loc[v] = value; break;
        case K_GATE_LUNG:  gater_lung_loc[v]  = value; break;

        // ---------- OFFSET ----------
        case K_CONTA_OFFSET: contaOffset_loc[v] = value; break;

        // ---------- MIDI ROUTING ----------
        case K_MIDI_CH:    midiCh_loc[v]    = value; break;
        case K_NOTA_SPLIT: notaSplit_loc[v] = value; break;
        case K_MIDI_MODE:  midiMode_loc[v]  = value; break;

        // ---------- SUB_VOCE 1 ----------
        case K_SUB1_LEVEL: sub_level_idx[v] = value; break;

        // ---------- VOLUME voce ----------
        case K_VOICE_VOL: voiceVol_idx[v] = value; break;

        // ---------- ADSR ----------
        case K_ADSR_A: adsr_a_loc[v] = value; break;
        case K_ADSR_D: adsr_d_loc[v] = value; break;
        case K_ADSR_S: adsr_s_loc[v] = value; break;
        case K_ADSR_R: adsr_r_loc[v] = value; break;

        default:
            LWS_DEBUG.printf("[%c] key voce non gestita: %c\n", MCU_ID, key);
            break;
    }
}

static inline void on_param_i32(char target, char key, int32_t value) {
    if (target != MCU_ID) return;
    LWS_DEBUG.printf("[%c] PARAM_I32 key=%c val=%ld\n",
                     MCU_ID, key, (long)value);

    switch (key) {
        case K_CALIB_SEL: calibraNota = (int)value; break;
        case K_CALIB_VAL:
            if (calibraNota >= 0 && calibraNota <= MAX_NOTEARRAY_IDX) {
                // NB: usa la prima voce locale come riferimento ottava
                noteArr[ottava_loc[0] - 1][calibraNota] = (float)value / 10.0f;
            }
            break;
        default: break;
    }
}

static inline void on_param_i32_voce(char target, uint8_t vGlobal,
                                     char key, int32_t value) {
    if (target != MCU_ID) return;
    int v = voiceToLocal(vGlobal);
    if (v < 0) return;

    switch (key) {
        case K_MOD_IN_B:   modInB_loc[v] = (int)value; break;
        case K_MOD_LEV:
            modLev_loc[v]  = (int)value;
            modLevA_loc[v] = modLev_loc[v] - (modLev_loc[v] * 2);
            break;
        case K_LFO_RATE:
            speedMod_loc[v] = (unsigned long)value;
            break;
        case K_PITCH_LEV:
            modPitchLev_loc[v]  = (int)value;
            modPitchLevA_loc[v] = modPitchLev_loc[v] - (modPitchLev_loc[v] * 2);
            break;
        case K_PITCH_RATE:
            speedPitchMod_loc[v] = (unsigned long)value;
            break;

        // ---------- FM operator parameters ----------
        case K_FM_SIN_0: fmSetSin[fmSel_loc[v]][0] = (float)value / 100.0f; break;
        case K_FM_SIN_1: fmSetSin[fmSel_loc[v]][1] = (float)value / 100.0f; break;
        case K_FM_SIN_2: fmSetSin[fmSel_loc[v]][2] = (float)value / 100.0f; break;
        case K_FM_DIV_0: fmSetDiv[fmSel_loc[v]][0] = (int)value; break;
        case K_FM_DIV_1: fmSetDiv[fmSel_loc[v]][1] = (int)value; break;
        case K_FM_DIV_2: fmSetDiv[fmSel_loc[v]][2] = (int)value; break;

        // ---------- SUB_VOCE 1 interval ----------
        case K_SUB1_INTERVAL: {
            int16_t iv = (int16_t)value;
            if (iv < -24) iv = -24;
            if (iv >  24) iv =  24;
            sub_interval[v] = iv;
            break;
        }
        default:
            LWS_DEBUG.printf("[%c] key i32 voce non gestita: %c\n", MCU_ID, key);
            break;
    }
}

static inline void on_midi_note_v(uint8_t vGlobal, uint8_t onoff,
                                  uint8_t pitch, uint8_t vel) {
    int v = voiceToLocal(vGlobal);
    if (v < 0) return;
    if (onoff) onNoteOn(v, pitch, vel);
    else       onNoteOff(v, pitch);
}

static inline void on_midi_bend_v(uint8_t vGlobal, int32_t bend) {
    int v = voiceToLocal(vGlobal);
    if (v < 0) return;

    float bendf = 0.0f;
    if (bend > 0)
        bendf = (float)map(bend, 1, 8191, 0, bendMaxUpArr[bendMaxUp_loc[v]]) / 4000.0f;
    if (bend < 0)
        bendf = (float)map(bend, 1, -8191, 0, bendMaxDownArr[bendMaxDown_loc[v]]) / 4000.0f;

    frBend_offset_loc[v] = bendf;
}

static inline void on_midi_cc_v(uint8_t vGlobal, uint8_t cc, uint8_t value) {
    int v = voiceToLocal(vGlobal);
    if (v < 0) return;

    switch (cc) {
        case 1: // mod wheel
            modPitchLev_loc[v]  = map(value, 0, 127, 0, 200);
            modPitchLevA_loc[v] = modPitchLev_loc[v] - (modPitchLev_loc[v] * 2);
            break;
        case 3: // mod lev
            modLev_loc[v]  = map(value, 0, 127, 0, 1023);
            modLevA_loc[v] = modLev_loc[v] - (modLev_loc[v] * 2);
            break;
        case 4: // mod in B
            modInB_loc[v] = map(value, 0, 127, 0, 1024);
            break;
        // Altri CC da mappare in fase di test.
        default: break;
    }
}

static inline void on_error(const char *msg) {
    LWS_DEBUG.printf("[%c] ERROR: %s\n", MCU_ID, msg);
}

// =========================================================================
// dumpStatus() — stampa stato interno su seriale USB
// =========================================================================
static inline void dumpStatus() {
    // ===== HEADER =====
    Serial.println();
    Serial.println("[DUMP] ========== SYNTHA STATUS ==========");
    Serial.printf("[DUMP] mcu=%c num_voci=%d voce_base=%d\n",
                  MCU_ID, NUM_VOCI, VOCE_BASE);
    Serial.printf("[DUMP] uptime=%lu ms\n", millis());
    Serial.printf("[DUMP] synthMode=%u presetSel=%u presetSave=%u\n",
                  synthMode, presetSel, presetSave);

    // ===== TRAFFIC =====
    Serial.println("[DUMP] --- TRAFFIC ---");
    Serial.printf("[DUMP] frame_from_router=%lu\n", g_frameFromRouter);
    Serial.printf("[DUMP] frame_from_chain=%lu\n",  g_frameFromChain);
    Serial.printf("[DUMP] pong_sent=%lu\n",         g_pongSent);
    Serial.printf("[DUMP] preset_applied=%lu\n",    g_presetApplied);

    // ===== PER VOICE =====
    for (int v = 0; v < NUM_VOCI; v++) {
        Serial.printf("[DUMP] --- VOICE %d (global %d) ---\n",
                      v, v + VOCE_BASE);
        Serial.printf("[DUMP]   mode=%u wf0=%u wf1=%u fmSel=%u\n",
                      mode_loc[v], waveform_loc[v][0],
                      waveform_loc[v][1], fmSel_loc[v]);
        Serial.printf("[DUMP]   ottava=%u attenua=%u tracking=%u detune=%u tempoSlide=%u\n",
                      ottava_loc[v], attenua_loc[v],
                      trackingLev_loc[v], detune_loc[v], tempoSlide_loc[v]);
        Serial.printf("[DUMP]   adsr A=%u D=%u S=%u R=%u\n",
                      adsr_a_loc[v], adsr_d_loc[v],
                      adsr_s_loc[v], adsr_r_loc[v]);
        Serial.printf("[DUMP]   vol_idx=%u sub1_lvl_idx=%u sub1_int=%d\n",
                      voiceVol_idx[v], sub_level_idx[v], sub_interval[v]);
        Serial.printf("[DUMP]   vcf wave=%u lfoLev=%u lfoRate=%u sync=%u multi=%u start=%u\n",
                      vcfWave_loc[v], lfoVcfLev_loc[v], speedVcfLfo_loc[v],
                      lfoVcf1Syn_loc[v], lfoVcfMulti_loc[v], lfoVcfLev2B_loc[v]);
        Serial.printf("[DUMP]   lfoRate=%lu pitchRate=%lu modInB=%d modLev=%d pitchLev=%d\n",
                      speedMod_loc[v], speedPitchMod_loc[v],
                      modInB_loc[v], modLev_loc[v], modPitchLev_loc[v]);
        Serial.printf("[DUMP]   bendUp=%u bendDown=%u sendBend=%u sendMod=%u\n",
                      bendMaxUp_loc[v], bendMaxDown_loc[v],
                      sendBend_loc[v], sendMod_loc[v]);
        Serial.printf("[DUMP]   slide=%u lfoModSync=%u dsp=%u\n",
                      slide_loc[v], lfoMod2Syn_loc[v], dspNum_loc[v]);
        Serial.printf("[DUMP]   arp on=%u mode=%u multi=%u oct=%u\n",
                      arp_on_loc[v], arp_mode_loc[v],
                      arp_multi_loc[v], arp_octaves_loc[v]);
        Serial.printf("[DUMP]   gater on=%u num=%u multi=%u lung=%u\n",
                      gater_on_loc[v], gater_num_loc[v],
                      gater_multi_loc[v], gater_lung_loc[v]);
        Serial.printf("[DUMP]   contaOffset=%u midiCh=%u notaSplit=%u midiMode=%u\n",
                      contaOffset_loc[v], midiCh_loc[v],
                      notaSplit_loc[v], midiMode_loc[v]);
    }

    // ===== FM OPERATORS =====
    Serial.println("[DUMP] --- FM OPERATORS ---");
    for (int p = 0; p < 8; p++) {
        Serial.printf("[DUMP]   fm[%d] sin=%.2f,%.2f,%.2f div=%d,%d,%d\n",
                      p,
                      fmSetSin[p][0], fmSetSin[p][1], fmSetSin[p][2],
                      fmSetDiv[p][0], fmSetDiv[p][1], fmSetDiv[p][2]);
    }

    // ===== CALIBRAZIONE =====
    Serial.printf("[DUMP] calibraNota=%d\n", calibraNota);

    // ===== CHAIN — last seen (passivo, no heartbeat) =====
    Serial.println("[DUMP] --- CHAIN LAST-SEEN ---");
    {
        uint32_t nowms = millis();
        for (int i = 0; i < 3; i++) {
            char id = (char)('a' + i);
            if (id == MCU_ID) continue;              // salta se stesso
            if (g_lastSeen_ms[i] == 0) {
                Serial.printf("[DUMP]   %c never seen\n", id);
            } else {
                Serial.printf("[DUMP]   %c last_seen=%lu ms ago\n",
                              id, (unsigned long)(nowms - g_lastSeen_ms[i]));
            }
        }
    }

    // ===== FOOTER =====
    Serial.println("[DUMP] ==========================================");
    Serial.println();
}

// -------------------------------------------------------------------------
// 5. DISPATCH LOCALE — chiamato quando un frame è destinato a noi
// -------------------------------------------------------------------------
static void dispatchLocal(const LwsFrame& f) {
    switch ((char)f.cmd) {
        case CMD_PARAM:
            if (f.len >= 3) {
                if ((char)f.data[1] == '?') { dumpStatus(); break; }
                on_param((char)f.data[0], (char)f.data[1], f.data[2]);
            }
            break;

        case CMD_PING:
            // [target_id] == MCU_ID -> PONG
            if (f.len >= 1 && (char)f.data[0] == MCU_ID) {
                lws_send_frame(LWS_PORT_UP, MCU_ID, lws_next_seq(),
                               CMD_PONG, nullptr, 0);
                g_pongSent++;
                LWS_DEBUG.printf("[%c] PONG\n", MCU_ID);
            }
            break;

        case CMD_PARAM_REL:
            if (f.len >= 3) {
                on_param((char)f.data[0], (char)f.data[1], f.data[2]);
                // ACK
                uint8_t ack[2] = { f.seq, f.cmd };
                lws_send_frame(LWS_PORT_UP, MCU_ID, lws_next_seq(),
                               CMD_PARAM_ACK, ack, 2);
            }
            break;

        case CMD_PARAM_VOCE:
            if (f.len >= 4)
                on_param_voce((char)f.data[0], f.data[1],
                              (char)f.data[2], f.data[3]);
            break;

        // ---------- I32 (case aggiunti: prima mancavano) ----------
        case CMD_PARAM_I32:
            if (f.len >= 6)
                on_param_i32((char)f.data[0], (char)f.data[1],
                             lws_unpack_i32_le(&f.data[2]));
            break;

        case CMD_PARAM_I32_V:
            if (f.len >= 7)
                on_param_i32_voce((char)f.data[0], f.data[1],
                                  (char)f.data[2],
                                  lws_unpack_i32_le(&f.data[3]));
            break;

        // ---------- MIDI_V (dual-format) ----------
        case CMD_MIDI_NOTE_V:
            if (f.len == 4)
                on_midi_note_v(f.data[0], f.data[1], f.data[2], f.data[3]);
            else if (f.len == 5)
                on_midi_note_v(f.data[1], f.data[2], f.data[3], f.data[4]);
            break;

        case CMD_MIDI_BEND_V:
            if (f.len == 5)
                on_midi_bend_v(f.data[0], lws_unpack_i32_le(&f.data[1]));
            else if (f.len == 6)
                on_midi_bend_v(f.data[1], lws_unpack_i32_le(&f.data[2]));
            break;

        case CMD_MIDI_CC_V:
            if (f.len == 3)
                on_midi_cc_v(f.data[0], f.data[1], f.data[2]);
            else if (f.len == 4)
                on_midi_cc_v(f.data[1], f.data[2], f.data[3]);
            break;

        // ---------- PRESET TRANSFER ----------
        case CMD_PRESET_BEGIN:
            if (f.len >= 5) {
                uint8_t voice = f.data[1];
                uint8_t id    = f.data[2];
                uint16_t len  = (uint16_t)f.data[3] | ((uint16_t)f.data[4] << 8);
                uint8_t status;
                if (len > PRESET_MAX_LEN) {
                    status = PRESET_ACK_LEN_FAIL;
                } else {
                    presetBegin(voice, id, len);
                    status = PRESET_ACK_OK;
                }
                uint8_t ack[3] = { f.data[0], voice, status };
                lws_send_frame(LWS_PORT_UP, MCU_ID, lws_next_seq(),
                               CMD_PRESET_ACK, ack, 3);
                LWS_DEBUG.printf("[%c] preset BEGIN voice=%u len=%u -> %u\n",
                                 MCU_ID, voice, len, status);
            }
            break;

        case CMD_PRESET_CHUNK:
            if (f.len >= 5) {
                uint8_t voice = f.data[1];
                uint16_t off  = (uint16_t)f.data[2] | ((uint16_t)f.data[3] << 8);
                uint8_t dlen  = f.len - 4;
                presetChunk(voice, off, &f.data[4], dlen);
            }
            break;

        case CMD_PRESET_END:
            if (f.len >= 3) {
                uint8_t voice = f.data[1];
                uint8_t crc   = f.data[2];
                uint8_t st    = presetEnd(voice, crc);
                if (st == PRESET_ACK_OK) {
                    uint8_t rc = presetApply(synthA_presetEntry);
                    if (rc != 0) st = rc;
                    else         g_presetApplied++;
                }
                uint8_t ack[3] = { f.data[0], voice, st };
                lws_send_frame(LWS_PORT_UP, MCU_ID, lws_next_seq(),
                               CMD_PRESET_ACK, ack, 3);
                LWS_DEBUG.printf("[%c] preset END voice=%u -> %u\n",
                                 MCU_ID, voice, st);
            }
            break;

        case CMD_PRESET_READ:
            // (fase 2: dump del preset corrente)
            break;

        case CMD_ERROR: {
            char msg[64] = {0};
            if (f.len >= 2) {
                size_t l = f.len - 1;
                if (l > 62) l = 62;
                memcpy(msg, &f.data[1], l);
                msg[l] = '\0';
            }
            on_error(msg);
            break;
        }

        default:
            // CMD non gestito: ignora silenziosamente
            break;
    }
}

// -------------------------------------------------------------------------
// 6. POLY BROADCAST (solo Master 'a')
// -------------------------------------------------------------------------
#if MCU_ID == 'a'
// Replica un frame voice-specific (voice 0) sulle voci 1..4, inviandolo
// su Chain. Il target del payload viene ricalcolato per ogni voce.
static void polyBroadcast(const LwsFrame& f) {
    for (uint8_t vg = 1; vg <= 4; vg++) {
        LwsFrame child = f;
        char owner = ownerOfVoice(vg);

        if (f.cmd == CMD_PARAM_VOCE || f.cmd == CMD_PARAM_I32_V) {
            // data = [target][voice][key][...]
            child.data[0] = (uint8_t)owner;
            child.data[1] = vg;
        } 
		else if (f.cmd == CMD_MIDI_NOTE_V || f.cmd == CMD_MIDI_BEND_V ||
                   f.cmd == CMD_MIDI_CC_V) {
            // data = [voice][...]  (formato voice-first interno)
            child.data[0] = vg;
        }
        sendToChain(child);
    }
    LWS_DEBUG.println("[a] poly broadcast -> 4 voci");
}
#endif

// -------------------------------------------------------------------------
// 7. GESTIONE FRAME DAL ROUTER (UP -> DOWN)
// -------------------------------------------------------------------------
static void handleFrameFromRouter(const LwsFrame& f) {
    g_frameFromRouter++;

    // =====================================================================
    // FASE 1 — VOICE-FIRST
    // Qualsiasi CMD con campo voice viene instradato in base a
    // ownerOfVoice(voice). Il campo target NON è usato come criterio
    // primario (evita errori del Display e sblocca il forwarding preset).
    // =====================================================================
    int vg = frameVoiceGlobal(f);
    if (vg >= 0) {
        char owner = ownerOfVoice((uint8_t)vg);

        if (owner != MCU_ID) {
            // Non è nostra: forward su Chain
            sendToChain(f);
            LWS_DEBUG.printf("[%c] relay voice=%d cmd=%c -> DOWN\n",
                             MCU_ID, vg, (char)f.cmd);
            return;
        }

        // È nostra: applica localmente
        dispatchLocal(f);

        // Poly broadcast (solo Master, solo Poly mode, solo voce 0)
#if MCU_ID == 'a'
        if (synthMode == 0 && frameIsVoiceSpecific(f.cmd) && vg == 0) {
            polyBroadcast(f);
        }
#endif
        return;
    }

    // =====================================================================
    // FASE 2 — TARGET GLOBALE (PING, PARAM, PARAM_REL, PARAM_I32)
    // =====================================================================
    if (frameIsGlobalTargeted(f.cmd) && f.len >= 1) {
        if ((char)f.data[0] != MCU_ID) {
            sendToChain(f);
            LWS_DEBUG.printf("[%c] relay target=%c cmd=%c -> DOWN\n",
                             MCU_ID, (char)f.data[0], (char)f.cmd);
            return;
        }
    }

    // =====================================================================
    // FASE 3 — FALLBACK (ERROR, PONG, CMD non classificati)
    // =====================================================================
    dispatchLocal(f);
}

// -------------------------------------------------------------------------
// 8. GESTIONE FRAME DALLA CHAIN (DOWN -> UP)
// -------------------------------------------------------------------------
// Tutto quello che arriva dalla Chain viene forwardato al Router.
// Il SENDER originale (b o c) viene preservato, quindi il Display vede
// i nodi remoti come se fossero direttamente connessi.
static void handleFrameFromChain(const LwsFrame& f) {
    g_frameFromChain++;
    trackDownstream(f.sender);            // tracking passivo per dumpStatus
    sendToRouter(f);
    LWS_DEBUG.printf("[%c] relay DOWN->UP sender=%c cmd=%c\n",
                     MCU_ID, (char)f.sender, (char)f.cmd);
}

// -------------------------------------------------------------------------
// 9. POLL DELLE DUE PORTE
// -------------------------------------------------------------------------
static LwsParser parserRouter;
static LwsParser parserChain;

static void pollRouter() {
    while (LWS_PORT_UP.available() > 0) {
        uint8_t b = (uint8_t)LWS_PORT_UP.read();
        LwsFrame f;
        if (parserRouter.feed(b, f)) handleFrameFromRouter(f);
    }
}

static void pollChain() {
#if HAS_DOWNSTREAM
    while (LWS_PORT_DOWN.available() > 0) {
        uint8_t b = (uint8_t)LWS_PORT_DOWN.read();
        LwsFrame f;
        if (parserChain.feed(b, f)) handleFrameFromChain(f);
    }
#endif
}
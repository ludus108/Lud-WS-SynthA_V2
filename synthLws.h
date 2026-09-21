#pragma once
#include "synthConfig.h"
#include "synthState.h"
#include "synthEngine.h"
#include "synthControl.h"

// =========================================================================
// synthLws.h — Layer LWS: callback, bridge, Poly broadcast
// =========================================================================
// Contiene:
//   - utility di routing (ownerOfVoice, voiceToLocal)
//   - dispatch per CMD (param, param_voce, i32, midi)
//   - bridge Router <-> Chain
//   - Poly broadcast (solo Master 'a')
//   - pollRouter / pollChain
//
// NON usa lws_mcu_poll() dell'header perché gestiamo 2 porte e CMD estesi.
// Usa solo le utility lws_send_frame() e lws_next_seq() dell'header.
// =========================================================================

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
// I CMD che hanno "target" nel primo byte del payload.
static inline bool frameHasTargetField(uint8_t cmd) {
    switch ((char)cmd) {
        case CMD_PING:
        case CMD_PARAM:
        case CMD_PARAM_REL:
        case CMD_PARAM_VOCE:
        case CMD_PARAM_I32:
        case CMD_PARAM_I32_V:
            return true;
        default:
            return false;
    }
}

// I CMD che hanno "voice" nel primo byte del payload.
static inline bool frameHasVoiceField(uint8_t cmd) {
    switch ((char)cmd) {
        case CMD_MIDI_NOTE_V:
        case CMD_MIDI_BEND_V:
        case CMD_MIDI_CC_V:
            return true;
        default:
            return false;
    }
}

// I CMD che sono "voice-specific" e in Poly vanno replicati.
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
// Sono richiamati da dispatchLocal(). Per l'editing: qui si aggiungono
// gli switch completi dei parametri (chiavi K_*). Per ora copriamo le
// chiavi essenziali per il test end-to-end.

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

// -------------------------------------------------------------------------
// 5. DISPATCH LOCALE — chiamato quando un frame è destinato a noi
// -------------------------------------------------------------------------
static void dispatchLocal(const LwsFrame& f) {
    switch ((char)f.cmd) {
        case CMD_PING:
            // [target_id] == MCU_ID -> PONG
            if (f.len >= 1 && (char)f.data[0] == MCU_ID) {
                lws_send_frame(LWS_PORT_UP, MCU_ID, lws_next_seq(),
                               CMD_PONG, nullptr, 0);
                LWS_DEBUG.printf("[%c] PONG\n", MCU_ID);
            }
            break;

        case CMD_PARAM:
            if (f.len >= 3) on_param((char)f.data[0], (char)f.data[1], f.data[2]);
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

        case CMD_MIDI_NOTE_V:
            if (f.len >= 4)
                on_midi_note_v(f.data[0], f.data[1], f.data[2], f.data[3]);
            break;

        case CMD_MIDI_BEND_V:
            if (f.len >= 5)
                on_midi_bend_v(f.data[0], lws_unpack_i32_le(&f.data[1]));
            break;

        case CMD_MIDI_CC_V:
            if (f.len >= 3)
                on_midi_cc_v(f.data[0], f.data[1], f.data[2]);
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
        } else if (frameHasVoiceField(f.cmd)) {
            // data = [voice][...]
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
    // Fase 1: è un frame con target?
    if (frameHasTargetField(f.cmd) && f.len >= 1) {
        char target = (char)f.data[0];
        if (target != MCU_ID) {
            // Non è nostro: forward su Chain
            sendToChain(f);
            LWS_DEBUG.printf("[%c] relay UP->DOWN target=%c\n", MCU_ID, target);
            return;
        }
    }

    // Fase 2: è un frame con voice?
    if (frameHasVoiceField(f.cmd) && f.len >= 1) {
        uint8_t vg = f.data[0];
        char owner = ownerOfVoice(vg);
        if (owner != MCU_ID) {
            // Non è nostra: forward su Chain
            sendToChain(f);
            LWS_DEBUG.printf("[%c] relay UP->DOWN voice=%u\n", MCU_ID, vg);
            return;
        }
    }

    // Fase 3: applica localmente
    dispatchLocal(f);

    // Fase 4: Poly broadcast (solo Master)
#if MCU_ID == 'a'
    if (synthMode == 0 && frameIsVoiceSpecific(f.cmd) && f.len >= 1) {
        // Estrai la voice globale per capire se è la voce 0 (locale Master)
        uint8_t vg = 0;
        if (f.cmd == CMD_PARAM_VOCE || f.cmd == CMD_PARAM_I32_V) {
            if (f.len >= 2) vg = f.data[1];
        } else {
            vg = f.data[0];
        }
        if (vg == 0) polyBroadcast(f);
    }
#endif
}

// -------------------------------------------------------------------------
// 8. GESTIONE FRAME DALLA CHAIN (DOWN -> UP)
// -------------------------------------------------------------------------
// Tutto quello che arriva dalla Chain viene forwardato al Router.
// Il SENDER originale (b o c) viene preservato, quindi il Display vede
// i nodi remoti come se fossero direttamente connessi.
static void handleFrameFromChain(const LwsFrame& f) {
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
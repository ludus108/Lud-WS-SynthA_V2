#pragma once
#include "synthConfig.h"
#include "synthState.h"

// =========================================================================
// synthEngine.h — Motore audio del SynthA
// =========================================================================
// Contiene:
//   - utility di init (intervalMul, ottava, glide)
//   - ADSR (init + tick)
//   - Note stack (push/remove/clear)
//   - NoteOn / NoteOff / AllNotesOff (logica monofonica)
//   - Wavetable setup per-voce
//   - IRQ PWM (on_pwm_wrap)
//
// Dipende da: synthConfig.h, synthState.h (definizioni nel .ino)
// Va incluso DOPO la definizione delle variabili e PRIMA di setup().
// =========================================================================

// -------------------------------------------------------------------------
// 1. INIT — interval multipliers (sub_voce 1)
// -------------------------------------------------------------------------
static void init_intervalMul() {
    // index 24 = 1.0x (interval = 0 semitoni)
    for (int i = 0; i < 49; i++)
        intervalMul[i] = powf(2.0f, (float)(i - 24) / 12.0f);
}

// -------------------------------------------------------------------------
// 2. ADSR
// -------------------------------------------------------------------------
static inline float adsrTimeSeconds(uint8_t p) {
    if (p == 0) return 0.0f;
    float norm = (float)p / 255.0f;
    return norm * norm * 3.0f;      // 0..3 s
}

static inline void adsr_noteOn(uint8_t v) {
    envState_loc[v] = ENV_ATTACK;
    envPhase_loc[v] = 0.0f;
    // envLevel non viene resettato (evita click se eravamo in release)
}

static inline void adsr_noteOff(uint8_t v) {
    if (envState_loc[v] == ENV_IDLE) return;
    envState_loc[v]    = ENV_RELEASE;
    envPhase_loc[v]    = 0.0f;
    envRelStart_loc[v] = envLevel_loc[v];
}

// Chiamata dall'IRQ. dt = 1/PWM_IRQ_RATE_HZ.
static inline void adsr_tick(uint8_t v) {
    const float dt = 1.0f / PWM_IRQ_RATE_HZ;
    switch (envState_loc[v]) {
        case ENV_IDLE:
            envLevel_loc[v] = 0.0f;
            break;

        case ENV_ATTACK: {
            float t = adsrTimeSeconds(adsr_a_loc[v]);
            if (t < 1e-4f) {
                envLevel_loc[v] = 1.0f;
                envState_loc[v] = ENV_DECAY;
                envPhase_loc[v] = 0.0f;
                break;
            }
            envPhase_loc[v] += dt / t;
            if (envPhase_loc[v] >= 1.0f) {
                envLevel_loc[v] = 1.0f;
                envState_loc[v] = ENV_DECAY;
                envPhase_loc[v] = 0.0f;
            } else {
                envLevel_loc[v] = envPhase_loc[v];
            }
            break;
        }

        case ENV_DECAY: {
            float t    = adsrTimeSeconds(adsr_d_loc[v]);
            float sLev = (float)adsr_s_loc[v] / 255.0f;
            if (t < 1e-4f) {
                envLevel_loc[v] = sLev;
                envState_loc[v] = ENV_SUSTAIN;
                break;
            }
            envPhase_loc[v] += dt / t;
            if (envPhase_loc[v] >= 1.0f) {
                envLevel_loc[v] = sLev;
                envState_loc[v] = ENV_SUSTAIN;
            } else {
                envLevel_loc[v] = 1.0f - envPhase_loc[v] * (1.0f - sLev);
            }
            break;
        }

        case ENV_SUSTAIN:
            envLevel_loc[v] = (float)adsr_s_loc[v] / 255.0f;
            break;

        case ENV_RELEASE: {
            float t = adsrTimeSeconds(adsr_r_loc[v]);
            if (t < 1e-4f) {
                envLevel_loc[v] = 0.0f;
                envState_loc[v] = ENV_IDLE;
                break;
            }
            envPhase_loc[v] += dt / t;
            if (envPhase_loc[v] >= 1.0f) {
                envLevel_loc[v] = 0.0f;
                envState_loc[v] = ENV_IDLE;
            } else {
                envLevel_loc[v] = envRelStart_loc[v] * (1.0f - envPhase_loc[v]);
            }
            break;
        }
    }
}

// -------------------------------------------------------------------------
// 3. NOTE STACK
// -------------------------------------------------------------------------
static void noteStack_push(uint8_t v, uint8_t pitch) {
    // evita duplicati
    for (uint8_t i = 0; i < noteCount_loc[v]; i++)
        if (noteStack_loc[v][i] == pitch) return;

    if (noteCount_loc[v] >= NOTE_STACK_MAX) {
        // overflow: scarta la più vecchia (indice 0)
        for (uint8_t i = 1; i < NOTE_STACK_MAX; i++)
            noteStack_loc[v][i - 1] = noteStack_loc[v][i];
        noteCount_loc[v] = NOTE_STACK_MAX - 1;
    }
    noteStack_loc[v][noteCount_loc[v]++] = pitch;
    currentNote_loc[v] = pitch;
}

static bool noteStack_remove(uint8_t v, uint8_t pitch) {
    int found = -1;
    for (uint8_t i = 0; i < noteCount_loc[v]; i++)
        if (noteStack_loc[v][i] == pitch) { found = i; break; }
    if (found < 0) return false;

    for (uint8_t i = found; i < noteCount_loc[v] - 1; i++)
        noteStack_loc[v][i] = noteStack_loc[v][i + 1];
    noteCount_loc[v]--;

    if (noteCount_loc[v] > 0)
        currentNote_loc[v] = noteStack_loc[v][noteCount_loc[v] - 1];
    return true;
}

static void noteStack_clear(uint8_t v) {
    noteCount_loc[v] = 0;
}

// -------------------------------------------------------------------------
// 4. FREQUENZA / GLIDE / OTTAVA
// -------------------------------------------------------------------------
static inline void setOttava_loc(uint8_t v, int ott) {
    switch (ott) {
        case 3:  oct_sw_loc[v] = 4; break;
        case 2:  oct_sw_loc[v] = 2; break;
        case 1:  oct_sw_loc[v] = 1; break;
        default: oct_sw_loc[v] = 2; break;
    }
    if (noteCount_loc[v] > 0) {
        // ricalcola freq se c'è una nota attiva
        // (definite sotto, forward implicit)
    }
}

// Ritorna la frequenza base (sub_voce 0) della voce v per il pitch dato.
// pitch = MIDI 0..127.
static float calcBaseFreq(uint8_t v, uint8_t pitch) {
    int n = (int)pitch - 24;
    if (n < 0)                    n = 0;
    if (n > MAX_NOTEARRAY_IDX)    n = MAX_NOTEARRAY_IDX;
    int ot = (int)noteArr[ottava_loc[v] - 1][n] + (int)calb;
    if (ot > 1225) ot = 1225;
    return 256.0f * freq_table[ot] / 122070.0f * (float)oct_sw_loc[v];
}

static void updateVoceTarget(uint8_t v) {
    if (noteCount_loc[v] == 0) {
        // nessuna nota: target 0 (l'ampiezza è gestita dall'ADSR in release)
        glideTarget_loc[v] = 0.0f;
        return;
    }
    glideTarget_loc[v] = calcBaseFreq(v, currentNote_loc[v]);
}

static void updateGlideStep(uint8_t v) {
    if (slide_loc[v] == 0) {
        glideCurrent_loc[v] = glideTarget_loc[v];
        glideStep_loc[v]    = 0.0f;
        return;
    }
    // tempo di glide 1ms..1s (parametro 0..99)
    float t = 0.001f + (float)tempoSlide_loc[v] / 99.0f * 1.0f;
    float diff = fabsf(glideTarget_loc[v] - glideCurrent_loc[v]);
    glideStep_loc[v] = diff / (t * PWM_IRQ_RATE_HZ);
    if (glideStep_loc[v] < 1e-6f) glideStep_loc[v] = 1e-6f;
}

// -------------------------------------------------------------------------
// 5. WAVETABLE SETUP per-voce
// -------------------------------------------------------------------------
static void wavetable_setup_voce(uint8_t v) {
    for (uint8_t s = 0; s < SUBVOCI_PER_VOCE; s++) {
        uint8_t wf = waveform_loc[v][s];
        int *wt = wavetable_loc[v][s];

        if (mode_loc[v] == 0) {
            // ---------- WAVEFOLD ----------
            switch (wf) {
                case 0: // SAW
                    for (int i = 0; i < 256; i++) wt[i] = i * 4 - (int)sampleLev + 1;
                    break;
                case 1: // SINE
                    for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2 * i / 256.0f) * sampleLev);
                    break;
                case 2: // SQR
                    for (int i = 0; i < 128; i++) { wt[i] = (int)sampleLev; wt[i + 128] = -(int)sampleLev; }
                    break;
                case 3: // TRI
                    for (int i = 0; i < 128; i++) {
                        wt[i]       = i * 8 - (int)sampleLev;
                        wt[i + 128] = (int)sampleLev - i * 8;
                    }
                    break;
                case 4: // OCT-SAW
                    for (int i = 0; i < 128; i++) {
                        wt[i]       = i * 4 - ((int)sampleLev + 1) + i * 2;
                        wt[i + 128] = i * 2 - (((int)sampleLev + 1) / 2) + i * 4;
                    }
                    break;
                case 5: // FM1
                    for (int i = 0; i < 256; i++)
                        wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*3.0f*i/256.0f)) * sampleLev);
                    break;
                case 6: // FM2
                    for (int i = 0; i < 256; i++)
                        wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*7.0f*i/256.0f)) * sampleLev);
                    break;
                case 7: // FM3
                    for (int i = 0; i < 256; i++)
                        wt[i] = (int)(sinf(PIx2*i/256.0f +
                                           sinf(PIx2*4.0f*i/256.0f +
                                                sinf(PIx2*11.0f*i/256.0f))) * sampleLev);
                    break;
                case 8: // NOISE
                    for (int i = 0; i < 256; i++)
                        wt[i] = random(511, 1020) - (int)sampleLev;
                    break;
                default:
                    for (int i = 0; i < 256; i++) wt[i] = 0;
                    break;
            }
            attenua_loc[v] = (wf == 2) ? 6 : 5;   // (per ora non usato in IRQ)
        }
        else if (mode_loc[v] == 2) {
            // ---------- AM ----------
            switch (wf) {
                case 0: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f) * sampleLev); break;
                case 1: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*3.0f*i/256.0f)) * sampleLev); break;
                case 2: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*5.0f*i/256.0f)) * sampleLev); break;
                case 3: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*4.0f*i/256.0f + sinf(PIx2*11.0f*i/256.0f))) * sampleLev); break;
                case 4: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*1.28f*i/256.0f)) * sampleLev); break;
                case 5: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*3.19f*i/256.0f)) * sampleLev); break;
                case 6: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*2.3f*i/256.0f + sinf(PIx2*7.3f*i/256.0f))) * sampleLev); break;
                case 7: for (int i = 0; i < 256; i++) wt[i] = (int)(sinf(PIx2*i/256.0f + sinf(PIx2*6.3f*i/256.0f + sinf(PIx2*11.3f*i/256.0f))) * sampleLev); break;
                case 8: for (int i = 0; i < 256; i++) wt[i] = random(511, 1020) - (int)sampleLev + 1; break;
                default: for (int i = 0; i < 256; i++) wt[i] = 0; break;
            }
            attenua_loc[v] = 5;
        }
        else {
            // ---------- FM ---------- (mod2_wavetable è riempita in loop1)
            for (int i = 0; i < 256; i++) wt[i] = 0;
            attenua_loc[v] = 5;
        }
    }

    // Copia diretta in mod2 (sarà aggiornata da loop1 ad ogni tick)
    for (uint8_t s = 0; s < SUBVOCI_PER_VOCE; s++)
        for (int i = 0; i < 256; i++)
            mod2_wavetable_loc[v][s][i] = wavetable_loc[v][s][i];
}

// -------------------------------------------------------------------------
// 6. NOTE HANDLING
// -------------------------------------------------------------------------
static void onNoteOn(uint8_t v, uint8_t pitch, uint8_t velocity) {
    UNUSED(velocity);
    bool wasSilent = (noteCount_loc[v] == 0);

    noteStack_push(v, pitch);

    if (wasSilent) {
        // prima nota: parte l'envelope
        adsr_noteOn(v);
    }
    updateVoceTarget(v);
    updateGlideStep(v);
}

static void onNoteOff(uint8_t v, uint8_t pitch) {
    noteStack_remove(v, pitch);

    if (noteCount_loc[v] == 0) {
        // ultima nota rilasciata: parte il release
        adsr_noteOff(v);
    }
    updateVoceTarget(v);
    updateGlideStep(v);
}

static void onAllNotesOff(uint8_t v) {
    noteStack_clear(v);
    adsr_noteOff(v);
}

// -------------------------------------------------------------------------
// 7. IRQ PWM WRAP — generazione audio
// -------------------------------------------------------------------------
// Somma i contributi di tutte le voci locali (o 1 o 2) e delle loro 2 sub_voci.
// Applica ADSR + voiceVol + subLevel, clampa e scrive il registro PWM.
//
// NB: mod2_wavetable_loc viene scritta da loop1() (core 1). C'è una
// possibile race; non peggiora rispetto all'originale, ma va tenuta a mente.
// -------------------------------------------------------------------------
void on_pwm_wrap() {
    pwm_clear_irq(slice_num);

    int32_t levelOut = 0;

    for (uint8_t v = 0; v < NUM_VOCI; v++) {
        // --- ADSR ---
        adsr_tick(v);

        // --- Glide ---
        if (slide_loc[v] == 1 && glideStep_loc[v] > 0.0f) {
            if (glideCurrent_loc[v] < glideTarget_loc[v]) {
                glideCurrent_loc[v] += glideStep_loc[v];
                if (glideCurrent_loc[v] > glideTarget_loc[v])
                    glideCurrent_loc[v] = glideTarget_loc[v];
            } else if (glideCurrent_loc[v] > glideTarget_loc[v]) {
                glideCurrent_loc[v] -= glideStep_loc[v];
                if (glideCurrent_loc[v] < glideTarget_loc[v])
                    glideCurrent_loc[v] = glideTarget_loc[v];
            }
        } else {
            glideCurrent_loc[v] = glideTarget_loc[v];
        }

        // --- Frequenza sub_voci ---
        // baseFreq include bend + mod pitch come fattore esponenziale
        float baseFreq = glideCurrent_loc[v];
        if (frBend_offset_loc[v] != 0.0f || frMod_offset_loc[v] != 0.0f) {
            float bendAmt = frBend_offset_loc[v] * 2.0f + frMod_offset_loc[v];
            baseFreq *= powf(2.0f, bendAmt / 12.0f);
        }
        osc_freq_loc[v][0] = baseFreq;
        osc_freq_loc[v][1] = baseFreq * intervalMul[sub_interval[v] + 24];

        // --- Pesi ---
        int env_mul = (int)(envLevel_loc[v] * 256.0f);
        int vol_mul = (int)(levArr[voiceVol_idx[v]] * 256.0f);

        for (uint8_t s = 0; s < SUBVOCI_PER_VOCE; s++) {
            // Fase
            f_loc[v][s] += osc_freq_loc[v][s];
            while (f_loc[v][s] >= 256.0f) f_loc[v][s] -= 256.0f;
            while (f_loc[v][s] < 0.0f)    f_loc[v][s] += 256.0f;

            // Campione
            int32_t samp = mod2_wavetable_loc[v][s][(uint8_t)f_loc[v][s]];

            // Peso sub_voce
            int sub_mul = (s == 0) ? 256 : (int)(levArr[sub_level_idx[v]] * 256.0f);

            // Applica i tre pesi progressivamente (shifts)
            int32_t sval = samp;
            sval = (sval * env_mul) >> 8;
            sval = (sval * vol_mul) >> 8;
            sval = (sval * sub_mul) >> 8;

            levelOut += sval;
        }
    }

    // Clamp al range PWM (il centro è 512)
    if (levelOut >  512) levelOut =  512;
    if (levelOut < -512) levelOut = -512;

    pwm_set_chan_level(slice_num, PWM_CHAN_B, (uint16_t)(levelOut + 512));
}
#pragma once
#include "synthConfig.h"
#include "synthState.h"

// =========================================================================
// synthControl.h — Modulazioni, arpeggiator, gater, VCF LFO
// =========================================================================
// Contiene:
//   - arp_step()     per-voce
//   - gater_step()   per-voce
//   - lfoTick_all()  LFO mod + LFO pitch + trigger arp/gater
//   - vcfTick_all()  VCF LFO (per ora calcolo interno, target futuro)
//
// Dipende da: synthConfig.h, synthState.h, synthEngine.h
// =========================================================================

// -------------------------------------------------------------------------
// 1. VCF LFO — output locale (usato in futuro dal "nuovo modo VCF")
// -------------------------------------------------------------------------
// Nel codice originale il VCF era esterno (Vermona) e veniva pilotato via
// MIDI CC. Il forward Vermona è stato rimosso, quindi per ora il VCF LFO
// calcola internamente un valore 0..127 che non viene inviato a nessuno.
// Sarà disponibile per il nuovo modo VCF quando lo implementeremo.
static uint8_t vcfLfoOut_loc[NUM_VOCI] = {};

// Stato interno del VCF LFO (per-voce)
static int     vcfConta_loc[NUM_VOCI]  = {};
static uint8_t vcfVerso_loc[NUM_VOCI]  = {};   // per TRI
static int     vcfRndHold_loc[NUM_VOCI] = {};

// -------------------------------------------------------------------------
// 2. ARP STEP (per-voce)
// -------------------------------------------------------------------------
static void arp_step(uint8_t v) {
    if (!arp_on_loc[v])     return;
    if (noteCount_loc[v] == 0) return;

    int8_t maxIdx = (int8_t)(noteCount_loc[v] - 1);

    switch (arp_mode_loc[v]) {
        case 0: // >> su
            arp_step_loc[v]++;
            if (arp_step_loc[v] > maxIdx) {
                arp_step_loc[v] = 0;
                arp_octave_loc[v]++;
                if (arp_octave_loc[v] > arp_octaves_loc[v])
                    arp_octave_loc[v] = 0;
            }
            break;

        case 1: // << giù
            arp_step_loc[v]--;
            if (arp_step_loc[v] < 0) {
                arp_step_loc[v] = maxIdx;
                if (arp_octave_loc[v] == 0) arp_octave_loc[v] = arp_octaves_loc[v];
                else                        arp_octave_loc[v]--;
            }
            break;

        case 2: // >< ping-pong
            if (arp_dir_loc[v] == 0) {
                arp_step_loc[v]++;
                if (arp_step_loc[v] > maxIdx) {
                    arp_step_loc[v] = (maxIdx > 0) ? maxIdx - 1 : 0;
                    arp_dir_loc[v]  = 1;
                }
            } else {
                arp_step_loc[v]--;
                if (arp_step_loc[v] < 0) {
                    arp_step_loc[v] = (maxIdx > 0) ? 1 : 0;
                    arp_dir_loc[v]  = 0;
                }
            }
            break;
    }

    // Applica nota corrente + ottava
    uint8_t basePitch = noteStack_loc[v][(uint8_t)arp_step_loc[v]];
    uint16_t playPitch = (uint16_t)basePitch + arp_octave_loc[v] * 12;
    if (playPitch > MAX_PITCH) playPitch = MAX_PITCH;

    currentNote_loc[v] = (uint8_t)playPitch;
    updateVoceTarget(v);
    updateGlideStep(v);
}

// -------------------------------------------------------------------------
// 3. GATER STEP (per-voce)
// -------------------------------------------------------------------------
static void gater_step(uint8_t v) {
    if (!gater_on_loc[v]) return;

    uint8_t pattern = gater_num_loc[v] & 0x07;   // 0..7
    uint8_t step    = gater_step_loc[v];

    if (gaterArr[pattern][step] == 1) {
        // Gate aperto: riattiva ADSR (breve riattacco)
        adsr_noteOn(v);
    } else {
        // Gate chiuso: rilascio
        adsr_noteOff(v);
    }
    gater_step_loc[v] = (gater_step_loc[v] + 1) & 0x0F;   // wrap 0..15
}

// -------------------------------------------------------------------------
// 4. LFO TICK — mod + pitch + trigger arp/gater
// -------------------------------------------------------------------------
static void lfoTick_all() {
    unsigned long now = micros();

    for (uint8_t v = 0; v < NUM_VOCI; v++) {

        // -------------------------------------------------------------
        // LFO mod (timbro) — può anche triggherare arp/gater
        // -------------------------------------------------------------
        if ((now - prevTimeMod_loc[v]) > speedMod_loc[v] &&
            (modLev_loc[v] > 1 || arp_on_loc[v] || gater_on_loc[v])) {

            contaMod_loc[v]++;
            if (contaMod_loc[v] > 255) contaMod_loc[v] = 0;

            // Arp trigger
            if (arp_on_loc[v]) {
                uint8_t mult = (uint8_t)arpMultiArr[arp_multi_loc[v]];
                if (mult > 0 && ((contaMod_loc[v] + 1) % mult) == 0) arp_step(v);
            }

            // Gater trigger
            if (gater_on_loc[v]) {
                uint8_t mult = (uint8_t)arpMultiArr[gater_multi_loc[v]];
                if (mult > 0 && ((contaMod_loc[v] + 1) % mult) == 0) gater_step(v);
            }

            // Applica modulazione LFO mod
            int modVal = sineModArr[contaMod_loc[v]];
            // map(value, -511, 511, modLevA, modLev) in float
            float modA = (float)map(modVal, -511, 511,
                                    modLevA_loc[v], modLev_loc[v]);
            modIn_loc[v] = (modLev_loc[v] > 1) ? (int)modA : 0;

            prevTimeMod_loc[v] = now;
        }

        // -------------------------------------------------------------
        // LFO pitch (vibrato)
        // -------------------------------------------------------------
        if ((now - prevTimePitchMod_loc[v]) > speedPitchMod_loc[v] &&
            modPitchLev_loc[v] > 2) {

            contaPitchMod_loc[v]++;
            if (contaPitchMod_loc[v] > 255) contaPitchMod_loc[v] = 0;

            int modPVal = sinePitchModArr[contaPitchMod_loc[v]];
            float modP = (float)map(modPVal, -511, 511,
                                    modPitchLevA_loc[v], modPitchLev_loc[v])
                         / maxModPitchLev;

            frMod_offset_loc[v] = modP;
            prevTimePitchMod_loc[v] = now;
        }
    }
}

// -------------------------------------------------------------------------
// 5. VCF TICK — LFO dedicato al VCF (per ora calcolo interno)
// -------------------------------------------------------------------------
static void vcfTick_all() {
    for (uint8_t v = 0; v < NUM_VOCI; v++) {
        uint8_t speed = speedVcfLfo_loc[v];
        if (speed == 0) continue;

        // Avanza contatore con la sua velocità (speed è 0..255 → periodo)
        // NB: il timing del tick è gestito dal chiamante; qui adottiamo una
        // approssimazione "una chiamata per step" analoga al loop originale.
        int lev = lfoVcfLev_loc[v];
        if (lev == 0) continue;

        switch (vcfWave_loc[v]) {
            case 0: { // SINE
                vcfConta_loc[v]++;
                if (vcfConta_loc[v] > 511) vcfConta_loc[v] = 0;
                float s = sinf(PIx2 * vcfConta_loc[v] / 512.0f);
                int out = (int)((s * 0.5f + 0.5f) * (float)lev);
                vcfLfoOut_loc[v] = (uint8_t)constrain(out, 0, 255);
                break;
            }
            case 1: { // TRI
                if (vcfVerso_loc[v] == 0) {
                    vcfConta_loc[v]++;
                    if (vcfConta_loc[v] >= 255) { vcfConta_loc[v] = 255; vcfVerso_loc[v] = 1; }
                } else {
                    vcfConta_loc[v]--;
                    if (vcfConta_loc[v] <= 0) { vcfConta_loc[v] = 0; vcfVerso_loc[v] = 0; }
                }
                vcfLfoOut_loc[v] = (uint8_t)map(vcfConta_loc[v], 0, 255, 0, lev);
                break;
            }
            case 2: { // SAW
                vcfConta_loc[v]++;
                if (vcfConta_loc[v] > 511) vcfConta_loc[v] = 0;
                vcfLfoOut_loc[v] = (uint8_t)map(vcfConta_loc[v], 0, 511, 0, lev);
                break;
            }
            case 3: { // R-SAW
                vcfConta_loc[v]--;
                if (vcfConta_loc[v] < 0) vcfConta_loc[v] = 511;
                vcfLfoOut_loc[v] = (uint8_t)map(vcfConta_loc[v], 0, 511, 0, lev);
                break;
            }
            case 4: { // RND
                vcfConta_loc[v]++;
                if (vcfConta_loc[v] > 511) {
                    vcfConta_loc[v] = 0;
                    vcfRndHold_loc[v] = random(255);
                }
                vcfLfoOut_loc[v] = (uint8_t)map(vcfRndHold_loc[v], 0, 255, 0, lev);
                break;
            }
        }
    }
}
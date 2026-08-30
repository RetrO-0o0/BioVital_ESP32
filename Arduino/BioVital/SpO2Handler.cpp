#include "SpO2Handler.hpp"
#include <math.h>

float irACBuffer[WINDOW_SIZE];
float redACBuffer[WINDOW_SIZE];
int sampleIndex = 0;
int sampleCount = 0;

float dcIR = 0;
float dcRed = 0;
float prevRawIR = 0;
float prevRawRed = 0;
float prevACIR = 0;
float prevACRed = 0;

float spo2_final_result = 0.0f;
int spo2_ui_state = 0;
int spo2_progress = 0;
float spo2_dc_warmup_sec = 0.0f;

void resetSignalProcessingSpO2() {
    sampleIndex = 0;
    sampleCount = 0;
    dcIR = 0;
    dcRed = 0;
    prevRawIR = 0;
    prevRawRed = 0;
    prevACIR = 0;
    prevACRed = 0;
    spo2_final_result = 0.0f;
    spo2_ui_state = 0;
    spo2_progress = 0;
    spo2_dc_warmup_sec = 0.0f;
}

void processSpO2(uint32_t irRaw, uint32_t redRaw) {
    if (irRaw < 30000) {
        resetSignalProcessingSpO2();
        return;
    }

    if (dcIR == 0) {
        dcIR = irRaw;
        dcRed = redRaw;
        prevRawIR = irRaw;
        prevRawRed = redRaw;
        return;
    }

    dcIR = 0.99f * dcIR + 0.01f * irRaw;
    dcRed = 0.99f * dcRed + 0.01f * redRaw;

    float acIR = 0.95f * (prevACIR + (float)irRaw - prevRawIR);
    float acRed = 0.95f * (prevACRed + (float)redRaw - prevRawRed);

    prevRawIR = irRaw;
    prevRawRed = redRaw;
    prevACIR = acIR;
    prevACRed = acRed;

    if (sampleCount < 300) {
        sampleCount++;
        spo2_ui_state = 1; 
        spo2_dc_warmup_sec = (300 - sampleCount) / 100.0f;
        return;
    }

    irACBuffer[sampleIndex] = acIR;
    redACBuffer[sampleIndex] = acRed;

    sampleIndex = (sampleIndex + 1) % WINDOW_SIZE;
    if (sampleCount < WINDOW_SIZE) sampleCount++;

    spo2_progress = (sampleCount * 100) / WINDOW_SIZE;

    if (sampleCount < WINDOW_SIZE) {
        spo2_ui_state = 2; 
    } else {
        spo2_ui_state = 3; 

        float sumSqIR = 0, sumSqRed = 0;
        for (int i = 0; i < WINDOW_SIZE; i++) {
            sumSqIR += irACBuffer[i] * irACBuffer[i];
            sumSqRed += redACBuffer[i] * redACBuffer[i];
        }

        float rmsIR = sqrt(sumSqIR / WINDOW_SIZE);
        float rmsRed = sqrt(sumSqRed / WINDOW_SIZE);

        if (dcIR > 0 && dcRed > 0 && rmsRed > 0) {
            float R = (rmsIR / dcIR) / (rmsRed / dcRed);
            float calculatedSpO2 = -45.060f * R * R + 30.354f * R + 94.845f;

            if (calculatedSpO2 > 100.0f) calculatedSpO2 = 100.0f;
            if (calculatedSpO2 < 0.0f) calculatedSpO2 = 0.0f;

            spo2_final_result = calculatedSpO2;
        }
    }
}


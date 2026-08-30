#include "BPMHandler.hpp"

IIRFilter::IIRFilter(float b0, float b1, float b2, float a1, float a2) {
    b[0] = b0; b[1] = b1; b[2] = b2;
    a[0] = a1; a[1] = a2;
}

float IIRFilter::process(float in) {
    x[0] = x[1]; x[1] = x[2]; x[2] = in;
    y[0] = y[1]; y[1] = y[2];
    y[2] = b[0]*x[2] + b[1]*x[1] + b[2]*x[0] - a[0]*y[1] - a[1]*y[0];
    return y[2];
}

void IIRFilter::reset() {
    x[0] = x[1] = x[2] = 0;
    y[0] = y[1] = y[2] = 0;
}

float MovingAverage::process(float in) {
    sum -= buf[idx];
    buf[idx] = in;
    sum += in;
    idx = (idx + 1) % N;
    return sum / N;
}

void MovingAverage::reset() {
    for (int i = 0; i < N; i++) buf[i] = 0;
    sum = 0;
    idx = 0;
}

float PeakDetector::process(float x) {
    unsigned long now = millis();
    if (prev_x < 0 && x >= 0) {
        if (lastPeakTime != 0) {
            float dt = now - lastPeakTime;
            if (dt >= 333 && dt <= 1500) {
                float currentBPM = 60000.0f / dt;
                float allowedMaxBPM = 60000.0f / (smoothedPeakInterval - 30);
                float allowedMinBPM = 60000.0f / (smoothedPeakInterval + 30);
                if (currentBPM >= allowedMinBPM && currentBPM <= allowedMaxBPM) {
                    smoothedPeakInterval = 0.8f * smoothedPeakInterval + 0.2f * dt;
                    lastValidBPM = 60000.0f / smoothedPeakInterval;
                }
            }
        }
        lastPeakTime = now;
    }
    prev_x = x;
    return lastValidBPM;
}

void PeakDetector::reset() {
    prev_x = 0;
    lastPeakTime = 0;
    smoothedPeakInterval = 800.0f;
    lastValidBPM = 0;
}

void Smoothing::addSample(float val) {
    if (val <= 0) return;
    sum -= buf[idx];
    buf[idx] = val;
    sum += val;
    idx = (idx + 1) % N;
    if (count < N) count++;
}

float Smoothing::getAverage() {
    if (count == 0) return 0;
    return sum / count;
}

void Smoothing::reset() {
    for (int i = 0; i < N; i++) buf[i] = 0;
    sum = 0;
    idx = 0;
    count = 0;
}

IIRFilter highPass(0.978f, -1.956f, 0.978f, -1.9556f, 0.9565f);
IIRFilter lowPass(0.01336f, 0.02672f, 0.01336f, -1.6475f, 0.7009f);
MovingAverage movAvg;
PeakDetector peakDet;
Smoothing bpmSmoothing;

const unsigned long WARM_UP_INTERVAL = 6000;
const unsigned long DISPLAY_INTERVAL = 3000;

unsigned long bpm_startTime = 0;
unsigned long bpm_lastDisplayTime = 0;
int bpm_ui_state = 0;
float bpm_final_result = 0.0f;

void resetSignalProcessingBPM() {
    highPass.reset();
    lowPass.reset();
    movAvg.reset();
    peakDet.reset();
    bpmSmoothing.reset();
    bpm_startTime = millis();
    bpm_lastDisplayTime = millis();
    bpm_ui_state = 0;
    bpm_final_result = 0.0f;
}

void processBPM(uint32_t irRaw) {
    if (irRaw < 30000) {
        resetSignalProcessingBPM();
        return;
    }

    unsigned long now = millis();
    float hp = highPass.process((float)irRaw);
    float lp = lowPass.process(hp);
    float ma = movAvg.process(lp);
    float currentBPM = peakDet.process(ma);

    if (now - bpm_startTime < WARM_UP_INTERVAL) {
        bpm_ui_state = 1; 
    } else {
        bpm_ui_state = 2; 
        if (currentBPM > 0) {
            bpmSmoothing.addSample(currentBPM);
        }
        if (now - bpm_lastDisplayTime >= DISPLAY_INTERVAL) {
            bpm_lastDisplayTime = now;
            bpm_final_result = bpmSmoothing.getAverage();
        }
    }
}


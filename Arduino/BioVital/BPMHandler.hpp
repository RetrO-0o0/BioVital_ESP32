#ifndef BPM_HANDLER_HPP
#define BPM_HANDLER_HPP

#include <Arduino.h>

class IIRFilter {
private:
    float b[3], a[2];
    float x[3] = {0}, y[3] = {0};
public:
    IIRFilter(float b0, float b1, float b2, float a1, float a2);
    float process(float in);
    void reset();
};

class MovingAverage {
private:
    static const int N = 10;
    float buf[N] = {0};
    int idx = 0;
    float sum = 0;
public:
    float process(float in);
    void reset();
};

class PeakDetector {
private:
    float prev_x = 0;
    unsigned long lastPeakTime = 0;
    float smoothedPeakInterval = 800.0f;
    float lastValidBPM = 0;
public:
    float process(float x);
    void reset();
};

class Smoothing {
private:
    static const int N = 5;
    float buf[N] = {0};
    int idx = 0;
    float sum = 0;
    int count = 0;
public:
    void addSample(float val);
    float getAverage();
    void reset();
};

extern IIRFilter highPass;
extern IIRFilter lowPass;
extern MovingAverage movAvg;
extern PeakDetector peakDet;
extern Smoothing bpmSmoothing;

extern unsigned long bpm_startTime;
extern unsigned long bpm_lastDisplayTime;
extern int bpm_ui_state; 
extern float bpm_final_result;

void resetSignalProcessingBPM();
void processBPM(uint32_t irRaw);

#endif // BPM_HANDLER_HPP
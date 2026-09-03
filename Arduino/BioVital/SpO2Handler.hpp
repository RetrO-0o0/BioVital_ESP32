#pragma once

#include <Arduino.h>
#include <math.h>

class SpO2Handler 
{
private:
    static constexpr int WINDOW_SIZE = 1000;
    float                irACBuffer[WINDOW_SIZE];
    float                redACBuffer[WINDOW_SIZE];
    
    int   sampleCount;
    int   warmupCount;
    
    float dcIR;
    float dcRed;
    float w_ir;
    float w_red;
    
    bool  is_initialized;
    
    int   spo2_ui_state;      // 0: No Finger, 1: Stabilizing, 2: Recording, 3: Result
    float spo2_final_result;
    int   spo2_progress;

public:
    SpO2Handler();

    void reset();
    void process(long irRaw, long redRaw);

    // Getters برای استفاده در UI
    int   getUIState() const;
    float getFinalResult() const;
    int   getProgress() const;
    int   getWarmupRemainingSeconds() const;
};

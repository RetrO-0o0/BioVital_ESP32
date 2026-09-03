#pragma once

#include <Arduino.h>
#include <stdint.h>

// -----------------------------------------------
// Class IIRFilter
// -----------------------------------------------
class IIRFilter
{
private:
    double b0, b1, b2;
    double a1, a2;

    double x1 {0};
    double x2 {0};
    double y1 {0};
    double y2 {0};

public:
    IIRFilter(double b0_, double b1_, double b2_, double a1_, double a2_);

    double process(float in);

    void reset();
};

// -----------------------------------------------
// Class MovingAverage
// -----------------------------------------------
class MovingAverage
{
private:

    static constexpr int N           {10};
    double               buffer[N] = {0.0};
    double               sum         {0.0};
    uint8_t              idx         {0};

public:

    double process(double input);

    void reset();
};

// -----------------------------------------------
// Class PeakDetector
// -----------------------------------------------
class PeakDetector
{
private:

    double        lastSample     {0.0};
    unsigned long lastPeakTime   {0};
    double        calculatedBPM  {75.0};
    bool          initialized    {false};

public:

    bool process(double sample);

    double getBPM() const;

    void reset();
};

// -----------------------------------------------
// Class Smoothing
// -----------------------------------------------
class Smoothing
{
private:

    static constexpr int N           {5};
    double               buffer[N] = {0.0};
    double               sum         {0.0};
    double               avg         {0.0};
    uint8_t              idx         {0};

public:
    void process(PeakDetector input);

    void reset();

    double getAvg() const;
};

// -----------------------------------------------
// Objects and Variables
// -----------------------------------------------
extern IIRFilter     highPass;
extern IIRFilter     lowPass;
extern MovingAverage movAvg;
extern PeakDetector  peakDet;
extern Smoothing     bpmSmoothing;

extern unsigned long       bpm_startTime;
extern unsigned long       bpm_lastDisplayTime;
extern uint8_t             bpm_ui_state;
extern double              bpm_final_result;
extern const unsigned long WARM_UP_INTERVAL;
extern const unsigned long DISPLAY_INTERVAL;

// -----------------------------------------------
// Functions
// -----------------------------------------------
void resetSignalProcessingBPM();
void processBPM(uint32_t irRaw);
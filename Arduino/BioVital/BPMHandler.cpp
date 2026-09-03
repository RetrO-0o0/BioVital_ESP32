#include "BPMHandler.hpp"

// -----------------------------------------------
// Class IIRFilter
// -----------------------------------------------
IIRFilter::IIRFilter(double b0_, double b1_, double b2_, double a1_, double a2_)
{
    this->b0 = b0_;
    this->b1 = b1_;
    this->b2 = b2_;
    this->a1 = a1_;
    this->a2 = a2_;
}

double IIRFilter::process(float in)
{
    double x = static_cast<double>(in);

    double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

    this->x2 = this->x1;
    this->x1 = x;
    this->y2 = this->y1;
    this->y1 = y;

    return y;
}

void IIRFilter::reset()
{
    this->x1 = 0.0;
    this->x2 = 0.0;
    this->y1 = 0.0;
    this->y2 = 0.0;
}

// -----------------------------------------------
// Class MovingAverage
// -----------------------------------------------
double MovingAverage::process(double in) 
{
    this->sum -= this->buffer[this->idx];

    this->buffer[this->idx] = in;
    
    this->sum += in;
    
    this->idx = (this->idx + 1) % this->N;
    
    return static_cast<double>(this->sum / this->N);
}

void MovingAverage::reset() 
{
    for (int i = 0; i < this->N; i++)
        this->buffer[i] = 0.0;

    this->sum = 0.0;
    this->idx = 0;
}

// -----------------------------------------------
// Class PeakDetector
// -----------------------------------------------
bool PeakDetector::process(double sample)
    {
        // First initialization
        // Don't process the first sample
        if (!this->initialized)
        {
            this->lastSample  = sample;
            this->initialized = true;
            return false;
        }

        // Zero crossing
        bool peak = (this->lastSample < 0.0) && (sample >= 0.0);
        
        if (peak)
        {
            unsigned long currentTime = millis();

            if (this->lastPeakTime != 0)
            {
                unsigned long interval = currentTime - this->lastPeakTime;
                // char buff[50];
                // sprintf(buff, "Interval in peakDetector: %lu", interval);
                // serial.println(buff);
                
                // ------------------------------------------------
                // Physiological BPM range:
                // 40 BPM -> 1500 ms
                // 180 BPM -> 333 ms
                // ------------------------------------------------
                if (interval > 333 && interval < 1500)
                {
                    double instantaneousBPM = 60000.0f / static_cast<double>(interval);
                    // sprintf(buff, "rawBPM: %f", instantaneousBPM);
                    
                    // --------------------------------------------
                    // Rate limiter
                    // Maximum change = 3 BPM per beat
                    // --------------------------------------------
                    const double MAX_CHANGE = 3.0;

                    if (instantaneousBPM > this->calculatedBPM + MAX_CHANGE)
                    {
                        this->calculatedBPM += MAX_CHANGE;
                    }
                    else if (instantaneousBPM < this->calculatedBPM - MAX_CHANGE)
                    {
                        this->calculatedBPM -= MAX_CHANGE;
                    }
                    else
                    {
                        // Exponential smoothing
                        this->calculatedBPM = 0.8f * this->calculatedBPM + 0.2f * instantaneousBPM;
                    }

                    this->lastPeakTime = currentTime;
                    this->lastSample   = sample;

                    return true;
                }
                // Deadlock revival
                else if (interval >= 1500)
                {
                    this->lastPeakTime = currentTime; 
                }
            }
            else
            {
                // First valid peak
                this->lastPeakTime = currentTime;
            }
        }

        this->lastSample = sample;

        return false;
    }

void PeakDetector::reset() 
{
    this->lastSample    = 0.0;
    this->lastPeakTime  = 0;
    this->calculatedBPM = 75.0;
    this->initialized   = false;
}

double PeakDetector::getBPM() const
{
    return this->calculatedBPM;
}

// -----------------------------------------------
// Class Smoothing
// -----------------------------------------------
void Smoothing::process(PeakDetector input)
{
    this->sum -= buffer[this->idx];
    
    this->buffer[this->idx] = input.getBPM();
    
    this->sum += input.getBPM();
    
    this->idx = (this->idx + 1) % this->N;
    
    this->avg =  this->sum / this->N;
}

double Smoothing::getAvg() const
{
    return this->avg;
}

void Smoothing::reset() 
{
    for (int i = 0; i < this->N; i++)
        this->buffer[i] = 0.0;

    this->sum = 0.0;
    this->idx = 0;
}

// -----------------------------------------------
// Objects
// -----------------------------------------------
IIRFilter highPass(
    0.978030479,
   -1.956060958,
    0.978030479,
   -1.955578240,
    0.956543676
);

IIRFilter lowPass(
    0.013359200,
    0.026718400,
    0.013359200,
   -1.647459981,
    0.700896781
);

MovingAverage movAvg;
PeakDetector  peakDet;
Smoothing     bpmSmoothing;

// -----------------------------------------------
// Variables
// -----------------------------------------------
uint8_t             bpm_ui_state        {0};   // 0: no finger | 1: Warmup | 2: Ready
double              bpm_final_result    {0.0};
unsigned long       bpm_startTime       {0};
const unsigned long WARM_UP_INTERVAL    {6000}; // Warmup for 6s
const unsigned long DISPLAY_INTERVAL    {3000}; // Display after 3s
unsigned long       bpm_lastDisplayTime {0};

// -----------------------------------------------
// Functions
// -----------------------------------------------
void resetSignalProcessingBPM() 
{
    highPass.reset();
    lowPass.reset();
    movAvg.reset();
    peakDet.reset();
    bpmSmoothing.reset();

    bpm_startTime = millis();
    bpm_lastDisplayTime = millis();

    bpm_ui_state = 0;
    bpm_final_result = 0.0;
}

void processBPM(uint32_t irRaw) 
{
    if (irRaw < 30000) 
    {
        resetSignalProcessingBPM();
        return;
    }

    unsigned long now = millis();
    
    double hp         = highPass.process(static_cast<double>(irRaw)); // HIGH-PASS
    double lp         = lowPass.process(hp);                          // LOW-PASS
    double ma         = movAvg.process(lp);                           // MOVING AVERAGE
    bool   currentBPM = peakDet.process(ma);                          // PEAK DETECTION

    if (now - bpm_startTime < WARM_UP_INTERVAL) 
    {
        bpm_ui_state = 1; 
    } 
    else 
    {
        bpm_ui_state = 2;

        if (currentBPM) 
        {
            bpmSmoothing.process(peakDet);
        }

        if (now - bpm_lastDisplayTime >= DISPLAY_INTERVAL) 
        {
            bpm_lastDisplayTime = now;

            bpm_final_result = bpmSmoothing.getAvg();
        }
    }
}
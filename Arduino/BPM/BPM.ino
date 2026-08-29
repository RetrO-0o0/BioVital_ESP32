#include <Wire.h>
#include "MAX30105.h"

#define serial Serial

class IIRFilter
{
private:
    float b0, b1, b2;
    float a1, a2;
    float x1 = 0;
    float x2 = 0;
    float y1 = 0;
    float y2 = 0;

public:
    IIRFilter(float b0_, float b1_, float b2_, float a1_, float a2_)
    {
        b0 = b0_;
        b1 = b1_;
        b2 = b2_;
        a1 = a1_;
        a2 = a2_;
    }

    float process(float x)
    {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }

    void reset()
    {
        x1 = 0;
        x2 = 0;
        y1 = 0;
        y2 = 0;
    }
};

class MovingAverage
{
private:
    static constexpr int WINDOW = 10;
    float buffer[WINDOW] = {0};
    float sum = 0;
    uint8_t idx = 0;

public:
    float process(float input)
    {
        sum -= buffer[idx];
        buffer[idx] = input;
        sum += input;
        idx = (idx + 1) % WINDOW;
        return sum / WINDOW;
    }

    void reset()
    {
        for (int i = 0; i < WINDOW; i++)
            buffer[i] = 0;
        sum = 0;
        idx = 0;
    }
};

class PeakDetector
{
private:
    // Previous filtered sample
    float lastSample = 0;
    // Time of previous detected beat
    unsigned long lastPeakTime = 0;
    // Smoothed BPM
    float calculatedBPM = 75.0f;
    // The flag showing the Moving Average function is initialized
    bool initialized = false;

public:
    bool process(float sample)
    {
        if (!initialized)
        {
            lastSample = sample;
            initialized = true;
            return false;
        }

        // Zero crossing
        bool peak = (lastSample < 0.0f) && (sample >= 0.0f);
        
        if (peak)
        {
            unsigned long currentTime = millis();

            if (lastPeakTime != 0)
            {
                unsigned long interval = currentTime - lastPeakTime;
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
                    float instantaneousBPM = 60000.0f / static_cast<float>(interval);
                    // sprintf(buff, "rawBPM: %f", instantaneousBPM);
                    
                    // --------------------------------------------
                    // Rate limiter
                    // Maximum change = 3 BPM per beat
                    // --------------------------------------------
                    const float MAX_CHANGE = 3.0f;

                    if (instantaneousBPM > calculatedBPM + MAX_CHANGE)
                    {
                        calculatedBPM += MAX_CHANGE;
                    }
                    else if (instantaneousBPM < calculatedBPM - MAX_CHANGE)
                    {
                        calculatedBPM -= MAX_CHANGE;
                    }
                    else
                    {
                        // ----------------------------------------
                        // Exponential smoothing
                        // ----------------------------------------
                        calculatedBPM = 0.8f * calculatedBPM + 0.2f * instantaneousBPM;
                    }

                    lastPeakTime = currentTime;
                    lastSample = sample;
                    return true;
                }
                else if (interval >= 1500)
                {
                    lastPeakTime = currentTime; 
                }
            }
            else
            {
                // First valid peak
                lastPeakTime = currentTime;
            }
        }

        lastSample = sample;
        return false;
    }

    float getBPM()
    {
        return calculatedBPM;
    }

    float getCalculatedBPM()
    {
        return this->calculatedBPM;
    }

    void reset()
    {
        lastSample = 0;
        lastPeakTime = 0;
        calculatedBPM = 75.0f;
        initialized = false;
    }
};

class Smoothing
{
private:
    float buffer[5] = {0.0};
    float sum         {0.0};
    float avg         {0.0};
    int idx           {0};

public:
    void process(PeakDetector input)
    {
        this->sum -= buffer[this->idx];
        this->buffer[this->idx] = input.getCalculatedBPM();
        this->sum += input.getCalculatedBPM();
        this->idx = (this->idx + 1) % 5;
        this->avg =  this->sum / 5;
    }

    void reset()
    {
        for (int i = 0; i < 5; i++)
            this->buffer[i] = 0;
        this->sum = 0;
        this->idx = 0;
    }

    float getAvg()
    {
        return this->avg;
    }
};

MAX30105 particleSensor;

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

MovingAverage movingAverage;
PeakDetector peakDetector;
Smoothing smooth;

// Reset Function
void resetSignalProcessing()
{
    highPass.reset();
    lowPass.reset();
    movingAverage.reset();
    peakDetector.reset();
    smooth.reset();
}

void setup()
{
    serial.begin(115200);

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST))
    {
        serial.println("MAX30102 Error");
        while (1);
    }

    particleSensor.setup(
        0x3F,   // LED brightness
        1,      // sample averaging
        2,      // Red + IR
        100,    // sample rate = 100 Hz
        411,    // pulse width
        4096    // ADC range
    );

    particleSensor.clearFIFO();

    serial.println("Ready");
    serial.println("Place your finger on the sensor.");
}

// 6 sec warm-up and 3 sec display delay
unsigned long          warmup_start       {0};
unsigned long          last_display_time  {0};
constexpr unsigned int WARM_UP_INTERVAL   {6000}; // 6s
constexpr unsigned int DISPLAY_INTERVAL   {3000}; // 3s

void loop()
{
    particleSensor.check();

    while (particleSensor.available())
    {
        // IR value
        long irRaw = particleSensor.getFIFOIR();

        // FINGER DETECTION
        if (irRaw < 30000)
        {
            serial.println("No finger detected!");
            warmup_start = 0;
            resetSignalProcessing();
            particleSensor.nextSample();
            continue;
        }

        float  signal      =    static_cast<float>(irRaw);      // RAW SIGNAL
        float  hp          =    highPass.process(signal);       // BAND-PASS
        float  lp          =    lowPass.process(hp);            // BAND-PASS
        float  filtered    =    movingAverage.process(lp);      // MOVING AVERAGE
        // char buff[30];
        // sprintf(buff, "Filtered: %f", filtered);
        // serial.println(buff);
        bool   peak        =    peakDetector.process(filtered); // PEAK DETECTION
        smooth.process(peakDetector);                           // SMOOTHING


        unsigned long current_time = millis();
        if (warmup_start == 0)
            warmup_start = current_time;

        if (current_time - warmup_start < WARM_UP_INTERVAL)
        {
            serial.println("Warming up...");
            particleSensor.nextSample();
            continue;
        }

        if (current_time - last_display_time >= DISPLAY_INTERVAL)
        {
            last_display_time = current_time;

            int bpm = static_cast<int>(smooth.getAvg());

            serial.print("BPM: ");
            serial.println(bpm, 1);
        }

        particleSensor.nextSample();
    }
}

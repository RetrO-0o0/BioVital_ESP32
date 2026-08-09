#include <Wire.h>
#include "MAX30105.h"

#define serial Serial

class IIRFilter
{
private:

    float b0,b1,b2;
    float a1,a2;

    float x1=0;
    float x2=0;

    float y1=0;
    float y2=0;


public:

    IIRFilter(float b0_, float b1_, float b2_, float a1_, float a2_)
    {
        b0=b0_;
        b1=b1_;
        b2=b2_;

        a1=a1_;
        a2=a2_;
    }

    float process(float x)
    {
        // y[n] = b0.x[n] + b1.x[n-1] + b2.x[n-2] - a1.y[n-1] - a2.y[n-2]
        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;


        x2 = x1;
        x1 = x;

        y2 = y1;
        y1 = y;

        return y;
    }
};

class MovingAverage 
{
private:

    static constexpr int WINDOW {5};

    float buffer[WINDOW]    =   {0};
    float sum                   {0};
    uint8_t idx                 {0};


public:
    float process(float input)
    {
        this->sum -= buffer[idx];

        this->buffer[idx] = input;

        this->sum += input;

        idx = (idx + 1) % WINDOW;

        return sum / WINDOW;
    }
};


class PeakDetector 
{
private:

    float const THRESHOLD     {0.0};
    uint32_t const REFRACTORY {30};

    float prev;
    float curr;
    float bpm;

    uint32_t last_peak_sample;
    uint32_t sample_counter;

    bool initialized          {false};

public:

    bool process(float sample)
    {
        sample_counter++;

        if(!this->initialized)
        {
            this->prev        = sample;
            this->curr        = sample;
            this->initialized = true;
            return false;
        }

        float next = sample;

        bool peak  = (this->curr > this->prev) && (this->curr > next) && (this->curr > this->THRESHOLD);
        if(peak)
        {
            if(this->sample_counter - this->last_peak_sample > this->REFRACTORY)
            {
                if(this->last_peak_sample != 0)
                {
                    uint32_t interval = this->sample_counter - this->last_peak_sample;
                    this->bpm         = 6000.0f / interval; // BPM =  Fs(=100Hz) * 60(seconds) / Interval
                }

                this->last_peak_sample = this->sample_counter;
            }
            else
            {
                peak = false;
            }
        }

        this->prev = this->curr;
        this->curr = next;

        return peak;
    }

    float getBPM()
    {
        // serial.print("BPM: ");
        // serial.println(this->bpm);

        return this->bpm;
    }
};


// Sensor Object
MAX30105 particleSensor;

// High Pass 0.5Hz
IIRFilter highPass(
    0.978030479,
   -1.956060958,
    0.978030479,
   -1.955578240,
    0.956543676
);



// Low Pass 4Hz
IIRFilter lowPass(
    0.013359200,
    0.026718400,
    0.013359200,
   -1.647459981,
    0.700896781
);


// Moving Average Object
MovingAverage moving_average;

// Peak Detector Object
PeakDetector peak_detector;

void setup()
{
    serial.begin(115200);

    if(!particleSensor.begin(Wire,I2C_SPEED_FAST))
    {
        Serial.println("MAX30102 Error");
        while(1);
    }


    particleSensor.setup(
        0x3F, // LED brightness
        1,    // sample average
        2,    // LED mode 2: Red + IR
        100,  // sample rate
        411,  // pusle width
        4096  // ADC range
    );

    particleSensor.clearFIFO();

    serial.println("Ready");
}


void loop()
{
    particleSensor.check();

    while(particleSensor.available())
    {
        long irRaw = particleSensor.getFIFOIR();

        if(irRaw < 30000)
        {
            serial.println("No finger detected!");
            particleSensor.nextSample();
            continue;
        }

        float hp = highPass.process(irRaw);
        float lp = lowPass.process(hp);
        float mv = moving_average.process(lp);
        bool  pk = peak_detector.process(mv);

        if (pk)
        {
            serial.print("BPM: ");
            serial.println(peak_detector.getBPM());
        }

        particleSensor.nextSample();
    }
}
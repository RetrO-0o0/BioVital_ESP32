#include <Wire.h>
#include "MAX30105.h"

#define serial Serial

MAX30105 particleSensor;



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

    static float moving_average(float input)
    {
        static int idx           {0};
        static float sum         {0};
        static float buffer[5] = {0};

        sum -= buffer[idx];

        buffer[idx] = input;

        sum += input;

        idx++;
        idx %= 5;

        return sum / 5.0;
    }
};



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
        float mv = IIRFilter::moving_average(lp);

        serial.println(mv);

        particleSensor.nextSample();
    }
}
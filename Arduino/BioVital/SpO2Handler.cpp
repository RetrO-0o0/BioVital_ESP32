#include "SpO2Handler.hpp"

SpO2Handler::SpO2Handler() 
{
    reset();
}

void SpO2Handler::reset() 
{
    is_initialized    = false;
    sampleCount       = 0;
    warmupCount       = 0;
    spo2_ui_state     = 0;
    spo2_final_result = 0.0f;
    spo2_progress     = 0;
    dcIR              = 0.0f;
    dcRed             = 0.0f;
    w_ir              = 0.0f;
    w_red             = 0.0f;
}

void SpO2Handler::process(long irRaw, long redRaw) 
{
    if (irRaw < 30000) 
    {
        reset();
        return;
    }

    if (!is_initialized) 
    {
        dcIR           = static_cast<float>(irRaw);
        dcRed          = static_cast<float>(redRaw);
        w_ir           = static_cast<float>(irRaw);
        w_red          = static_cast<float>(redRaw);
        is_initialized = true;
    }

    dcIR  = 0.99f * dcIR  + 0.01f * static_cast<float>(irRaw);
    dcRed = 0.99f * dcRed + 0.01f * static_cast<float>(redRaw);

    const float alpha = 0.95f;
    float w_ir_new    = static_cast<float>(irRaw) + alpha * w_ir;
    float acIR        = w_ir_new - w_ir;
    w_ir              = w_ir_new;

    float w_red_new = static_cast<float>(redRaw) + alpha * w_red;
    float acRed     = w_red_new - w_red;
    w_red           = w_red_new;

    if (warmupCount < 300) 
    {
        warmupCount++;
        spo2_ui_state = 1;
        return;
    }

    irACBuffer[sampleCount]  = acIR;
    redACBuffer[sampleCount] = acRed;
    sampleCount++;

    spo2_ui_state = (spo2_ui_state == 3) ? 3 : 2;
    spo2_progress = sampleCount / 10;

    if (sampleCount >= WINDOW_SIZE) 
    {
        float irAcSqSum = 0.0f;
        float redAcSqSum = 0.0f;

        for (int i = 0; i < WINDOW_SIZE; i++) 
        {
            irAcSqSum += irACBuffer[i] * irACBuffer[i];
            redAcSqSum += redACBuffer[i] * redACBuffer[i];
        }

        float acIR_RMS  = sqrtf(irAcSqSum / WINDOW_SIZE);
        float acRed_RMS = sqrtf(redAcSqSum / WINDOW_SIZE);

        if (dcIR > 0.0f && dcRed > 0.0f && acIR_RMS > 0.0f) 
        {
            float R            = (acIR_RMS / dcIR) / (acRed_RMS / dcRed);
            float current_spo2 = -45.060f * R * R + 30.354f * R + 94.845f;

            if (current_spo2 > 100.0f) current_spo2 = 100.0f;
            if (current_spo2 < 0.0f) current_spo2   = 0.0f;

            spo2_final_result = current_spo2;
            spo2_ui_state     = 3;
        }
        sampleCount = 0;
    }
}

int SpO2Handler::getUIState() const 
{
    return spo2_ui_state;
}

float SpO2Handler::getFinalResult() const 
{
    return spo2_final_result;
}

int SpO2Handler::getProgress() const 
{
    return spo2_progress;
}

int SpO2Handler::getWarmupRemainingSeconds() const 
{
    return 3 - (warmupCount / 100);
}

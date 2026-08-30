#ifndef SPO2_HANDLER_HPP
#define SPO2_HANDLER_HPP

#include <Arduino.h>

#define WINDOW_SIZE 1000

extern float irACBuffer[WINDOW_SIZE];
extern float redACBuffer[WINDOW_SIZE];
extern int sampleIndex;
extern int sampleCount;

extern float dcIR;
extern float dcRed;
extern float prevRawIR;
extern float prevRawRed;
extern float prevACIR;
extern float prevACRed;

extern float spo2_final_result;
extern int spo2_ui_state; 
extern int spo2_progress;
extern float spo2_dc_warmup_sec;

void resetSignalProcessingSpO2();
void processSpO2(uint32_t irRaw, uint32_t redRaw);

#endif // SPO2_HANDLER_HPP


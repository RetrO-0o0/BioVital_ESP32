#include <Wire.h>
#include <U8g2lib.h>
#include <Button2.h>
#include "MAX30105.h"

#include "SpO2Handler.hpp"
#include "BPMHandler.hpp"

// ---------------------------------------------------------
// Button Pins
// ---------------------------------------------------------
#define BTN_NAVIGATE 14
#define BTN_SELECT   15

// ---------------------------------------------------------
// Objects
// ---------------------------------------------------------
MAX30105 particleSensor;

SpO2Handler spo2Handler;

Button2 btnNav;
Button2 btnSel;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ---------------------------------------------------------
// State Machine
// ---------------------------------------------------------
uint8_t systemState        = 0; // 0: Menu, 1: SpO2, 2: BPM
bool menuCursor            = 0; // 0: SpO2, 1: BPM
unsigned long lastDrawTime = 0;

// ---------------------------------------------------------
// 
// ---------------------------------------------------------
void onNavClick(Button2& btn) 
{
    if (systemState == 0) 
        menuCursor = !menuCursor;
}

void onSelClick(Button2& btn) 
{
    if (systemState == 0) 
    {
        systemState = (menuCursor == 0) ? 1 : 2;
        
        if (systemState == 1) 
        {
            spo2Handler.reset();
        } 
        else 
        {
            resetSignalProcessingBPM();
        }
        particleSensor.clearFIFO();
    } 
    else 
    {
        systemState = 0;
    }
}

// ---------------------------------------------------------
// SSD1306 Menu
// ---------------------------------------------------------
void drawMenu() 
{
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(32, 12, "MAIN MENU");
    u8g2.drawLine(0, 16, 128, 16);

    u8g2.setFont(u8g2_font_helvB14_tr);
    if (menuCursor == 0) 
    {
        u8g2.drawStr(10, 40, "> SpO2");
        u8g2.drawStr(10, 60, "  BPM");
    } 
    else 
    {
        u8g2.drawStr(10, 40, "  SpO2");
        u8g2.drawStr(10, 60, "> BPM");
    }
}

void drawSpO2() 
{
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(0, 12, "SpO2 Monitor");
    u8g2.drawLine(0, 15, 128, 15);

    int state = spo2Handler.getUIState();
    
    if (state == 0) 
    {
        u8g2.setFont(u8g2_font_helvB08_tr);
        u8g2.drawStr(0, 36, "Place Finger...");
    } 
    else if (state == 1) 
    {
        u8g2.setFont(u8g2_font_helvB08_tr);
        u8g2.setCursor(0, 36);
        u8g2.print("Stabilizing: ");
        u8g2.print(spo2Handler.getWarmupRemainingSeconds());
        u8g2.print("s");
    } 
    else if (state == 2) 
    {
        u8g2.setFont(u8g2_font_helvB08_tr);
        u8g2.setCursor(0, 32);
        u8g2.print("Recording: ");
        u8g2.print(spo2Handler.getProgress());
        u8g2.print("%");

        u8g2.drawFrame(0, 42, 128, 10);
        u8g2.drawBox(2, 44, (spo2Handler.getProgress() * 124) / 100, 6);
    } 
    else if (state == 3) 
    {
        u8g2.setFont(u8g2_font_logisoso22_tr);
        u8g2.setCursor(10, 44);
        u8g2.print(spo2Handler.getFinalResult(), 1);

        u8g2.setFont(u8g2_font_helvB10_tr);
        u8g2.print(" %");

        u8g2.setFont(u8g2_font_helvB08_tr);
        u8g2.setCursor(0, 60);
        u8g2.print("Updating: ");
        u8g2.print(spo2Handler.getProgress());
        u8g2.print("%");

        u8g2.drawFrame(70, 53, 58, 8);
        u8g2.drawBox(72, 55, (spo2Handler.getProgress() * 54) / 100, 4);
    }
}


void drawBPM() 
{
    u8g2.setFont(u8g2_font_helvB08_tr);
    u8g2.drawStr(0, 12, "BPM Monitor");
    u8g2.drawLine(0, 16, 128, 16);

    if (bpm_ui_state == 0) 
    {
        u8g2.drawStr(0, 32, "Place Finger...");
        
    } 
    else if (bpm_ui_state == 1) 
    {
        u8g2.setFont(u8g2_font_helvB08_tr);
        u8g2.drawStr(0, 32, "Warming up...");

        u8g2.drawFrame(0, 42, 128, 10);

        unsigned long elapsedTime = millis() - bpm_startTime;
        if (elapsedTime > WARM_UP_INTERVAL)
            elapsedTime = WARM_UP_INTERVAL;

        u8g2.drawBox(2, 44, ((elapsedTime * 124) / WARM_UP_INTERVAL), 6);
    } 
    else if (bpm_ui_state == 2) 
    {
        if (bpm_final_result > 0) 
        {
            // Thin Progress Bar
            unsigned long elapsedTime = millis() - bpm_lastDisplayTime;
            if (elapsedTime > DISPLAY_INTERVAL)
                elapsedTime = DISPLAY_INTERVAL;

            int lineOffset = ((elapsedTime * 64) / DISPLAY_INTERVAL) + 32;
            u8g2.drawLine(32, 60, lineOffset, 60);

            // bpm_final_result in c-syle str
            char bpm_buff[5];
            snprintf(bpm_buff, sizeof(bpm_buff), "%d", static_cast<int>(bpm_final_result));

            // Dynamic Display
            u8g2.setFont(u8g2_font_logisoso24_tr); // bpm_final_result
            int w_num   = u8g2.getStrWidth(bpm_buff);
            u8g2.setFont(u8g2_font_logisoso16_tr); // BPM unit
            int w_bpm   = u8g2.getStrWidth("BPM");
            int spacing = 4;                       // Spacing
            int start_x = (u8g2.getDisplayWidth() - (w_bpm + w_num + spacing)) / 2; // Dynamic BPM num
            int bpm_spacing_unit = start_x + w_num + spacing;                       // Dynamic BPM unit

            // Final BPM Result
            u8g2.setFont(u8g2_font_logisoso24_tr);
            u8g2.drawStr(start_x, 50, bpm_buff);

            // BPM Unit
            u8g2.setFont(u8g2_font_logisoso16_tf);
            u8g2.drawStr(bpm_spacing_unit, 50, "BPM");
        }
    }
}

void updateUI() 
{
    if (millis() - lastDrawTime < 200) 
        return; 

    lastDrawTime = millis();

    u8g2.clearBuffer();

    if (systemState == 0) 
    {
        drawMenu();
    } 
    else if (systemState == 1) 
    {
        drawSpO2();
    } 
    else if (systemState == 2) 
    {
        drawBPM();
    }

    u8g2.sendBuffer();
}

// ---------------------------------------------------------
// Sensor Functions
// ---------------------------------------------------------
void readSensor() 
{
    particleSensor.check();
    
    while (particleSensor.available()) 
    {
        uint32_t irRaw = particleSensor.getFIFOIR();
        uint32_t redRaw = particleSensor.getFIFORed();

        if (systemState == 1) {
            spo2Handler.process(irRaw, redRaw);
        } else if (systemState == 2) {
            processBPM(irRaw);
        }
        
        particleSensor.nextSample();
    }
}

// ---------------------------------------------------------
// Setup & Loop
// ---------------------------------------------------------
void setup() 
{
    Serial.begin(115200);

    u8g2.begin();

    btnNav.begin(BTN_NAVIGATE);
    btnSel.begin(BTN_SELECT);
    btnNav.setTapHandler(onNavClick);
    btnSel.setTapHandler(onSelClick);

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) 
    {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_helvB08_tr);
        u8g2.drawStr(0, 20, "MAX30105 Not Found! Please Reset.");
        u8g2.sendBuffer();
        while (1);
    }
    
    // ---------------------------------------------------------
    // Particle Sensor Setup
        // ledBrightness=0x3F=60, 
        // sampleAverage=1, 
        // ledMode=2, 
        // sampleRate=100, 
        // pulseWidth=411, 
        // adcRange=4096
    // ---------------------------------------------------------
    particleSensor.setup(0x3F, 1, 2, 100, 411, 4096);
    particleSensor.clearFIFO();
}

void loop() 
{
    btnNav.loop();
    btnSel.loop();
    
    readSensor();
    
    updateUI();
}

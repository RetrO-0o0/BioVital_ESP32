#include <Wire.h>
#include "MAX30105.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "BPMHandler.hpp"
#include "SpO2Handler.hpp"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BTN_NAVIGATE 14
#define BTN_SELECT   15

unsigned long lastNavPress = 0;
unsigned long lastSelPress = 0;
const unsigned long DEBOUNCE_DELAY = 250;

int systemState = 0; 
int menuCursor = 1;  

MAX30105 particleSensor;

void setup() {
    Serial.begin(115200);

    pinMode(BTN_NAVIGATE, INPUT_PULLUP);
    pinMode(BTN_SELECT, INPUT_PULLUP);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
        Serial.println("MAX30102 was not found. Please check wiring/power.");
        while (1);
    }

    particleSensor.setup(60, 1, 2, 100, 411, 4096);
    particleSensor.clearFIFO();
}

void loop() {
    unsigned long now = millis();

    if (digitalRead(BTN_NAVIGATE) == LOW && (now - lastNavPress > DEBOUNCE_DELAY)) {
        lastNavPress = now;
        if (systemState == 0) {
            menuCursor = (menuCursor == 1) ? 2 : 1;
        }
    }

    if (digitalRead(BTN_SELECT) == LOW && (now - lastSelPress > DEBOUNCE_DELAY)) {
        lastSelPress = now;
        if (systemState == 0) {
            systemState = menuCursor; 
            if (systemState == 1) {
                resetSignalProcessingSpO2();
                particleSensor.clearFIFO();
            } else if (systemState == 2) {
                resetSignalProcessingBPM();
                particleSensor.clearFIFO();
            }
        } else {
            systemState = 0; 
        }
    }

    particleSensor.check();
    while (particleSensor.available()) {
        uint32_t irRaw = particleSensor.getFIFOIR();
        uint32_t redRaw = particleSensor.getFIFORed();
        particleSensor.nextSample();

        if (systemState == 1) {
            processSpO2(irRaw, redRaw);
        } else if (systemState == 2) {
            processBPM(irRaw);
        }
    }

    static unsigned long lastOledUpdate = 0;
    if (now - lastOledUpdate >= 200) {
        lastOledUpdate = now;
        display.clearDisplay();

        if (systemState == 0) {
            display.setTextSize(1);
            display.setCursor(30, 0);
            display.println("MAIN MENU");
            display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

            display.setCursor(10, 25);
            if (menuCursor == 1) display.print("> "); else display.print("  ");
            display.println("SpO2");

            display.setCursor(10, 40);
            if (menuCursor == 2) display.print("> "); else display.print("  ");
            display.println("BPM");

        } else if (systemState == 1) {
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("Mode: SpO2");
            display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

            if (spo2_ui_state == 0) {
                display.setCursor(0, 30);
                display.println("Place Finger...");
            } else if (spo2_ui_state == 1) {
                display.setCursor(0, 25);
                display.println("Stabilizing...");
                display.setCursor(0, 40);
                display.print("Wait: ");
                display.print(spo2_dc_warmup_sec, 1);
                display.println(" s");
            } else if (spo2_ui_state == 2) {
                display.setCursor(0, 25);
                display.println("Recording...");
                display.setCursor(0, 40);
                display.print("Progress: ");
                display.print(spo2_progress);
                display.println("%");
            } else if (spo2_ui_state == 3) {
                display.setCursor(0, 20);
                display.println("SpO2 Result:");
                display.setTextSize(3);
                display.setCursor(20, 35);
                display.print((int)spo2_final_result);
                display.setTextSize(2);
                display.print("%");
            }

        } else if (systemState == 2) {
            display.setTextSize(1);
            display.setCursor(0, 0);
            display.println("Mode: BPM");
            display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

            if (bpm_ui_state == 0) {
                display.setCursor(0, 30);
                display.println("Place Finger...");
            } else if (bpm_ui_state == 1) {
                display.setCursor(0, 30);
                display.println("Warming up...");
            } else if (bpm_ui_state == 2) {
                display.setCursor(0, 20);
                display.println("Heart Rate:");
                display.setTextSize(3);
                display.setCursor(20, 35);
                if (bpm_final_result > 0) {
                    display.print((int)bpm_final_result);
                    display.setTextSize(1);
                    display.print(" BPM");
                } else {
                    display.setTextSize(2);
                    display.print("Reading...");
                }
            }
        }
        display.display();
    }
}


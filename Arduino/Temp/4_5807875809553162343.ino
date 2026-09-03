#include <Wire.h>
#include "MAX30105.h"
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =======================================================
// تنظیمات OLED و دکمه‌ها
// =======================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int BTN_NAVIGATE = 14; 
const int BTN_SELECT = 15;   

unsigned long lastNavPress = 0;
unsigned long lastSelPress = 0;
const int debounceDelay = 250;

int systemState = 0; // 0: Menu, 1: SpO2, 2: BPM
int menuCursor = 0;  // 0: SpO2, 1: BPM
unsigned long lastOledUpdate = 0;

MAX30105 particleSensor;

// =======================================================
// کلاس‌ها و متغیرهای عینا کپی شده از کد BPM شما
// =======================================================
class IIRFilter {
private: float b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
public:
    IIRFilter(float b0_, float b1_, float b2_, float a1_, float a2_) {
        b0 = b0_; b1 = b1_; b2 = b2_; a1 = a1_; a2 = a2_;
    }
    float process(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
    void reset() { x1 = 0; x2 = 0; y1 = 0; y2 = 0; }
};

class MovingAverage {
private:
    static constexpr int WINDOW = 10;
    float buffer[WINDOW] = {0};
    float sum = 0;
    uint8_t idx = 0;
public:
    float process(float input) {
        sum -= buffer[idx]; buffer[idx] = input; sum += input;
        idx = (idx + 1) % WINDOW; return sum / WINDOW;
    }
    void reset() {
        for (int i = 0; i < WINDOW; i++) buffer[i] = 0;
        sum = 0; idx = 0;
    }
};

class PeakDetector {
private:
    float lastSample = 0;
    unsigned long lastPeakTime = 0;
    float calculatedBPM = 75.0f;
    bool initialized = false;
public:
    bool process(float sample) {
        if (!initialized) { lastSample = sample; initialized = true; return false; }
        bool peak = (lastSample < 0.0f) && (sample >= 0.0f);
        if (peak) {
            unsigned long currentTime = millis();
            if (lastPeakTime != 0) {
                unsigned long interval = currentTime - lastPeakTime;
                if (interval > 333 && interval < 1500) {
                    float instantaneousBPM = 60000.0f / static_cast<float>(interval);
                    const float MAX_CHANGE = 3.0f;
                    if (instantaneousBPM > calculatedBPM + MAX_CHANGE) calculatedBPM += MAX_CHANGE;
                    else if (instantaneousBPM < calculatedBPM - MAX_CHANGE) calculatedBPM -= MAX_CHANGE;
                    else calculatedBPM = 0.8f * calculatedBPM + 0.2f * instantaneousBPM;
                    lastPeakTime = currentTime; lastSample = sample; return true;
                }
                else if (interval >= 1500) { lastPeakTime = currentTime; }
            } else { lastPeakTime = currentTime; }
        }
        lastSample = sample; return false;
    }
    float getCalculatedBPM() { return calculatedBPM; }
    void reset() { lastSample = 0; lastPeakTime = 0; calculatedBPM = 75.0f; initialized = false; }
};

class Smoothing {
private: float buffer[5] = {0.0}; float sum {0.0}; float avg {0.0}; int idx {0};
public:
    void process(PeakDetector input) {
        this->sum -= buffer[this->idx];
        this->buffer[this->idx] = input.getCalculatedBPM();
        this->sum += input.getCalculatedBPM();
        this->idx = (this->idx + 1) % 5;
        this->avg =  this->sum / 5;
    }
    void reset() {
        for (int i = 0; i < 5; i++) this->buffer[i] = 0;
        this->sum = 0; this->idx = 0;
    }
    float getAvg() { return this->avg; }
};

IIRFilter highPass(0.978030479, -1.956060958, 0.978030479, -1.955578240, 0.956543676);
IIRFilter lowPass(0.013359200, 0.026718400, 0.013359200, -1.647459981, 0.700896781);
MovingAverage movingAverage;
PeakDetector peakDetector;
Smoothing smoothBPM;

void resetSignalProcessingBPM() {
    highPass.reset(); lowPass.reset(); movingAverage.reset();
    peakDetector.reset(); smoothBPM.reset();
}

unsigned long warmup_start {0};
unsigned long last_display_time {0};
constexpr unsigned int WARM_UP_INTERVAL {6000}; 
constexpr unsigned int DISPLAY_INTERVAL {3000}; 

int bpm_ui_state = 0; // 0: No Finger, 1: Warmup, 2: Ready
int bpm_final_result = 0;

// =======================================================
// متغیرهای عینا کپی شده از کد SpO2 شما
// =======================================================
const int WINDOW_SIZE = 1000; 
float irACBuffer[WINDOW_SIZE];
float redACBuffer[WINDOW_SIZE];
int sampleCount = 0;
int warmupCount = 0; 
float dcIR = 0, dcRed = 0, w_ir = 0, w_red = 0;
bool is_initialized = false;

int spo2_ui_state = 0; // 0: No Finger, 1: Stabilizing, 2: Recording, 3: Result
float spo2_final_result = 0.0;
int spo2_progress = 0;

void resetSignalProcessingSpO2() {
    is_initialized = false;
    sampleCount = 0; 
    warmupCount = 0;
    spo2_ui_state = 0;
}

// =======================================================
// Setup
// =======================================================
void setup() {
  Serial.begin(115200);
  pinMode(BTN_NAVIGATE, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED Error")); while(1);
  }
  display.clearDisplay(); display.display();

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 Error"); while (1);
  }

  particleSensor.setup(60, 1, 2, 100, 411, 4096); 
  particleSensor.clearFIFO();
}

// =======================================================
// Loop 
// =======================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. خواندن دکمه‌ها و مدیریت منو
  bool navPressed = (digitalRead(BTN_NAVIGATE) == LOW);
  bool selPressed = (digitalRead(BTN_SELECT) == LOW);

  if (systemState == 0) {
    if (navPressed && (currentMillis - lastNavPress > debounceDelay)) {
      menuCursor = (menuCursor + 1) % 2;
      lastNavPress = currentMillis;
    }
    if (selPressed && (currentMillis - lastSelPress > debounceDelay)) {
      systemState = (menuCursor == 0) ? 1 : 2;
      if (systemState == 1) resetSignalProcessingSpO2();
      if (systemState == 2) { warmup_start = 0; resetSignalProcessingBPM(); }
      particleSensor.clearFIFO();
      lastSelPress = currentMillis;
    }
  } 
  else {
    // بازگشت به منو در حین اجرای الگوریتم
    if (selPressed && (currentMillis - lastSelPress > debounceDelay)) {
      systemState = 0;
      lastSelPress = currentMillis;
      return; 
    }
  }

 // 2. پردازش هسته‌ی اصلی سنسور
  particleSensor.check(); 
  
  while (particleSensor.available()) {
    long irRaw = particleSensor.getFIFOIR();
    long redRaw = particleSensor.getFIFORed();

    // ==============================================
    // اجرای دقیق کد SpO2 
    // ==============================================
    if (systemState == 1) {
      if (irRaw < 30000) {
        resetSignalProcessingSpO2();
        particleSensor.nextSample();
        continue;
      }

      if (!is_initialized) {
        dcIR = irRaw; dcRed = redRaw; w_ir = irRaw; w_red = redRaw;
        is_initialized = true;
      }

      dcIR = 0.99 * dcIR + 0.01 * (float)irRaw;
      dcRed = 0.99 * dcRed + 0.01 * (float)redRaw;
      
      float alpha = 0.95; 
      float w_ir_new = (float)irRaw + alpha * w_ir;
      float acIR = w_ir_new - w_ir; w_ir = w_ir_new;

      float w_red_new = (float)redRaw + alpha * w_red;
      float acRed = w_red_new - w_red; w_red = w_red_new;

      if (warmupCount < 300) {
          warmupCount++;
          spo2_ui_state = 1;
          particleSensor.nextSample();
          continue; 
      }

      irACBuffer[sampleCount] = acIR;
      redACBuffer[sampleCount] = acRed;
      sampleCount++;
      spo2_ui_state = (spo2_ui_state == 3) ? 3 : 2; // حفظ حالت نتیجه
      spo2_progress = (sampleCount / 10);

      if (sampleCount >= WINDOW_SIZE) {
          float irAcSqSum = 0; float redAcSqSum = 0;
          for (int i = 0; i < WINDOW_SIZE; i++) {
              irAcSqSum += irACBuffer[i] * irACBuffer[i];
              redAcSqSum += redACBuffer[i] * redACBuffer[i];
          }
          float acIR_RMS = sqrt(irAcSqSum / WINDOW_SIZE);
          float acRed_RMS = sqrt(redAcSqSum / WINDOW_SIZE);

          if (dcIR > 0 && dcRed > 0 && acIR_RMS > 0) {
              float R = (acIR_RMS / dcIR) / (acRed_RMS / dcRed);
              float current_spo2 = -45.060 * R * R + 30.354 * R + 94.845;
              if (current_spo2 > 100.0) current_spo2 = 100.0;
              if (current_spo2 < 0.0) current_spo2 = 0.0;
              
              spo2_final_result = current_spo2;
              spo2_ui_state = 3;
          }
          sampleCount = 0; 
      }
      particleSensor.nextSample();
    }
    
    // ==============================================
    // اجرای دقیق کد BPM 
    // ==============================================
    else if (systemState == 2) {
      if (irRaw < 30000) {
        warmup_start = 0;
        bpm_ui_state = 0;
        resetSignalProcessingBPM();
        particleSensor.nextSample();
        continue;
      }

      float signal = static_cast<float>(irRaw);
      float hp = highPass.process(signal);
      float lp = lowPass.process(hp);
      float filtered = movingAverage.process(lp);
      bool peak = peakDetector.process(filtered);
      smoothBPM.process(peakDetector);

      unsigned long current_time = millis();
      if (warmup_start == 0) warmup_start = current_time;

      if (current_time - warmup_start < WARM_UP_INTERVAL) {
          bpm_ui_state = 1;
          particleSensor.nextSample();
          continue;
      }

      bpm_ui_state = 2; // زمان‌بندی گرم‌شدن تمام شد

      if (current_time - last_display_time >= DISPLAY_INTERVAL) {
          last_display_time = current_time;
          bpm_final_result = static_cast<int>(smoothBPM.getAvg());
      }

      particleSensor.nextSample();
    }
    else {
      particleSensor.nextSample(); // تخلیه بافر در حالت منو
    }
  }

  // ==========================================
  // آپدیت OLED در محیطی کاملا امن (هر 200 میلی‌ثانیه)
  // ==========================================
  if (currentMillis - lastOledUpdate > 200) {
    display.clearDisplay();
    display.setTextColor(WHITE);

    if (systemState == 0) {
      // 1. هدر منو (سایز کوچک)
      display.setTextSize(1);
      display.setCursor(35, 2); 
      display.print("MAIN MENU");
      display.drawLine(0, 14, 128, 14, WHITE);
      
      // 2. تنظیم سایز فونت برای گزینه‌ها (سایز بزرگ)
      display.setTextSize(2);
      
      // 3. گزینه اول (SpO2)
      display.setCursor(0, 22); 
      if (menuCursor == 0) display.print(">"); else display.print(" ");
      display.setCursor(20, 22); display.print("SpO2");
      
      // 4. گزینه دوم (BPM)
      display.setCursor(0, 44);
      if (menuCursor == 1) display.print(">"); else display.print(" ");
      display.setCursor(20, 44); display.print("BPM");
    } 
    
    else if (systemState == 1) {
      display.setTextSize(1);
      display.setCursor(0, 0); display.print("SpO2 Monitor");
      display.drawLine(0, 10, 128, 10, WHITE);
      
      if (spo2_ui_state == 0) {
        display.setCursor(10, 30); display.print("Place Finger...");
      } else if (spo2_ui_state == 1) {
        display.setCursor(10, 30); display.print("Stabilizing: ");
        display.print(3 - (warmupCount/100)); display.print("s");
      } else {
        display.setCursor(0, 20); display.print("Recording: "); display.print(spo2_progress); display.print("%");
        if (spo2_ui_state == 3) {
          display.setTextSize(3);
          display.setCursor(20, 35); display.print(spo2_final_result, 1);
          display.setTextSize(1); display.print("%");
        }
      }
    } 
    
    else if (systemState == 2) {
      display.setTextSize(1);
      display.setCursor(0, 0); display.print("BPM Monitor");
      display.drawLine(0, 10, 128, 10, WHITE);
      
      if (bpm_ui_state == 0) {
        display.setCursor(10, 30); display.print("Place Finger...");
      } else if (bpm_ui_state == 1) {
        display.setCursor(10, 30); display.print("Warming up...");
      } else if (bpm_ui_state == 2) {
        display.setCursor(0, 20); display.print("Reading...");
        if (bpm_final_result > 0) {
          display.setTextSize(3);
          display.setCursor(35, 35); display.print(bpm_final_result);
        }
      }
    }
    
    display.display();
    lastOledUpdate = currentMillis;
  }
} // <--- پایان تابع loop
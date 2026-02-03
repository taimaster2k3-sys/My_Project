#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_SHT31.h> 
#include <cmath> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64 

const char* WIFI_SSID = "IP7 pls";
const char* WIFI_PASS = "00000000";

const char* SERVER_URL = "http://54.153.160.152:8000/write_data";
const char* STATUS_DEVICE_URL = "http://54.153.160.152:8000/status_device";
const char* READ_BUTTON_URL = "http://54.153.160.152:8000/read_button";
// Gửi dữ liệu mỗi 1 giây
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 000;
// phun sương
unsigned long humidifierStartTime = 0;
bool humidifierRunning = false;


U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE); 

// --- Cấu hình Cảm biến MH-Z19 PWM ---
const int CO2_PWM_PIN = 18; 
volatile unsigned long tH = 0; 
volatile unsigned long tL = 0; 
volatile unsigned long pulseStart = 0;
volatile unsigned long pulseEnd = 0;

Adafruit_SHT31 sht30 = Adafruit_SHT31();


int co2_ppm = 0;
float temperature = 0.0;
float humidity = 0.0;
int button1 = 1;
int button2 = 0;
int button3 = 0;
int button4 = 0;
int button5 = 0;
int button6 = 0;
int button7 = 0;
int fan1_duty_percent = 0;
int fan2_duty_percent = 0;

String humidifier_status = "OFF";
String heating_status = "OFF";

// CẤU HÌNH
#define FAN1_PIN 17     
#define FAN2_PIN 16     
#define HUMIDIFIER_PIN 27
#define HEATING_PIN 26

#define FAN_PWM_FREQ    25000   // 25 kHz
#define FAN_PWM_RES     8       // 0–255
#define FAN_MIN_DUTY    63      // Duty tối thiểu khi BẬT quạt
#define FAN_MAX_DUTY    255



void connectWiFi() {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
}

void read_button() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(READ_BUTTON_URL);
    int httpCode = http.GET();

    if (httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<300> doc;

        if (!deserializeJson(doc, payload) && doc["data"] != nullptr) {
            button1 = doc["data"]["button1"];
            button2 = doc["data"]["button2"];
            button3 = doc["data"]["button3"];
            button4 = doc["data"]["button4"];
            button5 = doc["data"]["button5"];
            button6 = doc["data"]["button6"];
            button7 = doc["data"]["button7"];
        }
    }
    http.end();
}


void manualControl() {
    // FAN 1
    fan1_duty_percent = constrain(button2, 0, 100);
    int duty1 = (fan1_duty_percent == 0) ? 0 :
        map(fan1_duty_percent, 0, 100, FAN_MIN_DUTY, FAN_MAX_DUTY);
    ledcWrite(FAN1_PIN, duty1);

    // FAN 2
    fan2_duty_percent = constrain(button3, 0, 100);
    int duty2 = (fan2_duty_percent == 0) ? 0 :
        map(fan2_duty_percent, 0, 100, FAN_MIN_DUTY, FAN_MAX_DUTY);
    ledcWrite(FAN2_PIN, duty2);

    // PHUN SƯƠNG
    digitalWrite(HUMIDIFIER_PIN, button4 ? HIGH : LOW);
    humidifier_status = button4 ? "ON" : "OFF";

    // NUNG NHIỆT
    digitalWrite(HEATING_PIN, button5 ? HIGH : LOW);
    heating_status = button5 ? "ON" : "OFF";
}


void sendSensorData() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    http.begin(SERVER_URL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["temperature"] = temperature;
    doc["humidity"]    = humidity;
    doc["CO2"]         = co2_ppm;

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);


    http.end();
}

void sendDeviceStatus() {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    http.begin(STATUS_DEVICE_URL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["fan1_duty"]   = fan1_duty_percent;
    doc["fan2_duty"]   = fan2_duty_percent;
    doc["humidifier"]  = humidifier_status;
    doc["heating"]     = heating_status;

    String payload;
    serializeJson(doc, payload);

    int httpCode = http.POST(payload);

    http.end();
}

void readSHT30() {
  temperature = sht30.readTemperature(); 
  humidity = sht30.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    temperature = -99.0;
    humidity = -99.0;
  }
}


int calculateCO2(unsigned long highTime, unsigned long lowTime) {
    unsigned long cycleTime = highTime + lowTime;
    if (cycleTime < 500000) { return -1; } 
    float dutyCycle = (float)highTime / cycleTime;
    int co2_ppm = (int)(5000.0 * dutyCycle); 
    if (co2_ppm < 400) { co2_ppm = 400; }
    return co2_ppm;
}

void IRAM_ATTR readCO2PWM() {
    int state = digitalRead(CO2_PWM_PIN);
    unsigned long currentTime = micros();

    if (state == HIGH) {
        pulseStart = currentTime;
        if (pulseEnd > 0) {
            tL = pulseStart - pulseEnd; 
        }
    } else {
        pulseEnd = currentTime;
        if (pulseStart > 0) {
            tH = pulseEnd - pulseStart; 
        }
    }
}


void control_ketto() {
    // ===== FAN 1 =====
    int duty1 = 0;
    //if (humidity < 92) duty1 = 10;
    //else if (co2_ppm > 1800) duty1 = 20;
    //else duty1 = 0;
    if (humidity < 92) { float Kp_humidity = 5.0; duty1 = (int)((92 - humidity) * Kp_humidity); }
    else if (co2_ppm > 1800) { float Kp_co2_ppm = 1.0 / 20.0; duty1 = (int)((co2_ppm - 1800) * Kp_co2_ppm); }
    else if (temperature > 33.5) { float Kp_temperature = 30; duty1 = (int)((temperature - 33.5) * Kp_temperature); }
    else duty1 = 0;
    duty1 = constrain(duty1, 0, 100);

    ledcWrite(FAN1_PIN, duty1 == 0 ? 0 :
              map(duty1, 0, 100, FAN_MIN_DUTY, FAN_MAX_DUTY));
    fan1_duty_percent = duty1;

    // ===== FAN 2 =====
    int duty2 = 0;
    //if (temperature > 35) duty2 = 70;
    //else if (temperature > 34) duty2 = 40;
    //else if (temperature > 33.5) duty2 = 20;
    //else if (humidity >= 99) duty2 = 15;
    //else if (co2_ppm > 1800) duty2 = 20;
    //else duty2 = 0;
    if (humidity >= 99) { float Kp = 30; duty2 = (int)((humidity - 99) * Kp); }
    else if (co2_ppm > 1800) { float Kp_co2_ppm = 1.0 / 20.0; duty2 = (int)((co2_ppm - 1800) * Kp_co2_ppm); }
    else if (temperature > 33.5) { float Kp_temperature = 30; duty2 = (int)((temperature - 33.5) * Kp_temperature); }
    else duty2 = 0;
    duty2 = constrain(duty2, 0, 100);

    ledcWrite(FAN2_PIN, duty2 == 0 ? 0 :
              map(duty2, 0, 100, FAN_MIN_DUTY, FAN_MAX_DUTY));
    fan2_duty_percent = duty2;

    // ===== PHUN SƯƠNG =====
    if (humidity < 92 && !humidifierRunning) {
        digitalWrite(HUMIDIFIER_PIN, HIGH);
        humidifier_status = "ON";
        humidifierStartTime = millis();
        humidifierRunning = true;
    }
    if (humidifierRunning && millis() - humidifierStartTime >= 2000) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifier_status = "OFF";
        humidifierRunning = false;
    }

    // ===== Nung nhiệt =====
    digitalWrite(HEATING_PIN, temperature < 33.0 ? HIGH : LOW);
    heating_status = (temperature < 33.0) ? "ON" : "OFF";
}


void control_quathe() {
    // ===== FAN 1 =====
    int duty1 = 0;
    if (humidity < 88) { float Kp_humidity = 3.0; duty1 = (int)((88 - humidity) * Kp_humidity); }
    else if (co2_ppm > 1200) { float Kp_co2_ppm = 1.0 / 20.0; duty1 = (int)((co2_ppm - 1200) * Kp_co2_ppm); }
    else if (temperature > 31) { float Kp_temperature = 3.0; duty1 = (int)((temperature - 31) * Kp_temperature); }
    else duty1 = 0;
    duty1 = constrain(duty1, 0, 100);

    ledcWrite(FAN1_PIN, duty1 == 0 ? 0 :
              map(duty1, 0, 100, FAN_MIN_DUTY, FAN_MAX_DUTY));
    fan1_duty_percent = duty1;

    // ===== FAN 2 =====
    int duty2 = 0;
    if (humidity > 95) { float Kp = 3.0; duty2 = (int)((humidity - 95) * Kp); }
    else if (co2_ppm > 1200) { float Kp_co2_ppm = 1.0 / 20.0; duty2 = (int)((co2_ppm - 1200) * Kp_co2_ppm); }
    else if (temperature > 31) { float Kp_temperature = 3.0; duty2 = (int)((temperature - 31) * Kp_temperature); }
    else duty2 = 0;
    duty2 = constrain(duty2, 0, 100);

    ledcWrite(FAN2_PIN, duty2 == 0 ? 0 :
              map(duty2, 0, 100, FAN_MIN_DUTY, FAN_MAX_DUTY));
    fan2_duty_percent = duty2;

    // ===== PHUN SƯƠNG =====
    if (humidity < 88 && !humidifierRunning) {
        digitalWrite(HUMIDIFIER_PIN, HIGH);
        humidifier_status = "ON";
        humidifierStartTime = millis();
        humidifierRunning = true;
    }
    if (humidifierRunning && millis() - humidifierStartTime >= 2000) {
        digitalWrite(HUMIDIFIER_PIN, LOW);
        humidifier_status = "OFF";
        humidifierRunning = false;
    }

    // ===== Nung nhiệt =====
    digitalWrite(HEATING_PIN, temperature < 30.0 ? HIGH : LOW);
    heating_status = (temperature < 30.0) ? "ON" : "OFF";
}

void drawOLED() {
    u8g2.clearBuffer();

    String modeStr;
    if (button1 == 0) {
        modeStr = "MANUAL";
    } else {
        if (button6 == 1 && button7 == 0)      modeStr = "AUTO - KET TO";
        else if (button6 == 0 && button7 == 1) modeStr = "AUTO - QUAT THE";
        else                                   modeStr = "AUTO";
    }

    u8g2.setFont(u8g2_font_6x12_tr);

    u8g2.setCursor(2, 12);
    u8g2.print("T:");
    u8g2.print(temperature, 1);
    u8g2.print("C");

    u8g2.setCursor(50, 12);
    u8g2.print("H:");
    u8g2.print(humidity, 0);
    u8g2.print("%");

    u8g2.setCursor(83, 12);
    u8g2.print("CO2:");
    u8g2.print(co2_ppm > 0 ? co2_ppm : 0);

    u8g2.drawLine(0, 15, 127, 15);

    /* ====== TRUNG TÂM: CHẾ ĐỘ ====== */
    u8g2.setFont(u8g2_font_ncenB10_tr);
    int modeWidth = u8g2.getStrWidth(modeStr.c_str());
    u8g2.setCursor((128 - modeWidth) / 2, 38);
    u8g2.print(modeStr);

    /* ====== FOOTER: THIẾT BỊ ====== */
    u8g2.drawLine(0, 42, 127, 42);
    u8g2.setFont(u8g2_font_5x8_tr);

    u8g2.setCursor(2, 62);
    u8g2.print("F1:");
    u8g2.print(fan1_duty_percent);
    u8g2.print("%");

    u8g2.setCursor(35, 62);
    u8g2.print("F2:");
    u8g2.print(fan2_duty_percent);
    u8g2.print("%");

    u8g2.setCursor(70, 62);
    u8g2.print("CA:");
    u8g2.print(humidifier_status == "ON" ? "ON" : "OFF");

    u8g2.setCursor(100, 62);
    u8g2.print("CN:");
    u8g2.print(heating_status == "ON" ? "ON" : "OFF");

    u8g2.sendBuffer();
}

void setup() {

    connectWiFi();
    Wire.begin(); 
    u8g2.begin();
    u8g2.setFont(u8g2_font_ncenB10_tr); 
    if (!sht30.begin(0x44)) { Serial.println("Could not find SHT30"); }
    pinMode(CO2_PWM_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(CO2_PWM_PIN), readCO2PWM, CHANGE);


    //Cấu hình PWM cho Quạt
    ledcAttach(FAN1_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
    ledcAttach(FAN2_PIN, FAN_PWM_FREQ, FAN_PWM_RES);
    
    // Trạng thái các thiết bị chấp hành ban đầu
    ledcWrite(FAN1_PIN, 0); 
    ledcWrite(FAN2_PIN, 0);
    pinMode(HUMIDIFIER_PIN, OUTPUT);
    digitalWrite(HUMIDIFIER_PIN, LOW);
    pinMode(HEATING_PIN, OUTPUT);
    digitalWrite(HEATING_PIN, LOW);
}

void loop() {

    read_button();
    
    // ==== ĐỌC CO2 ====
    noInterrupts();
    unsigned long current_tH = tH;
    unsigned long current_tL = tL;
    tH = 0;
    tL = 0;
    interrupts();

    if (current_tH > 0 && current_tL > 0) {
        co2_ppm = calculateCO2(current_tH, current_tL);
    } else {
        co2_ppm = 0;
    }

    readSHT30();

    if (button1 == 1) {
        if (button6 == 1 && button7 == 0) {control_ketto();}
        else if (button6 == 0 && button7 == 1) {control_quathe();}
    } else {
        manualControl();
    }

    drawOLED();

    unsigned long now = millis();
    if (now - lastSendTime >= SEND_INTERVAL) {
        lastSendTime = now;
        sendSensorData();
        sendDeviceStatus();
    }

    delay(1000);
}

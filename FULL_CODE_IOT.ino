#define BLYNK_TEMPLATE_ID "TMPL6ahF784hO"
#define BLYNK_TEMPLATE_NAME "AI FARMING"
#define BLYNK_AUTH_TOKEN "EjDTTx53glCKykibuLGqgAOMIUXN2PE8"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <VL53L0X.h>
#include <BlynkSimpleEsp32.h>

// ---------------- LCD ----------------
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ---------------- WiFi & Blynk ----------------
const char* ssid = "IPHONE 17 PRO MAX";
const char* password = "Azam1111";
char auth[] = "EjDTTx53glCKykibuLGqgAOMIUXN2PE8";

// ---------------- OpenWeatherMap ----------------
String URL = "http://api.openweathermap.org/data/2.5/weather?";
String ApiKey = "0e89cd95c00a211987107c36a93a8539";
String lat = "6.460716038346288";
String lon = "100.4303498";

// ---------------- Google Sheets ----------------
String googleScriptURL = "https://script.google.com/macros/s/AKfycbyh5NZpdUeForRbpC157Z8AhGRXJXgNsHwHmy97kCkKBwpVWZ1mW5ywd1efStCqCou75g/exec";

// ---------------- DHT ----------------
#define DHTPIN 17
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ---------------- Soil Moisture ----------------
#define MOISTURE_SENSOR_PIN 4
#define RELAY_PUMP 21
int lowWaterThreshold = 2700;
int highWaterThreshold = 2900;
String pumpStatus = "Off";

// ===== Soil Moisture % =====
int soilDry = 3000;
int soilWet = 1200;
int soilMoisturePercent = 0;

// ---------------- LDR ----------------
#define LDR_PIN 5
#define RELAY_LIGHT 18
int thresholdNight = 3000;
String lampStatus = "Off";

// ---------------- VL53L0X ----------------
VL53L0X sensor;
float height_cm = 0;
float sensorToSoil_cm = 38.0;

// ================= TIMER =================
unsigned long previousLCDMillis = 0;
unsigned long previousSheetMillis = 0;
const unsigned long lcdInterval = 8000;
const unsigned long sheetInterval = 120000;

// ================= AI NOTIFICATION =================
String plantStage = "Seedling";
bool matureNotified = false;
unsigned long lastDryNotify = 0;
const unsigned long dryNotifyInterval = 1800000; // 30 min

// =================================================================
void setup() {
  Serial.begin(115200);

  Wire.begin(8, 9);
  lcd.init();
  lcd.backlight();
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }

  lcd.clear();
  lcd.print("WiFi Connected");
  delay(1500);

  Blynk.begin(auth, ssid, password);
  dht.begin();

  pinMode(RELAY_PUMP, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  digitalWrite(RELAY_PUMP, LOW);
  digitalWrite(RELAY_LIGHT, LOW);

  if (!sensor.init()) {
    lcd.clear();
    lcd.print("VL53L0X ERROR");
    while (1);
  }
  sensor.startContinuous();
}

// =================================================================
void loop() {
  Blynk.run();
  unsigned long currentMillis = millis();

  // ---------- SENSOR READ ----------
  float temp = dht.readTemperature();
  float humd = dht.readHumidity();
  int moistureValue = analogRead(MOISTURE_SENSOR_PIN);
  int ldrValue = analogRead(LDR_PIN);

  int dist_mm = sensor.readRangeContinuousMillimeters();
  float distance_cm = dist_mm / 10.0;

  height_cm = sensorToSoil_cm - distance_cm;
  if (height_cm < 0) height_cm = 0;

  // ---------- SOIL MOISTURE % ----------
  soilMoisturePercent = map(moistureValue, soilDry, soilWet, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);

  // ---------- PUMP ----------
  if (moistureValue >= highWaterThreshold) {
    digitalWrite(RELAY_PUMP, HIGH);
    pumpStatus = "On";
  } else {
    digitalWrite(RELAY_PUMP, LOW);
    pumpStatus = "Off";
  }

  // ---------- LAMP ----------
  if (ldrValue > thresholdNight) {
    digitalWrite(RELAY_LIGHT, HIGH);
    lampStatus = "Off";
  } else {
    digitalWrite(RELAY_LIGHT, LOW);
    lampStatus = "On";
  }

  // ================= AI PLANT STAGE =================
  if (height_cm < 3) {
    plantStage = "Seedling";
  } else if (height_cm < 7) {
    plantStage = "Growing";
  } else {
    plantStage = "Mature";
  }

  // ================= AI DRY SOIL ALERT =================
  if (soilMoisturePercent < 50 && pumpStatus == "Off") {
    if (millis() - lastDryNotify > dryNotifyInterval) {
      Blynk.logEvent("dry_water",
        "⚠️ AI: The soil is almost dry. Please water in 30 minutes..");
      lastDryNotify = millis();
    }
  }

  // ================= AI MATURE ALERT =================
  if (plantStage == "Mature" && !matureNotified) {
    Blynk.logEvent("mature_tree",
      "🌱 AI Detects: The sapling is mature and ready to be transplanted.");
    matureNotified = true;
  }

  // ================= LCD =================
  if (currentMillis - previousLCDMillis >= lcdInterval) {
    previousLCDMillis = currentMillis;
    lcd.clear();

    // -------- OpenWeatherMap --------
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String fullURL = URL + "lat=" + lat + "&lon=" + lon + "&units=metric&appid=" + ApiKey;
      http.begin(fullURL);
      if (http.GET() > 0) {
        DynamicJsonDocument doc(2048);
        deserializeJson(doc, http.getString());

        lcd.setCursor(0,0);
        lcd.print(doc["weather"][0]["description"].as<const char*>());

        lcd.setCursor(0,1);
        lcd.print("Temp:");
        lcd.print(doc["main"]["temp"].as<float>(),1);
        lcd.print("C Hum:");
        lcd.print(doc["main"]["humidity"].as<float>(),0);
        lcd.print("%");
      }
      http.end();
    }

    // -------- Soil + Height + Temp + Hum --------
    lcd.setCursor(0,2);
    lcd.print("Soil:");
    lcd.print(soilMoisturePercent);
    lcd.print("% H:");
    lcd.print(height_cm,1);
    lcd.print("cm");

    lcd.setCursor(0,3);
    lcd.print("Temp:");
    lcd.print(temp,1);
    lcd.print("C Hum:");
    lcd.print(humd,0);
    lcd.print("%");
  }

  // ================= GOOGLE SHEETS =================
  if (currentMillis - previousSheetMillis >= sheetInterval) {
    previousSheetMillis = currentMillis;
    sendToGoogleSheets(temp, humd, soilMoisturePercent, height_cm);
  }

  // ================= BLYNK =================
  Blynk.virtualWrite(V0, temp);
  Blynk.virtualWrite(V1, humd);
  Blynk.virtualWrite(V2, soilMoisturePercent);
  Blynk.virtualWrite(V3, height_cm);
  Blynk.virtualWrite(V4, pumpStatus == "On" ? 1 : 0);
  Blynk.virtualWrite(V5, lampStatus == "On" ? 1 : 0);
  Blynk.virtualWrite(V6, plantStage);
}

// =================================================================
void sendToGoogleSheets(float temperature, float humidity, int soilValue, float heightValue) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = googleScriptURL;
    url += "?sts=write";
    url += "&temp=" + String(temperature, 1);
    url += "&humd=" + String(humidity, 1);
    url += "&swtc1=" + pumpStatus;
    url += "&swtc2=" + lampStatus;
    url += "&soil=" + String(soilValue);
    url += "&height=" + String(heightValue, 1);
    http.begin(url);
    http.GET();
    http.end();
  }
}
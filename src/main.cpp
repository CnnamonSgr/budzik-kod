#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h> // Do zapamiętywania WiFi
#include "Audio.h"
#include "SD.h"
#include "SPI.h"
#include "time.h"
#include "secrets.h" // Dane tajne

// PINY
#define I2S_LRC 25
#define I2S_BCLK 26
#define I2S_DOUT 32
#define SD_CS 5
#define BATTERY_PIN 34
#define BUTTON_PIN 4

// KONFIGURACJA
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;      
const int   daylightOffset_sec = 3600; 

// OBIEKTY
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Audio audio;
Preferences preferences;

// ZMIENNE GLOBALNE
float filtrowaneNapiecie = 0;
bool alarmAktywny = false;
String godzinaAlarmu = "07:00";
int wyswietlanyProcent = 0;

// --- FUNKCJA ODCZYTU BATERII (FILTR EMA) ---
int pobierzStabilneProcenty() {
  long sumaRaw = 0;
  for(int i = 0; i < 30; i++) sumaRaw += analogRead(BATTERY_PIN);
  float vBat = ((float)sumaRaw / 30 / 4095.0) * 3.3 * 2.0 * 1.07;
  if (filtrowaneNapiecie == 0) filtrowaneNapiecie = vBat;
  filtrowaneNapiecie = (vBat * 0.1) + (filtrowaneNapiecie * 0.9);
  return constrain(map(filtrowaneNapiecie * 100, 335, 415, 0, 100), 0, 100);
}

// --- POBIERANIE KONFIGURACJI Z SERWERA (Z KLUCZEM API) ---
void pobierzKonfiguracje() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(SERVER_URL) + "/robot/config");
    http.addHeader("X-API-KEY", API_KEY); // Zabezpieczenie
    int httpCode = http.GET();
    if (httpCode == 200) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, http.getString());
      godzinaAlarmu = doc["godzina"].as<String>();
    }
    http.end();
  }
}

// --- SPRAWDZANIE CZY UŻYTKOWNIK ROZWIĄZAŁ ZAGADKĘ ---
void sprawdzStatusSerwera() {
  if (WiFi.status() == WL_CONNECTED && alarmAktywny) {
    HTTPClient http;
    http.begin(String(SERVER_URL) + "/robot/status");
    http.addHeader("X-API-KEY", API_KEY); // Zabezpieczenie
    int httpCode = http.GET();
    if (httpCode == 200) {
      String status = http.getString();
      if (status == "idle") { // Odpowiedź serwera
        alarmAktywny = false;
        audio.stopSong();
      }
    }
    http.end();
  }
}

void audio_eof_mp3(const char *info){
  if(alarmAktywny) audio.connecttoFS(SD, "/test_dzwiek.mp3");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED błąd!");
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();

  // ODCZYT WIFI Z PAMIĘCI
  preferences.begin("wifi-config", true);
  String ssid = preferences.getString("ssid", "");
  String pass = preferences.getString("pass", "");
  preferences.end();

  if (ssid != "") {
    display.setCursor(0, 20);
    display.println("Laczenie: " + ssid);
    display.display();
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      timeout++;
    }
  }

  if(WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    pobierzKonfiguracje();
  } else {
    // JEŚLI NIE MA WIFI -> TUTAJ MOŻNA DODAĆ TRYB KONFIGURACJI (Access Point)
    Serial.println("Brak WiFi. Dzialam w trybie offline.");
  }

  if(!SD.begin(SD_CS)) Serial.println("SD błąd!");

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(15);
}

void loop() {
  audio.loop(); 

  // 1. PRZYCISK (Snooze manualny)
  if (digitalRead(BUTTON_PIN) == LOW && alarmAktywny) {
    alarmAktywny = false;
    audio.stopSong();
    delay(200);
  }

  // 2. LOGIKA CZASOWA (Co sekundę)
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 1000) {
    struct tm timeinfo;
    char nowTime[6]; 
    char fullTime[9];
    
    if(getLocalTime(&timeinfo)){
      sprintf(nowTime, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      sprintf(fullTime, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      
      // SPRAWDZANIE CZY CZAS NA ALARM
      if (String(nowTime) == godzinaAlarmu && !alarmAktywny && timeinfo.tm_sec < 2) {
        alarmAktywny = true;
        audio.connecttoFS(SD, "/test_dzwiek.mp3");
        // Powiadom serwer, że budzik ruszył (żeby apka wiedziała, by pokazać pytanie)
        HTTPClient http;
        http.begin(String(SERVER_URL) + "/robot/trigger");
        http.addHeader("X-API-KEY", API_KEY);
        http.POST("");
        http.end();
      }
    }

    if (alarmAktywny) sprawdzStatusSerwera();

    // RYSOWANIE EKRANU
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(95, 0);
    display.print(pobierzStabilneProcenty()); display.print("%");
    
    display.setTextSize(2);
    display.setCursor(16, 25);
    display.print(fullTime);

    display.setTextSize(1);
    display.setCursor(0, 55);
    if(alarmAktywny) display.print("ALARM: GRA");
    else display.print("BUDZIK NA: " + godzinaAlarmu);
    
    display.display();
    lastUpdate = millis();
  }
}
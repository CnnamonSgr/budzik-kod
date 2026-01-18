#include <Arduino.h>

// --- PINY SILNIKÓW (Zgodnie z Twoim schematem KiCad) ---
const int IN1 = 14; // Silnik A (Lewy)
const int IN2 = 27; // Silnik A (Lewy)
const int IN3 = 33; // Silnik B (Prawy)
const int IN4 = 13; // Silnik B (Prawy)

// --- USTAWIENIA PWM ---
const int freq = 500;      // 500Hz - idealne dla siły silników N20
const int resolution = 8;  // Rozdzielczość 0-255

void setup() {
  Serial.begin(115200);
  Serial.println("--- TEST NAPEDU START ---");

  // Konfiguracja kanałów PWM dla ESP32
  ledcSetup(0, freq, resolution); // Kanał 0 -> IN1
  ledcSetup(1, freq, resolution); // Kanał 1 -> IN2
  ledcSetup(2, freq, resolution); // Kanał 2 -> IN3
  ledcSetup(3, freq, resolution); // Kanał 3 -> IN4

  // Przypisanie pinów do kanałów
  ledcAttachPin(IN1, 0);
  ledcAttachPin(IN2, 1);
  ledcAttachPin(IN3, 2);
  ledcAttachPin(IN4, 3);

  Serial.println("System gotowy. Start za 3 sekundy...");
  delay(3000);
}

void stopMotors() {
  ledcWrite(0, 0); ledcWrite(1, 0);
  ledcWrite(2, 0); ledcWrite(3, 0);
}

void loop() {
  // 1. JAZDA DO PRZODU (Moc 180/255)
  Serial.println("Jazda do przodu...");
  ledcWrite(0, 120); ledcWrite(1, 0);
  ledcWrite(2, 120); ledcWrite(3, 0);
  delay(2000);

  // 2. STOP
  Serial.println("Stop...");
  stopMotors();
  delay(3000);

  // 3. JAZDA DO TYŁU
  Serial.println("Jazda do tylu...");
  ledcWrite(0, 0); ledcWrite(1, 120);
  ledcWrite(2, 0); ledcWrite(3, 120);
  delay(2000);

  // 4. STOP
  Serial.println("Stop...");
  stopMotors();
  delay(5000);
}
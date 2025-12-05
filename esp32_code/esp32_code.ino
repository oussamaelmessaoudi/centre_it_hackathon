#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <WiFi.h>
#include "time.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define PULSE_SENSOR_PIN 34
#define BUTTON_PIN 16
#define LED_PIN 2     // LED pour détection de chute
#define BUZZER_PIN 17 // Buzzer pour alerte sonore

// ========== Fall Detection Configuration ==========
#define FALL_THRESHOLD 15.0       // Seuil d'accélération pour détecter une chute (m/s²)
#define FALL_DURATION 100         // Durée minimale de l'alerte (ms)
#define LED_ALERT_DURATION 5000   // Durée d'allumage de la LED (5 secondes)

// ========== WiFi & NTP Configuration ==========
const char* ssid = "HUAWEI_B320_C01C";
const char* password = "Yh6RTN23eFN";

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_MPU6050 mpu;

// Variables pour le capteur de pouls
int pulseSignal = 0;
int filteredSignal = 0;
int bpm = 0;
unsigned long lastBeatTime = 0;
int threshold = 2000;
bool beatDetected = false;
int signalMin = 4095;
int signalMax = 0;

// Buffer pour filtrage
#define FILTER_SIZE 4
int signalBuffer[FILTER_SIZE];
int bufferIndex = 0;

// Variables pour l'animation du coeur
int heartSize = 0;
unsigned long lastHeartBeat = 0;

// Variables pour la page d'accueil
int batteryLevel = 85;
float animationPhase = 0;
int notificationCount = 3;
bool timeAvailable = false;
bool showSplashScreen = true;
unsigned long splashStartTime = 0;
float logoAnimPhase = 0;

// Variables pour l'animation MPU
float graphX[30];
int graphIndex = 0;

// Variables pour le bouton
int displayMode = 0;
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// ========== Variables pour la détection de chute ==========
bool fallDetected = false;
unsigned long fallDetectionTime = 0;
unsigned long ledAlertStartTime = 0;
bool ledAlertActive = false;
int fallCount = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Health Monitor with Fall Detection ===");

  Wire.begin(21, 22);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);     // Configurer la LED
  pinMode(BUZZER_PIN, OUTPUT);  // Configurer le buzzer
  digitalWrite(LED_PIN, LOW);   // LED éteinte au démarrage
  digitalWrite(BUZZER_PIN, LOW); // Buzzer éteint au démarrage

  // Initialiser l'écran OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ Écran OLED introuvable!");
    while (1);
  }
  Serial.println("✓ Écran OLED OK");

  // Afficher le logo de démarrage
  splashStartTime = millis();
  while (millis() - splashStartTime < 3000) {
    displaySplashScreen();
  }
  showSplashScreen = false;

  // Connexion WiFi (silencieuse, en arrière-plan)
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(40, 28);
  display.println("...");
  display.display();
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10) {
      delay(1000);
      retries++;
    }
    
    if (getLocalTime(&timeinfo)) {
      timeAvailable = true;
    }
    
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }

  // Initialiser le MPU6050
  if (!mpu.begin()) {
    Serial.println("❌ MPU6050 introuvable!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("MPU6050 ERROR!");
    display.display();
    while (1);
  }
  Serial.println("✓ MPU6050 OK");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  analogReadResolution(12);
  pinMode(PULSE_SENSOR_PIN, INPUT);

  for (int i = 0; i < FILTER_SIZE; i++) {
    signalBuffer[i] = 0;
  }

  for (int i = 0; i < 30; i++) {
    graphX[i] = 0;
  }

  // Calibration du capteur de pouls (silencieuse)
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(40, 28);
  display.println("...");
  display.display();
  
  unsigned long calibrationStart = millis();
  int sampleCount = 0;
  
  while (millis() - calibrationStart < 5000) {
    pulseSignal = analogRead(PULSE_SENSOR_PIN);
    
    if (pulseSignal > 100 && pulseSignal < 4000) {
      if (pulseSignal < signalMin) signalMin = pulseSignal;
      if (pulseSignal > signalMax) signalMax = pulseSignal;
      sampleCount++;
    }
    
    delay(10);
  }
  
  int range = signalMax - signalMin;
  threshold = signalMin + (range * 0.6);
  
  // Démarrer l'écran splash après la configuration
  splashStartTime = millis();
  showSplashScreen = true;

  Serial.println("✓ Fall Detection Ready");
}

// ========== Fonction de détection de chute ==========
void checkFallDetection() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  // Calculer la magnitude totale de l'accélération
  float accelMagnitude = sqrt(
    a.acceleration.x * a.acceleration.x +
    a.acceleration.y * a.acceleration.y +
    a.acceleration.z * a.acceleration.z
  );
  
  // Détecter une chute soudaine (accélération élevée)
  if (accelMagnitude > FALL_THRESHOLD && !fallDetected) {
    fallDetected = true;
    fallDetectionTime = millis();
    ledAlertActive = true;
    ledAlertStartTime = millis();
    fallCount++;
    
    // Allumer la LED et le buzzer
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    
    // Log dans le Serial Monitor
    Serial.println("⚠️⚠️⚠️ FALL DETECTED! ⚠️⚠️⚠️");
    Serial.print("Acceleration Magnitude: ");
    Serial.print(accelMagnitude);
    Serial.println(" m/s²");
    Serial.print("Fall Count: ");
    Serial.println(fallCount);
    
    // Afficher alerte à l'écran
    displayFallAlert();
  }
  
  // Maintenir la LED et le buzzer allumés pendant LED_ALERT_DURATION
  if (ledAlertActive && (millis() - ledAlertStartTime > LED_ALERT_DURATION)) {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    ledAlertActive = false;
    fallDetected = false;
    Serial.println("✓ Fall alert cleared");
  }
  
  // Faire clignoter la LED et sonner le buzzer pendant l'alerte
  if (ledAlertActive) {
    if ((millis() / 250) % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }
}

// ========== Affichage d'alerte de chute ==========
void displayFallAlert() {
  display.clearDisplay();
  
  // Bordure d'alerte clignotante
  display.drawRect(0, 0, 128, 64, WHITE);
  display.drawRect(1, 1, 126, 62, WHITE);
  
  // Icône d'alerte (triangle avec !)
  display.fillTriangle(64, 15, 54, 30, 74, 30, WHITE);
  display.fillRect(62, 20, 4, 6, BLACK);
  display.fillRect(62, 27, 4, 2, BLACK);
  
  // Texte d'alerte
  display.setTextSize(2);
  display.setCursor(25, 35);
  display.println("FALL!");
  
  display.setTextSize(1);
  display.setCursor(20, 52);
  display.print("Alert #");
  display.print(fallCount);
  
  display.display();
  delay(2000); // Afficher l'alerte pendant 2 secondes
}

void drawHeart(int x, int y, int size) {
  display.fillCircle(x - size/2, y, size/2, WHITE);
  display.fillCircle(x + size/2, y, size/2, WHITE);
  display.fillTriangle(
    x - size, y,
    x + size, y,
    x, y + size,
    WHITE
  );
}

void drawLifeSaversLogo(int centerX, int centerY, int size, float animPhase) {
  int outerRadius = size;
  int innerRadius = size * 0.6;
  
  for (int angle = 0; angle < 360; angle += 3) {
    float rad = (angle + animPhase * 50) * PI / 180.0;
    int x1 = centerX + cos(rad) * outerRadius;
    int y1 = centerY + sin(rad) * outerRadius;
    int x2 = centerX + cos(rad) * innerRadius;
    int y2 = centerY + sin(rad) * innerRadius;
    
    if ((angle / 45) % 2 == 0) {
      display.drawLine(x1, y1, x2, y2, WHITE);
    }
  }
  
  for (int i = 0; i < 4; i++) {
    float angle = (i * 90 + animPhase * 30) * PI / 180.0;
    
    for (int r = innerRadius; r < outerRadius; r++) {
      int x = centerX + cos(angle) * r;
      int y = centerY + sin(angle) * r;
      display.drawPixel(x, y, WHITE);
      display.drawPixel(x+1, y, WHITE);
      display.drawPixel(x, y+1, WHITE);
    }
  }
  
  display.drawCircle(centerX, centerY, outerRadius, WHITE);
  display.drawCircle(centerX, centerY, outerRadius - 1, WHITE);
  display.drawCircle(centerX, centerY, innerRadius, WHITE);
  display.drawCircle(centerX, centerY, innerRadius + 1, WHITE);
  
  int crossSize = innerRadius * 0.5;
  display.fillRect(centerX - 2, centerY - crossSize, 4, crossSize * 2, WHITE);
  display.fillRect(centerX - crossSize, centerY - 2, crossSize * 2, 4, WHITE);
  
  float sparkAngle = animPhase * 2;
  int sparkX = centerX + cos(sparkAngle) * (outerRadius - 3);
  int sparkY = centerY + sin(sparkAngle) * (outerRadius - 3);
  display.fillCircle(sparkX, sparkY, 2, WHITE);
}

void displaySplashScreen() {
  display.clearDisplay();
  
  logoAnimPhase += 0.08;
  
  drawLifeSaversLogo(64, 28, 20, logoAnimPhase);
  
  int textSize = 1 + (sin(logoAnimPhase * 2) * 0.3);
  display.setTextSize(textSize > 0 ? textSize : 1);
  display.setCursor(30, 52);
  display.println("LifeSavers");
  
  for (int i = 0; i < 8; i++) {
    float angle = (logoAnimPhase + i * 0.785) * 2;
    int px = 64 + cos(angle) * (25 + sin(logoAnimPhase * 3) * 5);
    int py = 28 + sin(angle) * (25 + sin(logoAnimPhase * 3) * 5);
    display.drawPixel(px, py, WHITE);
    display.drawPixel(px+1, py, WHITE);
  }
  
  int progress = map(millis() - splashStartTime, 0, 3000, 0, 128);
  progress = constrain(progress, 0, 128);
  display.drawRect(0, 62, 128, 2, WHITE);
  display.fillRect(0, 62, progress, 2, WHITE);
  
  display.display();
}

void displayHome() {
  display.clearDisplay();
  
  logoAnimPhase += 0.05;
  drawLifeSaversLogo(12, 5, 4, logoAnimPhase);
  
  animationPhase += 0.1;
  if (animationPhase > 6.28) animationPhase = 0;
  
  for (int x = 0; x < 128; x += 4) {
    int y = 60 + sin(animationPhase + x * 0.05) * 3;
    display.drawPixel(x, y, WHITE);
    display.drawPixel(x, y + 1, WHITE);
  }
  
  display.drawLine(0, 10, 128, 10, WHITE);
  
  int batX = 100;
  display.drawRect(batX, 2, 20, 7, WHITE);
  display.fillRect(batX + 20, 4, 2, 3, WHITE);
  int batFill = map(batteryLevel, 0, 100, 0, 18);
  display.fillRect(batX + 1, 3, batFill, 5, WHITE);
  
  display.setTextSize(1);
  display.setCursor(batX - 18, 2);
  display.print(batteryLevel);
  display.print("%");
  
  // Afficher indicateur de chute si alerte active
  if (ledAlertActive) {
    display.fillTriangle(30, 2, 26, 8, 34, 8, WHITE);
    display.fillRect(29, 4, 2, 2, BLACK);
    display.fillRect(29, 7, 2, 1, BLACK);
  } else if (notificationCount > 0) {
    display.fillCircle(25, 5, 3, WHITE);
    display.setTextColor(BLACK);
    display.setCursor(23, 3);
    display.print(notificationCount);
    display.setTextColor(WHITE);
  }
  
  struct tm timeinfo;
  if (timeAvailable && getLocalTime(&timeinfo)) {
    display.setTextSize(3);
    display.setCursor(10, 18);
    if (timeinfo.tm_hour < 10) display.print("0");
    display.print(timeinfo.tm_hour);
    display.print(":");
    if (timeinfo.tm_min < 10) display.print("0");
    display.print(timeinfo.tm_min);
    
    display.setTextSize(1);
    display.setCursor(100, 28);
    if (timeinfo.tm_sec < 10) display.print("0");
    display.print(timeinfo.tm_sec);
    
    display.setTextSize(1);
    display.setCursor(15, 42);
    const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", 
                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    display.print(months[timeinfo.tm_mon]);
    display.print(" ");
    if (timeinfo.tm_mday < 10) display.print("0");
    display.print(timeinfo.tm_mday);
    display.print(", ");
    display.print(timeinfo.tm_year + 1900);
    
  } else {
    display.setTextSize(2);
    display.setCursor(20, 25);
    display.println("NO TIME");
    display.setTextSize(1);
    display.setCursor(15, 42);
    display.println("Check WiFi");
  }
  
  if (bpm > 0) {
    int hSize = 3;
    if (millis() - lastHeartBeat < 200) {
      hSize = 4;
    }
    drawHeart(10, 52, hSize);
    
    display.setTextSize(1);
    display.setCursor(18, 50);
    display.print(bpm);
    display.print(" BPM");
  }
  
  display.setTextSize(1);
  display.setCursor(110, 50);
  display.print("1/4");
  
  display.display();
}

int filterSignal(int newValue) {
  signalBuffer[bufferIndex] = newValue;
  bufferIndex = (bufferIndex + 1) % FILTER_SIZE;
  
  long sum = 0;
  for (int i = 0; i < FILTER_SIZE; i++) {
    sum += signalBuffer[i];
  }
  return sum / FILTER_SIZE;
}

void displayBPM() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(110, 0);
  display.println("2/4");
  
  display.setCursor(25, 0);
  display.println("HEART RATE");
  display.drawLine(0, 10, 128, 10, WHITE);
  
  display.setTextSize(3);
  display.setCursor(20, 20);
  if (bpm > 0 && bpm < 200) {
    display.print(bpm);
  } else {
    display.print("--");
  }
  
  display.setTextSize(1);
  display.setCursor(80, 35);
  display.println("BPM");
  
  if (millis() - lastHeartBeat < 200) {
    heartSize = map(millis() - lastHeartBeat, 0, 200, 10, 6);
  } else {
    heartSize = 6;
  }
  drawHeart(110, 25, heartSize);
  
  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print("R:");
  display.print(pulseSignal);
  display.print(" F:");
  display.print(filteredSignal);
  
  display.drawRect(0, 55, 128, 7, WHITE);
  int signalWidth = map(filteredSignal, 0, 4095, 0, 126);
  signalWidth = constrain(signalWidth, 0, 126);
  display.fillRect(1, 56, signalWidth, 5, WHITE);
  
  display.display();
}

void displayMPU() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(110, 0);
  display.println("3/4");
  
  display.setCursor(30, 0);
  display.println("MOVEMENT");
  display.drawLine(0, 10, 128, 10, WHITE);
  
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  display.setTextSize(1);
  display.setCursor(0, 14);
  display.print("X:");
  display.print(a.acceleration.x, 1);
  
  display.setCursor(65, 14);
  display.print("Y:");
  display.print(a.acceleration.y, 1);
  
  display.setCursor(0, 24);
  display.print("Z:");
  display.print(a.acceleration.z, 1);
  
  display.setCursor(65, 24);
  display.print("T:");
  display.print(temp.temperature, 1);
  display.print("C");
  
  // Afficher le compteur de chutes
  display.setCursor(0, 34);
  display.print("Falls: ");
  display.print(fallCount);
  
  graphX[graphIndex] = a.acceleration.x;
  graphIndex = (graphIndex + 1) % 30;
  
  int graphY = 42;
  int graphHeight = 18;
  display.drawRect(0, graphY, 128, graphHeight, WHITE);
  
  for (int i = 0; i < 29; i++) {
    int idx1 = (graphIndex + i) % 30;
    int idx2 = (graphIndex + i + 1) % 30;
    
    int y1 = graphY + graphHeight/2 - (int)(graphX[idx1] * 2);
    int y2 = graphY + graphHeight/2 - (int)(graphX[idx2] * 2);
    
    y1 = constrain(y1, graphY + 1, graphY + graphHeight - 2);
    y2 = constrain(y2, graphY + 1, graphY + graphHeight - 2);
    
    display.drawLine(i * 4, y1, (i + 1) * 4, y2, WHITE);
  }
  
  display.display();
}

void displayBoth() {
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(110, 0);
  display.println("4/4");
  
  display.setCursor(30, 0);
  display.println("OVERVIEW");
  display.drawLine(0, 10, 128, 10, WHITE);
  
  display.setTextSize(1);
  display.setCursor(0, 12);
  display.print("Heart:");
  display.setTextSize(2);
  display.setCursor(45, 12);
  if (bpm > 0 && bpm < 200) {
    display.print(bpm);
  } else {
    display.print("--");
  }
  display.setTextSize(1);
  display.setCursor(85, 18);
  display.print("BPM");
  
  if (millis() - lastHeartBeat < 200) {
    heartSize = 4;
  } else {
    heartSize = 3;
  }
  drawHeart(115, 18, heartSize);
  
  display.drawLine(0, 30, 128, 30, WHITE);
  
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  
  display.setTextSize(1);
  display.setCursor(0, 33);
  display.print("Move: X:");
  display.print(a.acceleration.x, 1);
  
  display.setCursor(0, 43);
  display.print("Y:");
  display.print(a.acceleration.y, 1);
  display.print(" Z:");
  display.print(a.acceleration.z, 1);
  
  display.setCursor(0, 53);
  display.print("Falls: ");
  display.print(fallCount);
  display.print(" | T:");
  display.print(temp.temperature, 0);
  display.print("C");
  
  display.display();
}

void readPulseSensor() {
  pulseSignal = analogRead(PULSE_SENSOR_PIN);
  filteredSignal = filterSignal(pulseSignal);
  
  static unsigned long lastDebugPrint = 0;
  if (millis() - lastDebugPrint > 1000) {
    Serial.print("Raw: ");
    Serial.print(pulseSignal);
    Serial.print(" | Filtered: ");
    Serial.print(filteredSignal);
    Serial.print(" | Threshold: ");
    Serial.print(threshold);
    Serial.print(" | BPM: ");
    Serial.print(bpm);
    Serial.print(" | Beat: ");
    Serial.println(beatDetected ? "YES" : "NO");
    lastDebugPrint = millis();
  }
  
  if (filteredSignal > threshold && !beatDetected) {
    beatDetected = true;
    unsigned long currentTime = millis();
    
    if (lastBeatTime > 0) {
      unsigned long interval = currentTime - lastBeatTime;
      
      if (interval > 300 && interval < 2000) {
        int instantBPM = 60000 / interval;
        
        if (instantBPM >= 40 && instantBPM <= 180) {
          if (bpm == 0) {
            bpm = instantBPM;
          } else {
            bpm = (bpm * 4 + instantBPM) / 5;
          }
          
          lastHeartBeat = millis();
          Serial.print("💓 BEAT! Interval: ");
          Serial.print(interval);
          Serial.print("ms | Instant BPM: ");
          Serial.print(instantBPM);
          Serial.print(" | Average BPM: ");
          Serial.println(bpm);
        }
      }
    }
    
    lastBeatTime = currentTime;
  }
  
  if (filteredSignal < threshold - 100) {
    beatDetected = false;
  }
  
  if (millis() - lastBeatTime > 3000) {
    if (bpm > 0) {
      Serial.println("⚠️ Timeout - Reset BPM");
      bpm = 0;
    }
  }
}

void checkButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      
      if (buttonState == LOW) {
        displayMode = (displayMode + 1) % 4;
        
        Serial.print("🔄 Mode: ");
        const char* modes[] = {"HOME", "BPM", "MPU", "OVERVIEW"};
        Serial.println(modes[displayMode]);
        
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(30, 25);
        display.print("MODE ");
        display.print(displayMode + 1);
        display.display();
        delay(300);
      }
    }
  }
  
  lastButtonState = reading;
}

void loop() {
  if (showSplashScreen) {
    if (millis() - splashStartTime < 3000) {
      displaySplashScreen();
      return;
    } else {
      showSplashScreen = false;
    }
  }
  
  // Vérifier la détection de chute
  checkFallDetection();
  
  readPulseSensor();
  checkButton();
  
  switch(displayMode) {
    case 0:
      displayHome();
      break;
    case 1:
      displayBPM();
      break;
    case 2:
      displayMPU();
      break;
    case 3:
      displayBoth();
      break;
  }
  
  delay(50);
}
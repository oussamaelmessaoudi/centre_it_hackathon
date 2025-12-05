#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== TEST SIMPLE ===");

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erreur OLED!");
    while (1);
  }

  Serial.println("Écran OK");
  
  // TEST: Afficher quelque chose de très simple
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(10, 20);
  display.println("TEST");
  display.display();
  
  delay(2000);
  
  // Maintenant un pattern simple
  display.clearDisplay();
  
  // Dessiner un damier simple au centre
  int startX = 32;
  int startY = 0;
  int boxSize = 8;
  
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if ((x + y) % 2 == 0) {
        display.fillRect(startX + x*boxSize, startY + y*boxSize, boxSize, boxSize, WHITE);
      }
    }
  }
  
  display.display();
  
  Serial.println("Pattern affiché");
  Serial.println("\nPouvez-vous voir:");
  Serial.println("1. Le texte 'TEST'?");
  Serial.println("2. Le damier noir et blanc?");
  Serial.println("\nSi OUI: L'écran fonctionne bien");
  Serial.println("Si NON: Problème matériel avec l'écran");
}

void loop() {
  delay(1000);
}
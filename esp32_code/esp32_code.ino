#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------------------
// DUMMY DATA (exemple bitmap)
// ----------------------------

// Bitmap 16x16 pixels (motif simple)
static const uint8_t dummyBitmap[] = {
  0b11111111, 0b11111111,
  0b10000000, 0b00000001,
  0b10111111, 0b11111001,
  0b10100000, 0b00001001,
  0b10101111, 0b11101001,
  0b10101000, 0b00101001,
  0b10101011, 0b10101001,
  0b10101000, 0b00101001,
  0b10101111, 0b11101001,
  0b10100000, 0b00001001,
  0b10111111, 0b11111001,
  0b10000000, 0b00000001,
  0b11111111, 0b11111111,
  0b00000000, 0b00000000,
  0b11110000, 0b00001111,
  0b11111111, 0b11111111
};

const int dummyWidth = 16;
const int dummyHeight = 16;

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22); // SDA = 21, SCL = 22
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Écran OLED introuvable !");
    while (true);
  }

  display.clearDisplay();

  // ----------------------------
  // Affichage du DUMMY BITMAP
  // ----------------------------
  display.drawBitmap(
    50, 20,              // Position X, Y
    dummyBitmap,         // Notre dummy data
    dummyWidth,          // Largeur
    dummyHeight,         // Hauteur
    SSD1306_WHITE        // Couleur
  );

  display.display();
}

void loop() {}

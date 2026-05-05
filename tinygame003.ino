#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel pixels(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

enum GameState {
  START_SCREEN,
  PLAYING,
  CLEAR_SCREEN
};

GameState currentState = START_SCREEN;

// Board representation
uint8_t board[5][5];
int emptyRow = 4;
int emptyCol = 4;

// Time tracking
uint32_t startTime = 0;
uint32_t currentElapsedMs = 0;
uint32_t clearTimeMs = 0;

// Button state tracking for Edge detection and Long Press
struct ButtonState {
  uint8_t pin;
  bool isPressed;
  bool lastState;
  uint32_t pressStartTime;
  bool longPressTriggered;
};

ButtonState btnUp = {BUTTON_UP, false, false, 0, false};
ButtonState btnDown = {BUTTON_DOWN, false, false, 0, false};
ButtonState btnLeft = {BUTTON_LEFT, false, false, 0, false};
ButtonState btnRight = {BUTTON_RIGHT, false, false, 0, false};

// Function prototypes
void initBoardToSolved();
void shuffleBoard(int moves);
bool moveTile(int dRow, int dCol);
bool checkClear();
void updateButtons();
void handleStartScreen();
void handlePlaying();
void handleClearScreen();
void drawStartScreen();
void drawPlaying();
void drawClearScreen();
void drawGrid();
void updateNeoPixel();
void playClick();
void playBeep();

void setup() {
  Serial.begin(115200);

  // Setup I2C for XIAO ESP32C3
  Wire.begin(OLED_SDA, OLED_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.display();

  pixels.begin();
  pixels.clear();
  pixels.show();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  initBoardToSolved();
  shuffleBoard(500); // Shuffle with 500 valid moves
}

void loop() {
  updateButtons();
  updateNeoPixel();

  switch (currentState) {
    case START_SCREEN:
      handleStartScreen();
      drawStartScreen();
      break;
    case PLAYING:
      handlePlaying();
      drawPlaying();
      break;
    case CLEAR_SCREEN:
      handleClearScreen();
      drawClearScreen();
      break;
  }
}

// ==========================================
// Game Logic
// ==========================================

void initBoardToSolved() {
  uint8_t num = 1;
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      if (r == 4 && c == 4) {
        board[r][c] = 0;
      } else {
        board[r][c] = num++;
      }
    }
  }
  emptyRow = 4;
  emptyCol = 4;
}

void shuffleBoard(int moves) {
  int lastMoveIdx = -1;
  int dx[] = {-1, 1, 0, 0};
  int dy[] = {0, 0, -1, 1}; // Up, Down, Left, Right
  
  for (int i = 0; i < moves; i++) {
    int validMoves[4];
    int numValid = 0;

    for (int m = 0; m < 4; m++) {
      // Prevent reversing the immediately preceding move to improve shuffle quality
      if ((m == 0 && lastMoveIdx == 1) || (m == 1 && lastMoveIdx == 0) ||
          (m == 2 && lastMoveIdx == 3) || (m == 3 && lastMoveIdx == 2)) {
        continue;
      }

      int newR = emptyRow + dy[m];
      int newC = emptyCol + dx[m];
      if (newR >= 0 && newR < 5 && newC >= 0 && newC < 5) {
        validMoves[numValid++] = m;
      }
    }

    if (numValid > 0) {
      int rIdx = random(numValid);
      int m = validMoves[rIdx];
      moveTile(dy[m], dx[m]);
      lastMoveIdx = m;
    }
  }
}

// Attempt to move a tile INTO the empty space.
// If dRow=-1, dCol=0, the tile ABOVE the empty space moves DOWN into the empty space.
// This means the empty space moves UP (dRow=-1).
bool moveTile(int dRow, int dCol) {
  int targetRow = emptyRow + dRow;
  int targetCol = emptyCol + dCol;

  if (targetRow >= 0 && targetRow < 5 && targetCol >= 0 && targetCol < 5) {
    board[emptyRow][emptyCol] = board[targetRow][targetCol];
    board[targetRow][targetCol] = 0;
    emptyRow = targetRow;
    emptyCol = targetCol;
    return true;
  }
  return false;
}

bool checkClear() {
  uint8_t expected = 1;
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      if (r == 4 && c == 4) {
        return board[r][c] == 0;
      }
      if (board[r][c] != expected++) {
        return false;
      }
    }
  }
  return true;
}

// ==========================================
// Input Handling
// ==========================================

void processButton(ButtonState &btn) {
  bool reading = (digitalRead(btn.pin) == LOW); // LOW means pressed

  if (reading && !btn.lastState) {
    // Just pressed
    btn.pressStartTime = millis();
    btn.isPressed = true;
    btn.longPressTriggered = false;
  } else if (!reading && btn.lastState) {
    // Just released
    btn.isPressed = false;
  }

  // Check long press
  if (reading && !btn.longPressTriggered && (millis() - btn.pressStartTime >= 3000)) {
    btn.longPressTriggered = true;
    
    // Global reset action on long press
    initBoardToSolved();
    shuffleBoard(500);
    currentState = START_SCREEN;
  }

  btn.lastState = reading;
}

void updateButtons() {
  processButton(btnUp);
  processButton(btnDown);
  processButton(btnLeft);
  processButton(btnRight);
}

// Helper to get simple press event (consumes it)
bool wasPressed(ButtonState &btn) {
  if (btn.isPressed && !btn.longPressTriggered) {
    btn.isPressed = false; // Consume the press
    return true;
  }
  return false;
}

// ==========================================
// State Handlers
// ==========================================

void handleStartScreen() {
  if (wasPressed(btnUp) || wasPressed(btnDown) || wasPressed(btnLeft) || wasPressed(btnRight)) {
    // Consume presses and start
    currentState = PLAYING;
    startTime = millis();
  }
}

void handlePlaying() {
  currentElapsedMs = millis() - startTime;
  
  bool moved = false;
  bool triedMove = false;

  // The design:
  // UP (D7): Tile below empty moves UP. Empty moves DOWN (+1, 0)
  // DOWN (D8): Tile above empty moves DOWN. Empty moves UP (-1, 0)
  // LEFT (D9): Tile right of empty moves LEFT. Empty moves RIGHT (0, +1)
  // RIGHT (D10): Tile left of empty moves RIGHT. Empty moves LEFT (0, -1)

  if (wasPressed(btnUp)) {
    triedMove = true;
    moved = moveTile(1, 0); 
  } else if (wasPressed(btnDown)) {
    triedMove = true;
    moved = moveTile(-1, 0);
  } else if (wasPressed(btnLeft)) {
    triedMove = true;
    moved = moveTile(0, 1);
  } else if (wasPressed(btnRight)) {
    triedMove = true;
    moved = moveTile(0, -1);
  }

  if (triedMove) {
    if (moved) {
      playClick();
      if (checkClear()) {
        clearTimeMs = millis() - startTime;
        currentState = CLEAR_SCREEN;
      }
    } else {
      playBeep();
    }
  }
}

void handleClearScreen() {
  if (wasPressed(btnUp) || wasPressed(btnDown) || wasPressed(btnLeft) || wasPressed(btnRight)) {
    initBoardToSolved();
    shuffleBoard(500);
    currentState = START_SCREEN;
  }
}

// ==========================================
// Drawing Functions
// ==========================================

void drawGrid() {
  // Grid size: 60x60 on the left side
  // Top left: 0,0
  // Cell size: 12x12
  
  display.drawRect(0, 0, 61, 61, SSD1306_WHITE);
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      int x = c * 12 + 1;
      int y = r * 12 + 1;
      
      uint8_t val = board[r][c];
      if (val != 0) {
        // Center text in 12x12 block
        // Standard font is 5x8 pixels
        // If 1 digit: w=5, h=8. offset_x = (12-5)/2 = 3. offset_y = (12-8)/2 = 2
        // If 2 digits: w=11, h=8. offset_x = (12-11)/2 = 0.
        int tx = x + (val < 10 ? 3 : 0);
        int ty = y + 2;
        
        display.setCursor(tx, ty);
        display.print(val);
      }
    }
  }
}

void drawStartScreen() {
  display.clearDisplay();
  
  drawGrid();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(68, 10);
  display.print(F("Time:"));
  display.setCursor(68, 20);
  display.print(F("0.0s"));
  
  display.setCursor(68, 40);
  display.print(F("Press"));
  display.setCursor(68, 50);
  display.print(F("to Start"));

  display.display();
}

void drawPlaying() {
  display.clearDisplay();
  
  drawGrid();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(68, 10);
  display.print(F("Time:"));
  
  // Format time in 1/10 seconds (e.g., 12.3)
  uint32_t totalTenths = currentElapsedMs / 100;
  display.setCursor(68, 20);
  display.print(totalTenths / 10);
  display.print(F("."));
  display.print(totalTenths % 10);
  display.print(F("s"));
  
  display.display();
}

void drawClearScreen() {
  display.clearDisplay();
  
  drawGrid();
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(68, 0);
  display.print(F("GAME"));
  display.setCursor(68, 10);
  display.print(F("CLEAR!"));
  
  display.setCursor(68, 25);
  display.print(F("Time:"));
  uint32_t totalTenths = clearTimeMs / 100;
  display.setCursor(68, 35);
  display.print(totalTenths / 10);
  display.print(F("."));
  display.print(totalTenths % 10);
  display.print(F("s"));

  display.display();
}

// ==========================================
// Hardware Feedback
// ==========================================

void updateNeoPixel() {
  uint32_t t = millis();
  bool blinkPhase = (t % 1000) < 500; // 0.5s ON, 0.5s OFF

  if (currentState == PLAYING) {
    pixels.clear();
    pixels.show();
  } else if (currentState == START_SCREEN) {
    if (blinkPhase) {
      pixels.fill(pixels.Color(0, 0, 64)); // Blue, dim
    } else {
      pixels.clear();
    }
    pixels.show();
  } else if (currentState == CLEAR_SCREEN) {
    if (blinkPhase) {
      pixels.fill(pixels.Color(0, 64, 0)); // Green, dim
    } else {
      pixels.clear();
    }
    pixels.show();
  }
}

void playClick() {
  tone(BUZZER_PIN, 1000, 50); // 1000Hz for 50ms
}

void playBeep() {
  tone(BUZZER_PIN, 200, 100); // 200Hz for 100ms
}

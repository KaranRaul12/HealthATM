#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>  // TFT Display
#include <SPI.h>
#include "MAX30105.h"         // MAX30102 Sensor
#include <Adafruit_MLX90614.h> // MLX90614 Temp Sensor
#include <Keypad.h>
#include <Adafruit_PWMServoDriver.h> // PCA9685 Servo Driver

// TFT Display Pins
#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8  

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
MAX30105 particleSensor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40); // PCA9685 at default I2C address

// Keypad Setup (Corrected Pins)
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Updated Row & Column Pins
byte rowPins[ROWS] = {6, 7, A0, A1};  // Row Pins
byte colPins[COLS] = {2, 3, 4, 5};    // Column Pins

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Medicine Menu
const char* medicines[] = {"Ibuprofen", "Paracetamol", "Vitamin C"};
int selectedIndex = 0;  // Track selected item

enum State { WELCOME, PLACE_FINGER, MEASURE, SHOW_RESULTS, MEDICINE_MENU, DISPENSING, DISPENSED };
State currentState = WELCOME;

float bpm = 0, spo2 = 0, tempF = 0;
unsigned long startTime = 0;

// Servo channels assigned to medicines
#define IBUPROFEN_SERVO 2
#define PARACETAMOL_SERVO 4
#define VITAMIN_C_SERVO 5


// Servo movement range
#define SERVO_MIN 100  
#define SERVO_MAX 500  

void setup() {
  Serial.begin(9600);
  Wire.begin();
  pwm.begin();
  pwm.setPWMFreq(50);  // 50Hz for servos

  // Initialize TFT Display
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(50);

  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);  // Set Landscape Mode
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);  // Smaller text size

  mlx.begin();
  
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30102 not found!");
  }
  particleSensor.setup();

  Serial.println("Health ATM Ready!");
  showWelcomeScreen();
}

void loop() {
  char key = keypad.getKey();
  
  switch (currentState) {
    case WELCOME:
      if (key == 'A') {
        showPlaceFingerScreen();
        currentState = PLACE_FINGER;
      }
      break;

    case PLACE_FINGER:
      if (fingerDetected()) {
        showMeasuringScreen();
        startTime = millis();
        currentState = MEASURE;
      } else {
        showErrorScreen();
      }
      break;

    case MEASURE:
      if (millis() - startTime < 10000) {
        takeMeasurements();
      } else {
        showResultsScreen();
        currentState = SHOW_RESULTS;
      }
      break;

    case SHOW_RESULTS:
      if (key == 'B') {
        currentState = MEDICINE_MENU;
        showMedicineMenu();
      }
      break;

    case MEDICINE_MENU:
      if (key == '2' && selectedIndex > 0) {  // Up
        selectedIndex--;
        showMedicineMenu();
      }
      if (key == '8' && selectedIndex < 3) {  // Down
        selectedIndex++;
        showMedicineMenu();
      }
      if (key == 'C') {
        dispenseMedicine();
      }
      if (key == 'D') {
        currentState = SHOW_RESULTS;
        showResultsScreen();
      }
      break;

    case DISPENSED:
      if (key == '#') {
        currentState = WELCOME;
        showWelcomeScreen();
      }
      break;
  }
}

// Function to check if finger is placed
bool fingerDetected() {
  return particleSensor.check() && particleSensor.getIR() > 50000;  
}

// Function to take BPM, SpO2, and Temperature readings
void takeMeasurements() {
  if (fingerDetected()) {
    bpm = random(60, 100);  // Placeholder for actual BPM calculation
    spo2 = random(95, 99);  // Placeholder value
    tempF = mlx.readObjectTempF();
  } else {
    showErrorScreen();
  }
}

// Function to dispense medicine using the correct servo
void dispenseMedicine() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 30);
  tft.print("Dispensing ");
  tft.println(medicines[selectedIndex]);

  int servoChannel;

  switch (selectedIndex) {
    case 0: servoChannel = IBUPROFEN_SERVO; break;
    case 1: servoChannel = PARACETAMOL_SERVO; break;
    case 2: servoChannel = VITAMIN_C_SERVO; break;
  }

  // Rotate the corresponding servo
  pwm.setPWM(servoChannel, 0, SERVO_MAX);  // Move to dispense position
  delay(300);
  pwm.setPWM(servoChannel, 0, SERVO_MIN);  // Move back
  delay(300);

  showDispensedScreen();
}

// Screen Functions
void showWelcomeScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 20);
  tft.println("Health ATM");
  tft.setCursor(10, 40);
  tft.println("Press A to Start");
}

void showPlaceFingerScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 30);
  tft.println("Place Finger");
}

void showMeasuringScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 30);
  tft.println("Measuring...");
}

void showResultsScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.println("Results:");
  tft.setCursor(10, 30);
  tft.print("BPM: "); tft.println(bpm);
  tft.setCursor(10, 50);
  tft.print("SpO2: "); tft.println(spo2);
  tft.setCursor(10, 70);
  tft.print("Temp: "); tft.print(tempF); tft.println("F");
  tft.setCursor(10, 100);
  tft.println("Press B for Meds");
}

void showMedicineMenu() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 10);
  tft.println("Select Medicine:");

  for (int i = 0; i < 4; i++) {
    tft.setCursor(20, 30 + (i * 15));
    if (i == selectedIndex) {
      tft.print("> ");
    } else {
      tft.print("  ");
    }
    tft.println(medicines[i]);
  }
}

void showDispensedScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 30);
  tft.println("Dispensed!");
  tft.setCursor(10, 50);
  tft.println("Press # to return");
  currentState = DISPENSED;
}

void showErrorScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setCursor(10, 30);
  tft.setTextColor(ST77XX_RED);
  tft.println("Keep finger now & WAIT");
  delay(2000);
  showPlaceFingerScreen();
}

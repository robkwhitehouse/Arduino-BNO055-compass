/*
  Electronic Audio Compass - R K Whitehouse Oct 2021
  Uses the Bosch BNO-055 absolute orientation sensor module

  ------
  About:
  ------
  This sketch has been tested on an ESP-32 Devkit C

  ---------------
  Hardware setup:
  ---------------
  The BNO-055 is connected to the ESP-32 via the standard I2S pins as follows;
  CLK - GPIO 22
  DAT - GPIO 21
  GND - GND
  VCC - 3.3v

  Initial prototype has 4 push-buttons connected to GPIOs 25, 26, 32 & 33 for control purposes

  There is an 16 X 2 LCD display connected via I2S. Same connections as the 9250 except that VCC is 5v

  External pull-ups are not required as the BNO-055 has internal 10K pull-ups to an internal 3.3V supply.

  The BNO-055 is an I2C sensor and uses the Arduino Wire library. Because the sensor is not 5V tolerant,
  the internal pull-ups used by the Wire library in the Wire.h/twi.c utility file.

  This may be achieved by editing lines 75,76,77 in your
  "C:\Users\Your_name\Documents\Arduino\libraries\Wire\utility\wire.c" file to read:

  // deactivate internal pullups for twi.
  digitalWrite(SDA, 0);
  digitalWrite(SCL, 0);

  ---------------
  Terms of use:
  ---------------
  The software is provided "AS IS", without any warranty of any kind, express or implied,
  including but not limited to the warranties of mechantability, fitness for a particular
  purpose and noninfringement. In no event shall the authors or copyright holders be liable
  for any claim, damages or other liability, whether in an action of contract, tort or
  otherwise, arising from, out of or in connection with the software or the use or other
  dealings in the software.

  -----------

*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Effortless_SPIFFS.h>
#include "LCDmenu.h"
//#include "RKW_BNO055.h"
#include <BNO055_support.h>
#include <BNO055.h>
#include "scheduler.hpp"
#include "RotaryEncoder.hpp"

/* Create global objects */
eSPIFFS fileSys;
//BNO bno;  //create bno from the Class BNO
Scheduler scheduler;

//If you add new settings parameters then you will need to increase the size of the JSON doc
StaticJsonDocument<1024> settings;
String buf;

//LCD Display constants
#define COLUMS           16
#define ROWS             2
#define LCD_SPACE_SYMBOL 0x20  //space symbol from the lcd ROM, see p.9 of GDM2004D datasheet

LiquidCrystal_I2C lcd;

//Set up rotary encoder
#define ENCODER_PIN_A 34
#define ENCODER_PIN_B 35

RotaryEncoder knob1(ENCODER_PIN_A, ENCODER_PIN_B);

/* Setup initial menu data */

Menu mainMenuOptions;
LCDmenu mainMenu(&lcd, &mainMenuOptions);

Menu calMenuOptions;
LCDmenu calMenu(&lcd, &calMenuOptions);

// --- Set up tasks
struct Task displayCalibTask;
struct Task displayHeadingTask;
struct Task scanEncoderTask;

struct bno055_t bno055;
BNO055_RETURN_FUNCTION_TYPE comres;

//Difference in horizontal orientation between sensor and boat centreline
short boatCompassDelta; //degrees - can be positive or negative -180 to +180


const char* settingsFN = "/settings.json";           // settings file name (SPIFFS)

// -----------------
// setup()
// -----------------
void setup()
{
  bool calibration_needed, spiffsOK = false;
  Wire.begin();
  Wire.setClock(10000);                            // 10 kbit/sec I2C speed - slow but allows long connection leads
  while (!Serial);                                  // required for Feather M4 Express
  Serial.begin(115200);

  comres = BNO_Init(&bno055);
  if ( bno055.sw_revision_id != 0 ) {
    Serial.print("BNO055 Software revision: ");
    Serial.println(bno055.sw_revision_id);
  } else {
    Serial.println("bno055_init() failed");
  }
  
  serializeJson(settings, buf);                    // Settings are saved in JSON format

  // ----- Display title
  Serial.println(F("Electronic Compass V0.C"));
  Serial.println("");
  delay(2000);

  //Puts the compass into fusion output mode NDOF.
  Serial.println("Putting BNO055 into Sensor Fusion Compass Mode");
  comres = bno055_set_operation_mode(OPERATION_MODE_COMPASS); 
  if ( comres != SUCCESS ) {
    Serial.println("Could not configure BNO055 into compass mode");    
  }

  // Check for existing settings file
  spiffsOK = fileSys.checkFlashConfig();          //check if SPIFFS is mounted
  if (spiffsOK) {
    if (SPIFFS.exists(settingsFN)) {
      if (fileSys.openFromFile(settingsFN, buf)) { // Read settings from file into buf
        DeserializationError err = deserializeJson(settings, buf);
        if (err) {
          Serial.print(F("deserializeJson() failed with code: "));
          Serial.println(err.f_str());
        }

        boatCompassDelta = settings["boatCompassDelta"];

        Serial.println(F("Successfully read and parsed settings data."));
      } else Serial.println(F("Could not open settings file."));
    } else {
      Serial.println(F("No saved calibration data - Calibration required"));
      calibration_needed = true;
    }
  } else {
    Serial.println(F("Could not mount SPIFFS FS - perhaps not yet formatted?"));
  }



  // ----- Level surface message
  Serial.println(F("Place the compass on a level surface"));
  Serial.println("");
  delay(2000);


  lcd.begin(COLUMS, ROWS, LCD_5x8DOTS); //colums - 16, rows - 2, pixels - 5x8


  // ----- Display title
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print(F("Electronic"));
  lcd.setCursor(2, 1);
  lcd.print(F("Compass V0.C"));
  delay(2000);

  // --- Populate Scheduler Task List
  // --- 1. IMU Calibration checker
  displayCalibTask.proc = &outputCalibrationStatus;
  displayCalibTask.repeating = true;
  displayCalibTask.period = 500;                    // Every 500ms
  scheduler.add(&displayCalibTask);

  // --- 2. Output Compass Heading
  displayHeadingTask.proc = &outputHeading;
  displayHeadingTask.repeating = true;
  displayHeadingTask.period = 500;                  // Every 500ms
  scheduler.add(&displayHeadingTask);

  // --- 3. Poll the rotary encoder input pins
  scanEncoderTask.proc = &scanEncoder;
  scanEncoderTask.repeating = true;
  scanEncoderTask.period = 1;
  scheduler.add(&scanEncoderTask);
  
}

// ----------
// loop()
// ----------
void loop()
{
  unsigned short heading;
  long loopStart = micros();
  
  // --- run the scheduler as often as possible
  scheduler.dispatch();
  
}


// ------------------------
// print_number()
// ------------------------
/* Overloaded routine to stop integer numbers jumping around */
long print_number(short number) {
  String myString = String(number);
  short numberChars = myString.length();
  for (short i = 0; i < 6 - numberChars; i++) {
    Serial.print(" ");
  }
  Serial.print(myString);
}

// ------------------------
// print_number()
// ------------------------
/* Overloaded routine to stop float numbers jumping around */
float print_number(float number) {
  String myString = String(number);
  short numberChars = myString.length();
  for (short i = 0; i < 6 - numberChars; i++) {
    Serial.print(" ");
  }
  Serial.print(myString);
}

// ------------------
// Ouput IMU Calibration Status
// ------------------
void outputCalibrationStatus() {

  uint8_t calStatus;
  char calString[17];

  calStatus = getBNOCalibration();
  
  uint8_t sys = (calStatus & B11000000) >> 6;
  uint8_t gyro = (calStatus & B00110000) >> 4;
  uint8_t accel = (calStatus & B00001100) >> 2;
  uint8_t mag = (calStatus & B00000011);
  // The following string is 16 chars long to fit the LCD display
  sprintf(calString,"Cal S%1d G%1d A%1d M%1d ", sys, gyro, accel, mag);  
  lcd.setCursor(0, 1);
  lcd.print(calString);
  Serial.println(calString);
}

// -------------------------
// Output Heading
// -------------------------

void outputHeading() {
  uint16_t heading;
  char lcdString[17];

  heading = getBNOHeading();
  sprintf(lcdString,"Heading %03d deg.", heading);
  lcd.setCursor(0,0);
  lcd.print(lcdString);
  Serial.println(lcdString);  
}

void scanEncoder() {
  knob1.scan();
}
uint8_t getBNOCalibration() {
  BNO055_RETURN_FUNCTION_TYPE comres = BNO055_Zero_U8X;
  unsigned char v_data_u8r = BNO055_Zero_U8X;
  unsigned char status = BNO055_Zero_U8X;
  
  status = bno055_write_page_id(PAGE_ZERO);
  if (status == SUCCESS) {
      comres = bno055.BNO055_BUS_READ_FUNC(bno055.dev_addr, BNO055_CALIB_STAT_ADDR, &v_data_u8r, 1);
  }
  return(comres);
}

uint16_t getBNOHeading() {
  BNO055_S16 eul_h;
  
  comres = bno055_read_euler_h(&eul_h);
  return(eul_h);
}

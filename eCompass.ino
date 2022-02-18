/*
 - Electronic Compass
 - RKW Jan 2022
*/

/* Imported libraries */
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>
#include <EEPROM.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <cppQueue.h>
#include <TaskScheduler.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <DNSServer.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "Configuration.h"
#include "Dump.h"
#include "NMEA.hpp"

/* Local libs */
#include "StateMachine.hpp"
#include "RotaryEncoder.hpp"

#define TELNET_PORT 23
#define MAX_TELNET_CLIENTS 4


#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO

#define BNO055_SAMPLERATE_DELAY_MS 100

//GPIO pin definitions
#define ENCODER_DATA 16       //Roraty encoder data input (interrupt)
#define ENCODER_CLK  17       //Rotary encoder clock input
#define ENCODER_BUTTON 4     //Rotary encoder push button (interrupt)

Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Declare background task methods
void output();
void updateHeading();
void turnOff();
void getCalStatus();
void checkWiFiClients();

/* Declare Global Singleton Objects */
// Background tasks
Task outputTask(1000, TASK_FOREVER, &output);              // do output every 1 seconds
Task updateHeadingTask(500, TASK_FOREVER, &updateHeading); //Read sensor twice per second
Task getCalStatusTask(500, TASK_FOREVER, &getCalStatus);   //Read sensor calibration every 500ms
Task checkWiFiClientsTask(2000, TASK_FOREVER, &checkWiFiClients); //Check WiFi client connections every 2 secs

//Background task scheduler
Scheduler runner;

//Rotary Encoder
RotaryEncoder encoder(ENCODER_DATA,ENCODER_CLK, ENCODER_BUTTON);

//State machine - runs foreground tasks
FSM fsm;   //Finite State Machine

//BNO055 sensor module
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

enum displayMode {splashScreen,runModeDisplay,menuModeDisplay,offsetEntryDisplay} currentDisplay;

unsigned heading;
const char *ssid = "NavSource";
WiFiServer *telnetServer = NULL;
WiFiClient **telnetClients = {NULL};

//Globals to hold calibration status
struct CalStatus {
  uint8_t system;
  uint8_t gyro;
  uint8_t accel;
  uint8_t mag;
} calStatus;

//Menu mainMenu;

/* The main loop is implemented as a simple state machine with the following
 *  states and transitions.
 *  State 0 - Intialise - this is called automatically in the Arduino Setup() procedure but
 *  State 1 - Set target heading mode - this mode is entered after initialisation
 *  State 2 - Navigate - normal operation
 *  State 3 - Calibrate - input the current boat compass reading and calculate the offset
 *  State 4 - Menu input - get menu selection from user
 *  
 *  The following events are recognised by the state machine;
 *  
 *  1. Long button press (2 seconds) transition to Menu mode (from anywhere)
 *  2. Short button press - accept current user input and transition to appropriate state
 *  3. Rotary encoder motion - does not cause any state transition
 *  
 *  The menu options (displayed on the OLED display) are;
 *  1. Enter new heading - transitions to SetCourseMode
 *  2. Calibrate - transitions to calibation mode
 *  3. Navigate - transition to normal run mode
 */

enum FSMevents { LONGPRESS, SHORTPRESS };
// State Machine Event Queue
cppQueue eventQueue(sizeof(FSMevents),10,FIFO);

/**************************************************************************/
/*
    Display the raw calibration offset and radius data
    */
/**************************************************************************/
void printSensorOffsets(const adafruit_bno055_offsets_t &calibData)
{
    Serial.print("Accelerometer: ");
    Serial.print(calibData.accel_offset_x); Serial.print(" ");
    Serial.print(calibData.accel_offset_y); Serial.print(" ");
    Serial.print(calibData.accel_offset_z); Serial.print(" ");

    Serial.print("\nGyro: ");
    Serial.print(calibData.gyro_offset_x); Serial.print(" ");
    Serial.print(calibData.gyro_offset_y); Serial.print(" ");
    Serial.print(calibData.gyro_offset_z); Serial.print(" ");

    Serial.print("\nMag: ");
    Serial.print(calibData.mag_offset_x); Serial.print(" ");
    Serial.print(calibData.mag_offset_y); Serial.print(" ");
    Serial.print(calibData.mag_offset_z); Serial.print(" ");

    Serial.print("\nAccel Radius: ");
    Serial.print(calibData.accel_radius);

    Serial.print("\nMag Radius: ");
    Serial.print(calibData.mag_radius);
}
/**************************************************************************/
/*
    Displays some basic information on this sensor from the unified
    sensor API sensor_t type (see Adafruit_Sensor for more information)
    */
/**************************************************************************/
void printSensorDetails(void)
{
    sensor_t sensor;
    bno.getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.print("Sensor:       "); Serial.println(sensor.name);
    Serial.print("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" xxx");
    Serial.print("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" xxx");
    Serial.print("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" xxx");
    Serial.println("------------------------------------");
    Serial.println("");
    delay(500);
}

/**************************************************************************/
/*
    Display some basic info about the sensor status
    */
/**************************************************************************/
void printSensorStatus(void)
{
    /* Get the system status values (mostly for debugging purposes) */
    uint8_t system_status, self_test_results, system_error;
    system_status = self_test_results = system_error = 0;
    bno.getSystemStatus(&system_status, &self_test_results, &system_error);

    /* Display the results in the Serial Monitor */
    Serial.println("");
    Serial.print("System Status: 0x");
    Serial.println(system_status, HEX);
    Serial.print("Self Test:     0x");
    Serial.println(self_test_results, HEX);
    Serial.print("System Error:  0x");
    Serial.println(system_error, HEX);
    Serial.println("");
    delay(500);
}

/**************************************************************************/
/*
    Display sensor calibration status
    */
/**************************************************************************/
void printCalStatus(void)
{
    /* Get the four calibration values (0..3) */
    /* Any sensor data reporting 0 should be ignored, */
    /* 3 means 'fully calibrated" */

    /* Display the individual values */
    Serial.print("Sys:");
    Serial.print(calStatus.system, DEC);
    Serial.print(" G:");
    Serial.print(calStatus.gyro, DEC);
    Serial.print(" A:");
    Serial.print(calStatus.accel, DEC);
    Serial.print(" M:");
    Serial.print(calStatus.mag, DEC);
}

void displayCalStatus() {
  char textBuff[20];
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.setTextColor(SH110X_WHITE);
  display.println("Calibration Status");
  display.setCursor(25,0);
  sprintf(textBuff,"S: %1d G: %1d A: %1d M: %1d",
      calStatus.system, calStatus.gyro, calStatus.accel, calStatus.mag);
  display.println(textBuff);
  display.drawLine(0, 59, 127, 59, SH110X_WHITE);
  display.drawCircle(63, 59, 4, SH110X_WHITE);
  display.display();
 
}

void errorDisplay(char *errorString) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0,10);
  display.setTextColor(SH110X_WHITE);
  display.println("System error:");
  display.setCursor(0,25);
  display.println(errorString);
  display.display();
}

void setup() {
  uint8_t system, gyro, accel, mag;
  int eeAddress = 0;
  long bnoID;
  bool foundCalib = false;
  Wire.begin();
  Serial.begin(115200);
  delay(1000);
  if (!EEPROM.begin(512))
  {
    Serial.println("EEPROM failed to initialise");
    while (true);
  }
  else
  {
    Serial.println("EEPROM initialised");
  }
  //Rotary Encoder pins
  pinMode(ENCODER_DATA, INPUT_PULLUP);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);

  //Init OLED display
  display.begin(i2c_Address, true); // Address 0x3C default

  //Display splash screen on OLED
  displayOLEDSplash();

 
  //Initialization of the BNO055
  /* Initialise the sensor */
  if (!bno.begin())
  {
    /* There was a problem detecting the BNO055 ... check your connections */
    Serial.print("Ooops, no BNO055 detected ... Check your wiring or I2C ADDR!");
    errorDisplay("No BNO-055 detected.");
    while (1);
  }

  //Look to see if we have existing sensor calibration offset data in EEPROM
  EEPROM.get(eeAddress, bnoID);

  adafruit_bno055_offsets_t calibrationData;
  sensor_t sensor;

  /*
    *  Look for the sensor's unique ID at the beginning oF EEPROM.
    *  This isn't foolproof, but it's better than nothing.
  */
  bno.getSensor(&sensor);
  if (bnoID != sensor.sensor_id)
  {
    Serial.println("\nNo Calibration Data for this sensor exists in EEPROM");
    delay(500);
  }
   else
  {
    Serial.println("\nFound Calibration for this sensor in EEPROM.");
    eeAddress += sizeof(long);
    EEPROM.get(eeAddress, calibrationData);

    printSensorOffsets(calibrationData);

    Serial.println("\n\nRestoring Calibration data to the BNO055...");
    bno.setSensorOffsets(calibrationData);

    Serial.println("\n\nCalibration data loaded into BNO055");
    foundCalib = true;
  }

  delay(1000);

  /* Display some basic information on this sensor */
  printSensorDetails();

  /* Optional: Display current status */
  printSensorStatus();

 /* Crystal must be configured AFTER loading calibration data into BNO055. */
  bno.setExtCrystalUse(true);

  sensors_event_t event;
  bno.getEvent(&event);
  /* always recal the mag as It goes out of calibration very often */
  if (foundCalib){
//    Serial.println("Move sensor slightly to calibrate magnetometers");
//    while (!bno.isFullyCalibrated())
//    {
//      bno.getEvent(&event);
//      delay(BNO055_SAMPLERATE_DELAY_MS);
//    }
  }
  else
  {
    Serial.println("Please Calibrate Sensor: ");
    /* Get the four calibration values (0..3) */
    /* Any sensor data reporting 0 should be ignored, */
    /* 3 means 'fully calibrated" */

    system = gyro = accel = mag = 0;
    bno.getCalibration(&system, &gyro, &accel, &mag);
    while ( system < 2 || mag < 2 || accel < 1 ) {
        Serial.print(event.orientation.x, 4);
        Serial.print("\tY: ");
        Serial.print(event.orientation.y, 4);
        Serial.print("\tZ: ");
        Serial.print(event.orientation.z, 4);

        /* Optional: Display calibration status */
        printCalStatus();
        displayCalStatus();

        /* New line for the next sample */
        Serial.println("");
        bno.getCalibration(&system, &gyro, &accel, &mag);

        /* Wait the specified delay before requesting new data */
        delay(BNO055_SAMPLERATE_DELAY_MS);
    }
  }

  Serial.println("Calibration Offsets: ");
  adafruit_bno055_offsets_t newCalib;
  bno.getSensorOffsets(newCalib);
  printSensorOffsets(newCalib);

  Serial.println("\n\nStoring calibration data to EEPROM...");

  eeAddress = 0;
  bno.getSensor(&sensor);
  bnoID = sensor.sensor_id;

  EEPROM.put(eeAddress, bnoID);

  eeAddress += sizeof(long);
  EEPROM.put(eeAddress, newCalib);
  EEPROM.commit();
  Serial.println("Data stored to EEPROM.");

  Serial.println("\n--------------------------------\n");
  delay(500);

  // -- We have a workin sensor so get the scheduler running
  runner.init();
  runner.addTask(outputTask);
  runner.addTask(updateHeadingTask);
  runner.addTask(getCalStatusTask);
  runner.addTask(checkWiFiClientsTask);
  
  outputTask.enable();
  updateHeadingTask.enable();
  getCalStatusTask.enable();
  checkWiFiClientsTask.enable();
  
  //Init Rotary Encoder
  encoder.begin();

  //Startup the Wifi access point
  WiFi.softAP(ssid);
  telnetClients = new WiFiClient*[4];
  for(int i = 0; i < 4; i++)
  {
    telnetClients[i] = NULL;
  }
  
  telnetServer = new WiFiServer(23);
  telnetServer->begin();
  IPAddress myAddr = WiFi.softAPIP();
  Serial.print("My IP Address = ");
  Serial.println(myAddr);
  
  //Set up inital foreground mode
  fsm.currentState = runMode; 

}

void loop() {
  // put your main code here, to run repeatedly:
  long now;
  uint8_t sysStatus, sysSelfTest, sysError;

    //Run the background tasks
  runner.execute();
  //Check encoder debounce timer
  if (encoder.inDebounceDelay) {
    now = micros();
    if ( now > encoder.deBounceEnd || now < encoder.deBounceStart ) {
      encoder.inDebounceDelay = false;  
    }
  } 
  // Run the foreground task
  fsm.currentState();

}


char buff[5];
void output() {
 
  sprintf(buff, "Current heading: %03d deg.\n", heading);
  Serial.print(buff);
  HDMmessage msg(heading);
  Serial.println(msg.msgString);
  for ( int i=0; i<MAX_TELNET_CLIENTS; i++ ) {
    if ( telnetClients[i] != NULL ) {
      telnetClients[i]->println(msg.msgString);
    }
  }
}

void displayMainMenu() {
      int clicks;
    
    clicks = encoder.getPulseCount(); //Returns number of clicks since previous call
}

void updateHeading() {
          /* Get a new sensor event */
    sensors_event_t event;
    bno.getEvent(&event);
    heading = event.orientation.x;  
}  

void getCalStatus() {
  bno.getCalibration(&calStatus.system, &calStatus.gyro, &calStatus.accel, &calStatus.mag);
}

void checkWiFiClients() {
  WiFiClient tempClient = telnetServer->available();   // listen for incoming clients

  if (tempClient) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
     for (int i=0; i<MAX_TELNET_CLIENTS; i++ ) {
      if ( telnetClients[i] == NULL ) {
        WiFiClient* client = new WiFiClient(tempClient);
        telnetClients[i] = client;
        break;
      }
    }

  }
}

void displayOLEDSplash()
{
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20,0);
  display.setTextColor(SH110X_WHITE);
  display.println("E.A.S.T.");
  display.setCursor(25,25);
  display.setTextSize(1);
  display.println("Audio Compass");
  display.setCursor(8,40);
  display.println("Development Version");

  display.drawLine(0, 59, 127, 59, SH110X_WHITE);
  display.drawCircle(63, 59, 4, SH110X_WHITE);
  display.display();

}
// 
// Get the required course from the user
//
void runMode()
{
    //Set up run mode OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0);
  display.println("  Current    Target  ");
  display.setTextSize(2);
  display.setCursor(14,12);
  sprintf(buff,"%03d", heading);
  display.println(buff);
  display.setCursor(78,12);
  display.println("359");
  display.drawLine(64, 0, 64, 32, SH110X_WHITE);
  display.drawLine(0, 32, 127, 32, SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0,37);
  display.println("Calibration Status");
  display.setCursor(0,50);
  display.setTextSize(1);
  sprintf(buff,"S:%1d G:%1d A:%1d M:%1d",
      calStatus.system, calStatus.gyro, calStatus.accel, calStatus.mag);
  display.printf(buff);
  display.display();
   
}

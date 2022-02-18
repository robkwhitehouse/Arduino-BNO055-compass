# Arduino-BNO055 boat compass
An electronic boat compass based on the BOSCH BNO055 chip. The compass is compensated for tilt and roll
i.e. it has a "software gimbal"

This version is basically working but still has some bugs (see below).

The hardware required is;

1. An ESP32 Devkit
2. An Adafruit BNO055 sensor module (connected via I2C)
3. A 64X128 OLED display (with I2C connection)
4. A 3K3 pull-up reistor for the I2C DTA line

The standard I2C ESP32 pins are used.

In the near future, I propose to add a rotary encoder with push button to allow management of various settings.
These will be accessed via menus displayed on the OLED display

On first power-up, the sensors will require initial calibration. This may take some time.
When this has been done, the offsets will be stored in the ESP32 Flash memory and, on subsequent
power up, these offsets are reloaded. This radically reduces calibration time.

A WiFi Access Point is started and a server is run up on port 23 (Telnet).
A scan is performed every couple seconds for new WiFi clients connected to port 23

After calibraton, the current heading is displayed on the OLED display and it is
also output to all/any WiFi telnet clients in the format of an NMEA "HDM" message.

The sketch design is described in the "eCompass.ino" file. 

Known Bugs
==========

There is a bit of a problem with the BNO055 sensor self-calibration process. The sensor is used in "Sensor Fusion" mode.
In this mode, the sensor is continuously monitoring it's environment and recalibrating itself. Sadly this often seems to make 
things worse rather than better. I'm not sure why this is, but I am hopeful that I will eventually learn enough about this device
so that I can rectify this issue. If anybody has any suggestions or would like to help with the testing I would be very grateful
if you would contact me. - bob.whitehouse@btinternet.com

Futures/ work-in-progress
=========================

I am intending to add a facility to output the heading as an audio announcement. This will be streamed over
Bluetooth using the ESP built-in Bluetooth to an external BT speaker or earpiece.
The iniention is to allow visually impaired people to steer the boat.

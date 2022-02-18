//
// Configuration.h
//
//  See Configuration.cpp for details.
//

#define EEPROM_SIZE 512

#define ACCESS_POINT_SSID_SIZE 32
#define ACCESS_POINT_PASSWORD_SIZE 64

#define BLUETOOTH_DEVICE_NAME_SIZE 31

// The configuration structure holds the basic configuration data.

struct Configuration
{
  uint8_t MajorVersion;
  uint8_t MinorVersion;

  // WiFi Related Data
  
  char AccessPointSSID[ACCESS_POINT_SSID_SIZE + 1];
  char AccessPointPassword[ACCESS_POINT_PASSWORD_SIZE + 1];

  // TCP Client related data
  
  uint16_t TCPPort;
  uint8_t MaximumTCPClientCount;

  // BlueTooth Related Data

  bool BlueToothEnabled;
  char BlueToothDeviceName[BLUETOOTH_DEVICE_NAME_SIZE + 1];

  // NMEA Releated Data

  uint NMEABaudRate;
};

// The EPPROM_DATA structure wraps the configuration structure and adds a checksum
// to it. The checkum will be validated on reads and updated on writes.

struct EEPROM_DATA
{
  struct Configuration Configuration;
  uint16_t Size;
  uint16_t CheckSum;
};

// The following methods are used to manipulate the configuration information

bool readConfiguration(struct Configuration *configuration);
void writeConfiguration(struct Configuration *configuration);
void dumpConfiguration(struct Configuration *configuration);

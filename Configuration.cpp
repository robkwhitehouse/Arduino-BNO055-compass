//
//  Configuration.cpp
//
//  The configuration data for the device is held in EEPROM (or flash) and the
//  code in this module is here to save and retrieve the configuration data.
//

#include <HardwareSerial.h>
#include <EEPROM.h>

#include "Configuration.h"

// The following method is used to calculate a 16 bit checksum for the data held 
// in the configuration. The algorithm is details here:
//
// https://en.wikipedia.org/wiki/Fletcher%27s_checksum

uint16_t Fletcher16( uint8_t *data, int count )
{
   uint16_t sum1 = 0;
   uint16_t sum2 = 0;
   int index;

   for( index = 0; index < count; ++index )
   {
      sum1 = (sum1 + data[index]) % 255;
      sum2 = (sum2 + sum1) % 255;
   }

   return (sum2 << 8) | sum1;
}

void dumpConfiguration(struct Configuration *configuration)
{
  Serial.println("Configuration");
  Serial.printf("   MajorVersion: ........... %d\n",configuration->MajorVersion);
  Serial.printf("   MinorVersion: ........... %d\n",configuration->MinorVersion);
  Serial.printf("   AccessPointSSID: ........ %s\n",configuration->AccessPointSSID);
  Serial.printf("   AccessPointPassword: .... %s\n",configuration->AccessPointPassword);
  Serial.printf("   TCPPort: ................ %d\n",configuration->TCPPort);
  Serial.printf("   MaximumTCPClientCount: .. %d\n",configuration->MaximumTCPClientCount);
  Serial.printf("   BlueToothEnabled: ....... %s\n",true == configuration->BlueToothEnabled ? "true" : "false");
  Serial.printf("   BlueToothDeviceName: .... %s\n",configuration->BlueToothDeviceName);
  Serial.printf("   NMEABaudRate: ........... %d\n",configuration->NMEABaudRate);
}

//
// This method is called to read the configuration information from the EEPROM
//
bool readConfiguration(struct Configuration *configuration)
{
  // Activate EEPROM access
  
  EEPROM.begin(EEPROM_SIZE);

  // We read from the EEPROM byte by byte, Allocate a structure to read
  // into and locate the start of it.

  struct EEPROM_DATA eepromData;
  char *q = (char *)&eepromData;

  // Read all the data from the EEPROM, this will include the checksum
  
  for(int i = 0; i < sizeof(EEPROM_DATA); i++)
    *q++ = EEPROM.read(i);

  // End the EEPROM access
  
  EEPROM.end();

  // Either way, if the checksum is correct or not we will copy over the
  // configuration information read back.

  memcpy(configuration,&eepromData.Configuration,sizeof(Configuration));

  // Display the stored size of checksum

  Serial.print("readConfiguration, Size = ");
  Serial.println(eepromData.Size);  
  Serial.print("readConfiguration, CheckSum = ");
  Serial.println(eepromData.CheckSum);    
  
  // We can now check the checksum
  
  uint16_t checkSum = Fletcher16((uint8_t *)&eepromData.Configuration,sizeof(Configuration));

  Serial.print("readConfiguration, expected CheckSum = ");
  Serial.println(checkSum);  

  // Return the a Bool to indicate if all is OK?
  
  return (sizeof(Configuration) == eepromData.Size) && (checkSum == eepromData.CheckSum);
}

//
// This method is called to write the configuration information to the EEPROM
//
void writeConfiguration(struct Configuration *configuration)
{
  // Activate EEPROM access
  
  EEPROM.begin(EEPROM_SIZE);

  // We read need to allocate an EEPROM_DATA object, populate it with the
  // configuration information passed in, and update size and the checksum.

  struct EEPROM_DATA eepromData;
  memcpy(&eepromData.Configuration,configuration,sizeof(Configuration));
  eepromData.Size = sizeof(Configuration);
  eepromData.CheckSum = Fletcher16((uint8_t *)&eepromData.Configuration,sizeof(Configuration));

  Serial.print("writeConfiguration, checksum = ");
  Serial.println(eepromData.CheckSum);
  
  // We write the data out byte by byte so we need a character pointer to
  // the start of the data.
  
  char *p = (char *)&eepromData;

  // Write out the data
  
  for(int i = 0; i < sizeof(EEPROM_DATA); i++)
    EEPROM.write(i,*p++);

  // End the EEPROM access
  
  EEPROM.end();
}

//
//  Dump.cpp
//
//  This module provides methods of dumping data.
//

#include <HardwareSerial.h>

char *toHex(int v, int ndigits)
{
  // Build the array of hex digits

  char *hexDigits = "0123456789ABCDEF";

  // Build the output buffer, this will be static and will get 
  // over-written on the next call.

  static char outputBuffer[9];

  // Fill the buffer with leading zeroes and zero terminate it.

  memset(outputBuffer, '0', 8);
  outputBuffer[8] = '\0';

  // Start our output pointer att the end, we will 'print' backwards

  char *p = &outputBuffer[8];

  // While we have a value to process

  while (v > 0)
  {
    // Pull out the remainder

    int digit = v % 16;
    
    // 'Print' the digit

    *--p = hexDigits[digit];

    // Move to the next digit.

    v = v / 16;
  }

  // How long a string to we have? 

  int outputLength = strlen(p);

  // If this is not long enough then move back

  if (outputLength < ndigits)
    p -= ndigits - outputLength;

  // Return the buffer pointer

  return p;
}

//
// This method does the classis HEX dump of data.
//

void hexDump(char *p, int l)
{
  // Dump out the data in clocks of 16

  for (int i = 0; i < l; i += 16)
  {
    // Put in the offset

    Serial.print(toHex(i, 8));
    Serial.print(" ");

    // Process 16 bytes

    for (int j = 0; j < 16; j++)
    {
      // Add spacing at the 8 byte point

      if (j == 8) Serial.print(" ");

      // Put in the hex digit or spaces as required

      if (i + j < l)
      {
        Serial.print(toHex(p[i + j], 2));
        Serial.print(" ");
      }
      else
        Serial.print("   ");
    }

    // Now print out the text if we can

    Serial.print(" ");
    for (int j = 0; j < 16; j++)
    {
      // Add spacing at the 8 byte point

      if (j == 8) Serial.print(" ");

      // Put in the character for '.' if not printable

      if (i + j < l)
      {
        if (isprint(p[i + j]))
          Serial.print(p[i + j]);
        else
        Serial.print(".");
      }

    }

    // Done!
    
    Serial.println("");
  }
}

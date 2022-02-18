
#define MAXLEN 255
/*
 * Format of HDM Message
 * $--HDM,x.x,M*hh
 * 
 * 
 */

const char* sourceID = "HC"; //Heading Compass

//Virtual base class. Completely useless until derived.
//It just provides the method to calculate and add 
//The NMEA checkSum which is common to all NMEA messages

class NMEAmessage {
  public:  
    char msgString[MAXLEN];
  protected:
    void addCheckSum() {
       int checkSum = 0;
       char temp[MAXLEN];
       for(int i = 1; i < strlen(msgString); i++)
       {
         checkSum ^= msgString[i];
       } 
       // Add the "*" and check sum
       sprintf(msgString,"%s*%02x", msgString, checkSum);    
    }


};

/*
 * "HDM" message - Magnetic Heading
 *
 * All the work is done in the constructor.
 * Once created, this object is read-only
 */


class HDMmessage : public NMEAmessage {
  public:
    HDMmessage(float heading) {
      sprintf(msgString,"$%sHDM,%03d,M",sourceID, static_cast<int>(std::round(heading))); 
      addCheckSum();
    }
};

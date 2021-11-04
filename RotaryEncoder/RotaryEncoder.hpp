/*
  Device handler for a rotary encoder
  R K Whitehouse - Nov 2021
  
  The encoder requires two input pins. It will send a stream of pulses
  to each pin while being rotated. The number of pulses indicates the degree
  of rotation. Typically around 20-25 per full rotation.
  The two streams of  pulses are identical except for the relative phase.
  If the pulses on pin A are in advance of those on pin B then it is a clockwise rotation and vice-versa.
  The pulse duration is a minimum of around 20ms (knob turned very quickly) and a
  practical maximum around 200ms (knob turned slowly).
  Although in theory there is no maximum duration.
  
  Most endcoders are mechanical and tend to suffer (sometimes very badly) from contact bounce
  So it is essential to perform some form of filtering to remove higher freqency transient pulses
  This should be done in both hardware (with a capacitor) and in software (here).
  
  Typically these transient pulses will be less than 100us duration.
  
  This driver assumes that anything with duration greater than 5ms is a valid pulse and
  ignores anything with shorter duration. This can be tweaked if necessary (see below). 
  
  The driver is not interrupt driven and the "scan() method should be called every  millisecond 
  (you might get awy with doing this every 2ms)
  
*/
 
#define MIN_PERIOD 5  // milliseconds
 
 
class RotaryEncoder {
   public:
     RotaryEncoder(uint8_t _pinA, uint8_t _pinB) {
       pinA = _pinA;
       pinB = _pinB;
     }
     int pulseCount () {  // is positive for clockwise steps, negative for anticlocwise
       int retVal = _pulseCount;
       _pulseCount = 0;
       return(retVal);
     }
     // This next procedure must be called every 1 millisecond
     void scan() {
       int pinAval, pinBval;
       
       if (deBounceTimer > 0) {  //In de-bounce period (2ms)
         deBounceTimer--;
         return;
       }
       
       // --- Get the current pin states
       pinAval = digitalRead(pinA);
       pinBval = digitalRead(pinB);

       if ( pinAval == 1 && pinAprev == 0  ) // New positive edge detected
       {
          deBounceTimer = 2; //Ignore any further edges for 2ms
          if ( pinBval == 0 ) _pulseCount++; //clockwise rotation
          else _pulseCount--;
       }
       
       pinAprev = pinAval;
     }
     
   private:
     int _pulseCount;
     uint8_t pinA, pinB; 
     int pinAprev;
     uint8_t deBounceTimer;
};

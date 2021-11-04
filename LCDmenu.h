/*
 A Class to implement a simple UI menu system to get option selections from the user
 Assumes a 16X2 LCD display with 4 push buttons underneath
 Individual option text should be 3 chars
 */

#define NUMBUTTONS 4

enum ButtonState {UP,DOWN};

class Button {
  private:
   uint8_t pin;
 public:  
   ButtonState state() { return((digitalRead(pin) ? UP : DOWN)); }  //Our button pin is normally HIGH unless pulled down by a button press
   void begin(uint8_t _pin) {
    pin = _pin;
    pinMode(pin, INPUT_PULLUP);
   };
};

 
class MenuOption {
  public:
    char text[4];         //3 chars max plus NULL
    uint8_t val;
};

class Menu  {
  public:
    char title[17];        //16 chars max plus NULL
    MenuOption options[4]; 
};

class LCDmenu {
  public:
  //Constructor
    LCDmenu( LiquidCrystal_I2C* _lcd, Menu* _menu) { 
    lcd = _lcd;
    menu = _menu;
    buttons[0].begin(32);
    buttons[1].begin(33);
    buttons[2].begin(26);
    buttons[3].begin(27);   
   };
  private:
    LiquidCrystal_I2C* lcd;  //Must pre-exist;
    Button buttons[NUMBUTTONS];
    Menu* menu;
    uint8_t selection = 0;
  public:
    uint8_t go();            //Display the menu and return the selection 
 };


uint8_t LCDmenu::go() {

  selection = 0;
  lcd->clear();
  lcd->setCursor(0,0);
  lcd->print(menu->title);
  
  for (int i=0; i<NUMBUTTONS; i++ ) {
    lcd->setCursor(i*4,1);
    lcd->print(menu->options[i].text);
  }
  while (selection == 0) {  //scan the buttons
    for(int button=0; button<NUMBUTTONS; button++) {
      if (buttons[button].state() == DOWN) {
        selection = menu->options[button].val;
        break;
      }
    }
  }
  return(selection);
}

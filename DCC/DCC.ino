//todo split function for lcd write gc/dd ram and lcd command

/*
  Used Parts:
  1x Arduino nano
  1x LCD "SSC2A16"
  4x potiometer
  5x pushbutton (pulls to ground when pressed)
  3x led        (needs to be pulled to GND to light up)

  Pinouts:
  LCD:
  ╔═════════════════════════════╗
  ║                                   Data6 | Data7  ║
  ║ Backlight +                       Data4 | Data5  ║
  ║                                   Data2 | Data3  ║
  ║            LCD display away       Data0 | Data1  ║
  ║                              Read/Write | Enable ║
  ║ Backlight -                    Contrast |  RS    ║
  ║                                     +5V | GND    ║
  ╚═════════════════════════════╝

  Pin definition arduino nano:
  A0 <- Potentiometer 1
  A1 <- Potentiometer 2
  A6 <- Potentiometer 3
  A7 <- Potentiometer 4
  D2 -> LCD Data4
  D3 -> LCD Data5
  D4 -> LCD Data6
  D5 -> LCD Data7
  D6 -> LCD RS
  D7 -> LCD Enable
*/
#include <EEPROM.h>
#define Poti1 A0
#define Poti2 A1
#define Poti3 A6
#define Poti4 A7

#define ButtonLOCswitch 8
#define ButtonMenu 9
#define ButtonRight 10
#define ButtonLeft 11
#define ButtonStop 12
#define LEDG1 A3
#define LEDG2 A4
#define LEDO 13

#define AliasStop 1
#define AliasMenu 2
#define AliasLOCswitch 4
#define AliasLeft 8
#define AliasRight 16

#define Warmup 300

#define LCD_clear(); LCD_write(0x01,false);
#define LCD_home();  LCD_write(0x03,false);
#define LCD_move_left(); LCD_write(0x10,false);
#define LCD_move_right(); LCD_write(0x14,false);
#define LCD_blink_on(); LCD_write(0x0E,false);
#define LCD_blink_off(); LCD_write(0x0C,false);

byte LOC_speed[4] = {0x00, 0x00, 0x00, 0x00}; //first bit is forward backward //TODO change?
unsigned int LOC_address[4] = {0x0000, 0x0000, 0x0000, 0x0000};
byte LOC_function_and_dir[4] = {0xC0, 0xC0, 0xC0, 0xC0};
char DCC_command_buffer[63];
byte SelectedLOC;

unsigned int tenthpower(byte n) {
  unsigned int result = 1;
  while ((n--) != 0) {
    result = result * 10;
  }
  return result;
}

void Menu_Stop() {
  LCD_blink_off();
  Serial.write("X\xA5\r");       //write STOP coammand to DCC
  while (Serial.available() < 2) {} //wait for answer
  if (!Serial.readString().equals("0\r")) {
    Serial.write("UnknownResponse"); //TODO implement propper error routine
  }
  digitalWrite(LEDG2, HIGH);
  digitalWrite(LEDO, LOW);
  LCD_clear();
  print_string_LCD("!Emergency Stop!", 0x80);
  print_string_LCD("Exit->STOPbutton", 0xC0);
  while (getButtonInput() != AliasStop) {}
  Serial.write("X\xA7\r");       //write XHalt coammand to DCC
  while (Serial.available() < 2) {} //wait for answer
  if (!Serial.readString().equals("0\r")) {
    Serial.write("UnknownResponse"); //TODO implement propper error routine
  }
  LCD_clear();
  digitalWrite(LEDO, HIGH);
  digitalWrite(LEDG2, LOW);
  LCD_clear();
  LCD_blink_on();
  return;
}

void print_string_LCD(char a[], byte address) {
  LCD_write(address, false);
  for (byte b = 0; ((b < 255) && (a[b] != 0));) {
    LCD_write(a[b++], true);
  }
}

byte refresh_Poti() { //TODO implement median or simmilar routine to make sure that potentiometer does not fluctuate maybe with modulo and a byte which track if incresed or decreased last time
  byte return_changed = 0x00;
  unsigned int poti_val = 0x0000;
  for (byte LOC_num = 0; LOC_num < 4; LOC_num++) {
    switch (LOC_num) {                  //read from dedicated Potentiometer
      case 0:
        poti_val = analogRead(Poti1);
        break;
      case 1:
        poti_val = analogRead(Poti2);
        break;
      case 2:
        poti_val = analogRead(Poti3);
        break;
      case 3:
        poti_val = analogRead(Poti4);
        break;
    }
    if (poti_val > 511) {
      poti_val = poti_val & 0x01FF; //Cut off first bit from adc
      LOC_function_and_dir[LOC_num] = LOC_function_and_dir[LOC_num] | 0x20; //Set direction to forward
    } else {
      poti_val = 511 - poti_val;
      LOC_function_and_dir[LOC_num] = LOC_function_and_dir[LOC_num] & 0xBF; //Set direction to backward
    }
    poti_val = poti_val / 4;
    if (poti_val == 1) {      //1 would be emergency stop + increases deadzone by x3
      poti_val = 0;
    }
    if ((poti_val) != LOC_speed[LOC_num]) { //check if meassured value has changed from stored value
      return_changed = return_changed | ((0x01) << LOC_num); //update the return byte
      LOC_speed[LOC_num] = poti_val;  //if so update the value
      DCC_command_buffer[0] = 'X';
      DCC_command_buffer[1] = 0x80; //Loc command
      DCC_command_buffer[2] = (LOC_address[LOC_num] & 0x00FF);
      DCC_command_buffer[3] = ((LOC_address[LOC_num] & 0xFF00) >> 8);
      DCC_command_buffer[4] = (LOC_speed[LOC_num]);
      DCC_command_buffer[5] = LOC_function_and_dir[LOC_num];
      DCC_command_buffer[6] = 0x0D;
      DCC_command_buffer[7] = '\n';
      //TODO remove comment Serial.write(DCC_command_buffer);
    }
  }
  return return_changed;
}



void LCD_init4bit() {
  delay(15);              //Wait 15ms for LCD internal power supply
  PORTD = PORTD & 0x03;   //Set D7-D2 to low and leave serial pins unchanged
  DDRD = DDRD | 0xFC;     //Set D7-D2 as output
  PORTD = PORTD | 0x08;   //Set D5 high command for 4 bit operation
  PORTD = PORTD | 0x80;   //Set Enable high
  delayMicroseconds(1);   //enable cycle time
  PORTD = PORTD & 0x7F;   //Set enable low
  delay(5);               //Wait 5ms
  LCD_write(0x2C, false); //set 4bit,2lines,5x10font
  LCD_blink_on(); //turn on display, no cursor, no blink cursor
  LCD_write(0x06, false); //increment DD-Ram address to write from left to right
  LCD_clear();
}

void LCD_write(byte data, boolean RS) {
  if (RS == true) {
    PORTD = PORTD & 0x43; //Clear all data bits
    PORTD = PORTD | 0x40; //Set RS high
  } else {
    PORTD = PORTD & 0x03; //Clear all data bits and RS
  }
  PORTD = ((data & 0xF0) >> 2) | (PORTD & 0x43); //Prepare high data bits
  PORTD = PORTD | 0x80;         //set Enable high
  delayMicroseconds(1);         //enable cycle time
  PORTD = PORTD & 0x7F;         //set enable low

  PORTD = ((data & 0x0F) << 2) | (PORTD & 0x43); //Prepare low data bits
  PORTD = PORTD | 0x80;         //set Enable high
  delayMicroseconds(1);         //enable cycle time
  PORTD = PORTD & 0x7F;         //set enable low

  if ((data & 0xFC) == 0) { //check if command is "Clear Display" or "Return Home" for appropriate delay
    delayMicroseconds(1600); //Wait 1600µs + 40µs
  }
  delayMicroseconds(40);
}

void update_LOC_icon(byte LOC_num) {
  if (LOC_address[LOC_num] < 101) {          //only 100 icons are stored in 1kbyte (1000)byte
    LCD_write(0x40 + (LOC_num << 4), false); //16 lines for each double icon of 8 rows each
    for (byte LCD_row = 0; LCD_row < 16; LCD_row++) {
      if (((LCD_row * 5) % 8) > 3) { //if 5 bit cross 8 byte border of eeprom TODO TEST THIS
        LCD_write(((EEPROM.read(((LOC_address[LOC_num] - 1) * 10) + ((LCD_row * 5) / 8)) << (((LCD_row * 5) % 8) - 3)) | (EEPROM.read(((LOC_address[LOC_num] - 1) * 10) + (((LCD_row * 5) / 8) + 1)) >> (11 - ((LCD_row * 5) % 8)))), true);
      } else {
        LCD_write((EEPROM.read(((LOC_address[LOC_num] - 1) * 10) + ((LCD_row * 5) / 8)) >> (3 - ((LCD_row * 5) % 8))), true); //read data from byte and shift accordingly
      }
    }
  } else {
    LCD_write(0x40 + (LOC_num * 20), false);
    for (byte LCD_row = 0; LCD_row < 20; LCD_row++) {
      LCD_write(LCD_row, true);
    }
  }
}

byte getButtonInput() {
  static unsigned int StopBut;
  static unsigned int MenuBut;
  static unsigned int LOCswitchBut;
  static unsigned int LeftBut;
  static unsigned int RightBut;
  byte PressedBut = 0;
  if (digitalRead(ButtonStop) == LOW) {
    if (StopBut != 65535) {
      StopBut++;
    }
    PressedBut |= AliasStop;
  } else {
    StopBut = 0;
  }
  if (digitalRead(ButtonMenu) == LOW) {
    if (MenuBut != 65535) {
      MenuBut++;
    }
    PressedBut |= AliasMenu;
  } else {
    MenuBut = 0;
  }
  if (digitalRead(ButtonLOCswitch) == LOW) {
    if (LOCswitchBut != 65535) {
      LOCswitchBut++;
    }
    PressedBut |= AliasLOCswitch;
  } else {
    LOCswitchBut = 0;
  }
  if (digitalRead(ButtonLeft) == LOW) {
    if (LeftBut != 65535) {
      LeftBut++;
    }
    PressedBut |= AliasLeft;
  } else {
    LeftBut = 0;
  }
  if (digitalRead(ButtonRight) == LOW) {
    if (RightBut != 65535) {
      RightBut++;
    }
    PressedBut |= AliasRight;
  } else {
    RightBut = 0;
  }
  if ((StopBut == Warmup) || (MenuBut == Warmup) || (LOCswitchBut == Warmup) || (LeftBut == Warmup) || (RightBut == Warmup)) {
    StopBut = 65535;
    MenuBut = 65535;
    LOCswitchBut = 65535;
    LeftBut = 65535;
    RightBut = 65535;
    return PressedBut;
  }
  return 0;
}

void Menu_ChangeFunction() {
  byte Position = 0;
  byte refresh = 0x01;
  while (true) {
    if (refresh == 0x01) {
      LCD_clear();
      LCD_write(0x80, false);
      LCD_write((0x31 + SelectedLOC), true);
      print_string_LCD(":F0 F4 F3 F2 F1", 0x81);
      LCD_write(0xC1, false);
      for (byte function = 0; function < 5; function++) {
        LCD_move_right(); //move cursor to the right twice
        LCD_move_right(); // "
        LCD_write(0x30 + ((LOC_function_and_dir[SelectedLOC] & (0x10 >> function)) >> (4 - function)), true);
      }
      LCD_move_left();  //Position Cursor over selected bit
      for (byte shift = 0; shift < Position; shift++) {
        LCD_move_left(); //
        LCD_move_left(); //
        LCD_move_left(); //
      }
    }
    switch (getButtonInput()) {
      case AliasStop:
        Menu_Stop();
        refresh = 0x01;
        break;
      case AliasMenu:
        LCD_clear();
        return;
        break;
      case AliasLOCswitch:
        if ((SelectedLOC++) == 3) {
          SelectedLOC = 0;
        }
        Position = 0;
        refresh = 0x01;
        break;
      case AliasLeft:
        if ((Position++) == 4) {
          Position = 0;
        }
        refresh = 0x01; //trigger refresh
        break;
      case AliasRight:
        if (Position == 0) {
          Position--;
        }
        refresh = 0x01; //trigger refresh
        break;
      case (AliasLeft+AliasRight):
        LOC_function_and_dir[SelectedLOC] = LOC_function_and_dir[SelectedLOC] ^ (0x01 << Position); //toggle it on or off
        refresh = 0x01;
        break;
      default:
        refresh = 0x00;
        break;
    }
    refresh_Poti();
  }
}

void Menu_ChangeAddress() {
  byte Position = 0;
  unsigned int LOC_address_new = LOC_address[SelectedLOC];
  unsigned int written_address;
  unsigned int change = 0;
  byte number_of_shifts;
  byte refresh = 0x01;
  while (true) {
    byte digitNum;
    byte tempDigit;
    if (refresh == 0x01) {
      digitNum = 5;
      tempDigit = 0;
      LCD_clear();
      print_string_LCD("Address of LOC ", 0x80);
      LCD_write(0x30 + (SelectedLOC + 1), true);
      number_of_shifts = 0;
      LCD_write(0xC4, false); //TODO move right?
      written_address = 0;
      while (digitNum != 0) {
        tempDigit = ((LOC_address_new - written_address) / tenthpower(--digitNum));
        if (tempDigit == 0) {
          LCD_write(0x30, true); //print '0'
        } else {
          LCD_write(0x30 + tempDigit, true);
          written_address = written_address + (tempDigit * tenthpower(digitNum));
        }
      }
      LCD_move_left();
      for (byte i = 0; i < Position; i++) {
        LCD_move_left();
      }
    }
    switch (getButtonInput()) {
      case AliasStop:
        Menu_Stop();
        refresh = true;
        break;
      case AliasMenu:
        { //is needed fot int eeprom address
          LOC_address[SelectedLOC] = LOC_address_new;
          int EEPROM_address = 0x03E8;
          for (byte LOC_num = 0; LOC_num < 4; LOC_num++) {                                                     //store LOC addresses
            EEPROM.update(EEPROM_address++, ((LOC_address[LOC_num] & 0xFF00) >> 8));
            EEPROM.update(EEPROM_address++, (LOC_address[LOC_num] & 0x00FF));
          }
          return;
        }
        LCD_clear();
        return;
        break;
      case AliasLOCswitch:
        LOC_address[SelectedLOC] = LOC_address_new; //write back new LOC_address
        if ((SelectedLOC++) == 3) {
          SelectedLOC = 0;
        }
        Position = 0;
        refresh = 0x01;
        LOC_address_new = LOC_address[SelectedLOC]; //get new LOC_address from storage
        break;
      case AliasLeft:
        if ((Position++) == 4) {
          Position = 0;
        }
        refresh = 0x01;
        break;
      case AliasRight:
        if (Position != 0) {
          Position--;
        }
        refresh = 0x01;
        break;
      case (AliasLeft+AliasRight):
        digitNum = 5;
        written_address = 0;
        while (digitNum != Position) {
          tempDigit = ((LOC_address_new - written_address) / tenthpower(--digitNum));
          written_address = written_address + (tempDigit * tenthpower(digitNum));
        }
        change = tenthpower(digitNum);
        if (change + LOC_address_new < 10237 && tempDigit != 9) {
          LOC_address_new = LOC_address_new + tenthpower(Position);
        } else {
          if (LOC_address_new - (change * tempDigit) == 0) {
            LOC_address_new = 1;
          } else {
            LOC_address_new = LOC_address_new - (change * tempDigit);
          }
        }
        refresh = 0x01;
        break;
      default:
        refresh = 0x02;
        break;
    }
    refresh_Poti();
  }
}

void Menu_ChangeIcon() {
  byte Position = 0; //Max 79
  byte IconCache[16];
  if (LOC_address[SelectedLOC] < 101) {
    for (byte LCD_row = 0; LCD_row < 16; LCD_row++) {
      if (((LCD_row * 5) % 8) > 3) { //if 5 bit cross 8 byte border of eeprom TODO TEST THIS
        IconCache[LCD_row] = 0x1F & ((EEPROM.read(((LOC_address[SelectedLOC] - 1) * 10) + ((LCD_row * 5) / 8)) << (((LCD_row * 5) % 8) - 3)) | (EEPROM.read(((LOC_address[SelectedLOC] - 1) * 10) + (((LCD_row * 5) / 8) + 1)) >> (11 - ((LCD_row * 5) % 8))));
      } else {
        IconCache[LCD_row] = 0x1F & (EEPROM.read(((LOC_address[SelectedLOC] - 1) * 10) + ((LCD_row * 5) / 8)) >> (3 - ((LCD_row * 5) % 8))); //read data from byte and shift accordingly
      }
    }
  }
  byte refresh = 0x01;
  while (true) {
    if (refresh == 0x01) {
      print_string_LCD("Change Icon LOC", 0x80);
      LCD_write(0x31 + SelectedLOC, true);
      print_string_LCD("IC:", 0xC0);
      LCD_write(0x31 + (Position / 40), true);
      LCD_write(' ', true);
      LCD_write('R', true);
      LCD_write(':', true);
      LCD_write(0x31 + ((Position % 40) / 5), true);
      LCD_write(' ', true);
      LCD_write(SelectedLOC << 1, true);
      LCD_write((SelectedLOC << 1) + 1, true);
      byte pixel = 4;
      do {
        if ((IconCache[Position / 5] & (0x01 << pixel)) > 0) {
          LCD_write('x', true); //set (black box)
        } else {
          LCD_write(0xF8, true); //not set
        }
      } while ((pixel--) != 0);
      for (byte shift = ((Position % 5) + 1); shift != 0; shift--) {
        LCD_move_left();
      }
    }
    switch (getButtonInput()) {
      case AliasStop:
        Menu_Stop();
        refresh = 0x01;
        break;
      case AliasMenu://TODO check if loco is valid <1000 for settings edit
        {
          unsigned int EEPROM_address = (LOC_address[SelectedLOC] - 1) * 10; //because Loc addresses start at 1
          byte ICONrow = 0;
          byte byte_offset = 0;
          byte value_to_store = 0x00;
          while (ICONrow != 16) { //edit
            do { //if we don't need to shift a bit right we are not finished yet
              value_to_store = value_to_store | (IconCache[ICONrow++] << (((3 + 8) - byte_offset) % 8));
              byte_offset = ((ICONrow * 5) % 8);
            } while ((11 - byte_offset) % 8 < 3); //we need to shift value to far to the left
            value_to_store = value_to_store | (IconCache[ICONrow] >> (byte_offset - 3));
            EEPROM.update(EEPROM_address++, value_to_store);
            value_to_store = 0x00;
          }
        }
        LCD_clear();
        return;
        break;
      case AliasLOCswitch:
        { unsigned int EEPROM_address = (LOC_address[SelectedLOC] - 1) * 10; //because Loc addresses start at 1
          byte ICONrow = 0;
          byte byte_offset = 0;
          byte value_to_store = 0x00;
          while (ICONrow != 16) { //edit
            do { //if we don't need to shift a bit right we are not finished yet
              value_to_store = value_to_store | (IconCache[ICONrow++] << (((3 + 8) - byte_offset) % 8));
              byte_offset = ((ICONrow * 5) % 8);
            } while ((11 - byte_offset) % 8 < 3); //we need to shift value to far to the left
            value_to_store = value_to_store | (IconCache[ICONrow] >> (byte_offset - 3));
            EEPROM.update(EEPROM_address++, value_to_store);
            value_to_store = 0x00;
          }
        }
        if ((SelectedLOC++) == 3) {
          SelectedLOC = 0;
        }
        for (byte LCD_row = 0; LCD_row < 16; LCD_row++) {
          for (byte LCD_row = 0; LCD_row < 16; LCD_row++) {
            if (((LCD_row * 5) % 8) > 3) { //if 5 bit cross 8 byte border of eeprom TODO TEST THIS
              IconCache[LCD_row] = 0x1F & ((EEPROM.read(((LOC_address[SelectedLOC] - 1) * 10) + ((LCD_row * 5) / 8)) << (((LCD_row * 5) % 8) - 3)) | (EEPROM.read(((LOC_address[SelectedLOC] - 1) * 10) + (((LCD_row * 5) / 8) + 1)) >> (11 - ((LCD_row * 5) % 8))));
            } else {
              IconCache[LCD_row] = 0x1F & (EEPROM.read(((LOC_address[SelectedLOC] - 1) * 10) + ((LCD_row * 5) / 8)) >> (3 - ((LCD_row * 5) % 8))); //read data from byte and shift accordingly
            }
          }
        }
        refresh = 0x01;
        break;
      case AliasLeft:
        if ((Position++) == 59) {
          Position = 0;
        }
        refresh = 0x01;
        break;
      case AliasRight:
        if (Position != 0) {
          Position--;
          refresh = 0x01;
        }
        break;
      case (AliasLeft+AliasRight):
        IconCache[Position / 5] = IconCache[Position / 5] ^ (0x01 << (Position % 5));
        LCD_write(SelectedLOC * 16 | 0x40, false); //Set dd-ram address
        for (byte LCD_row = 0; LCD_row < 16; LCD_row++) {
          LCD_write(IconCache[LCD_row], true); //write changed lines
        }
        refresh = 0x01;
        break;
      default:
        refresh = 0x00;
        break;
    }
    refresh_Poti();
  }

}

void setup() {
  pinMode(ButtonLeft, INPUT_PULLUP);
  pinMode(ButtonRight, INPUT_PULLUP);
  pinMode(ButtonLOCswitch, INPUT_PULLUP);
  pinMode(ButtonMenu, INPUT_PULLUP);
  pinMode(ButtonStop, INPUT_PULLUP);
  digitalWrite(LEDG1, HIGH);
  pinMode(LEDG1, OUTPUT);
  digitalWrite(LEDG2, LOW);
  pinMode(LEDG2, OUTPUT);
  digitalWrite(LEDO, HIGH);
  pinMode(LEDO, OUTPUT);
  unsigned int EEPROM_address = 0x03E8;                                                                       //Address after first 100 stored loco data (1000) (directly after icons)
  LCD_init4bit();                                                                                     //initialize LCD display
  Serial.begin(115200, SERIAL_8N2);                                                                   //Init DCC Serial Interface
  for (byte LOC_num = 0; LOC_num < 4; LOC_num++) {                                                     //load LOC addresses TODO impelement read function data
    LOC_address[LOC_num] = (EEPROM.read(EEPROM_address) << 8) | (EEPROM.read(EEPROM_address + 1));    //combine data to address
    if (LOC_address[LOC_num] > 10239) {
      LOC_address[LOC_num] = LOC_num + 1;
    }
    update_LOC_icon(LOC_num);                                                                         //load LOC icons
    EEPROM_address = EEPROM_address + 2;                                                              //increment to next loco (address has 2 bytes (+2))
  }
  Menu_None();
}


void Menu_None() {
  LCD_clear();
  byte refresh = 0x0F;
  while (true) {
    for (byte changedLoc = 0; changedLoc < 4; changedLoc++) {
      if ((refresh & (0x01 << changedLoc)) != 0) {
        byte tempDigit = 0;
        byte digitNum = 3;
        byte written_speed = 0;
        switch (changedLoc) {
          case 0:
            LCD_write(0x80, false); //set DD-ram-address to first row first char
            break;
          case 1:
            LCD_write(0x88, false); //set DD-ram-address to first row eights char
            break;
          case 2:
            LCD_write(0xC0, false); //set DD-ram-address to second row first char
            break;
          case 3:
            LCD_write(0xC8, false); //set DD-ram-address to second row eights char
            break;
        }
        LCD_write((changedLoc * 2), true); //first part of custom icon
        LCD_write(((changedLoc * 2) + 1), true); //second part of custom icon
        LCD_write(0x20, true); //print ' '
        while (digitNum != 0) {
          tempDigit = ((LOC_speed[changedLoc] - written_speed) / tenthpower(--digitNum));
          if (tempDigit == 0) {
            if ((written_speed == 0) && (digitNum != 0)) {
              LCD_write(0x20, true); //print ' '
            } else {
              LCD_write(0x30, true); //print '0'
            }
          } else {
            LCD_write(0x30 + tempDigit, true);
            written_speed = written_speed + (tempDigit * tenthpower(digitNum));
          }
        }

        if (LOC_speed[changedLoc] == 0) {
          LCD_write(0x7C, true); //write "|" character if speed null
        } else if (LOC_function_and_dir[changedLoc] > 223) {
          LCD_write(0x7E, true); //write "->" character
        } else {
          LCD_write(0x7F, true); //write "<-" character
        }
      }
    }
    if (refresh > 1) { //routin for cursor positioning
      switch (SelectedLOC) {
        case 0:
          LCD_write(0x82, false);
          break;
        case 1:
          LCD_write(0x8A, false);
          break;
        case 2:
          LCD_write(0xC2, false);
          break;
        case 3:
          LCD_write(0xCA, false);
          break;
      }
    }
    switch (getButtonInput()) {
      case AliasStop:
        Menu_Stop();
        refresh = 0x0F;
        break;
      case AliasMenu:
        Menu_ChangeAddress();
        Menu_ChangeFunction();
        Menu_ChangeIcon();
        refresh = 0x0F;
        break;
      case AliasLOCswitch:
        if ((SelectedLOC++) == 3) {
          SelectedLOC = 0;
        }
        refresh = 0x10;
        break;
      default:
        refresh = refresh_Poti();
    }
  }
}
void loop() {

}

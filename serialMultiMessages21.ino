/* Serially managed message board

Version: 0.2
   Author: quarterturn
   Date: 8/25/2012
   Hardware: Any ATMEGA with at least 1K EEPROM
  
   An electronic sign which can display and edit a number of strings stored in EEPROM,
   along with a program for which strings and for how long.

   Also displays a single string for a fixed time period - useful as a caller-ID display.   
   
   User interaction is via the serial interface. The serial baudrate is 9600. 
   The interface provides a simple menu selection which should work in most
   terminal emulation programs.
   
*/

// for reading flash memory
#include <avr/pgmspace.h>

// for using eeprom memory
#include <avr/eeprom.h>

// for LCD
#include <LiquidCrystal.h>

// memory size of the internal EEPROM in bytes
#define EEPROM_BYTES 1024

// how many strings
// 81 char x 6 strings
#define NUM_STRINGS 6

// message string size
#define MAX_SIZE 81

// display size
#define DISPLAY_CHARS 40

// total characters in LCD
#define LCD_TOTAL_SIZE 40

// lcd width
#define LCD_ROW_SIZE 20

// default message time in seconds
#define DEFAULT_TIME 5

// time to pause with blank screen between messages
// in milliseconds
#define INTER_DELAY 200

// serial baudrate
#define SERIAL_BAUD 9600

// compare this to what is stored in the eeprom to see if it has been cleared
#define EE_MAGIC_NUMBER 0xBADA

// for tracking time in delay loops
// global so it can be used in any function
unsigned long previousMillis;

// row flag - keeps track of the last row used for scrolling mode
byte row = 0;

// slot
byte slot;

// message number
byte msgNum;

// message time
byte msgTime;

// a global to contain serial menu input
char menuChar;

// global to track main menu display
byte mainMenuCount = 0;

// global buffer for a string copied from internal FLASH or EEPROM
// initialize to '0'
char currentString[MAX_SIZE] = {0};

// RAM copy of eeprom magic number
uint16_t magicNumber;

// eeprom message string
uint8_t EEMEM ee_msgString0[MAX_SIZE];
uint8_t EEMEM ee_msgString1[MAX_SIZE];
uint8_t EEMEM ee_msgString2[MAX_SIZE];
uint8_t EEMEM ee_msgString3[MAX_SIZE];
uint8_t EEMEM ee_msgString4[MAX_SIZE];
uint8_t EEMEM ee_msgString5[MAX_SIZE];

// eeprom magic number to detect if eeprom has been cleared
uint16_t EEMEM ee_magicNumber;

// loop break flag on input
byte breakout = 0;

// arrays stored in RAM
uint8_t msgIndexes[NUM_STRINGS];
uint8_t msgTimes[NUM_STRINGS];

// arrays store in EEPROM
uint8_t EEMEM ee_msgIndexes[NUM_STRINGS];
uint8_t EEMEM ee_msgTimes[NUM_STRINGS];

// display power save - default is off
byte powerSave = 1;

// LDR connected to Arduino pin 0
int LDRpin = 0;

// LDR voltage from divider (10 kohm)
int LDRreading;

// create an LCD object and assign the pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

//---------------------------------------------------------------------------------------------//
// setup
//---------------------------------------------------------------------------------------------//
void setup()
{
  // set up the serial port
  Serial.begin(SERIAL_BAUD);
  
  // see if the eeprom has been cleared
  // if it has, initialize it with default values
  magicNumber = eeprom_read_word(&ee_magicNumber);
  if (magicNumber != EE_MAGIC_NUMBER)
  {
    initEeprom();
  }
  
  // configure the LCD size for 20x2
  lcd.begin(LCD_ROW_SIZE, 2);
  
  // clear the lcd
  lcd.clear();
  
  // set brightness to 50%
  lcd.vfdDim(2);
  
  // turn the display off
  lcd.noDisplay();
  
  // read the values from the EEPROM into the arrays stored in RAM
  eeprom_read_block((void*)&msgIndexes, (const void*)&ee_msgIndexes, sizeof(msgIndexes));
  eeprom_read_block((void*)&msgTimes, (const void*)&ee_msgTimes, sizeof(msgTimes));
  
} // end setup

//---------------------------------------------------------------------------------------------//
// main loop
//---------------------------------------------------------------------------------------------//
void loop()
{ 
  // if there is something in the serial buffer read it
  if (Serial.available() >  0) 
  {
    // print the main menu once
    if (mainMenuCount == 0)
    {
      clearAndHome();
      Serial.println(F("Message Box - Main Menu"));
      Serial.println(F("----------------------"));
      Serial.println(F("1     Show String"));
      Serial.println(F("2     Edit String"));
      Serial.println(F("3     Instant Message"));
      Serial.println(F("4     Edit Program"));
      Serial.println(F("5     Display Program"));
      Serial.println(F("6     Display On/Off"));
      Serial.print(F("<ESC> Exit setup"));
      Serial.println();
      mainMenuCount = 1;
    }
      
    menuChar = Serial.read();
    if (menuChar == '1')
    {
      // show the string
      clearAndHome();
      showString();
    }
    else if (menuChar == '2')
    {
      // edit the string
      clearAndHome();
      editString();
    }
    else if (menuChar == '3')
    {
      // send message to the display right now
      clearAndHome();
      // if the lcd is off, turn it on
      if (powerSave == 1)
      {
        lcd.display();
      }
      Serial.println(F("Instant mode selected."));
      Serial.println(F("Enter your message: "));
      // get a string from serial for currentString
      getSerialString();
      Serial.println();
      Serial.println(F("Current message: "));
      Serial.println(currentString);
      // clear the lcd
      lcd.clear();
      // display it on the lcd
      if (strlen(currentString) < 41)
      {
        displayMsgDirect(currentString);
      }
      else
      {
        displayMsgScroll(currentString);
      }
      // pause for 60 seconds
      previousMillis = millis();
      while (millis() - previousMillis < (60000))
      {
        if (Serial.available() > 0)
        {
          // break the delay loop
          break;
        }
      }
      lcd.clear();
      // turn display off if it should be off
      if (powerSave == 1)
      {
        lcd.noDisplay();
      }       
    }
    else if (menuChar == '4')
    {
      // edit the program
      clearAndHome();
      editProgram();
    }
    else if (menuChar == '5')
    {
      // display the program
      clearAndHome();
      displayProgram();
    }
    else if (menuChar == '6')
    {
      // toggle display on or off
      if (powerSave == 0)
      {
        lcd.noDisplay();
        powerSave = 1;
      }
      else
      {
        lcd.display();
        powerSave = 0;
      }
    }
    else if (menuChar == 27)
    {
      mainMenuCount = 1;
    }
  }
  // reset the main menu if the serial connection drops
  mainMenuCount = 0;
  
  Serial.flush();
  
  // follow the display program
  // slot
  for (slot = 0; slot < NUM_STRINGS; slot++)
  {
    // msg number
    msgNum = msgIndexes[slot];
    // msg time
    msgTime = msgTimes[slot];
    // if msgTime = 0 skip the message
    if (msgTime > 0)
    {
      // fetch the string from EEPROM
      readEepromBlock(msgNum);      
      // clear the lcd
      lcd.clear();
      // display it on the lcd
      if (strlen(currentString) < 41)
      {
        displayMsgDirect(currentString);
      }
      else
      {
        displayMsgScroll(currentString);
      }
      // pause for msgTime seconds
      previousMillis = millis();
      while (millis() - previousMillis < (msgTime * 1000))
      {
        if (Serial.available() > 0)
        {
          breakout = 1;
          // break the delay loop
          break;
        }
      }
      lcd.clear();
      setDisplayBright();
      // pause between messages
      previousMillis = millis();
      while (millis() - previousMillis < INTER_DELAY)
      {
        if (Serial.available() > 0)
        {
          breakout = 1;
          // break the delay loop
          break;
        }
      }
      if (breakout == 1)
      {
        breakout = 0;
        // break the message program loop
        break;
      }
    }
  } 
}

//---------------------------------------------------------------------------------------------//
// function clearAndHome
// returns the cursor to the home position and clears the screen
//---------------------------------------------------------------------------------------------//
void clearAndHome()
{
  Serial.write(27); // ESC
  Serial.print("[2J"); // clear screen
  Serial.write(27); // ESC
  Serial.print("[H"); // cursor to home
}

//---------------------------------------------------------------------------------------------//
// function showString
// displays the string stored in EEPROM
//---------------------------------------------------------------------------------------------//
void showString()
{
  byte msgCount = 0;
  
  // clear the terminal
  clearAndHome();
  
  // clear the string in RAM
  sprintf(currentString, "");
  
  // display the menu on entry
  Serial.println(F("Here are the strings"));
  Serial.println(F("that are stored in the EEPROM:"));
  Serial.println(F("-------------------------"));
  Serial.println();
  // read the string stored in EEPROM starting at address 0 into the RAM string
  while (msgCount < NUM_STRINGS)
  {
    readEepromBlock(msgCount);
    Serial.print("String ");
    Serial.print(msgCount, DEC);
    Serial.print(": ");
    Serial.println(currentString);
    msgCount++;
  }
  
  Serial.println();
  Serial.println(F("<ESC> return to Main Menu"));
  Serial.flush();

  // poll serial until exit
  while (1)
  {
    // if there is something in the serial buffer read it
    if (Serial.available() >  0) 
    {
      menuChar = Serial.read();
      if (menuChar == 27)
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // return to main menu and return the mode
        return;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------//
// function displayProgram
// shows the program stored in the EEPROM
//---------------------------------------------------------------------------------------------//
void displayProgram()
{
  // message values
  int slot;
  int msgNum;
  int msgTime;
  
  Serial.println(F("MessageBox - Display Message Program"));    
  Serial.println(F("This is a list of message slots, times"));
  Serial.println(F("and messages."));
  Serial.println(F("----------------------------------"));
  Serial.println(F("slot:msgNum:msgTime:Message"));
  
  for (slot = 0; slot < NUM_STRINGS; slot++)
  {
    msgNum = msgIndexes[slot];
    msgTime = msgTimes[slot];
    // clear the string in RAM
    sprintf(currentString, "");
    readEepromBlock(msgNum);
    Serial.print(slot, DEC);
    Serial.print(":");
    Serial.print(msgNum, DEC);
    Serial.print(":");
    Serial.print(msgTime, DEC);
    Serial.print(":");
    Serial.println(currentString);
  }
  Serial.println();
  Serial.println(F("<ESC> return to Main Menu"));
  Serial.flush();
  // poll serial until exit
  while (1)
  {
    // if there is something in the serial buffer read it
    if (Serial.available() >  0) 
    {
      menuChar = Serial.read();
      if (menuChar == 27)
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // return to main menu and return the mode
        sprintf(currentString, "");
        return;
      }
    }
  }
}

//---------------------------------------------------------------------------------------------//
// function editProgram
// edits the display program stored in the EEPROM
//---------------------------------------------------------------------------------------------//
void editProgram()
{
  // clear the terminal
  clearAndHome();
  // track menu display
  byte displayEditProgramMenu = 0;
  
  // message values
  int slot;
  int msgNum;
  int msgTime;
  
  // input char
  char menuChar;
  
  // input valid flag
  byte inputBad = 1;
  
  // loop flag
  byte loopFlag = 1;
  
  // display the menu on entry
  if (displayEditProgramMenu == 0)
  {
    Serial.println(F("MessageBox - Edit Message Program"));    
    Serial.println(F("This is a list of message slots and message times."));    
    Serial.print(F("If a message is bigger than "));
    Serial.print(DISPLAY_CHARS);
    Serial.println(F(" characters it will scroll."));
    Serial.println(F("Otherwise, it displays directly."));   
    Serial.println(F("To skip a message set the time to zero."));    
    Serial.println(F("---------------------------"));   
    Serial.println(F("Slots available: "));   
    Serial.print("0 to ");
    Serial.println(NUM_STRINGS - 1, DEC);
    
    displayEditProgramMenu = 1;
  }
  
  // loop until we return from the function
  while(1)
  {
    Serial.println(F("Enter the number of the slot to edit: "));
    // loop until the input is acceptable
    while (inputBad)
    {
     slot = getSerialInt();
     if ((slot >= 0) && (slot < NUM_STRINGS))
     {
       inputBad = 0;
     }
     else
     {
       Serial.println(F("Error: slot "));      
       Serial.print(slot);
       Serial.println(F(" is out of range. Try again."));
       
     }   
    }
    // reset the input test flag for the next time around
    inputBad = 1;
    
    // show the choice since no echo
    Serial.println(F("Slot: "));   
    Serial.println(slot);   
    Serial.println(F("Enter the message number: "));    
    // loop until the input is acceptable
    while (inputBad)
    {
     msgNum = getSerialInt();
     if ((msgNum >= 0) && (slot < NUM_STRINGS))
     {
       inputBad = 0;
     }
     else
     {
       Serial.println(F("Error: message "));       
       Serial.print(msgNum);
       Serial.println(F(" is out of range. Try again."));
       
     }   
    }
    // reset the input test flag for the next time around
    inputBad = 1;
    
     // fetch the string from EEPROM
    readEepromBlock(msgNum);

    // print the string to serial
    Serial.println(F("Message "));   
    Serial.print(msgNum);
    Serial.print(": ");
    Serial.println(currentString);    
    Serial.println(F("Enter the message time in seconds 0-255: "));
    
    // loop until the input is acceptable
    while (inputBad)
    {
      msgTime = getSerialInt();
      if ((slot >= 0) && (slot < 256))
      {
        inputBad = 0;
      }
      else
      {
        Serial.println(F("Error: Time "));       
        Serial.print(slot);
        Serial.println(F(" is out of range. Try again."));      
      }   
    }
    // reset the input test flag for the next time around
    inputBad = 1;
    
    // show the choice since no echo
    Serial.println(F("Time: "));   
    Serial.println(msgTime);       
    // write the data to the eeprom
    Serial.println(F("Writing data to eeprom..."));    
    // write the message number into the slot
    msgIndexes[slot] = msgNum;
    // write the message time into the slot
    msgTimes[slot] = msgTime;
    
    Serial.println(F("Edit another? (y/n): "));    
    Serial.flush();
    while (loopFlag)
    {
      if (Serial.available() > 0)
      {
        menuChar = Serial.read();
        if (menuChar == 'n')
        {
          Serial.println(menuChar);
          
          // write the program to the EEPROM before we return
          eeprom_write_block((const void*)&msgIndexes, (void*) &ee_msgIndexes, sizeof(msgIndexes));
          eeprom_write_block((const void*)&msgTimes, (void*) &ee_msgTimes, sizeof(msgTimes));
          
          return;
        }
        if (menuChar == 'y')
        {
          loopFlag = 0;
          Serial.println(menuChar);
          Serial.flush();
        }
      }
    }
    loopFlag = 1;
  }
  return;
}


//---------------------------------------------------------------------------------------------//
// function editString
// edits the string and writes it to EEPROM
//---------------------------------------------------------------------------------------------//
void editString()
{
  
  // track how many characters entered
  byte cCount;
  // track menu display
  byte displayEditProgramMenu = 0;  
  // input valid flag
  byte inputBad = 1;  
  // slot number
  int msgNum;
  // track when string is done
  byte stringDone = 0;
  
  // clear the terminal
  clearAndHome();
  
  // clear the string in RAM
  sprintf(currentString, "");
  
  // display the menu on entry
  if (displayEditProgramMenu == 0)
  {
    // display the menu on entry
    Serial.println(F("Enter a new string to be stored in EEPROM"));
    Serial.println(F("up to "));
    Serial.print(MAX_SIZE - 1, DEC);
    Serial.println(F(" characters."));
    Serial.println(F("-------------------------"));
    Serial.print(F("Choose a message 0 to "));
    Serial.println(NUM_STRINGS - 1, DEC);
    Serial.println(F(" or enter "));
    Serial.print(NUM_STRINGS);
    Serial.print(F(" to exit"));
    Serial.println();
  }
  
  // poll serial until exit
  while (1)
  {
    // set the string index to 0 each time through the loop
    cCount = 0;
    
    Serial.print(F("Enter the number of the message to edit: "));
    Serial.println();
    // loop until the input is acceptable
    while (inputBad)
    {
      msgNum = getSerialInt();
      // slots 0 to NUM_STRINGS
      // 0 is ok as it means exit
      if ((msgNum >= 0) && (msgNum <= NUM_STRINGS))
      {
        inputBad = 0;
      }
      else
      {
        Serial.print(F("Error: message "));
        Serial.print(msgNum);
        Serial.println(F(" is out of range. Try again."));
      }   
    }
    // reset the input test flag for the next time around
    inputBad = 1;
    
    // the user wants to edit a slot
    if (msgNum < NUM_STRINGS)
    {
      // show the choice since no echo
      Serial.print(F("Message: "));
      Serial.println(msgNum);
      Serial.flush();
      // get the string for currentString
      getSerialString();    
      // reset string done flag
      stringDone = 0;
      // write the string to the EEPROM
      writeEepromBlock(msgNum);
      // display the string
      Serial.println();
      Serial.println(F("You entered: "));
      Serial.println(currentString);
      Serial.println(F("<y> to enter another or"));
      Serial.println(F("<n> return to Main Menu"));
      Serial.flush();
      while (menuChar != 'y')
      {
        if (Serial.available() > 0)
        {
          menuChar = Serial.read();
          if (menuChar == 'n')
          {
            Serial.println(menuChar);
            // set flag to redraw menu
            mainMenuCount = 0;
            // return to main menu
            return;
          }
          if (menuChar == 'y')
          {
            Serial.println(menuChar);
          }
        }
      } // end of the y/n input loop     
    }
    // the user did not want to edit anything
    else
    {
      break;
    }
  } // end of the edit string loop
  // set flag to redraw menu
  mainMenuCount = 0;
  // return to main menu
  return;
}

//---------------------------------------------------------------------------------------------//
// function getSerialInt
// uses serial input to get an integer
//---------------------------------------------------------------------------------------------//
int getSerialInt()
{
  char inChar;
  int in;
  int input = 0;
  
  Serial.flush();  
  do
  // do at least once
  {
    while (Serial.available() > 0)
    {
      inChar = Serial.read();
       // echo the input
       Serial.print(inChar);
       // convert 0-9 character to 0-9 int
       in = inChar - '0';
       if ((in >= 0) && (in <= 9))
       {          
          // since numbers are entered left to right
          // the current number can be shifted to the left
          // to make room for the new digit by multiplying by ten
          input = (input * 10) + in;
        }
     }
  }
  // stop looping when an ^M is received
  while (inChar != 13);
  // return the number
  return input;
}

//---------------------------------------------------------------------------------------------//
// function getSerialString
// uses serial input to get a string
//---------------------------------------------------------------------------------------------//
void getSerialString()
{  
  // track how many characters entered
  byte cCount = 0; 
  // input valid flag
  byte inputBad = 1;  
  // track when string is done
  byte stringDone = 0;
  
  // clear the string in RAM
  sprintf(currentString, "");
  
  Serial.flush();
  // loop until done
  while (stringDone == 0)
  {
    // if there is something in the serial buffer read it
    while (Serial.available() >  0) 
    {
      // grab a character
      menuChar = Serial.read();
      // echo the input
      Serial.print(menuChar);
      
      // do stuff until we reach MAX_SIZE
      if (cCount < (MAX_SIZE - 1))
      {
        // pressed <ENTER> (either one)
        if ((menuChar == 3) || (menuChar == 13))
        {
          // set flag to redraw menu
          mainMenuCount = 0;
          // make the last character a null
          // cCount++;
          currentString[cCount] = 0;
          // mark the string done
          stringDone = 1;             
        }
        // if we are not at the end of the string and <delete> not pressed
        else if (menuChar != 127)
        {
          currentString[cCount] = menuChar;
          cCount++;
        }
        // if index is between start and end and delete is pressed
        // clear the current character and go back one in the index
        else if ((cCount > 0) && (menuChar == 127))
        {
          currentString[cCount] = 0;
          cCount--;
          // print a backspace to the screen so things get deleted
          Serial.write(8);
        }
      }
      // we reached MAX_SIZE
      else
      {
        // set flag to redraw menu
        mainMenuCount = 0;
        // set the current character to null
        currentString[cCount] = 0;
        // mark the string as done
        stringDone = 1;
      }
    }
  } // end of the string input loop

  return;
}



//---------------------------------------------------------------------------------------------//
// function readEeepromBlock
// reads a block from eeprom into a char arry
// uses globals
// returns nothing
//---------------------------------------------------------------------------------------------//
void readEepromBlock(byte msgNum)
{
  switch (msgNum)
  {
    case 0:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString0, sizeof(currentString));
      break;
    case 1:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString1, sizeof(currentString));
      break;
    case 2:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString2, sizeof(currentString));
      break;
    case 3:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString3, sizeof(currentString));
      break;
    case 4:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString4, sizeof(currentString));
      break;
    case 5:
      eeprom_read_block((void*)&currentString, (const void*)&ee_msgString5, sizeof(currentString));
      break;
    default:
      break;
  }
}

//---------------------------------------------------------------------------------------------//
// function writeEeepromBlock
// writes a block from a char array into the eeprom
// uses globals
// returns nothing
//---------------------------------------------------------------------------------------------//
void writeEepromBlock(byte msgNum)
{
  switch (msgNum)
  {
    case 0:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString0, sizeof(currentString));
      break;
    case 1:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString1, sizeof(currentString));
      break;
    case 2:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString2, sizeof(currentString));
      break;
    case 3:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString3, sizeof(currentString));
      break;
    case 4:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString4, sizeof(currentString));
      break;
    case 5:
      eeprom_write_block((const void*) &currentString, (void*) &ee_msgString5, sizeof(currentString));
      break;
    default:
      break;
  }
}

//---------------------------------------------------------------------------------------------//
// function initEeprom
// loads default values into the eeprom if nothing is present based on ee_magicNumber
// not having the correct value
//---------------------------------------------------------------------------------------------//
void initEeprom()
{
  
  int slot;
  // make all slots default to message 0 with 0 seconds
  for (slot = 1; slot < (NUM_STRINGS - 1); slot++)
  {
    // write the message number into the slot
    // msg number
    msgIndexes[slot] = 0;
    // msg time
    msgTimes[slot] = 0;
  }
  // set the first slot to whatever the default time is
  msgTimes[0] = DEFAULT_TIME;
  
  // set the default messages to blank except the first
  sprintf(currentString, "Hello, World!");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString0, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString1, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString2, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString3, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString4, sizeof(currentString));
  sprintf(currentString, "");
  eeprom_write_block((const void*) &currentString, (void*) &ee_msgString5, sizeof(currentString));

  // set the magic number
  eeprom_write_word(&ee_magicNumber, EE_MAGIC_NUMBER);
  
  // copy the arrays in RAM to the EEPROM
  eeprom_write_block((const void*)&msgIndexes, (void*) &ee_msgIndexes, sizeof(msgIndexes));
  eeprom_write_block((const void*)&msgTimes, (void*) &ee_msgTimes, sizeof(msgTimes));
  
  // read it back into RAM
  eeprom_read_block((void*)&msgIndexes, (const void*)&ee_msgIndexes, sizeof(msgIndexes));
  eeprom_read_block((void*)&msgTimes, (const void*)&ee_msgTimes, sizeof(msgTimes));
  
  sprintf(currentString, "");
}

//---------------------------------------------------------------------------------------------//
// function displayMsgDirect()
// displays a string directly to an LCD
// breaks the string up between the two lines
// expects a pointer to an array of chars (a string)
// return nothing
//---------------------------------------------------------------------------------------------//
void displayMsgDirect(char *currentString)
{
  int position = 0,
      breakflag = 0;
  char outChar;
  
  // set cursor to upper left
  // first row
  lcd.home();
  for (position = 0; position < LCD_ROW_SIZE; position++)
  {
     outChar = *(currentString + position);
     // break if end of string
     if (outChar == '\0')
     {
       breakflag = 1;
       break;
     }
     lcd.write(outChar);
  }
  // second row if string did not end on first
  if (breakflag == 0)
  {
    lcd.setCursor(0, 1);
    for (position = LCD_ROW_SIZE; position < LCD_TOTAL_SIZE; position++)
    {
      outChar = *(currentString + position);
      // break if end of string
      if (outChar == '\0')
      {
        break;
      }
      lcd.write(outChar);
    }
  }
  return;
}

//---------------------------------------------------------------------------------------------//
// function displayMsgScroll()
// scrolls a string from right to left across an LCD
// alternaltes between lines to even wear on a lcd-compatible VFD
// expects a pointer to an array of chars
// returns nothing
//---------------------------------------------------------------------------------------------//
void displayMsgScroll(char *currentString)
{
  int strPos,
    bufPos,
    relStrPos,
    strSize,
    bufSize,
    r;
    
  char displayBuffer[21] = "                    ";
    
  bufSize = (strlen(displayBuffer)) - 1;
  strSize = (strlen(currentString)) - 1;
  
  // slide the buffer from the left side of the string all the way
  // over to the right side plus the length of the buffer
  for (strPos = 0; strPos < (strSize + bufSize + 1); strPos++)
  { 
    // fill the buffer with spaces to clear it each time for display
    // we are only going to copy data where it exists
    // but only if we aren't past the end of the string
    if (strPos < strSize)
    {
      for (r = 0; r < (strlen(displayBuffer) - 1); r++)
      {
        displayBuffer[r] = ' ';
      }
    }
    // store the current position in the string
    relStrPos = strPos;
    // start from the right side of the buffer
    bufPos = bufSize;
    // loop until we try to copy past the left side of the string
    // or we reach the left side of the buffer
    while ((relStrPos > -1) && (bufPos > -1))
    {
      // only copy if the relative position in the string based on the position in
      // the buffer is not past the right side of the string
      if (relStrPos < (strSize + 1))
      {
        displayBuffer[bufPos] = currentString[relStrPos];
      }
      else
      // if the buffer is past the end of the string insert spaces
      {
        displayBuffer[bufPos] = ' ';
      }
      
      // move left one in the string
      relStrPos--;
      // move left one in the buffer
      bufPos--;
    }
    
    // set the cursor to the start of the row
    lcd.setCursor(0, row);
    
    // send the buffer to the lcd
    lcd.print(displayBuffer);
    
    // loop and check serial for data
    previousMillis = millis();
    while (millis() - previousMillis < INTER_DELAY)
    {
      if (Serial.available() > 0)
      {
        // jump back to main
        // even the wear on the VFD by toggling the row used
        if (row == 0)
        {
          row = 1;
        }
        else
        {
          row = 0;
        }   
        lcd.clear();
        return;
      }
    }
    
  }
  
  // even the wear on the VFD by toggling the row used
  if (row == 0)
  {
    row = 1;
  }
  else
  {
    row = 0;
  }   
  lcd.clear(); 
}

//---------------------------------------------------------------------------------------------//
// function setDisplayBright()
// sets the brightness of a Noritake LCD-compatible vfd
// based on ambient light level
// requires customized LiquidCrystal library with lcd.vfdDim()
// expects nothing
// return nothing
// uses LDRreading and LDRpin as globals
//---------------------------------------------------------------------------------------------//
void setDisplayBright()
{
  LDRreading = analogRead(LDRpin);
  if (LDRreading > 800)
  {
    lcd.vfdDim(0);
  }
  else if ((LDRreading <= 800) && (LDRreading > 450))
  {
    lcd.vfdDim(1);
  }
  else if ((LDRreading <= 450) && (LDRreading > 200))
  {
    lcd.vfdDim(2);
  }
  else
  {
    lcd.vfdDim(3);
  }
  return;
}


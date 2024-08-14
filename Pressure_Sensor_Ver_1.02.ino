/*Release Notes
  Writen By : David Yoder
  Date      : 4/23/22
  Function  : Monitors the house and barn water system pressure and controls water pump in Sistern based on the parameters set by user via pushbuttons and Oled display.

  Version History
    Ver. 1.00
      *Alpha Testing.
    
    Ver. 1.01
      *Beta Testing.
      *Code Cleanup.
    Ver. 1.02
      Date : 6/30/23
      Pump will run immediately after power cycle.
        line 117
      Prime_PSI can be set to 0 effectively disabling it.
        line 354,359,363
      Pump_Protect is diasbled for 20 seseconds after power up to allow pump to automatically prime once after a power outage.
        line 73,398
      Pump can now be manually primed via push buttons until the water pressure reaches the Start PSI Set point.
        line 394

  */
// EEPROM
  #include<EEPROM.h>
  int EEPROM_Index = 0;

// Pushbuttons
  #include "MomentaryButton.h"
  byte Debounce_Dur = 50;
  long int No_Press_Last_ms = 0; // Time stamp of last time that no pushbuttons were pressed.
  MomentaryButton Enter_PB(2); //Define pin that pushbutton is conected to between ().
  bool Enter_PB_PState = LOW; // Previous state of pushbutton.
  long int 00000 = 0; // Time stamp of last time that Enter_PB was pressed.
  MomentaryButton Up_PB(3); //Define pin that pushbutton is conected to between ().
  bool Up_PB_PState = LOW; // Previous state of pushbutton.
  MomentaryButton Dn_PB(4); //Define pin that pushbutton is conected to between ().
  bool Dn_PB_PState = LOW; // Previous state of pushbutton.
  MomentaryButton Set_PB(5); //Define pin that pushbutton is conected to between ().
  bool Set_PB_PState = LOW; // Previous state of pushbutton.

// Display
  #include <SPI.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>

  // Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
  // The pins for I2C are defined by the Wire-library. 
  // On an arduino UNO:       A4(SDA), A5(SCL)
  // On an arduino MEGA 2560: 20(SDA), 21(SCL)
  #define OLED_RESET 4 // Reset pin # (or -1 if sharing Arduino reset pin)
  #define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
  #define SCREEN_WIDTH 128 // OLED display width, in pixels
  #define SCREEN_HEIGHT 64 // OLED display height, in pixels
  
  Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, & Wire, OLED_RESET); // Create display object

// Water Pressure
  const int Water_P_Sensor = A1; // Define pin that Water Pressure sensor is connected to.
  byte Water_P = 0; // Current Water Pressure

  //Calculate average Water Pressure
    const byte numReadings = 10; // Number of readings used to calculte average
    byte readings[numReadings]; // The readings from the analog input
    byte readIndex = 0; // The index of the current reading
    int total = 0; // The running total

	
// Water Pump
  const int Water_Pump_Run = 6; // Define pin that Water pump relay is connected to.
  bool Pump_Protect = false; // Switch this to false to allow pump to run immediately after power cycle.
  int Pump_Protect_Delay_On_Start_ms = 20000; // Delay Pump Protection on startup.
  byte Pump_Status = 0;
  long int Pump_Run_Last_ms = 0;
  int Pump_Run_Interval_ms = 5000; // Water Pump has to be off for this amount of time before it can run agian (ms).


// Pressure Switch Settings
  const char * Prams_Options[]  = {"Prime PSI", "Start PSI", "Stop PSI"}; // Define Option Names
  int optionValue[] = {20,30,50}; // Options Default values
  int optionValue_len = sizeof(optionValue)/sizeof(optionValue[0]); // Determine the number of options.
  byte Selected_Option = 0; // Current option that is selected.
  long int Update_Home_Last_ms = 0; // Time stamp of last home sreen update.
  int Update_Home_ms = 500; // Update display home screen at this rate.
  bool Print_Screen = true; // Update display.
  bool Edit_Mode = false; // Display is in edit mode.
  int Edit_Mode_Delay_ms = 2000; // Enter_PB has to be pushed for this amount of time to enter edit mode. 
  int No_Press_Return_Home_ms = 20000; // Return to Home screen if no pushbuttons have been pressed for this amount of time.
  bool Exiting_Edit_Mode = false;
  long int Exiting_Edit_Mode_last_ms = 0;
  int Exiting_Edit_Mode_ms = 1000; // The amount of time "menu extied" should be be displayed when menu is exited
  byte P_Hi_Limit = 100; // Max Pressure setpoint.
  byte P_Lo_Limit = 40; // Min Pressure setpoint.
  byte Pump_Protect_Limit = 20; // Pump Protect Pressure setpoint.
  byte P_Hyst = 10; // Min Hysteresis between min and max Pressure setpoint.

// Serial Print
  byte Enable_Serial_Print = 1; // Enable Serial printing.
  long int Print_Serial_Last_ms = 0; // Time stamp of last serial print.
  int Print_Serial_Interval = 1000; // Serial Print Update rate.

void setup(){

  // Serail Print to Console
    if(Enable_Serial_Print){
      Serial.begin(9600);
    }
  
  // Display
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      Serial.println(F("SSD1306 failed"));
      // for(;;); // Don't proceed, loop forever // this feature is commented out so Arduino can still controll pump even if display connection fails. 
    }

  // Water Pump
    pinMode(Water_Pump_Run, OUTPUT); // Define Water_Pump_Run as output.
    digitalWrite(Water_Pump_Run, HIGH); 

  // EEPROM
    // Check if this is first time EEPROM is used 
    if (EEPROM.read(256) != 123){ // not equal to 123 
      EEPROM.write(256,123); // write value 123 to byte 256 
      Serial.println(F("EEPROM.write"));
      for(int i = 0; i<optionValue_len;){ // Loop through options and set default values to EEPROM if EEPROM hasn't been used before.
        EEPROM.put(EEPROM_Index,optionValue[i]);
        Serial.print(F("EEPROM.write : "));
        Serial.print(EEPROM_Index);
        Serial.print(F(" , Val : "));
        Serial.println(optionValue[i]);
        EEPROM_Index = EEPROM_Index+2; // Index in increments of 2 to avoid memory overlaps.
        i = i+1;
      }
    }else{
      for(int i = 0 ; i<optionValue_len ;){ // Loop through options and read values from EEPROM and save to respective option.
        EEPROM.get(EEPROM_Index,optionValue[i]);
        Serial.print(F("EEPROM.read : "));
        Serial.print(EEPROM_Index);
        Serial.print(F(" , Val : "));
        Serial.println(optionValue[i]);
        if(i == 0){
          Pump_Protect_Limit = optionValue[i];
        }
        if(i == 1){
          P_Lo_Limit = optionValue[i];
        }
        if(i == 2){
          P_Hi_Limit = optionValue[i];
        }
        if(i == 3){
          Enable_Serial_Print = optionValue[i];
        }    
        EEPROM_Index = EEPROM_Index+2;
        i = i+1;
      }
    }
}

  // Functions
    // Function to return the current selected option
      char *ReturnOptionSelected(){
      char *menuOption = Prams_Options[Selected_Option];
      // Return optionSelected
      return menuOption;
      }

    // Function to return Value of current selected option
      int ReturnOptionValue(){
      int optionSetting = optionValue[Selected_Option];
      return optionSetting;
      }
    // Function to increase Selected option value
      int Selected_Option_Inc(){
      optionValue[Selected_Option] = optionValue[Selected_Option]+1;
      return optionValue[Selected_Option];
      }
    // Function to decrease Selected option value
      int Selected_Option_Dec(){
      optionValue[Selected_Option] = optionValue[Selected_Option]-1;
      return optionValue[Selected_Option];
      }

void loop(){

  // Get the current time
  	long int Current_Time = millis();

  // Get average water pressure.
    total = total - readings[readIndex]; // subtract the last water pressure reading:
    readings[readIndex] = map(analogRead(Water_P_Sensor), 0, 1023, 5, 150); // read from the sensor
    total = total + readings[readIndex]; // add the reading to the total
    readIndex = readIndex + 1; // advance to the next position in the array
    // if we're at the end of the array...
    if (readIndex >= numReadings) {
      // ...wrap around to the beginning:
      readIndex = 0;
    }
    // calculate the average:
      Water_P = total / numReadings;
  
  // Set Pushbutton debounce
    Enter_PB.setDebounceInterval(Debounce_Dur);
    Up_PB.setDebounceInterval(Debounce_Dur);
    Dn_PB.setDebounceInterval(Debounce_Dur);
    Set_PB.setDebounceInterval(Debounce_Dur);

  // Nothing is pressed
    if(!Enter_PB.pushed() && !Up_PB.pushed() && !Dn_PB.pushed() && !Set_PB.pushed()){
      Enter_PB_PState = LOW;
      Up_PB_PState = LOW;
      Dn_PB_PState = LOW;
      Set_PB_PState = LOW;
    }

    if(Enter_PB.pushed() || Up_PB.pushed() || Dn_PB.pushed() || Set_PB.pushed()){
      
      No_Press_Last_ms = Current_Time; // Update Time stamp of last time that no pushbuttons were pressed.

    }

  // Enter_PB Logic
    if(!Enter_PB.pushed()){
      Enter_PB_Not_Pressed_Last_ms = Current_Time;
    }
    if((Current_Time - Enter_PB_Not_Pressed_Last_ms) > Edit_Mode_Delay_ms || Edit_Mode){ // If Enter_PB is pressed for Edit_Mode_Delay_ms then enter edit mode.

      if(Enter_PB.pushed() && Enter_PB_PState == LOW && Set_PB.pushed() != true) { // Index through options when Enter_PB is pressed.
        if(Edit_Mode == false){
          Edit_Mode = true;
          Serial.println(F("Edit active"));
          
        }else if (Edit_Mode == true && Selected_Option < (optionValue_len-1)){
          Selected_Option = Selected_Option + 1;

        }else if (Edit_Mode == true && Selected_Option >= (optionValue_len-1)){
          Selected_Option = 0;
          }
          
        Print_Screen = true; // Update Display 
        Enter_PB_PState = Enter_PB.pushed(); // This is so it only executes if released and pressed again.
      }
    }

  // Up_PB logic
    if(Up_PB.pushed() && Up_PB_PState == LOW){
      
      if(Edit_Mode){
        // Print the menu
        Print_Screen = true;
        // increase the selected option by 1.
        int Inc = Selected_Option_Inc();
        if(Inc){
          Print_Screen = true;
        }else{
          Serial.print(F("try again"));
        }  
      }
      // Increase value while button is pressed or Toggle state to only Increase if released and pressed again
      Up_PB_PState = Up_PB.pushed();
    }
    
  // Dn_PB logic
    if(Dn_PB.pushed() && Dn_PB_PState == LOW){
      
      if(Edit_Mode){
        // Print the menu
        Print_Screen = true;
        // decrease the selected option by 1.
        int Dec = Selected_Option_Dec();
        if(Dec){
          Print_Screen = true;
        }else{
          Serial.print(F("try again"));
        }
      }
      // Increase value while button is pressed or Toggle state to only Increase if released and pressed again
      Dn_PB_PState = Dn_PB.pushed();
    }

  // Set_PB logic. Exit Edit mode.
    if(Set_PB.pushed() && Set_PB_PState == LOW && Edit_Mode || (Current_Time-No_Press_Last_ms)>No_Press_Return_Home_ms && Edit_Mode){
      // Save to option value to EEPROM
        EEPROM.put(Selected_Option*2,Selected_Option[optionValue]);
        if(Selected_Option == 0){
          Pump_Protect_Limit = optionValue[Selected_Option];
        }
        if(Selected_Option == 1){
          P_Lo_Limit = optionValue[Selected_Option];
        }
        if(Selected_Option == 2){
          P_Hi_Limit = optionValue[Selected_Option];
        }
        if(Selected_Option == 3){
          Enable_Serial_Print = optionValue[Selected_Option];
        }
      // Serial Print option value
        Serial.print(F("EEPROM.write : "));
        Serial.print(Selected_Option*2);
        Serial.print(F(" : "));
        char *optionActive = ReturnOptionSelected();
        Serial.print(optionActive);
        Serial.print(F(" : "));
        Serial.println(Selected_Option[optionValue]);
      // Exit the menu
        Edit_Mode = false;
        Exiting_Edit_Mode = true;
        Exiting_Edit_Mode_last_ms = Current_Time;
        Serial.println(F("Menu exited"));
        Set_PB_PState = Set_PB.pushed(); // Toggle state so menu only exits once
    }

  // Pressure Switch
    // Option value Limits
      // P_Hi_Limit
        if(Print_Screen){
          Pump_Protect_Limit = optionValue[0];
          P_Lo_Limit = optionValue[1];
          P_Hi_Limit = optionValue[2];
          }
      
        if(P_Hi_Limit < P_Lo_Limit){
          P_Hi_Limit = P_Lo_Limit + P_Hyst;
          optionValue[2] = P_Hi_Limit;
        }
        if(Selected_Option == 2 && (Selected_Option_Inc or Selected_Option_Dec)){
          if(P_Hi_Limit < P_Lo_Limit + P_Hyst){
            P_Hi_Limit = 70;
            optionValue[2] = P_Hi_Limit;
          }
        }
        if(P_Hi_Limit > 70){
          P_Hi_Limit = P_Lo_Limit + P_Hyst;
          optionValue[2] = P_Hi_Limit;
          
        }

      // P_Lo_Limit
        if(Selected_Option == 1 && (Selected_Option_Inc or Selected_Option_Dec)){
          if(P_Lo_Limit < Pump_Protect_Limit + 10){
            P_Lo_Limit = P_Hi_Limit - P_Hyst;
            optionValue[1] = P_Lo_Limit;
          }
        }
        if(P_Lo_Limit > (P_Hi_Limit - P_Hyst)){
          P_Lo_Limit = Pump_Protect_Limit + 10;
          optionValue[1] = P_Lo_Limit;
        }
        if(P_Lo_Limit > (70- P_Hyst)){
          P_Lo_Limit = 70- P_Hyst;
          optionValue[1] = P_Lo_Limit;
        }

      // Pump_Protect_Limit
        if(Pump_Protect_Limit < 0){
          Pump_Protect_Limit = P_Lo_Limit - 10;
          optionValue[0] = Pump_Protect_Limit;
        }
        if(Pump_Protect_Limit > P_Lo_Limit - 10){
          Pump_Protect_Limit = 0;
          optionValue[0] = Pump_Protect_Limit;
        }
        if(Pump_Protect_Limit > 30){
          Pump_Protect_Limit = 0;
          optionValue[0] = Pump_Protect_Limit;
        } 
      
  // Pump Status

      if(digitalRead(Water_Pump_Run)){
        Pump_Status = 1;
      }else{
        Pump_Status = 2;
      }
      
      if(Pump_Protect == true){
        Pump_Status = 4;
      }

  // Water Pump Run Logic
      if(digitalRead(Water_Pump_Run)){
        Pump_Run_Last_ms = Current_Time;
      }
      if (Water_P <= P_Lo_Limit && Water_P > Pump_Protect_Limit && !Pump_Protect && (Current_Time - Pump_Run_Last_ms > Pump_Run_Interval_ms)){
        // Turn ON Water_Pump_Run
        digitalWrite(Water_Pump_Run, HIGH);

      }else if (Water_P >= P_Hi_Limit) {
        // Turn off Water_Pump_Run
        digitalWrite(Water_Pump_Run, LOW);
      }
      
      if (Water_P < P_Lo_Limit && Enter_PB.pushed() && Set_PB.pushed()) {
        digitalWrite(Water_Pump_Run, HIGH);
        Pump_Status = 3;
        Pump_Protect = false;

      }else if(Water_P < Pump_Protect_Limit && Current_Time > Pump_Protect_Delay_On_Start_ms) {
        digitalWrite(Water_Pump_Run, LOW);
        Pump_Status = 4;
        Pump_Protect = true;
      }
  
  // Oled Display
    display.clearDisplay(); // Clear the display Before trying to print
    display.setTextColor(WHITE); // Set the color - always use white despite actual display color

    // Update the display while in Edit mode.
      if(Edit_Mode && Print_Screen){
        char *optionActive = ReturnOptionSelected(); // Get Selected option
        display.setTextSize(2); // Set the font size
        display.setCursor(0,0); // Set the cursor coordinates
        display.print(optionActive);
        display.setCursor(0,20); // Set the cursor coordinates
        display.print(optionValue[Selected_Option]);
        display.display();
        Print_Screen = false;
      }
    // Update the display while Exiting Edit mode.
      if (Exiting_Edit_Mode){
        if((Current_Time - Exiting_Edit_Mode_last_ms)>Exiting_Edit_Mode_ms){
          Exiting_Edit_Mode = false;
        }

        display.setTextSize(2); // Set the font size
        display.setCursor(0,0); // Set location of text
        display.print(F("Menu Exit"));
        display.display();
      }
    // Update the display while not Edit mode (Home screen).
      if(!Edit_Mode && !Exiting_Edit_Mode && (Current_Time-Update_Home_Last_ms)>Update_Home_ms){
        display.clearDisplay(); // Clear the display Before trying to print
        display.setTextSize(2); // Set the font size
        display.setCursor(0,0); // Set the cursor coordinates
        if(Pump_Status == 1){
          display.print(F("On"));
        }
        if(Pump_Status == 2){
          display.print(F("Off"));
        }
        if(Pump_Status == 3){
          display.print(F("Priming"));
        }
        if(Pump_Status == 4){
          display.print(F("Protect"));
        }
        display.setTextSize(3); // Set the font size
        display.setCursor(0,20); // Set the cursor coordinates
        display.print(Water_P);
        display.print(F(" PSI"));
        display.display();
        Update_Home_Last_ms = Current_Time;

        }

  // Debug
    if((Current_Time - Print_Serial_Last_ms)>Print_Serial_Interval){
      Serial.print(Pump_Protect_Limit);
      Serial.print(F(" : "));
      Serial.print(optionValue[0]);
      Serial.print(F(" : "));
      Serial.print(P_Lo_Limit);
      Serial.print(F(" : "));
      Serial.print(optionValue[1]);
      Serial.print(F(" : "));
      Serial.print(P_Hi_Limit);
      Serial.print(F(" : "));
      Serial.print(optionValue[2]);
      Serial.print(F(" : "));
      //Serial.print(Pump_Run_Last_ms);
      //Serial.println();
      if(No_Press_Last_ms != Current_Time){
        Serial.print(F(" No press"));
      }
      /*if(Edit_Mode){
        Serial.print(F(" Edit Active"));
      }
      Serial.print(F(" exitT "));
      Serial.print(Current_Time-No_Press_Last_ms);*/

      Serial.println();
      Print_Serial_Last_ms = Current_Time;
    }
  }
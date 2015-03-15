#include <LiquidCrystal.h>  // binde LCd-Bibliothek ein
#include <Time.h> // binde Bibliothek Time ein
#include "Timer.h"
#include <SoftwareSerial.h>
#include <DFPlayer_Mini_Mp3.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);

// timer for display update
Timer displayTimer;

//rotary encoder stuff
const int encoderPin1 = 2;
const int encoderPin2 = 3;
const int encoderSwitchPin = 4; //push button switch
volatile int lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;

//push button switching the alarm
const int alarmPin = 13;  


//global settings state, the value is incremented with each rotary push
int SETTINGS_MODE = 0;

//setup variables
int alarmEnabled = 0;
int alarmHour = 0;
int alarmMinutes = 0;

int timeHour = 0;
int timeMinutes = 0;

int dateDay = 1;
int dateMonth = 1;
int dateYear = 15;

int SETTINGS_DEFAULTS[] = {alarmHour, alarmMinutes, 0, timeHour, timeMinutes, dateDay, dateMonth, dateYear};

//mp3 player state
int playing = 0;
int volume = 5;

void setup() 
{
  Serial.begin(9600); 
  mp3_set_serial (Serial);    //set Serial for DFPlayer-mini mp3 module 
  mp3_set_volume (volume); 
  
  setTime(timeHour,timeMinutes,0,dateDay,dateMonth,dateYear); 
  
  //rotary encoder
  pinMode(encoderPin1, INPUT); 
  pinMode(encoderPin2, INPUT);
  pinMode(encoderSwitchPin, INPUT);//the push button of the rotary encoder
  
  //alarm on/off push button
  pinMode(alarmPin, INPUT);     

  digitalWrite(encoderPin1, HIGH); //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH); //turn pullup resistor on
  digitalWrite(encoderSwitchPin, HIGH); //turn pullup resistor on

  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3) 
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);
  
  //timer init
  displayTimer.every(1000, refreshUI);
  
  //lcd
  lcd.begin(20, 4); // Display hat 16 Zeichen x 2 Zeilen,
 
  //set initial alarm state 
  lcd.setCursor(0, 0); 
  lcd.print("Weckzeit:      ");
  printNumber(alarmHour); 
  lcd.print(":"); 
  printNumber(alarmMinutes);
   
  lcd.setCursor(0, 1); 
  lcd.print("Wecker an:       [ ]");
}

void loop()
{     
  if(playing == 0) {
    //set initial delay, waiting for the dfplayer to be ready
    delay(3000);
    mp3_playback_mode(0);
    mp3_play ();   
    playing = 1;
  }
  
  //check if alarm should be triggered
  checkAlarm();
  
  //check if the volume level should be changed
  checkVolume();
  
  //check if the settings button is pressed
  checkSettingsSwitch();  
  
  //check if the alarm button is pressed
  checkAlarmButton();
  
  //check if settigns are applied
  if(SETTINGS_MODE > 0) {
    checkSettingsMode();
  }

  //disable the timer if the time is set
  if(SETTINGS_MODE < 4) {
    displayTimer.update();
  }
}

/**
 * Checks if the rotary value should 
 * be applied as current volume value.
 */
void checkVolume() {
  if(playing == 1 && SETTINGS_MODE <= 0) {
    //reset rotary encoder value
    if(encoderValue < 0 || encoderValue > 30) {
      encoderValue = volume; //apply initial volume
    }    
    
    //apply only changed volume
    if(encoderValue != volume) {
      volume = encoderValue;
      mp3_set_volume(volume);
      Serial.print("Volume: ");
      Serial.println(volume);
    }    
  }
}

/**
 * Checks if the alarm is enabled and the alarm time
 * matches the current time
 */
void checkAlarm() {
  if(alarmEnabled == 1) {
  }
}

/**
 * Reads the rotary encoder button to
 * switch between the editing modes. 
 */
void checkSettingsSwitch() {
  if(!digitalRead(encoderSwitchPin)){
    lcd.cursor();
    encoderValue = SETTINGS_DEFAULTS[SETTINGS_MODE];
    SETTINGS_MODE++;
    //swtich back to regular mode
    if(SETTINGS_MODE > 8) {
      SETTINGS_MODE = 0;
      encoderValue = volume;
      lcd.noCursor();
    }
    delay(300); //debounce rotary push button
  }    
}

/**
 * Called when the clock is in settings mode.
 * Each SETTINGS_MODE represents a value on the display that 
 * should be updated.
 */
void checkSettingsMode() {
  if(SETTINGS_MODE == 1) {
    updateValue(15, 0, 0, 23, alarmHour, 0);
  }
  else if(SETTINGS_MODE == 2) {
    updateValue(18, 0, 0, 59, alarmMinutes, 0);
  }
  else if(SETTINGS_MODE == 3) {
    updateAlarm(encoderValue % 2);
  }
  else if(SETTINGS_MODE == 4) {
    updateValue(12, 2, 0, 23, timeHour, 1);
  }
  else if(SETTINGS_MODE == 5) {
    updateValue(15, 2, 0, 59, timeMinutes, 1);
  }
  else if(SETTINGS_MODE == 6) {
    updateValue(10, 3, 1, 31, dateDay, 1);
  }
  else if(SETTINGS_MODE == 7) {
    updateValue(13, 3, 1, 12, dateMonth, 1);
  }  
  else if(SETTINGS_MODE == 8) {
    updateValue(16, 3, 2015, 2100, dateYear, 1);
  }  
}

void checkAlarmButton() {
    // read the state of the pushbutton value:
  int buttonState = digitalRead(alarmPin);
  if (buttonState == HIGH) {    
    delay(200);
    buttonState = digitalRead(alarmPin);
    if (buttonState == HIGH) {    
      alarmEnabled = alarmEnabled == 0 ? 1 : 0;
      updateAlarm(alarmEnabled);
    }
  }
}

/**
 * Updates the UI of the alarm, boolean setting
 */
void updateAlarm(int enable) {
  lcd.setCursor(18, 1);   
  if(enable != 0) {
    alarmEnabled = 1;
    lcd.print("x");
  }
  else {
    alarmEnabled = 0;
    lcd.print(" ");
  }
  lcd.setCursor(18, 1);   
}

/**
 * Generic time and alarm update method
 */
void updateValue(int col, int row, int minValue, int maxValue, int &value, int updateTime) {
  lcd.setCursor(col, row); 
  if(encoderValue < minValue) {
    encoderValue = minValue;    
  }
  if(encoderValue > maxValue) {
    encoderValue = maxValue;
  }
  
  if(value != encoderValue) {
    value = encoderValue;
    printNumber(encoderValue);
    SETTINGS_DEFAULTS[SETTINGS_MODE-1] = encoderValue;
  }

  if(updateTime == 1) {
    setTime(timeHour,timeMinutes,0,dateDay,dateMonth,dateYear);
  }
}

/**
 * The method is called by the time timer to update the UI
 */
void refreshUI() // definieren Subroutine
{
   lcd.setCursor(0, 2);
   lcd.print("Uhrzeit:    ");
   printNumber(hour()); 
   lcd.print(":"); 
   printNumber(minute());
   lcd.print(":"); 
   printNumber(second());

   lcd.setCursor(0, 3); 
   lcd.print("Datum:    ");
   printNumber(day()); 
   lcd.print("."); 
   printNumber(month());
   lcd.print("."); 
   printNumber(year());
}

/**
 * Number formatting to 2 digits
 */
void printNumber(int number) 
{
  if(number < 10) 
   {
     lcd.print("0");  
   }
   lcd.print(number); 
}

/**
 * The rotary encoder implementation
 */
void updateEncoder(){
  int MSB = digitalRead(encoderPin1); //MSB = most significant bit
  int LSB = digitalRead(encoderPin2); //LSB = least significant bit

  int encoded = (MSB << 1) |LSB; //converting the 2 pin value to single number
  int sum  = (lastEncoded << 2) | encoded; //adding it to the previous encoded value
  
  //if(sum == 13 || sum == 4 || sum == 2 || sum == 11) encoderValue ++;
  if(sum == 2) encoderValue --;//skip updates
  //if(sum == 14 || sum == 7 || sum == 1 || sum == 8 ) encoderValue --;
  if(sum == 1) encoderValue ++;//skip updates

  lastEncoded = encoded; //store this value for next time
}          

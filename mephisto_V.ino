#include <LiquidCrystal.h>  // binde LCd-Bibliothek ein
#include <Time.h> // binde Bibliothek Time ein
#include "Timer.h"
#include <SoftwareSerial.h>
#include <DFPlayer_Mini_Mp3.h>

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(7, 8, 9, 10, 11, 12);
#define LCD_LIGHT_PIN A4

const int TRACK_COUNT = 50;

const int PUSH_BUTTON_DEBOUNCE = 150;
const int ALARM_INCREASE_INTERVAL = 2500;

// timer for display update
Timer displayTimer;
Timer volumeTimer;

//rotary encoder stuff
const int encoderPin1 = 3;
const int encoderPin2 = 2;
const int encoderSwitchPin = 4; //push button switch
volatile int lastEncoded = 0;
volatile long encoderValue = 0;
long lastencoderValue = 0;
int lastMSB = 0;
int lastLSB = 0;

//push button switching the alarm
const int ALARM_PIN = 13;  


//global settings state, the value is incremented with each rotary push
int SETTINGS_MODE = 0;

//setup variables
int alarmEnabled = 0;
int alarmHour = 0;
int alarmMinutes = 1;

int timeHour = 0;
int timeMinutes = 0;
int timeSeconds = 0;

int SETTINGS_DEFAULTS[] = {alarmHour, alarmMinutes, 0, timeHour, timeMinutes};

//mp3 player state
int alarmRunning = 0;
int volume = 5;

//lcd display switch
const int LCD_PIN = 5;
int lcdEnabled = 1;

//play button
const int PLAY_PIN = 6;
int playing = 0;

void setup() 
{
  Serial.begin(9600); 
  mp3_set_serial (Serial);    //set Serial for DFPlayer-mini mp3 module 
  
  //random seed
  randomSeed(analogRead(0));
  
  setTime(timeHour,timeMinutes,0,0,0,0); 
  
  //rotary encoder
  pinMode(encoderPin2, INPUT); 
  pinMode(encoderPin1, INPUT);
  pinMode(encoderSwitchPin, INPUT);//the push button of the rotary encoder
  
  //alarm on/off push button
  pinMode(ALARM_PIN, INPUT);     
  
  //lcd toggle button
  pinMode(LCD_PIN, INPUT);
  
  //play button
  pinMode(PLAY_PIN, INPUT);

  digitalWrite(encoderPin1, HIGH); //turn pullup resistor on
  digitalWrite(encoderPin2, HIGH); //turn pullup resistor on
  digitalWrite(encoderSwitchPin, HIGH); //turn pullup resistor on

  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3) 
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);
  
  // Set the LCD display backlight pin as an output.
  pinMode(LCD_LIGHT_PIN, OUTPUT);
  // Turn on the LCD backlight.
  digitalWrite(LCD_LIGHT_PIN, HIGH);
  
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
  
  encoderValue = volume;
  
  setVolume(volume); 
}

void loop()
{     
  //check if alarm should be triggered
  checkAlarm();
  
  //check if the volume level should be changed
  checkVolume();
  
  //check if the settings button is pressed
  checkSettingsSwitch();  
  
  //check if the alarm button is pressed
  checkAlarmButton();
  
  //check if the LCD  button is pressed
  checkLcdButton();
  
  //check if the play button is pressed
  checkPlayButton();
  
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
  if(SETTINGS_MODE <= 0) {
    //reset rotary encoder value
    if(encoderValue < 0 || encoderValue > 30) {
      encoderValue = volume; //apply initial volume
    }    
    
    //apply only changed volume
    if(encoderValue != volume) {
      Serial.println("Stopping volume timer");
      Serial.println(encoderValue);
      Serial.println(volume);      
      volumeTimer.stop(0);
      setVolume(encoderValue);
    }    
  }
}

/**
 * Checks if the alarm is enabled and the alarm time
 * matches the current time
 */
void checkAlarm() {
  if(alarmEnabled == 1 && alarmRunning == 0 && playing == 0) {
    if(alarmHour == timeHour && alarmMinutes == timeMinutes && timeSeconds == 0) {
      digitalWrite(LCD_LIGHT_PIN, 1);
      alarmRunning = 1;
      playing = 1;
      volume = 1;
      encoderValue = 1;
      mp3_set_volume(volume);
      delay(1000);
      playNext();
      volumeTimer.every(ALARM_INCREASE_INTERVAL, increaseVolume);
    }
  }
  
  if(alarmRunning) {
    volumeTimer.update();
  }
}

/**
 * Increases the volume by the timer fired every n seconds.
 */ 
void increaseVolume() {
  //stop at the volume level 15
  if(volume == 15) {
    Serial.println("Stopping timer");
    volumeTimer.stop(0);
  }
  else {
    volume++;
    encoderValue = volume;
    setVolume(volume); 
  }
}

/**
 * Reads the rotary encoder button to
 * switch between the editing modes. 
 */
void checkSettingsSwitch() {
  if(digitalRead(encoderSwitchPin)){
    lcd.cursor();
    encoderValue = SETTINGS_DEFAULTS[SETTINGS_MODE];
    SETTINGS_MODE++;
    //swtich back to regular mode
    if(SETTINGS_MODE > 5) {
      SETTINGS_MODE = 0;
      encoderValue = volume;
      lcd.noCursor();
    }
    delay(PUSH_BUTTON_DEBOUNCE); //debounce rotary push button
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
}

/**
 * Stops the playback.
 */
void stopPlaying() {
  digitalWrite(LCD_LIGHT_PIN, 0);
  alarmRunning = 0;
  playing = 0;
  mp3_stop();
}

/**
 * Shuffle play
 */
void playNext() {
  playing = 1;
  // print a random number from 10 to 19
  int randNumber = random(1, TRACK_COUNT+1);
  mp3_play (randNumber); 
}

/**
 * Handler for the LCD toggle button
 */
void checkLcdButton() {
    // read the state of the pushbutton value:
  int buttonState = digitalRead(LCD_PIN);
  if (buttonState == HIGH) {    
    delay(PUSH_BUTTON_DEBOUNCE);
    buttonState = digitalRead(LCD_PIN);
    if (buttonState == HIGH) {    
      if(playing) {
        stopPlaying();
        return;
      }
      
      lcdEnabled = lcdEnabled == 0 ? 1 : 0;
      //toggle display light
      digitalWrite(LCD_LIGHT_PIN, lcdEnabled);
    }
  }
}

/**
 * Handler for the Alarm button
 */
void checkAlarmButton() {
    // read the state of the pushbutton value:
  int buttonState = digitalRead(ALARM_PIN);
  if (buttonState == HIGH) {    
    delay(PUSH_BUTTON_DEBOUNCE);
    buttonState = digitalRead(ALARM_PIN);
    if (buttonState == HIGH) {
      if(playing) {
        stopPlaying();
        return;
      }
      
      alarmEnabled = alarmEnabled == 0 ? 1 : 0;
      updateAlarm(alarmEnabled);
    }
  }
}

/**
 * Handler for the Alarm button
 */
void checkPlayButton() {
    // read the state of the pushbutton value:
  int buttonState = digitalRead(PLAY_PIN);
  if (buttonState == HIGH) {    
    delay(PUSH_BUTTON_DEBOUNCE);
    buttonState = digitalRead(PLAY_PIN);
    if (buttonState == HIGH) {
      playNext();
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
    setTime(timeHour,timeMinutes,second(),0, 0, 0);
  }
}

/**
 * The method is called by the time timer to update the UI
 */
void refreshUI() // definieren Subroutine
{
   lcd.setCursor(0, 2);
   lcd.print("Uhrzeit:    ");
   timeHour = hour();
   printNumber(hour()); 
   lcd.print(":"); 
   timeMinutes = minute();
   printNumber(minute());
   lcd.print(":"); 
   timeSeconds = second();
   printNumber(timeSeconds);

   refreshVolume();
}

/**
 * Only updates the volume section
 */
void refreshVolume() {
  lcd.setCursor(0, 3); 
  lcd.print("Volume: [");
  int blocks = volume/3;
  for(int i=1; i<=10; i++) {
    if(i <= blocks) {
      lcd.print(char(255));
    }
    else {
      lcd.print(" ");
    }
  }
  lcd.print("]");
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
 * Applies the volume to the mp3 player and updates the display.
 */
void setVolume(int vol) {
  Serial.print("Volume: ");
  Serial.println(volume);
  volume = vol;
  mp3_set_volume(vol);
  refreshVolume();
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

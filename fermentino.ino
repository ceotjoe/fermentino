/*
Fermentino
Arduino Sketch zur Steuerung einer Gärbox für Sauerteiggärung
(C) Jörg Holzapfel 2013

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Dieses Programm ist Freie Software: Sie können es unter den Bedingungen
der GNU General Public License, wie von der Free Software Foundation,
Version 3 der Lizenz oder (nach Ihrer Option) jeder späteren
veröffentlichten Version, weiterverbreiten und/oder modifizieren.

Dieses Programm wird in der Hoffnung, dass es nützlich sein wird, aber
OHNE JEDE GEWÄHRLEISTUNG, bereitgestellt; sogar ohne die implizite
Gewährleistung der MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
Siehe die GNU General Public License für weitere Details.

Sie sollten eine Kopie der GNU General Public License zusammen mit diesem
Programm erhalten haben. Wenn nicht, siehe <http://www.gnu.org/licenses/>.
*/

#include <Time.h>
#include <LiquidCrystal.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEBUG 1

#define TIME_MSG_LEN  11   // time sync to PC is HEADER followed by Unix time_t as ten ASCII digits
#define TIME_HEADER  'T'   // Header tag for serial time sync message
#define TIME_REQUEST  7    // ASCII bell character requests a time sync message 

//Pin setup LCD display
#define RS 12
#define E 11
#define D4 5
#define D5 4
#define D6 3
#define D7 2
#define COLS 16
#define ROWS 2
LiquidCrystal lcd(RS, E, D4, D5, D6, D7);

//Pin setup other pins
#define sensorPin 0
#define upbuttonPin 10
#define okbuttonPin 9
#define downbuttonPin 8
#define heatingPin 6
#define piezoPin 13
#define toneDuration 500
#define tonePause 800
#define DELAY 10

#define UP 1
#define OK 2
#define DOWN 3
#define NULL 0

#define MINUTE 60;
#define HOUR 60 * MINUTE;
#define DAY 24 * HOUR;

#define TEMPDELAY 10 //all 10 seconds getTemp

//Dallas Temperature
// Data wire is plugged into port 7 on the Arduino
#define ONE_WIRE_BUS 7
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);
// arrays to hold device address
DeviceAddress insideThermometer;

//Variables
const String Version = "Fermentino V0.1";
const String Author = "by J. Holzapfel";
boolean serialProtocoll = false;
const int cycles = 20;
const int minTemperature = 21;
const int maxTemperature = 40;
int targetTemperature = minTemperature;
int intresultTemp;
float resultTemp;
int heatingStatus;
int timerStatus;
time_t targetTime = 0;
time_t serialDelay = 0;
time_t getTempDelay = 0;

//operating modes
int operatingMode = 0; // 0 = Leerlauf; 1 = manuelle Temperatur; 2 = program
//String operatingModeText [3] = {"STBY", "MATP", "PROG"};

//gaerbox Sauerteig Programme
const int maxPrograms = 4;
String programName [10] = {
  "Aufheizen","Klassisch3St","Detmolder3St","Detmolder1St","Poelt3St"};
String programShortName [10] = {
  "Aufh","Kl3St","De3St","De1St","Po3St"};
int programSteps [10] = {
  1,3,3,1,3};
//int programDuration [10] [3] = {{40 * 60}, {8 * 3600, 8 * 3600, 4 * 3600}, {8 * 3600, 8 * 3600, 4 * 3600}, {4 * 3600}, {8 * 3600, 8 * 3600, 4 * 3600}};
int programDuration [10] [3] = {
  {
    14  }
  , {
    8, 8, 4  }
  , {
    10, 5, 3  }
  , {
    15  }
  , {
    11, 6, 4  }
};
int programTemperatureHigh [10] [3] = {
  {
    30  }
  , {
    30,26,22  }
  , {
    30,26,22  }
  , {
    30,26,22  }
  , {
    28  }
  , {
    30,26,22  }
};
int programTemperatureLOW [10] [3] =  {
  {
    30  }
  , {
    30,26,22  }
  , {
    30,26,22  }
  , {
    30,26,22  }
  , {
    28  }
  , {
    30,26,22  }
};
int programSelected = 999; //start with program 999 stby

void setup(){
  noTone(piezoPin);
  pinMode(upbuttonPin, INPUT);
  pinMode(downbuttonPin, INPUT);
  pinMode(okbuttonPin, INPUT);
  pinMode(heatingPin, OUTPUT);
  timerStatus = LOW;
  Serial.begin(9600);
  lcd.begin(COLS,ROWS);
  lcd.print(Version);
  lcd.setCursor(0,1);
  lcd.print(Author);
  delay(3000);
  lcd.clear();
  getTempDelay = now();
  sensors.begin();
  while (!sensors.getAddress(insideThermometer, 0)) {
    Serial.println("Unable to find address for Device 0");
    lcd.print("ERROR!!");
    lcd.setCursor(0, 1);
    lcd.print("No Sensor found!");
  }
  sensors.setResolution(insideThermometer, 9);
  Serial.println(Version);
  Serial.println(Author);
}

void loop(){

  //measure temperature
  if (now()>=getTempDelay) 
    resultTemp = getTempDS();

  switch (checkButtons()) {
  case UP:
    if (targetTemperature < maxTemperature)
      targetTemperature++;
    break;
  case DOWN:
    if (targetTemperature > minTemperature)
      targetTemperature--;
    break;
  case OK:
    selectProgram();
    break;
  }

  checkSerial();

  if (programSelected!=999){
    workProgram(programSelected);
    programSelected = 999; //switch to standby after program run
  }

  heatingStatus = switchHeating(resultTemp, targetTemperature);
  updateDisplay((int) (resultTemp+.5), targetTemperature, heatingStatus, targetTime, timerStatus, "Stby");

}

void workProgram(int programSelected){
  int currStep;

  for (currStep = 0; currStep <= programSteps [programSelected] - 1; currStep++) {

    targetTime = now() + programDuration [programSelected] [currStep];
    targetTemperature = programTemperatureHigh [programSelected] [currStep];
    timerStatus = HIGH;

    do
    {
      //measure temperature
      if (now()>=getTempDelay) 
        resultTemp = getTempDS();

      switch (checkButtons()) {
      case UP:
        if (targetTemperature < maxTemperature)
          targetTemperature++;
        break;
      case DOWN:
        if (targetTemperature > minTemperature)
          targetTemperature--;
        break;
      case OK:
        targetTime = now();
        currStep = programSteps [programSelected];
        break;
      }

      checkSerial();
      heatingStatus = switchHeating(resultTemp, targetTemperature);
      updateDisplay((int) (resultTemp+.5), targetTemperature, heatingStatus, targetTime, timerStatus, programShortName [programSelected]); 

    } 
    while (targetTime>now());

  }

  //return to Standby
  playSound();
  targetTemperature=minTemperature;
  timerStatus = LOW;
}

float getTempDS(){
  sensors.requestTemperatures();
  getTempDelay = now() + TEMPDELAY;
  return sensors.getTempC(insideThermometer);
}


//get temperature from analog pin 0
float getTemp(){

  float resultTemp = 0.0;
  for(int i = 0; i < cycles; i++){
    int analogValue = analogRead(sensorPin);
    float temperature = (5.0 * 100.0 * analogValue) / 1024;
    resultTemp += temperature;
    delay(DELAY);
  }
  //average temp
  resultTemp /= cycles;
  getTempDelay = now() + TEMPDELAY;
  return resultTemp;
}

//switch heating on and off depending on targetTemperature
int switchHeating(float resultTemp, int targetTemperature){

  if(resultTemp < targetTemperature){
    digitalWrite(heatingPin, HIGH);
    lcd.setCursor(15, 0);
    lcd.print("H");
    return HIGH;
  }
  else
  {
    digitalWrite(heatingPin, LOW);
    lcd.setCursor(15, 0);
    lcd.print(" ");
    return LOW;
  }

} 

//check command buttons
int checkButtons(){

  int upbuttonStatus;
  int downbuttonStatus;
  int okbuttonStatus;

  delay(150);

  upbuttonStatus = digitalRead(upbuttonPin);
  if(upbuttonStatus == HIGH)
    return UP;

  downbuttonStatus = digitalRead(downbuttonPin);
  if(downbuttonStatus == HIGH)
    return DOWN;

  okbuttonStatus = digitalRead(okbuttonPin);
  if(okbuttonStatus == HIGH)
    return OK;

  return NULL;

}

//update LCD display and serial output
void updateDisplay(int intresultTemp, int targetTemperature, int heatingStatus, time_t targetTime, int timerStatus, String programName){
  time_t Timer;

  //Current temperature
  lcd.setCursor(0, 0);
  lcd.print("C:");
  lcd.print(intresultTemp);
#if ARDUINO < 100
  lcd.print(0xD0 + 15, BYTE);
#else
  lcd.write(0xD0 + 15);
#endif
  lcd.print("C ");

  //targetTemperature
  lcd.print("T:");
  lcd.print(targetTemperature);
#if ARDUINO < 100
  lcd.print(0xD0 + 15, BYTE);
#else
  lcd.write(0xD0 + 15);
#endif
  lcd.print("C");

  lcd.setCursor(15, 0);
  if (heatingStatus==HIGH)
    lcd.print("H");
  else
    lcd.print(" ");

  Timer = targetTime - now(); 

  lcd.setCursor(0, 1);
  lcd.print("T:");

  if (timerStatus==HIGH){
    if (hour(Timer)<10)
      lcd.print("0");
    lcd.print(hour(Timer));
    lcd.print(":");
    if (minute(Timer)<10)
      lcd.print("0");
    lcd.print(minute(Timer));
    lcd.print(":");
    if (second(Timer)<10)
      lcd.print("0");
    lcd.print(second(Timer));
  }
  else
  {
    lcd.print ("00:00:00");
  }

  lcd.print("  ");
  lcd.print(programName);

  //serial protocoll
  if ((serialProtocoll) && (now()>=serialDelay)) {
    serialDelay = now() + 10;
    Serial.print("T");
    Serial.print(now());
    Serial.print(",");
    Serial.print(resultTemp);
    Serial.print(",");
    Serial.print(targetTemperature);
    Serial.print(",");
    Serial.print(heatingStatus);
    Serial.print(",");
    Serial.println(programName);
  }
}

//select program

void selectProgram(){
  int buttonPressed;

  if (programSelected==999)
    programSelected=0;

  lcd.clear();
  lcd.print("Programmwahl:");
  lcd.setCursor(0,1);
  lcd.print(programName [programSelected]);

  do
  {
    delay(250);
    buttonPressed = checkButtons();

    switch (buttonPressed) {
    case UP:
      if (programSelected < maxPrograms){
        programSelected++;
        lcd.setCursor(0,1);
        lcd.print(programName [programSelected]);
      }
      break;
    case DOWN:
      if (programSelected>0){
        programSelected--;
        lcd.setCursor(0,1);
        lcd.print(programName [programSelected]);
      }
      break;
    case NULL:
      break;
    }


  } 
  while (buttonPressed!=OK);

  lcd.clear();

}

void checkSerial(){

  while (Serial.available()>0) {

    char inByte = Serial.read();

     switch (inByte) {

     case 'P':
       //programSelected = (int) Value;
       Serial.print("start program ");
       Serial.println(programSelected);
       break;
     case 't':
       Serial.print("T");
       Serial.print(now());
       Serial.print(",");
       Serial.print(resultTemp);
       Serial.print(",");
       Serial.print(targetTemperature);
       Serial.print(",");
       Serial.print(heatingStatus);
       Serial.print(",");
       Serial.print(programSelected + "\n");
       break;
     case 'S':
       Serial.println("TSREADY");
       delay(10000);
       processSyncMessage();
       Serial.print("time synced to ");
       Serial.print(day());
       Serial.print(".");
       Serial.print(month());
       Serial.print(".");
       Serial.print(year());
       Serial.print(" ");
       Serial.print(hour());
       Serial.print(":");
       Serial.print(minute());
       Serial.print(":"); 
       Serial.print(second());
       Serial.print("  "); 
       Serial.println(now());
       Serial.print("  "); 
       //Serial.println(Value);
       break;
     case 'p':
       if (serialProtocoll)
         serialProtocoll = false;
       else
         serialProtocoll = true;
       break;  
        
    }   
  }
} 

void processSyncMessage() {
  // if time sync available from serial port, update time and return true
  while(Serial.available() >=  TIME_MSG_LEN ){  // time message consists of header & 10 ASCII digits
    char c = Serial.read() ; 
    Serial.print(c);  
    if( c == TIME_HEADER ) {       
      time_t pctime = 0;
      for(int i=0; i < TIME_MSG_LEN -1; i++){   
        c = Serial.read();          
        if( c >= '0' && c <= '9'){   
          pctime = (10 * pctime) + (c - '0') ; // convert digits to a number    
        }
      }   
      setTime(pctime);   // Sync Arduino clock to the time received on the serial port
    }  
  }
}

void playSound(){
  int tones[] = {523, 659, 587, 698, 659, 784, 698, 880};
  int elements = sizeof(tones) / sizeof(tones[0]);
  
  for(int i = 0; i < elements; i++) {
    tone(piezoPin, tones[i], toneDuration);
    delay(tonePause);
  }
}







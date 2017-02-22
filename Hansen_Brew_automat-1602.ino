

// Get the LCD I2C Library here:
// https://bitbucket.org/fmalpartida/new-liquidcrystal/downloads
// Move any other LCD libraries to another folder or delete them
// See Library "Docs" folder for possible commands etc.
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h> // Used to store step mash programs
#include <TimerOne.h>

int Hours, Minutes, Seconds = 0;
int CountHours, CountMinutes, CountSeconds = 0;
unsigned long start, stop = millis();
int SetTemp;
int CountDown,TimedSeconds,LeftSeconds;

//Number of StepMashPrograms
int  const StepMashPrograms = 10;
int const MashSteps = 3;
int const SetupMenues = 6;
byte const NumLetters = 94; // offset 32 Z + 3
// byte const AllLetters = 93;  // Number of chars

struct MyMashObject {
  float MashTemp[MashSteps]; // Temperature
  byte MashTime[MashSteps];  // Minutes
  char name[16] = "AutoMash x";  // Guess what ;-)
};

struct MyMashObject MashProgram[StepMashPrograms]; //

float Temperature;
char OutString[16];
int RunMashProgramNumber;

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);


// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/*-----( Declare Constants )-----*/
//none
/*-----( Declare objects )-----*/
// set the LCD address to 0x20 for a 20 chars 4 line display
// Set the pins on the I2C chip used for LCD connections:
//                    addr, en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address


int Mainmenu = 0;

const byte ledPin = 13;
const byte interruptShaftPinA = 2;
const byte interruptShaftPinB = 3;
const byte encoderSwitchPin = 5;
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile byte encoderPos = 0; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte oldEncPos = 0; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent


void InitVars() {
  char charBuf[10];
  String output;
  // Construct empty contens;
  int eeAddress = 0;   //Location we want the data to be put.
  for (int MashProgramNumber = 0; MashProgramNumber < StepMashPrograms; MashProgramNumber++) {
    for (int MashStepNumber = 1; MashStepNumber < MashSteps; MashStepNumber++) {
      MashProgram[MashProgramNumber].MashTemp[MashStepNumber] = MashProgramNumber * 10 + MashStepNumber;
      MashProgram[MashProgramNumber].MashTime[MashStepNumber] = MashStepNumber * 10 + 1;
    }
    sprintf(MashProgram[MashProgramNumber].name, " Mash Program %i", MashProgramNumber);;
  }
  //  for (int MashProgramNumber = 0; MashProgramNumber < StepMashPrograms; MashProgramNumber++) Serial.println(MashProgram[MashProgramNumber].name);
  //  Serial.println("Init done");
}

void WritetoEEprom() {
  int eeAddress = 0;   //Location we want the data to be put.
  for (int MashProgramNumber = 0; MashProgramNumber < StepMashPrograms; MashProgramNumber++) {
    // Write each set to EEprom;
    //One simple call, with the address first and the object second.
    EEPROM.put(eeAddress, MashProgram[MashProgramNumber]);
    // Serial.println(MashProgram[MashProgramNumber].name);
    // get ready for next set
    eeAddress += sizeof(MyMashObject); //Move address to the next afterMyMashObject.
  }
  // Serial.println("Written to EEprom");
}


void ReadFromEEprom() {
  int eeAddress = 0;   //Location we want the data to be put.
  for (int MashProgramNumber = 0; MashProgramNumber < StepMashPrograms; MashProgramNumber++) {
    // Read each set from EEprom;
    //One simple call, with the address first and the object second.
    EEPROM.get(eeAddress, MashProgram[MashProgramNumber]);
    //   Serial.println(MashProgram[MashProgramNumber].name);
    // get ready for next set
    eeAddress += sizeof(MyMashObject); //Move address to the next afterMyMashObject.
  }
  //  for (int MashProgramNumber = 0; MashProgramNumber < StepMashPrograms; MashProgramNumber++) Serial.println(MashProgram[MashProgramNumber].name);
  //  Serial.println("Read done");
}


boolean CheckKeyPress() {
  if (digitalRead(encoderSwitchPin) != true) {
    // Key IS pressed
    delay(25);
    while (digitalRead(encoderSwitchPin) !=  true); // wait for key release
    return true;
  } else return false;
}

char selectChar(char letter, int i) {
  char Character;
  encoderPos = byte(letter) - 32;  // Offset = 32
  do
  {
    if (oldEncPos != encoderPos) {
      if (encoderPos >= 250) encoderPos = NumLetters; // Check for rollover down - Offset = 32
      if (encoderPos > NumLetters) encoderPos = 0; // Offset = 32
      oldEncPos = encoderPos;
      byte char1 = (encoderPos + NumLetters - 6) % NumLetters;
      lcd.setCursor(0, 0);
      for (int c = 0; c < 14; c++) {
        Character = char((c + char1 + NumLetters) % NumLetters + 32);
        if (c != 6) lcd.write(Character);
        else {
          lcd.write(" ");
          lcd.write(Character);
          lcd.write(" ");
        }
      }
    }
    lcd.setCursor(i, 1);
    lcd.blink();
  } while (CheckKeyPress() == false);
  Character = char(encoderPos + 32);
  return Character;
}

void SetupMenu() {
  int i, Menu;
  char streng[16];
  encoderPos = 3;
  do {
    do {
      if (oldEncPos != encoderPos) {
        lcd.setCursor(0, 1);
        Menu = encoderPos % SetupMenues;
        switch (Menu) {
          case 0 : lcd.print("Exit from Setup ");
            break;
          case 1 : lcd.print("AllFactory reset");
            break;
          case 2 : lcd.print("Edit progr. Name");
            break;
          case 3 : lcd.print("Edit Mash Steps ");
            break;
          case 4 : lcd.print("Save Mash progr.");
            break;
          case 5 : lcd.print("Load Mash progr.");
            break;
        }
        oldEncPos = encoderPos;
      }
    } while (CheckKeyPress() == false);
    switch (Menu) {
      case 0 : break; // Do nothing - exit
      case 1 :
        InitVars();  // Load array with defaults
        WritetoEEprom();
        ReadFromEEprom();
        break;
      case 2 :
        encoderPos = 0;
        lcd.clear();
        do { // Select the right StepMashProgram
          if (oldEncPos != encoderPos) {
            oldEncPos = encoderPos;
            RunMashProgramNumber = encoderPos % StepMashPrograms;
            lcd.setCursor(0, 1);
            lcd.print(MashProgram[RunMashProgramNumber].name);
          }
          if (encoderPos > StepMashPrograms)encoderPos = 0;
        } while (CheckKeyPress() == false);
        // Ready to edit [RunMashProgramNumber]
        for (i = 0; i < 16; i++) {
          // 16 Characters
          MashProgram[RunMashProgramNumber].name[i] = selectChar(MashProgram[RunMashProgramNumber].name[i], i);
          lcd.setCursor(0, 1);
          lcd.print(MashProgram[RunMashProgramNumber].name);
        } // All updated
        encoderPos = 4; // ready to save;
        break;
      case 3 :
        encoderPos = 0;
        lcd.clear();
        do { // Select the right StepMashProgram
          if (oldEncPos != encoderPos) {
            oldEncPos = encoderPos;
            RunMashProgramNumber = encoderPos % StepMashPrograms;
            lcd.setCursor(0, 0);
            lcd.print(MashProgram[RunMashProgramNumber].name);
          }
          if (encoderPos > StepMashPrograms)encoderPos = 0;
        } while (CheckKeyPress() == false);
        // Ready to edit [RunMashProgramNumber]
        for (i = 0; i < MashSteps; i++) {
          //  lcd.setCursor(0, 0);
          lcd.clear();
          lcd.print(MashProgram[RunMashProgramNumber].name);
          encoderPos = MashProgram[RunMashProgramNumber].MashTemp[i];
          do {
            if (encoderPos > 100) encoderPos = 100;
            sprintf(streng, "Step:%i Temp:%3i", i + 1, encoderPos);
            lcd.setCursor(0, 1);
            lcd.print(streng);
          } while (CheckKeyPress() == false);
          MashProgram[RunMashProgramNumber].MashTemp[i] = encoderPos;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(streng);
          encoderPos = MashProgram[RunMashProgramNumber].MashTime[i];
          do {
            if (encoderPos > 180) encoderPos = 180;
            sprintf(streng, " For %3i Minutes.", encoderPos);
            lcd.setCursor(0, 1);
            lcd.print(streng);
          } while (CheckKeyPress() == false);
          MashProgram[RunMashProgramNumber].MashTime[i] = encoderPos;
        }
        encoderPos = 4; // ready to save;
        oldEncPos = 0;
        break; // Do  exit
      case 4 :
        WritetoEEprom();
        encoderPos = 0;
        break;
      case 5 :
        ReadFromEEprom();
        break;
    }
    lcd.clear();
    lcd.noBlink();
  } while (Menu != 0);
}

void Thermostat() {
  lcd.print(SetTemp);
  if (Temperature < SetTemp) {
    //   digitalWrite(ledPin, HIGH);
    lcd.print(" * ");
  } else {
    //   digitalWrite(ledPin, LOW);
    lcd.print("   ");
  }
}

void RunStepMash(int RunMashProgramNumber) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(MashProgram[RunMashProgramNumber].name);
  lcd.setCursor(1, 1);
  lcd.print("Press to start");
  while (CheckKeyPress() ==  false);
  for (int i = 0; i < MashSteps; i++ ){ // Kør de 3 Programmer
      RunStep(MashProgram[RunMashProgramNumber].MashTime[i]*60, MashProgram[RunMashProgramNumber].MashTemp[i]);
  }
}

void PinA() {
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; // read all eight pin values then strip away all but pinA and pinB's values
  if (reading == B00001100 && aFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos --; //decrement the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00000100) bFlag = 1; //signal that we're expecting pinB to signal the transition to detent from free rotation
  sei(); //restart interrupts
}

void PinB() {
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; //read all eight pin values then strip away all but pinA and pinB's values
  if (reading == B00001100 && bFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos ++; //increment the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00001000) aFlag = 1; //signal that we're expecting pinA to signal the transition to detent from free rotation
  sei(); //restart interrupts
}

void TimeTick() {// Interrupt called every 1 second
  start = millis();
  sensors.requestTemperatures();
  if (Temperature < SetTemp) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }
  Temperature = sensors.getTempCByIndex(0);
  Seconds++;
  if (Seconds > 60) {
    Seconds = 0;
    Minutes++;
  }
  if (Minutes > 60) {
    Minutes = 0;
    Hours++;
  }
  if (Hours > 23) Hours = 0;
  stop = millis();
}

void RunStep(int Time, int Temp){
      Hours = 0;
      Minutes = 0;
      Seconds = 0;
      SetTemp = Temp;
      lcd.clear();
      do {
        lcd.setCursor(0,0);
        sprintf(OutString, "Set:%02i Temp:", Temp);
        lcd.print(OutString);
        dtostrf(Temperature, 4, 1, OutString);
        lcd.print(OutString);
        TimedSeconds = Hours*3600 + Minutes*60 + Seconds;
        LeftSeconds = Time - TimedSeconds;
        CountHours = int(LeftSeconds/3600);
        CountMinutes = (LeftSeconds/60) % 60;
        CountSeconds = LeftSeconds % 60;
        lcd.setCursor(1, 1); // 2. line
        sprintf(OutString, "Time: %i:%02i:%02i", CountHours,CountMinutes,CountSeconds);
        lcd.print(OutString);
      } while (CheckKeyPress() ==  false && LeftSeconds > 0 );
      SetTemp = 0;
}

void TimerThermostat(){
      lcd.clear();
      lcd.print("Set Timer HH:MM");
      encoderPos = 60;
      do {
        if (encoderPos > 180) encoderPos = 180;
        Minutes = encoderPos;
        Hours = int(Minutes / 60);
        Minutes = Minutes % 60;
        lcd.setCursor(6, 1); // 2. line
        sprintf(OutString, "%i:%02i", Hours, Minutes);
        lcd.print(OutString);
      } while (CheckKeyPress() ==  false);
      lcd.clear();
      CountDown = Hours * 3600 + Minutes * 60;
      lcd.print("Set Temperature");
      encoderPos = 25;
      do {
        if (encoderPos > 100) encoderPos = 100;
        lcd.setCursor(7, 1); // 2. line
        sprintf(OutString, "%3i C", encoderPos);
        lcd.print(OutString);
      } while (CheckKeyPress() ==  false);
      SetTemp = encoderPos % 100;
      // Run timed Thermostat
      RunStep(CountDown,SetTemp);
      SetTemp =  0;

}

void setup()
{

  Timer1.initialize(1000000);
  Timer1.attachInterrupt(TimeTick); //
  lcd.begin(16, 2);        // initialize the lcd for 20 chars 4 lines and turn on backlight
  lcd.setCursor(0, 0); // Header line
  lcd.print("Hansen Handbrew");
  lcd.setCursor(1, 1); // Header line
  lcd.print("Brewing Automat");

  //Setup Encoder
  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3)

  pinMode(ledPin, OUTPUT);
  pinMode(interruptShaftPinA, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(interruptShaftPinA), PinA, RISING);
  pinMode(interruptShaftPinB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(interruptShaftPinB), PinB, RISING);
  pinMode(encoderSwitchPin, INPUT_PULLUP);
  encoderPos = Mainmenu = 1;
  // Start up the sensor library
  sensors.begin();
  // sensors.setWaitForConversion(false);  // makes it async
  sensors.setWaitForConversion(true);
  // Serial.begin(115200);
  while (digitalRead(encoderSwitchPin) ==  true) ;
  ReadFromEEprom();
  // End Setup
}

void loop() {
  // sensors.setWaitForConversion(true);
  //  sensors.requestTemperatures();
  if (oldEncPos != encoderPos) {
    oldEncPos = encoderPos;
    //   while (digitalRead(encoderSwitchPin) != true) ;
    lcd.clear();
    do {
      if (Mainmenu != encoderPos % 5) lcd.clear();
      Mainmenu = encoderPos % 5;
      switch (Mainmenu) {
        case 0: // Setup
          lcd.setCursor(0, 0); // Header line
          lcd.print("Setup Menu");
          //do something when var equals 0
          break;
        case 1: // Thermometer
          lcd.setCursor(2, 0); // Header line
          lcd.print("Thermometer");
          //do something when var equals 1
          break;
        case 2: // Manuel Thermostat
          lcd.setCursor(0, 0); // Header line
          lcd.print("Man. Thermostat");
          //do something when var equals 2
          break;
        case 3: // Timer Thermostat
          lcd.setCursor(0, 0); // Header line
          lcd.print("Timer Thermostat");
          //do something when var equals 3
          break;
        case 4: // Sequense Thermostat
          lcd.setCursor(0, 0); // Header line
          lcd.print("Auto Step Mash ");
          //do something when var equals 4
          break;
        default:
          // if nothing else matches, do the default
          // default is optional
          break;
      }
    } while (CheckKeyPress() ==  false) ;
  }


  Hours, Minutes, Seconds = 0;
  switch (Mainmenu) {
    case 0: // Setup
      SetupMenu();
      encoderPos = 1;
      break;
    case 1: // Thermometer
      //    Temperature = sensors.getTempCByIndex(0);
      lcd.setCursor(2, 1); // 2. line
      dtostrf(Temperature, 5, 1, OutString);
      lcd.print("Temp :");
      lcd.print(OutString);
      //lcd.printf(Temperature);
      break;
    case 2: // Manuel Thermostat
      encoderPos = 25;
      Hours = 0;
      Minutes = 0;
      Seconds = 0;
      do {
        //  sensors.requestTemperatures();
        //  Temperature = sensors.getTempCByIndex(0);
        lcd.setCursor(0, 0); // Header line
        lcd.print("Temp. Set : ");
        SetTemp = encoderPos % 100;
        Thermostat();
        lcd.setCursor(0, 1); // 2. line
        sprintf(OutString, "%i:%02i:%02i -", Hours, Minutes, Seconds);
        lcd.print(OutString);
        dtostrf(Temperature, 5, 1, OutString);
        lcd.print(OutString);
        lcd.print(" C");
        if (encoderPos > 100 ) encoderPos = 100;
  //      Serial.print("Time used: ");
        //Serial.println(stop - start);
      } while (CheckKeyPress() ==  false);
      encoderPos = 1;
      Mainmenu = 0;
      break;
    case 3: // Timer Thermostat
      TimerThermostat();
      //do something when var equals 3
      break;
    case 4: // Sequense Thermostat
      lcd.setCursor(0, 0); // Header line
      lcd.print("Select Program ");
      do {
        RunMashProgramNumber = encoderPos % StepMashPrograms;
        lcd.setCursor(0, 1);
        lcd.print(MashProgram[RunMashProgramNumber].name);
      } while (CheckKeyPress() ==  false) ;
      RunStepMash(RunMashProgramNumber);
      lcd.clear();
      //do something when var equals 4
      break;
    default:
      // if nothing else matches, do the default
      // default is optional
      break;
  }
  SetTemp = 10;
  if (encoderPos > 4 ) encoderPos = 0;
  //Serial.println(encoderPos);
}



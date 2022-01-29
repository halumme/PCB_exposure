/*
 This sketch controls the operation of doubls sided 
 UV exposure box for making printed circuit boards.
 Exposure time is controlled. PCB is squeezed between the windows using a vacuum pump.
 Vacuum is controlled with a pressure sensor driving a DC air pump.
 UV LED array is controlled using one DO line for each side.
 Process parameters (time, vacuum) are permanently stored in EEPROM.
 Harri Lumme 25.3.2020 Init
 29.3.2020 meat on bones
 6.4.2020 major rewrite
 12.4.2020 Finalize and review the code
 22.10.2020 Changes after qualification test results implemented.
 -buttons, PID 
*/
#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <HX711.h>
#include <PWM.h>
#define PumpPin 9
#define Chrome 7

/* HX711 resistance bridge module is used to read
 * the pressure sensor, originally designed for scales.
*/
HX711 sensor(A0, A1, 32);
LiquidCrystal lcd(10,11,3,4,5,6); // LCD pins
int dblSide = 0;   // Single side as default
int prevBut, prevA, CumDif, MomDif, PrevDif ;
int lowLED = 12;
int highLED = 13;
long unsigned int lastpush; // debounce help
long unsigned int startms;  // Exposure start time
const int RotA = 0; // Encoder A channel pin
const int RotB = 1; // Encoder B channel pin
const int Rot = 2;  // Encoder button pin
const int debTime = 20; // Debounce time for buttons
int VacSPoint;    // unsigned and displayed as positive
int TimeSPoint;
int press = 0;    // Vacuum sensor reading after conversion
float Kp = 0.52;  // PID control parameters
float Ki = 0.100;
float Kd = 0.000;
long zero;
int State = 0;
  /* State values are as follows:
   * 0 => Stand by / Ready
   * 1 => Setup time
   * 2 => Setup vacuum
   * 3 => Choose 1- or 2-sided
   * 4 => Exposure on
   */
   
void setup() {
  /* initialize all inputs and outputs. 
   * Note that the LCD needs 6 DO lines!
   */
  lcd.begin(20,4); //initialize display
  pinMode(PumpPin, OUTPUT); // PWM
  pinMode(RotA, INPUT);
  pinMode(RotB, INPUT);
  pinMode(Rot, INPUT);
  pinMode(Chrome, INPUT);
  pinMode(lowLED, OUTPUT);
  pinMode(highLED, OUTPUT);
  InitTimersSafe();
  SetPinFrequencySafe(PumpPin, 5500);
  String alku = "*** UV EXP";
  String loppu = "OSURE  ***";
  String space = "  ";
  for(int i=1;i<=10;i++){
    lcd.setCursor(0,0);
    lcd.print(alku.substring(10-i));
    for (int j = 1; j<=(10-i); j++) lcd.print(space);
    lcd.print(loppu.substring(0,i));
    delay(180);
  }  
  VacSPoint  = EEPROM.read(11)+256*EEPROM.read(12); // LSB + MSB
  TimeSPoint = EEPROM.read(13)+256*EEPROM.read(14);
  lcd.setCursor(0,1);
  lcd.print("  Time ");
  lcd.print(TimeSPoint);
  lcd.setCursor(11,1);
  lcd.print("sec");
  lcd.setCursor(0,2);
  lcd.print("Vacuum ");
  lcd.print(VacSPoint);
  lcd.setCursor(11,2);
  lcd.print("hPa"); 
  zero = sensor.read_average(8);
 }

    /* The button() function reads two DI pins Rot and Chrome
     * They are called individually in different stages of the process
     * Note that both use the global variabe debTime 
     * to set the debounce time.
     * Adapted from the Arduino library Debounce sketch
    */
int button(int pinNro){
  int butState = digitalRead(pinNro);
  int push = 0;
  if((millis() - lastpush)> debTime) {    // read the large chrome button
    lastpush = millis();
    if (butState == 0 && prevBut == 1) {  // Trailing edge detected 1->0
      push = 1; 
      prevBut = 0;
    }
    if (butState == 1 && prevBut == 0) {  // Leading edge detected 0->1
      prevBut = 1;
    }
  }
  return push;
}

    /* The rotary() function reads the rotary encoder steps.
     * This is adapted from a tutorial video in YouTube
     * https://www.youtube.com/watch?v=v4BbSzJ-hz4
     * Note that in the last step the encoder just toggles
     * between values 1 and 2.
    */
int rotary(int param) {
  int enCodA = digitalRead(RotA);
  int muutos;
  if (enCodA != prevA){       // change has happened
    lastpush=millis();
    if (digitalRead(RotB) != enCodA){ // Clockwise change
      muutos = 1;
    } else{
      muutos = -1;
    }
    prevA = enCodA;
    switch (param){ // Modify and display requested parameter
      case 1:
      TimeSPoint += muutos;
      lcd.setCursor(7,1);
      lcd.print(TimeSPoint);
      lcd.print(" ");
      break;
      
      case 2:
      VacSPoint += muutos;
      lcd.setCursor(7,2);
      lcd.print(VacSPoint);
      lcd.print(" ");
      break;

      case 3:
      dblSide = (muutos > 0);
      lcd.setCursor(19,3);
      lcd.print(dblSide+1); // number or custom character??
      break;
    }
  }
}

    /* The vacuum() function reads the vacuum probe and 
     * adjusts the vacuum using the vacuum pump PWM
     * and updates the display when the LEDs are active.
     * Function modifies the global variable 'press'.
     * PID algorithm is used with fixed parameters.
     * To be tuned before final use.
     */
void vacuum(){
long AnVal=sensor.read_average(4);  // Read pressure through HX711
  press = (AnVal-zero)/20290;   // Conversion formula to hPa (mbar)
  int Dif = VacSPoint - press;
  CumDif += Dif;
  MomDif = Dif - PrevDif;
  PrevDif = Dif;
  int Cont = 50 + Kp * Dif + Ki * CumDif + Kd * MomDif; // Params set at row 38
  if (Cont>255) Cont = 255;
    pwmWrite(PumpPin, Cont);
    lcd.setCursor(15,2);
  lcd.print(press);
  lcd.print(" ");
}

    /* The expose() function controls the exposure routine
     * First, the initial vacuum is applied after which
     * the actual exposure is started (LEDs on and timer zeroing)
     * During the exposure, the vacuum is monitored and adjusted  
     * if necessary. At the same time the run time is measured and
     * break button monitored.
     */
void expose() {
  int RunTime=0;
  do {
    vacuum();   // Start the pump and wait (delay??)
  } while (press < VacSPoint); // until the set level of vacuum is achieved
  
  startms = millis();     // kirjaa aloitusaika
  digitalWrite(lowLED, HIGH); // Switch on the lower bank of LEDs
  if (dblSide) digitalWrite(highLED, HIGH); // also the upper bank of LEDs
  
  do {
    RunTime = (millis()-startms)/1000;
    vacuum();   // Update bar display
    lcd.setCursor(15,1);
    lcd.print(TimeSPoint-Runtime);
    lcd.print(" ");
    if (button(Chrome)) break;  // Check if reset button is being pressed and exit while  
  } while (RunTime < TimeSPoint);
  
  digitalWrite(lowLED, LOW);  // Shut the LEDs down
  digitalWrite(highLED, LOW);
  pwmWrite(PumpPin, 0); // Shut the pump down
  State = 0;
}

void loop() {
  switch (State) {
    case 0:
    if (button(Chrome)) State = 4; // keep reading the buttons
    if (button(Rot)) State = 1;
    break;
    
    case 1:
    rotary(1); // read rotary encoder vacuum set-up
    if (button(Rot)) State = 2;
    break;
    
    case 2:
    rotary(2); // read rotary encoder time
    if (button(Rot)) State = 3;
    break;
        
    case 3:
    rotary(3); // read rotary encoder dblSide
    if (button(Rot)) {    // If button pressed fourth time, write params
      EEPROM.write(11,VacSPoint & 0xff);
      EEPROM.write(12,VacSPoint >> 8);
      EEPROM.write(13,TimeSPoint & 0xff); // LSB
      EEPROM.write(14,TimeSPoint >> 8);   // MSB
      State = 0; // Back to stand-by
    }
    break;
    
    case 4:
    expose(); // run the exposure (line 184)
    break;    
  }
}
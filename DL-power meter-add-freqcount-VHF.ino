/*********************************************************************
 *
 * Dummy Load Power Meter for the  HF amateur bands  board uses Pro-Mini and tft display
 *
 * portions of this software are covered by the GNU General Public License ,
 * BEERWARE, or other open source licenses
 * DuWayne  KV4QB
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <duwayne@kv4qb.us> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   DuWayne
 * ----------------------------------------------------------------------------
 *
 * PC interface to be compatible with software from K6BEZ
 * uses a Arduino Nano , NOKIA 5110 LCD , Adafruit si5351 clock generator
 *
 * Libraries used Adafruit LCD and Graphics Libraries
 * si5351.h library from NT7S
 * rotary.h from AD7C
 *
 *
 *************/
#include <SPI.h>
#include <Adafruit_GFX.h>
//#include <Adafruit_PCD8544.h>
#include <TFT_ILI9163C.h>
#include <FreqCount.h>

#define __CS 4
#define __DC 6
#define __RST 7 //default=d5, untuk frequency counter

//Adafruit_PCD8544 display = Adafruit_PCD8544(4, 5, 6, 8, 7);

#define Backlight 9  // Analog output pin that the LED is attached to

// Color definitions
#define	BLACK   0x0000
#define	BLUE    0x001F
#define	RED     0xF800
#define	GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

TFT_ILI9163C display = TFT_ILI9163C(__CS, __DC, __RST);




//Global variables
// used by sweep and PC input

long serial_input_number; // Used to build number from serial stream
char incoming_char; // Character read from serial stream

// used in Power calculation
int AvePoint[128];       // average power is average of last 128 readings
int PwrPoint[8];      // display power is average of last 8 readings
float RFVoltage;      // input voltage
float Power;
float Average = 0;
int LastAve = 0;
float Peak  = 0;
int LastPwr = 0;   // used to clear display of last reading
int iPwr    ;
// used to measure time of loop
double then = 0;
double now  = 0;
//
//used by various display procedures

int RunOnce = 1;         // Only display splash screen first time through loop
int PwrCount = 0;        // set number of readings used for power
int AveCount = 0;        // set number of readings used for average power
float SupplyVoltage = 0; // Power supply voltage

int resetTimer = 0;      // counts time with no power reading before peak and average readings are cleared

// Analog inputs for VSWR computation
#define vin_PWR A2
// Analog input for battery monitor
#define vin_BAT A0
// voltage divider values
#define R3   (10)  // from GND to vin_BAT, expressed in 100R  (10 = 1000 Ohm)
#define R4   (47)  // from + power supply to vin_BAT, expressed in 100R  (47 = 4700 Ohm)
//change divider resistors to limit voltage to adc    ratio around 11 for 5 volt   16 for 3.3 volt
#define R1   (47)  // from GND to vin_PWR, expressed in 1K  (68 = 68k Ohm) 
#define R2   (320)  // from + RF Det to vin_PWR expressed in K  (680 = 680k Ohm)
#define VoltSupplyMini (50)       // minimum battery voltage expressed in 100mV (if lower, alarm is generated)

unsigned long f;float f0;
int x,n=3,r;



void setup() {
  
  pinMode (vin_PWR, INPUT);
  pinMode(vin_BAT, INPUT);
  pinMode(Backlight, OUTPUT);
  pinMode(12,INPUT);  //interval switch
  Serial.begin(57600);                    // connect to the serial port
  Serial.println("KV4QB Dummy Load Power Meter");
  FreqCount.begin(1000);
  // initalize display
  display.begin();
#if defined(__MK20DX128__) || defined(__MK20DX256__)
  tft.setBitrate(24000000);
#endif


  analogWrite(Backlight, 300);  // set backlight level

  display.setRotation(2);    // set orientation of display
  //display.display(); // show splashscreen
  dispScreen();
}



void loop(){

  measuresupplyvolt();   //check battery   and splash screen
  RunOnce = 0;

  RFVoltage = analogRead(vin_PWR);       //  Get rf voltage
 
  // also change divider resistors to limit voltage to adc    ratio around 11 for 5 volt   16 for 3.3 volt
  RFVoltage = map(RFVoltage, 0, 1023, 0, (500 * (float R1 + float R2) / float R1));  //  scale and adjsut for voltage divider     680k 68k = 11


 // Basic two resistors divider formula:
 // Vout = Vin * (R2 / R1+R2) , Original Sketch will showing reversed value when i tried to measure
 // Baofeng UV82 Hp.
  
  //RFVoltage = map((RFVoltage, 0, 1023, 0, )((500 * (float R2 / float R1 + float R2));  // modified by rudik, original pembacaan terbalik
  AvePoint[AveCount] = RFVoltage;        //  store value in arrays used to compute average
  PwrPoint[PwrCount] = RFVoltage;        //  and power
  AveCount++;
  PwrCount++;



  // Power reading is average of last 8 readings
  if (PwrCount == 8) {
    PwrCount = 0;
  }
  RFVoltage = 0;
  for (int i = 0; i < 8; i++)  {
    RFVoltage = RFVoltage + PwrPoint[i];
  }
  RFVoltage = RFVoltage / 8;
  // Serial.print(RFVoltage );Serial.print("+");Serial.print(PwrCount );
  Power = ((RFVoltage * RFVoltage) / 100) / 10000;    // compute power adjust for mw
  // only update  if changed from last reading
  iPwr = Power * 1000;  // do on integer to reduce bobble
  if (iPwr != LastPwr)  updatePowerBar(  Power  , BLACK,  Power);
  LastPwr = iPwr;
  // check for new peak value
  if (Power > Peak) {
    Peak = Power;
    updatePeakBar(  Peak , BLACK,  Peak);
  }

  //  Get average power from last 64 readings
  if (AveCount == 64) AveCount = 0;
  RFVoltage = 0;
  for (int i = 0; i < 64; i++)  {
    RFVoltage = RFVoltage + AvePoint[i];
  }
  RFVoltage = RFVoltage / 64;
  //
  Serial.print("-"); 
  Serial.print(RFVoltage ); 
  Serial.print(","); 
  Serial.println(AveCount );
  Average = ((RFVoltage * RFVoltage) / 100) / 10000;  // compute power adjust for mw
  // only update  if changed from last reading
  iPwr = Average * 1000;  //  use integer to reduce bobble I had with float
  if (iPwr != LastAve)  updateAverageBar(  Average  , BLACK,  Average);
  LastAve = iPwr;

  // check for no rf input

  if (LastPwr < 5) resetTimer++;   //  bump counter if no rf
  else resetTimer = 0;
  // No rf for about 10 seconds  then reset Peak
  if (resetTimer >= 10000) {
    resetTimer = 0;
    Peak = 0;
    Average = 0;
    updatePeakBar(  Peak , BLACK,  Peak);
    updateAverageBar(  Average , BLACK,  Average);

  }

}
/****

**/
void drawAverageBar() {
  display.fillRect(1, 1, 127, 40, YELLOW);
  display.setTextColor(BLUE);
  display.setTextSize(1);
  display.setCursor(2, 11);
  display.print("0  2  4   6  8  10 12");

  display.setTextSize(2);
  display.setCursor(3, 24);
  display.print("Ave ");
  display.setTextSize(1);
}

void drawPeakBar() {

  display.fillRect(1, 43, 127, 41, YELLOW);
  //display.setTextColor(BLUE);
  //display.setTextSize(1);
  //display.setCursor(2, 53);
  //display.print("0  2  4   6  8  10 12");
  display.setCursor(3,75);
  display.setTextSize(1);
  display.print("Peak");
}

void drawPowerBar() {
  display.fillRect(1, 86, 127, 41, YELLOW);
  display.setTextColor(BLUE);
  display.setTextSize(1);
  display.setCursor(2, 96);         //96
  display.print("0  2  4   6  8  10 12");
  display.setTextSize(2); //2
  display.setCursor(3, 110);
  display.print("Pwr ");
}

void updateAverageBar(float width, int color, float value) {
  if (width >= 10 )   color = RED;
  if (width >= 12 )   width = 122;
  display.fillRect(3, 3, 127, 4, YELLOW);
  display.fillRect(3, 3, width * 10, 4, color);
  display.fillRect(40, 21, 83, 16, YELLOW);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(40, 21);
  if (value <= 1) {
    display.print((value * 1000) , 0);
    display.setTextSize(1);
    display.print(" mW");
  }
  else {
    display.print(value , 3);
    display.setTextSize(1);
    display.print(" W");
  }
}


void updatePeakBar(float width, int color, float value) {
  if (width >= 122 )    width = 122;
  display.fillRect(3, 46, 127, 4, YELLOW);
  display.fillRect(3, 46, width * 10, 4, color);

  display.fillRect(40, 66, 83, 16, YELLOW);
  display.setTextSize(1);
  display.setTextColor(BLACK);
  display.setCursor(40,75);
  if (value <= 1) {
    display.print((value * 1000) , 0);
    display.setTextSize(1);
    display.print(" mW");
  }
  else {
    display.print(value , 3);
    display.setTextSize(1);
    display.print(" W");
  }
}


void updatePowerBar(float width, int color, float value) {
  if (width >= 10 )   color = RED;
  if (width >= 12 )   width = 122;
  display.fillRect(3, 90, 127, 4, YELLOW);
  display.fillRect(3, 90, width * 10, 4, color);
  display.fillRect(40, 110, 83, 16, YELLOW);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.setCursor(40, 110);
  if (value <= 1) {
    display.print((value * 1000) , 0);
    display.setTextSize(1);
    display.print(" mW");
  }
  else {
    display.print(value , 3);
    display.setTextSize(1);
    display.print(" W");
  }
}
// display screen with power peak and average bars
void dispScreen() {
  display.fillScreen(BLUE);
  drawAverageBar();
  drawPeakBar();
  drawPowerBar();

}

// Power SupplyVoltage measure voltage and test for low volts
//display splash screen and battery voltage

void measuresupplyvolt () {

  SupplyVoltage = analogRead(vin_BAT);      // Read power supply voltage
  SupplyVoltage = map(SupplyVoltage, 0, 1023, 0, (500 * (float R4 + float R3) / float R3)); // compute battery voltage using voltage divider values
  // SupplyVoltage = SupplyVoltage + 70;   // add in voltage drop of diode  if installed

//****************************added by rudik************************************************
void MeasureFreqCount(); {
  
if(digitalRead(10)==HIGH){n++;x=0;delay(100);}
  
  //display.begin();
  //display.clearScreen();
  display.setCursor(90,55);
    display.setTextSize(1);
    display.setTextColor(RED);
  
  if(n==1){x++;if(x==1){
    FreqCount.begin(100);
  }r=-1;
  display.print("T= 0.1 s");}
  if(n==2){x++;if(x==1){
    FreqCount.begin(10000);
    }r=1;
    display.print("T= 10 s");}
  if(n==3){x++;if(x==1){
    FreqCount.begin(1000);
    }r=0;
    display.print("T= 1 s");}
  if(n>3){n=1;} 
    display.setCursor(3,55);
    display.setTextSize(1);
    display.print("F=");
    
  if(f>=1000000 && n==3){f0=f/1000000.0;
  display.print(f0,6+r);
  display.print(" MHz");
  delay(50);
  }
  if(f<1000000 && n==3){f0=f/1000.0;
  display.print(f0,3+r);
  display.print(" kHz");
  }
  if(f>=100000 && n==1){f0=f/100000.0;
  display.print(f0,6+r);
  display.print(" MHz");
  }
  if(f<100000 && n==1){f0=f/100.0;
  display.print(f0,3+r);
  display.print(" kHz");
  }
  if(f>=10000000 && n==2){f0=f/10000000.0;
  display.print(f0,6+r);
  display.print(" MHz");
  }
  if(f<10000000 && n==2){f0=f/10000.0;
  display.print(f0,3+r);
  display.print(" kHz");
  }

  if (FreqCount.available()) { 
    f = FreqCount.read(); 
    
   display.setCursor(105,75);
   display.print("***");
  }
   delay(1000);
   //display.clearScreen();
}

  if (RunOnce == 1) {                     // splash screen
    display.setTextColor(YELLOW);
    display.fillScreen(BLUE);
    display.setCursor(24, 35);
    display.setTextSize(2);
    display.println("YD0AYA");
    display.setTextSize(2);
    display.setCursor(48, 65);
    display.print("QRP");
    display.setTextSize(1);
    display.setCursor(35, 80);
    display.print("Dummy Load");
    display.setCursor(34, 90);
    display.print("Watt-Meter");

    display.setTextSize(1);
    display.setCursor(30, 110);
    display.print("Batt =");
    display.print((SupplyVoltage / 100));
    display.print("V");

    delay (2000);
    dispScreen();
  }
  if (SupplyVoltage  <= VoltSupplyMini) {
    if (SupplyVoltage <= 500) {       // not running on batteries !
      return;
    }
    display.fillScreen(BLUE);
    display.setTextSize(1);
    display.setTextColor(YELLOW);
    display.setCursor(30, 80);
    display.print("LOW BATTERY !");
    display.setCursor(30, 110);
    display.print("Batt =");
    display.print((SupplyVoltage / 100));
    display.print("V");
    delay (2000);
    dispScreen();

  }
}


//EoF = Inspected by rudik wid, YD0AYA, 02-AGUST-2022

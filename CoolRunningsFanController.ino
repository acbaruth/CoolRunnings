/*
  DS18B20 Pinout(Left to Right, pins down, flat side toward you)
  - Left = Ground - Center = Signal(Pin 2):(with 3.3 K to 4.7 K resistor to +
  5 or 3.3)
    - Right = +5 or + 3.3 V
*/
/*-----( Import needed libraries )-----*/
#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
/*-----( Declare Constants and Pin Numbers )-----*/
#define ONE_WIRE_BUS_PIN 11  // Must be Digital Pin
/*-----( Declare objects )-----*/
// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS_PIN);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

/*-----( Declare Variables )-----*/

LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);             // Set the LCD I2C address different for some modules
DeviceAddress Probe01 = { 0x28, 0xFF, 0x4E, 0xD6, 0xC1, 0x16, 0x04, 0xD4 };
DeviceAddress Probe02 = { 0x28, 0xFF, 0xAC, 0x8F, 0x90, 0x16, 0x05, 0x48 };

int fanpwr = 9;   //Fan Pin Must be PWM pin
int gled = 5;     //Green LED
int yled = 8;     //Yellow LED
int rled = 7;     //Red LED
unsigned long lastmillis_term = 0;
unsigned long lastmillis_fan = 0;
unsigned long previousMillis = 0;        // will store last time
volatile unsigned long NbTopsFan1;
volatile unsigned long NbTopsFan2;
int hallsensor1 = 2;                    //Arduino pins 2 and 3 must be used - interrupt 0
int hallsensor2 = 3;                    //Arduino pins 2 and 3 must be used - interrupt 1
unsigned long totcalc1;
unsigned long totcalc2;
int rpmcalc1;
int rpmcalc2;
volatile unsigned long calcsec1;
volatile unsigned long calcsec2;
boolean automode = true;
char *fanmode;
boolean manled;
boolean lowled;
boolean medled;
boolean highled;
boolean autobled = true;
boolean autoupdate = false;
boolean contemp = true;
float tempIn;
float tempOut;
int pwmlow = 70;   // PWM defaults
int pwmmed = 120;
int pwmhigh = 255;
float lowTemp = 1; //temperature defaults
float medTemp = 4;
float highTemp = 6;
char junk = ' ';
unsigned long looptime = 10000;
unsigned long looptimeSec = 10;


typedef struct fan1
{
  //Defines the structure for multiple fans and their dividers
  char fantype1;
  unsigned int fandiv1;
}
fanspec1;

typedef struct fan2
{
  //Defines the structure for multiple fans and their dividers
  char fantype2;
  unsigned int fandiv2;
}

fanspec2;

fanspec1 fanspace1[3] = {{0, 1}, {1, 2}, {2, 8}};  char fan1 = 1;
fanspec2 fanspace2[3] = {{0, 1}, {1, 2}, {2, 8}};  char fan2 = 1;

void rpm1 ()
//This is the function that the interupt calls
{
  NbTopsFan1++;
}

void rpm2 ()
//This is the function that the interupt calls
{
  NbTopsFan2++;
}

//Function Definitions for non arduino IDE
void handleSerial();
void autocontrol();
void setpwm();
void setTemp();
void pollTime();
int readtemps(DeviceAddress);
void serialoutput();



void setup() {
  // start serial port to show results
  Serial.begin(9600);
  lcd.begin(16, 2);  // initialize the lcd for 16 chars 2 lines, turn on backlight

  // Initialize the Temperature measurement library
  sensors.begin();
  Serial.println("CoolRunnings v2.0");
  Serial.print("Initializing Temperature Control Library Version ");
  Serial.println(DALLASTEMPLIBVERSION);
  Serial.print("Tempurature sensors found = ");
  Serial.print(sensors.getDeviceCount());
  Serial.println();
  Serial.println("Type o to view control output");          //print these commands during setup
  Serial.println("Type ? to view command list");

  lcd.setCursor(1, 0); //Start at character 0 on line 0
  lcd.print("Cool Runnings");
  lcd.setCursor(0, 1); //Start at character 0 on line 1
  lcd.print("Fan Control v2.0");

  pinMode(hallsensor1 , INPUT);
  pinMode(hallsensor2 , INPUT);
  attachInterrupt(0, rpm1, FALLING);
  attachInterrupt(1, rpm2, FALLING);

  // set the resolution to 10 bit (Can be 9 to 12 bits .. lower is faster)
  sensors.setResolution(Probe01, 10);
  sensors.setResolution(Probe02, 10);



  TCCR1B = (TCCR1B & 0b11111000) | 0x05;  //for 30Hz pwm on pins 9 and 10. RPM reading works best with current filter capacitor values. Slight audible fan noise.
  //   TCCR1B = (TCCR1B & 0b11111000) | 0x01;  //for 31KHz pwm on pins 9 and 10. effective pwm range is good, but unable to find filter capacitor values to elminate pwm noise. audible noise is gone.

}       //--(end setup )---

void loop() {


  readtemps(Probe01);
  handleSerial();
  autocontrol();


  if (millis() - lastmillis_fan >= 2000)         // Interval at which to run fan rpm code. If this changes, so does the divider for rpmcaclc1/2
  {
    lastmillis_fan = millis();                   // Update lasmillis
    // Command all devices on bus to read temperature

    totcalc1 = ((NbTopsFan1 ) / fanspace1[fan1].fandiv1);
    totcalc2 = ((NbTopsFan2 ) / fanspace2[fan2].fandiv2);
    unsigned long currentMillis = millis();

    previousMillis = currentMillis;

    rpmcalc1 = ((totcalc1 - calcsec1) * 60 / 2 ); //calculate rpm, divide by number of seconds which timer is run
    rpmcalc2 = ((totcalc2 - calcsec2) * 60 / 2); //count for a few seconds and divide for a smoother result

    if (previousMillis == currentMillis)    //store total rpm calculations at each interval
    {
      calcsec1 = totcalc1;
      calcsec2 = totcalc2;
    }

  }// End fan code here


  if (millis() - lastmillis_term >= looptime)         // Interval at which to run code
  {
    lastmillis_term = millis();                   // Update lasmillis
    // Command all devices on bus to read temperature

    sensors.requestTemperatures(); //Request temperatues at interval to avoid minor fluctuation at temp threshold value triggering fan

    if (autoupdate == true) //print values at the interval the code is run if autoupdate is enabled
    {
      serialoutput();
    }
  }
  if (tempIn == (-196.6 || -127) || tempOut == (-196.6 || -127)) //if sensors read -196.6 fahrenheit or -127 C they missing or not being read correctly, unless hell has frozen over

  {
    Serial.println("Error getting temperatures  ");
    lcd.setCursor(0, 0);
    lcd.print("Error getting   ");
    lcd.setCursor(0, 1);
    lcd.print("  Temperatures  ");
  }
  else
  {

    lcd.setCursor(0, 0);                        //Start at character 0 on line 0 and print out lcd information
    lcd.print("T1 ");
    lcd.print(tempIn, 1);                       //print temperature and show one decimal place
    lcd.print(" ");
    lcd.setCursor(8, 0);                        //Start at character 8 on line 0
    lcd.print("T2 ");
    lcd.print(tempOut, 1);
    lcd.print("  ");
    lcd.setCursor(0, 1);                        //Start at character 0 on line 1
    lcd.print("Fan: ");

  }

  if (automode == true)
  {
    lcd.print("Auto ");
  }
  else
  {
    lcd.print("Manual ");
  }
  lcd.print(fanmode);
  lcd.print("   ");
  /*
        lcd.print("F1 ");
        lcd.print(rpmcalc1);
        lcd.print("  ");
        lcd.setCursor(8, 1);                        //Start at character 8 on line 1
        lcd.print("F2 ");
        lcd.print(rpmcalc2);
        lcd.print("  ");
  */
  //  }


  lowled = digitalRead(gled); // read state of fan low speed led
  medled = digitalRead(yled);
  highled = digitalRead(rled);

  //Backlight controls
  if (manled == false && autobled == false) //manual off
  {
    lcd.noBacklight();
  }
  if (manled == true && autobled == false) //manual on
  {
    lcd.backlight();
  }
  if (autobled == true && lowled == true)  // auto on
  {
    lcd.backlight();
  }
  if (autobled == true && lowled == false) //auto off
  {
    lcd.noBacklight();
  }

  if (fanmode == "Off")       //set leds to turn off or on with fan speeds
  {
    analogWrite(gled, 0);
    analogWrite(yled, 0);
    analogWrite(rled, 0);
  }
  else if (fanmode == "Low")
  {
    analogWrite(gled, 255);
    analogWrite(yled, 0);
    analogWrite(rled, 0);
  }
  else if (fanmode == "Medium")
  {
    analogWrite(gled, 255);
    analogWrite(yled, 255);
    analogWrite(rled, 0);
  }
  else if (fanmode == "High")
  {
    analogWrite(gled, 255);
    analogWrite(yled, 255);
    analogWrite(rled, 255);
  }
}

void handleSerial() {

  while (Serial.available()) {
    switch (Serial.read()) {

      case 'a':
        automode = true;
        break;

      case 'l':
        analogWrite(fanpwr, pwmlow);
        automode = false;
        fanmode = "Low";
        break;

      case 'm':
        analogWrite(fanpwr, pwmmed);
        automode = false;
        fanmode = "Medium";
        break;

      case 'h':
        analogWrite(fanpwr, pwmhigh);
        automode = false;
        fanmode = "High";
        break;

      case 'k':
        analogWrite(fanpwr, 0);
        automode = false;
        fanmode = "Kill";
        break;

      case 'b':
        lcd.backlight();
        manled = true;
        autobled = false;
        break;

      case 'd':
        lcd.noBacklight();
        manled = false;
        autobled = false;
        break;

      case 'u':           //enable automatic backlight
        autobled = true;
        break;

      case 'r':
        autoupdate = true;
        break;

      case 's':
        autoupdate = false;
        break;

      case 'f':
        contemp = true;
        break;

      case 'c':
        contemp = false;
        break;

      case 't':
        setTemp();
        break;

      case 'p':
        setpwm();
        break;

      case 'z':
        Serial.print(F("Feel the Rhythm! Feel the Rhyme! Get on up, it's bobsled time!"));
        break;

      case 'o':
        serialoutput();
        break;

      case 'i':
        pollTime();
        break;
      
      case 'q':
        lcd.begin(16, 2);
        break;
      
      case '?':
        Serial.println();
        Serial.println(F("Serial Commands"));
        Serial.println(F("o = Show Control Output"));
        Serial.println(F("a = Automode On"));
        Serial.println(F("l = Manual Fan Low"));
        Serial.println(F("m = Manual Fan Medium"));
        Serial.println(F("h = Manual Fan High"));
        Serial.println(F("k = Manual Fan Kill"));
        Serial.println(F("b = LCD Backlight Bright"));
        Serial.println(F("d = LCD Backlight Dark"));
        Serial.println(F("u = Auto Backlight Mode"));
        Serial.println(F("r = Auto Update Terminal(Recurring)"));
        Serial.println(F("s = Stop Auto Update Terminal"));
        Serial.println(F("f = Switch to Fahrenheit"));
        Serial.println(F("q = Restart LCD display"));
        Serial.println(F("c = Switch to Celsius"));
        Serial.println(F("p = Set PWM values(Advanced)"));
        Serial.println(F("t = Set temperature thresholds(Advanced)"));
        Serial.println(F("i = Set timer interval(Advanced)"));
        Serial.println(F("? = Print Command List"));
        break;
    }
  }
}

int readtemps(DeviceAddress)   //temp sensor reading
{
  if (contemp == true)
  {
    tempOut = sensors.getTempF(Probe01);
    tempIn = sensors.getTempF(Probe02);
  }
  else
  {
    tempOut = sensors.getTempC(Probe01);
    tempIn = sensors.getTempC(Probe02);
  }
}

/*
void autocontrol()            //temp based control
{
  if ((tempIn > tempOut + lowTemp) && (tempIn <= tempOut + medTemp) && automode)      //threshold temps for low auto speed here
  {
    analogWrite(fanpwr, pwmlow);                                           //low pwm setting here
    fanmode = "Low";
  }
  else if ((tempIn > tempOut + medTemp) && (tempIn <= tempOut + highTemp) && automode)   //threshold temps for medium auto speed here
  {
    analogWrite(fanpwr, pwmmed);                                           //medium pwm setting here
    fanmode = "Medium";
  }
  else if ((tempIn > tempOut + highTemp) && automode)                            //threshold temps for high auto speed here
  {
    analogWrite(fanpwr, pwmhigh);                                           //high pwm setting here maximum is 255(not actually pwm anymore)
    fanmode = "High";
  }
  else if (automode)                                                    //auto mode is true if manual mode is off
  {
    analogWrite(fanpwr, 0);                                             //fan is off if non of above criteria is met
    fanmode = "Off";
  }
}
*/
void autocontrol()            //temp based control
{

if (tempIn > 70)
{
  
  if ((tempIn > tempOut + lowTemp) && (tempIn <= tempOut + medTemp) && automode)      //threshold temps for low auto speed here
  {
    analogWrite(fanpwr, pwmlow);                                           //low pwm setting here
    fanmode = "Low";
  }
  else if ((tempIn > tempOut + medTemp) && (tempIn <= tempOut + highTemp) && automode)   //threshold temps for medium auto speed here
  {
    analogWrite(fanpwr, pwmmed);                                           //medium pwm setting here
    fanmode = "Medium";
  }
  else if ((tempIn > tempOut + highTemp) && automode)                            //threshold temps for high auto speed here
  {
    analogWrite(fanpwr, pwmhigh);                                           //high pwm setting here maximum is 255(not actually pwm anymore)
    fanmode = "High";
  }
}
  else if (tempIn <= 70 && automode)                                                    //auto mode is true if manual mode is off
  {
    analogWrite(fanpwr, 0);                                             //fan is off if non of above criteria is met
    fanmode = "Off";
  }
}

void setpwm()
{
  Serial.println("Enter a value from 0 to 255 for fan low power setting.(default 70)");
  while (Serial.available() == 0) ;  // Wait here until input buffer has a character
  {
    // read the incoming byte:
    pwmlow = Serial.parseFloat();
    // say what you got:
    Serial.print("Value set to: ");
    Serial.println(pwmlow, DEC);
    while (Serial.available() > 0)  // .parseFloat() can leave non-numeric characters
    {
      junk = Serial.read() ;  // clear the keyboard buffer
    }
  }
  Serial.println("Enter a value from 0 to 255 for fan Medium power setting. (default 120)");
  while (Serial.available() == 0) ;
  {
    pwmmed = Serial.parseFloat();
    Serial.print("Value set to: ");
    Serial.println(pwmmed, DEC);
    while (Serial.available() > 0)
    {
      junk = Serial.read() ;
    }
  }
  Serial.println("Enter a value between 0 and 255 for fan High power setting. (default 255)");
  while (Serial.available() == 0) ;
  pwmhigh = Serial.parseFloat();
  Serial.print("Value set to: ");
  Serial.println(pwmhigh, DEC);
  Serial.println("Done!");
  while (Serial.available() > 0)
  {
    junk = Serial.read() ;
  }

}


void setTemp()
{
  Serial.println("Enter number of degrees sensor 1 must differ from sensor 2 to active the fan on the low setting.(default 1)");
  while (Serial.available() == 0) ;  // Wait here until input buffer has a character
  {
    // read the incoming byte:
    lowTemp = Serial.parseFloat();
    // say what you got:
    Serial.print("Value set to: ");
    Serial.println(lowTemp, DEC);
    while (Serial.available() > 0)  // .parseFloat() can leave non-numeric characters
    {
      junk = Serial.read() ;  // clear the keyboard buffer
    }
  }
  Serial.println("Enter number of degrees sensor 1 must differ from sensor 2 to active the fan on the medium setting. (default 4)");
  while (Serial.available() == 0) ;
  {
    medTemp = Serial.parseFloat();
    Serial.print("Value set to: ");
    Serial.println(medTemp, DEC);
    while (Serial.available() > 0)
    {
      junk = Serial.read() ;
    }
  }
  Serial.println("Enter number of degrees sensor 1 must differ from sensor 2 to active the fan on the high setting. (default 6)");
  while (Serial.available() == 0) ;
  highTemp = Serial.parseFloat();
  Serial.print("Value set to: ");
  Serial.println(highTemp, DEC);
  Serial.println("Done!");
  while (Serial.available() > 0)
  {
    junk = Serial.read() ;
  }

}

void serialoutput()
{
  Serial.println();
  Serial.print("Control Information:   \r\n");
  Serial.print("TempInside:  ");
  Serial.print(tempIn);
  if (contemp)
  {
    Serial.print(" F");
  }
  else
  {
    Serial.print(" C");
  }
  Serial.println();
  Serial.print("TempOutside: ");
  Serial.print(tempOut);
  if (contemp)
  {
    Serial.print(" F");
  }
  else
  {
    Serial.print(" C");
  }
  Serial.println();
  if (rpmcalc1 != 0)
  {
    Serial.print (rpmcalc1, 1);
    Serial.print (" Fan1 rpm\r\n");
  }
  if (rpmcalc2 != 0)
  {
    Serial.print (rpmcalc2, 1);
    Serial.print (" Fan2 rpm\r\n");
  }
  Serial.print ("Fan Speed: ");
  Serial.println (fanmode);

  if (automode == true)
  {

    Serial.println(F("Mode: Auto"));
  }
  else
  {
    Serial.println(F("Mode: Manual"));
  }

  if (looptimeSec != 10)
  {
    Serial.print ("Custom Timer: ");
    Serial.print(looptimeSec);
    Serial.println(" sec");
  }

  if (pwmlow != 70 || pwmmed != 120 || pwmhigh != 255)
  {
    Serial.println("Custom PWM Enabled");
  }
  if (pwmlow != 70)
  {
    Serial.print("Low PWM Setting: ");
    Serial.println(pwmlow);
  }
  if (pwmmed != 120)
  {
    Serial.print("Medium PWM Setting: ");
    Serial.println(pwmmed);
  }
  if (pwmhigh != 255)
  {
    Serial.print("High PWM Setting: ");
    Serial.println(pwmhigh);
  }
  if (lowTemp != 1 || medTemp != 4 || highTemp != 6)
  {
    Serial.println("Custom Temp Enabled");
  }
  if (lowTemp != 1)
  {
    Serial.print("Low Temp Threshold: +");
    Serial.print(lowTemp);
    if (contemp)
    {
      Serial.println(" F");
    }
    else
    {
      Serial.println(" C");
    }
  }
  if (medTemp != 4)
  {
    Serial.print("Medium Temp Threshold: +");
    Serial.print(medTemp);
    if (contemp)
    {
      Serial.println(" F");
    }
    else
    {
      Serial.println(" C");
    }
  }
  if (highTemp != 6)
  {
    Serial.print("High Temp Threshold: +");
    Serial.print(highTemp);
    if (contemp)
    {
      Serial.println(" F");
    }
    else
    {
      Serial.println(" C");
    }
  }
  Serial.print("-------------------------------\r\n");
}

void pollTime()
{
  Serial.println("Enter the number of seconds the controller should check to see if it should do somthing in automode(default 10)");
  while (Serial.available() == 0) ;  // Wait here until input buffer has a character
  {
    // read the incoming byte:
    looptimeSec = Serial.parseFloat();
    // say what you got:
    Serial.print("Value set to: ");
    Serial.println(looptimeSec, DEC);
    while (Serial.available() > 0)  // .parseFloat() can leave non-numeric characters
    {
      junk = Serial.read() ;  // clear the keyboard buffer
    }
    looptime = (looptimeSec * 1000);
    Serial.println("Done!");
  }

}

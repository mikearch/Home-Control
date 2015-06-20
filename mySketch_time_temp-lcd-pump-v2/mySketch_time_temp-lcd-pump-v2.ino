/*
   Home Control
   Current capabilities
    * Monitors RTC and displays time date and indoor temperature 
    * Logs date and temperature every 30 minutes on the hour
    * Controls pump in rainwater cistern and turns on pump based on:
    *   - ON if secondary tank is empty
    *   - ON if less than MAX fills per day
    *   - ON if current hour is between start and end hours
    *   - ON if equal or more than 8 hours from last pump time
    *   Note: pump has a float switch that prevents pumping when cistern is empty

   Next Implementation
   Monitors outdoor and indoor tempurature and status of rainwater storage barrel.
   Enables or disables boiler master switch, fuel pump switch and burner switch depending upon:
    * Indoor Temperature
    * Outdoor Temperature History
    * Month of the year
    * Occupancy status of the home
    
    Future Improvements
    * Create single function for data logging.  Currently data logging for pump and RTC display are redundant.
    * Utilize GSM module for text based notifications and instruction
    * Display status via WIFI to internet.
    * Create libraries for reusable sections of code.
    * Add RTC reset within core code to eliminate need to load separate RTC update sketch
    * Add DHT22 Sensor
    * Add Motion detection to control backlight on LCD
    * Add menu system
 
 Debounce Routine:
 by David A. Mellis
 modified 30 Aug 2011
 by Limor Fried
 modified 28 Dec 2012
 by Mike Walters

 RTC Routine:
 
    
 */
#include <SPI.h>
#include <Wire.h>
#include <Time.h>
#include <LiquidCrystal_I2C.h>
#include <DS1307RTC.h>
#include <SD.h>

File weatherfile;

float temp_in_celsius = 0;
float temp_in_kelvin=0;
float temp_sensor_adj = .9;

const int buttonPin = 2;      // the number of the pushbutton pin for backlight
const int buttonPinlog = 3;   // the number of the pushbutton for enabling-disabling logging
const int pumpPin = 4;        // number of pin for operating pump
const int pumpPinstatus = 5;  // number of pin to sense pump on
const int buttonPin_pump = 6;    // the number of the pushbutton pin for pump control

// Variables will change:

int buttonState_pump;             // the current reading from the input pin
int lastButtonState_pump = LOW;   // the previous reading from the input pin

// the following variables are long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long lastDebounceTime = 0;  // the last time the output pin was toggled
long debounceDelay = 50;    // the debounce time; increase if the output flickers



int currentMinsold =0;
int buttonState = 0;          // variable for reading the pushbutton status
int buttonStateLog = 0;       // variable for reading state of data logging pushbutton
int tankswitch = 0;
int water_str_hr = 6;         //fill tank no earlier than - 24hr
int water_end_hr = 15;        //fill tank no later than
int water_tank_fills = 0;     //counts daily fills
int water_fills_max = 1;      //maximun number of daily tank fills
int max_fill_time = 120;       //max time in seconds for tank to fill
int fill_start_second = 0;
int fill_start_minute =0;
int curr_fill_second = 0;
int min_time_next_fill = 8;   //max time in hours until next fill
int curr_fill_hr = 0;
int last_fill_hr =0;
int curr_fill_min = 0;

boolean pump_on = false;
boolean pump_ok_run = false;
boolean pump_enable = false;
boolean pump_log_on = false;

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

void setup() 
{
    // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);

  pinMode(buttonPin_pump, INPUT);


  
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, HIGH);
  pinMode(pumpPinstatus, INPUT);
  Serial.begin(9600);
  lcd.init();                      // initialize the lcd 
  lcd.init();
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Time:");
  lcd.setCursor(0,1);
  lcd.print("Date:");
  lcd.setCursor(0,2);
  lcd.print("Temperature:");
  lcd.setCursor(0,3);
  lcd.print("Pump:");
  
  while (!Serial) ; // wait for serial
  delay(200);
  SD.begin();
}

void loop() 
{
  button_read();
      
// --------------BUTTON FOR TURNING OFF DISPLAY BACKLIGHT
    buttonState = digitalRead(buttonPin);
   
      // check if the pushbutton is pressed.
      // if it is, the buttonState is HIGH:
 
    if (buttonState == HIGH) 
    {     
      // turn display off    
      lcd.noDisplay();
      lcd.noBacklight();  
     } 
    
    else 
    {
      // turn LED off:
      lcd.display();
      lcd.backlight();  
    }
 //----------------END OF DISPLAY BUTTON 

 lcd.setCursor(6,3);
 if (pump_enable == true)
 {
  lcd.print("enabled");
 }

 else
 {
  lcd.print ("disabled"); 
 }
 //---------------GETS AND CALCULATES CELSIUS TEMPERATURE
    //Reads the input and converts it to Kelvin degrees
  temp_in_kelvin = analogRead(0) * 0.004882812 * 100;
  
    //Converts Kelvin to Celsius minus 2.5 degrees error
  temp_in_celsius = temp_in_kelvin - temp_sensor_adj - 273.15; 
 //----------------END OF TEMPERATURE CALCULATION

 //----------------DISPLAYS TEMPERATURE 
  lcd.setCursor(13,2);
  lcd.print(temp_in_celsius);
  lcd.setCursor(0,3);
  //lcd.print(analogRead(0));
  //lcd.print(analogRead(1));
//-----------------END OF TEMPURATURE DISPLAY


//-----------------GETS AND DIPLAYS TIME AND DATE AND DECIDES TO LOG OR NOT
  //Reads RTC
  tmElements_t tm;
  int currentMins = (tm.Minute);  //used to permit logging to skip on 15 minute intervals
   
  if (RTC.read(tm)) 
   {
      lcd.setCursor(6,0);
      if ((tm.Hour)<10) 
         {
              lcd.print("0");  
         }
      lcd.print(tm.Hour);
      lcd.print(":");
      
      if ((tm.Minute)<10) 
         {
              lcd.print("0");
         }
      lcd.print(tm.Minute);
      lcd.setCursor(6,1);
      lcd.print(tm.Day); 
      lcd.print("/");
      lcd.print(tm.Month);
      lcd.print("/");
      lcd.print(tmYearToCalendar(tm.Year));  
    } 
    
    else 
    {
      lcd.setCursor(0,0);
      if (RTC.chipPresent()) 
        {
              lcd.print("Run the SetTime");
        } 
      else 
        {
              lcd.print("Chk Circuits");
        }
              //delay(9000);
    }
      //-----TEST IF DATA IN ON 30 MINUTE INTERVAL
      if (currentMins != currentMinsold)   // skips logging if previouslz logged on the 15 interval
      { 
            if (currentMins == 00 || currentMins == 30)
            {
                data_log ();
            }
           
        currentMinsold = currentMins;
      }
      //-----END OF 30 MINUTE TEST FOR LOGGING
      
  delay(1000);
  
  
  if (pump_enable == true)
  {
    pump_ok_run = pump_run_test();
    if (pump_ok_run == true)
    {  
      pump_run();
      pump_ok_run = false;
    }
     digitalWrite(pumpPin,HIGH);
  }
}

//------------END OF VOID LOOP--------------


//------------DATA LOGGING FOR TIME and TEMP----------
//Writes date, and temperature every 15 minutes.

void data_log ()
 {
  weatherfile = SD.open("weather.txt",FILE_WRITE);
 
  //----------FUNCTION TO PERFORM DATA LOGGING
  //Reads RTC
  tmElements_t tm;
  if (RTC.read(tm)) 
   {
     weatherfile.print(tm.Day);
     weatherfile.print("/");
     weatherfile.print(tm.Month);
     weatherfile.print("/");
     weatherfile.print(tmYearToCalendar(tm.Year));
     weatherfile.print(",");  
    
     if ((tm.Hour)<10) 
      {
         weatherfile.print("0");
      }
     weatherfile.print((tm.Hour));
     weatherfile.print(":");
  
      if ((tm.Minute)<10) 
      {
         weatherfile.print("0");
      }
     weatherfile.print((tm.Minute));
     weatherfile.print(","); 
      
     weatherfile.println(temp_in_celsius);
     weatherfile.close();
     return;
  }   
 }
//-------------PERFORMS PUMP ON AND OFF LOGGING----
 void log_pump()
 {
 weatherfile = SD.open("weather.txt",FILE_WRITE);
  tmElements_t tm;
  if (RTC.read(tm))
  {
     weatherfile.print(tm.Day);
     weatherfile.print("/");
     weatherfile.print(tm.Month);
     weatherfile.print("/");
     weatherfile.print(tmYearToCalendar(tm.Year));
     weatherfile.print(",");  
    
     if ((tm.Hour)<10) 
      {
         weatherfile.print("0");
      }
     weatherfile.print((tm.Hour));
     weatherfile.print(":");
  
      if ((tm.Minute)<10) 
      {
         weatherfile.print("0");
      }
     weatherfile.print((tm.Minute));
     weatherfile.print(","); 
     if (pump_log_on == true)
     {
      weatherfile.println ("pump on");
     }
     else
     {
      weatherfile.println("pump off");
     }
    
     weatherfile.close();
     return;    
  }
 }

 
 //----------FUNCTIONS TO CONTROL PUMP
   
 void pump_run()
  {
   pump_log_on = true;
   log_pump();
   Serial.println("pump run");
  tmElements_t tm;
   if (RTC.read(tm)) 
   {
  for( int counter=0; counter < max_fill_time; counter++)
    {
    digitalWrite(pumpPin, LOW);
    delay(1000);
    }
  last_fill_hr = (tm.Hour);
  digitalWrite(pumpPin, HIGH);
  pump_log_on = false;
  log_pump();
   }
 }
  


boolean pump_run_test ()
{
  boolean pump = false;
  tmElements_t tm;
   if (RTC.read(tm))
   {
  if ((tm.Hour) == 0)
    {
     water_tank_fills = 0; 
    }
    
   if ((tm.Hour) > water_str_hr  && (tm.Hour) < water_end_hr)
   {
      if (water_tank_fills >= water_fills_max)
      {
        pump = false;
        return pump;
      }
      if (((tm.Hour)- last_fill_hr) > min_time_next_fill)
      {
        water_tank_fills = water_tank_fills +1;
        pump = true;
        return pump;
      }
    }
    else
    {
      pump = false;
      return pump;
    }
   }  
 }

//----------------DEBOUNCES BUTTON FOR PUMP ENABLING
 void button_read()
 {
   // read the state of the switch into a local variable:
  int reading = digitalRead(buttonPin_pump);

  // check to see if you just pressed the button
  // (i.e. the input went from LOW to HIGH),  and you've waited
  // long enough since the last press to ignore any noise:

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState_pump) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState_pump) {
      buttonState_pump = reading;

      // only toggle the LED if the new button state is HIGH
      if (buttonState_pump == HIGH) {
        pump_enable =  !pump_enable;
      }
    }
  }
  // save the reading.  Next time through the loop,
  // it'll be the lastButtonState:
  lastButtonState_pump = reading;
 }


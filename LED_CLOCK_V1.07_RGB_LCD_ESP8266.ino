  /*
  WiFi connected round LED Clock. It gets NTP time from the internet and translates to a 60 RGB WS2812B LED strip.

  If you have another orientation where the wire comes out then change the methods getLEDHour and getLEDMinuteOrSecond

  Happy programming, Leon van den Beukel, march 2019

  ---  
  NTP and summer time code based on:
  https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html 
  https://github.com/SensorsIot/NTPtimeESP/blob/master/NTPtimeESP.cpp (for US summer time support check this link)  
  
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#define DEBUG_ON

const char Version[] = "LCD CLOCK V1.06";
char *weekday[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char ssid[] = "Wifi Network Name";       // Your network SSID name here
const char pass[] = "Wifi Network Password";   // Your network password here
unsigned long timeZone = 7.0;         // Change this value to your local timezone (in my case +7 for Vietnam)
unsigned int LCD_Delay;

#define NUM_LEDS 60   // number of CRGB LEDS (60 for 60 sec/min)  
#define DATA_PIN 12   // pin to drive Din of CRGB
#define Din_Offset 30 //offset at 00,30 for Din of CRGB at 0 or 30 minute marks

// Change the colors here if you want.
// Check for reference: https://github.com/FastLED/FastLED/wiki/Pixel-reference#predefined-colors-list
// You can also set the colors with RGB values, for example red:
// CRGB colorHour = CRGB(255, 0, 0);  or CRGB::Red;
CRGB colorHour = CRGB(1,00,00);
CRGB colorMinute = CRGB(0,1,00);
CRGB colorSecond = CRGB(1,1,0);
CRGB colorMarks5 = CRGB(0,0,5);
CRGB colorMarks15 = CRGB(0,0,3);
CRGB colorHour_day = CRGB(128,0,0);
CRGB colorMinute_day = CRGB(0,128,0);
CRGB colorSecond_day = CRGB(0,0,128);
CRGB colorMarks5_day = CRGB(16,0,16);
CRGB colorMarks15_day = CRGB(32,0,32);
CRGB colorHour_night = CRGB(10,0,0);
CRGB colorMinute_night = CRGB(0,10,0);
CRGB colorSecond_night = CRGB(0,0,10);
CRGB colorMarks5_night = CRGB(3,0,3);
CRGB colorMarks15_night = CRGB(6,0,6);

//CRGB LEDs[NUM_LEDS];
CRGBArray<NUM_LEDS>LEDs;
 
ESP8266WiFiMulti wifiMulti;                     
WiFiUDP UDP;                                    
IPAddress timeServerIP;                         
const char* NTPServerName = "pool.ntp.org";    //time.nist.gov or pool.ntp.org
const int NTP_PACKET_SIZE = 48;                 
byte NTPBuffer[NTP_PACKET_SIZE];                

unsigned long intervalNTP = 1 * 60000; // Request NTP time every 1 minutes
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t      timeUNIX = 0;
unsigned long prevActualTime = 0;

#define LEAP_YEAR(Y) ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )
static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct DateTime {
  int  year;
  byte month;
  byte day;
  byte hour;
  byte minute;
  byte second;
  byte dayofweek;
  };

DateTime currentDateTime;

LiquidCrystal_PCF8574 lcd(0x27); 


void setup() {
  int error;

  Serial.begin(115200);
  
  Serial.println("LCD...");
  while (! Serial);
  Serial.println("Checking for LCD");  // See http://playground.arduino.cc/Main/I2cScanner
  Wire.begin();
  Wire.beginTransmission(0x27);
  error = Wire.endTransmission();
  Serial.print("Error: ");
  Serial.print(error);
  if (error == 0) {
    Serial.println(": LCD found.");
    lcd.begin(16, 2); // initialize the lcd
    lcd.setBacklight(255);
    lcd.home(); lcd.clear();
    lcd.print(Version);
    LCD_Delay = 1000;                   // 1 sec delay to read LCD
  } else {
    Serial.println(": LCD not found.");
    LCD_Delay = 0;                      // no Delay needed no LCD
  }

  
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);  
  Serial.println();
  Serial.println("LED_CLOCK_V1.06");   //File name of Project
  Serial.print("Din Pin:\t\t");
  Serial.println(DATA_PIN);   //Din Data pin
  Serial.print("Din Offset:\t");
  Serial.println(Din_Offset);  
 // FastLED.delay(3000);
  

  startWiFi();
  startUDP();

  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { 
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
   
  Serial.print("Time server:\t");
  Serial.println(NTPServerName);
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  Serial.print("Sending Initial NTP request.");
  Serial.println();
   if (LCD_Delay!=0){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(NTPServerName);
    lcd.setCursor(0, 1);
    lcd.print(timeServerIP);
   }
  sendNTPpacket(timeServerIP);  
  delay(LCD_Delay);
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // If a minute has passed since last NTP request
    prevNTP = currentMillis;
    Serial.print("\t");
    Serial.print("NTP request: ");
    sendNTPpacket(timeServerIP);               // Send an NTP request
  }

  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time;
    // Serial.print(" NTP response:\t");
    Serial.print("\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
    } 
  else if ((currentMillis - lastNTPResponse) > 3600000) {
    Serial.println("More than 1 hour since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse)/1000;
  if (actualTime != prevActualTime && timeUNIX != 0) { // If a second has passed since last update
    prevActualTime = actualTime;
    convertTime(actualTime);

    if (analogRead(A0)>15){               //read Light dependant resitor(LDR) connected to ADC display dim if night
      colorHour = colorHour_night;        // Red night
      colorMinute = colorMinute_night;    // Green night
      colorSecond = colorSecond_night;    // Blue night
      colorMarks15 = colorMarks15_night;  // Purple night
      colorMarks5 = colorMarks5_night;    // Purple night
          
      if (getLEDHour(currentDateTime.hour)==getLEDMinuteOrSecond(currentDateTime.minute)&&((currentDateTime.second)%2==0)){    //if hour and minut use same LED 
         colorHour = colorMinute_night;                     //odd sec display minute color                                                                   //odd sec display minute color
            }else{  
            colorHour = colorHour_night;                    //even sec display hour                                                                          //even sec display hour                                                           
            }    
      }
   
     
      else {                            //display bright if day                     
      colorHour = colorHour_day;        // Red day  
      colorMinute = colorMinute_day;    // Green day
      colorSecond = colorSecond_day;    // Blue day
      colorMarks15 = colorMarks15_day;  // Purple day
      colorMarks5 = colorMarks5_day;    // Purple day
            
        if ((getLEDHour(currentDateTime.hour)==getLEDMinuteOrSecond(currentDateTime.minute))&&((currentDateTime.second)%2==0)){ //if hour and minut use same LED 
              colorHour = colorMinute_day;                      //odd sec display minute color
                  }else{
                  colorHour = colorHour_day;                    //even sec display hour
                  }  
      }
   
  
    for (int i=0; i<NUM_LEDS; i++) 
      if ((i%5==0) && (i%15==0))  
        LEDs[i] = colorMarks15;          //mark indices at every 15 minutes
      else
        if ((i%5==0)&&(i%15!=0))  
          LEDs[i] = colorMarks5;         //mark indices at every 5 minutes
        else 
          LEDs[i] = CRGB::Black;

   
    LEDs[getLEDMinuteOrSecond(currentDateTime.second)]  = colorSecond;
    LEDs[getLEDMinuteOrSecond(currentDateTime.minute)]  = colorMinute;    
    LEDs[getLEDHour(currentDateTime.hour)]              = colorHour;
    FastLED.show();
  }  
}

byte getLEDHour(byte hours) {         //make 13 to 24 time same as 1 to 12 time
    if (hours > 12)                 
        hours -= 12;
    if (hours <= 5)                     // used for connecting Din of LEDS at 6'oclock 
    hours = hours * 5 + Din_Offset;     //not needed if Din at 12'oclock
  else
    hours = hours * 5 - Din_Offset;

 if (currentDateTime.minute < 12)    
    return (hours); 
 if (currentDateTime.minute < 24)   //inc hour led by 1/5 
    return (hours +=1); 
 if (currentDateTime.minute < 36)   //inc hour led by 2/5 
    return (hours +=2); 
 if (currentDateTime.minute < 48)   //inc hour led by 3/5  
    return (hours +=3); 
 if (currentDateTime.minute < 60)    //inc hour led by 4/5 
    return (hours +=4); 
}

byte getLEDMinuteOrSecond(byte minuteOrSecond) {    // 0 offset just returns same value of minuteorsecond
  if (minuteOrSecond < Din_Offset)                  // used for calculating Din of LEDS at 30 min
      return minuteOrSecond + Din_Offset;           // add 30 or 0 value of offset 
    else 
      return minuteOrSecond - Din_Offset;           // sub 30 or 0 value of offset

}

void startWiFi() { 
  wifiMulti.addAP(ssid, pass);   

  Serial.println("Connecting");
   
   
    
    while (wifiMulti.run() != WL_CONNECTED) {   //show indication of wifi onnection
        for (int i=0; i < NUM_LEDS; i++) {
          static uint8_t hue=0;  //define hue variable
          LEDs.fill_rainbow(hue++); //fill all leds with rainbow
          FastLED.delay(5); //control speed of change
      Serial.print('.');
        if (LCD_Delay!=0){                          //if no LCD skip                                 
          if (i<=16){ 
          lcd.setCursor(i,1);              
          lcd.print(".");
          }
        }
/*
     //neopixel startup test   
      if (i <60)
      LEDs[i] = CRGB((i-49),0,(i-49));    //purple
      if (i <50)
      LEDs[i] = CRGB(0,(i-39),(i-39));    //cyan
      if (i <40)
      LEDs[i] = CRGB((i-29),(i-29),0);    //yellow
      if (i <30)
      LEDs[i] = CRGB(0,0,(i-19));         //blue
      if (i <20)
      LEDs[i] = CRGB(0,(i-9),0);          //Green 
      if (i <10)                                    
      LEDs[i] = CRGB((i+1),0,0);          //red 
 
      FastLED.show();
     */    
     }
    }
  Serial.println(); 
  Serial.print("Connected to:\t");
  Serial.println(WiFi.SSID());             
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());            
  Serial.println();
  if (LCD_Delay!=0){ 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(WiFi.SSID());      
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());     
    delay(LCD_Delay);
  }   
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
 }

uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

void convertTime(uint32_t time) {
 
  time += (3600 * timeZone); // Correct time zone
  
  currentDateTime.second = time % 60;
  currentDateTime.minute = time / 60 % 60;
  currentDateTime.hour   = time / 3600 % 24;
  time  /= 60;  // To minutes
  time  /= 60;  // To hours
  time  /= 24;  // To days
  currentDateTime.dayofweek = ((time + 4) % 7);   // 01/01/1970 = thursday (+4)
  int year = 0;
  int days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // To days in this year, starting at 0  
  days = 0;
  byte month = 0;
  byte monthLength = 0;
  for (month = 0; month < 12; month++) {
    if (month == 1) { // February
      if (LEAP_YEAR(year)) {
        monthLength = 29;
      } else {
        monthLength = 28;
      }
    } else {
      monthLength = monthDays[month];
    }
  
    if (time >= monthLength) {
      time -= monthLength;
    } else {
      break;
    }
  }
 
  currentDateTime.day = time + 1;
  currentDateTime.year = year + 1970;
  currentDateTime.month = month + 1;  

 
  
// Correct European Summer time   (no DST in Vietnam all references commeted out.)
//  if (summerTime()) {
//    currentDateTime.hour += 1;
//  }

#ifdef DEBUG_ON
  
char buf[25];   //Buffwe ro show time on Serial monitor
char lcd1[16];  //Buffer to show LCD Line 1  
char lcd2[16];  //Buffer to show LCD Line 2

   sprintf(buf, "%03s %02d/%02d/%04d %02d:%02d:%02d ",weekday[currentDateTime.dayofweek], currentDateTime.day, currentDateTime.month, currentDateTime.year, currentDateTime.hour, currentDateTime.minute, currentDateTime.second); 
   sprintf(lcd2, " %03s %02d/%02d/%04d", weekday[currentDateTime.dayofweek], currentDateTime.day, currentDateTime.month, currentDateTime.year); 
   sprintf(lcd1, "    %02d:%02d:%02d", currentDateTime.hour, currentDateTime.minute, currentDateTime.second); 
   //Serial.print("\r"); //use for serial Date Time on same line (CR)
   Serial.print(buf);
   Serial.println();
   if (LCD_Delay!=0){ 
    lcd.setCursor(0, 0);  //LCD Line 1  (row/col)
    lcd.print(lcd1);
    lcd.setCursor(0, 1);  //LCD Line 2  (row/col)
    lcd.print(lcd2);
   }
#endif  
}

/*
boolean summerTime() {

  if (currentDateTime.month < 3 || currentDateTime.month > 10) return false;  // No summer time in Jan, Feb, Nov, Dec
  if (currentDateTime.month > 3 && currentDateTime.month < 10) return true;   // Summer time in Apr, May, Jun, Jul, Aug, Sep
  if (currentDateTime.month == 3 && (currentDateTime.hour + 24 * currentDateTime.day) >= (3 +  24 * (31 - (5 * currentDateTime.year / 4 + 4) % 7)) || currentDateTime.month == 10 && (currentDateTime.hour + 24 * currentDateTime.day) < (3 +  24 * (31 - (5 * currentDateTime.year / 4 + 1) % 7)))
  return true;
    else
  return false;
}
*/

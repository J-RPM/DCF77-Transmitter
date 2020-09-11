/*
        This code is created from the information of Andreas Spiess
        https://github.com/SensorsIot/DCF77-Transmitter-for-ESP32
                        https://www.sensorsiot.org/
                  https://www.youtube.com/c/AndreasSpiess/

                                 --- oOo ---
                                
                              Modified by: J_RPM
                               http://j-rpm.com/
                        https://www.youtube.com/c/JRPM
                              September of 2020

               An OLED display is added to show the DFC77 data,
                adding some new routines and modifying others.

                              >>> HARDWARE <<<
                  LIVE D1 mini ESP32 ESP-32 WiFi + Bluetooth
                https://es.aliexpress.com/item/32816065152.html
                
            HW-699 0.66 inch OLED display module, for D1 Mini (64x48)
               https://es.aliexpress.com/item/4000504485892.html

                           >>> IDE Arduino <<<
                        Model: WEMOS MINI D1 ESP32
       Add URLs: https://dl.espressif.com/dl/package_esp32_index.json
                     https://github.com/espressif/arduino-esp32

  To help you, I copy here the structure and name of the date and time variables

struct tm {
        int8_t          tm_sec;   /**< seconds after the minute - [ 0 to 59 ] 
        int8_t          tm_min;   /**< minutes after the hour - [ 0 to 59 ] 
        int8_t          tm_hour;  /**< hours since midnight - [ 0 to 23 ] 
        int8_t          tm_mday;  /**< day of the month - [ 1 to 31 ] 
        int8_t          tm_wday;  /**< days since Sunday - [ 0 to 6 ] 
        int8_t          tm_mon;   /**< months since January - [ 0 to 11 ] 
        int16_t         tm_year;  /**< years since 1900 
        int16_t         tm_yday;  /**< days since January 1 - [ 0 to 365 ] 
        int16_t         tm_isdst; /**< Daylight Saving Time flag 
    };
____________________________________________________________________________________
*/
#include <WiFi.h>
#include <Ticker.h>
#include <Time.h>
#include <Adafruit_GFX.h>
//*************************************************IMPORTANT******************************************************************
#include "Adafruit_SSD1306.h" // Copy the supplied version of Adafruit_SSD1306.cpp and Adafruit_ssd1306.h to the sketch folder
// Adafruit_SSD1306.h >>> #define SSD1306_64_48
// Adafruit_SSD1306.cpp >>> #if (SSD1306_LCDWIDTH == 64 && SSD1306_LCDHEIGHT == 48)
//*************************************************+++++++++******************************************************************
#define  OLED_RESET 0         
Adafruit_SSD1306 display(OLED_RESET);

String CurrentTime = "HH:MM:SS";
String CurrentDate = "dd/mm/yy";
String HWversion = "Rev. 1.0";

// Change to your WiFi credentials and select your time zone
const char* ssid = "YourOwnSSID";
const char* password = "YourSoSecretPassword";                          

#define DCF_PULSE 27      //This is the pin for OUTPUT Pulse DCF77
#define ANTENNAPIN 25     //Output RF Signal DCF77
#define CONTINUOUSMODE    //Uncomment this line to bypass de cron and have the transmitter on all the time
#define DIMMER 32         //This is the pin for Dimmer of display

// cron (if you choose the correct values you can even run on batteries)
// If you choose really bad this minutes, everything goes wrong, so minuteToWakeUp must be greater than minuteToSleep
#define minuteToWakeUp  55 // Every hoursToWakeUp at this minute the ESP32 wakes up get time and star to transmit
#define minuteToSleep    8 // If it is running at this minute then goes to sleep and waits until minuteToWakeUp

byte hoursToWakeUp[] = {0,1,2,3}; // you can add more hours to adapt to your needs
                      // When the ESP32 wakes up, check if the actual hour is in the list and
                      // runs or goes to sleep until next minuteToWakeUp if onTimeAfterReset = 0

Ticker tickerDecisec; // TBD at 100ms

//complete array of pulses for a minute
//0 = no pulse, 1=100ms, 2=200ms
int impulseArray[60];
int impulseCount = 0;
int actualHours, actualMinutes, actualSecond, actualDay, actualMonth, actualYear, DayOfW;
long dontGoToSleep = 0;
// If onTimeAfterReset > 0 and it is commented: #define CONTINUOUSMODE
// it will connect in the selected minute by minuteToWakeUp of all hours
const long onTimeAfterReset = 600000; // Ten minutes
int timeRunningContinuous = 0;

// https://www.ntppool.org/es/use.html                                              
const char* ntpServer = "es.pool.ntp.org"; // enter your closer pool or pool.ntp.org
// https://remotemonitoringsystems.ca/time-zone-abbreviations.php
const char* TZ_INFO   = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // enter your time zone 

struct tm timeinfo;

//#####################################################
//                   setup()
// Startup settings and date and time synchronization
//#####################################################
void setup() {
  pinMode(DIMMER, INPUT_PULLUP);       // Input of DIMMER
  display.dim(false);                  // Not dimmer at the init
  
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  Serial.begin(115200);
  Serial.println(F("*****************"));
  Serial.println(F("DCF77 transmitter"));
  Serial.println(F("*****************"));

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(2,0);   
  display.println(F("DCF77"));

  display.setTextSize(1);   
  display.setCursor(0,16);   
  display.println(String(ssid));
  display.print(F("Sync."));
  display.display();

  if (esp_sleep_get_wakeup_cause() == 0) dontGoToSleep = millis();

  ledcSetup(0, 77500, 8);        // Channel (0-15), Frequency (Hz), N. Bits (1-16)
  ledcAttachPin(ANTENNAPIN, 0);  // Pin, Channel (ANTENNAPIN, has to be attached to the antenna)
  ledcWrite(0, 127);             // Channel 0, carrier with 50% duty cycle (8 Bit)

  pinMode (DCF_PULSE, OUTPUT);
  digitalWrite (DCF_PULSE, LOW); // LOW level of DCF_PULSE at the Start
 
  WiFi_on();
  getNTP();
  WiFi_off();
  show_time();
  
  CodeTime(); // first conversion just for cronCheck
#ifndef CONTINUOUSMODE
  if ((dontGoToSleep == 0) or ((dontGoToSleep + onTimeAfterReset) < millis())) cronCheck(); // first check before start anything
#else
  Serial.println("CONTINUOUS MODE NO CRON!!!");
#endif

  // Important: this routine has to last less than 4 seconds, to be able to synchronize the start of the NTP seconds.
  DisplayHW();      // DCF77 time synchronized, show version on display

  // Sync to the start of a second
  Serial.print(F("Syncing... "));
  int startSecond = timeinfo.tm_sec+5;
  long count = 0;
  do {
    count++;
    if(!getLocalTime(&timeinfo) || count > 200000){
      Serial.println(F("Error obtaining time..."));
      delay(3000);
      ESP.restart();
    }
  } while (startSecond > timeinfo.tm_sec);

  tickerDecisec.attach_ms(100, DcfOut); // from now on calls DcfOut every 100ms

  Serial.print(F("Ok "));
  Serial.println(count);
}
//###########################
//        loop()
// Not used in this program
//###########################
void loop() {
  // There is no code inside the loop. This is a syncronous program driven by the Ticker
}
/////////////////////////////
// Coding of the DCF77 bits
/////////////////////////////
void CodeTime() {
  DayOfW = timeinfo.tm_wday;
  if (DayOfW == 0) DayOfW = 7;
  actualDay = timeinfo.tm_mday;
  actualMonth = timeinfo.tm_mon + 1;
  actualYear = timeinfo.tm_year - 100;
  actualHours = timeinfo.tm_hour;
  actualMinutes = timeinfo.tm_min + 1; // DCF77 transmitts the next minute
  if (actualMinutes >= 60) {
    actualMinutes = 0;
    actualHours++;
  }

  // Wait 1 minute until the day changes
  if (actualHours > 23) {
    errChangeOfDay();
  }
  
  actualSecond = timeinfo.tm_sec; 
  if (actualSecond == 60) actualSecond = 0;

  int n, Tmp, TmpIn;
  int ParityCount = 0;

  //we put the first 20 bits of each minute at a logical zero value
  for (n = 0; n < 20; n++) impulseArray[n] = 1;
  
  // set DST bit
  if (timeinfo.tm_isdst == 0) {
    impulseArray[18] = 2; // CET or DST OFF
  } else {
    impulseArray[17] = 2; // CEST or DST ON
  }
  
  //bit 20 must be 1 to indicate active time
  impulseArray[20] = 2;

  //calculates the bits for the minutes
  TmpIn = Bin2Bcd(actualMinutes);
  for (n = 21; n < 28; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  if ((ParityCount & 1) == 0)
    impulseArray[28] = 1;
  else
    impulseArray[28] = 2;

  //calculates bits for the hours
  ParityCount = 0;
  TmpIn = Bin2Bcd(actualHours);
  for (n = 29; n < 35; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  if ((ParityCount & 1) == 0)
    impulseArray[35] = 1;
  else
    impulseArray[35] = 2;
  ParityCount = 0;

  //calculate the bits for the actual Day of Month
  TmpIn = Bin2Bcd(actualDay);
  for (n = 36; n < 42; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  TmpIn = Bin2Bcd(DayOfW);
  for (n = 42; n < 45; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calculates the bits for the actualMonth
  TmpIn = Bin2Bcd(actualMonth);
  for (n = 45; n < 50; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calculates the bits for actual year
  TmpIn = Bin2Bcd(actualYear);   // 2 digit year
  for (n = 50; n < 58; n++) {
    Tmp = TmpIn & 1;
    impulseArray[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //equal date
  if ((ParityCount & 1) == 0)
    impulseArray[58] = 1;
  else
    impulseArray[58] = 2;

  //last missing pulse
  impulseArray[59] = 0; // No pulse
}
//################################
//     Bin2Bcd(int dato)
// Conversion from binary to bcd
//################################
int Bin2Bcd(int dato) {
  int msb, lsb;
  if (dato < 10)
    return dato;
  msb = (dato / 10) << 4;
  lsb = dato % 10;
  return msb + lsb;
}
//###########################
//         DcfOut()
// Sending the DCF77 signal 
//###########################
void DcfOut() {
  OledLight();        // Dimmer
 
  switch (impulseCount++) {
    case 0:                               
      // Start ASK modulation (NO carrier 77,5 KHz)
      if (impulseArray[actualSecond] != 0) {
        digitalWrite(DCF_PULSE, HIGH);        
        ledcWrite(0, 0);                  // Init of every second, NO carrier 77,5 KHz except #59  
      }
      // Update values on the OLED display
      UpdateLocalTime();
      display_time();
      break;
    case 1:
      // DCF77 BIT=0 >>> Pulse of 100mS >>> End ASK modulation
      if (impulseArray[actualSecond] == 1) {
        digitalWrite(DCF_PULSE, LOW);
        ledcWrite(0, 127);                 // Channel: 0, carrier with 50% duty cycle (8 Bit)
      }
      break;
    case 2:
      // DCF77 BIT=1 >>> Pulse of 200mS >>> End ASK modulation
      digitalWrite(DCF_PULSE, LOW);
      ledcWrite(0, 127);                   // Channel: 0, carrier with 50% duty cycle (8 Bit)
      EffectOled(1);
      display.display();
      break;
    case 9:
      // End of the seond
      impulseCount = 0;

      if (actualSecond == 0 ) Serial.print(F("M"));
      if (actualSecond == 1 || actualSecond == 15 || actualSecond == 21  || actualSecond == 29 ) Serial.print(F("-"));
      if (actualSecond == 36  || actualSecond == 42 || actualSecond == 45  || actualSecond == 50 ) Serial.print(F("-"));
      if (actualSecond == 28  || actualSecond == 35  || actualSecond == 58 ) Serial.print(F("P"));

      if (impulseArray[actualSecond] == 1) Serial.print(F("0"));
      if (impulseArray[actualSecond] == 2) Serial.print(F("1"));

      if (actualSecond == 59 ) {
        Serial.print(F("#"));
        Serial.println();
        Serial.println(F("------------------------------------------------------------"));
        show_time();
      #ifndef CONTINUOUSMODE
          if ((dontGoToSleep == 0) or ((dontGoToSleep + onTimeAfterReset) < millis())) cronCheck();
      #else
          Serial.println(F("CONTINUOUS MODE NO CRON!!!"));
          timeRunningContinuous++;
          if (timeRunningContinuous > 360) ESP.restart(); // 6 hours running, then restart all over
      #endif
      }
      break;
  } // End: switch (impulseCount++)
  
  if(!getLocalTime(&timeinfo)){
    Serial.println(F("Error obtaining time..."));
    delay(3000);
    ESP.restart();
  }
  CodeTime();
}
//#########################################
//         display_time()
// Updates the information on the display
//#########################################
void display_time() { 
  display.clearDisplay();
  display.setTextSize(1);   
  display.setCursor(8,0);  
  display.println(CurrentDate);
  
  display.setTextSize(2);   
  display.setCursor(1,9);  
  display.println(CurrentTime.substring(0,5));

  EffectOled(0);
  
  display.setCursor(4,26);  
  display.println(F("SEC   BIT"));

  display.setTextSize(2);   
  display.setCursor(1,34); 
  display.print( CurrentTime.substring(6,8) + " ");
  // Label of Bit
  if (actualSecond == 28  || actualSecond == 35  || actualSecond == 58 ) display.print(F("P"));
  else if (actualSecond == 0 ) display.print(F("M"));
  else if (actualSecond == 20 ) display.print(F("S"));
  else display.print(F("#"));
  // Level of Bit
  if (impulseArray[actualSecond] == 1) display.print(F("0"));
  else if (impulseArray[actualSecond] == 2) display.print(F("1"));
  else display.print(F("#"));
    
  display.display();
}
//#####################################
//        UpdateLocalTime()
// Updating the date and time strings
//#####################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  time(&now);
  //See http://www.cplusplus.com/reference/ctime/strftime/
  char output[30];
  strftime(output, 30, "%d/%m/%y", localtime(&now));
  CurrentDate = output;
  strftime(output, 30, "%H:%M:%S", localtime(&now));
  CurrentTime = output;
  return true;
}
//###################################
//  IpToString(IPAddress inIP)
// Converting the IP char to String
//###################################
String IpToString(IPAddress inIP) {
  char IPno[20];
  sprintf(IPno, "%d.%d.%d.%d", inIP[0], inIP[1], inIP[2], inIP[3]);
  return String(IPno);
}
//##################
//    WiFi_on()
// WiFi connection
//##################
void WiFi_on() {
  Serial.print(F("Connecting WiFi..."));
  WiFi.begin(ssid, password);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (counter >= 15) ESP.restart();
    Serial.print (F("."));
    display.setTextSize(1);   
    display.print(F("."));
    display.display();
    counter++;
  }
  Serial.println();
  Serial.print(F("WiFi connected to address: "));
  Serial.println(IpToString(WiFi.localIP()));
  }
//#####################
//     WiFi_off()
// WiFi disconnection
//#####################
void WiFi_off() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println(F("WiFi disconnected"));
  Serial.flush();
}
//#######################################
//         getNTP()
// Date and time update from NTP server
//#######################################
void getNTP() {
  Serial.print(F("GetNTP "));
  int i = 0;
  do {
    i++;
    if (i > 40) {
      ESP.restart();
    }
    configTime(0, 0, ntpServer);
    setenv("TZ", TZ_INFO, 1);
    delay(500);
  } while (!getLocalTime(&timeinfo));
  Serial.println("Ok");
}
//##########################################
//        show_time()
// Shows date and time through serial port
//##########################################
void show_time() {
  Serial.print(&timeinfo, "Now: %B %d, %Y - %H:%M:%S (%A) %Z ");
  if (timeinfo.tm_isdst == 0) {
    Serial.println(F("DST=OFF"));
  } else {
    Serial.println(F("DST=ON"));
  }
}
//###############################
// sleepForMinutes(int minutes)
// Time control for croncheck
//###############################
void sleepForMinutes(int minutes) {
  if (minutes < 2) return;
  uint64_t uSecToMinutes = 60000000;
  esp_sleep_enable_timer_wakeup(minutes * uSecToMinutes);  // this timer works on uSecs, so 60M by minute
  //WiFi_off();
  Serial.print(F("To sleep... "));
  Serial.print(minutes);
  Serial.println(F(" minutes"));
  Serial.flush();
  displaySleep(minutes);
  esp_deep_sleep_start();
}
//################################
//        cronCheck()
// Date and time update interval
//################################
void cronCheck() {
  // is this hour in the list?
  boolean work = false;
  for (int n = 0; n < sizeof(hoursToWakeUp); n++) {
    //Serial.println(sizeof(hoursToWakeUp)); Serial.print(work); Serial.print(" "); Serial.print(n); Serial.print(" "); Serial.print(actualHours); Serial.print(" "); Serial.println(hoursToWakeUp[n]);
    if ((actualHours == hoursToWakeUp[n]) or (actualHours == (hoursToWakeUp[n] + 1))){
      work = true;
      // is this the minute to go to sleep?
      if ((actualMinutes > minuteToSleep) and (actualMinutes < minuteToWakeUp)) {
        // go to sleep (minuteToWakeUp - actualMinutes)
        Serial.print(F("."));
        sleepForMinutes(minuteToWakeUp - actualMinutes);   
      }
      break;
    }
  }
  if (work == false) { // sleep until minuteToWakeUp (take into account the ESP32 can start for some reason between minuteToWakeUp and o'clock)
    if (actualMinutes >= minuteToWakeUp) {
        Serial.print(F(".."));
      sleepForMinutes(minuteToWakeUp + 60 - actualMinutes);
    } else {
      // goto sleep for (minuteToWakeUp - actualMinutes) minutes
        Serial.print(F("..."));
      sleepForMinutes(minuteToWakeUp - actualMinutes);        
    }
  }
}
//############################################
//              DisplayHW()
// Shows the hardware version on the display
//############################################
void DisplayHW() {
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(2,6);   
  display.println(F("DCF77"));

  display.setTextSize(1);   
  display.setCursor(14,24);   
  display.println(F("J_RPM"));
 
  display.setCursor(6,37);   
  display.println(HWversion);

  for (int i=0; i<8; i++) {
    display.invertDisplay(true);
    display.display();
    delay (150);
    display.invertDisplay(false);
    display.display();
    delay (150);
  }
}
//#############################################
//         displaySleep(int m)
// It shows on the display that it is stopped
//#############################################
void displaySleep(int m) {
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(2,8);   
  display.println(F("DCF77"));

  display.setTextSize(1);   
  display.setCursor(8,30);   
  display.println(F("Stopped"));
  display.print(m);
  display.println(" minutes");
  display.display();
}
//##################################
//         OledLight()
// OLED display brightness control
//##################################
void OledLight() {
 if(digitalRead (DIMMER)==HIGH) {
   digitalWrite (2, HIGH);
   display.dim(false);
 }else {
   digitalWrite (2, LOW);
   display.dim(true);
 }
}
//#################################
//       EffectOled(int e)
// Shows activity on OLED display
//#################################
void EffectOled(int e) {
  display.setTextSize(1);   
  display.setCursor(28,24);  

  if (actualSecond == 59  || e == 1) {
    display.write(3);
  }else {
    display.fillCircle(30, 27, 1, WHITE);
  }
}
//######################################
//        errChangeOfDay()
// Wait 1 minute until the day changes
//######################################
void errChangeOfDay() {
  tickerDecisec.detach();
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(2,0);   
  display.println(F("DCF77"));
  
  display.setTextSize(1);   
  display.setCursor(0,16);   
  display.println(F(" Waiting"));
  display.println(F(" for the"));
  display.println(F("change of"));
  display.println(F("   day!"));
  display.display();
  
  // Wait showing text for 50 seconds
  for (int i=0; i<100; i++) {
    display.invertDisplay(true);
    display.display();
    delay (250);
    display.invertDisplay(false);
    display.display();
    delay (250);
  }
  ESP.restart();
}
/////////// END ////////////////////////


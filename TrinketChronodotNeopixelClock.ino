// Note: Trinket/Gemma I2C pins are #0 for SDA and #2 for SCL
//       Will also require pullup resistors when using Chonodot since Trinket/Gemma
//       don't have internal pullup resistors. Using 2.2K resistors.
// Mod:  Updated for DST adjustment.
//       The DateTime for RTC should be set to UTC.
//       The steps that checks for DST are based on the variables that store the start and end of DST in UTC
// DST methods ported from the TimeZone library by Jack Christensen. https://github.com/JChristensen/Timezone

// Updated 7/12/14 - Attempting to correct display so that LEDs are more in sync with current time
// Changed delay so that second hand is smoother.
// Debug with software serial (not sure how it will fit. May have to comment out LEDs to use Serial output)

// Updated 7/14/14 - Corrected minute 59 and 0 line. Added animation for change in hour.
// Updated 7/15/14 - Attempt to make hourly animation fit with regular timekeeping by savind DST dates in PROGMEM.
// Everything fits in <5k with valid DST dates from 2014-2037.
// Updated 7/16/14 - Corrected formula so that 12pm displays at the correct location.
// Updated 8/4/114 - Adjusted start index for hour sweep and removed manual override for minute 59 at 0. (uploaded 8/5/14)

#include <Adafruit_NeoPixel.h>
#include <TinyWireM.h>
#include <TinyRTClib.h>
// For debug, uncomment serial code, use a FTDI Friend with its RX pin connected to Pin 3 
// You will need a terminal program (such as freeware PuTTY for Windows) set to the
// USB port of the FTDI friend at 9600 baud.  Uncomment out Serial commands to see what's up
//#include <SendOnlySoftwareSerial.h>  // See http://forum.arduino.cc/index.php?topic=112013.0

#define LED_PIN 1
#define FIRST_DAY_WK 1       // Sunday = 1
#define SECS_PER_DAY  (86400UL)
#define localOffset 5

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(36, LED_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS3231 rtc;              // Set up real time clock
uint32_t dstUTC, stdUTC;     // Save start and end dates for specific year
boolean dst;                 // boolean value for calculation DST offset (0 = STD, 1 = DST)
//uint8_t localOffset;         // Standard time offset from UTC for local time
uint16_t currYear;           // keep track of current year to limit the number of times to set dstUTC and stdUTC
//uint32_t second_color = strip.Color(75,0,0); //slightly yellower
//uint32_t hour_color   = strip.Color(0,0,100); //Blue
//uint32_t minute_color = strip.Color(0,75,0); //Green
//SendOnlySoftwareSerial Serial(3);  // Serial transmission on Trinket Pin 3

// store color values in program memory for blend sweep
static const uint32_t PROGMEM colors[] = {
  0x0021DE, 0x0042BD, 0x00609F, 0x00817E, 0x00A25D, 0x00C03F, 
  0x00E11E, 0x00FF00, 0x1EE100, 0x3FC000, 0x609F00, 0x7E8100, 
  0x9F6000, 0xC03F00, 0xDE2100, 0xFF0000, 0xDE0021, 0xC0003F, 
  0x9F0060, 0x7E0081, 0x60009F, 0x3F00C0, 0x1E00E1, 0x0000FF
};

// store DST change dates to save program compile size (Years 2014 - 2037)
static const uint32_t PROGMEM DST[] = {
  0x531C1170, 0x54FBF370, 0x56E50FF0, 0x58C4F1F0, 0x5AA4D3F0, 0x5C84B5F0, 
  0x5E6497F0, 0x604DB470, 0x622D9670, 0x640D7870, 0x65ED5A70, 0x67CD3C70, 
  0x69AD1E70, 0x6B963AF0, 0x6D761CF0, 0x6F55FEF0, 0x7135E0F0, 0x7315C2F0, 
  0x74FEDF70, 0x76DEC170, 0x78BEA370, 0x7A9E8570, 0x7C7E6770, 0x7E5E4970
};

// store DST change dates to save program compile size (Years 2014 - 2037)
static const uint32_t PROGMEM STD[] = {
  0x5455C860, 0x5635AA60, 0x581EC6E0, 0x59FEA8E0, 0x5BDE8AE0, 0x5DBE6CE0, 
  0x5F9E4EE0, 0x61876B60, 0x63674D60, 0x65472F60, 0x67271160, 0x6906F360, 
  0x6AE6D560, 0x6CCFF1E0, 0x6EAFD3E0, 0x708FB5E0, 0x726F97E0, 0x744F79E0, 
  0x76389660, 0x78187860, 0x79F85A60, 0x7BD83C60, 0x7DB81E60, 0x7F980060
};

void setup() {
  strip.begin();
  strip.show();              // Initialize all pixels to 'off'
  
  TinyWireM.begin();         // initialize I2C lib
  rtc.begin();               // Begin DS1307 real time clock
  //Serial.begin(9600);        // Begin Serial Monitor at 9600 baud
  
  //if (! rtc.isrunning()) {      // Uncomment lines below first use of clock to set time
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(__DATE__, __TIME__));
  //}
/*
  DateTime now = rtc.now();       // Get the RTC info
  setDstDates(now.year());        // initialize dates for time change
  currYear = now.year();          // initialize current year
*/
}

void loop() {
  DateTime now = rtc.now();
  if (currYear != now.year()) // When year changes update dstUTC and stdUTC
  {
    setDstDates(now.year());  // set the DST start and end dates based on current year
    currYear = now.year();    // update current year
  }
  
  if (now.unixtime() > dstUTC && now.unixtime() < stdUTC) {
    dst = true;
  }
  else {
    dst = false;
  }
  
  uint8_t hours = ((24+now.hour()+(dst*1)-localOffset)%24);  // Adjust hours based on DST
 
  if (hours >= 12) hours -= 12; // change to 12 hour clock (changed to >= so that 12pm displays on LED 0)
  
  //int LED_Min = map(now.minute(),0,59,12,35); // Note the map() function rounds numbers down which can cause the minutes to look slow
  uint8_t LED_Min;
  if (now.minute() == 0) LED_Min = 12; // Set minute 0 to 12 (0 minutes)
  else if (now.minute() == 59) LED_Min = 35; // Set minute 59 to 35
  else LED_Min = map(now.minute(),0,59,12,35)+1;
  
  //int LED_Sec = map(now.second(),0,59,12,35);
  uint8_t LED_Sec;
  //uint8_t offset;
  if ((now.minute() & 1) == 0) LED_Sec = ((now.second()+(0))%24)+12; //offset = 0;
  else if ((now.minute() & 1) == 1 && now.second() < 48) LED_Sec = ((now.second()+(12))%24)+12; //offset = 12;
  else LED_Sec = ((now.second()+(36))%24)+12; //offset = 36;
  
  // Sweep animation for change of hour
  //if (now.second() == 0) colorWipe(hours); // Test colorWipe function
  if (now.minute() == 0 && now.second() == 0) colorWipe(hours);
  
  for (int i = 0; i<36; i++)
  {
    if (i == hours)
      strip.setPixelColor(hours, 0x64);         //strip.Color(0,0,100));
    else if (i == LED_Sec && i == LED_Min)
        strip.setPixelColor(LED_Sec, 0x4b4b00); //strip.Color(75,75,0));
    //  strip.setPixelColor(LED_Sec, blend(strip.Color(0,75,0), strip.Color(75,0,0))); // blend function takes too much memory??
    else if (i == LED_Min)
      strip.setPixelColor(LED_Min, 0x4b00);     //strip.Color(0,75,0));
    else if (i == LED_Sec)
      strip.setPixelColor(LED_Sec, 0x4b0000);   //strip.Color(75,0,0));
    else
      strip.setPixelColor(i, 0);
  }
  
  strip.show(); 
  delay(25); // was 250. Caused occasional hesitations in changing of seconds.
  
  /*
  // Uncomment out this and other serial code to check that your clock is working.
  if ((now.second()%5) == 0) { 
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.print(" UTC - ");
    //hours = ((24+hours+(dst*1)-localOffset)%24);  // Adjust hours based on DST
    Serial.print(hours, DEC);
    Serial.print(':');
    Serial.print(LED_Min, DEC);
    Serial.print(':');
    Serial.print(LED_Sec, DEC);
    if (dst) Serial.print(" EDT");
    else  Serial.print(" EST");
    Serial.println();
  }*/
}
/* Original method that utilizaes function
// Calculate the start and end of DST in UTC
void setDstDates(uint16_t yr) 
{
  //Mar 9 at 07:00:00 UTC Sun, Mar 9 at 2:00 AM Eastern - 2014
  dstUTC   = toTime_t( yr, 3, 2, 7);
  //Nov 2 at 06:00:00 UTC Sun, Nov 2 at 2:00 AM Eastern - 2014
  stdUTC   = toTime_t( yr, 11, 1, 6);
}
*/
// Calculate the start and end of DST in UTC
void setDstDates(uint16_t yr) 
{
  //Mar 9 at 07:00:00 UTC Sun, Mar 9 at 2:00 AM Eastern - 2014
  dstUTC   = pgm_read_dword(&DST[yr-2014]);
  //Nov 2 at 06:00:00 UTC Sun, Nov 2 at 2:00 AM Eastern - 2014
  stdUTC   = pgm_read_dword(&STD[yr-2014]);
}

/* original method that calculates DST time changes based on formula
uint32_t toTime_t(uint16_t yr, uint8_t mon, uint8_t wk, uint8_t hr)
{
  DateTime tm;
  uint32_t t;
  uint8_t m, w;            // temp copies of mon and wk

  m = mon;
  w = wk;
  if (w == 0) {            //Last week = 0
      if (++m > 12) {      //for "Last", go to the next month
          m = 1;
          yr++;
      }
      w = 1;               //and treat as first week of next month, subtract 7 days later
  }

  tm = DateTime(yr, m, 1, hr, 0, 0); // temporarily set DateTime to first day of month.
  t = tm.unixtime();        //first day of the month, or first day of next month for "Last" rules
  // next step calculates the day of the week for DST time change and adds 86400 SECONDS_PER_DAY
  t += (7 * (w - 1) + (FIRST_DAY_WK - (tm.dayOfWeek()+1) + 7) % 7) * SECS_PER_DAY; // dayOfWeek() returns 0-6
  if (wk == 0) t -= 7 * SECS_PER_DAY;    //back up a week if this is a "Last" rule
  return t;
  
}
*/
// method adds a bit of overhead to the code
uint32_t blend (uint32_t color1, uint32_t color2)
{
  uint8_t r1,g1,b1;
  uint8_t r2,g2,b2;
 
  r1 = (uint8_t)(color1 >> 16),
  g1 = (uint8_t)(color1 >>  8),
  b1 = (uint8_t)(color1 >>  0);
 
  r2 = (uint8_t)(color2 >> 16),
  g2 = (uint8_t)(color2 >>  8),
  b2 = (uint8_t)(color2 >>  0);
 
 
  return strip.Color (constrain (r1+r2, 0, 255), constrain (g1+g2, 0, 255), constrain (b1+b2, 0, 255));
}

// Animation for change of hour
void colorWipe(uint8_t h) {
  for (uint8_t i = 0; i<36; i++) {
      strip.setPixelColor(i, 0);
  }
  strip.show();
  
  for(uint8_t i=12; i< 36; i++) {
    strip.setPixelColor(i, pgm_read_dword(&colors[i-12]));
    strip.show();
    delay(41);
  }
  delay(250);
  
  // sweep hours up to current hour
  if (h == 0) {h=12;} // Special case for noon and midnight
  
  for(uint8_t i=1; i<=h; i++) { // Start at 1 o'clock
    strip.setPixelColor((i%12), 0x64); // Hours: 1,2,3,4,5,6,7,8,9,10,11,0
    strip.show();
    delay(41); // 1000/24 = 41.66 ms
  }
//  for(uint8_t i=0; i<=h; i++) { // Original version
//    strip.setPixelColor(i, 0x64);
//    strip.show();
//    delay(41); // 1000/24 = 41.66 ms
//  }
  delay(1000);
  
  for (uint8_t i = 0; i<36; i++) {
      strip.setPixelColor(i, 0);
      delay(20);
      strip.show();
  }
}

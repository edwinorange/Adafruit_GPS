/***********************************
This is a our GPS library

Adafruit invests time and resources providing this open source code, 
please support Adafruit and open-source hardware by purchasing 
products from Adafruit!

Written by Limor Fried/Ladyada  for Adafruit Industries.  
BSD license, check license.txt for more information
All text above must be included in any redistribution
****************************************/

#include <Adafruit_GPS.h>

// how long are max NMEA lines to parse?
#define MAXLINELENGTH 120

// we double buffer: read one line in and leave one for the main program
volatile char line1[MAXLINELENGTH];
volatile char line2[MAXLINELENGTH];
// our index into filling the current line
volatile uint8_t lineidx=0;
// pointers to the double buffers
volatile char *currentline;
volatile char *lastline;
volatile boolean recvdflag;


boolean Adafruit_GPS::parse(char *nmea) {
  // do checksum check

  // first look if we even have one
  if (nmea[strlen(nmea)-4] == '*') {
    uint16_t sum = parseHex(nmea[strlen(nmea)-3]) * 16;
    sum += parseHex(nmea[strlen(nmea)-2]);
    
    // check checksum 
    for (uint8_t i=1; i < (strlen(nmea)-4); i++) {
      sum ^= nmea[i];
    }
    if (sum != 0) {
      // bad checksum :(
      //return false;
    }
  }

  // look for a few common sentences
  if (strstr(nmea, "$GPGGA")) {
    // found GGA
    char *p = nmea;
    // get time
    p = strchr(p, ',')+1;
    float timef = atof(p);
    uint32_t time = timef;
    hour = time / 10000;
    minute = (time % 10000) / 100;
    seconds = (time % 100);

    milliseconds = fmod(timef, 1.0) * 1000;

    // parse out latitude
    p = strchr(p, ',')+1;
    latitude = atof(p);

    p = strchr(p, ',')+1;
    if (p[0] == 'N') lat = 'N';
    else if (p[0] == 'S') lat = 'S';
    else if (p[0] == ',') lat = 0;
    else return false;

    // parse out longitude
    p = strchr(p, ',')+1;
    longitude = atof(p);

    p = strchr(p, ',')+1;
    if (p[0] == 'W') lon = 'W';
    else if (p[0] == 'E') lon = 'E';
    else if (p[0] == ',') lon = 0;
    else return false;

    p = strchr(p, ',')+1;
    fixquality = atoi(p);

    p = strchr(p, ',')+1;
    satellites = atoi(p);

    p = strchr(p, ',')+1;
    HDOP = atof(p);

    p = strchr(p, ',')+1;
    altitude = atof(p);
    p = strchr(p, ',')+1;
    p = strchr(p, ',')+1;
    geoidheight = atof(p);
    return true;
  }
  if (strstr(nmea, "$GPRMC")) {
   // found RMC
    char *p = nmea;

    // get time
    p = strchr(p, ',')+1;
    float timef = atof(p);
    uint32_t time = timef;
    hour = time / 10000;
    minute = (time % 10000) / 100;
    seconds = (time % 100);

    milliseconds = fmod(timef, 1.0) * 1000;

    p = strchr(p, ',')+1;
    // Serial.println(p);
    if (p[0] == 'A') 
      fix = true;
    else if (p[0] == 'V')
      fix = false;
    else
      return false;

    // parse out latitude
    p = strchr(p, ',')+1;
    latitude = atof(p);

    p = strchr(p, ',')+1;
    if (p[0] == 'N') lat = 'N';
    else if (p[0] == 'S') lat = 'S';
    else if (p[0] == ',') lat = 0;
    else return false;

    // parse out longitude
    p = strchr(p, ',')+1;
    longitude = atof(p);

    p = strchr(p, ',')+1;
    if (p[0] == 'W') lon = 'W';
    else if (p[0] == 'E') lon = 'E';
    else if (p[0] == ',') lon = 0;
    else return false;

    // speed
    p = strchr(p, ',')+1;
    speed = atof(p);

    // angle
    p = strchr(p, ',')+1;
    angle = atof(p);

    p = strchr(p, ',')+1;
    uint32_t fulldate = atof(p);
    day = fulldate / 10000;
    month = (fulldate % 10000) / 100;
    year = (fulldate % 100);

    // we dont parse the remaining, yet!
    return true;
  }

  return false;
}

char Adafruit_GPS::read(void) {
  char c = 0;
  
  if (paused)
    return c;


  if (gpsSwSerial->available()) {
    c = gpsSwSerial->read();

    //Serial.print(c);

    if (c == '$') {
      currentline[lineidx] = 0;
      lineidx = 0;
    }
    if (c == '\n') {
      currentline[lineidx] = 0;

      if (currentline == line1) {
        currentline = line2;
        lastline = line1;
      } else {
        currentline = line1;
        lastline = line2;
      }
      

      //Serial.println("----");
      //Serial.println((char *)lastline);
      //Serial.println("----");
      lineidx = 0;
      recvdflag = true;
    }
    
    currentline[lineidx++] = c;
    if (lineidx >= MAXLINELENGTH)
      lineidx = MAXLINELENGTH-1;
  }
  return c;
}

// Constructor when using SoftwareSerial or NewSoftSerial
#if ARDUINO >= 100
Adafruit_GPS::Adafruit_GPS(SoftwareSerial *ser) {
#else
  Adafruit_GPS::Adafruit_GPS(NewSoftSerial *ser) {
#endif

  common_init();  // Set everything to common state, then...
  gpsSwSerial = ser; // ...override swSerial with value passed.

  recvdflag = false;
  paused = false;
  lineidx = 0;
  currentline = line1;
  lastline = line2;
  
  hour = minute = seconds = year = month = day = milliseconds = 0;
  latitude = longitude = geoidheight = altitude = 0;
  speed = angle = magvariation = HDOP = 0;
  lat = lon = mag = 0;
  fix = false;
  fixquality = satellites = 0;
}


void Adafruit_GPS::begin(uint16_t baud) 
{
  gpsSwSerial->begin(baud);
}

static uint16_t parsed[25];

uint8_t Adafruit_GPS::parseResponse(char *response) {
  uint8_t i;
  
  for (i=0; i<25; i++) parsed[i] = -1;

  response = strstr(response, ",");

  for (i=0; i<25; i++) {
    if (!response || (response[0] == 0) || (response[0] == '*')) 
      return i;
    
    response++;
    parsed[i]=0;
    while ((response[0] != ',') && (response[0] != '*') && (response[0] != 0)) {
      parsed[i] *= 10;
      char c = response[0];
      //Serial.print("("); Serial.write(c); Serial.print(")");
      if (isDigit(c))
        parsed[i] += c - '0';
      else
        parsed[i] = c;
      response++;
    }
    //Serial.print(i); Serial.print("   ");
    //Serial.println(parsed[i]);
    //Serial.println(response);
  }
  return i;
}

// Initialization code used by all constructor types
void Adafruit_GPS::common_init(void) {
  gpsSwSerial  = NULL;
  gpsHwSerial  = NULL;
}


void Adafruit_GPS::sendCommand(char *str) {
  gpsSwSerial->println(str);
}



boolean Adafruit_GPS::newNMEAreceived(void) {
  return recvdflag;
}


void Adafruit_GPS::pause(boolean p) {
  paused = p;
}

char *Adafruit_GPS::lastNMEA(void) {
  recvdflag = false;
  return (char *)lastline;
}


// read a Hex value and return the decimal equivalent
uint8_t Adafruit_GPS::parseHex(char c) {
    if (c < '0')
      return 0;
    if (c <= '9')
      return c - '0';
    if (c < 'A')
       return 0;
    if (c <= 'F')
       return (c - 'A')+10;
}

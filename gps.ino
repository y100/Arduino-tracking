#include <Adafruit_SleepyDog.h>
#include <TinyGPS++.h>
#include <EEPROM.h>

int ad1 = 1; //address for storing relay 1 status in the EEPROM
int ad2 = 2; //address for storing relay 2 status in the EEPROM

byte h; //
byte i; //

char BluetoothData; // For recieving BT input

// Config (Use APN corresponding to your service providers configs)
static String apn = "diginet";
static String loggingPassword = "track";
static String serverIP = "www.wmvnow.com";

int r1 = 2;  // RLY connected to pin 2
int r2 = 3;  // KEY connected to pin 3
int rstpin = 4;  // RST connected to pin 4

// Used baud rates (define based on used modules)
static const uint32_t SimBaudrate = 9600;
static const uint32_t GPSBaud = 9600;
static const uint32_t SerialBaudrate = 9600;

// How frequently we want to send the location (milliseconds)
static const unsigned long frequency = 10000;

// Maximum time to wait SIM module for response
static long maxResponseTime = 15000;

String responseString;
TinyGPSPlus gps;
unsigned long previous = 0;
unsigned long beltTime;

void setup()
{
  /*
    Start serial communications. We can only listen to one ss at a time so changing that
    between sim and gps as needed
  */
  Serial.begin(SerialBaudrate);
  Serial1.begin(SimBaudrate);
  Serial2.begin(GPSBaud);
  Serial3.begin(9600);         // start serial communication at 9600bps
  pinMode(r1 = 2, OUTPUT); // pin 2 as OUTPUT
  pinMode(r2 = 3, OUTPUT); // pin 3 as OUTPUT
  pinMode(rstpin = 4, OUTPUT); // pin 4 as RESET
  digitalWrite(rstpin = 4, HIGH); // disable RESET pin
  h = EEPROM.read(ad1);
  i = EEPROM.read(ad2);
  bluetoothstatus();
  gprs();
}

void loop()
{
  bluetooth();
  // If we have data, decode and log the data
  while (Serial2.available() > 0)
    if (gps.encode(Serial2.read()))
      logInfo();

  // Test that we have had something from GPS module within first 10 seconds
  if (millis() - previous > 10000 && gps.charsProcessed() < 10)
  {
    Serial.println("GPS wiring error!");
    while (true);
  }
}

void logInfo()
{
  // Causes us to wait until we have satelite fix
  if (!gps.location.isValid())
  {
    Serial.println("Not a valid location. Waiting for satelite data.");
    return;
  }

  // Only log once per frequency
  if (millis() - previous > frequency)
  {
    previous = millis();
    String url = "AT+HTTPPARA=\"URL\",\"http://";
    url += serverIP;
    url += "/map/log.php?key=";
    url += loggingPassword;
    url += "&coordinates=";
    url += String(gps.location.lat(), DEC);
    url += ",";
    url += String(gps.location.lng(), DEC);
    url += "\"";
    Serial1.println(url);
    waitUntilResponse("OK");
    Serial1.println("AT+HTTPACTION=0");
    waitUntilResponse("+HTTPACTION:");
    return;
  }
}

void gprs()
{
  Serial.println("Waiting for init");
  // Wait few seconds so that module is able to take AT commands
  delay(10000);
  Serial.println("Init... waiting until module has connected to network");

  // Start AT communication. This sets auto baud and enables module to send data
  Serial1.println("AT");
  // Wait until module is connected and ready
  waitUntilResponse("OK");

  // Full mode
  Serial1.println("AT+CFUN=1");
  waitUntilResponse("OK");

  // Set credentials (TODO username and password are not configurable from variables). This may work without CSTT and CIICR but sometimes it caused error without them even though APN is given by SAPBR
  Serial1.write("AT+CSTT=\"");
  Serial1.print(apn);
  Serial1.write("\",\"\",\"\"\r\n");
  waitUntilResponse("OK");

  // Connect and get IP
  Serial1.println("AT+CIICR");
  waitUntilResponse("OK");

  // Some more credentials
  Serial1.write("AT+SAPBR=3,1,\"APN\",\"");
  Serial1.print(apn);
  Serial1.write("\"\r\n");
  waitUntilResponse("OK");

  Serial1.println("AT+SAPBR=1,1");
  waitUntilResponse("OK");

  Serial1.println("AT+HTTPINIT");
  waitUntilResponse("OK");
  previous = millis();
  Serial.println("starting loop!");
}
/*
   If we have anything available on the serial, append it to response string
 * */
void tryToRead()
{
  while (Serial1.available())
  {
    char c = Serial1.read();  //gets one byte from serial buffer
    responseString += c; //makes the string readString
  }
}

/*
   Read from serial until we get response line ending with line separator
 * */
void readResponse()
{
  responseString = "";
  while (responseString.length() <= 0 || !responseString.endsWith("\n"))
  {
    tryToRead();

    if (millis() - beltTime > maxResponseTime)
    {
      return;
    }
  }
}
/*
    Read from SIM serial until we get known response. TODO error handling!
 * */
void waitUntilResponse(String response)
{
  beltTime = millis();
  responseString = "";
  String totalResponse = "";
  while (responseString.indexOf(response) < 0 && millis() - beltTime < maxResponseTime)
  {
    readResponse();
    totalResponse = totalResponse + responseString;
    Serial.println(responseString);
  }

  if (totalResponse.length() <= 0)
  {
    Serial.println("No response from the module. Check wiring, SIM-card and power!");
    return;
  }
  else if (responseString.indexOf(response) < 0)
  {
    Serial.println("Unexpected response from the module");
    Serial.println(totalResponse);
    Watchdog.enable(2000);
    digitalWrite(rstpin = 4, LOW);
    delay(1000);
    digitalWrite(rstpin = 4, HIGH);
    Watchdog.reset();
  }
}

void bluetooth()
{
  if (Serial3.available())
  {
    BluetoothData = Serial3.read();
    switch (BluetoothData)
    {
      // RELAY - 1
      case 'R' :
        digitalWrite(r1, HIGH);
        EEPROM.write(ad1, 1);
        break;
      case 'r' :
        digitalWrite(r1, LOW);
        EEPROM.write(ad1, 0);
        break;
      // RELAY - 2
      case 'K' :
        digitalWrite(r2, HIGH);
        EEPROM.write(ad2, 1);
        break;
      case 'k' :
        digitalWrite(r2, LOW);
        EEPROM.write(ad2, 0);
        break;
    }
  }
}

void bluetoothstatus()
{
  // RELAY - 1

  if (h == 1)
  {
    digitalWrite(r1, HIGH);
  }
  else if (h == 0)
  {
    digitalWrite(r1, LOW);
  }
  // RELAY - 2
  if (i == 1)
  {
    digitalWrite(r2, HIGH);
  }
  else if (i == 0)
  {
    digitalWrite(r2, LOW);
  }
}


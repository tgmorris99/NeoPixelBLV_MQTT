#include <Adafruit_NeoPixel.h>
#include <Bounce2.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // modified for larger buffer size
#if MQTT_MAX_PACKET_SIZE < 1024  // If the max message size is too small, throw an error at compile time. See PubSubClient.h line 26
#error "MQTT_MAX_PACKET_SIZE is too small in libraries/PubSubClient/src/PubSubClient.h at line 26, increase it to 1024"
#endif

// MQTT connection info
//#define REMOTE_SERVER
#if defined(REMOTE_SERVER)
const char* ssid          = "network-ssid";
const char* password      = "network-password";
const char* mqttServer    = "test.mosquitto.org";
#else
const char* ssid          = "network-ssid";
const char* password      = "network-password";
const char* mqttServer    = "octopi.local";
#endif
const int   mqttPort      = 1883;
const char* mqttUser      = ""; // fill in MQTT User if needed
const char* mqttPassword  = ""; // fill in MQTT Password if needed

// octoPrint MQTT information
const char*   TopicBase         = "octoPrint";
const char*   TopicEvent        = "octoPrint/event/#";
const char*   TopicTemperature  = "octoPrint/temperature/#";
const char*   TopicProgress     = "octoPrint/progress/printing/#";
const char*   MQTTClient        = "octoPrintClient";
unsigned long MQTTFlush;
unsigned long lastReconnectAttempt = 0;

#define MQTTDelay 5000  // number of milliseconds to ignore retained TopicEvent messages after startup
#define MQTTRetry 5000  // number of milliseconds to wait if the client.connect() fails

// MQTT Message Types
#define BED       1
#define EXTRUDER  2
#define PROGRESS  3
#define EVENT     4
#define NOMATCH   0

#define DIRECT_UPDATE // bypasses encoding/decoding a pseudo serial message

// DEBUG Control
// connection messages
//#define DEBUGLEVEL0_ACTIVE
//#define DEBUGLEVEL1_ACTIVE
//#define DEBUGLEVEL2_ACTIVE
// MQTT messages
//#define DEBUGLEVEL3_ACTIVE
//#define DEBUGLEVEL4_ACTIVE
// standalone testing (i.e. no wifi, MQTT)
//#define STANDALONE_TESTING

static uint32_t  ConvertColor(uint8_t red, uint8_t green, uint8_t blue) {
  return ((uint32_t)red << 16) | ((uint32_t)green <<  8) | blue;
}

// ***************************************
// ********** User-Config Begin **********

#define PIN_D2  4   // GPIO4
#define PIN_D3  0   // GPIO0 - 10k pull-up
#define PIN_D4  2   // GPIO2 - Built-in LED
#define PIN_D5  14  // GPIO14
#define PIN_D6  13  // GPIO13
#define PIN_D7  12  // GPIO12

// NeoPixels On-Off button - based on branch done by sadegroo
// Select NEITHER or ONE of these options to use an on-off button
// The code is set to have the Button LED illuminate when the printer display is turned off

// The default can be left with the momentary button selected as the default starting value is
// for the displays to be in the ON state. A button can then be added at a later date if desired.
#define ON_OFF_BUTTON_MOMENTARY
//#define ON_OFF_BUTTON_LATCHING
//#define ON_OFF_BUTTON_DEBUG

#if defined(ON_OFF_BUTTON_MOMENTARY) && defined(ON_OFF_BUTTON_LATCHING)
#error "Please choose only MOMENTARY or LATCHING button type, not both."
#endif

#if defined(ON_OFF_BUTTON_MOMENTARY) || defined(ON_OFF_BUTTON_LATCHING)
#define BUTTON_PIN  PIN_D3  // ON-OFF button
#define LED_PIN     PIN_D4  // LED for debounce check
bool OnOffState = true;     // default initial state
Bounce OnOffDebouncer = Bounce();
#endif

//Hotend
bool NeoPixelTempHotendActive = true; //NeoPixel showing the hotend temperature (true = activated / false = deactivated)
neoPixelType NeoPixelTempHotendType = NEO_GRB + NEO_KHZ800; //Neopixel type (do not change if using NeoPixel from the BOM)
uint16_t NeoPixelTempHotendCount = 16; //Count of Neopixel-LEDs (do not change if using Neopixel from the BOM)
uint8_t NeoPixeTempHotendArduinoPin = PIN_D7; //Arduino pin used to control the Neopixel
int NeoPixelTempHotendPixelOffset = 0; //Usually LED number one of the NeoPixel is being used as the start point. Using the offset you can move the start point clockwise (positive offset) or anti-clockwise (negative offset)
int NeoPixelTempHotendTempOffset = 0; //Minimum Temperature at which the first LED lights up
int NeoPixelTempHotendScale = 20; //Temperature steps at which the following LEDs light up
bool NeoPixelTempHotendAnimationActive = true; //Animation when the Hotend heats up (true = activated / false = deactivated)
uint8_t NeopixelTempHotendBrightness = 8; //Overall brightness of the Neopixel-LEDs
uint32_t NeoPixelTempHotendColorIdle = ConvertColor(255, 255, 255); //RGB values for specified status
uint32_t NeoPixelTempHotendColorHeatUp = ConvertColor(255, 64, 64); //RGB values for specified status
uint32_t NeoPixelTempHotendColorHeatUpDone = ConvertColor(255, 0, 0); //RGB values for specified status
uint32_t NeoPixelTempHotendColorCoolDown = ConvertColor(0, 0, 255); //RGB values for specified status
uint32_t NeoPixelTempHotendColorAnimation = ConvertColor(0, 0, 0); //RGB values for specified status

//Printer status
bool NeoPixelPrinterStatActive = true; //NeoPixel showing the printer status (true = activated / false = deactivated)
neoPixelType NeoPixelPrinterStatType = NEO_GRB + NEO_KHZ800; //Neopixel type (do not change if using NeoPixel from the BOM)
uint16_t NeoPixelPrinterStatCount = 16; //Count of NeoPixel-LEDs (do not change if using Neopixel from the BOM)
uint8_t NeoPixePrinterStatArduinoPin = PIN_D6; //Arduino pin used to control the Neopixel
int NeoPixelPrinterStatPixelOffset = 0; //Usually LED number one of the NeoPixel is being used as the start point. Using the offset you can move the startpoint clockwise (positive offset) or anti-clockwise (negative offset).
bool NeoPixelPrinterStatAnimationActive = true; ///Animation for print progress (true = activated / false = deactivated)
uint8_t NeopixelTempPrinterStatBrightness = 8; //Overall brightness of the Neopixel-LEDs
uint32_t NeoPixelPrinterStatColorIdle = ConvertColor(255, 255, 255); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorPrinting = ConvertColor(64, 255, 64); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorPrintingDone = ConvertColor(0, 255, 0); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorStopped = ConvertColor(0, 0, 255); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorConfiguring = ConvertColor(255, 255, 0); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorPaused = ConvertColor(160, 32, 240); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorBusy = ConvertColor(255, 255, 0); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorPausing = ConvertColor(160, 32, 240); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorResuming = ConvertColor(255, 255, 0); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorFlashing = ConvertColor(255, 255, 0); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorDefault = ConvertColor(255, 255, 255); //RGB values for specified status
uint32_t NeoPixelPrinterStatColorAnimation = ConvertColor(0, 0, 0); //RGB values for specified status

//Heatbed
bool NeoPixelTempHeatbedActive = true; //NeoPixel showing the heatbed temperature (true = activated / false = deactivated)
neoPixelType NeoPixelTempHeatbedType = NEO_GRB + NEO_KHZ800; //Neopixel type (do not change if using NeoPixel from the BOM)
uint16_t NeoPixelTempHeatbedCount = 16; //Count of NeoPixel-LEDs (do not change if using Neopixel from the BOM)
uint8_t NeoPixeTempHeatbedArduinoPin = PIN_D5; //Arduino pin used to control the Neopixel
int NeoPixelTempHeatbedPixelOffset = 0; //Usually LED number one of the NeoPixel is being used as the start point. Using the offset you can move the start point clockwise (positive offset) or anti-clockwise (negative offset)
int NeoPixelTempHeatbedTempOffset = 0; //Minimum Temperature at which the first LED lights up
int NeoPixelTempHeatbedScale = 10; //Temperature steps at which the following LEDs light up
bool NeoPixelTempHeatbedAnimationActive = true; //Animation when the Heatbed heats up (true = activated / false = deactivated)
uint8_t NeopixelTempHeatbedBrightness = 8; //Overall brightness of the Neopixel-LEDs
uint32_t NeoPixelTempHeatbedColorIdle = ConvertColor(255, 255, 255); //RGB values for specified status
uint32_t NeoPixelTempHeatbedColorHeatUp = ConvertColor(255, 64, 64); //RGB values for specified status
uint32_t NeoPixelTempHeatbedColorHeatUpDone = ConvertColor(255, 0, 0); //RGB values for specified status
uint32_t NeoPixelTempHeatbedColorCoolDown = ConvertColor(0, 0, 255); //RGB values for specified status
uint32_t NeoPixelTempHeatbedColorAnimation = ConvertColor(0, 0, 0); //RGB values for specified status

// ********** User-Config End **********
// *************************************

// WiFi & MQTT communication
WiFiClient espClient;
PubSubClient client(espClient);

// Initialize global variables
unsigned long SerialTimeout = 100; // this effectively now controls how fase the Neopixels can update
unsigned long NeopixelRefreshSpeed = 200;
unsigned long NeoPixelTimerRefresh = millis();

#if !defined(DIRECT_UPDATE)
String SerialMessage;
#endif

int SetTempHotend = 0;
int SetTempHeatbed = 0;

void NeoPixelComplete();
// NeoPattern Class - derived from the Adafruit_NeoPixel class
class NeoPatterns : public Adafruit_NeoPixel
{
  public:

    // Member Variables:

    // Constructor - calls base-class constructor to initialize strip
    NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*)())
      : Adafruit_NeoPixel(pixels, pin, type)
    {
    }
};

NeoPatterns NeoPixelTempHotend(NeoPixelTempHotendCount, NeoPixeTempHotendArduinoPin, NeoPixelTempHotendType, &NeoPixelComplete);
NeoPatterns NeoPixelPrinterStat(NeoPixelPrinterStatCount, NeoPixePrinterStatArduinoPin, NeoPixelPrinterStatType, &NeoPixelComplete);
NeoPatterns NeoPixelTempHeatbed(NeoPixelTempHeatbedCount, NeoPixeTempHeatbedArduinoPin, NeoPixelTempHeatbedType, &NeoPixelComplete);

class NeoPixelAnimation {
  public:
    int Position;
    int Position_Memory;
    int RangeBegin;
    int RangeEnd;
    int Running;
};
NeoPixelAnimation NeoPixelTempHotendAnimation;
NeoPixelAnimation NeoPixelPrinterStatAnimation;
NeoPixelAnimation NeoPixelTempHeatbedAnimation;

class PrinterInfoMessage {
  public:
    char Status;
    float ActTempHeatbed;
    float ActTempHotend;
    float ActiveTempHeatbed;
    float ActiveTempHotend;
    float StandbyTempHeatbed;
    float StandbyTempHotend;
    int HeaterStatusHeatbed;
    int HeaterStatusHotend;
    float FractionPrinted;
    bool UpdatePending;
};
PrinterInfoMessage Printer;
PrinterInfoMessage DebugPrinter;

class octoPrintStatus {
  public:
    char  Status;
    float ActualTempHeatbed;
    float ActualTempHotend;
    float ActiveTempHeatbed;
    float ActiveTempHotend;
    float StandbyTempHeatbed;
    float StandbyTempHotend;
    float TargetTempHeatbed;
    float TargetTempHotend;
    int   HeaterStatusHeatbed;
    int   HeaterStatusHotend;
    float FractionPrinted;
    bool  UpdatePending;
};
octoPrintStatus MQTTPrinter;

static int ConvertPosition2PixelIndex(int PixelCount, int PixelOffset, int Position) {
  if ((PixelCount - Position) + (PixelOffset * -1) >= PixelCount) {
    return (((PixelCount - Position) + (PixelOffset * -1)) - PixelCount);
  }
  else {
    if ((PixelCount - Position) + (PixelOffset * -1) >= 0) {
      return ((PixelCount - Position) + (PixelOffset * -1));
    }
    else {
      return (((PixelCount - Position) + (PixelOffset * -1)) + PixelCount);
    }
  }
}

#if !defined(DIRECT_UPDATE)
void AnalyzeSerialMessage() {
  Printer.UpdatePending = false;
  if (JsonParseRoot(SerialMessage, "status", 0) != "") {
    Printer.Status = JsonParseRoot(SerialMessage, "status", 0).charAt(0);
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "heaters", 1) != "") {
    Printer.ActTempHeatbed = JsonParseRoot(SerialMessage, "heaters", 1).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "heaters", 2) != "") {
    Printer.ActTempHotend = JsonParseRoot(SerialMessage, "heaters", 2).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "active", 1) != "") {
    Printer.ActiveTempHeatbed = JsonParseRoot(SerialMessage, "active", 1).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "active", 2) != "") {
    Printer.ActiveTempHotend = JsonParseRoot(SerialMessage, "active", 2).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "standby", 1) != "") {
    Printer.StandbyTempHeatbed = JsonParseRoot(SerialMessage, "standby", 1).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "standby", 2) != "") {
    Printer.StandbyTempHotend = JsonParseRoot(SerialMessage, "standby", 2).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "hstat", 1) != "") {
    Printer.HeaterStatusHeatbed = JsonParseRoot(SerialMessage, "hstat", 1).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "hstat", 2) != "") {
    Printer.HeaterStatusHotend = JsonParseRoot(SerialMessage, "hstat", 2).toFloat();
    Printer.UpdatePending = true;
  }
  if (JsonParseRoot(SerialMessage, "fraction_printed", 0) != "") {
    Printer.FractionPrinted = JsonParseRoot(SerialMessage, "fraction_printed", 0).toFloat();
    Printer.UpdatePending = true;
  }
#if defined(DEBUGLEVEL2_ACTIVE)
  if (Printer.UpdatePending == true) {
    Serial.println("StatusUpdate: Status= " + String(Printer.Status) + " / ActualTHb= " + Printer.ActTempHeatbed + " / ActualTHe= " + Printer.ActTempHotend + " / ActiveTHb= " + Printer.ActiveTempHeatbed + " / ActiveTHe= " + Printer.ActiveTempHotend + " / StandbyTHb= " + Printer.StandbyTempHeatbed + " / StandbyTHe= " + Printer.StandbyTempHotend + " / StatusHb= " + Printer.HeaterStatusHeatbed + " / StatusHe= " + Printer.HeaterStatusHotend + " / PrintProgress= " + Printer.FractionPrinted);
  }
  else {
    //        Serial.println("Error: JsonObjectValue");
  }
#endif
}
#endif

// Example-Json-Message: {"status":"I","heaters":[31.5,28.1],"active":[0.0,0.0],"standby":[0.0,0.0],"hstat":[0,0],"pos":[0.000,0.000,0.000],"machine":[0.000,0.000,0.000],"sfactor":100.00,"efactor":[100.00],"babystep":0.000,"tool":-1,"probe":"0","fanPercent":[0.0,0.0,100.0,100.0,0.0,0.0,0.0,0.0,0.0,0.0],"fanRPM":0,"homed":[0,0,0],"msgBox.mode":-1,"geometry":"coreXY","axes":3,"totalAxes":3,"axisNames":"XYZ","volumes":2,"numTools":1,"myName":"BLV mgn Cube","firmwareName":"RepRapFirmware for Duet 2 WiFi/Ethernet"}
String JsonParseRoot(String JsonMessage, String JsonRootObject, int JsonObjectIndex) {
  long PositionBegin = -1;
  long PositionEnd = -1;
  int JsonValueIndex = 0;
  String JsonValue = "";

  //Remove Quotes
  JsonMessage.replace("\"", "");

  PositionBegin = JsonMessage.indexOf(JsonRootObject + ":");
  //Object found?
  if (PositionBegin == -1) {
    return "";
  }
  else {
    PositionBegin = (PositionBegin + JsonRootObject.length()) + 1;
    if (JsonMessage.charAt(PositionBegin) == '[') {
      PositionEnd = JsonMessage.indexOf("]", PositionBegin);
      if (PositionEnd == -1) {
        //StartBracket without StopBracket found
        return "";
      }
      else {
        JsonValue = JsonMessage.substring((PositionBegin + 1), PositionEnd);
        if (JsonObjectIndex == 0) {
          PositionBegin = 0;
          if (JsonValue.indexOf(",", PositionBegin) == -1) {
            return JsonValue;
          }
          else {
            //SingleValue expected, but MultiValue found
            return "";
          }
        }
        else {
          PositionBegin = 0;
          if (JsonValue.indexOf(",", PositionBegin) == -1) {
            //MultiValue expected, but Singlevalue found
            return "";
          }
          else {
            for (int JsonValueIndex = 1; JsonValueIndex < JsonObjectIndex; JsonValueIndex++) {
              if (JsonValue.indexOf(",", PositionBegin) == -1) {
                //Fewer Values found than expected
                return "";
              }
              else {
                PositionBegin = JsonValue.indexOf(",", PositionBegin);
                PositionBegin++;
              }
            }
            String test = JsonValue.substring(PositionBegin, PositionBegin + 1);
            if (JsonValue.indexOf(",", PositionBegin) != -1) {
              PositionEnd = JsonValue.indexOf(",", PositionBegin);
            }
            else {
              PositionEnd = JsonValue.length();
            }
            JsonValue = JsonValue.substring(PositionBegin, PositionEnd);
            return JsonValue;
          }
        }
      }
    }
    else if (JsonObjectIndex != 0) {
      //Multivalue expected but not found
      return "";
    }
    else {
      if (JsonMessage.indexOf(",", PositionBegin) != -1) {
        PositionEnd = JsonMessage.indexOf(",", PositionBegin);
        JsonValue = JsonMessage.substring(PositionBegin, PositionEnd);
        return JsonValue;
      }
      else if (JsonMessage.indexOf("}", PositionBegin) != -1) {
        PositionEnd = JsonMessage.indexOf("}", PositionBegin);
        JsonValue = JsonMessage.substring(PositionBegin, PositionEnd);
        return JsonValue;
      }
      else {
        //No EndPoint , or } for SingleValue found
        return "";
      }
    }
  }
}

void GetSerialMessage() {
#if defined(DIRECT_UPDATE)
  // this is a direct update from the MQTT information w/o serializing/deserializing the information
  // MQTTPrinter information is filled in the background by the MQTT callback function
  Printer.Status = MQTTPrinter.Status;
  Printer.ActTempHeatbed = MQTTPrinter.ActualTempHeatbed;
  Printer.ActTempHotend = MQTTPrinter.ActualTempHotend;
  Printer.ActiveTempHeatbed = MQTTPrinter.ActiveTempHeatbed;
  Printer.ActiveTempHotend = MQTTPrinter.ActiveTempHotend;
  Printer.StandbyTempHeatbed = MQTTPrinter.StandbyTempHeatbed;
  Printer.StandbyTempHotend = MQTTPrinter.StandbyTempHotend;
  Printer.HeaterStatusHeatbed = MQTTPrinter.HeaterStatusHeatbed;
  Printer.HeaterStatusHotend = MQTTPrinter.HeaterStatusHotend;
  Printer.FractionPrinted = MQTTPrinter.FractionPrinted;
  Printer.UpdatePending = true;
#else
  SerialMessage = "";
  char JsonMessage[200];
  sprintf(JsonMessage, "(\"status\":\"%c\",\"heaters\":[%.1f,%.1f],\"active\":[%.1f,%.1f],\"standby\":[%.1f,%.1f],\"hstat\":[%d,%d],\"fraction_printed\":[%.2f]}",
          MQTTPrinter.Status,
          MQTTPrinter.ActualTempHeatbed, MQTTPrinter.ActualTempHotend,
          MQTTPrinter.ActiveTempHeatbed, MQTTPrinter.ActiveTempHotend,
          MQTTPrinter.StandbyTempHeatbed, MQTTPrinter.StandbyTempHotend,
          MQTTPrinter.HeaterStatusHeatbed, MQTTPrinter.HeaterStatusHotend,
          MQTTPrinter.FractionPrinted);

  //  delay(SerialTimeout);

  SerialMessage = JsonMessage;
  //  Serial.println(SerialMessage);
  AnalyzeSerialMessage();
#endif
}
void NeoPixelWalkingBit(unsigned PixelNumber, uint32_t PixelColor, int wait) {

  if (PixelNumber > 0 ) {
    // turn off the previous pixel
    NeoPixelTempHotend.setPixelColor((PixelNumber - 1) % NeoPixelTempHotendCount, ConvertColor(0, 0, 0));
    NeoPixelPrinterStat.setPixelColor((PixelNumber - 1) % NeoPixelPrinterStatCount, ConvertColor(0, 0, 0));
    NeoPixelTempHeatbed.setPixelColor((PixelNumber - 1) % NeoPixelTempHeatbedCount, ConvertColor(0, 0, 0));
  }

  // turn on the desired pixel
  NeoPixelTempHotend.setPixelColor(PixelNumber % NeoPixelTempHotendCount, PixelColor);
  NeoPixelPrinterStat.setPixelColor(PixelNumber % NeoPixelPrinterStatCount, PixelColor);
  NeoPixelTempHeatbed.setPixelColor(PixelNumber % NeoPixelTempHeatbedCount, PixelColor);

  // refresh the displays
  NeoPixelRefresh(true);
}

void NeoPixelRainbow(int wait) {
  int i, j, n;

  // use the largest number of pixels for the loop in case they're different for some reason
  n = max(max(NeoPixelTempHotendCount, NeoPixelPrinterStatCount), NeoPixelTempHeatbedCount);

  for (j = 0; j < 256; j++) {
    for (i = 0; i < n; i++) {
      if (i <= NeoPixelTempHotendCount) NeoPixelTempHotend.setPixelColor(i, Wheel(NeoPixelTempHotend, (i + j) & 255));
      if (i <= NeoPixelPrinterStatCount) NeoPixelPrinterStat.setPixelColor(i, Wheel(NeoPixelPrinterStat, (i + j) & 255));
      if (i <= NeoPixelTempHeatbedCount) NeoPixelTempHeatbed.setPixelColor(i, Wheel(NeoPixelTempHeatbed, (i + j) & 255));
    }

    // refresh the displays
    NeoPixelRefresh(true);

    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colors are a transition r - g - b - back to r.
uint32_t Wheel(Adafruit_NeoPixel &strip, byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void NeoPixelRefresh(bool MustShow) {
#if defined(ON_OFF_BUTTON_MOMENTARY) || defined(ON_OFF_BUTTON_LATCHING)
  if (OnOffState || MustShow) {
#endif
    if (NeoPixelTempHotendActive == true) NeoPixelTempHotend.show();
    if (NeoPixelPrinterStatActive == true) NeoPixelPrinterStat.show();
    if (NeoPixelTempHeatbedActive == true) NeoPixelTempHeatbed.show();
#if defined(ON_OFF_BUTTON_MOMENTARY) || defined(ON_OFF_BUTTON_LATCHING)
  }
  else {
    NeoPixelTempHotend.clear(); NeoPixelTempHotend.show();
    NeoPixelPrinterStat.clear(); NeoPixelPrinterStat.show();
    NeoPixelTempHeatbed.clear(); NeoPixelTempHeatbed.show();
  }
#endif
}

void  NeoPixelReset()
{
  // (re)Initialize Neopixels after WiFi & MQTT connect
  if (NeoPixelTempHotendActive == true) {
    NeoPixelTempHotend.begin();
    NeoPixelTempHotend.setBrightness(NeopixelTempHotendBrightness);
    NeoPixelTempHotend.fill(ConvertColor(0, 0, 0));
    NeoPixelTempHotend.show();
  }
  if (NeoPixelPrinterStatActive == true) {
    NeoPixelPrinterStat.begin();
    NeoPixelPrinterStat.setBrightness(NeopixelTempPrinterStatBrightness);
    NeoPixelPrinterStat.fill(ConvertColor(0, 0, 0));
    NeoPixelPrinterStat.show();
  }
  if (NeoPixelTempHeatbedActive == true) {
    NeoPixelTempHeatbed.begin();
    NeoPixelTempHeatbed.setBrightness(NeopixelTempHeatbedBrightness);
    NeoPixelTempHeatbed.fill(ConvertColor(0, 0, 0));
    NeoPixelTempHeatbed.show();
  }
}

boolean reconnect() { // MQTT (re)connect and subscribe to topics
#if defined(DEBUGLEVEL0_ACTIVE)
  Serial.print("Connecting to ");
  Serial.println(mqttServer);
#endif

  // need to ignore retained printer status messages when (re)connecting
  MQTTFlush = millis() + MQTTDelay;

  NeoPixelRainbow(10);
  NeoPixelReset();

#if defined(REMOTE_SERVER)
  if (client.connect(MQTTClient)) {
#else
  if (client.connect(MQTTClient, mqttUser, mqttPassword)) {
#endif
    // ... and (re)subscribe
#if defined(DEBUGLEVEL0_ACTIVE)
    Serial.println("Connected");
    Serial.println("Subscribing to topics");
#endif
    client.subscribe(TopicEvent);
    client.subscribe(TopicProgress);
    client.subscribe(TopicTemperature);
  }
  else
  {
#if defined(DEBUGLEVEL0_ACTIVE)
    Serial.print("failed with state ");
    Serial.println(client.state());
#endif
  }
  return client.connected();
}


// this examines the incoming MQTT packet and returns the topic to which it belongs
int parseTopic(char* topic, unsigned int length)
{
  int index;

  if (strncmp(topic, TopicEvent, strlen(TopicEvent) - 2) == 0) {
    return EVENT;
  }
  if (strncmp(topic, TopicProgress, strlen(TopicProgress) - 2) == 0) {
    return PROGRESS;
  }
  if (strncmp(topic, TopicTemperature, strlen(TopicTemperature) - 2) == 0) {
    for (int i = 0; i < length; i++) {
      if ((char)topic[i] == '/') {
        index = i;
      }
    }
    index++;  // index now points to the last element of the topic
    if (strcmp(&topic[index], "bed") == 0)
      return BED;
    else if (strcmp(&topic[index], "tool0") == 0)
      return EXTRUDER;
    else
      return NOMATCH;
  }
  return NOMATCH;
}

// handles the asynchronous communication from the MQTT server
void callback(char* topic, byte* payload, unsigned int length)
{
  int   retval;
  int   index = 0;
  char  tempstring[1056]; // 1024 plus a bit to handle the large progress payload

  for (int i = 0; i < length; i++) {
    tempstring[index++] = payload[i];
  }
  tempstring[index] = 0;

  if (millis() <  MQTTFlush) {  // ignore the initial retained messages
#if defined(DEBUGLEVEL4_ACTIVE)
    Serial.print("ignoring message: ");
    Serial.println(tempstring);
#endif
    return;
  }

#if defined(DEBUGLEVEL4_ACTIVE)
  Serial.println(tempstring);
#endif
#if defined(DEBUGLEVEL3_ACTIVE)
  Serial.print("Message arrived in topic(");
#endif
  switch (retval = parseTopic(topic, length)) {
    case BED:
      MQTTPrinter.ActualTempHeatbed = JsonParseRoot(tempstring, "actual", 0).toFloat();
      MQTTPrinter.TargetTempHeatbed = JsonParseRoot(tempstring, "target", 0).toFloat();
      MQTTPrinter.ActiveTempHeatbed = MQTTPrinter.TargetTempHeatbed;
      if (MQTTPrinter.TargetTempHeatbed == 0.0) {
        MQTTPrinter.HeaterStatusHeatbed = 0;  // heater is off
      }
      else if (MQTTPrinter.TargetTempHeatbed > 0.0) {
        MQTTPrinter.HeaterStatusHeatbed = 2;  // heater is active
      }
      else if (MQTTPrinter.ActualTempHeatbed < 0.0) {
        MQTTPrinter.HeaterStatusHeatbed = 3;  // heater fault
      }
#if defined(DEBUGLEVEL3_ACTIVE)
      Serial.print("BED: ");
      Serial.print(MQTTPrinter.ActualTempHeatbed);
      Serial.print(", ");
      Serial.print(MQTTPrinter.TargetTempHeatbed);
#endif
      break;
    case EXTRUDER:
      MQTTPrinter.ActualTempHotend = JsonParseRoot(tempstring, "actual", 0).toFloat();
      MQTTPrinter.TargetTempHotend = JsonParseRoot(tempstring, "target", 0).toFloat();
      MQTTPrinter.ActiveTempHotend = MQTTPrinter.TargetTempHotend;
      if (MQTTPrinter.TargetTempHotend == 0.0) {
        MQTTPrinter.HeaterStatusHotend = 0;   // heater is off
      }
      else if (MQTTPrinter.TargetTempHotend > 0.0) {
        MQTTPrinter.HeaterStatusHotend = 2;    // heater is in active
      }
      else if (MQTTPrinter.ActualTempHotend < 0.0) {
        MQTTPrinter.HeaterStatusHotend = 3;    // heater fault
      }
#if defined(DEBUGLEVEL3_ACTIVE)
      Serial.print("EXTRUDER: ");
      Serial.print(MQTTPrinter.ActualTempHotend);
      Serial.print(", ");
      Serial.print(MQTTPrinter.TargetTempHotend);
#endif
      break;
    case PROGRESS:
      // octoPrint reports %Complete as 0-100 while the expectation is 0.0 to 1.0
      MQTTPrinter.FractionPrinted = JsonParseRoot(tempstring, "progress", 0).toFloat() / 100.0;
#if defined(DEBUGLEVEL3_ACTIVE)
      Serial.print("PROGRESS: ");
      Serial.println(MQTTPrinter.FractionPrinted);
#endif
      break;
    case EVENT:
      // print job possibilities
      //    PRINT_STARTED = "PrintStarted"
      //    PRINT_DONE = "PrintDone"
      //    PRINT_FAILED = "PrintFailed"
      //    PRINT_CANCELLING = "PrintCancelling"
      //    PRINT_CANCELLED = "PrintCancelled"
      //    PRINT_PAUSED = "PrintPaused"
      //    PRINT_RESUMED = "PrintResumed"
      //    ERROR = "Error"

      // status:
      //    I=idle,
      //    P=printing from SD card,
      //    S=stopped (i.e. needs a reset),
      //    C=running config file (i.e starting up),
      //    A=paused,
      //    D=pausing,
      //    R=resuming from a pause,
      //    B=busy (e.g. running a macro),
      //    F=performing firmware update

#if defined(DEBUGLEVEL3_ACTIVE)
      Serial.print("EVENT");
#endif
      if (JsonParseRoot(tempstring, "_event", 0) == " PrintStarted") {
        MQTTPrinter.Status = 'P';
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " PrintDone") {
        MQTTPrinter.Status = 'I';
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " PrintFailed") {
        if (MQTTPrinter.Status != 'B') {  // if prior state was canceled then just leave that
          MQTTPrinter.Status = 'S';
        }
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " PrintCancelling") {
        MQTTPrinter.Status = 'B';
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " PrintCancelled") {
        MQTTPrinter.Status = 'B';
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " PrintPaused") {
        MQTTPrinter.Status = 'R';
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " PrintResumed") {
        MQTTPrinter.Status = 'P';
      }
      else if (JsonParseRoot(tempstring, "_event", 0) == " Error") {
        MQTTPrinter.Status = 'S';
      }
      else {
        MQTTPrinter.Status = 'I'; // no match so just call it OPERATIONAL
      }
      break;
    case NOMATCH:
#if defined(DEBUGLEVEL3_ACTIVE)
      Serial.print("!! NO MATCH !!");
#endif
      break;
    default:
#if defined(DEBUGLEVEL3_ACTIVE)
      Serial.print((float)retval);
#endif
      break;
  }
#if defined(DEBUGLEVEL3_ACTIVE)
  Serial.print("): ");
  Serial.println(topic);
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
#endif
}

// Initialize everything and prepare to start
void setup()
{
  unsigned walkingPixel = 0;

  // set up serial port for debug if needed
#if defined(DEBUGLEVEL0_ACTIVE) || defined(DEBUGLEVEL1_ACTIVE) || defined(DEBUGLEVEL2_ACTIVE) || defined(DEBUGLEVEL3_ACTIVE) || defined(DEBUGLEVEL4_ACTIVE) || defined(ON_OFF_BUTTON_DEBUG)
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }
#endif
#if defined(DEBUGLEVEL0_ACTIVE)
  Serial.println("\nStart");
#endif

#ifndef STANDALONE_TESTING
  // establish WiFi connection
  WiFi.begin(ssid, password);
#if defined(DEBUGLEVEL0_ACTIVE)
  Serial.print("Connecting to WiFi..");
#endif

  // reset / initialize the NeopPixels since we're going to use them
  NeoPixelReset();

  while (WiFi.status() != WL_CONNECTED) {
    NeoPixelWalkingBit(walkingPixel++, ConvertColor(0, 128, 0), 10);
    delay(250);
#if defined(DEBUGLEVEL0_ACTIVE)
    Serial.print(".");
#else
    ;
#endif
  }

#if defined(DEBUGLEVEL0_ACTIVE)
  Serial.println("\nConnected to the WiFi network");
#endif

  // set up the MQTT communication & flush timer
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

#if defined(DEBUGLEVEL0_ACTIVE)
  Serial.print("MQTT_MAX_PACKET_SIZE = ");
  Serial.println(MQTT_MAX_PACKET_SIZE);
#endif
#endif //STANDALONE_TESTING

  lastReconnectAttempt = 0;

#if defined(ON_OFF_BUTTON_MOMENTARY) || defined(ON_OFF_BUTTON_LATCHING)
  OnOffDebouncer.attach(BUTTON_PIN, INPUT_PULLUP); // Attach the debouncer to a pin with INPUT_PULLUP mode
  OnOffDebouncer.interval(40); // Use a debounce interval of 40 milliseconds
  pinMode(LED_PIN, OUTPUT); // Setup the LED
#endif

  //Default start values for animations
  NeoPixelTempHotendAnimation.Position = 0;
  NeoPixelTempHotendAnimation.Position_Memory = 0;
  NeoPixelTempHotendAnimation.RangeBegin = 0;
  NeoPixelTempHotendAnimation.RangeEnd = 0;
  NeoPixelTempHotendAnimation.Running = false;

  NeoPixelPrinterStatAnimation.Position = 0;
  NeoPixelPrinterStatAnimation.Position_Memory = 0;
  NeoPixelPrinterStatAnimation.RangeBegin = 0;
  NeoPixelPrinterStatAnimation.RangeEnd = 0;
  NeoPixelPrinterStatAnimation.Running = false;

  NeoPixelTempHeatbedAnimation.Position = 0;
  NeoPixelTempHeatbedAnimation.Position_Memory = 0;
  NeoPixelTempHeatbedAnimation.RangeBegin = 0;
  NeoPixelTempHeatbedAnimation.RangeEnd = 0;
  NeoPixelTempHeatbedAnimation.Running = false;

  Printer.Status = ' ';
  Printer.ActTempHeatbed = 0.0;
  Printer.ActTempHotend = 0.0;
  Printer.ActiveTempHeatbed = 0.0;
  Printer.ActiveTempHotend = 0.0;
  Printer.StandbyTempHeatbed = 0.0;
  Printer.StandbyTempHotend = 0.0;
  Printer.HeaterStatusHeatbed = 0.0;
  Printer.HeaterStatusHotend = 0.0;
  Printer.FractionPrinted = 0.0;
  Printer.UpdatePending = false;

#if defined(DEBUGLEVEL1_ACTIVE)
  DebugPrinter.ActTempHeatbed = 0.0;
  DebugPrinter.ActTempHotend = 0.0;
  DebugPrinter.ActiveTempHeatbed = 0.0;
  DebugPrinter.ActiveTempHotend = 0.0;
  DebugPrinter.StandbyTempHeatbed = 0.0;
  DebugPrinter.StandbyTempHotend = 0.0;
  DebugPrinter.HeaterStatusHeatbed = 0.0;
  DebugPrinter.HeaterStatusHotend = 0.0;
  DebugPrinter.FractionPrinted = 0.0;
#endif

  MQTTPrinter.Status = ' ';
  MQTTPrinter.ActualTempHeatbed = 0.0;
  MQTTPrinter.ActualTempHotend = 0.0;
  MQTTPrinter.ActiveTempHeatbed = 0.0;
  MQTTPrinter.ActiveTempHotend = 0.0;
  MQTTPrinter.StandbyTempHeatbed = 0.0;
  MQTTPrinter.StandbyTempHotend = 0.0;
  MQTTPrinter.TargetTempHeatbed = 0.0;
  MQTTPrinter.TargetTempHotend = 0.0;
  MQTTPrinter.HeaterStatusHeatbed = 0;
  MQTTPrinter.HeaterStatusHotend = 0;
  MQTTPrinter.FractionPrinted = 0.0;
  MQTTPrinter.UpdatePending = false;
}

#if defined(ON_OFF_BUTTON_MOMENTARY) || defined(ON_OFF_BUTTON_LATCHING)
void DebugOnOff() {
  static unsigned long millismem = 0;

  if (millis() > millismem) {
    Serial.print("button pin: ");
    Serial.print(digitalRead(BUTTON_PIN));
    Serial.print(", Debounced: ");
    Serial.print(OnOffDebouncer.read());
    Serial.print(", OnOff: ");
    Serial.println(OnOffState);
    millismem += 1000;
  }
}
#endif

// Main loop
void loop()
{
#ifndef STANDALONE_TESTING
  // Make sure we are connected and if not then reconnect
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > MQTTRetry) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  }
  else
  {
    // Client connected
    // Read pseudo serial (MQTT) messages
    client.loop();
  }
#endif  // STANDALONE_TESTING

  GetSerialMessage();

#if defined(ON_OFF_BUTTON_MOMENTARY)
  OnOffDebouncer.update();
  if (OnOffDebouncer.fell()) {
    OnOffState = !OnOffState;
  }
  digitalWrite(LED_PIN, OnOffState);
#elif defined(ON_OFF_BUTTON_LATCHING)
  OnOffDebouncer.update();
  OnOffState = !OnOffDebouncer.read();
  digitalWrite(LED_PIN, OnOffState);
#endif

#if (defined(ON_OFF_BUTTON_MOMENTARY) || defined(ON_OFF_BUTTON_LATCHING)) && defined(ON_OFF_BUTTON_DEBUG)
  DebugOnOff();
#endif

  //NeopixelRefresh?
  if ((millis() - NeoPixelTimerRefresh) >= NeopixelRefreshSpeed) {
    NeoPixelTimerRefresh = millis();

    //New PrinterStatusUpdate?
    if (Printer.UpdatePending == true) {
      Printer.UpdatePending = false;

#if defined(DEBUGLEVEL1_ACTIVE)
      if (((Printer.ActTempHeatbed > DebugPrinter.ActTempHeatbed) && (Printer.ActTempHeatbed / DebugPrinter.ActTempHeatbed) > 2) || ((Printer.ActTempHeatbed < DebugPrinter.ActTempHeatbed) && (DebugPrinter.ActTempHeatbed / Printer.ActTempHeatbed) > 2)) {
        Serial.println("High Delta ActTempHeatbed: Act=" + String(Printer.ActTempHeatbed) + " / Memory= " + String(DebugPrinter.ActTempHeatbed));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      if (((Printer.ActTempHotend > DebugPrinter.ActTempHotend) && (Printer.ActTempHotend / DebugPrinter.ActTempHotend) > 2) || ((Printer.ActTempHotend < DebugPrinter.ActTempHotend) && (DebugPrinter.ActTempHotend / Printer.ActTempHotend) > 2)) {
        Serial.println("High Delta ActTempHotend: Act=" + String(Printer.ActTempHotend) + " / Memory= " + String(DebugPrinter.ActTempHotend));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      if (((Printer.ActiveTempHeatbed > DebugPrinter.ActiveTempHeatbed) && (Printer.ActiveTempHeatbed / DebugPrinter.ActiveTempHeatbed) > 2) || ((Printer.ActiveTempHeatbed < DebugPrinter.ActiveTempHeatbed) && (DebugPrinter.ActiveTempHeatbed / Printer.ActiveTempHeatbed) > 2)) {
        Serial.println("High Delta ActiveTempHeatbed: Act=" + String(Printer.ActiveTempHeatbed) + " / Memory= " + String(DebugPrinter.ActiveTempHeatbed));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      if (((Printer.ActiveTempHotend > DebugPrinter.ActiveTempHotend) && (Printer.ActiveTempHotend / DebugPrinter.ActiveTempHotend) > 2) || ((Printer.ActiveTempHotend < DebugPrinter.ActiveTempHotend) && (DebugPrinter.ActiveTempHotend / Printer.ActiveTempHotend) > 2)) {
        Serial.println("High Delta ActiveTempHotend: Act=" + String(Printer.ActiveTempHotend) + " / Memory= " + String(DebugPrinter.ActiveTempHotend));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      if (((Printer.StandbyTempHeatbed > DebugPrinter.StandbyTempHeatbed) && (Printer.StandbyTempHeatbed / DebugPrinter.StandbyTempHeatbed) > 2) || ((Printer.StandbyTempHeatbed < DebugPrinter.StandbyTempHeatbed) && (DebugPrinter.StandbyTempHeatbed / Printer.StandbyTempHeatbed) > 2)) {
        Serial.println("High Delta StandbyTempHeatbed: Act=" + String(Printer.StandbyTempHeatbed) + " / Memory= " + String(DebugPrinter.StandbyTempHeatbed));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      if (((Printer.StandbyTempHotend > DebugPrinter.StandbyTempHotend) && (Printer.StandbyTempHotend / DebugPrinter.StandbyTempHotend) > 2) || ((Printer.StandbyTempHotend < DebugPrinter.StandbyTempHotend) && (DebugPrinter.StandbyTempHotend / Printer.StandbyTempHotend) > 2)) {
        Serial.println("High Delta StandbyTempHotend: Act=" + String(Printer.StandbyTempHotend) + " / Memory= " + String(DebugPrinter.StandbyTempHotend));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      if (((Printer.FractionPrinted > DebugPrinter.FractionPrinted) && (Printer.FractionPrinted / DebugPrinter.FractionPrinted) > 2) || ((Printer.FractionPrinted < DebugPrinter.FractionPrinted) && (DebugPrinter.FractionPrinted / Printer.FractionPrinted) > 2)) {
        Serial.println("High Delta FractionPrinted: Act=" + String(Printer.FractionPrinted) + " / Memory= " + String(DebugPrinter.FractionPrinted));
#if !defined(DIRECT_UPDATE)
        Serial.println("SerialMessage= " + SerialMessage);
#endif
        Serial.println("");
      }
      DebugPrinter.ActTempHeatbed = Printer.ActTempHeatbed;
      DebugPrinter.ActTempHotend = Printer.ActTempHotend;
      DebugPrinter.ActiveTempHeatbed = Printer.ActiveTempHeatbed;
      DebugPrinter.ActiveTempHotend = Printer.ActiveTempHotend;
      DebugPrinter.StandbyTempHeatbed = Printer.StandbyTempHeatbed;
      DebugPrinter.StandbyTempHotend = Printer.StandbyTempHotend;
      DebugPrinter.FractionPrinted = Printer.FractionPrinted;
#endif

      //Update Neopixel Hotend
      if (NeoPixelTempHotendActive == true) {
        //Initialize AnimationRange
        NeoPixelTempHotendAnimation.RangeBegin = 0;
        NeoPixelTempHotendAnimation.RangeEnd = 0;

        //NeoPixel: Bed-Temperature
        SetTempHotend = 0;
        if (Printer.HeaterStatusHotend == 2)
        {
          SetTempHotend = Printer.ActiveTempHotend;
        }
        else if (Printer.HeaterStatusHotend == 1) {
          SetTempHotend = Printer.StandbyTempHotend;
        }
        for (int NeoPixelPosition = 1; NeoPixelPosition <= NeoPixelTempHotendCount; NeoPixelPosition++)
        {
          if ((((Printer.ActTempHotend - NeoPixelTempHotendTempOffset) / NeoPixelTempHotendScale) < NeoPixelPosition) && (((SetTempHotend - NeoPixelTempHotendTempOffset) / NeoPixelTempHotendScale) >= NeoPixelPosition)) {
            NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelPosition), NeoPixelTempHotendColorHeatUp);
            //Define Animation-Range
            if (NeoPixelTempHotendAnimation.RangeBegin == 0) {
              NeoPixelTempHotendAnimation.RangeBegin = NeoPixelPosition;
              NeoPixelTempHotendAnimation.RangeEnd = NeoPixelPosition;
            } else {
              NeoPixelTempHotendAnimation.RangeEnd++;
            }
          }
          else if ((((Printer.ActTempHotend - NeoPixelTempHotendTempOffset) / NeoPixelTempHotendScale) >= NeoPixelPosition) && (((SetTempHotend - NeoPixelTempHotendTempOffset) / NeoPixelTempHotendScale) >= NeoPixelPosition)) {
            NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelPosition), NeoPixelTempHotendColorHeatUpDone);
          }
          else if ((((Printer.ActTempHotend - NeoPixelTempHotendTempOffset) / NeoPixelTempHotendScale) >= NeoPixelPosition) && (((SetTempHotend - NeoPixelTempHotendTempOffset) / NeoPixelTempHotendScale) < NeoPixelPosition)) {
            NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelPosition), NeoPixelTempHotendColorCoolDown);
          }
          else {
            NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelPosition), NeoPixelTempHotendColorIdle);
          }
        }
      }

      //Update Neopixel PrinterStatus
      if (NeoPixelPrinterStatActive == true) {
        //Initialize AnimationRange Hotend
        NeoPixelPrinterStatAnimation.RangeBegin = 0;
        NeoPixelPrinterStatAnimation.RangeEnd = 0;

        //NeoPixel: Printer-Status & Print-Progress
        if (Printer.Status == 'P') {
          //Display Print-Progress
          for (int NeoPixelPosition = 1; NeoPixelPosition <= NeoPixelPrinterStatCount; NeoPixelPosition++)
          {
            if ((NeoPixelPosition == 1) && (Printer.FractionPrinted > 0.0)) { // this catches values that are too low to trigger initially
              NeoPixelPrinterStat.setPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPosition), NeoPixelPrinterStatColorPrintingDone);
            }
            else if (NeoPixelPosition < (Printer.FractionPrinted * NeoPixelPrinterStatCount)) {
              NeoPixelPrinterStat.setPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPosition), NeoPixelPrinterStatColorPrintingDone);
            }
            else {
              NeoPixelPrinterStat.setPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPosition), NeoPixelPrinterStatColorPrinting);
              //Define Animation-Range
              if (NeoPixelPrinterStatAnimation.RangeBegin == 0) {
                NeoPixelPrinterStatAnimation.RangeBegin = NeoPixelPosition;
                NeoPixelPrinterStatAnimation.RangeEnd = NeoPixelPosition;
              } else {
                NeoPixelPrinterStatAnimation.RangeEnd++;
              }
            }
          }
        }
        else {
          // Display Printer-Status:
          // I = idle
          // P = printing
          // S = stopped
          // C = configuring
          // A = paused
          // D = busy
          // R = pausing
          // B = resuming
          // F = flashing
          switch (Printer.Status)
          {
            case 'I':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorIdle);
              break;
            case 'P':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorPrinting);
              break;
            case 'S':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorStopped);
              break;
            case 'C':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorConfiguring);
              break;
            case 'A':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorPaused);
              break;
            case 'D':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorBusy);
              break;
            case 'R':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorPausing);
              break;
            case 'B':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorResuming);
              break;
            case 'F':
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorFlashing);
              break;
            default:
              NeoPixelPrinterStat.fill(NeoPixelPrinterStatColorDefault);
              break;
          }
        }
      }

      //Update Neopixel Heatbed
      if (NeoPixelTempHeatbedActive == true) {
        //Initialize AnimationRange
        NeoPixelTempHeatbedAnimation.RangeBegin = 0;
        NeoPixelTempHeatbedAnimation.RangeEnd = 0;

        //NeoPixel: Bed-Temperature
        SetTempHeatbed = 0;
        if (Printer.HeaterStatusHeatbed == 2)
        {
          SetTempHeatbed = Printer.ActiveTempHeatbed;
        }
        else if (Printer.HeaterStatusHeatbed == 1) {
          SetTempHeatbed = Printer.StandbyTempHeatbed;
        }
        for (int NeoPixelPosition = 1; NeoPixelPosition <= NeoPixelTempHeatbedCount; NeoPixelPosition++)
        {
          if ((((Printer.ActTempHeatbed - NeoPixelTempHeatbedTempOffset) / NeoPixelTempHeatbedScale) < NeoPixelPosition) && (((SetTempHeatbed - NeoPixelTempHeatbedTempOffset) / NeoPixelTempHeatbedScale) >= NeoPixelPosition)) {
            NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelPosition), NeoPixelTempHeatbedColorHeatUp);
            //Define Animation-Range
            if (NeoPixelTempHeatbedAnimation.RangeBegin == 0) {
              NeoPixelTempHeatbedAnimation.RangeBegin = NeoPixelPosition;
              NeoPixelTempHeatbedAnimation.RangeEnd = NeoPixelPosition;
            } else {
              NeoPixelTempHeatbedAnimation.RangeEnd++;
            }
          }
          else if ((((Printer.ActTempHeatbed - NeoPixelTempHeatbedTempOffset) / NeoPixelTempHeatbedScale) >= NeoPixelPosition) && (((SetTempHeatbed - NeoPixelTempHeatbedTempOffset) / NeoPixelTempHeatbedScale) >= NeoPixelPosition)) {
            NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelPosition), NeoPixelTempHeatbedColorHeatUpDone);
          }
          else if ((((Printer.ActTempHeatbed - NeoPixelTempHeatbedTempOffset) / NeoPixelTempHeatbedScale) >= NeoPixelPosition) && (((SetTempHeatbed - NeoPixelTempHeatbedTempOffset) / NeoPixelTempHeatbedScale) < NeoPixelPosition)) {
            NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelPosition), NeoPixelTempHeatbedColorCoolDown);
          }
          else {
            NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelPosition), NeoPixelTempHeatbedColorIdle);
          }
        }
      }
    }

    //Animation Hotend
    if (NeoPixelTempHotendAnimationActive == true) {
      //AnimationRange exists?
      if (NeoPixelTempHotendAnimation.RangeBegin == 0) {
        NeoPixelTempHotendAnimation.Running = false;
      }
      else {
        //Animation initialize?
        if (NeoPixelTempHotendAnimation.Running == false) {
          NeoPixelTempHotendAnimation.Running = true;
          NeoPixelTempHotendAnimation.Position = NeoPixelTempHotendAnimation.RangeBegin;
          NeoPixelTempHotendAnimation.Position_Memory = 0;
        }
        //Restart animation at new position?
        if (NeoPixelTempHotendAnimation.Position < NeoPixelTempHotendAnimation.RangeBegin) {
          NeoPixelTempHotendAnimation.Position = NeoPixelTempHotendAnimation.RangeBegin;
        }
        if (NeoPixelTempHotendAnimation.Position_Memory != NeoPixelTempHotendAnimation.Position && NeoPixelTempHotendAnimation.Position_Memory >= NeoPixelTempHotendAnimation.RangeBegin) {
          NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelTempHotendAnimation.Position_Memory), NeoPixelTempHotendColorHeatUp);
        }
        if (NeoPixelTempHotend.getPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelTempHotendAnimation.Position)) == NeoPixelTempHotendColorAnimation) {
          NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelTempHotendAnimation.Position), NeoPixelTempHotendColorHeatUp);
        }
        else {
          NeoPixelTempHotendAnimation.Position_Memory = NeoPixelTempHotendAnimation.Position;
          NeoPixelTempHotend.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHotendCount, NeoPixelTempHotendPixelOffset, NeoPixelTempHotendAnimation.Position), NeoPixelTempHotendColorAnimation);
        }
        NeoPixelTempHotendAnimation.Position++;
        if (NeoPixelTempHotendAnimation.Position > NeoPixelTempHotendAnimation.RangeEnd) {
          NeoPixelTempHotendAnimation.Position = NeoPixelTempHotendAnimation.RangeBegin;
        }
      }
    }

    //Animation PrinterStatus
    if (NeoPixelPrinterStatAnimationActive == true) {
      //AnimationRange exists?
      if (NeoPixelPrinterStatAnimation.RangeBegin == 0) {
        NeoPixelPrinterStatAnimation.Running = false;
      }
      else {
        //Animation initialize?
        if (NeoPixelPrinterStatAnimation.Running == false) {
          NeoPixelPrinterStatAnimation.Running = true;
          NeoPixelPrinterStatAnimation.Position = NeoPixelPrinterStatAnimation.RangeBegin;
          NeoPixelPrinterStatAnimation.Position_Memory = 0;
        }
        //Restart animation at new position?
        if (NeoPixelPrinterStatAnimation.Position < NeoPixelPrinterStatAnimation.RangeBegin) {
          NeoPixelPrinterStatAnimation.Position = NeoPixelPrinterStatAnimation.RangeBegin;
        }
        if (NeoPixelPrinterStatAnimation.Position_Memory != NeoPixelPrinterStatAnimation.Position && NeoPixelPrinterStatAnimation.Position_Memory >= NeoPixelPrinterStatAnimation.RangeBegin) {
          NeoPixelPrinterStat.setPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPrinterStatAnimation.Position_Memory), NeoPixelPrinterStatColorPrinting);
        }
        if (NeoPixelPrinterStat.getPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPrinterStatAnimation.Position)) == NeoPixelPrinterStatColorAnimation) {
          NeoPixelPrinterStat.setPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPrinterStatAnimation.Position), NeoPixelPrinterStatColorPrinting);
        }
        else {
          NeoPixelPrinterStatAnimation.Position_Memory = NeoPixelPrinterStatAnimation.Position;
          NeoPixelPrinterStat.setPixelColor(ConvertPosition2PixelIndex(NeoPixelPrinterStatCount, NeoPixelPrinterStatPixelOffset, NeoPixelPrinterStatAnimation.Position), NeoPixelPrinterStatColorAnimation);
        }
        NeoPixelPrinterStatAnimation.Position++;
        if (NeoPixelPrinterStatAnimation.Position > NeoPixelPrinterStatAnimation.RangeEnd) {
          NeoPixelPrinterStatAnimation.Position = NeoPixelPrinterStatAnimation.RangeBegin;
        }
      }
    }

    //Animation Heatbed
    if (NeoPixelTempHeatbedAnimationActive == true) {
      //AnimationRange exists?
      if (NeoPixelTempHeatbedAnimation.RangeBegin == 0) {
        NeoPixelTempHeatbedAnimation.Running = false;
      }
      else {
        //Animation initialize?
        if (NeoPixelTempHeatbedAnimation.Running == false) {
          NeoPixelTempHeatbedAnimation.Running = true;
          NeoPixelTempHeatbedAnimation.Position = NeoPixelTempHeatbedAnimation.RangeBegin;
          NeoPixelTempHeatbedAnimation.Position_Memory = 0;
        }
        //Restart animation at new position?
        if (NeoPixelTempHeatbedAnimation.Position < NeoPixelTempHeatbedAnimation.RangeBegin) {
          NeoPixelTempHeatbedAnimation.Position = NeoPixelTempHeatbedAnimation.RangeBegin;
        }
        if (NeoPixelTempHeatbedAnimation.Position_Memory != NeoPixelTempHeatbedAnimation.Position && NeoPixelTempHeatbedAnimation.Position_Memory >= NeoPixelTempHeatbedAnimation.RangeBegin) {
          NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelTempHeatbedAnimation.Position_Memory), NeoPixelTempHeatbedColorHeatUp);
        }
        if (NeoPixelTempHeatbed.getPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelTempHeatbedAnimation.Position)) == NeoPixelTempHeatbedColorAnimation) {
          NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelTempHeatbedAnimation.Position), NeoPixelTempHeatbedColorHeatUp);
        }
        else {
          NeoPixelTempHeatbedAnimation.Position_Memory = NeoPixelTempHeatbedAnimation.Position;
          NeoPixelTempHeatbed.setPixelColor(ConvertPosition2PixelIndex(NeoPixelTempHeatbedCount, NeoPixelTempHeatbedPixelOffset, NeoPixelTempHeatbedAnimation.Position), NeoPixelTempHeatbedColorAnimation);
        }
        NeoPixelTempHeatbedAnimation.Position++;
        if (NeoPixelTempHeatbedAnimation.Position > NeoPixelTempHeatbedAnimation.RangeEnd) {
          NeoPixelTempHeatbedAnimation.Position = NeoPixelTempHeatbedAnimation.RangeBegin;
        }
      }
    }

    // refresh the displays if they're on
    NeoPixelRefresh(false);
  }
}

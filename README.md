NeoPixel sketch based on the work done by Claus Noack (https://github.com/mule1972/NeoPixelBLVmgn) for the 3D-Printer project from Ben Levi (https://www.thingiverse.com/thing:3382718)

With this version of the sketch you can use Neopixels to show the hotend- and heatbed-temperature and the different printer status including print progress while using OctoPrint with almost any printer and not just those that are Duet based systems. As with the original version you retain the ability to have nearly full control on the NeoPixels and their configuration.

Requirements:

  •	Raspberry Pi with MQTT broker installed and running OctoPrint with MQTT Plugin
  
  •	Working WiFi connection to RPi

  •	Tested with WeMos D1 Mini compatible (https://www.amazon.com/gp/product/B081PX9YFV) and NodeMCU CP2102 ESP-12E (https://www.amazon.com/gp/product/B07HF44GBT) but almost any ESP8266 based board should work.

  •	Adafruit.NeopixelLibrary (minimum version 1.2.3) (https://github.com/adafruit/Adafruit_NeoPixel)

  •	WiFi Library (https://www.arduino.cc/en/Reference/WiFi)

  •	ESP8266 Library (https://github.com/plapointe6/EspMQTTClient)

  •	MQTT Library (https://github.com/plapointe6/EspMQTTClient)

To configure the sketch to your personal needs, change the parameters for the WiFI and MQTT section at the top and then move on to those marked in the "User-Config" part in the sketch file before compiling and uploading it to your Arduino.

To configure the MQTT Plugin select the option as shown below. You can leave the default value of “octoPrint” for the base value if you are using a local MQTT Broker. If using a common broker, select a unique identifier and also adjust the settings in the sketch to match or you won’t see any updates.

The sketch will first connect to the specified Wifi network and then connect to the MQTT client. Once connected then it will subscribe to the topics needed to provide the status information needed to update the status rings. Each time it connects to the MQTT client the active rings will display a rainbow pattern, which will also provide notification if the client connection ever drops and reconnects.

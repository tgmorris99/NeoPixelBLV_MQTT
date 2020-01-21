# NeoPixelBLV_MQTT
NeoPixel sketch based on the work done by Claus Noack (https://github.com/mule1972/NeoPixelBLVmgn) for the 3D-Printer project from Ben Levi (https://www.thingiverse.com/thing:3382718)

With this version of the sketch you can use Neopixels to show the hotend- and heatbed-temperature and the different printer status including print progress while using OctoPrint with almost any printer and not just those that are Duet based systems. As with the original version you retain the ability to have nearly full control on the NeoPixels and their configuration.

## Getting Started
These instructions will get you a copy of the project up and running on your local machine.

### Prerequisites
* Raspberry Pi with MQTT broker installed and running OctoPrint with MQTT Plugin
* Working WiFi connection to RPi

### Requirements
* Tested with WeMos D1 Mini compatible (https://www.amazon.com/gp/product/B081PX9YFV) and NodeMCU CP2102 ESP-12E (https://www.amazon.com/gp/product/B07HF44GBT) but almost any ESP8266 based board should work.
* Adafruit.NeopixelLibrary (minimum version 1.2.3) (https://github.com/adafruit/Adafruit_NeoPixel)
* WiFi Library (https://www.arduino.cc/en/Reference/WiFi) NOTE: Requires a change to MQTT_MAX_PACKET_SIZE from the default value of 128 to a value of 1024 in PubSubClient.h
* ESP8266 Library (https://github.com/plapointe6/EspMQTTClient)
* MQTT Library (https://github.com/plapointe6/EspMQTTClient)

## Installing
To configure the sketch to your personal needs, change the parameters for the WiFI and MQTT section near the top and then move on to those settings marked in the "User-Config" part in the sketch file before compiling and uploading it to your Arduino.

To configure the MQTT Plugin select the options as shown below. Feel free to leave the default value of “octoPrint” for the base value if a local MQTT Broker is being used. If using a common broker, select a unique identifier and also adjust the settings in the sketch to match or you won’t see any updates.

The sketch will first connect to the specified Wifi network and then connect to the MQTT client. Once fully connected it will subscribe to the topics needed to provide the status information used to update the individual status rings. Each time a connection to the MQTT client is made the active rings will display a rainbow pattern. This feature will also provide notification if the client connection ever drops and reconnects.

### Installing

A step by step series of examples that tell you how to get a development env running

Say what the step will be

```
Give the example
```

And repeat

```
until finished
```

End with an example of getting some data out of the system or using it for a little demo

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

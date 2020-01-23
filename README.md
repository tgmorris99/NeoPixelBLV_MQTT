# NeoPixelBLV_MQTT
NeoPixel sketch based on the work done by Claus Noack (https://github.com/mule1972/NeoPixelBLVmgn) for the 3D-Printer project from Ben Levi (https://www.thingiverse.com/thing:3382718)

With this version of the sketch you can use Neopixels to show the hotend- and heatbed-temperature and the different printer status including print progress while using OctoPrint with almost any printer and not just those that are Duet based systems. As with the original version you retain the ability to have nearly full control on the NeoPixels and their configuration.

## Getting Started
These instructions will get you a copy of the project up and running on your local machine.

### Prerequisites
* Raspberry Pi with MQTT broker and client installed and running OctoPrint with the MQTT Plugin - this is the minimum configuration and is a standalone environment. The MQTT client is required by the MQTT Plugin and must be installed on the RPi running OctoPrint. The broker may either be hosted locally or on a common device so long as the necessary configuration changes are made in the sketch.
* Working WiFi connection to RPi

### Requirements
* Tested with WeMos D1 Mini compatible (https://www.amazon.com/gp/product/B081PX9YFV) and NodeMCU CP2102 ESP-12E (https://www.amazon.com/gp/product/B07HF44GBT) but almost any ESP8266 based board should work.
* Windows driver installed for the CH340 communications chip the above boards use. The driver link can be found on [this page](https://www.arduined.eu/ch340g-converter-windows-7-driver-download/) or [downloaded directly](https://www.arduined.eu/files/CH341SER.zip)
* Adafruit.NeopixelLibrary (minimum version 1.2.3) (https://github.com/adafruit/Adafruit_NeoPixel)
* WiFi Library (https://www.arduino.cc/en/Reference/WiFi)
* ESP8266 Library (https://github.com/plapointe6/EspMQTTClient)
* MQTT Library (https://pubsubclient.knolleary.net/) NOTE: Requires a change to MQTT_MAX_PACKET_SIZE from the default value of 128 to a value of 1024 in PubSubClient.h

### Notes
It is also necessary to include some additional libraries for the Arduino IDE. Depending on which board you choose select one of the following - or both as it really doesn't matter.

To set up your Arduino IDE go to File->Preferences and copy the desired URL to the *Additional Boards Manager URLs* to get the necessary extensions:
* http://arduino.esp8266.com/stable/package_esp8266com_index.json
* https://dl.espressif.com/dl/package_esp32_index.json

## Installing
To configure the sketch to your personal needs, change the parameters for the WiFI and MQTT section near the top and then move on to the settings marked in the "User-Config" part in the sketch file before compiling and uploading it to your Arduino.

To configure the MQTT Plugin select the options as explained below. Feel free to leave the default value of “octoPrint” for the base topic if a local MQTT Broker is being used. If using a common broker, select a unique identifier and also adjust the settings in the sketch to match or you won’t see any updates.

The sketch will first display a walking bit pattern while connecting to the specified WiFi network. After the WiFi connection is established the sketch will then connect to the MQTT client. Once fully connected the sketch will subscribe to the topics needed to provide the status information used to update the individual status rings. Each time a connection to the MQTT client is made the active rings will display a rainbow pattern. This feature will also provide notification if the MQTT client connection ever drops and reconnects.

I used a tutorial like [this one](https://randomnerdtutorials.com/how-to-install-mosquitto-broker-on-raspberry-pi/) to install the MQTT broker and clients on the RPi.

### Configuring the OctoPrint MQTT Plugin

#### Broker Tab
For the Host use either "octopi.local" or the IP Address of the RPi. The other inputs may remain at the default values. All the other options should remain unchecked except for *Enable retain flag* and *Clean Session*.

#### Topics Tab
Under *Event Messages* only *Activate event messages* and *Printing events* should be checked. For *Progress Messages* only *Activate progress messages* should be checked. For *Temperature messages*, *Activate temperature messages* should be checked. It is safe to set the threshold to zero as that will provide a regular update even when the temperature isn't changing.

#### Status & Last Will
Remains unchanged.

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

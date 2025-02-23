ESP8266 Smart Programmer

A smart programmer using ESP8266 for EEPROM programming and frequency generation.

Features

Program EEPROM: Read, write EEPROM chips.

Frequency Generator: Generate specific frequencies for testing.

Wi-Fi Connectivity: Control programming wirelessly.

Android App-Based UI: Configure settings via ExpertCare android App.



---

Hardware Requirements

ESP8266 (NodeMCU, Wemos D1 Mini, or similar)

EEPROM chip 

Supporting resistors and capacitors

USB Cable (for initial flashing)

Power supply (5V or 3.3V, depending on EEPROM type)



---

Software Requirements

Arduino IDE

ESP8266 Board Package

Required Libraries:

EEPROM.h

ESP8266WiFi

ESP8266WebServer

Arduino.json

Wire.h 




---

Installation & Setup

1. Install ESP8266 Board Support in Arduino IDE:

Open Arduino IDE → Go to File > Preferences.

In Additional Board Manager URLs, add:

http://arduino.esp8266.com/stable/package_esp8266com_index.json

Go to Tools > Board > Board Manager, search ESP8266, and install it.



2. Install Required Libraries:

Open Arduino IDE > Go to Sketch > Include Library > Manage Libraries.

Search and install the required libraries.



3. Flash the Code:

Connect ESP8266 via USB.

Select the correct board and port in Tools.

Click Upload.





---

Usage

1. Connect to Wi-Fi

The ESP8266 will create a hotspot (SmartProgrammer).

Connect to it using default credentials (SSID: SmartProgrammer, Password: 12345678).

Open a browser and go to http://192.168.4.1.



2. Programming EEPROM

Select the EEPROM type.

Choose read or write.

Upload the binary file for writing.



3. Frequency Generator

Set desired frequency.

Click Generate Frequency to generate.



---

Troubleshooting

ESP8266 not detected? Check USB cable or drivers.

Can't connect to Wi-Fi? Reset ESP and try again.

EEPROM programming fails? Verify wiring and chip type.



---

Contributing

Feel free to fork this repository, submit PRs, or report issues!


---

License

This project is licensed under the MIT License.


---

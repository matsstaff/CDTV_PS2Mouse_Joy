# CDTV_PS2Mouse_Joy
Arduino PS/2 mouse and joystick adapter for Commodore CDTV

![Commodore CDTV](http://www.oldcomputers.net/pics/cdtv-right.jpg)

This project aims to read data from a PS/2 mouse and a normal Amiga/Atari/C64 joystick and translate it to the weird serial protocol that the Commodore CDTV uses.

* Single Arduino .ino file, without any dependencies. Easy peasy.
* Utilises hardware features as much as possible. Robust design.

# Mouse connection (Mini-DIN 6 female)
![Mini-DIN 6](http://www.betaarchive.com/imageupload/2012-02/1329677183.or.24457.png)

|Arduino Pin|Description|Mouse|
|-----------|-----------|-----|
|5V|PS/2 POWER|4|
|2|PS/2 CLOCK|5|
|4|PS/2 DATA|1|
|GND|PS/2 GROUND|3|
 
# Joystick connection (DB9 male)
![DB9](http://old-computers.com/MUSEUM/connectors/sms_joystick.gif)

|Arduino Pin|Description|Joystick|
|-----------|-----------|-----|
|A0|JOYSTICK RIGHT|4|
|A1|JOYSTICK LEFT|3|
|A2|JOYSTICK DOWN|2|
|A3|JOYSTICK UP|1|
|A4|JOYSTICK BTN2|6|
|A5|JOYSTICK BTN1|5|
 
# Output (Mini-DIN 4 male)
![Mini DIN 4](https://upload.wikimedia.org/wikipedia/commons/thumb/5/57/Mini-din-4.svg/320px-Mini-din-4.svg.png)

|Arduino Pin|Description|Mini-DIN 4| 
|-----------|-----------|---|
|GND|GND|1|
|GND|GND|2|
|3|PRDT (CDTV serial data)|3|
|5V|POWER|4|


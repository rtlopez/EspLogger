# EspLogger

Minimal version of firmware similar to [OpenLog](https://github.com/sparkfun/OpenLog) which allows to log serial data on SD cards.
The difference is that this firmware and hardware combination allows logging with speed up to 500kbps.

## Requirements

- Arduino IDE 1.8
- [ESP8266 Arduino](https://github.com/esp8266/Arduino) SDK installed from github (see [using git version](https://github.com/esp8266/Arduino#using-git-version) chapter)
- [SdFat](https://github.com/greiman/SdFat) library (copy it from the libraries subdirectory into Arduino/libraries)
- [WeMos D1 mini](https://www.wemos.cc/product/d1-mini.html) board or even any ESP-12E
- [WeMos SD card sield](https://www.wemos.cc/product/micro-sd-card-shield.html)

## Configuration

Put config.txt file in root directory of sd card. If not exists, it will be created with default configuration
```
115200
baud
```

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "Arduino.h"

#define BUFFER_SIZE 512

#define SYNC_TIMEOUT_MS 200

// #define DBG

void readPort() ICACHE_RAM_ATTR;
void writeSd() ICACHE_RAM_ATTR;

void initSd();
void readEeprom();
void readConfig();
void initPort();
void writeConfig();
void writeEeprom();
void openFile();

class ConfigData
{
  public:
    ConfigData(): baud(115200), writeConfig(false) {}
    int32_t baud;
    bool writeConfig;
    String fileName;
};

class EepromData
{
  public:
    EepromData(): seq(1) {}
    int16_t magic;
    int16_t seq;
    static const int16_t MAGIC = 0x55AA;
};

#endif // _CONFIG_H_

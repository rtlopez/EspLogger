#include <EEPROM.h>
#include "SdFat.h"
#include "config.h"

#define LED_SD    4 //D2
#define LED_PORT  5 //D1

// WeMos D1 esp8266: pin 4 (D2) as the first schema or pin 15 (D8) as standard
const uint8_t SD_CS_PIN = 15;

// Try max SPI clock for an SD. Reduce SPI_CLOCK if errors occur.
#define SPI_CLOCK SD_SCK_MHZ(50)

// Try to select the best SD card configuration.
#if HAS_SDIO_CLASS
#define SD_CONFIG SdioConfig(FIFO_SDIO)
#elif ENABLE_DEDICATED_SPI
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SPI_CLOCK)
#else  // HAS_SDIO_CLASS
#define SD_CONFIG SdSpiConfig(SD_CS_PIN, SHARED_SPI, SPI_CLOCK)
#endif  // HAS_SDIO_CLASS

ConfigData config;
EepromData eeprom;
HardwareSerial& serial = Serial;
SdFat sd;
SdFile file;

char buf[BUFFER_SIZE];
size_t idx = 0;

void setup()
{
  pinMode(LED_PORT, OUTPUT); digitalWrite(LED_PORT, LOW);
  pinMode(LED_SD, OUTPUT); digitalWrite(LED_SD, LOW);
  serial.begin(115200);
  serial.println();

  initSd();
  readEeprom();
  readConfig();
  initPort();
  writeConfig();
  writeEeprom();
  openFile();

  while(serial.available()) serial.read(); // flush input buffer
}

void loop()
{
  readPort();
  writeSd();
}

void initPort()
{
  serial.begin(config.baud);
  serial.setRxBufferSize(1024);

  #ifdef DBG
  serial.print("si: "); serial.println(config.baud);
  #endif
}

void initSd()
{
  if (!ENABLE_DEDICATED_SPI) {
    #ifdef DBG
    serial.println("Set ENABLE_DEDICATED_SPI nonzero in SdFatConfig.h for best SPI performance");
    #endif
  }
  if(sd.begin(SD_CONFIG))
  {
    #ifdef DBG
    serial.println("SD Begin");
    #endif

    return;
  }
  
  if(!sd.chdir())
  {
    serial.println("FileSystem error");
    while(true);
  }
}

void openFile()
{
  digitalWrite(LED_SD, HIGH);
  if(!file.open(config.fileName.c_str(), O_CREAT | O_WRITE | O_APPEND))
  {
    serial.println("Open error");
    while(true);
  }
  if(file.fileSize() == 0)
  {
    file.rewind();
    file.sync();
  }
  digitalWrite(LED_SD, LOW);

  #ifdef DBG
  serial.print("fo: "); serial.println(!!file);
  #endif
}
unsigned long lastReadTs = 0;

void readPort()
{
  if(idx < BUFFER_SIZE && serial.available())
  {
    size_t len = 0;
    digitalWrite(LED_PORT, HIGH);
    while(idx < BUFFER_SIZE && serial.available())
    {
      buf[idx] = serial.read();
      idx++;
      len++;
    }
    lastReadTs = millis();
    digitalWrite(LED_PORT, LOW);

    #ifdef DBG
    serial.print("sr: "); serial.println(len);
    #endif
    //serial.print(len);
  }
}
unsigned long lastSyncTs = 0;

void writeSd()
{
  if(!file.isOpen()) return;

  unsigned long now = millis();
  // TODO: make sure that every write is aligned to 512 block size
  if(idx >= BUFFER_SIZE || (idx > 0 && (now - lastReadTs > SYNC_TIMEOUT_MS || (lastSyncTs > 0 && now - lastSyncTs > SYNC_TIMEOUT_MS))))
  {
    digitalWrite(LED_SD, HIGH);
    
    file.write(buf, idx);
    file.sync();
    size_t sent = idx;
    idx = 0;
    
    lastSyncTs = millis();

    #ifdef DBG
    serial.print("fw: "); serial.println(sent);
    #endif
    //serial.print(sent);
    
    digitalWrite(LED_SD, LOW);
  }
}

void readConfig()
{
  SdFile file;
  file.open("CONFIG.TXT", O_READ);
  if(file.isOpen())
  {
    char delim[] = "\n";
    char line[32];
    file.fgets(line, 32, delim);
    config.baud = String(line).toInt();
    file.close();

    #ifdef DBG
    serial.print("cb: "); serial.println(config.baud);
    #endif
  }
}

void writeConfig()
{
  SdFile file;
  file.open("CONFIG.TXT", O_READ);
  if(file.isOpen())
  {
    char delim[] = "\n";
    char line[32];
    file.fgets(line, 32, delim);
    int baud = String(line).toInt();
    if(baud != config.baud) config.writeConfig = true;
    file.close();
  }
  else
  {
    config.writeConfig = true;
  }

  if(config.writeConfig)
  {
    file.open("CONFIG.TXT", O_WRITE | O_CREAT | O_SYNC);
    if(file.isOpen())
    {
      file.println(config.baud);
      file.println("baud");
      file.close();

      #ifdef DBG
      serial.print("cw: "); serial.println(config.baud);
      #endif
    }
  }
}

void readEeprom()
{
  EEPROM.begin(32);
  EEPROM.get(0, eeprom.magic);
  if(eeprom.magic == EepromData::MAGIC)
  {
    EEPROM.get(2, eeprom.seq);
  }
  else
  {
    eeprom.magic = EepromData::MAGIC;
    eeprom.seq = 0;
  }
  eeprom.seq++;

  if(eeprom.seq < 10)        config.fileName = "LOG_000";
  else if(eeprom.seq < 100)  config.fileName = "LOG_00";
  else if(eeprom.seq < 1000) config.fileName = "LOG_0";
  else                       config.fileName = "LOG_";
  config.fileName += eeprom.seq;
  config.fileName += ".TXT";

  #ifdef DBG
  serial.print("er: "); serial.println(config.fileName);
  #endif
}

void writeEeprom()
{
  EEPROM.put(0, eeprom.magic);
  EEPROM.put(2, eeprom.seq);
  EEPROM.commit();

  #ifdef DBG
  serial.print("ew: "); serial.println(eeprom.seq);
  #endif
}

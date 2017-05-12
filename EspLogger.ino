#include <EEPROM.h>
#include <SdFat.h>

#include "config.h"

extern "C" {
#include "user_interface.h"
}

ConfigData config;
EepromData eeprom;
HardwareSerial& serial = Serial;
SdFat sd;
SdFile file;
char buf[BUFFER_SIZE];
size_t idx = 0;


#define BUFF_LEN 32768
char * buff = NULL;
const char * benchFilename = "bench.dat";
uint32_t mem_prev = 0;
uint32_t mem = 0;
uint32_t ts_prev = 0;
uint32_t ts = 0;

void benchStatus(const char * label)
{
  ts = micros();
  if(!ts_prev) ts_prev = ts;
  
  mem = system_get_free_heap_size();
  if(!mem_prev) mem_prev = mem;
  
  Serial.print(label);
  Serial.print(": ");
  Serial.print(mem);
  Serial.print(' ');
  Serial.print((int)mem_prev - (int)mem);
  Serial.print(' ');
  Serial.println((ts - ts_prev) / 1000.0f);
  
  mem_prev = mem;
  ts_prev = ts;
}

void setup()
{
  pinMode(D1, OUTPUT); digitalWrite(D1, LOW);
  pinMode(D2, OUTPUT); digitalWrite(D2, LOW);
  serial.begin(115200);
  serial.println();

#ifndef BENCHMARK_MODE
  initSd();
  readEeprom();
  readConfig();
  initPort();
  writeConfig();
  writeEeprom();
  openFile(config.fileName.c_str());
  while(serial.available()) serial.read(); // flush input buffer
#else
  benchStatus("start");
  initSd();
  benchStatus("   sd");

  initPort(); 
  benchStatus(" port");

  buff = new char[BUFF_LEN];
  for(size_t i = 0; i < BUFF_LEN; i++) buff[i] = 0;
  benchStatus(" alloc");
  
  benchmarkBegin(benchFilename);
  benchStatus("bench");
  
  openFile(benchFilename);
  benchStatus(" open");
#endif
}

void loop()
{
#ifndef BENCHMARK_MODE
  readPort();
  writeSd();
#else
  benchmarkLoop();
#endif
}

#define BENCH_LEN 8
size_t bench_packs[] { 128, 256, 512, 1024, 2048, 200, 400, 800 };
size_t bench_patts[] { '0', '1', '2',  '3',  '4', '5', '6', '7' };
size_t bench_pack = 0;
size_t bench_written = 0;

void benchmarkBegin(const char * fileName)
{
  digitalWrite(D2, HIGH);
  SdFile file;
  if(file.open(fileName, O_WRITE))
  {
    if(!file.remove()) serial.println("cannot remove file");
    //if(!file.truncate(0)) serial.println("cannot truncate file");
    file.close();
  }
  else
  {
    serial.println("cannot open file");
  }
  digitalWrite(D2, LOW);
}

void benchmarkFill()
{
  benchStatus("* time");
  for(size_t i = 0; i < BUFF_LEN; i++)
  {
    buff[i] = bench_patts[bench_pack];
  }
  serial.print("* pattern: ");
  for(size_t i = 0; i < 8; i++)
  {
    serial.print(buff[i]);
  }
  serial.println();
}

const size_t block_rate = 1024;

void benchmarkLoop()
{
  if(!file.isOpen()) return; 
  if(bench_pack >= BENCH_LEN) return;
  if(bench_written == 0)
  {
    benchmarkFill();
  }
  
  if(bench_written >= 63 * block_rate)
  {
    int left = 64 * block_rate - bench_written;
    while(left < 0) left += block_rate;
    if(left > 0)
    {
      digitalWrite(D1, HIGH);
      file.write(buff, left);
      file.sync();
      digitalWrite(D1, LOW);
    }
    serial.print("* align: ");
    serial.println(left);
    bench_written = 0;
    bench_pack++;
  }
  else
  {
    digitalWrite(D1, HIGH);
    size_t len = bench_packs[bench_pack];
    file.write(buff, len);
    bench_written += len;
    file.sync();
    digitalWrite(D1, LOW);
  }
  //benchStatus("loop");
}

void initPort()
{
  serial.begin(config.baud);
  serial.setRxBufferSize(1024);

  #ifdef DBG
  serial.print("si: "); serial.println(config.baud);
  #endif
}

static SPISettings sd_speeds[] = { SPI_FULL_SPEED, SPI_HALF_SPEED, SPI_QUARTER_SPEED, SD_SCK_MHZ(8) };

void initSd()
{
  for(size_t i = 0; i < 4; i++)
  {
    if(sd.begin(SS, sd_speeds[i]))
    {
      #ifdef DBG
      serial.print("fi: "); serial.println(i);
      #endif

      return;
    }
  }
  if(!sd.chdir())
  {
    serial.println("FileSystem error");
    while(true);
  }
}

void openFile(const char * fileName)
{
  digitalWrite(D2, HIGH);
  if(!file.open(fileName, O_CREAT | O_WRITE | O_APPEND))
  {
    serial.println("File open error");
  }
  if(file.fileSize() == 0)
  {
    file.rewind();
    file.sync();
  }
  //else
  //{
  //  if(!file.truncate(0)) Serial.println("File truncate error");
  //}
  digitalWrite(D2, LOW);

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
    digitalWrite(D1, HIGH);
    while(idx < BUFFER_SIZE && serial.available())
    {
      buf[idx] = serial.read();
      idx++;
      len++;
    }
    lastReadTs = millis();
    digitalWrite(D1, LOW);

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
    digitalWrite(D2, HIGH);
    
    file.write(buf, idx);
    file.sync();
    #ifdef DBG
    size_t sent = idx;
    #endif
    idx = 0;
    
    lastSyncTs = millis();

    #ifdef DBG
    serial.print("fw: "); serial.println(sent);
    #endif
    //serial.print(sent);
    
    digitalWrite(D2, LOW);
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

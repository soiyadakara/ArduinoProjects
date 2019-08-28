// 16x16 matrix LED Master

// pin assign
//  ESP8266   <->   SLAVE
//  2         RESET 7
//  4         SDA   4
//  14        SCL   3
//  12        TXD    5
//  13        RET    6
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <time.h>

const char* ssid = "ssid";
const char* password = "passwd";

#include <SoftwareSerial.h>
#include <FS.h>
#include <Wire.h>
#define SLA_START 0x09

#define MATRIX_CMD_CLEAR_DATA 0x10      // 0x10 clear data
#define MATRIX_CMD_WRITE_ALL_DATA 0x20  // 0x20 write all data, receive 32byte
#define MATRIX_CMD_WRITE_DATA 0x30      // 0x30 write selected row, receive 1+2byte
#define MATRIX_CMD_WRITE_DATA_MASK 0x31 // 0x30 write selected row with mask, receive 1(row)+2(data)+2(mask)byte
#define MATRIX_CMD_SHIFT_LEFT 0x40      // 0x30 shift data to left, receive 2byte
#define MATRIX_CMD_SHIFT_RIGHT 0x41     // 0x31 shift data to right, receive 2byte
#define MATRIX_CMD_READ_ALL_DATA 0x80   // 0x80 read all data, send 32byte
#define MATRIX_CMD_READ_DATA 0x90       // 0x90 read selected row, receive 1byte, send 2byte

String constdata = "unko!ｳﾝｺ！祗園精舎の鐘の声、諸行無常の響きあり。娑羅双樹の花の色、盛者必衰の理をあらはす。おごれる人も久しからず、唯春の夜の夢のごとし。たけき者も遂にはほろびぬ、偏に風の前の塵に同じ。";
//String constdata = "The quick brown fox jumps over the lazy dog";
#define G_STRINGDATA_SIZE 1024
byte g_stringdata[G_STRINGDATA_SIZE];
uint16_t g_stringdata_ptr;
uint16_t g_stringdata_end;

typedef struct {
  uint16_t tstart;
  uint16_t tend;
  uint32_t datastart;
} table_t;
table_t fonttable[128];
uint8_t fonttablenum;
table_t unicodemaptable[10];
uint8_t unicodemaptablenum;
File halfwidth;
File fullwidth;
File unicodemap;
#define IsSJISCode(c) ( ((0x81 <= c) && (c <= 0x9F)) || (0xE0 <= c) && (c <= 0xEF) )
#define FONT_XSIZE  16
#define FONT_YSIZE  16

// get bitmap from UTF-8 code
//  *code:   UTF-8 code(variable rength)
//  bitmap: buffer for write bitmap
//  return: 0 if fail to get bitmap
//          the number of read byte of *code if success
int getBitmapFromUTF8(uint8_t* code, uint16_t* bitmap, uint8_t* bitmapwidth) {
  int ret = 0;

  uint16_t unicode = 0;
  if (!((*code) & 0x80)) {
    unicode = *code;
    ret = 1;
  } else if ((*code & 0xc0) && !(*code & 0x20)) {
    unicode = ((*code & 0x1f) << 6) + (*(code + 1) & 0x3f);
    ret = 2;
  } else if ((*code & 0xe0) && !(*code & 0x10)) {
    unicode = ((*code & 0x0f) << 12) + ((*(code + 1) & 0x3f) << 6) + (*(code + 2) & 0x3f);
    ret = 3;
  }

  if (!(unicode & 0xff00)) { // if 1byte code
    getBitmapFromJIS(unicode, bitmap);
    *bitmapwidth = FONT_XSIZE/2;
  } else {
    uint16_t sjiscode;
    for (int i = 0; i < unicodemaptablenum; i++) {
      if (unicodemaptable[i].tstart <= unicode && unicode <= unicodemaptable[i].tend) {
        unicodemap.seek(unicodemaptable[i].datastart + 2 * (unicode - unicodemaptable[i].tstart), SeekSet);
        sjiscode = unicodemap.read() << 8;
        sjiscode += unicodemap.read();
        break;
      }
    }
    if (sjiscode & 0xff00) {
      getBitmapFromSJIS(sjiscode, bitmap);
    *bitmapwidth = FONT_XSIZE;
    } else {
      getBitmapFromJIS(sjiscode, bitmap);
    *bitmapwidth = FONT_XSIZE/2;
    }
  }
  return ret;
}

// get bitmap from SJIS code
//  code:   SJIS code
//  bitmap: buffer for write bitmap
//  return: 0 if fail to get bitmap
//          1 if successfully completed
int getBitmapFromSJIS(uint16_t code, uint16_t* bitmap) {
  int ret = 0;

  for (int i = 0; i < fonttablenum; i++) {
    if (fonttable[i].tstart <= code && code <= fonttable[i].tend) {
      fullwidth.seek(fonttable[i].datastart + (FONT_XSIZE / 8)*FONT_YSIZE * (code - fonttable[i].tstart), SeekSet);
      for (int j = 0; j < FONT_YSIZE; j++) {
        bitmap[j] = fullwidth.read() << 8;
        bitmap[j] += fullwidth.read();
      }
      ret = 1;
      break;
    }
  }
  return ret;
}
// get bitmap from JIS X 0201 code
//  code:   JIS code
//  bitmap: buffer for write bitmap
//  return: 0 if fail to get bitmap
//          1 if successfully completed
int getBitmapFromJIS(uint8_t code, uint16_t* bitmap) {
  int ret = 0;

  halfwidth.seek(17 + (FONT_XSIZE / 8) * (FONT_YSIZE / 2)*code, SeekSet);
  for (int j = 0; j < FONT_YSIZE / 2; j++) {
    bitmap[j] = halfwidth.read() << 8;
    bitmap[j] += halfwidth.read();
  }
  ret = 1;
  return ret;
}


uint16_t g_data[16];
uint8_t g_data_ptr;
uint8_t g_data_end;
uint8_t matrixnum;
int resetPin = 2;
void setup() {
  // reset all slaves
  delay(1000);
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);
  delay(100);
  digitalWrite(resetPin, HIGH);
  delay(1000);

  Wire.begin(4, 14);       // sda,scl
  Serial.begin(115200);  // start serial for output
  Serial.println("initialization start!");

  // get font table
  SPIFFS.begin();
  halfwidth = SPIFFS.open("/shnm8x16r_transpose.fnt", "r");
  if (!halfwidth) {
    Serial.println("shnm8x16r.fnt file open failed");
  }
  fullwidth = SPIFFS.open("/shnmk16_transpose.fnt", "r");
  if (!fullwidth) {
    Serial.println("shnmk16.fnt file open failed");
  }
  fullwidth.seek(17, SeekSet);
  fonttablenum = fullwidth.read();
  uint16_t temp;
  uint32_t datastart = 18 + fonttablenum * 4;
  for (int i = 0; i < fonttablenum; i++) {
    temp = fullwidth.read();
    temp += (fullwidth.read() << 8);
    fonttable[i].tstart = temp;
    temp = fullwidth.read();
    temp += (fullwidth.read() << 8);
    fonttable[i].tend = temp;
    fonttable[i].datastart = datastart;
    datastart += (fonttable[i].tend - fonttable[i].tstart + 1) * (FONT_XSIZE / 8) * FONT_YSIZE;
  }
  unicodemap = SPIFFS.open("/unicode.map", "r");
  if (!unicodemap) {
    Serial.println("unicode.map file open failed");
  }
  unicodemap.seek(17, SeekSet);
  unicodemaptablenum = unicodemap.read();
  datastart = 18 + unicodemaptablenum * 4;
  for (int i = 0; i < unicodemaptablenum; i++) {
    temp = unicodemap.read();
    temp += (unicodemap.read() << 8);
    unicodemaptable[i].tstart = temp;
    temp = unicodemap.read();
    temp += (unicodemap.read() << 8);
    unicodemaptable[i].tend = temp;
    unicodemaptable[i].datastart = datastart;
    datastart += (unicodemaptable[i].tend - unicodemaptable[i].tstart + 1) * 2;
  }

  ESP.wdtFeed();

  // config all matrix address
  SoftwareSerial SWSerial(13, 12); // RX,TX
  SWSerial.begin(9600);
  SWSerial.flush();
  SWSerial.write(SLA_START);
  for (int i = 0; i < 100; i++) {
    delay(100);
    if (SWSerial.available() > 0)
      break;
  }
  matrixnum = SWSerial.read();
  if (matrixnum != 0xff) {
    Serial.print("last addr:");
    Serial.println(matrixnum, HEX);
    matrixnum -= SLA_START;
    Serial.print("matrix quantity:");
    Serial.println(matrixnum, HEX);
  } else {
    matrixnum = 0;
    Serial.println("matrix not found");
  }
  // clear data
  for (int i = 0; i < matrixnum; i++) {
    Wire.beginTransmission(SLA_START + i);
    Wire.write(MATRIX_CMD_CLEAR_DATA);
    Wire.endTransmission();
  }
  Serial.println("initialization finished");

  constdata.getBytes(g_stringdata, G_STRINGDATA_SIZE);
  g_stringdata_end = constdata.length();
  g_stringdata_ptr = 0;

  g_data_ptr = 0;
  g_data_end = 0;

  // connect to wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  
  // ArduinoOTA
    ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  configTime( 3600*9, 0, "ntp.nict.jp", "ntp.jst.mfeed.ad.jp");
  Serial.println("ntp ok");
}

uint8_t data[64];
uint16_t incomingByte = 0;
void loop() {
  if (Serial.available() > 0) {
    g_stringdata[g_stringdata_end++] = Serial.read();
  }

  if (g_data_ptr == g_data_end && g_stringdata_ptr != g_stringdata_end) {
    uint8_t readbyte,bitmapwidth;
    readbyte = getBitmapFromUTF8(g_stringdata + g_stringdata_ptr, g_data, &bitmapwidth);
    g_stringdata_ptr += readbyte;
    g_data_end = bitmapwidth;
    g_data_ptr = 0;
  }

  // send SHIFT_LEFT
  data[0] = MATRIX_CMD_SHIFT_LEFT;
  data[1] = g_data[g_data_ptr] >> 8;
  data[2] = g_data[g_data_ptr];
  for (int j = 0; j < matrixnum; j++) {
    Wire.beginTransmission(SLA_START + j);
    Wire.write(data, 3);
    Wire.endTransmission();
    do {
      Wire.requestFrom(SLA_START + j, 2);
    } while (Wire.available() < 2);
    data[1] = Wire.read();
    data[2] = Wire.read();
  }
  if(g_data_ptr != g_data_end)
    g_data_ptr++;
  delay(50);

  /*  // send CLEAR_DATA
    data[0] = MATRIX_CMD_CLEAR_DATA;
    for(int i=0;i<matrixnum;i++){
      Wire.beginTransmission(SLA_START+i);
      Wire.write(data, 1);
      Wire.endTransmission();
    }
    delay(1000);
    // send WRITE_ALL_DATA
    data[0] = MATRIX_CMD_WRITE_ALL_DATA;
    for (int j = 0; j < 5; j++) {
      for (int i = 0; i < 16; i++) {
        data[1 + i * 2] = g_data[16 * j + i] >> 8;
        data[1 + i * 2 + 1] = g_data[16 * j + i];
      }
    for(int k=0;k<matrixnum;k++){
      Wire.beginTransmission(SLA_START+k);
      Wire.write(data, 33);
      Wire.endTransmission();
    }
      delay(500);
    }
    delay(500);
    // send WRITE_DATA
    data[0] = MATRIX_CMD_WRITE_DATA;
    for (int i = 0; i < 16; i++) {
      data[1] = i;
      data[2] = g_data[i] >> 8;
      data[3] = g_data[i];
    for(int j=0;j<matrixnum;j++){
      Wire.beginTransmission(SLA_START+j);
      Wire.write(data, 4);
      Wire.endTransmission();
    }
      delay(100);
    }
    delay(1000);
    for (int i = 0; i < 16; i++) {
      data[1] = 15 - i;
      data[2] = g_data[16 + 15 - i] >> 8;
      data[3] = g_data[16 + 15 - i];
    for(int j=0;j<matrixnum;j++){
      Wire.beginTransmission(SLA_START+j);
      Wire.write(data, 4);
      Wire.endTransmission();
    }
      delay(100);
    }
    delay(1000);
    // send WRITE_DATA_MASK
    data[0] = MATRIX_CMD_WRITE_DATA_MASK;
    for (int j = 0; j < 16; j++) {
      for (int i = 0; i < 16; i++) {
        data[1] = i;
        data[2] = g_data[32 + i] >> 8;
        data[3] = g_data[32 + i];
        data[4] = 1 << (j - 8);
        data[5] = 1 << j;
    for(int k=0;k<matrixnum;k++){
        do {
          Wire.beginTransmission(SLA_START+k);
          Wire.write(data, 6);
        } while (Wire.endTransmission());
    }
      }
      delay(100);
    }
    delay(1000);
    for (int j = 0; j < 16; j++) {
      for (int i = 0; i < 16; i++) {
        data[1] = i;
        data[2] = g_data[48 + i] >> 8;
        data[3] = g_data[48 + i];
        data[4] = 1 << (15 - j - 8);
        data[5] = 1 << (15 - j);

    for(int k=0;k<matrixnum;k++){
        do {
          Wire.beginTransmission(SLA_START+k);
          Wire.write(data, 6);
        } while (Wire.endTransmission());
    }
      }
      delay(100);
    }
    delay(1000);
    // send WRITE_DATA
    data[0] = MATRIX_CMD_WRITE_DATA;
    char collist[] = {7, 8, 6, 9, 5, 10, 4, 11, 3, 12, 2, 13, 1, 14, 0, 15};
    for (int i = 0; i < 16; i++) {
      data[1] = collist[i];
      data[2] = g_data[64 + collist[i]] >> 8;
      data[3] = g_data[64 + collist[i]];
    for(int j=0;j<matrixnum;j++){
      Wire.beginTransmission(SLA_START+j);
      Wire.write(data, 4);
      Wire.endTransmission();
    }
      delay(100);
    }
    delay(1000);
    // send SHIFT_LEFT
    data[0] = MATRIX_CMD_SHIFT_LEFT;
    for (int i = 0; i < sizeof(g_data) / sizeof(g_data[0]); i++) {
    data[1] = g_data[i] >> 8;
    data[2] = g_data[i];
    for (int j = 0; j < matrixnum; j++) {
      Wire.beginTransmission(SLA_START + j);
      Wire.write(data, 3);
      Wire.endTransmission();
      do {
        Wire.requestFrom(SLA_START + j, 2);
      } while (Wire.available() < 2);
      data[1] = Wire.read();
      data[2] = Wire.read();
    }
    delay(100);
    }
    delay(1000);
    // send SHIFT_RIGHT
    data[0] = MATRIX_CMD_SHIFT_RIGHT;
    for (int i = 16; i < sizeof(g_data) / sizeof(g_data[0]); i++) {
    data[1] = g_data[sizeof(g_data) / sizeof(g_data[0]) - 1 - i] >> 8;
    data[2] = g_data[sizeof(g_data) / sizeof(g_data[0]) - 1 - i];
    for (int j = 0; j < matrixnum; j++) {
      Wire.beginTransmission(SLA_START + (matrixnum - 1 - j));
      Wire.write(data, 3);
      Wire.endTransmission();
      do {
        Wire.requestFrom(SLA_START + (matrixnum - 1 - j), 2);
      } while (Wire.available() < 2);
      data[1] = Wire.read();
      data[2] = Wire.read();
    }
    delay(100);
    }
    delay(1000);
    // send READ_ALL_DATA
    data[0] = MATRIX_CMD_READ_ALL_DATA;
    Wire.beginTransmission(SLA_START);
    Wire.write(data, 1);
    Wire.endTransmission();
    delay(10);
    Wire.requestFrom(SLA_START, 32);
    Serial.print("READ_ALL_DATA:");
    for (int i = 0; i < 16; i++) {
    Serial.print(Wire.read(), HEX);
    Serial.print(Wire.read(), HEX);
    Serial.print(",");
    }
    Serial.println();
    // send READ_DATA
    data[0] = MATRIX_CMD_READ_DATA;
    data[1] = 3;
    Wire.beginTransmission(SLA_START);
    Wire.write(data, 2);
    Wire.endTransmission();
    delay(10);
    Wire.requestFrom(SLA_START, 2);
    Serial.print("READ_DATA of column 3:");
    for (int i = 0; i < 2; i++) {
    Serial.print(Wire.read(), HEX);
    }
    Serial.println();
    delay(2000);
  */
  ArduinoOTA.handle();
}

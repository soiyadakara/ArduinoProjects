/*// ESP32
  #include <WiFi.h>
  #include <ESPmDNS.h>
  #include <WiFiUdp.h>
  #include <ArduinoOTA.h>
*/
// ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


// SkySafariのデフォルト設定より、ポート4030
WiFiServer server(4030);
WiFiServer telnetserver(23);
WiFiClient telnetclient;

// 作成するアクセスポイントのSSID、パスワード
const char SOFTAP_SSID[] = "AVX-Serial";
const char SOFTAP_PW[] = "advancedvx";

// 接続するアクセスポイントのSSID、パスワード
const char AP_SSID[] = "ssid";
const char AP_PW[] = "passwd";


// SkySafariのデフォルト設定より、IPアドレス10.0.0.1
const IPAddress ip(10, 0, 0, 1);
const IPAddress subnet(255, 255, 255, 0);

void setup() {
  // Advanced-VXのボーレート9600
  Serial.begin(9600);

  WiFi.disconnect();
  WiFi.mode(WIFI_AP_STA);
  // アクセスポイント設定
  WiFi.softAPConfig(ip, ip, subnet);
  WiFi.softAP(SOFTAP_SSID, SOFTAP_PW);
  // STA設定
  WiFi.begin(AP_SSID, AP_PW);

  // APへの接続、5秒試行しAPなければあきらめる
  for (int i = 0; i < 50; i++) {
    //    Serial.print(".");
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(100);
  }


  // ----  AndroidOTAコード開始  ----
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("avx");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (!telnetclient) {
      telnetclient = telnetserver.available();
    }
    if (telnetclient && telnetclient.connected()) {
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      telnetclient.println("Start updating " + type);
    }
  });
  ArduinoOTA.onEnd([]() {
    if (!telnetclient) {
      telnetclient = telnetserver.available();
    }
    if (telnetclient && telnetclient.connected()) {
      telnetclient.println("\nEnd");
    }
    delay(10);
    telnetclient.stop();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (!telnetclient) {
      telnetclient = telnetserver.available();
    }
    if (telnetclient && telnetclient.connected()) {
      telnetclient.printf("Progress: %u%%\r", (progress / (total / 100)));
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if (!telnetclient) {
      telnetclient = telnetserver.available();
    }
    if (telnetclient && telnetclient.connected()) {
      telnetclient.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) telnetclient.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) telnetclient.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) telnetclient.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) telnetclient.println("Receive Failed");
      else if (error == OTA_END_ERROR) telnetclient.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  //  Serial.println("Ready");
  //  Serial.print("IP address: ");
  //  Serial.println(WiFi.localIP());
  // ----  AndroidOTAコード終了  ----

  // サーバー開始
  server.begin();
  telnetserver.begin();
}

// RA,DEC
uint32_t AVX_RA;
uint32_t AVX_DEC;
uint32_t target_RA;
uint32_t target_DEC;
byte slewrate;

// Location, Time
uint8_t lat_d;
uint8_t lat_m;
uint8_t lat_ns;
uint8_t hour;
uint8_t minuites;
uint8_t second;

byte command[32];
byte commandlen;
byte buf[32];
byte buflen;
void loop() {
  WiFiClient client = server.available();
  // SkySafariからの接続が無ければ何もしない
  if (client) {
    while (client.connected()) {
      if (!telnetclient) {
        telnetclient = telnetserver.available();
      }

      while (client.available()) {
        client.setTimeout(100);
        commandlen = client.readBytes(command, 32);

        if (command[0] == ':') {
          // コマンド変換
          if (command[1] == 'R') {
            if (command[2] == 'G') {
              slewrate = 2;
            } else if (command[2] == 'C') {
              slewrate = 5;
            } else if (command[2] == 'M') {
              slewrate = 7;
            } else if (command[2] == 'S') {
              slewrate = 9;
            }
            if (commandlen > 4) {
              for (byte i = 4; i < commandlen; i++) {
                command[i - 4] = command[i];
              }
            }
          }
          if (command[1] == 'G' && command[2] == 'D') {
            // Get DEC
            Serial.write('e');

            String radec;
            for (byte i = 0; i < 10; i++) {
              Serial.setTimeout(100);
              radec = Serial.readString();
              if (radec) {
                break;
              }
            }
            if (radec) {
              AVX_RA = HEX2UINT32(radec.substring(0, 8));
              AVX_DEC = HEX2UINT32(radec.substring(9, 17));
              /*
                            radec.substring(0, 8).toCharArray((char*)buf, 32);
                            AVX_RA = strtoul( (char*)buf, nullptr, 16);
                            radec.substring(9, 17).toCharArray((char*)buf, 32);
                            AVX_DEC = strtoul( (char*)buf, nullptr, 16);
              */
              AVXtoLX200_DEC(AVX_DEC).toCharArray((char*)buf, 32);
              buflen = 10;
              client.write(buf, 10);
            }
          } else if (command[1] == 'G' && command[2] == 'R') {
            // Get RA
            AVXtoLX200_RA(AVX_RA).toCharArray((char*)buf, 32);
            buflen = 9;
            client.write(buf, 9);
          } else if (command[1] == 'S' && command[2] == 'r') {
            // Set target RA
            target_RA = LX200toAVX_RA((command[3] - 0x30) * 10 + (command[4] - 0x30),
                                      (command[6] - 0x30) * 10 + (command[7] - 0x30),
                                      (command[9] - 0x30) * 10 + (command[10] - 0x30));
            client.print("1");
          } else if (command[1] == 'S' && command[2] == 'd') {
            // Set target DEC
            target_DEC = LX200toAVX_DEC(command[3], (command[4] - 0x30) * 10 + (command[5] - 0x30),
                                        (command[7] - 0x30) * 10 + (command[8] - 0x30),
                                        (command[10] - 0x30) * 10 + (command[11] - 0x30));
            client.print("1");
          } else if (command[1] == 'C' && command[2] == 'M') {
            // Align
            sprintf((char*)buf, "s%08X,%08X", target_RA, target_DEC);
            buflen = 18;
            Serial.write(buf, 18);

            String response;
            for (byte i = 0; i < 10; i++) {
              Serial.setTimeout(100);
              response = Serial.readString();
              if (response) {
                break;
              }
            }
            if (response == "#") {
              client.print("ok#");
            }
          } else if (command[1] == 'M' && command[2] == 'S') {
            // Goto
            sprintf((char*)buf, "r%08X,%08X", target_RA, target_DEC);
            buflen = 18;
            Serial.write(buf, 18);

            String response;
            for (byte i = 0; i < 10; i++) {
              Serial.setTimeout(100);
              response = Serial.readString();
              if (response) {
                break;
              }
            }
            if (response == "#") {
              client.write("0");
            }
          } else if (command[1] == 'M' || command[1] == 'Q') {
            // Slew to
            buf[0] = 'P';
            buf[1] = 2;
            if (command[2] == 'n') {
              // North
              buf[2] = 17;
              buf[3] = 36;
            } else if (command[2] == 's') {
              // South
              buf[2] = 17;
              buf[3] = 37;
            } else if (command[2] == 'e') {
              // East
              buf[2] = 16;
              buf[3] = 36;
            } else if (command[2] == 'w') {
              // West
              buf[2] = 16;
              buf[3] = 36;
            }
            if (command[2] != '#') {
              buf[4] = command[1] == 'Q' ? 0 : slewrate;
              buf[5] = 0;
              buf[6] = 0;
              buf[7] = 0;
              buflen = 8;
              Serial.write(buf, 8);
            } else {
              // Halt all
              Serial.print("M");
            }

            String response;
            for (byte i = 0; i < 10; i++) {
              Serial.setTimeout(100);
              response = Serial.readString();
              if (response) {
                break;
              }
            }
            if (response == "#") {
              // ok
            }
          } else if (command[1] == 'S' && command[2] == 't') {
            // Set Latitude
            lat_ns = command[3] == '+' ? 0 : 1;
            lat_d = (command[4] - 0x30) * 10 + command[5] - 0x30;
            lat_m = (command[7] - 0x30) * 10 + command[8] - 0x30;
            client.write("1");
          } else if (command[1] == 'S' && command[2] == 'g') {
            // Set Longitude
            uint16_t lon_d;
            lon_d = (command[3] - 0x30) * 100 + (command[4] - 0x30) * 10 + command[5] - 0x30;
            if (lon_d > 180) {
              lon_d = 359 - lon_d;
              buf[8] = 0;
            } else {
              buf[8] = 1;
            }

            buf[0] = 'W';
            buf[1] = lat_d;
            buf[2] = lat_m;
            buf[3] = 0x00;
            buf[4] = lat_ns;
            buf[5] = lon_d & 0xFF;
            buf[6] = (command[7] - 0x30) * 10 + command[8] - 0x30;
            buf[7] = 0x00;

            if (buf[6] == 0) {
              buf[5]++;
            }
            buflen = 9;
            Serial.write(buf, 9);

            String response;
            for (byte i = 0; i < 10; i++) {
              Serial.setTimeout(100);
              response = Serial.readString();
              if (response) {
                break;
              }
            }
            if (response == "#") {
              client.write("1");
            }
          } else if (command[1] == 'S' && command[2] == 'G') {
            // Set time difference
            client.write("1");
          } else if (command[1] == 'S' && command[2] == 'L') {
            // Set local time
            /*            hour = (command[3]-0x30)*10 + command[4]-0x30;
                        minuites = (command[6]-0x30)*10 + command[7]-0x30;
                        second = (command[9]-0x30)*10 + command[10]-0x30;
            */            client.write("1");
          } else if (command[1] == 'S' && command[2] == 'C') {
            // Set date (UTC)
            /*            buf[0] = 'H';
                        buf[1] = hour;
                        buf[2] = minuites;
                        buf[3] = second;
                        buf[4] = (command[3]-0x30)*10 + command[4]-0x30;
                        buf[5] = (command[6]-0x30)*10 + command[7]-0x30;
                        buf[6] = (command[9]-0x30)*10 + command[10]-0x30;
                        buf[7] = 256-9;
                        buf[8] = 0;
                        if(buf[1] >= 15){   // カレンダーを考慮していない
                          buf[5]++;
                        }
                        buflen = 9;
                        Serial.write(buf, 9);

                        String response;
                        for (byte i = 0; i < 10; i++) {
                          Serial.setTimeout(100);
                          response = Serial.readString();
                          if (response) {
                            break;
                          }
                        }
                        if (response == "#") {
            */              client.write("1");
            //            }
          }
          if (telnetclient && telnetclient.connected()) {
            telnetclient.write(command, commandlen);
            telnetclient.println();
            telnetclient.write(buf, buflen);
            telnetclient.println();
          }
        } else {
          Serial.write(command, commandlen);
          if (telnetclient && telnetclient.connected()) {
            telnetclient.write(command, commandlen);
          }
        }
      }

      // uart -> wifi
      if (command[0] != ':') {
        if (Serial.available()) {
          // WiFiにはまとめて送るため、データをまとめて受信
          Serial.setTimeout(100);
          buflen = Serial.readBytes(buf, 32);

          client.write(buf, buflen);
          if (telnetclient && telnetclient.connected()) {
            telnetclient.write(buf, buflen);
          }
        }
      }
    }
    delay(1);

    // 接続終了
    client.stop();
  }
  // ArduinoOTA
  ArduinoOTA.handle();
}

uint32_t HEX2UINT32(String d) {
  uint32_t ret = 0;
  for (uint8_t i = 0; i < d.length(); i++) {
    ret <<= 4;
    if ('0' <= d[i] && d[i] <= '9') {
      ret += d[i] - '0';
    } else if ('A' <= d[i] && d[i] <= 'F') {
      ret += d[i] + 10 - 'A';
    } else if ('a' <= d[i] && d[i] <= 'f') {
      ret += d[i] + 10 - 'a';
    }
  }
  return ret;
}

String AVXtoLX200_RA(uint32_t radata) {
  /*  int h = (radata >> 24) * 0.088;
    //  radata -= h*4294967296/24;
    radata -= h * 178956970;
    int m = (radata >> 18) * 0.087;
    //  radata -= m*4294967296/(24*60);
    radata -= m * 2982616;
    int s = (radata >> 12) * 0.082;
  */
  radata >>= 8;
  int h = (radata / 16777216.0) * 24;
  radata -= h * 699050;
  int m = (radata / 16777216.0) * 24 * 60;
  radata -= m * 11650;
  int s = (radata / 16777216.0) * 24 * 60 * 60;

  char ret[16];
  sprintf(ret, "%02d:%02d:%02d#", h, m, s);
  return ret;
}

String AVXtoLX200_DEC(uint32_t decdata) {
  int pm = 1;
  if (decdata & 0x80000000) {
    decdata *= -1;
    pm = -1;
  }
  /*  int d = (decdata >> 20) * 0.088;
    //  decdata -= dd*4294967296/360;
    decdata -= d * 11930464;
    int m = (decdata >> 14) * 0.082;
    //  decdata -= dm*4294967296/(360*60);
    decdata -= m * 198841;
    int s = (decdata >> 8) * 0.078;
  */
  decdata = decdata >> 8;
  int d = (decdata / 16777216.0) * 360;
  decdata -= d * 46603;
  int m = (decdata / 16777216.0) * 360 * 60;
  decdata -= m * 776;
  int s = (decdata / 16777216.0) * 360 * 60 * 60;


  char ret[16];
  sprintf(ret, "%c%02d*%02d'%02d#", pm > 0 ? '+' : '-', d, m, s);
  return ret;
}

uint32_t LX200toAVX_RA(uint8_t h, uint8_t m, uint8_t s) {
  uint32_t ret;

  ret = h * 178956970;
  ret += m * 2982616;
  //  ret += s*4294967296/(24*60*60);
  ret += s * 49710;
  return ret;
}

uint32_t LX200toAVX_DEC(uint8_t pm, uint8_t d, uint8_t m, uint8_t s) {
  uint32_t ret;

  ret = d * 11930464;
  ret += m * 198841;
  //  ret += s*4294967296/(360*60*60);
  ret += s * 3314;
  ret *= (pm == '-') ? -1 : 1;
  return ret;
}

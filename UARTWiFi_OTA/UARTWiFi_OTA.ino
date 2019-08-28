#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// SkySafariのデフォルト設定より、ポート4060
WiFiServer server(4060);
WiFiServer telnetserver(23);
WiFiClient telnetclient;

// 作成するアクセスポイントのSSID、パスワード
const char SOFTAP_SSID[] = "AVX-Serial";
const char SOFTAP_PW[] = "advancedvx";

// 接続するアクセスポイントのSSID、パスワード
const char AP_SSID[] = "ssid";
const char AP_PW[] = "pass";


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

byte buf[16];
byte len;
void loop() {
  WiFiClient client = server.available();
  // SkySafariからの接続が無ければ何もしない
  if (client) {
    while (client.connected()) {
      if (!telnetclient) {
        telnetclient = telnetserver.available();
      }
      // wifi -> uart
      while (client.available()) {
        client.setTimeout(100);
        len = client.readBytes(buf, 16);

        Serial.write(buf,len);
        if (telnetclient && telnetclient.connected()) {
          telnetclient.write(buf, len); 
        }
      }

      // uart -> wifi
      if (Serial.available()) {
        // WiFiにはまとめて送るため、データをまとめて受信
        Serial.setTimeout(100);
        len = Serial.readBytes(buf, 16);
        client.write(buf, len); 
        if (telnetclient && telnetclient.connected()) {
          telnetclient.write(buf, len); 
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

//#include <WiFi.h>
#include <ESP8266WiFi.h>
//#include <WiFiClient.h>

WiFiServer server(2000);
WiFiClient client;

const char ssid[] = "ESP32-WiFi";  // SSID
const char pass[] = "esp32wifi";   // password

const IPAddress ip(1, 2, 3, 4);      // IPアドレス
const IPAddress subnet(255, 255, 255, 0); // サブネットマスク

void setup() {
  Serial.begin(115200);

  WiFi.softAP(ssid, pass);           // SSIDとパスの設定
  delay(100);                        // 追記：このdelayを入れないと失敗する場合がある
  WiFi.softAPConfig(ip, ip, subnet); // IPアドレス、ゲートウェイ、サブネットマスクの設定

  IPAddress myIP = WiFi.softAPIP();  // WiFi.softAPIP()でWiFi起動
  server.begin();                    // サーバーを起動(htmlを表示させるため)

  /* 各種情報を表示 */
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  Serial.println("Server start!");
}

void loop() {
  String line;
  if (server.hasClient()) {
    client = server.available();
  }

  if (client) {
    if (client.connected()) {
      if (client.available()) {
        while (client.available()) {
          client.setTimeout(200);
          line = client.readStringUntil('\r');
          Serial.print("> ");
          Serial.println(line);
          for (int i = 0; i < line.length(); i++) {
            Serial.print(line[i], HEX);
            Serial.print(" ");
          }
          Serial.println();
        }

        if (line.substring(0, 3) == "$$$") {
          client.print("CMD\r");
          Serial.println("CMD");
        } else if (line.substring(0, 15) == "set comm open 0") {
          client.print("123456789012345678AOK");
        } else if (line.substring(0, 17) == "set comm remote 0") {
          client.print("12345678901234567890AOK");
        } else if (line.substring(0, 16) == "set comm close 0") {
          client.print("1234567890123456789AOK");
        } else if (line.substring(0, 4) == "exit") {
          client.print("1234567EXIT");
        } else if (line[4] == 0xFE) {
          byte response[] = {0x3b, 0x03, 0x20, 0x10, 0xfe, 0xcf, 0x3b, 0x07, 0x10, 0x20, 0xfe, 0x07, 0x0a, 0x10, 0x0d, 0x9d};
          client.write(response, 16);
          Serial.println("0xFE");
        } else if (line[4] == 0x05) {
          byte response[] = {0x3b , 0x03 , 0x20 , 0x10 , 0x05 , 0xc8 , 0x3b , 0x05 , 0x10 , 0x20 , 0x05 , 0x16 , 0x87 , 0x29};
          client.write(response, 14);
          Serial.println("0x05");
        } else {
          Serial.println("?");
        }
        //          break;
      }
    } else {
      // 接続が切れた場合
      client.stop();
      Serial.println("client disonnected");
    }
  }
}

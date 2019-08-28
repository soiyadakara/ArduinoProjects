#include <WiFi.h>

WiFiServer server(2000);

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

String hoge = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ";
int incommingbyte;
int charnum = 18;
int charnum2 = 20;
int charnum3 = 19;
int charnum4 = 7;
void loop() {
  WiFiClient client = server.available();
  String line;
  
  if (client) {
//    client.print("*HELLO*");
    Serial.println("new client!");
    while (client.connected()) {
      if (client.available()) {  
          while(client.available()){
              line = client.readStringUntil('\r');
              Serial.print("> ");
              Serial.println(line);
              for(int i=0;i<line.length();i++){
                Serial.print(line[i],HEX);
                Serial.print(" ");
              }
              Serial.println();
          }
/*          client.println("HTTP/1.1 200 OK");
          client.println("Content-type:text/html");
          client.println();
          client.println("hello!");
*/          
          if(line.substring(0,3) == "$$$"){
            client.print("CMD\r");
//            client.print("ABCDEFG");
            Serial.println("CMD");
          }else if(line.substring(0,15) == "set comm open 0"){
            client.print(hoge.substring(0,charnum) + String("AOK"));
//            Serial.println("AOK\r<2.40-CEL> ");
            Serial.println("AOK");
          }else if(line.substring(0,17) == "set comm remote 0"){
            client.print(hoge.substring(0,charnum2) + String("AOK"));
            Serial.println("AOK");
          }else if(line.substring(0,16) == "set comm close 0"){
            client.print(hoge.substring(0,charnum3) + String("AOK"));
            Serial.println("AOK");
          }else if(line.substring(0,4) == "exit"){
            client.print(hoge.substring(0,charnum4) + String("EXIT"));
//            client.print("EXIT");
            Serial.println("EXIT");
          }else if(line.substring(0,11) == "load config"){
            client.print("\rAOK\r<2.40-CEL> \r");
//            Serial.println("AOK\r\n<2.40-CEL> ");
          }else if(line[4] == 0xFE){
            byte response[] = {0x3b, 0x03, 0x20, 0x10, 0xfe, 0xcf, 0x3b, 0x07, 0x10, 0x20, 0xfe, 0x07, 0x0a, 0x10, 0x0d, 0x9d};
            client.write(response, 16);
            Serial.println("0xFE");
          }else if(line[4] == 0x05){
            byte response[] = {0x3b ,0x03 ,0x20 ,0x10 ,0x05 ,0xc8 ,0x3b ,0x05 ,0x10 ,0x20 ,0x05 ,0x16 ,0x87 ,0x29};
            client.write(response, 14);
            Serial.println("0x05");
          }else{
            Serial.println("?");
          }
//          break;
      }
    }

    // 接続が切れた場合
    client.stop();
    Serial.println("client disonnected");
  }

  if(Serial.available()>0){
    incommingbyte = Serial.read();
    if(incommingbyte == 'u'){
      charnum3++;
    }else if(incommingbyte == 'd'){
      charnum3--;
    }if(incommingbyte == 'j'){
      charnum4++;
    }else if(incommingbyte == 'k'){
      charnum4--;
    }
    Serial.print("charnum3: ");
    Serial.println(charnum3);
    Serial.print("charnum4: ");
    Serial.println(charnum4);
  }
}

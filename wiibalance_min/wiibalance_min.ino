/*
  wiibalance

  created 2017/02
  by JR2GEV

*/

#include "WiiBalanceBoard.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

WiiBalanceBoard wbb;
#define LED_MAIN 16 //LOWで点灯
#define PWR_ON 14 //LOWでON
#define SW_SYNC 13
#define SW_MAIN 12

// wifi設定
String ssid = "ssid";
String pass = "pass";
String host = "script.google.com";
String url = "macros/nanyarakanyara/exec";

void setup() {
  // 電源のFETをON
  pinMode(PWR_ON, OUTPUT);
  digitalWrite(PWR_ON, LOW);

  pinMode(SW_SYNC, INPUT);
  pinMode(SW_MAIN, INPUT);
  pinMode(LED_MAIN, OUTPUT);

  //Serialのinitialization
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  wbb.init();
  digitalWrite(LED_MAIN, LOW);
  delay(1000);

  // wifiの挙動が怪しい場合があるので、切断しておく
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
}

void loop() {
  float total;
  float weights_buf[4];

  wbb.getWeights(weights_buf);
  total = (weights_buf[0] + weights_buf[1] + weights_buf[2] + weights_buf[3]) / 4;
  Serial.println(String(total, 2));

  for (byte i = 0; i < 4; i++) {
    digitalWrite(LED_MAIN, HIGH);
    delay(200);
    digitalWrite(LED_MAIN, LOW);
    delay(200);
  }

  // 測定値をPOST
  postWeight(total);

  // 終了
  digitalWrite(PWR_ON, HIGH);
  pinMode(PWR_ON, INPUT);
}

byte postWeight(float weight) {

  Serial.println("SSID: " + ssid);
  Serial.println("PASS: " + pass);
  Serial.println(host + url);

  WiFi.begin(ssid.c_str(), pass.c_str());
  while (1) {
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  Serial.println("WiFi connected");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  WiFiClientSecure sslclient;
  String params;
  params = "key=rinpoyo&weight=" + String(weight, 2);

  if (sslclient.connect(host.c_str(), 443) > 0) {
    Serial.println("[HTTPS] POST... \n");
    sslclient.println("POST " + url + " HTTP/1.1");
    sslclient.println("Host: " + host);
    sslclient.println("User-Agent: ESP8266/1.0");
    sslclient.println("Connection: close");
    sslclient.println("Content-Type: application/x-www-form-urlencoded;");
    sslclient.print("Content-Length: ");
    sslclient.println(params.length());
    sslclient.println();
    sslclient.println(params);
    delay(10);
    String response = sslclient.readString();
    int bodypos =  response.indexOf("\r\n");
    Serial.println( response.substring(0, bodypos));
  } else {
    // HTTP client errors
    Serial.println("[HTTPS] no connection or no HTTP server.");
  }
}

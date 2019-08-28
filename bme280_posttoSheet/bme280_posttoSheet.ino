/***************************************************************************
  BME280 test program
  Acquired data are posted to Google Spreadsheet.
 ***************************************************************************/

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

const char* ssid     = "ssid";
const char* password = "passwd";

const String host = "script.google.com";
const String url = "/macros/s/nantokakantoka/exec";

Adafruit_BME280 bme; // I2C

void setup() {
  Serial.begin(9600);
  Serial.println(F("BME280 test"));

  bool status;

  // BME280への接続
  Wire.begin(4, 5); // SDA=IO4, SCL=IO5
  status = bme.begin(0x76, &Wire);  // BME280アドレス指定
  if (!status) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }

  Serial.println();

  // おまじない風切断
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
}


void loop() {
  postValues();
  delay(10*60*1000);
}


void postValues() {
  // WiFi接続開始
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  
  WiFiClientSecure sslclient;
  String params;

  // センサデータ取得、パラメータ準備
  params = "temperature=" + String(bme.readTemperature(), 2);
  params += "&pressure=" + String(bme.readPressure()/100.0F, 2);
  params += "&humidity=" + String(bme.readHumidity(), 2);
  Serial.println(params);

  // SSL接続開始、POSTメッセージ送信
  if (sslclient.connect(host, 443) > 0) {
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
  } else {
    // HTTP client errors
    Serial.println("[HTTPS] no connection or no HTTP server.");
  }

  // 送信完了、接続終了
  WiFi.disconnect();
}

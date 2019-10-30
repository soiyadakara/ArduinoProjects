
#include <mcp_can.h>
#include <SPI.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#define CAN0_INT 2                              // Set INT to pin 2
MCP_CAN CAN0(10);                               // Set CS to pin 10

volatile bool refleshDisplay = true;
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

volatile unsigned int tacho;
volatile unsigned char coolanttemp;
volatile unsigned char vspeed;
volatile float HVlife;
void readCANMsg() {
  CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
  if ((rxId & 0x1FFFFFFF) == 0x18DAF105) {
    switch (rxBuf[2]) {
      case 0x05: //coolant
        coolanttemp = (unsigned char)rxBuf[3] - 40;
        break;
      case 0x0c: //tacho
        tacho = ((unsigned int)rxBuf[3] * 256 + rxBuf[4]) / 4;
        break;
      case 0x0d: //speed
        vspeed = rxBuf[3];
        break;
      case 0x5B: //HV remaining life
        HVlife = (float)rxBuf[3] * 100 / 255;
        break;
    }
    refleshDisplay = true;
  }
}

void setup()   {
  Serial.begin(115200);
  delay(2000); 

  // initialize display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  display.clearDisplay();

  // text display tests
  display.setTextSize(2);
  display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print("1234567890ABCDEF");
    display.display();
    delay(2000);

  // initialize CANBUS
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");

  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
  attachInterrupt(digitalPinToInterrupt(CAN0_INT), readCANMsg, LOW);

  Serial.println("MCP2515 Library Receive Example...");
}

byte data[8] = {0x02, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00};
char strbuf[16];

volatile unsigned long prevtimemillis = 0;
void loop() {
  if (refleshDisplay)                        // If CAN0_INT pin is low, read receive buffer
  {
    refleshDisplay = false;
    
    display.clearDisplay();
    // speed
    display.setCursor(0, 0);
    sprintf(strbuf, "%4d", vspeed);
    display.print(strbuf);
    // coolant temp
    display.setCursor(80, 0);
    sprintf(strbuf, "%3d", coolanttemp);
    display.print(strbuf);
    // tacho
    display.setCursor(0, 16);
    sprintf(strbuf, "%4d", tacho);
    display.print(strbuf);
    // HV battery
    display.setCursor(56, 16);
    dtostrf(HVlife, 5, 1, strbuf);
    display.print(strbuf);
    display.display();

 /*    Serial.println(coolanttemp); 
    Serial.println(tacho); 
    Serial.println(vspeed); 
    Serial.println(HVlife); 
    Serial.println("reflesh!");
    Serial.println(len);
*/ }
  if(millis() - prevtimemillis > 250){
    data[2] = 0x05; // coolant temp
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    delay(50);
    data[2] = 0x0c; // tacho
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    delay(50);
    data[2] = 0x0d; // speed
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    delay(50);
    data[2] = 0x5B; // HV battery life
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    delay(50);
    prevtimemillis = millis();
    refleshDisplay = true;
  }
}

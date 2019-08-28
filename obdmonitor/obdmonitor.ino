
#include <mcp_can.h>
#include <SPI.h>

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

static const unsigned char PROGMEM batterymark[] = {
  0b00000000,
  0b00000000,
  0b00111100,
  0b11000011,
  0b10000001,
  0b10000001,
  0b10000001,
  0b10111101,
  0b10000001,
  0b10111101,
  0b10000001,
  0b10111101,
  0b10000001,
  0b11111111,
  0b00000000,
  0b00000000,
};
static const unsigned char PROGMEM hotwatermark[] = {
  0b00000000, 0b00000000,
  0b00000001, 0b00000000,
  0b00001000, 0b10100000,
  0b00000100, 0b10010000,
  0b00000110, 0b01010000,
  0b00000010, 0b01010000,
  0b00000100, 0b10010000,
  0b00101001, 0b10110100,
  0b01101001, 0b00100110,
  0b11000101, 0b00010011,
  0b10000000, 0b10000001,
  0b11000000, 0b00000011,
  0b01100000, 0b00000110,
  0b00111100, 0b00111100,
  0b00000111, 0b11100000,
};


#define CAN0_INT 7                              // Set INT to pin 2
MCP_CAN CAN0(10);                               // Set CS to pin 10

volatile bool refleshDisplay = true;
long unsigned int rxId;
unsigned char len = 0;
unsigned char rxBuf[8];

unsigned int tacho;
unsigned char coolanttemp;
unsigned char vspeed;
float HVlife;
void readCANMsg() {
  CAN0.readMsgBuf(&rxId, &len, rxBuf);      // Read data: len = data length, buf = data byte(s)
  if (rxId == 0x18DAF105) {
    switch (rxBuf[2]) {
      case 0x05: //coolant
        coolanttemp = rxBuf[2] - 40;
        break;
      case 0x0c: //tacho
        tacho = ((unsigned int)rxBuf[2] * 256 + rxBuf[3]) / 4;
        break;
      case 0x0d: //speed
        vspeed = rxBuf[2];
        break;
      case 0x5B: //HV remaining life
        HVlife = (float)rxBuf[2] * 100 / 255;
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
  display.drawBitmap(0, 0, batterymark, 8, 16, 1);
  display.drawBitmap(8, 0, hotwatermark, 16, 16, 1);
  display.setCursor(24, 0);
  display.println("SOC:80% TMP: 90℃");
  display.setCursor(0, 16);
  display.println("hoge!");
  display.display();
  delay(2000);
  display.clearDisplay();

  // initialize CANBUS
  if (CAN0.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK)
    Serial.println("MCP2515 Initialized Successfully!");
  else
    Serial.println("Error Initializing MCP2515...");

  CAN0.setMode(MCP_NORMAL);                     // Set operation mode to normal so the MCP2515 sends acks to received data.

  pinMode(CAN0_INT, INPUT);                            // Configuring pin for /INT input
  attachInterrupt(digitalPinToInterrupt(CAN0_INT), readCANMsg, FALLING);

  Serial.println("MCP2515 Library Receive Example...");
}

byte data[8] = {0x02, 0x01, 0x05, 0x00, 0x00, 0x00, 0x00};

volatile unsigned long prevtimemillis = 0;
void loop() {
  if (refleshDisplay)                        // If CAN0_INT pin is low, read receive buffer
  {
    refleshDisplay = false;

    display.clearDisplay();
    display.drawBitmap(0, 0, hotwatermark, 16, 16, 1);
    display.setCursor(17, 0);
    display.print(coolanttemp);
    display.setCursor(64, 0);
    display.println(tacho);
    display.setCursor(0, 16);
    display.print(vspeed);
    display.drawBitmap(64, 16, batterymark, 8, 16, 1);
    display.setCursor(73, 16);
    display.println(HVlife);
    display.display();
    
    Serial.println("reflesh!");
  }
  if(millis() - prevtimemillis > 500){
    data[2] = 0x05; // coolant temp
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    data[2] = 0x0c; // tacho
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    data[2] = 0x0d; // speed
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    data[2] = 0x5B; // HV battery life
    CAN0.sendMsgBuf(0x18DB33F1, CAN_EXTID, 8, data);
    prevtimemillis = millis();
  }
}


// 1. Set the camera to JEPG output mode.
// 2. if server.on("/capture", HTTP_GET, serverCapture),it can take photo and send to the Web.
// 3.if server.on("/stream", HTTP_GET, serverStream),it can take photo continuously as video 
//streaming and send to the Web.

//led bar

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
const int Led = D10;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, Led, NEO_GRB + NEO_KHZ800);
int ledType = 0;
bool isAuto = false;
const int t[] = {0,170,250};

//gy30
//#include <Wire.h> //BH1750 IIC Mode
#include <math.h> 
const int BH1750address = 0x23; //setting i2c address
byte buff[2];
uint16_t val=0;
char str[80];
String temp;
int Llevel=0;

//water check

#define Grove_Water_Sensor A0 // Attach Water sensor to Arduino Digital Pin 8
bool haveWater = false;
char *WaterStr[] = {"Water level: Empty","Water level: Low","Water level: Medium","Water level: High"};
int WaterState = 0;
int waterVal = 0;
const int ledPin = D2;

//cam
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include <Wire.h>
#include <ArduCAM.h>
#include <SPI.h>
#include "memorysaver.h"
#if !(defined ESP8266 )
#error Please select the ArduCAM ESP8266 UNO board in the Tools/Board
#endif

//This demo can only work on OV5642_MINI_5MP or OV5642_MINI_5MP_BIT_ROTATION_FIXED
//or OV5640_MINI_5MP_PLUS or ARDUCAM_SHIELD_V2 platform.
#if !(defined (OV5642_MINI_5MP) || defined (OV5642_MINI_5MP_BIT_ROTATION_FIXED) || defined (OV5642_MINI_5MP_PLUS) ||(defined (ARDUCAM_SHIELD_V2) && defined (OV5642_CAM)))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif

// set GPIO16 as the slave select :
const int CS = D9;

//you can change the value of wifiType to select Station or AP mode.
//Default is AP mode.
int wifiType = 0; // 0:Station  1:AP

//AP mode configuration
//Default is arducam_esp8266.If you want,you can change the AP_aaid  to your favorite name
const char *AP_ssid = "arducam_esp8266"; 
//Default is no password.If you want to set password,put your password here
const char *AP_password = "";

//Station mode you should put your ssid and password
const char* ssid = "wingwing-2.4G"; // Put your SSID here
const char* password = "12341234"; // Put your PASSWORD here

ESP8266WebServer server(80);
ArduCAM myCAM(OV5642, CS);

void start_capture(){
  myCAM.clear_fifo_flag();
  delay(0);
  myCAM.start_capture();
}

void camCapture(ArduCAM myCAM){
  WiFiClient client = server.client();
  yield();
  size_t len = myCAM.read_fifo_length();
  if (len >= MAX_FIFO_SIZE){
    Serial.println("Over size.");
    return;
  }else if (len == 0 ){
    Serial.println("Size is 0.");
    return;
  }
  
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  #if !(defined (OV5642_MINI_5MP_PLUS) ||(defined (ARDUCAM_SHIELD_V2) && defined (OV5642_CAM)))
  SPI.transfer(0xFF);
  #endif
  if (!client.connected()) return;
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: image/jpeg\r\n";
  response += "Content-Length: " + String(len) + "\r\n\r\n";
  server.sendContent(response);
  static const size_t bufferSize = 4096;
  static uint8_t buffer[bufferSize] = {0xFF};
  yield();
  while (len) {
      size_t will_copy = (len < bufferSize) ? len : bufferSize;
      myCAM.transferBytes(&buffer[0], &buffer[0], will_copy);
      if (!client.connected()) break;
      client.write(&buffer[0], will_copy);
      len -= will_copy;
  }
  
  myCAM.CS_HIGH();
}
//web
void serverCapture(){
  delay(0);
  start_capture();
  Serial.println("CAM Capturing");

  int total_time = 0;
  total_time = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  total_time = millis() - total_time;
  Serial.print("capture total_time used (in miliseconds):");
  Serial.println(total_time, DEC);
  total_time = 0;
  Serial.println("CAM Capture Done!");
  total_time = millis();
  camCapture(myCAM);
  total_time = millis() - total_time;
  Serial.print("send total_time used (in miliseconds):");
  Serial.println(total_time, DEC);
  Serial.println("CAM send Done!");
}

void serverStream(){

  WiFiClient client = server.client();
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  while (1){
    start_capture();   
    while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK)); 
    size_t len = myCAM.read_fifo_length();
    if (len >= MAX_FIFO_SIZE){
      Serial.println("Over size.");
      continue;
    }else if (len == 0 ){
      Serial.println("Size is 0.");
      continue;
    }

    myCAM.CS_LOW();
    myCAM.set_fifo_burst(); 
    #if !(defined (OV5642_MINI_5MP_PLUS) ||(defined (ARDUCAM_SHIELD_V2) && defined (OV5642_CAM)))
    SPI.transfer(0xFF);
    #endif   
    if (!client.connected()) break;
    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);
    
    static const size_t bufferSize = 4096;
    static uint8_t buffer[bufferSize] = {0xFF};
    
    while (len) {
      size_t will_copy = (len < bufferSize) ? len : bufferSize;
      myCAM.transferBytes(&buffer[0], &buffer[0], will_copy);
      if (!client.connected()) break;
      client.write(&buffer[0], will_copy);
      len -= will_copy;
    }
    myCAM.CS_HIGH();
    if (!client.connected()) break;
  }
}

void getLight(){
    Serial.println("getLight");
    Serial.print(lightCheck(),DEC); 
  server.send(200, "text/plain", String(lightLevel()));
}

void getWater(){
    Serial.println("getWater");
    Serial.println(waterVal);
  server.send(200, "text/plain", String(waterCheck()));
}

void getLedMod(){
    Serial.println("getLedMod");
  server.send(200, "text/plain", String(isAuto));
}

void getLedType(){
    Serial.println("getLedType");
  server.send(200, "text/plain", String(ledType));
}

void setLedAutoOn(){
    Serial.println("setLedAutoOn");
  isAuto = true;
  server.send(200, "text/plain", String(isAuto));
}

void setLedAutoOff(){
    Serial.println("setLedAutoOff");
  isAuto = false;
  server.send(200, "text/plain", String(isAuto));
}

void lightLV0(){
    Serial.println("lightLV0");
 ledType = 0;
  server.send(200, "text/plain", String(ledType));
}
void lightLV1(){
    Serial.println("lightLV1");
 ledType = 1;
  server.send(200, "text/plain", String(ledType));
}
void lightLV2(){
    Serial.println("lightLV2");
 ledType = 2;
  server.send(200, "text/plain", String(ledType));
}
void lightLV3(){
    Serial.println("lightLV3");
 ledType = 3;
  server.send(200, "text/plain", String(ledType));
}


void handleNotFound(){
  String message = "Server is running!\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  server.send(200, "text/plain", message);
  
  if (server.hasArg("ql")){
    int ql = server.arg("ql").toInt();
    myCAM.OV5642_set_JPEG_size(ql);
    delay(1000);
    Serial.println("QL change to: " + server.arg("ql"));
  }
}

void setup() {

//server & cam
  uint8_t vid, pid;
  uint8_t temp;
#if defined(__SAM3X8E__)
  Wire1.begin();
#else
  Wire.begin();
#endif
  Serial.begin(115200);
  Serial.println("ArduCAM Start!");

  // set the CS as an output:
  pinMode(CS, OUTPUT);

  // initialize SPI:
  SPI.begin();
  SPI.setFrequency(4000000); //4MHz

  //Check if the ArduCAM SPI bus is OK
  myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
  temp = myCAM.read_reg(ARDUCHIP_TEST1);
  if (temp != 0x55){
    Serial.println("SPI1 interface Error!");
    while(1);
  }
  //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
   if((vid != 0x56) || (pid != 0x42)){
   Serial.println("Can't find OV5642 module!");
   while(1);
   }
   else
   Serial.println("OV5642 detected.");
 

  //Change to JPEG capture mode and initialize the OV5642 module
  myCAM.set_format(JPEG);
   myCAM.InitCAM();
   myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
   myCAM.OV5642_set_JPEG_size(OV5642_320x240);
   delay(1000);
  if (wifiType == 0){
    if(!strcmp(ssid,"SSID")){
       Serial.println("Please set your SSID");
       while(1);
    }
    if(!strcmp(password,"PASSWORD")){
       Serial.println("Please set your PASSWORD");
       while(1);
    }
    // Connect to WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("WiFi connected");
    Serial.println("");
    Serial.println(WiFi.localIP());
  }else if (wifiType == 1){
    Serial.println();
    Serial.println();
    Serial.print("Share AP: ");
    Serial.println(AP_ssid);
    Serial.print("The password is: ");
    Serial.println(AP_password);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_ssid, AP_password);
    Serial.println("");
    Serial.println(WiFi.softAPIP());
  }

  //water check

  Serial.println();
  Serial.println("Start  Water_Sensor");
  pinMode(Grove_Water_Sensor, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.println("End Water_Sensor");

//gy-30


//led bar

  Serial.println("Start  Led");
  strip.begin();
  strip.setBrightness(50);
  strip.show(); // Initialize all pixels to 'off'
  Serial.println();
  Serial.println("Led line");
  Serial.println("Start  Led");
  delay(0);
  
  
  // Start the server
  server.on("/capture", HTTP_GET, serverCapture);
  //server.on("/stream", HTTP_GET, serverStream);


  server.on("/level=0", HTTP_GET, lightLV0);
  server.on("/level=1", HTTP_GET, lightLV1);
  server.on("/level=2", HTTP_GET, lightLV2);
  //server.on("/level=3", HTTP_GET, lightLV3);
  
  server.on("/atuoL=ON", HTTP_GET, setLedAutoOn);
  server.on("/atuoL=OFF", HTTP_GET, setLedAutoOff);

  
  server.on("/getLight", HTTP_GET, getLight);
  server.on("/getWater", HTTP_GET, getWater);
  server.on("/getLedMod", HTTP_GET, getLedMod);
  server.on("/getLedType", HTTP_GET, getLedType);

  
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Server started");
}
unsigned long previousMillis = 0;  const long interval = 2000;  
void loop() {
  delay(0);
  unsigned long currentMillis = millis();
  server.handleClient();

  
 if (currentMillis - previousMillis >= interval) {
  previousMillis = currentMillis;
    waterCheck();
    //led
    lightLevel();
    if(!isAuto)
        colorWipe(strip.Color(t[ledType], t[ledType], t[ledType]), 1);
      else{
        //Serial.println("new client");
        if(val < 120)
          colorWipe(strip.Color(t[2], t[2], t[2]), 1);     
        else  if(val < 200)
          colorWipe(strip.Color(t[1], t[1], t[1]), 1);
        else  
          colorWipe(strip.Color(t[0], t[0], t[0]), 1);
        }
 }

}

// gy30

int lightLevel(){
 int str_len = sprintf(str, "%d", lightCheck());
    //Serial.print(lightCheck(),DEC);     
    temp = str;
    int L = temp.toInt();
    if(L<30){return 0;}
    if(L<120){return 1;}
    if(L<200){return 2;}
    if(L<300){return 3;}
    if(L>300){return 4;}
  
}

uint16_t lightCheck(){
  delay(0);
  BH1750_Init(BH1750address);
  delay(200);
 
  if(2==BH1750_Read(BH1750address))
  {
    val=((buff[0]<<8)|buff[1])/1.2;
    return val;
  }
  return 0;
}

void BH1750_Init(int address)
{
   Wire.beginTransmission(address);
   Wire.write(0x10); //1lx reolution 120ms
   Wire.endTransmission();
}

int BH1750_Read(int address)
{
   int z=0;
   Wire.beginTransmission(address);
   Wire.requestFrom(address, 2);
   while(Wire.available()) 
   {
      buff[z] = Wire.read();  // receive one byte
      z++;
   }
   Wire.endTransmission();  
   return z;
}

//water check

String waterCheck(){
  //Serial.println("waterCheck");
  //Serial.println(analogRead(Grove_Water_Sensor));
  delay(0);
   waterVal = analogRead(Grove_Water_Sensor);
   Serial.println(waterVal);
  if( waterVal <= 200) {
    WaterState =0;//>25%
    haveWater = false;
  }else if( waterVal > 200 && waterVal <=300) {
    WaterState =1;//~25-50%
    haveWater = false;
  }else if( waterVal > 300 && waterVal <=400) {
    WaterState =2;//~50-75%
    haveWater = true;
  }else if( waterVal > 400) {
    WaterState =3;//75%~
    haveWater = true;
  }

  if(haveWater){
    digitalWrite(ledPin, LOW);
  }else{
    digitalWrite(ledPin, HIGH);
  }

  return WaterStr[WaterState];
}

//led bar

void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t k=0; k<strip.numPixels(); k++) {
    strip.setPixelColor(k, c);
    strip.show();
    delay(wait);
  }
}

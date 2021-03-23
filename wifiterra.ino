#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <RtcDS1307.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "CTBot.h"
CTBot myBot;  

#define ONE_WIRE_BUS D4
#define DHTPIN D7
#define DHTTYPE    DHT11
#define countof(a) (sizeof(a) / sizeof(a[0]))
DHT_Unified dht(DHTPIN, DHTTYPE);

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
RtcDS1307<TwoWire> Rtc(Wire);
bool first_start = true;
const int hoodPin = D6; 
const int loadPin = D5;
const int ledPin = D7; //TODO: введен новый пин под нагрузку
const int admin_id = 147391724; // my id in telegram
const float sensor_tolerance = 2.0;
/*sensors*/
enum sensors_type { HUM, TEMP};
int deviceCount = 0;
float p_t1,p_t2,p_t3,p_t4,p_t5,p_dht_t,p_dht_hum; //previos_sensors
float t1,t2,t3,t4,t5,dht_t,dht_hum; //sensors
/*end sensors*/
char datestring[20];
/* sun */
int duty_position = 400;
int sun_step = 10;
bool duty_mode;
bool sunrize, sunset;
int sun_position;
/*end sun */
/*hood - fan*/
int hood_pos = 0;
bool hood_auto = true;
const int hood_step = 100;
/*end_hood - fan*/
bool debug = true;
/* Temperature 
sensor addresses:
  Sensor 1 : 0x28, 0xFF, 0xAA, 0x9D, 0x61, 0xE0, 0xB6, 0x2E
  Sensor 2 : 0x28, 0xFF, 0x01, 0xA2, 0x61, 0xE0, 0x82, 0x60
  Sensor 3 : 0x28, 0xFF, 0x53, 0xA7, 0x61, 0xE0, 0xE3, 0xCC
  Sensor 4 : 0x28, 0xFF, 0xDF, 0x75, 0x61, 0xE0, 0x1D, 0x4B
  Sensor 5 : 0x28, 0xFF, 0x3F, 0x9D, 0x61, 0xE0, 0x5E, 0x60
*/
uint8_t ds1[8] = { 0x28, 0xFF, 0x01, 0xA2, 0x61, 0xE0, 0x82, 0x60 };
uint8_t ds2[8] = { 0x28, 0xFF, 0xAA, 0x9D, 0x61, 0xE0, 0xB6, 0x2E };
uint8_t ds3[8] = { 0x28, 0xFF, 0x53, 0xA7, 0x61, 0xE0, 0xE3, 0xCC };
uint8_t ds4[8] = { 0x28, 0xFF, 0x3F, 0x9D, 0x61, 0xE0, 0x5E, 0x60 };
uint8_t ds5[8] = { 0x28, 0xFF, 0xDF, 0x75, 0x61, 0xE0, 0x1D, 0x4B };
/* End Temperature */

/* TELEGRAM */
String ssid = "ASUS-Sol";
String pass = "wifihoshii";
String token = "1659853725:AAEGzJHnJgrPVbTgL1FbdLn9tLtPPNaQN7I";   
/* END TELEGRAM*/

void setup() {
  Serial.begin(9600);
  sun_position=0;
  sunrize=false;
  sunset=false;
  sunrize=true;
  pinMode(hoodPin, OUTPUT);
  dht.begin();
  Rtc.Begin();
  u8g2.begin();
  initDsSensors();
  initTelegramBot();
}

/* init subsystem*/
void initTelegramBot(){
  if(debug)
    Serial.println("Starting TelegramBot...");
  myBot.wifiConnect(ssid, pass);
  myBot.setTelegramToken(token);
  if (myBot.testConnection()){
    if(debug)
    Serial.println("\ntestConnection OK");
    myBot.sendMessage(admin_id, "wifi_terra online!");
  }
  else
    if(debug)
    Serial.println("\ntestConnection NOK"); 
  }

void initDsSensors(){
  sensors.begin();
  // найти устройства на шине
  if(debug)
    {
      Serial.print("Locating devices...");
      Serial.print("Found ");
    }
  deviceCount = sensors.getDeviceCount();
  if(debug)
    {
    Serial.print(deviceCount, DEC);
    Serial.println(" devices.");
    Serial.println("");
    }
  }
/* end init subsystem*/

void loop() {
  fn_hood_auto(); // влажность авто
  update_analog_pins();
  sun();
  sensor_polling();
  pullup();
  printScreen();
  if(sun_position >= 1024){
    night_mode();
    }
  else {
    morning_mode();
  }
  TBMessage msg;
  if (myBot.getNewMessage(msg)) {
    if (msg.text == "/status"){
      myBot.sendMessage(msg.sender.id, String(msg.sender.id) + "msg:"+ fn_telegram_status());
      }
   telegramHoodEvent(msg);
  }
  delay(1000);
  //duty_mode_on();
}
/* <<<---------------------- SENSORS ----------------------------->>>*/
void sensor_polling(){
  checkDht();
  checkClock();
  checkTemp();
}

void checkDht(){
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature) && debug) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    if(debug){
    Serial.print(F("dht11_T: "));
    Serial.println(event.temperature);
    }
    dht_t = event.temperature;
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity) && debug) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    if(debug){
    Serial.print(F("dht11_hum:"));
    Serial.println(event.relative_humidity);
    }
    dht_hum = event.relative_humidity;
  }
}
  
void checkClock(){
  RtcDateTime dt = Rtc.GetDateTime();
  snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            //dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
  }

void checkTemp(){
  sensors.requestTemperatures();
  t1 = sensors.getTempC(ds1);
  t2 = sensors.getTempC(ds2);
  t3 = sensors.getTempC(ds3);
  t4 = sensors.getTempC(ds4);
  t5 = sensors.getTempC(ds5);
  if(debug){
    Serial.print("ds1:");
    Serial.println(t1);
    Serial.print("ds2:");
    Serial.println(t2);
    Serial.print("ds3:");
    Serial.println(t3);
    Serial.print("ds4:");
    Serial.println(t4);
    Serial.print("ds5:");
    Serial.println(t5);
    }
  }
 /* <<<---------------------- END SENSORS ----------------------------->>>*/ 
 
void pullup(){
  if (t1<23){
    digitalWrite(loadPin, 1);
    }
  else {
    digitalWrite(loadPin, 0);
    }
}

/*<<<-----------------------SUN ------------------------------------>>>*/
void night_mode(){
  sunset = true;
  sunrize = false;
  }
void morning_mode(){
  sunset = false;
  sunrize = true;
  }

void duty_mode_on(){
  duty_mode = true;
  for (int i = 0;  i < duty_position;  i++)
    {
      analogWrite(ledPin, i);
      delay(50);
    }
  }
  
void duty_mode_off(){
  duty_mode = false;
  for (int i = duty_position;  i < 0 ;  i--)
    {
      analogWrite(ledPin, i);
      delay(50);
    }
  }
void sun(){
  if(sun_position <= 1024 && sunrize && !duty_mode){
    sun_position+=sun_step;
    analogWrite(ledPin, sun_position);
    }
  if (sun_position >= 0 && sunset && !duty_mode){
    sun_position-=sun_step;
    analogWrite(ledPin, sun_position);
  }
}
/*<<<----------------------- END SUN ------------------------------------>>>*/


/* <<<-------------------- SCREEN ------------------------------------- >>>
 ________________
|t   i   m   e   |
|t1   dhtT  t4   |
|sun  t2    ____ |
| ?   %     t5   |
_________________
*/
void printScreen() {
  u8g2.clearBuffer();         // clear the internal memory
  u8g2.setFont(u8g2_font_iranian_sans_14_t_all);
  //u8g2.setFont(u8g2_font_ncenB08_tr); // choose a suitable font
  u8g2.setCursor(0,10);
  u8g2.print(datestring);  // write something to the internal memory
  //u8g2.setFont(u8g2_font_iranian_sans_14_t_all);
  u8g2.setCursor(0,30);
  u8g2.print(t1,2); //t1
  u8g2.setCursor(0,64);
  u8g2.print(t2,2); //t2
  u8g2.setCursor(44,45);
  u8g2.print(t3,2); //t3
  u8g2.setCursor(88,30);
  u8g2.print(t4,2); //t4
  u8g2.setCursor(88,64);
  u8g2.print(t5,2); //t5
  u8g2.setCursor(0,44);
  u8g2.print(sun_position); //sun
  u8g2.setCursor(44,64);
  u8g2.print(dht_hum,2); // dht %
  u8g2.setCursor(44,30);
  u8g2.print(dht_t,2); // dht T
  u8g2.sendBuffer();          // transfer internal memory to the display
}
/* <<<-------------------- END SCREEN ------------------------------------- >>> */


/*<<<---------------------TELEGRAM BOT FUNCTIONS--------------------------->>>*/ 
void telegramHoodEvent(TBMessage t_msg){
  if (t_msg.text == "/hood:auto"){
      hood_auto = true;
      hood_pos = 500;
      myBot.sendMessage(t_msg.sender.id, "hood auto at" + String(hood_pos));
      }
    else if (t_msg.text == "/hood:off"){
      hood_auto = false;
      hood_pos = 0;
      myBot.sendMessage(t_msg.sender.id, String(hood_pos));
      }
    else if (t_msg.text == "/hood:up"){
      hood_auto = false;
      hood_pos += hood_step;
      myBot.sendMessage(t_msg.sender.id, String(hood_pos));
      }
    else if (t_msg.text == "/hood:down"){
      hood_auto = false;
      hood_pos -= hood_step;
      myBot.sendMessage(t_msg.sender.id, String(hood_pos));
      }
    else if (t_msg.text == "/hood:low"){
      hood_auto = false;
      hood_pos = 500;
      myBot.sendMessage(t_msg.sender.id, String(hood_pos));
      }
    else if (t_msg.text == "/hood:mean"){
      hood_auto = false;
      hood_pos = 800;
      myBot.sendMessage(t_msg.sender.id, String(hood_pos));
      }
    else if (t_msg.text == "/hood:hight"){
      hood_auto = false;
      hood_pos = 1024;
      myBot.sendMessage(t_msg.sender.id, String(hood_pos));
      }
  }
String fn_telegram_status(){
  String msg = float_param_to_str("ds1",t1)+","+
  float_param_to_str("ds2",t2)+","+
  float_param_to_str("ds3",t3)+","+
  float_param_to_str("ds4",t4)+","+
  float_param_to_str("ds5",t5)+","+
  float_param_to_str("dht_t",dht_t)+","+
  float_param_to_str("dht_hum",dht_hum);
  if(hood_pos > 0){
    msg += "; fan is " + String(hood_pos); //(1024 - hood_pos)*5.24 );
    }
  return msg;
}
/*<<<--------------------- END TELEGRAM BOT FUNCTIONS ------------------------->>>*/ 

/* FUNCTIONS */
void fn_hood_auto(){
  bool alert = dht_hum > 90 and dht_hum < 100;
  float hum = fn_sensor_average(p_dht_hum, dht_hum, alert);
  if (hood_auto){
      if(p_dht_hum >= 90.0){
        hood_pos = 1024;
      }
      else if(p_dht_hum <= 70.0){
        hood_pos = 0;
      }
      else if(p_dht_hum >= 80.0){
        hood_pos = 900;
      } 
      if(p_dht_hum == 90.0 || p_dht_hum == 80.0 || p_dht_hum == 70.0) {
        String mes =  "fn_hood_auto:"+String(hood_pos)+"; "+float_param_to_str("dht_hum",hum);
        myBot.sendMessage(admin_id,mes);
      }
    }
    p_dht_hum = hum;
  }
  
 
void update_analog_pins(){
  analogWrite(hoodPin, hood_pos);
  }
  
float fn_sensor_check(enum sensors_type){
  switch (sensors_type) {
  case SENSORS_TYPE_References_e::HUM: {
    /* do stuff */
    break;
  }
  case SENSORS_TYPE_References_e::TEMP: {
    /* do stuff */
    break;
  }
  default: {
    break;
  }

}
  }

float fn_sensor_average(float previos, float current, bool alert){
  if (fn_sensor_true(previos,current, alert))
    return current;
  else return previos;
}

bool fn_sensor_true(float previos, float current, bool alert){
  if (alert) 
    return true;
  else if(previos-current - current > sensor_tolerance and current - previos < sensor_tolerance)
    return true;
  else return false;
  }
void fn_set_hood_pos(float pos) {
    hood_pos = pos;
  }
  
String float_param_to_str(String param, float value){
  char outstr[15];
  dtostrf(value,7, 3, outstr);
  return param + ":" + value;
}
/* END FUNCTIONS */

/*
 2019年9月26日  esp8266 把gps经纬度 bme280温湿度气压数据发送到 aprs udp协议, Traccar http协议, mqtt协议对接telegraf写入influxdb数据库
针脚对应关系：GPIO6~GPIO11不要使用，否则引起存储错误，不停重启；  
arduino  esp8266   
D0 -->  GPIO16  user仅可以作为输出，不能输入
D1 -->  GPIO5
D2 -->  GPIO4
D3 -->  GPIO0
D4 -->  GPIO2   tx1
D5 -->  GPIO14
D6 -->  GPIO12
D7 -->  GPIO13  rx2
D8 -->  GPIO15  tx2
D9 -->  GPIO3   rx0
D10 --> GPIO1   tx0
A0  --> ADC0    模拟量输入

2019年9月27日 增加了gps数据是0的时候，不发送任何数据
2019年9月28日 增加了震动传感器
*/

#include <TinyGPS++.h>   //GPS库
#include <SoftwareSerial.h>  //读取PGS软串口库
#include <ESP8266WiFi.h>   //esp8266 wifi
#include <WiFiUdp.h> //提供UDP相关功能

//BME280
#include <Adafruit_BME280.h>   //温湿度压力传感器库
#include <Wire.h>
//bme280  iic 接口设置************************************ bme280  iic 接口设置*****************
#define SDA D5      //BME280 I2C在nodemcu的GPIO14
#define SDC D6      //BME280 I2C在nodemcu的GPIO12
//bme280 初始化
Adafruit_BME280 bme280;
bool bme280_CanRead = true;
//Tcrccat 服务器地址************************Tcrccat 服务器地址**********************************
//const char* traccar_host = "148.70.*";  //Tcrccat 服务器地址
const char* traccar_host = "129.28.*";
const int traccar_httpPort = 5055;  //Tcrccat 服务器端口
//traccar 名字*********************************traccar 名字*********************************
String traccar_id = "BH4TNA-12";
//判断280针脚地址
bool initBME280(char addr) {
  if (bme280.begin(addr)) {
    Serial.println("bme280 已找到");
    return true;
  } else {
    Serial.println("bme280 没找到");
    return false;
  }
}
//UDP服务器设置
//aprs udp服务器地址************************aprs udp服务器地址**************************************
unsigned int UDPremotePort = 14580;  //端口号
static unsigned char UDPremoteIP[4] = {202, 141, *} ; //UDP服务器地址
//static unsigned char UDPremoteIP[4] = {123, 206, *} ; //UDP服务器地址
WiFiUDP Udp;
//aprs 名字*********************************aprs 名字*********************************
String aprs_id = "BH4TNA-12";
TinyGPSPlus gps;  // The TinyGPS++ object  gps初始化
//gps软串口设置*******************************************gps软串口设置******************************
SoftwareSerial ss(4, 5); // 接收gps的软串口，rx在nodemcu的GPIO4
//wifi设置****************************wifi设置****************************wifi设置******************
const char* ssid = "*";    //路由器ssid
const char* password = "*";  // 密码
float latitude , longitude;  //浮点型定义经纬度变量
int year , month , date, hour , minute , second; //定义年月日
String date_str , time_str , lat_str , lng_str;
int pm;
//上传aprs数据周期
unsigned long uptime = 0;
unsigned long last_send = 0;
//循环发送时间************************循环发送时间************************循环发送时间*****************
const unsigned long sending_interval = 10000; //传递一次数据
const unsigned long zhendong_interval = 1000; //传递一次数据
//定义http服务器地址
WiFiServer server(80);

//mqtt开始
#include <PubSubClient.h>   //mqtt库
//mqtt服务器ip**************************************mqtt服务器ip***********************************
const char* mqtt_server = "129.28.*";  //mqtt服务器ip
WiFiClient espClient;
PubSubClient espclient(espClient);
long lastMsg = 0; //定义多长时间循环执行一次
int lastMsg_temp = 5000; //定义5秒多长时间循环执行一次

String bmp280_mqtt_outTopic_msg = "";//存储发往服务器的mqtt信息
//mqtt订阅的主题*************************************mqtt订阅的主题*********************************
String inTopic = "8266in";  //接收(订阅)的mqtt主题
String outTopic = "/uolian/esp8266"; // 发布mqtt的主题
int zhendong;
int zhendong_pin = 15;  //定义震动传感器输入针脚



void setup()
{
  Serial.begin(115200);
  Serial.println("kaisi");
  //读取gps定位
  // readgps();
  //mqtt开始
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  espclient.setServer(mqtt_server, 1883);
  espclient.setCallback(callback);
  //mqtt结束
  ss.begin(38400);  //gps模块波特率
  //连接无线路由器
  Serial.println();
  Serial.print("连接 ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(50);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi 已连接");
  server.begin();
  Serial.println("服务器已经启动");
  // Print the IP address
  Serial.print("本机ip ");
  Serial.println(WiFi.localIP());

  //BME280 设置
  Wire.pins(SDC, SDA);
  Wire.begin(SDC, SDA);
  // BME280 地址判断
  if (bme280_CanRead) {
    if (!initBME280(0x76) && !initBME280(0x77)) {
      Serial.println("检测BME280连线");
      bme280_CanRead = false;
    }
  }
  WiFiClient client;  //创建一个客户端实例
  client.connect(traccar_host, traccar_httpPort);

  pinMode(zhendong_pin, INPUT); //震动传感器针脚定义成输入
}
void loop()
{
  //读取gps定位
  readgps();
  WiFiClient client;  //创建一个客户端实例
  uptime = millis();
  int zhendong_n=digitalRead(zhendong_pin);
  if ( zhendong_n == HIGH && last_send + zhendong_interval < uptime ) //判断有无震动
  {
  zhendong=1;
 // Serial.println("正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动正在震动");
  }
  if (last_send == 0 || last_send + sending_interval < uptime || zhendong==1) {   //sending_interval是个变量
    zhendong=0;
    //处理lat 经度
    latitude = gps.location.lat();  //经度
    lat_str = String(latitude , 6); //浮点转字符串
    float Aprs_lat = lat_str.substring(0, 2).toFloat() * 100 + lat_str.substring(3, 10).toFloat() / 1000000 * 60 ; //39.851087N，转成aprs：39*100+0.851087*60=3951.06
    //lat_str.substring(0,2).toFloat()*100 ，截取39.851087中的39*100
    char char_lat[] = "";
    dtostrf( Aprs_lat, 3, 2, char_lat); //浮点转char,3代表小数点前面取3位，2代表小数点后面取2位，
    String str_lat;
    String N = "N";
    str_lat = char_lat + N; //根据aprs要求拼接+N
    //    Serial.println( str_lat);
    //处理lng 纬度
    longitude = gps.location.lng();
    lng_str = String(longitude , 6);
    float Aprs_lng = lng_str.substring(0, 3).toFloat() * 100 + lng_str.substring(4, 10).toFloat() / 1000000 * 60 ; //经度116.326323E，转成aprs：116*100+0.326323*60=11619.58
    char char_lng[] = "";
    String str_lng;
    dtostrf( Aprs_lng, 4, 2, char_lng);
    String E = "E";
    str_lng = char_lng + E;
    //Serial.println(str_lng);

    //处理温度，改成aprs标准数据 t华氏度=（摄氏度*9/5）+32 ，处理小于10  小于100  小于1000
    float Temperature = (bme280.readTemperature() * 9 / 5) + 32;
    //Serial.print("bme280温度:");
    //Serial.println(Temperature);

    char char_Temperature[] = "";
    dtostrf( Temperature, 2, 0, char_Temperature);
    String T = "0";
    String Aprs_Temperature = T + char_Temperature;
    //  Serial.println(Aprs_Temp);

    //处理湿度，处理小于10
    char Humidty[] = "";
    dtostrf( bme280.readHumidity(), 2, 0, Humidty);
    //Serial.print("bme280湿度:");
    //Serial.println(Humidty);
    String Aprs_Humidty = Humidty;

    //处理大气压，处理小于
    char Pressure[] = "";
    dtostrf(bme280.readPressure() / 10, 4, 0, Pressure);
    String Aprs_Pressure = Pressure;

    //拼接aprs数据格式
    String Aprs_string = "" + aprs_id + ">438.500Mhz:=" + str_lat + "/" + str_lng + "_" + "000/000g000t" + Aprs_Temperature + "r000p000h" + Aprs_Humidty + "b" + Aprs_Pressure + " 温度:" + bme280.readTemperature() + "°C" + " 湿度:" + bme280.readHumidity() + "%" + " 大气压:" + bme280.readPressure() / 100 + "mpar" + " 海拔:" + gps.altitude.meters() + "米" + " 速度:" + gps.speed.kmph() + "千米/时" + " 里程:" + gps.course.deg() + "公里" + " 卫星数量:" + gps.satellites.value() + "颗" + " 水平分量精度因子:" + gps.hdop.value()  + "\r\n" + "\r\n";
    //拼接出aprs后string 转char
    char Aprs_char[Aprs_string.length() + 1];  //定义数组
    Aprs_string.toCharArray(Aprs_char, Aprs_string.length() + 1);
    //高度速度*****************************************高度速度*****************************************高度速度*****************************************高度速度*****************************************
    //    Serial.print("高度:");
    //    Serial.println(gps.altitude.meters());
    //    Serial.print("速度:");
    //    Serial.println(gps.speed.kmph());

    //    Serial.print("lat_str:");
    //    Serial.println(lat_str.toInt());

    if (lat_str.toInt() == 0)   //当收不到gps数据时，不发送经纬度信息
    {
      Serial.println("GPS数据是0.不发送经纬度数据");
      //mqtt开始
      if (!espclient.connected()) {
        reconnect();
      }
      //espclient.loop();
      bmp280_mqtt_outTopic_msg.concat("{\"temp_bme280\":");
      bmp280_mqtt_outTopic_msg.concat(bme280.readTemperature()); //mqtt温度
      bmp280_mqtt_outTopic_msg.concat(",\"pres\": ");
      bmp280_mqtt_outTopic_msg.concat(bme280.readPressure());  //mqtt大气压
      bmp280_mqtt_outTopic_msg.concat(",\"humi\": ");// 湿度
      bmp280_mqtt_outTopic_msg.concat(bme280.readHumidity());
      bmp280_mqtt_outTopic_msg.concat("}");
      Serial.print("发往mqtt:");
      Serial.println(bmp280_mqtt_outTopic_msg);
      espclient.publish(outTopic.c_str(), bmp280_mqtt_outTopic_msg.c_str()); //mqtt最多能发送114个字符(含),多了不行
      //Serial.println("MQTT写入成功"); //串口输出写入成功
      bmp280_mqtt_outTopic_msg = "";
      //mqtt结束

      //发往aprs*******************************************发往aprs*******************************************发往aprs*******************************************发往aprs********************************
      Serial.print("发往Aprs:");
      Serial.println(Aprs_char);
      //以下udp  aprs
      Udp.beginPacket(UDPremoteIP, UDPremotePort);
      Udp.write(Aprs_char);
      Udp.endPacket();

      //发往client http traccar*****************************************发往client http traccar*****************************************发往client http traccar*****************************************
      String url = "GET /?id=";
      url += traccar_id;
      url += "&wendu=";
      url += lng_str;
      url += " HTTP/1.1\r\n";
      url += "Connection: keep-alive\r\n\r\n";

      if (client.connected()) {
        client.print(String(url));
      }
      else {
        client.connect(traccar_host, traccar_httpPort);
        client.print(String(url));
        Serial.print("发往traccar:");
        Serial.println(url);
      }
      //  client.print(String(url));
    }
    else   //当收到gps数据时，发送包含经纬度信息的全部信息
    {

      //发往mqtt****************************************************发往mqtt****************************************************发往mqtt****************************************************
      //mqtt开始
      if (!espclient.connected()) {
        reconnect();
      }
      //espclient.loop();
      bmp280_mqtt_outTopic_msg.concat("{\"temp_bme280\":");
      bmp280_mqtt_outTopic_msg.concat(bme280.readTemperature()); //mqtt温度
      bmp280_mqtt_outTopic_msg.concat(",\"pres\": ");
      bmp280_mqtt_outTopic_msg.concat(bme280.readPressure());  //mqtt大气压
      bmp280_mqtt_outTopic_msg.concat(",\"humi\": ");// 湿度
      bmp280_mqtt_outTopic_msg.concat(bme280.readHumidity());
      bmp280_mqtt_outTopic_msg.concat(",\"lat\": ");// 经度
      bmp280_mqtt_outTopic_msg.concat(lat_str);
      bmp280_mqtt_outTopic_msg.concat(",\"lng\": ");
      bmp280_mqtt_outTopic_msg.concat(lng_str);
      bmp280_mqtt_outTopic_msg.concat(",\"alt\": ");
      bmp280_mqtt_outTopic_msg.concat(gps.altitude.meters());  //gps高度
      bmp280_mqtt_outTopic_msg.concat("}");
      Serial.print("发往mqtt:");
      Serial.println(bmp280_mqtt_outTopic_msg);
      espclient.publish(outTopic.c_str(), bmp280_mqtt_outTopic_msg.c_str()); //mqtt最多能发送114个字符(含),多了不行
      //Serial.println("MQTT写入成功"); //串口输出写入成功
      bmp280_mqtt_outTopic_msg = "";
      //mqtt结束

      //发往aprs*******************************************发往aprs*******************************************发往aprs*******************************************发往aprs********************************
      Serial.print("发往Aprs:");
      Serial.println(Aprs_char);
      //以下udp  aprs
      Udp.beginPacket(UDPremoteIP, UDPremotePort);
      Udp.write(Aprs_char);
      Udp.endPacket();

      //发往client http traccar*****************************************发往client http traccar*****************************************发往client http traccar*****************************************
      String url = "GET /?id=";
      url += traccar_id;
      url += "&lat=";
      url += lat_str;
      url += "&lon=";
      url += lng_str;
      url += "&wendu=";
      url += lng_str;
      url += " HTTP/1.1\r\n";
      url += "Connection: keep-alive\r\n\r\n";

      if (client.connected()) {
        client.print(String(url));
      }
      else {
        client.connect(traccar_host, traccar_httpPort);
        client.print(String(url));
        Serial.print("发往traccar:");
        Serial.println(url);
      }
      //  client.print(String(url));
    }
    last_send = uptime;  //放错地方,会一直发数据,切记!
  }
}










//以下是函数勿动*********************************************以下是函数勿动*********************************************以下是函数勿动*********************************************
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("temp");

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!espclient.connected()) {
    Serial.println("尝试连接MQTT服务器...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (espclient.connect(clientId.c_str())) {
      Serial.println("MQTT服务器连接成功");
      // Once connected, publish an announcement...
      espclient.publish(outTopic.c_str(), "hello MQTT");
      // ... and resubscribe
      espclient.subscribe(inTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(espclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      // delay(5000);
    }
  }
}
//mqtt结束

void readgps() {
  // 读取gps数据,勿动
  while (ss.available() > 0)
    if (gps.encode(ss.read()))
    {
      if (gps.location.isValid())
      {
        latitude = gps.location.lat();
        lat_str = String(latitude , 6);
        //        Serial.print("lat:");
        //        Serial.print (lat_str);
        longitude = gps.location.lng();
        lng_str = String(longitude , 6);
        //        Serial.print("  lng:");
        //        Serial.println(lng_str);
      }

      //HTTP
      if (gps.date.isValid())
      {
        date_str = "";
        date = gps.date.day();
        month = gps.date.month();
        year = gps.date.year();

        if (date < 10)
          date_str = '0';
        date_str += String(date);

        date_str += " / ";

        if (month < 10)
          date_str += '0';
        date_str += String(month);

        date_str += " / ";

        if (year < 10)
          date_str += '0';
        date_str += String(year);
      }

      if (gps.time.isValid())
      {
        time_str = "";
        hour = gps.time.hour();
        minute = gps.time.minute();
        second = gps.time.second();

        minute = (minute + 30);
        if (minute > 59)
        {
          minute = minute - 60;
          hour = hour + 1;
        }
        hour = (hour + 5) ;
        if (hour > 23)
          hour = hour - 24;

        if (hour >= 12)
          pm = 1;
        else
          pm = 0;

        hour = hour % 12;

        if (hour < 10)
          time_str = '0';
        time_str += String(hour + 2);

        time_str += " : ";

        if (minute < 10)
          time_str += '0';
        time_str += String(minute + 30);

        time_str += " : ";

        if (second < 10)
          time_str += '0';
        time_str += String(second);

        if (pm == 1)
          time_str += " PM ";
        else
          time_str += " AM ";

      }
    }

}

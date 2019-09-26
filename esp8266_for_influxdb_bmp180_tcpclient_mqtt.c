/***************************************************
  //编译环境arduino ide 1.8.8
   板卡esp8266
   传感器BMP180
   传感器接线 D4接SDA，  D5接SCL
   2019-9-20添加了mqtt
 ****************************************************/
//#include <WiFi.h>  //引入ESP32Wifi库
#include <ESP8266WiFi.h> //引入ESP8266Wifi库
const char *ssid = "Uolian-*";
const char *password = "*";
const IPAddress serverIP(148, 70, *, *); //欲访问的地址
uint16_t serverPort = 8086;//Influxdb时序数据库服务器端口号
WiFiClient client; //声明一个WIFI客户端对象，用于与服务器进行连接

//bmp180传感器
String bmp180_tcp_client_msg = "";//存储发往服务器的信息
String bmp180_tcp_client_msg_length = "";//存储发往服务器的信息长度
#include <Adafruit_BMP085.h> //bmp180库
Adafruit_BMP085 bmp;
//mqtt开始
#include <PubSubClient.h>   //mqtt库
const char* mqtt_server = "148.70.*.*";  //mqtt服务器ip
WiFiClient espClient;
PubSubClient espclient(espClient);
long lastMsg = 0; //定义多长时间循环执行一次
int lastMsg_temp = 5000; //定义5秒多长时间循环执行一次

String bmp180_mqtt_outTopic_msg = "";//存储发往服务器的mqtt信息
String inTopic = "inTopic";  //接收(订阅)的mqtt主题
String outTopic = "/sensor/60:01:94:0f:76:11"; // 发布mqtt的主题

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
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!espclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (espclient.connect(clientId.c_str())) {
      Serial.println("MQTT服务器连接成功");
      // Once connected, publish an announcement...
      espclient.publish(outTopic.c_str(), "hello world");
      // ... and resubscribe
      espclient.subscribe(inTopic.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(espclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
//mqtt结束
void setup() {
  //mqtt开始
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
  espclient.setServer(mqtt_server, 1883);
  espclient.setCallback(callback);
  //mqtt结束
  Serial.begin(115200);
  //esp32联网
  WiFi.mode(WIFI_STA);
  //WiFi.setSleep(false); //关闭STA模式下wifi休眠，提高响应速度，8266不屏蔽报错，esp32没事
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected");
  Serial.print("分配的IP地址:");
  Serial.println(WiFi.localIP());
  //bmp180传感器
  Wire.begin(2, 14); //定义I2C接口，并启动，ESP8266-12E的 2 和 14 口是原本定义为I2C接口，其他模块查看手册，用于读取BMP180的参数
  if (!bmp.begin()) {
    Serial.println("找不到BMP180传感器，请检查!");
    while (1) {}
  }
}
void loop() {
  Serial.println("尝试访问服务器");
  if (client.connect(serverIP, serverPort)) { //尝试访问目标地址
    Serial.println("访问成功");
    //while (client.connected() || client.available()) { //如果已连接或有收到的未读取的数据
    while (client.connected()) { //如果已连接或有收到的未读取的数据
      yield();
      long now = millis();
      if (now - lastMsg > lastMsg_temp) {
        lastMsg = now;
        Serial.println("last_msg**************************************************************************");

        //拼接存储发往服务器的信息：
        float readTemperature = bmp.readTemperature() + 0.4; //温度
        float readPressure = bmp.readPressure(); //大气压
        bmp180_tcp_client_msg.concat("BMP180,host=server01,region=us-west Temperature=");
        bmp180_tcp_client_msg.concat(readTemperature); //温度
        bmp180_tcp_client_msg.concat(",Pressure="); //多条数据用逗号隔开
        bmp180_tcp_client_msg.concat(readPressure); //大气压
        Serial.print("bmp180_tcp_client_msg:");
        Serial.println(bmp180_tcp_client_msg);
        //拼接存储发往服务器的信息长度
        bmp180_tcp_client_msg_length.concat("Content-Length: ");
        bmp180_tcp_client_msg_length.concat(bmp180_tcp_client_msg.length());


        //开始向服务器发送
        client.write("POST /write?db=aprstest HTTP/1.1"); //post头
        client.write(0x0D0A); //回车换行
        client.write("Host:");//host:
        client.write(0x0D0A); //回车换行
        client.write(bmp180_tcp_client_msg_length.c_str()); //tcp_client自动计算长度
        client.write(0x0D0A);//回车换行
        client.write(0x0D0A);//回车换行
        client.write("");
        client.write(bmp180_tcp_client_msg.c_str());
        //client.write(0x0D0A);
        client.write("\r\n");
        delay(50);
        Serial.println("tcp_client写入成功"); //串口输出写入成功


        //mqtt开始
        if (!espclient.connected()) {
          reconnect();
        }
        espclient.loop();
        bmp180_mqtt_outTopic_msg.concat("{\"temperature_bmp180\":");
        bmp180_mqtt_outTopic_msg.concat(readTemperature); //mqtt温度
        bmp180_mqtt_outTopic_msg.concat(",\"press_bmp180\": ");
        bmp180_mqtt_outTopic_msg.concat(readPressure);  //mqtt大气压
        bmp180_mqtt_outTopic_msg.concat("}");
        Serial.print("bmp180_mqtt_outTopic_msg:");
        Serial.println(bmp180_mqtt_outTopic_msg);
        espclient.publish(outTopic.c_str(), bmp180_mqtt_outTopic_msg.c_str());
        Serial.println("MQTT写入成功"); //串口输出写入成功
        //mqtt结束

        //      读取tcp_client服务器返回的内容
        Serial.println("读取到tcp_client数据：");
        while ( client.available()) //如果有收到的未读取的数据
        {
          if (client.available()) //如果有数据可读取
          {
            //String line = client.readStringUntil('\n'); //读取数据到换行符
            String line = client.readStringUntil('\r'); //读取数据到换行符
            Serial.print(line);
            //client.write(line.c_str()); //将收到的数据回发
          }
        }

        bmp180_tcp_client_msg = "";
        bmp180_tcp_client_msg_length = "";
        bmp180_mqtt_outTopic_msg = "";
        //delay(5000);
      }
    }
    Serial.println("关闭当前连接,写入tcp_client失败");
    client.stop(); //关闭客户端
  }
  else
  {
    Serial.println("访问失败");
    client.stop(); //关闭客户端
  }
  delay(5000);
}

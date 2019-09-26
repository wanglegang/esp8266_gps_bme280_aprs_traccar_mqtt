#include <WiFi.h>
//arduino ide 1.8.8 ,ESP32 Wrover Module
//const char *ssid = "xm";
//const char *password = "12345678";
const char *ssid = "Uolian-*";
const char *password = "*";
//const IPAddress serverIP(123,206,*,*); //欲访问的地址
const IPAddress serverIP(148,70,*,*); //欲访问的地址
uint16_t serverPort = 8086;         //服务器端口号
WiFiClient client; //声明一个客户端对象，用于与服务器进行连接
String Cgq_value = "";//存储发往服务器的信息
String Cd_value = "";//存储发往服务器的信息长度

void setup()
{
    Serial.begin(115200);
    Serial.println();
//    WiFi.mode(WIFI_STA);
//    WiFi.setSleep(false); //关闭STA模式下wifi休眠，提高响应速度
//    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected");
    Serial.print("IP Address:");
    Serial.println(WiFi.localIP());
}
void loop()
{
    Serial.println("尝试访问服务器");
    if (client.connect(serverIP, serverPort)) //尝试访问目标地址
    {
        Serial.println("访问成功");
        while (client.connected() || client.available()) //如果已连接或有收到的未读取的数据
        {  
          //拼接存储发往服务器的信息：
           Cgq_value.concat("cuu,host=server01,region=us-west value=");
           Cgq_value.concat(random(0,101));
           Serial.println(Cgq_value);  
           //拼接存储发往服务器的信息长度 
           int Cd=Cgq_value.length();
           Cd_value.concat("Content-Length: ");
           Cd_value.concat(Cgq_value.length());
           
           //开始向服务器发送
           client.write("POST /write?db=aprstest HTTP/1.1");
           client.write(0x0D0A);
           client.write("Host:");
           client.write(0x0D0A);
           client.write(Cd_value.c_str());
           client.write(0x0D0A);
           client.write(0x0D0A);
           client.write("");
           delay(50);           
           client.write(Cgq_value.c_str());
           //client.write(0x0D0A);
           client.write("\r\n");           
           delay(50);
           Serial.println("写入成功"); //串口输出写入成功  
           //读取服务器返回的内容
           Serial.print("读取到数据：");
          while ( client.available()) //如果有收到的未读取的数据
          {
            if (client.available()) //如果有数据可读取
            {
                //String line = client.readStringUntil('\n'); //读取数据到换行符
                String line = client.readStringUntil('\r'); //读取数据到换行符
                //Serial.print("读取到数据：");
                Serial.println(line);
                //client.write(line.c_str()); //将收到的数据回发
             }
           }
             Cgq_value = "";
             Cd_value = "";
             delay(5000);
        }
        Serial.println("关闭当前连接");
        client.stop(); //关闭客户端
    }
    else
    {
        Serial.println("访问失败");
        client.stop(); //关闭客户端
    }
    delay(5000);
}

/**
 * RC522 MISO 接 GPIO13
 * RC522 MOSI 接 GPIO12
 * RC522 SCK  接 GPIO14
 * RC522 SDA  接 GPIO15
 * RC522 RST  接 GPIO5
 * RC522 GND  接 GND
 * RC522 3.3V 接 3.3V
 * 
 * 舵机棕色线 接 GND
 * 舵机红色线 接 5V
 * 舵机黄色线 接 GPIO4
 */
#define BLINKER_WIFI
#define BLINKER_MIOT_OUTLET
#define BLINKER_ALIGENIE_OUTLET
const uint8_t Earse_EEPROM_Flag = 99;  //修改和之前上传程序不同的数值,即可删除全部卡片
const uint8_t Serv_Pin = 4;            //舵机引脚连接定义
const uint8_t RC522_SDA = 15;          //RC522 SDA引脚连接定义
const uint8_t RC522_RST = 5;           //RC522 RST引脚连接定义
const uint8_t Lock_Angle = 0;          //上锁角度
const uint8_t unLock_Angle = 90;       //开锁角度
const char ssid[] = "*******";         //WiFi名称
const char pswd[] = "***********";     //WiFi密码
const char auth[] = "6859ef653813";    //Blinker秘钥
const unsigned long Delay_Time = 5000; //延时上锁时间 ms
#include <SPI.h>
#include <Servo.h>
#include <EEPROM.h>
#include <Blinker.h>
#include <MFRC522.h>
bool oState = false;
bool Add_Mode = false;
bool Clean_Mode = false;
uint32_t Card_ID = 0;
unsigned long Last_Time = 0;
Servo myservo;
BlinkerButton Button1("btn1");
BlinkerButton Button2("btn2");
BlinkerButton Button3("btn3");
MFRC522 rfid(RC522_SDA, RC522_RST);

/**************************RC522部分**************************/
void RC522_Init(void)
{
  pinMode(RC522_RST, OUTPUT);
  digitalWrite(RC522_RST, LOW);
  delay(100);
  digitalWrite(RC522_RST, HIGH);
  SPI.begin();
  rfid.PCD_Init();
}
uint32_t RC522_Scan(void)
{
  uint32_t Card_UUID = 0;
  if (!rfid.PICC_IsNewCardPresent())
    return 0;
  if (!rfid.PICC_ReadCardSerial())
    return 0;
  Card_UUID = rfid.uid.uidByte[0];
  Card_UUID <<= 8;
  Card_UUID |= rfid.uid.uidByte[1];
  Card_UUID <<= 8;
  Card_UUID |= rfid.uid.uidByte[2];
  Card_UUID <<= 8;
  Card_UUID |= rfid.uid.uidByte[3];
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return Card_UUID;
}
/***********************************************************/
/**************************舵机部分**************************/
void Servo_Init(void)
{
  myservo.attach(Serv_Pin);
  myservo.write(Lock_Angle);
}
void Open_Door(void)
{
  Last_Time = millis();
  myservo.write(unLock_Angle);
  BLINKER_LOG("Open the door");
  oState = true;
}
void Close_Door(void)
{
  if (millis() - Last_Time >= Delay_Time && myservo.read() != Lock_Angle)
  {
    myservo.write(Lock_Angle);
    BLINKER_LOG("Time to close the door");
    oState = false;
  }
}
/***********************************************************/
/**************************Blinker**************************/
void aligeniePowerState(const String &state)
{
  BLINKER_LOG("need set door state: ", state);
  if (state == BLINKER_CMD_ON)
  {
    BlinkerAliGenie.powerState("on");
    BlinkerAliGenie.print();
    Open_Door();
  }
  else if (state == BLINKER_CMD_OFF)
  {
    myservo.write(Lock_Angle);
    BlinkerAliGenie.powerState("off");
    BlinkerAliGenie.print();
    oState = false;
  }
}
void aligenieQuery(int32_t queryCode)
{
  BLINKER_LOG("AliGenie Query codes: ", queryCode);
  switch (queryCode)
  {
  case BLINKER_CMD_QUERY_ALL_NUMBER:
    BLINKER_LOG("AliGenie Query All");
    BlinkerAliGenie.powerState(oState ? "on" : "off");
    BlinkerAliGenie.print();
    break;
  case BLINKER_CMD_QUERY_POWERSTATE_NUMBER:
    BLINKER_LOG("AliGenie Query Power State");
    BlinkerAliGenie.powerState(oState ? "on" : "off");
    BlinkerAliGenie.print();
    break;
  default:
    BlinkerAliGenie.powerState(oState ? "on" : "off");
    BlinkerAliGenie.print();
    break;
  }
}
void miotPowerState(const String &state)
{
  BLINKER_LOG("need set power state: ", state);

  if (state == BLINKER_CMD_ON)
  {
    Open_Door();
    BlinkerMIOT.powerState("on");
    BlinkerMIOT.print();
  }
  else if (state == BLINKER_CMD_OFF)
  {
    myservo.write(Lock_Angle);
    BlinkerMIOT.powerState("off");
    BlinkerMIOT.print();
    oState = false;
  }
}

void miotQuery(int32_t queryCode)
{
  BLINKER_LOG("MIOT Query codes: ", queryCode);

  switch (queryCode)
  {
  case BLINKER_CMD_QUERY_ALL_NUMBER:
    BLINKER_LOG("MIOT Query All");
    BlinkerMIOT.powerState(oState ? "on" : "off");
    BlinkerMIOT.print();
    break;
  case BLINKER_CMD_QUERY_POWERSTATE_NUMBER:
    BLINKER_LOG("MIOT Query Power State");
    BlinkerMIOT.powerState(oState ? "on" : "off");
    BlinkerMIOT.print();
    break;
  default:
    BlinkerMIOT.powerState(oState ? "on" : "off");
    BlinkerMIOT.print();
    break;
  }
}
void button1_callback(const String &state)
{
  Serial.println("Button Press");
  Open_Door();
}
void button2_callback(const String &state)
{
  BLINKER_LOG("get button 2 state: ", state);
  if (state == BLINKER_CMD_ON)
  {
    Add_Mode = true;
    Serial.println("Turn on add mode");
    digitalWrite(LED_BUILTIN, LOW);
    Button2.print("on");
  }
  else if (state == BLINKER_CMD_OFF)
  {
    Add_Mode = false;
    Serial.println("Turn off add mode");
    digitalWrite(LED_BUILTIN, HIGH);
    Button2.print("off");
  }
}
void button3_callback(const String &state)
{
  Clean_Mode = true;
}
void Blinker_Init(void)
{
  BLINKER_DEBUG.stream(Serial);
  Blinker.begin(auth, ssid, pswd);
  Button1.attach(button1_callback);
  Button2.attach(button2_callback);
  Button3.attach(button3_callback);
  BlinkerMIOT.attachPowerState(miotPowerState);
  BlinkerMIOT.attachQuery(miotQuery);
  BlinkerAliGenie.attachPowerState(aligeniePowerState);
  BlinkerAliGenie.attachQuery(aligenieQuery);
}
/***********************************************************/
/**************************卡片处理部分**********************/
void Store_Init(void)
{
  EEPROM.begin(4096);
  if (EEPROM.read(2500) != Earse_EEPROM_Flag)
  {
    Serial.println("\r\n\r\nEEPROM unsuccessful format");
    EEPROM.write(2500, Earse_EEPROM_Flag);
    for (int i = 2501; i < 4096; i++)
      EEPROM.write(i, 0);
    EEPROM.commit();
    Serial.print("EEPROM format success");
  }
  Serial.print("\r\nSaved card num: ");
  Serial.println(EEPROM.read(2501));
  EEPROM.end();
}
bool Store_Search(uint32_t data)
{
  EEPROM.begin(4096);
  for (int i = 2504; i < (EEPROM.read(2501) * 4) + 2504; i += 4)
  {
    uint32_t result = EEPROM.read(i);
    result <<= 8;
    result |= EEPROM.read(i + 1);
    result <<= 8;
    result |= EEPROM.read(i + 2);
    result <<= 8;
    result |= EEPROM.read(i + 3);
    Serial.println(result);
    if (data == result)
    {
      Serial.println("Found Card");
      EEPROM.end();
      return true;
    }
  }
  Serial.println("no Found Card");
  EEPROM.end();
  return false;
}
void Store_Save(uint32_t data)
{
  EEPROM.begin(4096);
  int Card_num = EEPROM.read(2501);
  Serial.print("Card num: ");
  Serial.print(Card_num);
  if (Card_num >= 20)
  {
    Serial.println("\r\nOver Max Card num");
    EEPROM.end();
    return;
  }
  Serial.print(" Save Address: ");
  Serial.println((Card_num * 4) + 2504);
  for (int i = 0; i < 4; i++)
    EEPROM.write(2504 + (4 * Card_num) + i, (data >> (3 - i) * 8));
  EEPROM.write(2501, (Card_num + 1));
  EEPROM.end();
  Serial.println("Save new card success.");
}
/***********************************************************/
void setup()
{
  Serial.begin(115200);
  Servo_Init();
  RC522_Init();
  Store_Init();
  Blinker_Init();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop()
{
  Blinker.run();
  Close_Door();
  Card_ID = RC522_Scan();
  if (Card_ID)
  {
    Serial.print("Card ID: ");
    Serial.println(Card_ID);
    if (Add_Mode)
    {
      Store_Save(Card_ID);
      Add_Mode = false;
      digitalWrite(LED_BUILTIN, HIGH);
    }
    if (Store_Search(Card_ID))
    {
      Open_Door();
    }
  }
  if (Clean_Mode)
  {
    EEPROM.begin(4096);
    EEPROM.write(2500, Earse_EEPROM_Flag + 1);
    EEPROM.end();
    delay(500);
    ESP.reset();
  }
}

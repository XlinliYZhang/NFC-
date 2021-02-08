#define PN532_IRQ 3                         //PN532 IRQ 可任意引脚,除 A6 A7 SDA连接至A4
#define PN532_RESET 4                       //PN532 RST 可任意引脚,除 A6 A7 SCL连接至A5 NFC模块通讯跳线拨至I2C模式
#include <Adafruit_PN532.h>                 //包含PN532头文件,如果你需要一个干净的串口输出,请使用我提供的库
#include <EEPROM.h>                         //EEPROM 用于提供断电保存功能
#include <Servo.h>                          //舵机库 用于使用定时器驱动舵机
#include <Wire.h>                           //IIC总线库 用于使用硬件IIC
byte admin = 0;                             //已保存的卡片数量
bool add = false;                           //是否开启添加卡片的标志位
bool status = false;                        //保存当前状态的标志位
unsigned int new_address = 0;               //保存当前保存到的堆栈地址
unsigned long timers = 0;                   //用于记录按键按下的时间 判断长按还是短按
unsigned long lock_time = 0;                //用于记录已经开锁的时间
Servo myservo;                              //创建一个舵机对象用于控制舵机
Adafruit_PN532 nfc(A3, A2, A1, A0);

//Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET); //创建一个NFC对象用于控制读卡器
void setup()
{
  Serial.begin(115200);                            //初始化串口监视器,波特率 115200
  pinMode(13, OUTPUT);                             //初始化LED引脚(即板载LED),为输出模式
  pinMode(2, INPUT_PULLUP);                        //初始化2号口(D2)为内部带上拉电阻的输入模式
  digitalWrite(13, LOW);                           //熄灭LED
  myservo.attach(11);                               //注册舵机到9号口(D9)
  myservo.write(90);                               //舵机转到90° (0° ~ 90°)
  nfc.begin();                                     //NFC初始化
  uint32_t versiondata = nfc.getFirmwareVersion(); //读取版本信息
  if (!versiondata)                                //如果没有读取到则执行
  {
    Serial.print("Didn't find PN53x board"); //串口输出没有找到PN532的报错
    while (1)                                //死循环
    {
      digitalWrite(13, !digitalRead(13)); //翻转LED,即闪烁
      delay(100);                         //延迟 100ms
    }
  }

  Serial.print("Found chip PN5"); //串口输出获取到的版本信息
  Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. ");
  Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.');
  Serial.println((versiondata >> 8) & 0xFF, DEC);

  nfc.SAMConfig();             //我也不太确定这话什么意思 好像是配置读卡器
  admin = EEPROM.read(0);      //获取已保存卡的数量 0为保存卡片数量的地址,其余为卡片ID保存地址
  new_address = admin * 4 + 1; //根据获取到的卡片数量设置新卡片写入地址,同时也是查找卡片的截止地址
  Serial.print("Admin : ");    //串口打印相关的消息做DEBUG
  Serial.print(admin);
  Serial.print("\tAddress : ");
  Serial.println(new_address);
}

void loop()
{
  if (!status) //如果没有读到正确的卡
  {
    bool Button = Read_card(); //开始读卡 返回0,没有找到卡片或匹配失败 返回1,匹配成功
    if (Button)                //如果读卡成功
    {
      Serial.println("OPEN"); //串口输出DEBUG
      myservo.write(180);     //舵机转到180°
      digitalWrite(13, HIGH); //LED亮起
      lock_time = millis();   //记录读卡成功时间
      status = true;          //设置状态为读到正确的卡
    }
  }
  if (status && millis() - lock_time >= 3000) //如果读到正确的卡,且距离读卡成功的时间超过3000ms
  {
    digitalWrite(13, LOW);   //熄灭LED
    myservo.write(90);       //舵机转到 90°
    status = false;          //设置状态为没有读到正确的卡
    Serial.println("Close"); //串口输出DEBUG
  }
  if (!digitalRead(2) && millis() - timers > 200) //如果按键被按下,且两次之间间隔大于200ms(消抖)
  {
    unsigned long Timers = millis(); //记录按下的时间
    while (!digitalRead(2))          //循环等到按键被松开
    {
      if (millis() - Timers >= 2500)
        break;
    }
    if (millis() - Timers >= 2500) //如果按下时间大于2500ms,启动清空程序(清除所有已经保存的卡片)
    {
      for (int i = 0; i < EEPROM.length(); i++) //循环从0到EEPROM最大长度
      {
        EEPROM.write(i, 0); //写入0,即清空
      }
      while (1) //死循环
      {
        digitalWrite(13, !digitalRead(13)); //翻转LED
        delay(200);                         //延时200ms
      }
    }
    else
    {
      add = true;                  //激活储存卡模式
      timers = millis();           //当前时间
      Serial.println("Add modes"); //串口输出DEBUG
      digitalWrite(13, HIGH);      //LED亮
    }
  }
}
byte Read_card()
{
  uint8_t success;                                                                 //记录读卡成功与否
  uint8_t uid[] = {0, 0, 0, 0, 0, 0, 0};                                           //记录uid
  uint8_t uidLength;                                                               //记录uid长度
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100); //尝试读卡,超时时间设置为100ms
  if (success)                                                                     //如果读卡成功
  {
    Serial.print("  UID Length: "); //串口打印相关的消息做DEBUG
    Serial.print(uidLength, DEC);
    Serial.println(" bytes");
    Serial.print("  UID Value: ");

    nfc.PrintHex(uid, uidLength); //打印卡号(这样打印出来规整点)

    uint32_t cardid; //创建32位变量合并卡ID
    cardid = uid[0]; //一位操作 4个8位数字合成一个32位数据
    cardid <<= 8;
    cardid |= uid[1];
    cardid <<= 8;
    cardid |= uid[2];
    cardid <<= 8;
    cardid |= uid[3];
    Serial.print("  Card id: #"); //串口打印相关的消息做DEBUG
    Serial.println(cardid);
    if (admin != 0 && !add) //如果不是读卡模式且保存卡片数量不为0
    {
      for (int i = 1; i <= admin * 4; i += 4) //循环,从1开始到卡片保存到的位置,步进4
      {
        if (uid[0] == EEPROM.read(i)) //如果第一位符合
        {
          if (uid[1] == EEPROM.read(i + 1) && uid[2] == EEPROM.read(i + 2) && uid[3] == EEPROM.read(i + 3)) //尝试匹配后三位
            return true;                                                                                    //成功则退出函数返回true
        }
      }
    }
    else if (add) //如果为添加卡模式
    {
      Serial.println("new Admin"); //串口打印相关的消息做DEBUG
      admin++;                     //用户数+1
      EEPROM.write(0, admin);      //重新写入用户数
      for (byte i = 0; i < 4; i++) //往EEPROM里写入4*8位卡片ID
      {
        while (EEPROM.read(new_address) != uid[i])
        {
          EEPROM.write(new_address, uid[i]);
        }
        new_address++;
      }
      for (byte i = 0; i < 6; i++) //闪烁3次LED作为成功标识
      {
        digitalWrite(13, !digitalRead(13));
        delay(500);
      }
      digitalWrite(13, LOW);                     //熄灭LED
      Serial.println("Success to add new Card"); //串口打印相关的消息做DEBUG
      add = false;                               //关闭读卡模式
      return true;                               //返回ture
    }
  }
  return 0; //如果读卡失败或者匹配失败,则返回0
}

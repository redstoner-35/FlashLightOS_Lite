#include "ADC.h"
#include "ModeLogic.h"
#include "LEDMgmt.h"
#include <string.h>
#include "FirmwarePref.h"

//全局变量
BatteryAlertDef BattStatus;
extern volatile LightStateDef LightMode;

//自己实现的四舍五入函数声明
int iroundf(float IN);

//显示电池电压
void DisplayBatteryVoltage(void)
  {
	ADCOutTypeDef ADCO;
	//初始化显示，获取电压
	if(ExtLEDIndex!=NULL)return; //当前还未完成显示，不进行操作
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  ADC_GetResult(&ADCO);
  //显示电池电压	 
  strncat(LEDModeStr,"12030D",sizeof(LEDModeStr)-1);  //红切换到绿色，熄灭然后马上黄色闪一下，延迟1秒 
  LED_AddStrobe((int)(ADCO.BatteryVoltage)/10,"20");//红色显示十位
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  LED_AddStrobe((int)(ADCO.BatteryVoltage)%10,"30"); //使用黄色显示个位数
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  LED_AddStrobe(iroundf(ADCO.BatteryVoltage*(float)10)%10,"10");//绿色显示小数点后1位
  if(LightMode.LightGroup!=Mode_Off&&LightMode.LightGroup!=Mode_Sleep) //如果手电筒点亮状态则延迟一下再恢复正常显示
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);//结束闪烁
  ExtLEDIndex=&LEDModeStr[0];//传指针过去	
  }

//低压告警处理函数
void BatteryMonitorHandler(void)
  {
	ADCOutTypeDef ADCO;
  float BattLowThr=3.3,BattMidThr=3.7;
	//初始化变量并转换
  ADCO.BatteryVoltage=0;
  ADC_GetResult(&ADCO);
  //判断状态
	if(ADCO.BatteryVoltage<=(2.7*BatteryCellCount))BattStatus=Batt_LVFault;
	else if(ADCO.BatteryVoltage<=(3.01*BatteryCellCount))BattStatus=Batt_LVAlert;
	//控制侧按LED的电量显示
	if(LightMode.LightGroup==Mode_Turbo||LightMode.LightGroup==Mode_Strobe)
	   {
		 BattMidThr-=0.2;
	   BattLowThr-=0.2; //在极亮和爆闪模式下自动下调检测阈值避免错误黄灯
		 }
  switch(LightMode.LightGroup) //根据模式控制LED
    {
	  case Mode_Locked:break;
		case Mode_Off:break;
		case Mode_Sleep:CurrentLEDIndex=0;break; //手电处于关机状态，关闭指示灯
		default:
			 if(BattStatus==Batt_LVFault||BattStatus==Batt_LVAlert)CurrentLEDIndex=12; //警告置起，电量严重不足
		   else if(ADCO.BatteryVoltage<=(BattLowThr*BatteryCellCount))CurrentLEDIndex=3; //电量不足，红灯
		   else if(ADCO.BatteryVoltage<=(BattMidThr*BatteryCellCount))CurrentLEDIndex=2; //电量较充足，黄灯
		   else CurrentLEDIndex=1; //电量充足，绿灯
		}
	//如果温控降档发生则指示灯index +13使得侧按开始频闪指示降档
  if(LightMode.MainLEDThrottleRatio<100&&CurrentLEDIndex>0&&CurrentLEDIndex<4)CurrentLEDIndex+=13;
	}
//检测电池电压并复位警报的函数
void BatteryAlertResetDetect(void)
	{
	ADCOutTypeDef ADCO;
	//初始化变量并转换
  ADCO.BatteryVoltage=0;
  ADC_GetResult(&ADCO);
  //判断状态
	if(ADCO.BatteryVoltage>(3.05*BatteryCellCount))BattStatus=Batt_OK;
	}

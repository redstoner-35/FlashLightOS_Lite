#include "ADC.h"
#include "ModeLogic.h"
#include "LEDMgmt.h"
#include <string.h>
#include "SideKey.h"
#include "delay.h"
#include "FirmwarePref.h"
#include "Fan.h"

//全局变量
BatteryAlertDef BattStatus;
float CellCount;
extern volatile LightStateDef LightMode;

//自己实现的四舍五入函数声明
int iroundf(float IN);

#ifdef EnableVoltageQuery
//显示电池电压
void DisplayBatteryVoltage(void)
  {
	ADCOutTypeDef ADCO;
	//初始化显示，获取电压
	if(ExtLEDIndex!=NULL)return; //当前还未完成显示，不进行操作 
	ADC_GetResult(&ADCO);
	#ifndef BatteryQueryDivision_10V
	if(ADCO.BatteryVoltage>=10.00)return; 	//高精度电压显示模式，如果测量到的电压大于10V，则不执行显示	
  #endif
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
  //显示电池电压	 
  strncat(LEDModeStr,"12030D",sizeof(LEDModeStr)-1);  //红切换到绿色，熄灭然后马上黄色闪一下，延迟1秒 
  #ifdef BatteryQueryDivision_10V
  LED_AddStrobe((int)(ADCO.BatteryVoltage)/10,"20");//红色显示十位
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  LED_AddStrobe((int)(ADCO.BatteryVoltage)%10,"30"); //使用黄色显示个位数
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  LED_AddStrobe(iroundf(ADCO.BatteryVoltage*(float)10)%10,"10");//绿色显示小数点后1位
	#else
  LED_AddStrobe((int)(ADCO.BatteryVoltage)%10,"20");//红色显示十位
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  LED_AddStrobe((int)(ADCO.BatteryVoltage*(float)100)/10,"30"); //使用黄色显示个位数
  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  LED_AddStrobe(iroundf(ADCO.BatteryVoltage*(float)100)%10,"10");//绿色显示小数点后1位		
		
		
	#endif
  if(LightMode.LightGroup!=Mode_Off&&LightMode.LightGroup!=Mode_Sleep) //如果手电筒点亮状态则延迟一下再恢复正常显示
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
  strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);//结束闪烁
  ExtLEDIndex=&LEDModeStr[0];//传指针过去	
  }
#endif

//自动检测电池电压的处理函数
void CellCountDetectHandler(void)
  {
	#ifndef NoAutoBatteryDetect		
		#ifdef UsingLiFePO4Batt
		CellCount=BatteryCellCount; //此时固件不进行检测，按照指定的节数执行检测
		//显示自检结束
    CurrentLEDIndex=13;//LED自检结束
    while(CurrentLEDIndex==13)delay_ms(1);//等待
		ForceDisableFan();//自检已通过，关闭风扇
    getSideKeyShortPressCount(true);  //清除按键操作
		#else 
		ADCOutTypeDef ADCO;	
		bool IsCorrectBattCount=false;
		int Targetbattcount;
		//初始化变量并转换
		ADCO.BatteryVoltage=0;
		ADC_GetResult(&ADCO);	
		//判断电压
		if(ADCO.BatteryVoltage==0)CellCount=BatteryCellCount; //ADC无法获知电池电压
		else //检测电池电压
			{
		  if(ADCO.BatteryVoltage>=12.7)CellCount=4;  //电压大于12.7，有4节电池
			else if(ADCO.BatteryVoltage>=8.5)CellCount=3; //电压在8.5-12.6之间，有3节电池
			else if(ADCO.BatteryVoltage>=4.5)CellCount=2; //电压在4.5-8.5之间，有2节电池
			else CellCount=1; //电压小于4.5只有一节电池
			//显示自检结束
      CurrentLEDIndex=13;//LED自检结束
      while(CurrentLEDIndex==13)//等待
				{
				SideKey_LogicHandler();
				if(getSideKeyShortPressCount(true)==2)IsCorrectBattCount=true; //用户双击要求修正电池节数设置
				}
			ForceDisableFan();//自检已通过，关闭风扇
			//显示识别到的电池节数
			LED_Reset();//复位LED管理器
			memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
			strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
			LED_AddStrobe(CellCount,"10");  //显示识别到的电池节数
			strncat(LEDModeStr,"DDDE",sizeof(LEDModeStr)-1);//结束闪烁	
			ExtLEDIndex=&LEDModeStr[0];//传指针过去	
			while(ExtLEDIndex!=NULL&&!IsCorrectBattCount)//等待显示节数
				{
				SideKey_LogicHandler();
				if(getSideKeyShortPressCount(true)==2)IsCorrectBattCount=true; //用户双击要求修正电池节数设置
				}
			//用户请求修正电池节数
			if(IsCorrectBattCount)
			  {
				ExtLEDIndex=NULL; //立即中断显示
				delay_ms(300);	
				CurrentLEDIndex=21; //延时1秒后指示用户输入修正值
				while(CurrentLEDIndex==21);//等待用户输入修正值
				for(Targetbattcount=0;Targetbattcount==0;Targetbattcount=getSideKeyShortPressCount(true))SideKey_LogicHandler(); //等待用户按下按键
				CellCount=Targetbattcount;//存储输入的电池节数
				LED_Reset();//复位LED管理器
		  	memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
			  strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
			  LED_AddStrobe(CellCount,"30");  //显示用户输入的电池节数
			  strncat(LEDModeStr,"E",sizeof(LEDModeStr)-1);//结束闪烁	
			  ExtLEDIndex=&LEDModeStr[0];//传指针过去	
				}
			getSideKeyShortPressCount(true);  //清除按键操作
			}			
		#endif
	#else
	CellCount=BatteryCellCount; //用户在配置中禁用检测，按照指定的节数执行欠压检测	
	//显示自检结束
  CurrentLEDIndex=13;//LED自检结束
  while(CurrentLEDIndex==13)delay_ms(1);//等待
	ForceDisableFan();//自检已通过，关闭风扇
  getSideKeyShortPressCount(true);  //清除按键操作			
	#endif
	}	
	
//低压告警处理函数
void BatteryMonitorHandler(void)
  {
	ADCOutTypeDef ADCO;
  float BattLowThr=3.3,BattMidThr=3.7;
	//初始化变量并转换
  ADCO.BatteryVoltage=0;
  ADC_GetResult(&ADCO);
  //当手电处于运行状态时，判断电池电压是否低于警告门限	
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
			 //处理电量报警
		   #ifndef UsingLiFePO4Batt
		   if(ADCO.BatteryVoltage<=(2.7*CellCount))BattStatus=Batt_LVFault;
		   else if(ADCO.BatteryVoltage<=(3.01*CellCount))BattStatus=Batt_LVAlert;		
       else if(ADCO.BatteryVoltage>(3.2*CellCount))BattStatus=Batt_OK; //电池电压大于3.2，解除警报		
		   #else	
		   if(ADCO.BatteryVoltage<=(2.15*BatteryCellCount))BattStatus=Batt_LVFault;
		   else if(ADCO.BatteryVoltage<=(2.6*BatteryCellCount))BattStatus=Batt_LVAlert;	
	      else if(ADCO.BatteryVoltage>(2.8*CellCount))BattStatus=Batt_OK; //电池电压大于3.2，解除警报		
		   #endif
		   //显示电量	
		   if(BattStatus==Batt_LVFault||BattStatus==Batt_LVAlert)CurrentLEDIndex=12; //警告置起，电量严重不足
		   else if(ADCO.BatteryVoltage<=(BattLowThr*CellCount))CurrentLEDIndex=3; //电量不足，红灯
		   else if(ADCO.BatteryVoltage<=(BattMidThr*CellCount))CurrentLEDIndex=2; //电量较充足，黄灯
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
	if(ADCO.BatteryVoltage>(2.75*BatteryCellCount))BattStatus=Batt_OK;
	}

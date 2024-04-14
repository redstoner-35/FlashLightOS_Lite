#include <math.h>
#include "ModeLogic.h"
#include "ADC.h"
#include "TempControl.h"
#include "LEDMgmt.h"
#include <string.h>
#include "FirmwarePref.h"

//声明函数
float fmaxf(float x,float y);
float fminf(float x,float y);
int iroundf(float IN);

//全局变量
SystempStateDef TempState=SysTemp_OK;
extern volatile LightStateDef LightMode; //全局变量
static float err_last_temp = 0.0;	//上一温度误差
static float integral_temp = 0.0; //积分后的温度误差值
static bool TempControlEnabled = false; //是否激活温控
static bool CalculatePIDRequest = false; //PID计算请求
float ThermalLowPassFilter[8*ThermalLPFTimeConstant]={0}; //低通滤波器
float PIDInputTemp=0.0; //PID输入温度

//温度检测的简易数字滤波器（低通）
float LEDFilter(float DIN,float *BufIN,int bufsize)
{
 int i;
 float buf,min,max;
 //搬数据
 for(i=(bufsize-1);i>0;i--)BufIN[i]=BufIN[i-1];
 BufIN[i]=DIN;
 //找最高和最低的同时累加数据
 min=2000;
 max=-2000;
 buf=0;
 for(i=0;i<bufsize;i++)
	{
	buf+=BufIN[i];//累加数据
	min=fminf(min,BufIN[i]);
	max=fmaxf(max,BufIN[i]); //取最小和最大
	}
 buf-=(min+max);
 buf/=(float)(bufsize-2);//去掉最高和最低值，取剩下的8个值里面的平均值
 return buf;
}

//自检协商完毕之后填充温度滤波器
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult)
  {
	int i;
	float Temp;
	//写缓冲区
	if(ADCResult->MOSNTCState==NTC_OK&&ADCResult->LEDNTCState==NTC_OK)
		{
		Temp=ADCResult->LEDTemp*LEDThermalWeight/100;
		Temp+=ADCResult->MOSFETTemp*(100-LEDThermalWeight)/100; //进行加权平均计算
		}
	else if(ADCResult->MOSNTCState==NTC_OK)
    Temp=ADCResult->MOSFETTemp;
	else
		Temp=ADCResult->LEDTemp; //如果MOSFET温度正常就用MOSFET的，否则用LED温度
	PIDInputTemp=Temp; //存储温度值
  for(i=0;i<(8*ThermalLPFTimeConstant);i++)ThermalLowPassFilter[i]=Temp;
	}	

//计算实际温度的处理
void CalculateActualTemp(void)
 {
 float ActualTemp,DriverTemp,LEDTemp;
 float Weight;
 static bool IsOHAlertAsserted=false; //过热警报是否触发
 ADCOutTypeDef ADCO;
 //读取数据
 ADC_GetResult(&ADCO);
 //获取加权值
 Weight=LEDThermalWeight;
 if(ADCO.MOSNTCState==NTC_OK&&ADCO.MOSFETTemp>MOSThermalAlert)
    Weight-=2*(ADCO.MOSFETTemp-MOSThermalAlert); //当MOS过热后，减少LED热量的权重	
 if(Weight>80)Weight=80;
 if(Weight<5)Weight=5; //权重值限幅		
 //LED和驱动温度都正常，按照驱动温度计算
 if(ADCO.MOSNTCState==NTC_OK&&ADCO.LEDNTCState==NTC_OK)
   {
	 ActualTemp=ADCO.LEDTemp*Weight/100;
	 ActualTemp+=ADCO.MOSFETTemp*(100-Weight)/100; //进行加权平均计算
	 }
 else if(ADCO.MOSNTCState==NTC_OK) //MOS温度OK，按照MOSFET温度计算
   ActualTemp=ADCO.MOSFETTemp;
 else //否则使用LED温度
	 ActualTemp=ADCO.LEDTemp; 
 //过热警报
 DriverTemp=ADCO.MOSNTCState==NTC_OK?ADCO.MOSFETTemp:0;
 LEDTemp=ADCO.LEDNTCState==NTC_OK?ADCO.LEDTemp:0; //获取温度数据
 if(DriverTemp>MOSThermalTrip)TempState=Systemp_DriverHigh; //驱动过热
 else if(LEDTemp>LEDThermalTrip)TempState=SysTemp_LEDHigh; //LED过热
 if(TempState!=SysTemp_OK)IsOHAlertAsserted=true;
 if(DriverTemp<MaintainTemp&&LEDTemp<MaintainTemp&&IsOHAlertAsserted)//过热保护后温度回落到安全值，清除警报
   {
	 CurrentLEDIndex=0;
	 TempState=SysTemp_OK;
	 IsOHAlertAsserted=false;
	 }
 //计算结束，返回结果
 PIDInputTemp=LEDFilter(ActualTemp,ThermalLowPassFilter,8*ThermalLPFTimeConstant); //开始载入结果
 CalculatePIDRequest = true; //请求PID计算
 }

//显示降档情况
void DisplayTemp(float TempIN)
  {
  if(TempIN>=TriggerTemp)
	  strncat(LEDModeStr,"222",sizeof(LEDModeStr)-1);
	else if(TempIN>=MaintainTemp)
		strncat(LEDModeStr,"333",sizeof(LEDModeStr)-1);
	else 
		strncat(LEDModeStr,"111",sizeof(LEDModeStr)-1);
  strncat(LEDModeStr,"00",sizeof(LEDModeStr)-1);//显示个位
	} 
 
//显示温度
void DisplaySystemTemp(void)
 {
  float LEDTemp;
	ADCOutTypeDef ADCO;
	bool IsPowerON;
	//读取数据准备开始显示
	if(ExtLEDIndex!=NULL)return; //当前还未完成显示，不进行操作
	ADC_GetResult(&ADCO);
	IsPowerON=LightMode.LightGroup!=Mode_Off&&LightMode.LightGroup!=Mode_Sleep?true:false;
 	//开始显示结果
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
	strncat(LEDModeStr,IsPowerON?"D":"0",sizeof(LEDModeStr)-1);
	strncat(LEDModeStr,"2310020",sizeof(LEDModeStr)-1); //红黄绿切换之后熄灭,然后绿色闪一次表示LED温度
	LEDTemp=PIDInputTemp; //显示加权平均温度		 
	if(LEDTemp<0)
	 {
	 LEDTemp=-LEDTemp;//取正数
	 strncat(LEDModeStr,"30",sizeof(LEDModeStr)-1); //如果LED温度为负数则闪烁一次黄色
	 }
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);
	//显示数值
	LED_AddStrobe((int)(LEDTemp/100),"20");
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示百位
	LED_AddStrobe(((int)LEDTemp%100)/10,"30"); 
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1); //显示十位
	LED_AddStrobe((iroundf(LEDTemp)%100)%10,"10");
	strncat(LEDModeStr,"D",sizeof(LEDModeStr)-1);//显示个位
	//降档提示，当温度超过降档温度时显示信息	 
	if(ADCO.LEDNTCState==NTC_OK)DisplayTemp(ADCO.LEDTemp);	
	if(ADCO.MOSNTCState==NTC_OK)DisplayTemp(ADCO.MOSFETTemp);  //显示降档情况	
	//结束显示，传指针过去
	strncat(LEDModeStr,IsPowerON?"DE":"E",sizeof(LEDModeStr)-1); //添加结束符
	ExtLEDIndex=&LEDModeStr[0];//传指针过去
 }
 
//PID温控处理
void PIDStepdownCalc(void)
 {
 bool IsPIDEnabled; 
 float err_temp,AdjustValue;
 //判断PID温控是否需要启用
 if(LightMode.LightGroup==Mode_Strobe||LightMode.LightGroup==Mode_Turbo)IsPIDEnabled=true; //极亮和爆闪，启用温控
 else if(LightMode.LightGroup==Mode_Cycle&&LightMode.CycleModeCount==4)IsPIDEnabled=true; //高档，启用温控
 else IsPIDEnabled=false; 
 //PID温控的滞环检测
 if(!TempControlEnabled&&PIDInputTemp>=TriggerTemp)TempControlEnabled=true;       //温控触发
 else if(TempControlEnabled&&PIDInputTemp<=ReleaseTemp)TempControlEnabled=false;  //温控解除 
 //对于不需要启用温控或者滞环检测关闭温控，则不进行运算
 if(!IsPIDEnabled||!TempControlEnabled)
    {
	  integral_temp=0;
	  err_last_temp=0; 
    LightMode.MainLEDThrottleRatio=100;
		return; //PID不需要运算,复位积分器和微分器
		}
 //温控运算
 err_temp=MaintainTemp-PIDInputTemp; //计算误差值
	if(CalculatePIDRequest)
	  {
		integral_temp+=(err_temp*((float)2/(float)ThermalLPFTimeConstant)); //积分限幅(拓展系数取低通滤波器时间常数的0.5倍)
	  if(integral_temp>10)integral_temp=10;
	  if(integral_temp<-85)integral_temp=-85; //积分和积分限幅
		CalculatePIDRequest=false; //积分统计完成
		}
	AdjustValue=ThermalPIDKp*err_temp+ThermalPIDKi*integral_temp+ThermalPIDKd*(err_temp-err_last_temp); //PID算法算出调节值
	err_last_temp=err_temp;//记录上一个误差值
 	LightMode.MainLEDThrottleRatio=90+AdjustValue;//设置降档数值
 }

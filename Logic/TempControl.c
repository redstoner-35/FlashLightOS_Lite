#include <math.h>
#include "ModeLogic.h"
#include "ADC.h"
#include "TempControl.h"
#include "LEDMgmt.h"
#include <string.h>
#include "FirmwarePref.h"
#include "Delay.h"

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
float ThermalLowPassFilter[24]; //低通滤波器
static float PIDAdjustVal=100; //PID调节值

//关于温度控制模块的自动定义(严禁修改！)
#ifdef NTCStrictMode
  //禁用任意电阻出错
  #ifndef EnableDriverNTC
  #error "Strict NTC Selftest mode conflict with driver NTC detection disabled!"
  #endif

  #ifndef EnableLEDNTC
  #error "Strict NTC Selftest mode conflict with LED NTC detection disabled!"
  #endif
#endif
#ifndef EnableDriverNTC
  #ifndef EnableLEDNTC
  #error "You can't disable both NTC resistor of the driver!"
  #endif
#endif

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
//计算实际加权温度的函数
float CalculateInputTemp(ADCOutTypeDef *ADCResult)
  {
	float Temp,Weight;
  //获取加权值
  Weight=LEDThermalWeight;
  if(ADCResult->MOSNTCState==NTC_OK&&ADCResult->MOSFETTemp>MOSThermalAlert)
    Weight-=2*(ADCResult->MOSFETTemp-MOSThermalAlert); //当MOS过热后，减少LED热量的权重	
  if(Weight>80)Weight=80;
  if(Weight<5)Weight=5; //权重值限幅		
  //LED和驱动温度都正常，按照驱动温度计算
  if(ADCResult->MOSNTCState==NTC_OK&&ADCResult->LEDNTCState==NTC_OK)
    {
	  Temp=ADCResult->LEDTemp*Weight/100;
	  Temp+=ADCResult->MOSFETTemp*(100-Weight)/100; //进行加权平均计算
	  }
  else if(ADCResult->MOSNTCState==NTC_OK) //MOS温度OK，按照MOSFET温度计算
    Temp=ADCResult->MOSFETTemp;
  else //否则使用LED温度
	  Temp=ADCResult->LEDTemp; 
	//计算结束，返回结果
	return Temp;
	}

//自检协商完毕之后填充温度滤波器
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult)
  {
	int i;
  float Temp;
	//写缓冲区
	Temp=CalculateInputTemp(ADCResult);
  for(i=0;i<24;i++)ThermalLowPassFilter[i]=Temp;
	}	

//过热保护处理函数
void OverHeatProtectionHandler(void)
 {
 float DriverTemp,LEDTemp;
 static bool IsOHAlertAsserted=false; //过热警报是否触发
 ADCOutTypeDef ADCO;
 //读取数据
 ADC_GetResult(&ADCO);
 //过热警报核心逻辑
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
 }
#ifdef EnableTempQuery
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
	LEDTemp=CalculateInputTemp(&ADCO); //将ADC转换结果输入到换算函数内，换算出实际温度		 
 	//开始显示结果
	LED_Reset();//复位LED管理器
  memset(LEDModeStr,0,sizeof(LEDModeStr));//清空内存
	strncat(LEDModeStr,IsPowerON?"D":"0",sizeof(LEDModeStr)-1);
	strncat(LEDModeStr,"2310020",sizeof(LEDModeStr)-1); //红黄绿切换之后熄灭,然后绿色闪一次表示LED温度
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
	//LED降档提示
	#ifdef EnableLEDNTC
	if(ADCO.LEDNTCState==NTC_OK)
	  DisplayTemp(ADCO.LEDTemp); //显示降档情况
  else //LED NTC故障，显示错误
    {
		LED_AddStrobe(2,"20");
		strncat(LEDModeStr,"00",sizeof(LEDModeStr)-1);	
		}		
	#endif
  //驱动降档提示
	#ifdef EnableDriverNTC
	if(ADCO.MOSNTCState==NTC_OK)
		DisplayTemp(ADCO.MOSFETTemp);  //显示降档情况	
	else //LED NTC故障，显示错误
    {
		LED_AddStrobe(2,"20");
		strncat(LEDModeStr,"00",sizeof(LEDModeStr)-1);	
		}		
	#endif
	//结束显示，传指针过去
	strncat(LEDModeStr,IsPowerON?"DE":"E",sizeof(LEDModeStr)-1); //添加结束符
	ExtLEDIndex=&LEDModeStr[0];//传指针过去
 }
#endif
 
//PID温控处理
void PIDStepdownCalc(void)
 {
 bool IsPIDEnabled; 
 float err_temp,PIDInputTemp,integral_Limit;
 ADCOutTypeDef ADCO;
 //运行读取函数获取最新的LED温度数据
 ADC_GetResult(&ADCO);
 PIDInputTemp=LEDFilter(CalculateInputTemp(&ADCO),ThermalLowPassFilter,24);		  
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
		PIDAdjustVal=100;
		return; //PID不需要运算,复位积分器和微分器
		}
 //温控PID的误差计算以及Kp和Kd项调节
 err_temp=MaintainTemp-PIDInputTemp; //计算误差值
 PIDAdjustVal+=err_temp*ThermalPIDKp/100; //Kp(比例项)
 if(fabsf(err_temp)>2) //温度误差的绝对值大于2℃，进行微分调节
	  {	
		PIDAdjustVal+=(err_temp-err_last_temp)*ThermalPIDKd/100; //Kd(微分项)
		err_last_temp=err_temp;//记录上一个误差值
		}
 if(PIDAdjustVal>100)PIDAdjustVal=100;
 if(PIDAdjustVal<ThermalStepDownMinDuty)PIDAdjustVal=ThermalStepDownMinDuty; //PID调节值限幅
 //温控PID的Ki(积分项)
 integral_Limit=(PIDAdjustVal-(float)ThermalStepDownMinDuty)>0?(PIDAdjustVal-(float)ThermalStepDownMinDuty):0; //动态计算积分上限
 if(integral_Limit>0)
   {	 
	 if(integral_Limit>15)integral_Limit=15; //积分上限不高于15
   integral_temp+=(err_temp/(float)150); //积分累加
   if(integral_temp>integral_Limit)integral_temp=integral_Limit;
   if(integral_temp<(-integral_Limit))integral_temp=(-integral_Limit); //积分限幅
	 }
 else integral_temp=0; //积分上下限制幅度为0，禁止积分部分进行累加
 //返回计算结束的调节值
 delay_ms(10); //额外的延时，限制PID调节的速度避免超调
 LightMode.MainLEDThrottleRatio=PIDAdjustVal+ThermalPIDKi*integral_temp; //返回比例和微分结果以及积分结果相加的调节值
 }

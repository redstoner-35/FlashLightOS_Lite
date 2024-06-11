#ifndef TempCtrl
#define TempCtrl

//内部包含
#include "ADC.h"

//定义
#define LEDThermalWeight 93 //LED侧的权重值

typedef enum
 {
 SysTemp_OK,
 SysTemp_LEDHigh,
 Systemp_DriverHigh
 }SystempStateDef;

//PID参数
#define ThermalPIDKp 4.25
#define ThermalPIDKi 1.35
#define ThermalPIDKd 1.05 //PID温控的P I D
#define ThermalStepDownMinDuty 10 //温控降档最低的占空比限制(%)
 
//函数
void PIDStepdownCalc(void);//PID温控计算
void OverHeatProtectionHandler(void);//过热保护处理函数
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult);//填充温度数据
void DisplaySystemTemp(void);//显示系统温度
float CalculateInputTemp(ADCOutTypeDef *ADCResult);//计算实际加权温度
float LEDFilter(float DIN,float *BufIN,int bufsize);//温度检测的简易数字滤波器（低通）
 
#endif

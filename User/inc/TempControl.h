#ifndef TempCtrl
#define TempCtrl

//内部包含
#include "ADC.h"

//定义
#define ThermalLPFTimeConstant 15 //定义温度低通滤波器的时间常数（单位秒）
#define LEDThermalWeight 90 //LED侧的权重值

typedef enum
 {
 SysTemp_OK,
 SysTemp_LEDHigh,
 Systemp_DriverHigh
 }SystempStateDef;

//PID参数
#define ThermalPIDKp 0.31
#define ThermalPIDKi 0.86
#define ThermalPIDKd 0.35 //PID温控的P I D

//函数
void PIDStepdownCalc(void);//PID温控计算
void CalculateActualTemp(void);//计算实际温度
void FillThermalFilterBuf(ADCOutTypeDef *ADCResult);//填充温度数据
void DisplaySystemTemp(void);//显示系统温度

#endif

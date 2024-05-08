#ifndef _PWMDIM_
#define _PWMDIM_

#include "Pindefs.h" //引脚定义
#include <stdbool.h>
#include "ht32.h"

//PWM引脚自动定义
#define PWMO_IOG STRCAT2(HT_GPIO,PWMO_IOBank)
#define PWMO_IOB STRCAT2(GPIO_P,PWMO_IOBank)
#define PWMO_IOP STRCAT2(GPIO_PIN_,PWMO_IOPinNum)

//定义
#define SYSHCLKFreq 48000000  //系统AHB频率48MHz
#define PWMFreq 20000 //PWM频率20Khz
#define DDILoopKp 0.85 //直驱恒流电流环的Kp
#define DDILoopKi 1.10 //直驱恒流电流环的Ki
#define DDILoopKd 1.1 //直驱恒流电流环的Kd

//函数
void PWMTimerInit(void);//启动定时器
void SetPWMDuty(float Duty);//设置定时器输出值
float OC5021B_ICCMax(void);//计算OC5021B线性调光的最大可达电流
float OC5021B_CalcPWMDACDuty(float Current);//计算OC5021B在某个对应电流时PWMDAC的占空比
float CalcDirectDriveDuty(float Current);//极亮直驱的PID平均电流恒流运算
void ResetDirectDriveCCPID(void);//对直驱的PID恒流模块进行reset

#endif

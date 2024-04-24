#ifndef _Fan_
#define _Fan_

//内部包含
#include "Pindefs.h"

//引脚定义
#define FanPWMO_IOG STRCAT2(HT_GPIO,FanPWMO_IOBank)
#define FanPWMO_IOB STRCAT2(GPIO_P,FanPWMO_IOBank)
#define FanPWMO_IOP STRCAT2(GPIO_PIN_,FanPWMO_IOPinNum) //PWM输出引脚

#define FanPWREN_IOG STRCAT2(HT_GPIO,FanPWREN_IOBank)
#define FanPWREN_IOB STRCAT2(GPIO_P,FanPWREN_IOBank)
#define FanPWREN_IOP STRCAT2(GPIO_PIN_,FanPWREN_IOPinNum) //风扇电源控制


//函数
void FanOutputInit(void);//初始化风扇控制
void FanSpeedControlHandler(void); //风扇速度控制函数
void ForceDisableFan(void);//强制关闭风扇

#endif

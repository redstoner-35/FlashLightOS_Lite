#ifndef _PinDefs_
#define _PinDefs_

/***************************************************
整个工程所有的外设的引脚定义都在这里了，你可以按照
需要去修改。但是要注意，有些引脚是不可以修改的因为是
外设的输出引脚。
******* 目前已使用引脚  *******
PA：0 1 2 9 12 13 14
PB：0 1

******* 不可修改的引脚 *******
PA9：按键所在引脚，同时触发boot下载模式方便固件更新
PA12-13：预留给SWDIO debug port
PA14：预留给MCTM的PWM output channel
***************************************************/

//侧按按键LED
#define LED_Green_IOBank B 
#define LED_Green_IOPinNum 1 //绿色LED引脚定义（PB1）

#define LED_Red_IOBank B
#define LED_Red_IOPinNum 0 //红色LED引脚定义（PB0）


//基于定时器的PWM输出引脚(用于极亮直驱PWM调光和混合调光)
#define PWMO_IOBank A
#define PWMO_IOPinNum 14 //直驱PWM Pin=PA14（MT-CH0）

#define HybridPWMO_IOBank B
#define HybridPWMO_IOPinNum 8 //OC5021B 混合PWM Pin=PB8（MT-CH3）

//按键
#define ExtKey_IOBank A
#define ExtKey_IOPN 9  //外部侧按按钮（PA9）

//DC-DC使能输出
#define TurboSel_IOBank B
#define TurboSel_IOPinNum 2  //降压恒流+极亮直驱的选择PIN=PB2

#define VgsBoot_IOBank B
#define VgsBoot_IOPinNum 4  //极亮直驱模式下控制电荷泵抬高Vgs的EN Pin=PB4

//负责测量LED温度和SPS温度的ADC输入引脚
#define LED_NTC_Ch 0 //LED电压测量（PA0）
#define MOS_NTC_Ch 1  //LED温度测量模拟输入（PA1）
#define Batt_Vsense_Ch 2 //电池电量检测通道(PA2)
#define IMON_Ch 3 //INA240 电流输入(PA3)

#endif

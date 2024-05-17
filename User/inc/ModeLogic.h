#ifndef _ModeLogic_
#define _ModeLogic_

#include <stdbool.h>
#include "Pindefs.h"


//定义
#define DriverErrorOccurred() TempState!=SysTemp_OK||BattStatus==Batt_LVFault

//枚举
typedef enum
 {
 Mode_Sleep, //深度睡眠
 Mode_Locked,//锁定
 Mode_Off, //关机
 Mode_Cycle, //循环档
 Mode_Turbo, //极亮挡位
 Mode_Strobe, //爆闪挡位
 Mode_Ramp //无极调光挡位
 }LightModeSelDef;

typedef struct
 {
 LightModeSelDef LightGroup; //默认的模式组
 char CycleModeCount; //循环档次数
 bool IsMainLEDOn; //是否开启主LED
 bool IsLocked; //是否上锁
 float MainLEDThrottleRatio; //主LED降档的Ratio
 }LightStateDef;	

typedef enum
 {
 Batt_OK,
 Batt_LVAlert,
 Batt_LVFault
 }BatteryAlertDef;
 
/*  DCDC引脚的自动define,不允许修改！  */
#define AUXV33_IOB STRCAT2(GPIO_P,AUXV33_IOBank)
#define AUXV33_IOG STRCAT2(HT_GPIO,AUXV33_IOBank)
#define AUXV33_IOP STRCAT2(GPIO_PIN_,AUXV33_IOPinNum)
 
#define VgsBoot_IOB STRCAT2(GPIO_P,VgsBoot_IOBank)
#define VgsBoot_IOG STRCAT2(HT_GPIO,VgsBoot_IOBank)
#define VgsBoot_IOP STRCAT2(GPIO_PIN_,VgsBoot_IOPinNum)
 
//函数 
void SetupRTCForCounter(bool IsRTCStartCount);//设置RTC时钟
void DisplayBatteryVoltage(void);//显示电池电压
void CellCountDetectHandler(void);//检测电池节数
void LightLogicSetup(void);//上电期间初始化灯具模式
void ReverseModeCycleOpHandler(void);//处理循环档单击+长按的反向换挡按键操作的逻辑
void LightModeStateMachine(void);//换挡和手电开关状态机
void ControlMainLEDHandler(void);//处理主LED控制的函数
void BatteryMonitorHandler(void);//电池监控函数 
void BatteryAlertResetDetect(void);//检测并复位电池低压告警 
void LowVoltageAlertHandler(void);//低电压告警闪烁主LED的操作 
void BatteryLPFHandler(void);//电池电压低通滤波函数
 
#endif

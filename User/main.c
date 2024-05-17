#include "ht32.h"
#include "delay.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "PWMDIM.h"
#include "ADC.h"
#include "ModeLogic.h"
#include "TempControl.h"
#include "Fan.h"

//函数声明
void CheckForFlashLock(void);


//常量
bool SensorRefreshFlag=false;
bool EnteredMainApp=false;

int main(void)
 {
//初始化系统时钟
 CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
 CLKConfig.Bit.PA=1;
 CLKConfig.Bit.PB=1;
 CLKConfig.Bit.AFIO=1;
 CLKConfig.Bit.GPTM1=1;
 CLKConfig.Bit.EXTI=1;
 CLKConfig.Bit.MCTM0 = 1;
 CLKConfig.Bit.ADC0 = 1;
 CLKConfig.Bit.BKP = 1;
 CKCU_PeripClockConfig(CLKConfig,ENABLE);
 //初始化LSI和RTC
 while((PWRCU_CheckReadyAccessed() != PWRCU_OK)){}; //等待BKP可以访问
 RTC_LSILoadTrimData();//加载LSI的修正值
 RTC_DeInit();//复位RTC确保RTC不要在运行  
 //其余外设初始化
 delay_init();
 GPIO_DisableDebugPort();//关闭debug口
 CheckForFlashLock();//锁定存储区
 EnableHBTimer(); //初始化systick和定时器
 LED_Init(); //初始化LED管理器
 PWMTimerInit();//初始化PWM输出
 InternalADC_Init();//初始化内部ADC
 SideKeyInit();//初始化侧按按钮
 FanOutputInit();//启动风扇
 LightLogicSetup();//初始化灯具逻辑模块
 CellCountDetectHandler();//检测接入的电池节数
 EnteredMainApp=true; //已进入主APP
 //主循环
 while(1)
   {
	 //处理手电筒自身的逻辑事务
	 #ifndef CarLampMode
	 SideKey_LogicHandler();//处理侧按按键事务
	 #endif
	 BatteryMonitorHandler();//处理电池电压监控的事务
	 LightModeStateMachine();//处理灯具开关机和挡位逻辑
	 PIDStepdownCalc();//PID降档计算
	 ControlMainLEDHandler();//控制灯具的主LED
	 FanSpeedControlHandler();//控制风扇速度
	 //0.125S软件定时处理
	 if(!SensorRefreshFlag)continue;
	 BatteryLPFHandler();//低通滤波
	 CalculateActualTemp();//计算温度数值
	 LEDMgmt_CallBack();//处理电量指示灯的逻辑
	 #ifndef CarLampMode
	 ReverseModeCycleOpHandler();//处理用户单击+长按令循环档反向换挡的逻辑
	 #endif
	 LowVoltageAlertHandler();//主LED闪烁低压警报处理
	 SensorRefreshFlag=false;
	 }
 return 0;
 }

#include "delay.h"
#include "ModeLogic.h"
#include "LEDMgmt.h"
#include "FirmwarePref.h"

//函数和变量声明
void ExitLowPowerMode(void);
extern volatile LightStateDef LightMode; //全局变量
extern int DeepSleepTimer; //深度睡眠计时器



//设置RTC时钟
void SetupRTCForCounter(bool IsRTCStartCount)
  {
  //关闭RTC
	if(!IsRTCStartCount)
	  {
	  if(!(HT_RTC->CR&0x01))	return; //RTC已经被关闭，不需要再次关闭
		NVIC_DisableIRQ(RTC_IRQn); //禁用RTC中断
    RTC_DeInit();//禁用RTC
		}
	else if(LightMode.IsLocked)return; //手电已经是上锁的了，不需要再启动RTC
	else if(!(HT_RTC->CR&0x01)) //RTC未启用且自动上锁时间大于0，启用RTC
	  {
		NVIC_EnableIRQ(RTC_IRQn); //启用RTC中断
		HT_RTC->CR=0xF14; //使用LSI作为时钟，CMPCLR=1，当RTC溢出后自动清除CNT值 
		HT_RTC->CMP=AutoLockTimeOut*60; //设置比较值
		HT_RTC->IWEN=0x02; //比较中断使能
	  HT_RTC->CR|=0x01; //启动RTC开始计时
		}
	}
//重启RTC定时器
void RestartRTCTimer(void)
  {
	if(!(HT_RTC->CR&0x01))delay_ms(10); //如果RTC已被禁用则不执行重启，延迟10ms后退出（去抖）
	else
	  {
		SetupRTCForCounter(false);
	  delay_ms(10);
	  SetupRTCForCounter(true); //重启RTC
		}
	}

//RTC比较中断触发的处理函数
void RTCCMIntHandler(void)
  {
	/*******************************************
  首先检测电源状态，如果电源状态已经进入深度睡
	眠则首先退出低功耗模式，然后在结束写入后等待
	锁定提示的LED序列结束，指示灯熄灭之后再立即
	进入深度睡眠
	*******************************************/
  if(LightMode.LightGroup==Mode_Sleep)
	  {
		if(LightMode.IsLocked)DeepSleepTimer=0; //手电已锁定，立即回到睡眠阶段
	  else DeepSleepTimer=20;  //手电未锁定引入额外延迟，用于让LED播报提示
	  ExitLowPowerMode();
		}
	//关闭RTC并复位
	SetupRTCForCounter(false);
	//让手电进入锁定模式
	if(!LightMode.IsLocked)CurrentLEDIndex=11; //手电未上锁，此时指示手电已被锁定
  LightMode.LightGroup=Mode_Locked;
  LightMode.IsLocked=true;//进入锁定阶段
	}

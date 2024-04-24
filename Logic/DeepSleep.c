#include "delay.h"
#include "LEDMgmt.h"
#include "ADC.h"
#include "ModeLogic.h"
#include "FirmwarePref.h"

//外部变量
extern volatile LightStateDef LightMode; //状态机状态
extern bool IsFanNeedtoSpin; //风扇是否在旋转

//低功耗睡眠计时器
int DeepSleepTimer=DeepSleepTimeOut*8; //深度睡眠定时器
bool DeepSleepTimerFlag=false;

void DeepSleepTimerCallBack(void)
  {
	if(IsFanNeedtoSpin)return; //风扇在旋转期间不允许计时
	if(DeepSleepTimer>0&&DeepSleepTimerFlag)
	    {
		  DeepSleepTimerFlag=false;
			DeepSleepTimer--; //开始计时
			}
	}

//进入低功耗休眠模式
void EnteredLowPowerMode(void)
  {
  CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
  int retry=0;
  //关闭系统外设
	LED_Reset();//复位LED状态机
	DisableHBTimer(); //关闭定时器
	ADC_DeInit(HT_ADC0);//将ADC复位
  TM_Cmd(HT_MCTM0, DISABLE); 
  MCTM_CHMOECmd(HT_MCTM0, DISABLE); //关闭PWM输出
	//关闭外设时钟
  CLKConfig.Bit.PB=1;
  CLKConfig.Bit.MCTM0 = 1;
  CLKConfig.Bit.ADC0 = 1;
	CLKConfig.Bit.GPTM1=1;
  CKCU_PeripClockConfig(CLKConfig,DISABLE);
	//切换到32KHz时钟
	while(retry<500)//反复循环等待LSI稳定
	  {
		delay_ms(1);
		if(HT_CKCU->GCSR&0x20)break;
		retry++;
		}
	if(retry==500)return;
	retry=500;
  HT_CKCU->GCCR|=0x07; //令LSI作为系统时钟源
	while(--retry)if((HT_CKCU->CKST&0x07)==0x07)break; //等待LSI用于系统时钟源
	if(!retry)return;
	//按顺序关闭PLL，HSE，HSI
	HT_CKCU->GCCR&=0xFFFFFDFF;//关闭PLL
	while(HT_CKCU->GCSR&0x02);//等待PLL off		
	HT_CKCU->GCCR&=0xFFFFFBFF;//关闭HSE
	while(HT_CKCU->GCSR&0x04);//等待HSE off
	HT_CKCU->GCCR&=0xFFFFF7FF;//关闭HSI
	while(HT_CKCU->GCSR&0x08);//等待HSI off
	//进入休眠状态
	LightMode.LightGroup=Mode_Sleep;
	__wfi(); //进入暂停状态
	while(LightMode.LightGroup==Mode_Sleep);//等待
	}
//退出低功耗模式
void ExitLowPowerMode(void)
  {
	CKCU_PeripClockConfig_TypeDef CLKConfig={{0}};
	//启用HSE并使用HSE通过PLL作为单片机的运行时钟
	HT_CKCU->GCCR|=0x400;//启用HSE
  while(!(HT_CKCU->GCSR&0x04));//等待HSE就绪
  HT_CKCU->PLLCFGR&=0xF81FFFFF;
  HT_CKCU->PLLCFGR|=0x3000000;	//设置Fout=8Mhz*6/1=48MHz
	HT_CKCU->GCCR|=0x200;//启用PLL
	while(!(HT_CKCU->GCSR&0x02));//等待PLL就绪
	HT_CKCU->GCCR&=0xFFFFFFF8; //令PLL作为系统时钟源	
	while((HT_CKCU->CKST&0x100)!=0x100){};//等待48MHz的PLL输出切换为时钟源
	//打开外设时钟
  CLKConfig.Bit.PB=1;
  CLKConfig.Bit.MCTM0 = 1;
  CLKConfig.Bit.ADC0 = 1;
	CLKConfig.Bit.GPTM1=1;
  CKCU_PeripClockConfig(CLKConfig,ENABLE);
	//打开外设
	delay_init();//重新初始化Systick
	EnableHBTimer(); //打开定时器
  TM_Cmd(HT_MCTM0, ENABLE); 
  MCTM_CHMOECmd(HT_MCTM0, ENABLE); //打开PWM输出
	//重新初始化ADC
	CKCU_SetADCnPrescaler(CKCU_ADCPRE_ADC0, CKCU_ADCPRE_DIV16);//ADC时钟为主时钟16分频=3MHz                                               
  ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 4, 0);//单次触发模式,一次完成4个数据的转换
  ADC_SamplingTimeConfig(HT_ADC0,25); //采样时间（25个ADC时钟）
  ADC_RegularTrigConfig(HT_ADC0, ADC_TRIG_SOFTWARE);//软件启动		
  ADC_IntConfig(HT_ADC0,ADC_INT_CYCLE_EOC,ENABLE);                                                                            
  NVIC_EnableIRQ(ADC0_IRQn); //启用ADC中断    
	ADC_Cmd(HT_ADC0, ENABLE); //启用ADC
	//恢复到正常运行阶段
	if(LightMode.IsLocked)LightMode.LightGroup=Mode_Locked;//如果睡眠前手电筒被上锁则恢复到锁定模式
	else LightMode.LightGroup=Mode_Off; //否则回到关机模式
	}

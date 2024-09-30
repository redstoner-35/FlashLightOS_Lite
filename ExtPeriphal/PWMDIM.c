#include "delay.h"
#include "ModeLogic.h"
#include "PWMDIM.h"


//在车灯模式下生成闪烁提示用户故障发生
void GenerateMainLEDStrobe(int count)
 {
 #ifndef DCDCEN_Remap_FUN_TurboSel
 GPIO_SetOutBits(VgsBoot_IOG,VgsBoot_IOP); //启用EN
 GPIO_SetOutBits(AUXV33_IOG,AUXV33_IOP); //启用DCDC 
 #else //极亮直驱 先启用电荷泵然后启用驱动器
 GPIO_SetOutBits(VgsBoot_IOG,VgsBoot_IOP);
 delay_ms(10);
 GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); 
 #endif
 while(count>0) //开始循环制造闪烁	 
   {
   SetPWMDuty(10.0);
	 delay_ms(200);
	 SetPWMDuty(0.0); //制造闪烁
	 delay_ms(200);
	 count--;
	 }
 GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); //禁用DCDC
 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP); //禁用电荷泵
 }

//生成自检结束闪烁一下的提示 
void GenerateStrobeAfterPOST(void)
 {
 #ifndef DCDCEN_Remap_FUN_TurboSel 
 //IO为DCDC-EN
 int count=2;
 GPIO_SetOutBits(VgsBoot_IOG,VgsBoot_IOP); //启用电荷泵
 GPIO_SetOutBits(AUXV33_IOG,AUXV33_IOP); //启用DCDC 
 delay_ms(1);
 while(count>0) //开始循环制造闪烁	 
   {
   SetPWMDuty(5.0);
	 delay_ms(200);
	 SetPWMDuty(0.0); //制造5%占空比的闪烁
	 delay_ms(200);
	 count--;
	 }	 
 GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); 
 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP); //禁用DCDC和电荷泵	 
 #else
 //IO为极亮直驱和恒流输出选择
 GPIO_SetOutBits(VgsBoot_IOG,VgsBoot_IOP); //启用电荷泵
 GPIO_SetOutBits(AUXV33_IOG,AUXV33_IOP); //选择为恒流buck模式
 SetPWMDuty(10.0); 
 delay_ms(200);
 SetPWMDuty(0.0); //制造使用恒流buck的10%占空比的闪烁
 delay_ms(200);
 GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); //选择为极亮直驱模式 
 SetPWMDuty(5.0); 
 delay_ms(200);
 SetPWMDuty(0.0); //制造使用直驱的5%占空比的闪烁
 delay_ms(200); 	 
 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP); //禁用电荷泵	  
 #endif	 
 }
 
//设置占空比
void SetPWMDuty(float Duty)
 { 
 //计算reload Counter
 float buf;
 if(Duty>=100) //大于等于100%占空比
	 buf=(float)(SYSHCLKFreq / PWMFreq);//载入100%占空比时的比较值
 else if(Duty>0) //大于0占空比
   {
	 buf=(float)(SYSHCLKFreq / PWMFreq);//载入100%占空比时的比较值
	 buf*=Duty;
   buf/=(float)100;//计算reload值
	 }
 else buf=0;
 //写通道0比较寄存器以及GPIO配置寄存器
 AFIO_GPxConfig(PWMO_IOB,PWMO_IOP, Duty>0?AFIO_FUN_MCTM_GPTM:AFIO_FUN_GPIO);//如果占空比为0则将输出配置为普通GPIO直接输出0，否则输出PWM
 HT_MCTM0->CH0CCR=(unsigned int)buf; //设置CCR寄存器
 }

//初始化定时器 
void PWMTimerInit(void)
 {
 TM_TimeBaseInitTypeDef MCTM_TimeBaseInitStructure;
 TM_OutputInitTypeDef MCTM_OutputInitStructure;
 MCTM_CHBRKCTRInitTypeDef MCTM_CHBRKCTRInitStructure;
 //配置TBU(脉冲生成基本时基单元)
 MCTM_TimeBaseInitStructure.CounterReload = (SYSHCLKFreq / PWMFreq) - 1; //计数分频值按照设置频率来
 MCTM_TimeBaseInitStructure.Prescaler = 0; //分频器禁用
 MCTM_TimeBaseInitStructure.RepetitionCounter = 0; //重复定时器禁用
 MCTM_TimeBaseInitStructure.CounterMode = TM_CNT_MODE_UP;
 MCTM_TimeBaseInitStructure.PSCReloadTime = TM_PSC_RLD_IMMEDIATE; //向上计数模式，计数器满后立即重载为0重新计数(CNT = (CNT + 1)%Maxvalue)
 TM_TimeBaseInit(HT_MCTM0, &MCTM_TimeBaseInitStructure);
 //配置通道0的输出级
 MCTM_OutputInitStructure.Channel = TM_CH_0; //定时器输出通道0
 MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1; //PWM正输出模式（if CNT < CMP PWM=1 else PWM =0）
 MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
 MCTM_OutputInitStructure.ControlN = TM_CHCTL_DISABLE; //开启正极性输出，负极性禁用
 MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
 MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED; //配置极性
 MCTM_OutputInitStructure.IdleState = MCTM_OIS_LOW;
 MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_HIGH; //IDLE状态关闭
 MCTM_OutputInitStructure.Compare = 0; //默认输出0%占空比
 MCTM_OutputInitStructure.AsymmetricCompare = 0;  //非对称比较关闭
 TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure);
 //配置刹车，锁定之类的功能
 MCTM_CHBRKCTRInitStructure.OSSRState = MCTM_OSSR_STATE_DISABLE; //OSSR(互补输出功能)关闭
 MCTM_CHBRKCTRInitStructure.OSSIState = MCTM_OSSI_STATE_ENABLE; //OSSI设置在定时器关闭后回到IDLE状态
 MCTM_CHBRKCTRInitStructure.LockLevel = MCTM_LOCK_LEVEL_OFF; //关闭锁定
 MCTM_CHBRKCTRInitStructure.Break0 = MCTM_BREAK_DISABLE;
 MCTM_CHBRKCTRInitStructure.Break0Polarity = MCTM_BREAK_POLARITY_LOW;  //禁用制动功能
 MCTM_CHBRKCTRInitStructure.AutomaticOutput = MCTM_CHAOE_ENABLE; //启用自动输出
 MCTM_CHBRKCTRInitStructure.DeadTime = 0; 
 MCTM_CHBRKCTRInitStructure.BreakFilter = 0; //死区时间和制动功能过滤器禁用
 MCTM_CHBRKCTRConfig(HT_MCTM0, &MCTM_CHBRKCTRInitStructure);
 //配置PWM输出引脚
 AFIO_GPxConfig(PWMO_IOB,PWMO_IOP, AFIO_FUN_MCTM_GPTM);//配置为MCTM0复用GPIO
 GPIO_DirectionConfig(PWMO_IOG,PWMO_IOP,GPIO_DIR_OUT);//输出	
 GPIO_ClearOutBits(PWMO_IOG,PWMO_IOP);//将对应IO的输出ODR设置为0
 //启动定时器TBU和比较通道1的输出功能
 TM_Cmd(HT_MCTM0, ENABLE); 
 MCTM_CHMOECmd(HT_MCTM0, ENABLE);
 //初始化DCDC EN的GPIO 
 AFIO_GPxConfig(AUXV33_IOB,AUXV33_IOP, AFIO_FUN_GPIO);//配置为GPIO
 GPIO_DirectionConfig(AUXV33_IOG,AUXV33_IOP,GPIO_DIR_OUT);//输出
 GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP);//DCDC-EN 默认输出0		
 //初始化极亮电荷泵的IO
 AFIO_GPxConfig(VgsBoot_IOB,VgsBoot_IOP, AFIO_FUN_GPIO);//配置为GPIO
 GPIO_DirectionConfig(VgsBoot_IOG,VgsBoot_IOP,GPIO_DIR_OUT);//输出
 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP);//电荷泵IO 默认输出0	
 //初始化LD和LED控制的IO
 AFIO_GPxConfig(LDLEDMUX_IOB,LDLEDMUX_IOP, AFIO_FUN_GPIO);//配置为GPIO
 GPIO_DirectionConfig(LDLEDMUX_IOG,LDLEDMUX_IOP,GPIO_DIR_OUT);//输出
 GPIO_ClearOutBits(LDLEDMUX_IOG,LDLEDMUX_IOP);//默认输出0选择LED输出
 
 }

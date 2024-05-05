#include "ModeLogic.h"
#include "PWMDIM.h"
#include "SideKey.h"
#include "LEDMgmt.h"
#include "TempControl.h"
#include "delay.h"
#include "FirmwarePref.h"
#include <math.h>

//全局变量
extern BatteryAlertDef BattStatus;//电池状态
extern int DeepSleepTimer; //深度睡眠计时器
volatile LightStateDef LightMode; //全局变量
int ReverseSwitchDelay=0; //反向换挡时的延迟
extern SystempStateDef TempState; //驱动的温度状态
static float DimmingRatio=0.00; //调光数值 
static bool IsKeyStillPressed=false; //标志位，按键是否依然按下
static bool AdjustDir=false; //调节方向，false=低到高，true=高到低
static bool TacticalMode=false; //战术模式
static char LVAlertTimer=0; //低电压警报定时器

//常数
const float CycleModeRatio[5]={CycleMode1Duty,CycleMode2Duty,CycleMode3Duty,CycleMode4Duty,CycleMode5Duty}; //5个循环挡位的占空比设置(百分比)

//外部函数
void DeepSleepTimerCallBack(void);
void EnteredLowPowerMode(void);

//上电期间初始化灯具模式
void LightLogicSetup(void)
 {
 TM_TimeBaseInitTypeDef TimeBaseInit;
 //初始化结构体
 LightMode.CycleModeCount=0;
 LightMode.IsMainLEDOn=true;
 LightMode.MainLEDThrottleRatio=100;
 LightMode.LightGroup=Mode_Off;
 LightMode.IsLocked=false;
 BattStatus=Batt_OK;//上电之后触发警报
 #ifdef CarLampMode
 SetupRTCForCounter(false);	//车灯模式，禁止自动锁定检测
 TacticalMode=true;//车灯模式，直接配置为战术模式 
 #endif
 //初始化负责爆闪的GPTM1
 TimeBaseInit.Prescaler = 4799;                      // 48MHz->10KHz
 TimeBaseInit.CounterReload = (int)(10000/(StrobeFreq*2))-1;                  // 10KHz->指定爆闪频率*2
 TimeBaseInit.RepetitionCounter = 0;
 TimeBaseInit.CounterMode = TM_CNT_MODE_UP;
 TimeBaseInit.PSCReloadTime = TM_PSC_RLD_IMMEDIATE;
 TM_TimeBaseInit(HT_GPTM1, &TimeBaseInit);
 TM_ClearFlag(HT_GPTM1, TM_FLAG_UEV);
 NVIC_EnableIRQ(GPTM1_IRQn);
 TM_IntConfig(HT_GPTM1,TM_INT_UEV,ENABLE);//配置好中断	
 }

//低电压警报闪烁
void LowVoltageAlertHandler(void)
 {
 if(BattStatus!=Batt_LVAlert||LightMode.LightGroup==Mode_Strobe) //告警未发生或者位于爆闪
    {
    LVAlertTimer=0;
    return;
    }
 if(LVAlertTimer>0)LVAlertTimer--;
 else LVAlertTimer=64; //开始计时
 }

#ifndef CarLampMode
//处理循环档单击+长按的反向换挡按键操作的逻辑
void ReverseModeCycleOpHandler(void)
 {
 if(LightMode.LightGroup!=Mode_Cycle||BattStatus==Batt_LVAlert)return; //当前不是循环档模式或者电池低压警告触发禁止换挡，不执行该逻辑
 if(getSideKeyClickAndHoldEvent()) //用户单击并按住，开始反向循环
	 {	 
	 //如果用户一直按住则反复反向换挡
	 if(ReverseSwitchDelay==0)do
	   {
	   LightMode.CycleModeCount=LightMode.CycleModeCount>0?LightMode.CycleModeCount-1:4;
		 }
	 while(CycleModeRatio[LightMode.CycleModeCount]==-1); //持续换挡直到跳过设置为-1被禁用的挡位
	 ReverseSwitchDelay=ReverseSwitchDelay<5?ReverseSwitchDelay+1:0; //计时器累加
	 }
 else ReverseSwitchDelay=0; //用户松开按键，计时器复位
 }
#endif

//手电正常和异常关机处理函数
static void PowerOffProcess(void)
 {
 if(TempState!=SysTemp_OK)CurrentLEDIndex=TempState==Systemp_DriverHigh?7:8;//过热告警触发，显示原因
 else  CurrentLEDIndex=0; //关闭指示灯						
 LightMode.LightGroup=Mode_Off;
 BattStatus=Batt_OK; //电池电压检测重置		
 #ifndef CarLampMode	 
 SetupRTCForCounter(true);	//使能自动锁定检测
 #endif
 }
 
//换挡和手电开关状态机
void LightModeStateMachine(void)
 {
 #ifndef CarLampMode
 int Click=getSideKeyShortPressCount(true);
 #else
 int Click=0;
 #endif
 #ifdef EnableFanControl	
 extern bool IsFanNeedtoSpin; //风扇是否在旋转的标志位
 #endif
 switch(LightMode.LightGroup)
   {
	 /* 睡眠模式 */
	 case Mode_Sleep:
		  #ifndef CarLampMode
		  EnteredLowPowerMode();//令主控睡眠
	    #else
	    LightMode.LightGroup=Mode_Off; //车灯模式，禁止睡眠
	    #endif
	    break;
	 /* 锁定模式 */
	 case Mode_Locked:
		  #ifndef CarLampMode
		  if(DeepSleepTimer<=0)LightMode.LightGroup=Mode_Sleep; //闲置一定时间，进入睡眠阶段
		  else if(Click==5)
			   {
			   LightMode.LightGroup=Mode_Off; //五击解锁
				 LightMode.IsLocked=false;
				 TacticalMode=false; //默认返回到非战术模式
				 CurrentLEDIndex=10;  //提示用户解锁
				 }
	    else if(Click>0||getSideKeyHoldEvent()||getSideKeyClickAndHoldEvent())
		    CurrentLEDIndex=(CurrentLEDIndex==0)?9:CurrentLEDIndex; //其余任何操作显示非法
			else DeepSleepTimerCallBack();//执行倒计时
	    #else
	    LightMode.LightGroup=Mode_Off; //车灯模式，禁止睡眠
	    #endif
	    break;
	 /* 关机模式 */
	 case Mode_Off:
		 #ifndef CarLampMode
		  if(DeepSleepTimer<=0)LightMode.LightGroup=Mode_Sleep; //闲置一定时间，进入睡眠阶段
	    #ifdef EnableVoltageQuery
	    else if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    #endif
	    #ifdef EnableTempQuery
	    else if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    #endif
			else if(Click==4) //关机状态下四击切换战术模式
			  {
				TacticalMode=TacticalMode?false:true;
				CurrentLEDIndex=TacticalMode?17:18;	
				break;
				}
		  #ifdef EnableFanControl
		  else if(Click==6)IsFanNeedtoSpin=IsFanNeedtoSpin?false:true; //关机状态6击强制启动和关闭风扇
		  #endif
			else if(Click==5) //五击锁定
			  {
				LightMode.IsLocked=true;
				TacticalMode=false; //锁定后自动取消战术模式
			  CurrentLEDIndex=11; //提示用户锁定
				LightMode.LightGroup=Mode_Locked;
				}			
	    else if(DriverErrorOccurred())//电池低压锁定触发或者高温警报，禁止开机
			  {
				if((Click>0||getSideKeyAnyHoldEvent())&&CurrentLEDIndex==0)CurrentLEDIndex=BattStatus==Batt_LVFault?19:7; //显示导致无法开机的问题
				BatteryAlertResetDetect(); //检测电池电压，如果电池电压大于警告门限则允许开机
			  break;
				}			
			else if(TacticalMode)//战术模式
			  { 
				if(BattStatus!=Batt_LVAlert&&getSideKeyLongPressEvent())
				   {
				   SetupRTCForCounter(false);	//开机，禁止自动锁定检测
					 LightMode.LightGroup=Mode_Turbo; //单击开机		
			     }				
				else DeepSleepTimerCallBack();//执行倒计时
				}
			//正常开机
			if(IsKeyStillPressed) //按键依然按下，不执行本次判断直接退出
				{
				if(!getSideKeyLongPressEvent()&&Click==0)IsKeyStillPressed=false;
				break;
				}
			if(getSideKeyLongPressEvent())//长按开机直接进入无极调光
			  {
				IsKeyStillPressed=true;
			  DimmingRatio=0.01;
				LightMode.LightGroup=Mode_Ramp;
				AdjustDir=false; //复位方向
				SetupRTCForCounter(false);	//开机，禁止自动锁定检测
				}
	    else if(Click==1)
			  {
			  LightMode.LightGroup=Mode_Cycle; //单击开机进入循环档
				SetupRTCForCounter(false);	//开机，禁止自动锁定检测
				if(BattStatus==Batt_LVAlert)LightMode.CycleModeCount=0; //电池低压警报触发，开机锁定0档
				}		  
			#ifdef EnableInstantTurbo
	    else if(Click==2&&BattStatus!=Batt_LVAlert)//在电池电量充足的情况下双击开机直接一键极亮
			  {
				LightMode.LightGroup=Mode_Turbo; 
			  SetupRTCForCounter(false);	//开机，禁止自动锁定检测
				}
			#endif
      #ifdef EnableInstantStrobe				
	    else if(Click==3) //三击开机进入爆闪
			  {
			  SetupRTCForCounter(false);	//开机，禁止自动锁定检测
			  LightMode.LightGroup=Mode_Strobe; 
				}
			#endif
			else DeepSleepTimerCallBack();//执行倒计时
	  #else	
	    if(DriverErrorOccurred())LightMode.LightGroup=Mode_Off; //出现错误，禁止开机
			else if(!GPIO_ReadInBit(ExtKey_IOG,ExtKey_IOP))LightMode.LightGroup=Mode_Turbo; //按键按下，开机
	  #endif
	    break;
	 /* 开机,处于循环挡位模式 */
	 case Mode_Cycle:
		#ifndef CarLampMode
	    #ifdef EnableVoltageQuery
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    #endif
	    #ifdef EnableTempQuery
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    #endif
			if(getSideKeyLongPressEvent()||DriverErrorOccurred())//长按或者驱动出错之后，关机
          PowerOffProcess(); 
	    else if(Click==3)LightMode.LightGroup=Mode_Strobe; //三击爆闪
	    else if(BattStatus==Batt_LVAlert)LightMode.CycleModeCount=0;//低压警报触发，强制锁一档
			else if(Click==1)do
			    {
			    LightMode.CycleModeCount=LightMode.CycleModeCount<4?LightMode.CycleModeCount+1:0;	//在电池没有低压警告的时候用户单击，循环换挡
					}
			while(CycleModeRatio[LightMode.CycleModeCount]==-1); //持续循环换挡直到挡位配置不为-1
			else if(Click==2)LightMode.LightGroup=Mode_Turbo; //在电池没有低压警告的时候双击极亮
		 #else		
	   LightMode.LightGroup=Mode_Turbo; //车灯单档模式，强制跳转至极亮		
		 #endif			
		  break;		
	 /* 开机 处于极亮模式*/
	 case Mode_Turbo:
		#ifndef CarLampMode
	    #ifdef EnableVoltageQuery
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    #endif
	    #ifdef EnableTempQuery
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    #endif
			if(TacticalMode&&(!getSideKeyHoldEvent()||DriverErrorOccurred())) //战术模式下按键松开或者驱动出错之后执行关机
			   {
         PowerOffProcess();  
				 break;
				 }
   		if(getSideKeyLongPressEvent()||DriverErrorOccurred())//长按或者驱动出错之后，关机
				PowerOffProcess(); 
	    else if(BattStatus==Batt_LVAlert)//电池低压警报触发，强制锁定到1档
			  {
				LightMode.LightGroup=Mode_Cycle;
				LightMode.CycleModeCount=0; 
				}
	    else if(Click==1)LightMode.LightGroup=Mode_Cycle;//单击回到循环档
	    else if(Click==3)LightMode.LightGroup=Mode_Strobe; //三击爆闪
		#else
		  if(GPIO_ReadInBit(ExtKey_IOG,ExtKey_IOP)||DriverErrorOccurred())PowerOffProcess();//按键按下或者发生错误关机
		#endif
	    break;
	 /* 开机 处于爆闪模式*/
	 case Mode_Strobe:
		#ifndef CarLampMode
	    #ifdef EnableVoltageQuery
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    #endif
	    #ifdef EnableTempQuery
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    #endif
   		if(getSideKeyLongPressEvent()||DriverErrorOccurred())//长按或者驱动出错之后，关机
				PowerOffProcess(); 
	    else if(Click==1)LightMode.LightGroup=Mode_Cycle;//单击回到循环档
	    else if(Click==2)LightMode.LightGroup=Mode_Turbo; //双击极亮
	  #else
	  LightMode.LightGroup=Mode_Turbo; //车灯单档模式，强制跳转至极亮		
	  #endif
	    break;
	 /* 开机 处于无极调光模式*/
	 case Mode_Ramp:	
    #ifndef CarLampMode	 
	    #ifdef EnableVoltageQuery
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    #endif
	    #ifdef EnableTempQuery
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    #endif
		 if(IsKeyStillPressed&&!getSideKeyHoldEvent())IsKeyStillPressed=false; //按键已经松开，可以继续响应  
	   if(Click>0||DriverErrorOccurred()) //任意操作或者错误，关机
				{		
        PowerOffProcess(); 
				IsKeyStillPressed=true; //等待计时
			  while(getSideKeyHoldEvent())SideKey_LogicHandler();//等待按键松开
				}			
    #else
    LightMode.LightGroup=Mode_Turbo; //车灯单档模式，强制跳转至极亮		
    #endif	 
		 break;
	 }	 
 }
//无极调光处理函数
static bool IsKeyPressed=false;
static char MinMaxBrightNessStrobeTimer=0;
static bool IsEnabledLimitTimer=false;
 
void RampAdjustHandler(void)
 {
 #ifndef CarLampMode	 
 float incValue;
 bool Keybuf;
 //判断操作	 
 if(LightMode.LightGroup!=Mode_Ramp||IsKeyStillPressed)return;//非无极调光模式，或者按键仍然按下不做处理
 Keybuf=getSideKeyHoldEvent();
 if(Keybuf!=IsKeyPressed)//按键松开按下的检测
	  {
		IsKeyPressed=Keybuf; 
		if(!IsKeyPressed)
			AdjustDir=AdjustDir?false:true;//松开时翻转方向
		else
			IsEnabledLimitTimer=true; //每次按键按下时复位亮度达到的定时器的标志位
		}
	//亮度增减
	if(IsKeyPressed)
	  {
		//计算步进值
		incValue=(float)1/(50*RampTimeSpan);//计算出单位的步进值
	  //根据方向增减亮度值
    if(AdjustDir&&DimmingRatio>0)DimmingRatio-=incValue;
		else if(!AdjustDir&&DimmingRatio<1.00)DimmingRatio+=incValue;
		//限制亮度的幅度值为0-1
		if(DimmingRatio<0)DimmingRatio=0;
		if(DimmingRatio>1.0)DimmingRatio=1.0;//限幅
		}
	//负责短时间熄灭LED指示已经到达地板或者天花板亮度
	if(IsEnabledLimitTimer&&(DimmingRatio==0||DimmingRatio==1.0))
	  {
		IsEnabledLimitTimer=false;
		LightMode.IsMainLEDOn=false;
		MinMaxBrightNessStrobeTimer=25;//熄灭0.5秒
		}
	//提示定时器
  if(BattStatus==Batt_LVAlert)DimmingRatio=0.01;//电量警报，调低亮度到最低
	if(MinMaxBrightNessStrobeTimer>0)MinMaxBrightNessStrobeTimer--;
  else LightMode.IsMainLEDOn=true; //重新打开LED
 #endif
 }
//处理主LED控制的函数
void ControlMainLEDHandler(void)
 {
 float Duty;
 bool IsDCDCEnable,IschargepumpEnabled;	 
 #ifdef DCDCEN_Remap_FUN_TurboSel
 bool IsTurbomode;
 #endif
 //设置DCDC-EN
 switch(LightMode.LightGroup)
   {
	 case Mode_Cycle:
	 case Mode_Turbo:
	 case Mode_Ramp:
	 case Mode_Strobe:IsDCDCEnable=true;break;//灯具启动，使能DCDC
	 default:IsDCDCEnable=false;//其余状态DCDC-EN 默认输出0
	 }
 //电荷泵控制
 IschargepumpEnabled=GPIO_ReadOutBit(VgsBoot_IOG,VgsBoot_IOP)==SET?true:false; //检测电荷泵是否使能
 if(IsDCDCEnable!=IschargepumpEnabled)//电荷泵IO发生变化
   {
	 if(IsDCDCEnable) //电荷泵从关机到开机，先打开电荷泵确保Vgs建立后再送占空比数据
	   {
		 GPIO_SetOutBits(VgsBoot_IOG,VgsBoot_IOP);
		 delay_ms(10);
		 }
	 else //电荷泵从开机到关机，先禁用PWM输出，然后等待10mS后再关闭电荷泵
	   {
		 SetPWMDuty(0);
		 delay_ms(10);
		 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP);
		 }	 
	 }
 #ifndef DCDCEN_Remap_FUN_TurboSel
 //DCDC-EN控制
 if(!IsDCDCEnable||!LightMode.IsMainLEDOn)GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); //主LED关闭输出或者灯具在非开机状态，禁用DCDC
 else GPIO_SetOutBits(AUXV33_IOG,AUXV33_IOP); //启用DCDC 
 #else
 //极亮直驱和恒流输出选择
 if(LightMode.LightGroup==Mode_Turbo||LightMode.LightGroup==Mode_Strobe)IsTurbomode=true;
 else IsTurbomode=true; //极亮是否启用
 if(!IsDCDCEnable||IsTurbomode)GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); //处于关机状态或者位于极亮直驱，输出低电平启用驱动器
 else GPIO_SetOutBits(AUXV33_IOG,AUXV33_IOP); //启用恒流BUCK
 #endif	 
 delay_us(200); //这个延时确保EN先输出再给整定
 //设置爆闪定时器
 TM_SetCounterReload(HT_GPTM1,(int)(LightMode.LightGroup==Mode_Strobe?10000/(StrobeFreq*2)-1:199)); //根据是否无极调光模式设置定时器 
 TM_Cmd(HT_GPTM1,(LightMode.LightGroup==Mode_Strobe||LightMode.LightGroup==Mode_Ramp)?ENABLE:DISABLE);
 if(LightMode.LightGroup!=Mode_Strobe&&LightMode.LightGroup!=Mode_Ramp)LightMode.IsMainLEDOn=true; //非爆闪挡位关闭定时器
 //设置占空比
 switch(LightMode.LightGroup)
   {
	 case Mode_Cycle:Duty=CycleModeRatio[LightMode.CycleModeCount];break; //循环档，从模式组里面取占空比
	 case Mode_Turbo:
		       if(!TacticalMode)Duty=TurboDuty;//极亮占按照目标值设置占空比
	         else Duty=BattStatus==Batt_LVAlert?30:TurboDuty; //战术模式按照电池状态设置占空比
	         break;
	 case Mode_Ramp:Duty=RampMinDuty+((RampMaxDuty-RampMinDuty)*pow(DimmingRatio,GammaCorrectionValue));break; //无极调光模式，按照目标值设置占空比
	 case Mode_Strobe:Duty=BattStatus==Batt_LVAlert?30:TurboDuty;break;//爆闪，如果电池没有低压警告则按照极亮功率，否则锁30%
	 default:Duty=0; //其余状态占空比等于0
	 }
 if(Duty>100)Duty=100;
 if(Duty<0)Duty=0; //限制占空比范围为0-100%
 if(!LightMode.IsMainLEDOn)Duty=0; //禁用LED，占空比设置为0
 if(LVAlertTimer>62||(LVAlertTimer>57&&LVAlertTimer<59))Duty=0; //低压警报发生，熄灭LED
 SetPWMDuty(Duty*=(LightMode.MainLEDThrottleRatio/(float)100)); //写PWM寄存器设置占空比(和温度控制调节值加权之后的结果)
 }

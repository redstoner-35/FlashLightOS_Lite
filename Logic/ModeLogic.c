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
const float CycleModeCurrent[5]={CycleMode1Current,CycleMode2Current,CycleMode3Current,CycleMode4Current,CycleMode5Current}; //5个循环挡位的占空比设置(百分比)

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
 //初始化DCDC EN的GPIO 
 AFIO_GPxConfig(TurboSel_IOB,TurboSel_IOP, AFIO_FUN_GPIO);//配置为GPIO
 GPIO_DirectionConfig(TurboSel_IOG,TurboSel_IOP,GPIO_DIR_OUT);//输出
 GPIO_ClearOutBits(TurboSel_IOG,TurboSel_IOP);//DCDC-EN 默认输出0		
 //初始化极亮电荷泵的IO
 AFIO_GPxConfig(VgsBoot_IOB,VgsBoot_IOP, AFIO_FUN_GPIO);//配置为GPIO
 GPIO_DirectionConfig(VgsBoot_IOG,VgsBoot_IOP,GPIO_DIR_OUT);//输出
 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP);//电荷泵IO 默认输出0		
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
	 while(CycleModeCurrent[LightMode.CycleModeCount]==-1); //持续换挡直到跳过设置为-1被禁用的挡位
	 ReverseSwitchDelay=ReverseSwitchDelay<5?ReverseSwitchDelay+1:0; //计时器累加
	 }
 else ReverseSwitchDelay=0; //用户松开按键，计时器复位
 }
 
//手电正常和异常关机处理函数
static void PowerOffProcess(void)
 {
 if(TempState!=SysTemp_OK)CurrentLEDIndex=TempState==Systemp_DriverHigh?7:8;//过热告警触发，显示原因
 else CurrentLEDIndex=BattStatus!=Batt_LVFault?0:12; //如果电池电量没有严重不足则关闭指示灯，否则通过慢闪显示电量严重过低						
 ResetDirectDriveCCPID();//复位极亮直驱的PID
 LightMode.LightGroup=Mode_Off;
 BattStatus=Batt_OK; //电池电压检测重置				
 SetupRTCForCounter(true);	//使能自动锁定检测
 }
 
//换挡和手电开关状态机
void LightModeStateMachine(void)
 {
 int Click=getSideKeyShortPressCount(true);
 switch(LightMode.LightGroup)
   {
	 /* 睡眠模式 */
	 case Mode_Sleep:
		  EnteredLowPowerMode();//令主控睡眠
	    break;
	 /* 锁定模式 */
	 case Mode_Locked:
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
	    break;
	 /* 关机模式 */
	 case Mode_Off:
		  if(DeepSleepTimer<=0)LightMode.LightGroup=Mode_Sleep; //闲置一定时间，进入睡眠阶段
	    else if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    else if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
			else if(Click==4) //关机状态下四击切换战术模式
			  {
				TacticalMode=TacticalMode?false:true;
				CurrentLEDIndex=TacticalMode?17:18;	
				break;
				}
			else if(Click==5) //五击解锁
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
	    break;
	 /* 开机,处于循环挡位模式 */
	 case Mode_Cycle:
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
			if(getSideKeyLongPressEvent()||DriverErrorOccurred())//长按或者驱动出错之后，关机
          PowerOffProcess(); 
	    else if(Click==3)LightMode.LightGroup=Mode_Strobe; //三击爆闪
	    else if(BattStatus==Batt_LVAlert)LightMode.CycleModeCount=0;//低压警报触发，强制锁一档
			else if(Click==1)do
			    {
			    LightMode.CycleModeCount=LightMode.CycleModeCount<4?LightMode.CycleModeCount+1:0;	//在电池没有低压警告的时候用户单击，循环换挡
					}
			while(CycleModeCurrent[LightMode.CycleModeCount]==-1); //持续循环换挡直到挡位配置不为-1
			else if(Click==2)LightMode.LightGroup=Mode_Turbo; //在电池没有低压警告的时候双击极亮
	    else 
		  break;		
	 /* 开机 处于极亮模式*/
	 case Mode_Turbo:
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
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
	    break;
	 /* 开机 处于爆闪模式*/
	 case Mode_Strobe:
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
   		if(getSideKeyLongPressEvent()||DriverErrorOccurred())//长按或者驱动出错之后，关机
				PowerOffProcess(); 
	    else if(Click==1)LightMode.LightGroup=Mode_Cycle;//单击回到循环档
	    else if(Click==2)LightMode.LightGroup=Mode_Turbo; //双击极亮
	    break;
	 /* 开机 处于无极调光模式*/
	 case Mode_Ramp:				
		 if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	   if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
		 if(IsKeyStillPressed&&!getSideKeyHoldEvent())IsKeyStillPressed=false; //按键已经松开，可以继续响应  
	   if(Click>0||DriverErrorOccurred()) //任意操作或者错误，关机
				{		
        PowerOffProcess(); 
				IsKeyStillPressed=true; //等待计时
			  while(getSideKeyHoldEvent())SideKey_LogicHandler();//等待按键松开
				}				
		 break;
	 }	 
 }
//无极调光处理函数
static bool IsKeyPressed=false;
static char MinMaxBrightNessStrobeTimer=0;
static bool IsEnabledLimitTimer=false;
 
void RampAdjustHandler(void)
 {
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
 }
//处理主LED控制的函数
void ControlMainLEDHandler(void)
 {
 float Duty,Current;
 bool IsDCDCEnable,IschargepumpEnabled;	 
 bool IsTurbomode;
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
		 OC5021B_SetHybridDuty(0);
		 delay_ms(10);
		 GPIO_ClearOutBits(VgsBoot_IOG,VgsBoot_IOP);
		 }	 
	 }
 //设置爆闪定时器
 TM_SetCounterReload(HT_GPTM1,(int)(LightMode.LightGroup==Mode_Strobe?10000/(StrobeFreq*2)-1:199)); //根据是否无极调光模式设置定时器 
 TM_Cmd(HT_GPTM1,(LightMode.LightGroup==Mode_Strobe||LightMode.LightGroup==Mode_Ramp)?ENABLE:DISABLE);
 if(LightMode.LightGroup!=Mode_Strobe&&LightMode.LightGroup!=Mode_Ramp)LightMode.IsMainLEDOn=true; //非爆闪挡位关闭定时器
 //取出电流设置并应用降档参数
 if(LVAlertTimer>62||(LVAlertTimer>57&&LVAlertTimer<59))Current=0; //低压警报发生，熄灭LED
 else if(!LightMode.IsMainLEDOn)Current=0; //爆闪功能禁用LED，占空比设置为0)	 
 else switch(LightMode.LightGroup)
   {
	 case Mode_Cycle:Current=CycleModeCurrent[LightMode.CycleModeCount];break; //循环档，取电流数值
	 case Mode_Strobe:Current=BattStatus==Batt_OK?TurboCurrent:CycleMode3Current;break; //爆闪如果电池电压正常则取极亮电流，否则取低档位电流
	 case Mode_Turbo:Current=TurboCurrent;break; //极亮档，极亮电流
	 case Mode_Ramp:Current=BattStatus==Batt_OK?RampMinCurrent+(DimmingRatio*(RampMaxCurrent-RampMinCurrent)):CycleMode1Current;break; //无极调光挡位，如果电池正常则取调光电流，否则取
	 default:Current=0; //默认电流为0
	 }
 Current*=LightMode.MainLEDThrottleRatio/100; //应用降档系数
 if(Current>0&&Current<CycleMode1Current)Current=CycleMode1Current;
 if(Current>TurboCurrent)Current=TurboCurrent; //将应用了降档系数的电流进行限幅	  
 //设置占空比
 if(Current==0)Duty=0; //LED关闭，电流为0
 else if(!IsTurbomode)
   {
	 ResetDirectDriveCCPID();//非直驱模式复位极亮直驱的PID
	 Duty=OC5021B_CalcPWMDACDuty(Current); //非直驱模式，通过DC调光控制OC5021B
	 }
 else Duty=CalcDirectDriveDuty(Current); //极亮直驱模式，使用PID恒流	 
 SetPWMDuty(Duty); //写PWM寄存器设置占空比
 //极亮直驱和降压恒流输出选择
 IsTurbomode=(Current>OC5021B_ICCMax())?true:false; //电流大于降压恒流输出能力时启用直驱，否则启用OC5021B
 if(!IsDCDCEnable||IsTurbomode)GPIO_ClearOutBits(TurboSel_IOG,TurboSel_IOP); //处于关机状态或者位于极亮直驱，输出低电平启用驱动器
 else GPIO_SetOutBits(TurboSel_IOG,TurboSel_IOP); //启用恒流BUCK  
 }

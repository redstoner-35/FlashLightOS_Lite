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
 AFIO_GPxConfig(AUXV33_IOB,AUXV33_IOP, AFIO_FUN_GPIO);//配置为GPIO
 GPIO_DirectionConfig(AUXV33_IOG,AUXV33_IOP,GPIO_DIR_OUT);//输出
 GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP);//DCDC-EN 默认输出0		
 //显示自检结束
 CurrentLEDIndex=13;//LED自检结束
 while(CurrentLEDIndex==13)delay_ms(1);//等待
 getSideKeyShortPressCount(true);  //清除按键操作
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
	 while(CycleModeRatio[LightMode.CycleModeCount]==-1); //持续换挡直到跳过设置为-1被禁用的挡位
	 ReverseSwitchDelay=ReverseSwitchDelay<5?ReverseSwitchDelay+1:0; //计时器累加
	 }
 else ReverseSwitchDelay=0; //用户松开按键，计时器复位
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
		  else if(Click==4)
			   {
			   LightMode.LightGroup=Mode_Off; //四击解锁
				 LightMode.IsLocked=false;
				 CurrentLEDIndex=10;  //提示用户解锁
				 }
	    else if(Click>0||getSideKeyHoldEvent()||getSideKeyClickAndHoldEvent())
		    CurrentLEDIndex=(CurrentLEDIndex==0)?9:CurrentLEDIndex; //其余任何操作显示非法
			else DeepSleepTimerCallBack();//执行倒计时
	    break;
	 /* 关机模式 */
	 case Mode_Off:
		  if(DeepSleepTimer<=0)LightMode.LightGroup=Mode_Sleep; //闲置一定时间，进入睡眠阶段
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    if(BattStatus==Batt_LVFault||TempState!=SysTemp_OK)//电池低压锁定触发或者高温警报，禁止开机
			  {
				BatteryAlertResetDetect(); //检测电池电压，如果电池电压大于警告门限则允许开机
			  break;
				}			
      #ifdef EnableInstantStrobe				
	    if(Click==3)LightMode.LightGroup=Mode_Strobe; //三击开机进入爆闪
			#endif
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
				}
	    else if(Click==1)
			  {
			  LightMode.LightGroup=Mode_Cycle; //单击开机进入循环档
				if(BattStatus==Batt_LVAlert)LightMode.CycleModeCount=0; //电池低压警报触发，开机锁定0档
				}
		  #ifdef EnableInstantTurbo
	    else if(Click==2&&BattStatus!=Batt_LVAlert)LightMode.LightGroup=Mode_Turbo; //在电池电量充足的情况下双击开机直接一键极亮
			#endif
	    else if(Click==4)
			  {
				LightMode.IsLocked=true;
			  CurrentLEDIndex=11; //提示用户锁定
				LightMode.LightGroup=Mode_Locked;
				}
			else DeepSleepTimerCallBack();//执行倒计时
	    break;
	 /* 开机,处于循环挡位模式 */
	 case Mode_Cycle:
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    if(TempState!=SysTemp_OK) //发生过热
			    {
					LightMode.LightGroup=Mode_Off;
					CurrentLEDIndex=TempState==Systemp_DriverHigh?7:8;//驱动过热
					BattStatus=Batt_OK; //电池电压检测重置
					}
			if(getSideKeyLongPressEvent()||BattStatus==Batt_LVFault)//长按或者触发低压fault，关机
			    {		 
					CurrentLEDIndex=0; //关闭指示灯						
				  LightMode.LightGroup=Mode_Off;
		      BattStatus=Batt_OK; //电池电压检测重置
					}			
	    else if(Click==3)LightMode.LightGroup=Mode_Strobe; //三击爆闪
	    else if(BattStatus==Batt_LVAlert)LightMode.CycleModeCount=0;//低压警报触发，强制锁一档
			else if(Click==1)do
			    {
			    LightMode.CycleModeCount=LightMode.CycleModeCount<4?LightMode.CycleModeCount+1:0;	//在电池没有低压警告的时候用户单击，循环换挡
					}
			while(CycleModeRatio[LightMode.CycleModeCount]==-1); //持续循环换挡直到挡位配置不为-1
			else if(Click==2)LightMode.LightGroup=Mode_Turbo; //在电池没有低压警告的时候双击极亮
	    else 
		  break;		
	 /* 开机 处于极亮模式*/
	 case Mode_Turbo:
	    if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	    if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
	    if(TempState!=SysTemp_OK) //发生过热
			   {
				 LightMode.LightGroup=Mode_Off;
				 CurrentLEDIndex=TempState==Systemp_DriverHigh?7:8;//驱动过热
				 BattStatus=Batt_OK; //电池电压检测重置
				 }
   		if(getSideKeyLongPressEvent()||BattStatus==Batt_LVFault) //长按或者触发低压fault，关机
				{		
        CurrentLEDIndex=0; //关闭指示灯						
				LightMode.LightGroup=Mode_Off;
		    BattStatus=Batt_OK; //电池电压检测重置
				}		
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
	    if(TempState!=SysTemp_OK) //发生过热
			   {
				 LightMode.LightGroup=Mode_Off;
				 CurrentLEDIndex=TempState==Systemp_DriverHigh?7:8;//驱动过热
				 BattStatus=Batt_OK; //电池电压检测重置
				 }
		  if(getSideKeyLongPressEvent()||BattStatus==Batt_LVFault)//长按或者触发低压fault，关机
				{		 
        CurrentLEDIndex=0; //关闭指示灯						
				LightMode.LightGroup=Mode_Off;
		    BattStatus=Batt_OK; //电池电压检测重置
				}		
	    else if(Click==1)LightMode.LightGroup=Mode_Cycle;//单击回到循环档
	    else if(Click==2)LightMode.LightGroup=Mode_Turbo; //双击极亮
	    break;
	 /* 开机 处于无极调光模式*/
	 case Mode_Ramp:				
		 if(getSideKeyDoubleClickAndHoldEvent())DisplayBatteryVoltage();//显示电池电压
	   if(getSideKeyTripleClickAndHoldEvent())DisplaySystemTemp();//显示系统温度
		 if(IsKeyStillPressed&&!getSideKeyHoldEvent())IsKeyStillPressed=false; //按键已经松开，可以继续响应  
	    if(TempState!=SysTemp_OK) //发生过热
			  {
				LightMode.LightGroup=Mode_Off;
				CurrentLEDIndex=TempState==Systemp_DriverHigh?7:8;//驱动过热
				BattStatus=Batt_OK; //电池电压检测重置
				}
	   if(Click>0||BattStatus==Batt_LVFault) //任意操作或者触发低压fault，关机
				{		
        CurrentLEDIndex=0; //关闭指示灯						
				LightMode.LightGroup=Mode_Off;
		    BattStatus=Batt_OK; //电池电压检测重置
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
 float Duty;
 //设置DCDC-EN
 if(!LightMode.IsMainLEDOn)GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP); //主LED关闭输出，禁用DCDC
 else switch(LightMode.LightGroup)
   {
	 case Mode_Cycle:
	 case Mode_Turbo:
	 case Mode_Ramp:
	 case Mode_Strobe:GPIO_SetOutBits(AUXV33_IOG,AUXV33_IOP);break;//灯具启动，使能DCDC
	 default:GPIO_ClearOutBits(AUXV33_IOG,AUXV33_IOP);//其余状态DCDC-EN 默认输出0
	 }
 delay_ms(1); //这个延时确保EN先输出再给整定
 //设置爆闪定时器
 TM_SetCounterReload(HT_GPTM1,(int)(LightMode.LightGroup==Mode_Strobe?10000/(StrobeFreq*2)-1:199)); //根据是否无极调光模式设置定时器 
 TM_Cmd(HT_GPTM1,(LightMode.LightGroup==Mode_Strobe||LightMode.LightGroup==Mode_Ramp)?ENABLE:DISABLE);
 if(LightMode.LightGroup!=Mode_Strobe&&LightMode.LightGroup!=Mode_Ramp)LightMode.IsMainLEDOn=true; //非爆闪挡位关闭定时器
 //设置占空比
 switch(LightMode.LightGroup)
   {
	 case Mode_Cycle:Duty=CycleModeRatio[LightMode.CycleModeCount];break; //循环档，从模式组里面取占空比
	 case Mode_Turbo:Duty=TurboDuty;break;//极亮占按照目标值设置占空比
	 case Mode_Ramp:Duty=RampMinDuty+((RampMaxDuty-RampMinDuty)*pow(DimmingRatio,GammaCorrectionValue));break; //无极调光模式，按照目标值设置占空比
	 case Mode_Strobe:Duty=BattStatus==Batt_LVAlert?30:TurboDuty;break;//爆闪，如果电池没有低压警告则按照极亮功率，否则锁30%
	 default:Duty=0; //其余状态占空比等于0
	 }
 if(Duty>100)Duty=100;
 if(Duty<0)Duty=0; //限制占空比范围为0-100%
 if(!LightMode.IsMainLEDOn)Duty=0; //禁用LED，占空比设置为0
 SetPWMDuty(Duty*=(LightMode.MainLEDThrottleRatio/(float)100)); //写PWM寄存器设置占空比(和温度控制调节值加权之后的结果)
 }

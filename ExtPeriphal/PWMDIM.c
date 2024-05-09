#include "delay.h"
#include "PWMDIM.h"
#include "ADC.h"
#include "FirmwarePref.h"

//全局变量
static float Current_integral=0;
static float Current_Lasterror=0;
static float PIDDutyOut=10; //调节的占空比输出

//极亮直驱的PID平均电流恒流运算
float CalcDirectDriveDuty(float Current)
  {
	ADCOutTypeDef ADCO;
	float error,buf;
	//进行转换并计算误差
	ADC_GetResult(&ADCO);
	error=Current-ADCO.DDLEDCurrent; //将目标值减去实测值得到误差	
	//计算比例和微分项(PD)
	PIDDutyOut+=DDILoopKp*error; //比例项
	PIDDutyOut+=(error-Current_Lasterror)*DDILoopKd;//微分项
	Current_Lasterror=error;//存储本次计算的错误值
	//计算积分项	
  Current_integral+=error/500; //电流环积分时间常数不能过短，故需要减少误差
	if(Current_integral>15)Current_integral=15;
	if(Current_integral<-15)Current_integral=-15; //积分累计限幅
	buf=Current_integral*DDILoopKi; //暂存积分项
  //返回更新后的占空比
	if(PIDDutyOut>100)PIDDutyOut=100;
	if(PIDDutyOut<2)PIDDutyOut=2; //PWM调整值的最大和最小限幅
	return buf+PIDDutyOut; //返回计算的占空比结果
	}
//对直驱的PID恒流模块进行reset
void ResetDirectDriveCCPID(void)
  {
	Current_integral=0;
	Current_Lasterror=0; //复位微分器和积分器
	PIDDutyOut=10; //占空比输出为10%
	}
	
//计算OC5021B线性调光的最大可达电流
float OC5021B_ICCMax(void)
  {
	float buf;
	//计算PWMDAC FullScale时的shunt Voltage
	buf=(ADC_AVRef-1.1)/(3.1-1.1); //得到在DAC FullScele的时候的百分比
	buf*=250; //乘以DIM=3.1V时的full Scale shunt threshold得到实际的shunt电压(mV)
	return buf/OC5021B_ShuntmOhm; //将buf值除以算出的shunt voltage得到可能的最大电流
	}
//计算OC5021B在某个对应电流时PWMDAC的占空比
float OC5021B_CalcPWMDACDuty(float Current)
  {
	float DACVID;
	float MinimumRatio=(float)OC5021B_MinimumIsetRatio/100; //OC5021B能达到的最低电流比例
	Current=Current/OC5021B_ICCMax(); //将目标电流值除以ICCMax得到实际的电流比例
	if(Current<MinimumRatio)	//电流比例过小，低于5021B能达到的下限
	  {
		OC5021B_SetHybridDuty(Current/MinimumRatio); //设置混合调光的输出占空比为目标值
		Current=MinimumRatio;//限制PWMDAC的输出百分比为最低值
		}		
	else OC5021B_SetHybridDuty(100); //纯DC调光,PWM部分输出100%
	DACVID=1.1+((ADC_AVRef-1.1)*Current); //计算出DAC的目标电压
	if(DACVID<0)DACVID=0;
	if(DACVID>ADC_AVRef)DACVID=ADC_AVRef; //限制计算出的VID为模拟Full Scale 
	DACVID/=ADC_AVRef; //除以模拟的Full-Scale电压得到实际的占空比
	return DACVID;
	}	
//设置OC5021B混合调光的占空比
void OC5021B_SetHybridDuty(float Duty)	
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
  //写通道3比较寄存器后根据占空比启用/禁用定时器输出
  HT_MCTM0->CH3CCR=(unsigned int)buf; 
  AFIO_GPxConfig(HybridPWMO_IOB,HybridPWMO_IOP,Duty>0?AFIO_FUN_MCTM_GPTM:AFIO_FUN_GPIO); //如果占空比大于0，则设置为MCTM复用IO,否则为GPIO	
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
 //写通道0比较寄存器后根据占空比启用/禁用定时器输出
 HT_MCTM0->CH0CCR=(unsigned int)buf; 
 AFIO_GPxConfig(PWMO_IOB,PWMO_IOP,Duty>0?AFIO_FUN_MCTM_GPTM:AFIO_FUN_GPIO); //如果占空比大于0，则设置为MCTM复用IO,否则为GPIO
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
 //配置通道3的输出级
 MCTM_OutputInitStructure.Channel = TM_CH_3; //定时器输出通道3
 MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1; //PWM正输出模式（if CNT < CMP PWM=1 else PWM =0）
 MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
 MCTM_OutputInitStructure.ControlN = TM_CHCTL_DISABLE; //开启正极性输出，负极性禁用
 MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
 MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED; //配置极性
 MCTM_OutputInitStructure.IdleState = MCTM_OIS_LOW;
 MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_HIGH; //IDLE状态关闭
 MCTM_OutputInitStructure.Compare = (SYSHCLKFreq / PWMFreq); //默认输出100%占空比
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
 AFIO_GPxConfig(PWMO_IOB,PWMO_IOP, AFIO_FUN_MCTM_GPTM);//DC调光和极亮直驱PWM引脚配置为MCTM0复用GPIO
 GPIO_DirectionConfig(PWMO_IOG,PWMO_IOP,GPIO_DIR_OUT);	
 GPIO_ClearOutBits(PWMO_IOG,PWMO_IOP);//设置为输出,ODR=0
 //配置OC5021B 混合调光引脚
 AFIO_GPxConfig(HybridPWMO_IOB,HybridPWMO_IOP, AFIO_FUN_MCTM_GPTM);//DC调光和极亮直驱PWM引脚配置为MCTM0复用GPIO
 GPIO_DirectionConfig(HybridPWMO_IOG,HybridPWMO_IOP,GPIO_DIR_OUT);	
 GPIO_ClearOutBits(HybridPWMO_IOG,HybridPWMO_IOP);//设置为输出,ODR=0 
 //启动定时器TBU和比较通道1的输出功能
 TM_Cmd(HT_MCTM0, ENABLE); 
 MCTM_CHMOECmd(HT_MCTM0, ENABLE);
 }

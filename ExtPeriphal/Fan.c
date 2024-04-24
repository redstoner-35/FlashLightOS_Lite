#include "PWMDIM.h"
#include "Fan.h"
#include "ModeLogic.h"
#include "FirmwarePref.h"

#ifdef EnableFanControl
//外部和全局变量声明
extern float PIDInputTemp; //PID输入温度（作为风扇的控制温度）
extern volatile LightStateDef LightMode; //全局变量
extern BatteryAlertDef BattStatus;//电池状态
bool IsFanNeedtoSpin=false; //全局变量，风扇是否旋转
#endif

//初始化风扇控制
void FanOutputInit(void)
 {
 #ifdef EnableFanControl
 TM_OutputInitTypeDef MCTM_OutputInitStructure;
 //配置GPIO
 AFIO_GPxConfig(FanPWMO_IOB,FanPWMO_IOP, AFIO_FUN_MCTM_GPTM);//配置为MCTM0 CH3复用GPIO
 GPIO_DirectionConfig(FanPWMO_IOG,FanPWMO_IOP,GPIO_DIR_OUT);//输出	
 GPIO_ClearOutBits(FanPWMO_IOG,FanPWMO_IOP);//将对应IO的输出ODR设置为0
	 
 AFIO_GPxConfig(FanPWREN_IOB,FanPWREN_IOP, AFIO_FUN_GPIO);
 GPIO_DirectionConfig(FanPWREN_IOG,FanPWREN_IOP,GPIO_DIR_OUT);//配置为输出
 GPIO_ClearOutBits(FanPWREN_IOG,FanPWREN_IOP);//输出设置为0
 //配置输出通道
 MCTM_OutputInitStructure.Channel = TM_CH_3; //定时器输出通道3
 MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1; //PWM正输出模式（if CNT < CMP PWM=1 else PWM =0）
 MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
 MCTM_OutputInitStructure.ControlN = TM_CHCTL_DISABLE; //开启正极性输出，负极性禁用
 MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
 MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED; //配置极性
 MCTM_OutputInitStructure.IdleState = MCTM_OIS_LOW;
 MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_HIGH; //IDLE状态关闭
 MCTM_OutputInitStructure.Compare = ((SYSHCLKFreq/PWMFreq)*50)/100; //默认输出50%占空比
 MCTM_OutputInitStructure.AsymmetricCompare = 0;  //非对称比较关闭
 TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure); //配置输出比较
 //配置完毕，使能风扇让风扇开始旋转
 GPIO_SetOutBits(FanPWREN_IOG,FanPWREN_IOP);  //启用输出比较，电源使能=1
 #endif
 }

//强制关闭风扇
void ForceDisableFan(void)
 {
 #ifdef EnableFanControl
 GPIO_ClearOutBits(FanPWREN_IOG,FanPWREN_IOP); //设置风扇电源为0
 AFIO_GPxConfig(FanPWMO_IOB,FanPWMO_IOP,AFIO_FUN_GPIO);
 HT_MCTM0->CH3CCR=(unsigned int)0;  //直接把复用IO设置为GPIO，输出0% 
 #endif
 }	
 
//风扇速度控制函数
void FanSpeedControlHandler(void)
 {
 #ifdef EnableFanControl
 float PWMVal,delta;
 //计算风扇是否需要旋转
 if(LightMode.LightGroup==Mode_Turbo)IsFanNeedtoSpin=true; //极亮开启，风扇强制开始旋转
 else if(BattStatus==Batt_LVFault)IsFanNeedtoSpin=false; //电池电量严重过低，此时风扇关闭
 else if(IsFanNeedtoSpin&&PIDInputTemp<FanStopTemp)IsFanNeedtoSpin=false; //当前温度小于40，风扇关闭
 else if(!IsFanNeedtoSpin&&PIDInputTemp>FanStartupTemp)IsFanNeedtoSpin=true; //温度大于45风扇开始旋转
 //根据温度值计算目标的PWM数值
 if(PIDInputTemp<=FanStartupTemp)PWMVal=FanMinimumPWM; //温度小于或等于停止值，风扇最低速度运行
 else if(PIDInputTemp>=FanFullSpeedTemp)PWMVal=100; //温度达到或超过全速值，风扇全速运行
 else //使用比例计算
   {
	 delta=(float)(100-FanMinimumPWM); //计算出温度从开始到全速的总共PWM差值
	 delta/=(float)(FanFullSpeedTemp-FanStartupTemp); //除以温差得到每一摄氏度对应的PWM增量
	 PWMVal=delta*(PIDInputTemp-FanStartupTemp); //计算出PWM相对于风扇启动基准点的增量
	 PWMVal+=FanMinimumPWM; //加上基准点的数值得到最终的PWM值
	 }
 //控制风扇速度并设置风扇电源
 if(IsFanNeedtoSpin)GPIO_SetOutBits(FanPWREN_IOG,FanPWREN_IOP);
 else GPIO_ClearOutBits(FanPWREN_IOG,FanPWREN_IOP); //设置风扇电源
 if(PWMVal<0||!IsFanNeedtoSpin)PWMVal=0; //如果PWM值超出范围或者风扇被禁用，停止输出
 else if(PWMVal>100)PWMVal=100; //超过100则限制为100
 AFIO_GPxConfig(FanPWMO_IOB,FanPWMO_IOP, PWMVal>0?AFIO_FUN_MCTM_GPTM:AFIO_FUN_GPIO);//如果占空比为0则将输出配置为普通GPIO直接输出0，否则输出PWM
 PWMVal*=(float)(SYSHCLKFreq / PWMFreq); //乘以100%占空比的重装值
 PWMVal/=100; //除以100得到实际重装值	 
 HT_MCTM0->CH3CCR=(unsigned int)PWMVal;  //设置通道3CCR寄存器 
 #endif
 }

#ifndef Pref
#define Pref

/**************** 固件挡位和通用配置 *********************
这部分的宏定义负责配置固件中的循环档位，极亮挡位的参数。
默认出厂配置为2-5-10-25-50%循环，极亮和爆闪100%。如果将
循环档位配置为-1则可以直接跳过该挡位。
*********************************************************/
#define CycleMode1Duty 2
#define CycleMode2Duty 5
#define CycleMode3Duty 10
#define CycleMode4Duty 25
#define CycleMode5Duty 50  //五个循环档位的占空比(单位%)

#define RampMinDuty 2
#define RampMaxDuty 50 //无极调光模式的最低和最高占空比(单位%)
#define GammaCorrectionValue 2.45 //进行人眼亮度拟合的gamma修正值
#define RampTimeSpan 4 //无极调光从最低到最高亮度连续调节的总耗时(秒)

#define TurboDuty 100 //极亮和爆闪模式的占空比(单位%)
#define StrobeFreq 5 //爆闪挡位频率(Hz)

#define EnableInstantTurbo //启用一键极亮功能(关机状态下双击直接极亮)
#define EnableInstantStrobe //启用一键爆闪功能(关机状态下三击直接爆闪)

#define DeepSleepTimeOut 10 //设定驱动在无操作后进入深度睡眠前的延时(s)
#define AutoLockTimeOut 5 //设定驱动在无操作后自动锁定的延时(分钟)

/***************** 固件温度控制模块配置 ******************
这部分的宏定义负责配置固件的温度测量系统和PID温度控制器的
设置。默认固件支持B值3950，10K@25℃的NTC热敏电阻。温度控制
器则默认配置为在LED和驱动温度的加权值达到60度时启动温控，
通过PID方式将温度限制在50度，并且在温度低于45度时停止温控
*********************************************************/
#define NTCBValue 3950  //NTCb值
#define NTCUpperResValueK 10 //NTC上拉到VDD的电阻阻值(KΩ)
#define NTCT0 25   //NTC阻值=10K时的标定温度

#define LEDNTCTRIM 0.5
#define MOSNTCTRIM 0.5 //LED和驱动自身温度测量的修正值(℃)

#define TriggerTemp 60 //触发温度(℃)温度高于此数值后驱动将降档
#define ReleaseTemp 45 //恢复温度(℃)温度低于此数值后驱动将停止降档
#define MaintainTemp 50 //维持温度(℃)PID温控器对LED和驱动进行恒温操作的目标温度。
#define MOSThermalAlert 70 //驱动部分的温度警报阈值(℃)当驱动温度高于该值后会强制触发降档

#define MOSThermalTrip 105 //驱动自身的过热关机保护温度
#define LEDThermalTrip 80 //LED过热关机保护温度

#define EnableDriverNTC //使能驱动的热敏电阻检测
#define EnableLEDNTC //使能LED的热敏电阻检测
//#define NTCStrictMode //严格NTC自检模式，在此模式下任意NTC自检失败都会导致驱动无法开机

/***************** 固件电池测量模块配置 ******************
这部分的宏定义负责配置固件的电池测量系统以及电池低压警报所
适用的电芯节数，以及电池电压测量通道的检测电阻阻值。默认该
固件的配置为单节三元锂离子电池(满电4.2V，通常电压3.7)
*********************************************************/
#define BatteryCellCount 1 //该固件所适用的锂电的节数
//#define NoAutoBatteryDetect //禁用固件的自动电池节数检测功能
//#define UsingLiFePO4Batt //如果固件需要使用磷酸铁锂电池，则请去除此宏的注释（警告！磷酸铁锂下电池最少需要2节）

#define VbattUpResK 100 //电池检测分压电阻上端阻值(KΩ)
#define VbattDownResK 10 //电池检测分压电阻下端阻值(KΩ)

#endif

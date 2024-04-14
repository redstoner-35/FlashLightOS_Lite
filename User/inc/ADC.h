#ifndef _ADC_
#define _ADC_

#include <stdbool.h>
#include "Pindefs.h"
#include "FirmwarePref.h"

/*下面的自动Define负责处理ADC的IO，ADC通道定义，不允许修改！*/
#define LED_NTC_IOP STRCAT2(AFIO_PIN_,LED_NTC_Ch) 
#define MOS_NTC_IOP STRCAT2(AFIO_PIN_,MOS_NTC_Ch)
#define Batt_Vsense_IOP STRCAT2(AFIO_PIN_,Batt_Vsense_Ch)

#define _Batt_Vsense_Ch STRCAT2(ADC_CH_,Batt_Vsense_Ch)
#define _LED_NTC_Ch STRCAT2(ADC_CH_,LED_NTC_Ch)
#define _MOS_NTC_Ch STRCAT2(ADC_CH_,MOS_NTC_Ch)

//系统ADC的参数
#define ADC_AVRef 2.500
#define ADCAvg 5  //ADC平均次数和参考电压

//温度传感器状态Enum
typedef enum
{
NTC_OK, //NTC就绪
NTC_Short,//NTC短路
NTC_Open //NTC开路
}NTCSensorStatDef;
//ADC结果输出
typedef struct
{
float LEDTemp;//LED温度
float MOSFETTemp;//MOS温度
float BatteryVoltage; //电池电压
NTCSensorStatDef LEDNTCState;//LED基板温度状态
NTCSensorStatDef MOSNTCState;//MOS温度状态
}ADCOutTypeDef;//ADC

//函数
float QueueLinearTable(int TableSize,float Input,float *Thr,float *Value);//线性表插值
void InternalADC_Init(void);//初始化内部ADC
bool ADC_GetLEDIfPinVoltage(float *VOUT);//ADC获取LED电流测量引脚的电压输入值
bool ADC_GetResult(ADCOutTypeDef *ADCOut);//获取温度和LED Vf
void OnChipADC_FaultHandler(void);//ADC异常处理
void ADC_EOC_interrupt_Callback(void);//ADC结束转换处理
void InternalADC_QuickInit(void);//内部ADC快速初始化

//外部ref
extern const char *NTCStateString[3];
extern const char *SPSTMONString[3];

#endif

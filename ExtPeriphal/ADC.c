#include "delay.h"
#include "ADC.h"
#include "LEDMgmt.h"
#include <math.h>
#include "PWMDIM.h"
#include "TempControl.h"

volatile static bool ADCEOCFlag=false;

//ADC结束转换回调
void ADC_EOC_interrupt_Callback(void)
  {
	ADC_ClearIntPendingBit(HT_ADC0, ADC_FLAG_CYCLE_EOC);//清除中断
  ADCEOCFlag=true;
	}
//片内ADC异常处理
void OnChipADC_FaultHandler(void)
  {
	CurrentLEDIndex=6;//指示ADC异常
  while(1);
	}

//ADC获取数值
bool ADC_GetResult(ADCOutTypeDef *ADCOut)
  {
  int retry,avgcount,i;
	unsigned int ADCResult[4]={0};
  float buf,Rt;
	//开始测量
	for(avgcount=0;avgcount<ADCAvg;avgcount++)
		{
		//初始化变量
		retry=0;
		ADCEOCFlag=false;
		//设置单次转换的组别后启动转换
		ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 3, 0);//单次触发模式,一次完成3个数据的转换
    ADC_RegularChannelConfig(HT_ADC0, _LED_NTC_Ch, LED_NTC_Ch, 0);
    ADC_RegularChannelConfig(HT_ADC0, _MOS_NTC_Ch, MOS_NTC_Ch, 0);	
		ADC_RegularChannelConfig(HT_ADC0, _Batt_Vsense_Ch, Batt_Vsense_Ch, 0);	
	  ADC_SoftwareStartConvCmd(HT_ADC0, ENABLE); 
		//等待转换结束
		while(!ADCEOCFlag)
		  {
			retry++;//重试次数+1
			delay_us(2);
			if(retry==2000)return false;//转换超时
			}
		//获取数据
		for(i=0;i<3;i++)ADCResult[i]+=HT_ADC0->DR[i]&0x0FFF;//采集四个规则通道的结果		
		}
	//转换完毕，求平均
  for(i=0;i<3;i++)ADCResult[i]/=avgcount;
  //计算LED温度
	buf=(float)ADCResult[LED_NTC_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
	Rt=((float)NTCUpperResValueK*buf)/(ADC_AVRef-buf);//得到NTC的电阻
	buf=1/((1/(273.15+(float)NTCT0))+log(Rt/(float)NTCUpperResValueK)/(float)NTCBValue);//计算出温度
	buf-=273.15;//减去开氏温标常数变为摄氏度
	buf+=(float)LEDNTCTRIM;//加上修正值	
	if(buf<(-40)||buf>125)	//温度传感器异常
	  {
		ADCOut->LEDNTCState=buf<(-40)?NTC_Open:NTC_Short;
		ADCOut->LEDTemp=0;
		}
	else  //温度传感器正常
	  {
	  ADCOut->LEDNTCState=NTC_OK;
		ADCOut->LEDTemp=buf;
		}
  //计算MOS温度
	buf=(float)ADCResult[MOS_NTC_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
	Rt=((float)NTCUpperResValueK*buf)/(ADC_AVRef-buf);//得到NTC的电阻
	buf=1/((1/(273.15+(float)NTCT0))+log(Rt/(float)NTCUpperResValueK)/(float)NTCBValue);//计算出温度
	buf-=273.15;//减去开氏温标常数变为摄氏度
	buf+=(float)MOSNTCTRIM;//加上修正值	
	if(buf<(-40)||buf>125)	//温度传感器异常
	  {
		ADCOut->MOSNTCState=buf<(-40)?NTC_Open:NTC_Short;
		ADCOut->MOSFETTemp=0;
		}
	else  //温度传感器正常
	  {
	  ADCOut->MOSNTCState=NTC_OK;
		ADCOut->MOSFETTemp=buf;
		}
	//计算电池电压
	buf=(float)ADCResult[Batt_Vsense_Ch]*(ADC_AVRef/(float)4096);//将AD值转换为电压
  buf/=(float)VbattDownResK/((float)VbattUpResK+(float)VbattDownResK); //根据电阻分压公式换算实际电池电压
	ADCOut->BatteryVoltage=buf; //返回电池电压
	//计算完毕
	return true;
	}
//内部ADC初始化
void InternalADC_Init(void)
  {
	 int i;
	 ADCOutTypeDef ADCO;
	 //将LED Vf测量引脚和温度引脚配置为AD模式
	 AFIO_GPxConfig(GPIO_PA, LED_NTC_IOP,AFIO_FUN_ADC0);
	 AFIO_GPxConfig(GPIO_PA,MOS_NTC_IOP,AFIO_FUN_ADC0);
	 AFIO_GPxConfig(GPIO_PA,Batt_Vsense_IOP,AFIO_FUN_ADC0);
   //设置转换组别类型，转换时间，转换模式      
	 CKCU_SetADCnPrescaler(CKCU_ADCPRE_ADC0, CKCU_ADCPRE_DIV16);//ADC时钟为主时钟16分频=3MHz                                               
   ADC_RegularGroupConfig(HT_ADC0,DISCONTINUOUS_MODE, 4, 0);//单次触发模式,一次完成4个数据的转换
   ADC_SamplingTimeConfig(HT_ADC0,25); //采样时间（25个ADC时钟）
   ADC_RegularTrigConfig(HT_ADC0, ADC_TRIG_SOFTWARE);//软件启动	
	 //设置单次转换的组别
   ADC_RegularChannelConfig(HT_ADC0, _LED_NTC_Ch, LED_NTC_Ch, 0);
   ADC_RegularChannelConfig(HT_ADC0, _MOS_NTC_Ch, MOS_NTC_Ch, 0);	
	 ADC_RegularChannelConfig(HT_ADC0, _Batt_Vsense_Ch, Batt_Vsense_Ch, 0);	
	 //启用ADC中断    
   ADC_IntConfig(HT_ADC0,ADC_INT_CYCLE_EOC,ENABLE);                                                                            
   NVIC_EnableIRQ(ADC0_IRQn);
	 //启用ADC
	 ADC_Cmd(HT_ADC0, ENABLE);
	 //触发ADC转换检查转换是否成功
	 for(i=0;i<3;i++)if(!ADC_GetResult(&ADCO))OnChipADC_FaultHandler();//出现异常
	 //检查LED NTC状态
	 #ifdef EnableLEDNTC
	 if(ADCO.LEDNTCState!=NTC_OK)
	  {
		#ifdef NTCStrictMode	
		  #ifdef CarLampMode
	    GenerateMainLEDStrobe(3); //LED NTC故障，车灯模式，主灯闪烁3次提示用户
      #else			
	    CurrentLEDIndex=4;//指示LED NTC故障
	    #endif
    while(1);
		#else
		if(ADCO.MOSNTCState!=NTC_OK)	
		  {
			#ifdef CarLampMode	
      GenerateMainLEDStrobe(5); //所有NTC故障，车灯模式，主灯闪烁5次提示用户
			#else
			CurrentLEDIndex=20;//指示所有NTC均已损坏
		  #endif
			while(1);
			}
		else 
			#ifdef CarLampMode
	    GenerateMainLEDStrobe(3); //LED NTC故障，车灯模式，主灯闪烁3次提示用户
      #else		
		  LED_ShowLoopOperationOnce(4); //显示LED NTC故障后继续	
		  #endif
	  #endif
	  }
	#endif
	//检查驱动NTC状态
  #ifdef EnableDriverNTC		
	if(ADCO.MOSNTCState!=NTC_OK)
	  {
		#ifdef NTCStrictMode
		  #ifdef CarLampMode
			GenerateMainLEDStrobe(4); //驱动NTC故障，车灯模式，主灯闪烁4次提示用户
	    #else
			CurrentLEDIndex=5;//指示驱动自身 NTC故障
			#endif
    while(1);
		#else
		if(ADCO.LEDNTCState!=NTC_OK)	
	    {
      #ifdef CarLampMode	
      GenerateMainLEDStrobe(5); //所有NTC故障，车灯模式，主灯闪烁5次提示用户
			#else
			CurrentLEDIndex=20;//指示所有NTC均已损坏
		  #endif
			while(1);
			}
		else 
			#ifdef CarLampMode
		  GenerateMainLEDStrobe(4); //驱动NTC故障，车灯模式，主灯闪烁4次提示用户
		  #else		
		  LED_ShowLoopOperationOnce(5); //显示驱动NTC故障后继续		
      #endif		
	  #endif
	  }	
	#endif
	//填充温度数据
	FillThermalFilterBuf(&ADCO);	
	}

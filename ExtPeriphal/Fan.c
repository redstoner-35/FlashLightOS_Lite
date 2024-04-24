#include "PWMDIM.h"
#include "Fan.h"
#include "ModeLogic.h"
#include "FirmwarePref.h"

//�ⲿ��ȫ�ֱ�������
extern float PIDInputTemp; //PID�����¶ȣ���Ϊ���ȵĿ����¶ȣ�
extern volatile LightStateDef LightMode; //ȫ�ֱ���
extern BatteryAlertDef BattStatus;//���״̬
bool IsFanNeedtoSpin=false; //ȫ�ֱ����������Ƿ���ת

//��ʼ�����ȿ���
void FanOutputInit(void)
 {
 TM_OutputInitTypeDef MCTM_OutputInitStructure;
 //����GPIO
 AFIO_GPxConfig(FanPWMO_IOB,FanPWMO_IOP, AFIO_FUN_MCTM_GPTM);//����ΪMCTM0 CH3����GPIO
 GPIO_DirectionConfig(FanPWMO_IOG,FanPWMO_IOP,GPIO_DIR_OUT);//���	
 
 AFIO_GPxConfig(FanPWREN_IOB,FanPWREN_IOP, AFIO_FUN_GPIO);
 GPIO_DirectionConfig(FanPWREN_IOG,FanPWREN_IOP,GPIO_DIR_OUT);//����Ϊ���
 GPIO_ClearOutBits(FanPWREN_IOG,FanPWREN_IOP);//�������Ϊ0
 //�������ͨ��
 MCTM_OutputInitStructure.Channel = TM_CH_3; //��ʱ�����ͨ��3
 MCTM_OutputInitStructure.OutputMode = TM_OM_PWM1; //PWM�����ģʽ��if CNT < CMP PWM=1 else PWM =0��
 MCTM_OutputInitStructure.Control = TM_CHCTL_ENABLE;
 MCTM_OutputInitStructure.ControlN = TM_CHCTL_DISABLE; //��������������������Խ���
 MCTM_OutputInitStructure.Polarity = TM_CHP_NONINVERTED;
 MCTM_OutputInitStructure.PolarityN = TM_CHP_NONINVERTED; //���ü���
 MCTM_OutputInitStructure.IdleState = MCTM_OIS_LOW;
 MCTM_OutputInitStructure.IdleStateN = MCTM_OIS_HIGH; //IDLE״̬�ر�
 MCTM_OutputInitStructure.Compare = ((SYSHCLKFreq/PWMFreq)*50)/100; //Ĭ�����50%ռ�ձ�
 MCTM_OutputInitStructure.AsymmetricCompare = 0;  //�ǶԳƱȽϹر�
 TM_OutputInit(HT_MCTM0, &MCTM_OutputInitStructure); //��������Ƚ�
 //������ϣ�ʹ�ܷ����÷��ȿ�ʼ��ת
 GPIO_SetOutBits(FanPWREN_IOG,FanPWREN_IOP);  //��������Ƚϣ���Դʹ��=1
 }

//ǿ�ƹرշ���
void ForceDisableFan(void)
 {
 GPIO_ClearOutBits(FanPWREN_IOG,FanPWREN_IOP); //���÷��ȵ�ԴΪ0
 HT_MCTM0->CH3CCR=(unsigned int)0;  //���0% 
 }	
 
//�����ٶȿ��ƺ���
void FanSpeedControlHandler(void)
 {
 float PWMVal,delta;
 //��������Ƿ���Ҫ��ת
 if(LightMode.LightGroup==Mode_Turbo)IsFanNeedtoSpin=true; //��������������ǿ�ƿ�ʼ��ת
 else if(BattStatus==Batt_LVFault)IsFanNeedtoSpin=false; //��ص������ع��ͣ���ʱ���ȹر�
 else if(IsFanNeedtoSpin&&PIDInputTemp<FanStopTemp)IsFanNeedtoSpin=false; //��ǰ�¶�С��40�����ȹر�
 else if(!IsFanNeedtoSpin&&PIDInputTemp>FanStartupTemp)IsFanNeedtoSpin=true; //�¶ȴ���45���ȿ�ʼ��ת
 //�����¶�ֵ����Ŀ���PWM��ֵ
 if(PIDInputTemp<=FanStartupTemp)PWMVal=FanMinimumPWM; //�¶�С�ڻ����ֵֹͣ����������ٶ�����
 else if(PIDInputTemp>=FanFullSpeedTemp)PWMVal=100; //�¶ȴﵽ�򳬹�ȫ��ֵ������ȫ������
 else //ʹ�ñ�������
   {
	 delta=(float)(100-FanMinimumPWM); //������¶ȴӿ�ʼ��ȫ�ٵ��ܹ�PWM��ֵ
	 delta/=(float)(FanFullSpeedTemp-FanStartupTemp); //�����²�õ�ÿһ���϶ȶ�Ӧ��PWM����
	 PWMVal=delta*(PIDInputTemp-FanStartupTemp); //�����PWM����ڷ���������׼�������
	 PWMVal+=FanMinimumPWM; //���ϻ�׼�����ֵ�õ����յ�PWMֵ
	 }
 //���Ʒ����ٶȲ����÷��ȵ�Դ
 if(IsFanNeedtoSpin)GPIO_SetOutBits(FanPWREN_IOG,FanPWREN_IOP);
 else GPIO_ClearOutBits(FanPWREN_IOG,FanPWREN_IOP); //���÷��ȵ�Դ
 if(PWMVal<0||!IsFanNeedtoSpin)PWMVal=0; //���PWMֵ������Χ���߷��ȱ����ã�ֹͣ���
 else if(PWMVal>100)PWMVal=100; //����100������Ϊ100
 PWMVal*=(float)(SYSHCLKFreq / PWMFreq); //����100%ռ�ձȵ���װֵ
 PWMVal/=100; //����100�õ�ʵ����װֵ	 
 HT_MCTM0->CH3CCR=(unsigned int)PWMVal;  //����ͨ��3CCR�Ĵ���
 }

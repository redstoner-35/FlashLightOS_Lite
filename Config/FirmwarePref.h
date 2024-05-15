#ifndef Pref
#define Pref

/**************** �̼���λ��ͨ������ *********************
�ⲿ�ֵĺ궨�帺�����ù̼��е�ѭ����λ��������λ�ĵ�������
Ĭ�ϳ�������Ϊ0.4-2-3.5-7-15Aѭ���������ͱ���100%�������
ѭ����λ����Ϊ-1�����ֱ�������õ�λ��
*********************************************************/
#define CycleMode1Current 0.4
#define CycleMode2Current 2
#define CycleMode3Current 3.5
#define CycleMode4Current 7
#define CycleMode5Current 15  //���ѭ����λ�ĵ���(��λA)

#define RampMinCurrent 0.4
#define RampMaxCurrent 15 //�޼�����ģʽ����ͺ���ߵ���(��λA)
#define GammaCorrectionValue 2.45 //��������������ϵ�gamma����ֵ
#define RampTimeSpan 4 //�޼��������͵���������������ڵ��ܺ�ʱ(��)

#define TurboCurrent 68 //�����ͱ���ģʽ��Ŀ�����(��λA)
#define StrobeFreq 5 //������λƵ��(Hz)

//#define EnhancedLEDDriveStrength //LEDָʾ������Ƿ���Ҫ��ǿ��������
#define EnableInstantTurbo //����һ����������(�ػ�״̬��˫��ֱ�Ӽ���)
#define EnableInstantStrobe //����һ����������(�ػ�״̬������ֱ�ӱ���)

#define DeepSleepTimeOut 10 //�趨�������޲�����������˯��ǰ����ʱ(s)
#define AutoLockTimeOut 5 //�趨�������޲������Զ���������ʱ(����)

#define OC5021B_MinimumIsetRatio 10 //���ø�����������OC5021B���ܴﵽ����С���������ٷֱ�(%)
#define OC5021B_ShuntmOhm 30 //���ø�����������OC5021B�ļ���������ֵ(��λmR)
/***************** �̼��¶ȿ���ģ������ ******************
�ⲿ�ֵĺ궨�帺�����ù̼����¶Ȳ���ϵͳ��PID�¶ȿ�������
���á�Ĭ�Ϲ̼�֧��Bֵ3950��10K@25���NTC�������衣�¶ȿ���
����Ĭ������Ϊ��LED�������¶ȵļ�Ȩֵ�ﵽ60��ʱ�����¿أ�
ͨ��PID��ʽ���¶�������50�ȣ��������¶ȵ���45��ʱֹͣ�¿�
*********************************************************/
#define NTCBValue 3950  //NTCbֵ
#define NTCUpperResValueK 10 //NTC������VDD�ĵ�����ֵ(K��)
#define NTCT0 25   //NTC��ֵ=10Kʱ�ı궨�¶�

#define LEDNTCTRIM 0.5
#define MOSNTCTRIM 0.5 //LED�����������¶Ȳ���������ֵ(��)

#define TriggerTemp 60 //�����¶�(��)�¶ȸ��ڴ���ֵ������������
#define ReleaseTemp 45 //�ָ��¶�(��)�¶ȵ��ڴ���ֵ��������ֹͣ����
#define MaintainTemp 50 //ά���¶�(��)PID�¿�����LED���������к��²�����Ŀ���¶ȡ�
#define MOSThermalAlert 70 //�������ֵ��¶Ⱦ�����ֵ(��)�������¶ȸ��ڸ�ֵ���ǿ�ƴ�������

#define MOSThermalTrip 105 //��������Ĺ��ȹػ������¶�
#define LEDThermalTrip 80 //LED���ȹػ������¶�

#define EnableDriverNTC //ʹ������������������
#define EnableLEDNTC //ʹ��LED������������
//#define NTCStrictMode //�ϸ�NTC�Լ�ģʽ���ڴ�ģʽ������NTC�Լ�ʧ�ܶ��ᵼ�������޷�����

/***************** �̼���ز���ģ������ ******************
�ⲿ�ֵĺ궨�帺�����ù̼��ĵ�ز���ϵͳ�Լ���ص�ѹ������
���õĵ�о�������Լ���ص�ѹ����ͨ���ļ�������ֵ��Ĭ�ϸ�
�̼�������Ϊ������Ԫ����ӵ��(����4.2V��ͨ����ѹ3.7)
*********************************************************/
#define BatteryCellCount 1 //�ù̼������õ�﮵�Ľ���
//#define NoAutoBatteryDetect //���ù̼����Զ���ؽ�����⹦��
//#define UsingLiFePO4Batt //����̼���Ҫʹ��������﮵�أ�����ȥ���˺��ע�ͣ����棡��������µ��������Ҫ2�ڣ�

#define VbattUpResK 100 //��ؼ���ѹ�����϶���ֵ(K��)
#define VbattDownResK 10 //��ؼ���ѹ�����¶���ֵ(K��)

#endif

#ifndef _Fan_
#define _Fan_

//�ڲ�����
#include "Pindefs.h"

//���Ŷ���
#define FanPWMO_IOG STRCAT2(HT_GPIO,FanPWMO_IOBank)
#define FanPWMO_IOB STRCAT2(GPIO_P,FanPWMO_IOBank)
#define FanPWMO_IOP STRCAT2(GPIO_PIN_,FanPWMO_IOPinNum) //PWM�������

#define FanPWREN_IOG STRCAT2(HT_GPIO,FanPWREN_IOBank)
#define FanPWREN_IOB STRCAT2(GPIO_P,FanPWREN_IOBank)
#define FanPWREN_IOP STRCAT2(GPIO_PIN_,FanPWREN_IOPinNum) //���ȵ�Դ����


//����
void FanOutputInit(void);//��ʼ�����ȿ���
void FanSpeedControlHandler(void); //�����ٶȿ��ƺ���
void ForceDisableFan(void);//ǿ�ƹرշ���

#endif

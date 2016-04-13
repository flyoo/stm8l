/*===========================================================================
* ��ַ ��http://www.cdebyte.com/   http://yhmcu.taobao.com/                 *
* ���� ������  ԭ �ں͵��ӹ�����  �� �ڰ��ص��ӿƼ����޹�˾                 * 
* �ʼ� ��yihe_liyong@126.com                                                *
* �绰 ��18615799380                                                        *
============================================================================*/

#include "bsp.h"         

// ��������
#define SEND_MAX        48       // �������ݵ���󳤶�

INT8U   Cnt1ms = 0;             // 1ms����������ÿ1ms��һ               
INT8U   LED_Time = 0;           // ������LED����ʱ��

// ������ر���
INT8U   COM_TxNeed = 0;
INT8U   COM_TimeOut = 0;
INT8U   COM_RxCounter = 0;
INT8U   COM_TxCounter = 0;
INT8U   COM_RxBuffer[65] = {0};
INT8U   COM_TxBuffer[65] = {0};

#define USRAT_SendByte()    USART_SendData8(COM_TxBuffer[COM_TxCounter++])
#define USRAT_RecvByte()    COM_RxBuffer[COM_RxCounter++]=USART_ReceiveData8()

/*===========================================================================
* ���� : DelayMs() => ��ʱ����(ms��)                                        *
* ���� ��x, ��Ҫ��ʱ����(0-255)                                             *
============================================================================*/
void DelayMs(INT8U x)
{
    Cnt1ms = 0;
    while (Cnt1ms <= x);
}

/*===========================================================================
* ���� ��TIM3_1MS_ISR() => ��ʱ��3������, ��ʱʱ���׼Ϊ1ms               *
============================================================================*/
void TIM3_1MS_ISR(void)
{
    Cnt1ms++;

    if (0 != COM_TimeOut)           
    {    
        if (--COM_TimeOut == 0)     
        { 
            if (COM_RxCounter > SEND_MAX)       { COM_RxCounter = 0; }
        }
    }
        
    if (0 != LED_Time)
    {
        if (--LED_Time == 0 )                   { LED_Y_OFF(); }
    }
}

/*=============================================================================
* ���� : USART_Send() => ͨ�����ڷ�������                                     *
* ���� ��buff, ����������     size�� ���ͳ���                                *
=============================================================================*/
void USART_Send(INT8U *buff, INT8U size)
{
    if (size == 0)          { return; }
    
    COM_TxNeed = 0;
    
    while (size --)         { COM_TxBuffer[COM_TxNeed++] = *buff++; }
    
    COM_TxCounter = 0;
    USART_ITConfig(USART_IT_TXE, ENABLE);
}

/*=============================================================================
* ����:  USART_RX_Interrupt() => ���ڽ����ж�                                 *
=============================================================================*/
void USART_RX_Interrupt(void)
{
    USRAT_RecvByte();
    
    if (COM_RxCounter > 30)         { COM_RxCounter = 0; }
        
    COM_TimeOut = 5;
}

/*=============================================================================
* ����:  USART_TX_Interrupt() =>���ڷ����ж�
=============================================================================*/
void USART_TX_Interrupt(void)
{
    if (COM_TxCounter < COM_TxNeed)     { USRAT_SendByte(); }   
    else    
    { 
        USART_ITConfig(USART_IT_TC, ENABLE);
        USART_ITConfig(USART_IT_TXE, DISABLE);

        if (USART_GetFlagStatus(USART_FLAG_TC))      
        {
            USART_ITConfig(USART_IT_TC, DISABLE); 
            COM_TxNeed = 0;
            COM_TxCounter = 0;
        }
    }
}

/*===========================================================================
* ���� ��MCU_Initial() => ��ʼ��CPU����Ӳ��                                 *
* ˵�� ����������Ӳ���ĳ�ʼ���������Ѿ�������C�⣬��bsp.c�ļ�               *
============================================================================*/
void MCU_Initial(void)
{
    SClK_Initial();         // ��ʼ��ϵͳʱ�ӣ�16M     
    GPIO_Initial();         // ��ʼ��GPIO                  
    TIM3_Initial();         // ��ʼ����ʱ��3����׼1ms 
    USART_Initial();        // ��ʼ������
    SPI_Initial();          // ��ʼ��SPI               

    enableInterrupts();     // �����ж�              
}

/*===========================================================================
* ���� ��RF_Initial() => ��ʼ��RFоƬ                                       *
* ˵�� ��SI44XX�Ĳ������Ѿ�������C�⣬��SI446X.c�ļ��� �ṩSPI��CSN������	*
         ���ɵ������ڲ����к����û������ٹ���SI44XX�ļĴ����������⡣       *
============================================================================*/
void RF_Initial(void)
{
	SI446X_RESET();         // SI446X ģ�鸴λ
    DelayMs(200);
    SI446X_CONFIG_INIT();   // �Ĵ�����ʼ�����Ĵ�������WDS���õ�ͷ�ļ�
    SI446X_SET_POWER(0x7F); // �������������Ϊ���
    SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // �������ģʽ          
}

/*===========================================================================
* ����: System_Initial() => ��ʼ��ϵͳ��������                              *
============================================================================*/
void System_Initial(void)
{
    MCU_Initial();      // ��ʼ��CPU����Ӳ��   
    RF_Initial();       // ��ʼ������оƬ,����ģʽ
    
    LED_Y_ON();
    LED_Time = 250;
}

/*===========================================================================
* ���� ��RF_RecvHandler() => �������ݽ��մ���                               * 
============================================================================*/
void RF_RecvHandler(void)
{
    INT8U length=0, recv_buffer[64]={0};
    
    SI446X_INT_STATUS(recv_buffer);     // ����Ƿ��յ�һ�����ݰ�
    
    if (recv_buffer[3] & (1<<4))        // �յ�һ�����ݰ�
    {       
        // ��ȡ���յ������ݳ��Ⱥ���������
        length = SI446X_READ_PACKET(recv_buffer);

        // �жϽ��������Ƿ���ȷ
        if (length <= SEND_MAX)
        {
            LED_Y_ON();                      // LED�򿪣�����ָʾ�յ�����
            LED_Time = 200;                
            USART_Send((INT8U*)recv_buffer, length);
        } 
        
        SI446X_CONFIG_INIT();   // �Ĵ�����ʼ�����Ĵ�������WDS���õ�ͷ�ļ�
        SI446X_SET_POWER(0x7F); // �������������Ϊ���
        SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // �������ģʽ  
    }    
}

/*===========================================================================
* ���� : BSP_RF_SendPacket() => ���߷������ݺ���                            *
============================================================================*/
void RF_SendPacket(void)
{
    INT8U i=0, length=0, buffer[65]={0}, TxBuffer[100];

    if ((0==COM_TimeOut) && (COM_RxCounter>0))
    {
        LED_R_ON();
        
        length = COM_RxCounter;
        COM_RxCounter = 0;
        
        for (i=0; i<length; i++)   { buffer[i] = COM_RxBuffer[i]; }
        
        SI446X_SEND_PACKET(buffer, length, 0, 0);       // ��������    
        do
        {
            SI446X_INT_STATUS(TxBuffer);
        }while (!(TxBuffer[3] & (1<<5)));               // �ȴ��������

//        DelayMs(550);
        
        SI446X_CONFIG_INIT();   // �Ĵ�����ʼ�����Ĵ�������WDS���õ�ͷ�ļ�
        SI446X_SET_POWER(0x7F); // �������������Ϊ���
        SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // �������ģʽ  

        LED_R_OFF(); 
    }
            
    
 
          
           
    
}

void keypro(void)
{
      if (!KEY_READX())  // ���԰����Ƿ�����  

            { 
               //while(!KEY_READX());
               LED_R_TOG(); 
               INT8U sbuffer[]={1};
               SI446X_SEND_PACKET(sbuffer, 1, 0, 0);       // �������� 
               DelayMs(150);
        
                SI446X_CONFIG_INIT();   // �Ĵ�����ʼ�����Ĵ�������WDS���õ�ͷ�ļ�
                SI446X_SET_POWER(0x7F); // �������������Ϊ���
                SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // �������ģʽ
              
            }
     
          if (!KEY_READZ())  // ���԰����Ƿ�����  

            {// while(!KEY_READZ());
               LED_R_TOG(); 
               INT8U sbuffer[]={3};
               SI446X_SEND_PACKET(sbuffer, 1, 0, 0);       // �������� 
               DelayMs(150);
        
                SI446X_CONFIG_INIT();   // �Ĵ�����ʼ�����Ĵ�������WDS���õ�ͷ�ļ�
                SI446X_SET_POWER(0x7F); // �������������Ϊ���
                SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // �������ģʽ
              
            }
      if (!KEY_READ())  // ���԰����Ƿ�����  

            {  
              
               while(!KEY_READ());
               LED_R_TOG(); 
               INT8U sbuffer[]={2};
               SI446X_SEND_PACKET(sbuffer, 1, 0, 0);       // �������� 
               DelayMs(150);
        
                SI446X_CONFIG_INIT();   // �Ĵ�����ʼ�����Ĵ�������WDS���õ�ͷ�ļ�
                SI446X_SET_POWER(0x7F); // �������������Ϊ���
                SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // �������ģʽ
              
            }
}
/*===========================================================================
* ���� : main() => ���������������                                         *
============================================================================*/
void main(void)
{
	System_Initial();       // ��ʼ��ϵͳ��������               

	while (1)
	{
            //keypro();
	    RF_SendPacket();
	    RF_RecvHandler();
	}
}

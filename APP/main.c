/*===========================================================================
* 网址 ：http://www.cdebyte.com/   http://yhmcu.taobao.com/                 *
* 作者 ：李勇  原 亿和电子工作室  现 亿佰特电子科技有限公司                 * 
* 邮件 ：yihe_liyong@126.com                                                *
* 电话 ：18615799380                                                        *
============================================================================*/

#include "bsp.h"         

// 常量定义
#define SEND_MAX        48       // 发送数据的最大长度

INT8U   Cnt1ms = 0;             // 1ms计数变量，每1ms加一               
INT8U   LED_Time = 0;           // 接收数LED持续时间

// 串口相关变量
INT8U   COM_TxNeed = 0;
INT8U   COM_TimeOut = 0;
INT8U   COM_RxCounter = 0;
INT8U   COM_TxCounter = 0;
INT8U   COM_RxBuffer[65] = {0};
INT8U   COM_TxBuffer[65] = {0};

#define USRAT_SendByte()    USART_SendData8(COM_TxBuffer[COM_TxCounter++])
#define USRAT_RecvByte()    COM_RxBuffer[COM_RxCounter++]=USART_ReceiveData8()

/*===========================================================================
* 函数 : DelayMs() => 延时函数(ms级)                                        *
* 输入 ：x, 需要延时多少(0-255)                                             *
============================================================================*/
void DelayMs(INT8U x)
{
    Cnt1ms = 0;
    while (Cnt1ms <= x);
}

/*===========================================================================
* 函数 ：TIM3_1MS_ISR() => 定时器3服务函数, 定时时间基准为1ms               *
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
* 函数 : USART_Send() => 通过串口发送数据                                     *
* 输入 ：buff, 待发送数据     size， 发送长度                                *
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
* 函数:  USART_RX_Interrupt() => 串口接收中断                                 *
=============================================================================*/
void USART_RX_Interrupt(void)
{
    USRAT_RecvByte();
    
    if (COM_RxCounter > 30)         { COM_RxCounter = 0; }
        
    COM_TimeOut = 5;
}

/*=============================================================================
* 函数:  USART_TX_Interrupt() =>串口发送中断
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
* 函数 ：MCU_Initial() => 初始化CPU所有硬件                                 *
* 说明 ：关于所有硬件的初始化操作，已经被建成C库，见bsp.c文件               *
============================================================================*/
void MCU_Initial(void)
{
    SClK_Initial();         // 初始化系统时钟，16M     
    GPIO_Initial();         // 初始化GPIO                  
    TIM3_Initial();         // 初始化定时器3，基准1ms 
    USART_Initial();        // 初始化串口
    SPI_Initial();          // 初始化SPI               

    enableInterrupts();     // 打开总中断              
}

/*===========================================================================
* 函数 ：RF_Initial() => 初始化RF芯片                                       *
* 说明 ：SI44XX的操作，已经被建成C库，见SI446X.c文件， 提供SPI和CSN操作，	*
         即可调用其内部所有函数用户无需再关心SI44XX的寄存器操作问题。       *
============================================================================*/
void RF_Initial(void)
{
	SI446X_RESET();         // SI446X 模块复位
    DelayMs(200);
    SI446X_CONFIG_INIT();   // 寄存器初始化，寄存器来自WDS配置的头文件
    SI446X_SET_POWER(0x7F); // 将输出功率配置为最大
    SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // 进入接收模式          
}

/*===========================================================================
* 函数: System_Initial() => 初始化系统所有外设                              *
============================================================================*/
void System_Initial(void)
{
    MCU_Initial();      // 初始化CPU所有硬件   
    RF_Initial();       // 初始化无线芯片,发送模式
    
    LED_Y_ON();
    LED_Time = 250;
}

/*===========================================================================
* 函数 ：RF_RecvHandler() => 无线数据接收处理                               * 
============================================================================*/
void RF_RecvHandler(void)
{
    INT8U length=0, recv_buffer[64]={0};
    
    SI446X_INT_STATUS(recv_buffer);     // 检测是否收到一个数据包
    
    if (recv_buffer[3] & (1<<4))        // 收到一个数据包
    {       
        // 读取接收到的数据长度和数据内容
        length = SI446X_READ_PACKET(recv_buffer);

        // 判断接收数据是否正确
        if (length <= SEND_MAX)
        {
            LED_Y_ON();                      // LED打开，用于指示收到数据
            LED_Time = 200;                
            USART_Send((INT8U*)recv_buffer, length);
        } 
        
        SI446X_CONFIG_INIT();   // 寄存器初始化，寄存器来自WDS配置的头文件
        SI446X_SET_POWER(0x7F); // 将输出功率配置为最大
        SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // 进入接收模式  
    }    
}

/*===========================================================================
* 函数 : BSP_RF_SendPacket() => 无线发送数据函数                            *
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
        
        SI446X_SEND_PACKET(buffer, length, 0, 0);       // 发送数据    
        do
        {
            SI446X_INT_STATUS(TxBuffer);
        }while (!(TxBuffer[3] & (1<<5)));               // 等待发射完成

//        DelayMs(550);
        
        SI446X_CONFIG_INIT();   // 寄存器初始化，寄存器来自WDS配置的头文件
        SI446X_SET_POWER(0x7F); // 将输出功率配置为最大
        SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // 进入接收模式  

        LED_R_OFF(); 
    }
            
    
 
          
           
    
}

void keypro(void)
{
      if (!KEY_READX())  // 测试按键是否正常  

            { 
               //while(!KEY_READX());
               LED_R_TOG(); 
               INT8U sbuffer[]={1};
               SI446X_SEND_PACKET(sbuffer, 1, 0, 0);       // 发送数据 
               DelayMs(150);
        
                SI446X_CONFIG_INIT();   // 寄存器初始化，寄存器来自WDS配置的头文件
                SI446X_SET_POWER(0x7F); // 将输出功率配置为最大
                SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // 进入接收模式
              
            }
     
          if (!KEY_READZ())  // 测试按键是否正常  

            {// while(!KEY_READZ());
               LED_R_TOG(); 
               INT8U sbuffer[]={3};
               SI446X_SEND_PACKET(sbuffer, 1, 0, 0);       // 发送数据 
               DelayMs(150);
        
                SI446X_CONFIG_INIT();   // 寄存器初始化，寄存器来自WDS配置的头文件
                SI446X_SET_POWER(0x7F); // 将输出功率配置为最大
                SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // 进入接收模式
              
            }
      if (!KEY_READ())  // 测试按键是否正常  

            {  
              
               while(!KEY_READ());
               LED_R_TOG(); 
               INT8U sbuffer[]={2};
               SI446X_SEND_PACKET(sbuffer, 1, 0, 0);       // 发送数据 
               DelayMs(150);
        
                SI446X_CONFIG_INIT();   // 寄存器初始化，寄存器来自WDS配置的头文件
                SI446X_SET_POWER(0x7F); // 将输出功率配置为最大
                SI446X_START_RX(0, 0, PACKET_LENGTH, 8, 8, 8);  // 进入接收模式
              
            }
}
/*===========================================================================
* 函数 : main() => 主函数，程序入口                                         *
============================================================================*/
void main(void)
{
	System_Initial();       // 初始化系统所有外设               

	while (1)
	{
            keypro();
	    RF_SendPacket();
	    RF_RecvHandler();
	}
}

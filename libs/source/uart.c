#include "uart.h"
#include "stm32f10x.h"
#include "irda.h"
static int top = -1;	//Stack Pointer
static char gCmdCache[CMD_MAX_LENGTH];


void uart_init(unsigned int pclk2, unsigned int bound) {
    float temp;
    unsigned short mantissa;
    unsigned short fraction;
    temp = (float)(pclk2*1000000)/(bound*16);
    mantissa = temp;
    fraction = (temp - mantissa) * 16;
    mantissa <<= 4;
    mantissa += fraction;
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;		// 打开 GPIOA 时钟
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;	// 打开 USART1 时钟

    GPIOA->CRH &= 0xFFFFF00F;				// USART GPIO 引脚配置
    GPIOA->CRH |= 0x000008B0;

    RCC->APB2RSTR |= RCC_APB2RSTR_USART1RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART1RST;

    USART1->BRR = mantissa;
    USART1->CR1 |= 0x200C;	//设置 UE, TE and RE

    USART1->CR1 |= 1<<8;	//PE 中断使能
    USART1->CR1 |= 1<<5;	//RX 非空中断使能


    USART1->SR;     // 读取 SR 寄存器以将 TXE 和 TE 位清零,(复位值: 0x00C0)

	// SCB->AIRCR &= 0x05FAF8FF;	// AIRCE Key: 0x05FA
	// SCB->AIRCR |= 0x05FA0400;	// Set up group value
	NVIC_EnableIRQ(USART1_IRQn);
	NVIC_SetPriority(USART1_IRQn, 0b1001);

}

void USART1_IRQHandler(void) {
	if(USART1->SR & USART_SR_RXNE) {
		const char cmd = USART1->DR;	// 读取串口接收寄存器来清除 RXNE 标志
		switch (cmd) {
			case 0x0D:	//回车键
			case 0x0A:
				uart_sendStr("\n\r当前命令:\t");
				uart_sendStr(gCmdCache);
				UART_CR();
				uart_decode(gCmdCache);
				clrCache();
				break;
			case 0x08:	//退格键
			case 0x7F:
				if(top>=0) {		// 防止指针跨域操作, 否则多次按退格键会出现bug
					pop = '\0';
					uart_sendData(0x7F);
					uart_sendData(0x08);
				}
				break;
			case TOKEN_START:	//$ - 命令起始标志
				clrCache();
			default:	//其它按键
				if(STACK_OVERFLOW)	//如果指令缓存将要溢出, 则不会入栈当前字符
					break;
				push(cmd);			//保存当前字符
				uart_sendData(cmd);	//在终端回显, 反馈用户输入的字符
				break;
		}
	}
}

void uart_decode(char *token) {
	if(*token == 0)
		return;	//如果发生越界则结束 decode
	if(ISLEGAL_NUM(*token)) {	//如果当前操作符为效数字

		uart_sendStr(" - ");
		uart_sendData(*token);	//在终端显示当前处理的学码电路通道号

		if(gCmdCache[TOKEN_OFFSET] == TOKEN_LEARN) {		// 如果这条指令是学码命令
			*g_IrDA_Device[*token - '1'].IrInterrup ^= 1;	// (Toggle between enable and disable)如果该路学码中断是关闭的则使能, 反之则关闭
			uart_sendStr("号:学码");
			uart_sendStr(*g_IrDA_Device[*token - '1'].IrInterrup?"开启\n\r":"关闭\n\r");
		}
		else if(gCmdCache[TOKEN_OFFSET] == TOKEN_SEND) {	// 如果这条指令是发码命令
			uart_sendStr("号:发码开启\n\r");
			// 在发码的过程中为了防止自发自收, 得确保所有的接收中断都是关闭的, 否则中断会干扰发送
			*g_IrDA_Device[0].IrInterrup =
				*g_IrDA_Device[1].IrInterrup =
				*g_IrDA_Device[2].IrInterrup =
				*g_IrDA_Device[3].IrInterrup =
				*g_IrDA_Device[4].IrInterrup =
				*g_IrDA_Device[5].IrInterrup =
				*g_IrDA_Device[6].IrInterrup =
				*g_IrDA_Device[7].IrInterrup = 0;
			irda_encode(&g_IrDA_Device[*token - '1']);	//发码
		}
	}
	uart_decode(++token);
}

void uart_sendData(unsigned char data) {
    USART1->DR = data;
    while((USART1->SR & 0x40) == 0);
}

void uart_sendStr(char *cmd) {
	while(*cmd)
		uart_sendData(*cmd++);
}

void uart_int2char(unsigned int k) {
	char cache[] = "0000000000";	// 最大值为 4294967295
	unsigned char i = 9;
	const unsigned int bit[] = {1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1};

	do {
		cache[i] += (char)(k / bit[i] % 10);
	} while(i--);
	uart_sendStr(cache);
}

void uart_short2char(unsigned short k) {
	char cache[] = "00000";	// 最大值为 4294967295
	unsigned char i = 4;
	const unsigned int bit[] = {10000, 1000, 100, 10, 1};

	do {
		cache[i] += (char)(k / bit[i] % 10);
	} while(i--);
	uart_sendStr(cache);
}

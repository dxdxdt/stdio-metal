#ifndef CH56X_STDIO_COMMON_H_
#define CH56X_STDIO_COMMON_H_
#include "ch56x_stdio.h"

#ifndef __BASE_TYPE__

#ifndef PUINT8V
typedef volatile unsigned char *PUINT8V;
#endif

#define R8_SAFE_ACCESS_SIG      (*((PUINT8V)0x40001000))
#define R8_CLK_PLL_DIV          (*((PUINT8V)0x40001008))
#define R8_CLK_CFG_CTRL         (*((PUINT8V)0x4000100A))
#define RB_CLK_SEL_PLL          0x02

#define R8_UART0_TFC            (*((PUINT8V)0x4000300B))
#define R8_UART1_TFC            (*((PUINT8V)0x4000340B))
#define R8_UART2_TFC            (*((PUINT8V)0x4000380B))
#define R8_UART3_TFC            (*((PUINT8V)0x40003C0B))
#define R8_UART0_RFC            (*((PUINT8V)0x4000300A))
#define R8_UART1_RFC            (*((PUINT8V)0x4000340A))
#define R8_UART2_RFC            (*((PUINT8V)0x4000380A))
#define R8_UART3_RFC            (*((PUINT8V)0x40003C0A))

#endif /* __BASE_TYPE__ */

enum sysfreq_config {
	SYSFREQ_15MHz  =  15,	//Power-on default
	SYSFREQ_30MHz  =  30,
	SYSFREQ_60MHz  =  60,
	SYSFREQ_80MHz  =  80,
	SYSFREQ_96MHz  =  96,
	SYSFREQ_120MHz = 120,
};

static inline void SystemInit(const enum sysfreq_config sc)
{
	// enable safe access mode
	R8_SAFE_ACCESS_SIG = 0x57;
	R8_SAFE_ACCESS_SIG = 0xa8;

	switch(sc) {
	case SYSFREQ_15MHz:
		R8_CLK_PLL_DIV = 0x40 | 0x02;
		R8_CLK_CFG_CTRL = 0x80 ;
		break;
	case SYSFREQ_30MHz:
		R8_CLK_PLL_DIV = 0x40;
		R8_CLK_CFG_CTRL = 0x80 | RB_CLK_SEL_PLL;
		break;
	case SYSFREQ_60MHz:
		R8_CLK_PLL_DIV = 0x40 | 0x08;
		R8_CLK_CFG_CTRL = 0x80 | RB_CLK_SEL_PLL;
		break;
	case SYSFREQ_80MHz:
		R8_CLK_PLL_DIV = 0x40 | 0x06;
		R8_CLK_CFG_CTRL = 0x80 | RB_CLK_SEL_PLL;
		break;
	case SYSFREQ_96MHz:
		R8_CLK_PLL_DIV = 0x40 | 0x05;
		R8_CLK_CFG_CTRL = 0x80 | RB_CLK_SEL_PLL;
		break;
	case SYSFREQ_120MHz:
		R8_CLK_PLL_DIV = 0x40 | 0x04;
		R8_CLK_CFG_CTRL = 0x80 | RB_CLK_SEL_PLL;
		break;
	default :
		break;
	}

	R8_SAFE_ACCESS_SIG = 0;
}

static uint8_t p_us = 0;
static uint16_t p_ms = 0;

typedef struct __attribute__((packed))
{
	volatile uint32_t CTLR;
	volatile uint64_t CNT;
	volatile uint64_t CMP;
	volatile uint32_t CNTFG;
} SysTick_Type;
#define SysTick         ((SysTick_Type *) 0xE000F000)

static inline void Delay_Init(uint32_t systemclck)
{
	p_us = systemclck / 8000000;
	p_ms = (uint16_t)p_us * 1000;
}

static inline void mDelayuS(uint32_t n)
{
	uint32_t i;

	SysTick->CNTFG &= ~(1 << 1);

	i = (uint32_t)n * p_us;

	SysTick->CMP = i;
	SysTick->CTLR = (1 << 8) | (1 << 0);

	while ((SysTick->CNTFG & (1 << 1)) != (1 << 1));
	SysTick->CTLR = 0;
}

#ifndef STDIO_UART
#define STDIO_UART 1
#endif

#endif /* CH56X_STDIO_COMMON_H_ */

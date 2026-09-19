// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
#include "ch56x_stdio.h"

typedef volatile unsigned char *PUINT8V;
typedef volatile unsigned short *PUINT16V;
typedef volatile unsigned long *PUINT32V;

#ifndef __BASE_TYPE__

#define R8_UART0_DIV            (*((PUINT8V)0x4000300E))
#define R8_UART1_DIV            (*((PUINT8V)0x4000340E))
#define R8_UART2_DIV            (*((PUINT8V)0x4000380E))
#define R8_UART3_DIV            (*((PUINT8V)0x40003C0E))
#define R16_UART0_DL            (*((PUINT16V)0x4000300C))
#define R16_UART1_DL            (*((PUINT16V)0x4000340C))
#define R16_UART2_DL            (*((PUINT16V)0x4000380C))
#define R16_UART3_DL            (*((PUINT16V)0x40003C0C))
#define R8_UART0_FCR            (*((PUINT8V)0x40003002))
#define R8_UART1_FCR            (*((PUINT8V)0x40003402))
#define R8_UART2_FCR            (*((PUINT8V)0x40003802))
#define R8_UART3_FCR            (*((PUINT8V)0x40003C02))
#define R8_UART0_LCR            (*((PUINT8V)0x40003003))
#define R8_UART1_LCR            (*((PUINT8V)0x40003403))
#define R8_UART2_LCR            (*((PUINT8V)0x40003803))
#define R8_UART3_LCR            (*((PUINT8V)0x40003C03))
#define R8_UART0_IER            (*((PUINT8V)0x40003001))
#define R8_UART1_IER            (*((PUINT8V)0x40003401))
#define R8_UART2_IER            (*((PUINT8V)0x40003801))
#define R8_UART3_IER            (*((PUINT8V)0x40003C01))
#define R8_UART0_RBR            (*((PUINT8V)0x40003008))
#define R8_UART1_RBR            (*((PUINT8V)0x40003408))
#define R8_UART2_RBR            (*((PUINT8V)0x40003808))
#define R8_UART3_RBR            (*((PUINT8V)0x40003C08))
#define R8_UART0_THR            (*((PUINT8V)0x40003008))
#define R8_UART1_THR            (*((PUINT8V)0x40003408))
#define R8_UART2_THR            (*((PUINT8V)0x40003808))
#define R8_UART3_THR            (*((PUINT8V)0x40003C08))
#define R8_UART0_RFC            (*((PUINT8V)0x4000300A))
#define R8_UART1_RFC            (*((PUINT8V)0x4000340A))
#define R8_UART2_RFC            (*((PUINT8V)0x4000380A))
#define R8_UART3_RFC            (*((PUINT8V)0x40003C0A))
#define R8_UART0_TFC            (*((PUINT8V)0x4000300B))
#define R8_UART1_TFC            (*((PUINT8V)0x4000340B))
#define R8_UART2_TFC            (*((PUINT8V)0x4000380B))
#define R8_UART3_TFC            (*((PUINT8V)0x40003C0B))
#define R8_PIN_ALTERNATE        (*((PUINT8V)0x40001012))
#define RB_PIN_UART0            0x10
#define UART_FIFO_SIZE          8
#define RB_FCR_FIFO_TRIG        0xC0
#define RB_FCR_TX_FIFO_CLR      0x04
#define RB_FCR_RX_FIFO_CLR      0x02
#define RB_FCR_FIFO_EN          0x01
#define RB_LCR_WORD_SZ          0x03
#define RB_IER_TXD_EN           0x40
#define R32_PA_SMT              (*((PUINT32V)0x4000105C))
#define R32_PA_DIR              (*((PUINT32V)0x40001040))
#define R32_PB_SMT              (*((PUINT32V)0x4000107C))
#define R32_PB_DIR              (*((PUINT32V)0x40001060))

#endif /* __BASE_TYPE__ */

#define CH56X_STDIO_PORT_OUT    (0x80000000)
#define CH56X_STDIO_PORT_MASK   (0x7FFFFFFF)

#ifndef MEASURE_SP
#define MEASURE_SP 0
#endif

struct __file __iob[3];

#if MEASURE_SP
static uintptr_t min_sp = UINTPTR_MAX;
#endif

static void record_min_sp(void)
{
#if MEASURE_SP
	register uintptr_t sp __asm__("sp");

	if (sp < min_sp)
		min_sp = sp;
#endif
}

size_t ch56x_stdio_minstack(void)
{
#if MEASURE_SP
	return min_sp;
#else
	return 0;
#endif
}

static void ch56x_stdio_write(void *pv, char c)
{
	PUINT8V flen;
	PUINT8V fdata;

	if (!((unsigned long)pv & CH56X_STDIO_PORT_OUT))
		return;

	switch ((unsigned long)pv & CH56X_STDIO_PORT_MASK) {
	case 0:
		flen = &R8_UART0_TFC;
		fdata = &R8_UART0_THR;
		break;
	case 1:
		flen = &R8_UART1_TFC;
		fdata = &R8_UART1_THR;
		break;
	case 2:
		flen = &R8_UART2_TFC;
		fdata = &R8_UART2_THR;
		break;
	case 3:
		flen = &R8_UART3_TFC;
		fdata = &R8_UART3_THR;
		break;
	default:
		return;
	}

	while (*flen == UART_FIFO_SIZE);

	*fdata = (uint8_t)c;

	record_min_sp();
}

static int ch56x_stdio_read(void *pv)
{
	PUINT8V flen;
	PUINT8V fdata;

	if ((unsigned long)pv & CH56X_STDIO_PORT_OUT)
		return EOF;

	switch ((unsigned long)pv & CH56X_STDIO_PORT_MASK) {
	case 0:
		flen = &R8_UART0_RFC;
		fdata = &R8_UART0_RBR;
		break;
	case 1:
		flen = &R8_UART1_RFC;
		fdata = &R8_UART1_RBR;
		break;
	case 2:
		flen = &R8_UART2_RFC;
		fdata = &R8_UART2_RBR;
		break;
	case 3:
		flen = &R8_UART3_RFC;
		fdata = &R8_UART3_RBR;
		break;
	default:
		return EOF;
	}

	while (*flen == 0);

	record_min_sp();

	return *fdata;
}

static int ch56x_stdio_flush(void *pv)
{
	PUINT8V flen;
	PUINT8V fdata;

	if ((unsigned long)pv & CH56X_STDIO_PORT_OUT) {
		switch ((unsigned long)pv & CH56X_STDIO_PORT_MASK) {
		case 0:
			flen = &R8_UART0_TFC;
			fdata = &R8_UART0_THR;
			break;
		case 1:
			flen = &R8_UART1_TFC;
			fdata = &R8_UART1_THR;
			break;
		case 2:
			flen = &R8_UART2_TFC;
			fdata = &R8_UART2_THR;
			break;
		case 3:
			flen = &R8_UART3_TFC;
			fdata = &R8_UART3_THR;
			break;
		default:
			return EOF;
		}

		while (*flen > 0);

		record_min_sp();
	} else {
		volatile uint8_t b;

		switch ((unsigned long)pv & CH56X_STDIO_PORT_MASK) {
		case 0:
			flen = &R8_UART0_RFC;
			fdata = &R8_UART0_RBR;
			break;
		case 1:
			flen = &R8_UART1_RFC;
			fdata = &R8_UART1_RBR;
			break;
		case 2:
			flen = &R8_UART2_RFC;
			fdata = &R8_UART2_RBR;
			break;
		case 3:
			flen = &R8_UART3_RFC;
			fdata = &R8_UART3_RBR;
			break;
		default:
			return EOF;
		}

		while (*flen > 0)
			b = *fdata;
		(void)b; /* stfu */

		record_min_sp();
	}

	return 0;
}

#define FCR_MAGIC (RB_FCR_TX_FIFO_CLR | RB_FCR_RX_FIFO_CLR | RB_FCR_FIFO_EN)

__attribute__((noinline))
static void set_reg_flag(const bool flag, PUINT32V reg, const uint32_t b1, const uint32_t b0)
{
	if (flag)
		*reg |=  ((uint32_t)1 << b1);
	else
		*reg &= ~((uint32_t)1 << b0);
}

__attribute__((noinline))
static void set_smt_flag(const bool dir, const bool cond, const bool flag, PUINT32V reg,
		const uint32_t b_in, const uint32_t b_out)
{
	if (!cond)
		return;
	if (dir)
		set_reg_flag(flag, reg, b_out, b_out);
	else
		set_reg_flag(flag, reg, b_in, b_in);
}

void ch56x_stdio_open(const struct ch56x_stdio_desc desc[3], uint32_t sys_freq)
{
	unsigned long port;
	uint32_t x;
	bool is_out;

	for (size_t i = 0; i < 3; i++) {
		if (desc[i].baudrate == 0) {
			port = LONG_MAX;
			goto cont;
		}

		x = 10 * sys_freq * 2 / 16 / desc[i].baudrate;
		x = (x + 5) / 10;
		port = (unsigned long)desc[i].uart_port;
		is_out = i > 0;

		switch (desc[i].uart_port) {
		case -1:
			R8_PIN_ALTERNATE |= RB_PIN_UART0;
			R8_UART0_DIV = 1;
			R16_UART0_DL = x;
			R8_UART0_FCR = FCR_MAGIC;
			R8_UART0_LCR = RB_LCR_WORD_SZ;
			if (is_out)
				R8_UART0_IER = RB_IER_TXD_EN;
			set_reg_flag(is_out, &R32_PA_DIR, 6, 5);
			set_smt_flag(is_out, desc[i].smt_set, desc[i].smt_flag, &R32_PA_SMT, 6, 5);
			port = 0;
			break;
		case 0:
			R8_PIN_ALTERNATE &= ~((uint8_t)RB_PIN_UART0);
			R8_UART0_DIV = 1;
			R16_UART0_DL = x;
			R8_UART0_FCR = FCR_MAGIC;
			R8_UART0_LCR = RB_LCR_WORD_SZ;
			if (is_out)
				R8_UART0_IER = RB_IER_TXD_EN;
			set_reg_flag(is_out, &R32_PB_DIR, 6, 5);
			set_smt_flag(is_out, desc[i].smt_set, desc[i].smt_flag, &R32_PB_SMT, 6, 5);
			break;
		case 1:
			R8_UART1_DIV = 1;
			R16_UART1_DL = x;
			R8_UART1_FCR = FCR_MAGIC;
			R8_UART1_LCR = RB_LCR_WORD_SZ;
			if (is_out)
				R8_UART1_IER = RB_IER_TXD_EN;
			set_reg_flag(is_out, &R32_PA_DIR, 8, 7);
			set_smt_flag(is_out, desc[i].smt_set, desc[i].smt_flag, &R32_PA_SMT, 8, 7);
			break;
		case 2:
			R8_UART2_DIV = 1;
			R16_UART2_DL = x;
			R8_UART2_FCR = FCR_MAGIC;
			R8_UART2_LCR = RB_LCR_WORD_SZ;
			if (is_out)
				R8_UART2_IER = RB_IER_TXD_EN;
			set_reg_flag(is_out, &R32_PA_DIR, 3, 2);
			set_smt_flag(is_out, desc[i].smt_set, desc[i].smt_flag, &R32_PA_SMT, 3, 2);
			break;
		case 3:
			R8_UART3_DIV = 1;
			R16_UART3_DL = x;
			R8_UART3_FCR = FCR_MAGIC;
			R8_UART3_LCR = RB_LCR_WORD_SZ;
			if (is_out)
				R8_UART3_IER = RB_IER_TXD_EN;
			set_reg_flag(is_out, &R32_PB_DIR, 4, 3);
			set_smt_flag(is_out, desc[i].smt_set, desc[i].smt_flag, &R32_PB_SMT, 4, 3);
			break;
		default:
			port = LONG_MAX;
			goto cont;
		}

		if (is_out)
			port |= CH56X_STDIO_PORT_OUT;
cont:
		fdev_setup_stream(__iob + i,
				  ch56x_stdio_write,
				  ch56x_stdio_read,
				  ch56x_stdio_flush,
				  _FDEV_SETUP_RW,
				  (void*)port);
	}
}

void ch56x_stdio_close(void)
{
	for (size_t i = 0; i < 3; i++)
		__iob[i].udata = (void*)LONG_MAX;
}

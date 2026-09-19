// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
/*
 * Benchmark stdio-metal on CH56x for processor-bound bottleneck
 *
 * Repeatedly print the size of the FIFO to see if the processor is
 * processing putchar() fast enough to fill up the FIFO. If it reads 0
 * intermittently, the processor is not running fast enough to process
 * putchar(). Otherwise, the processor is fast enough.
 *
 * Note that there are two factors, baudrate and the speed of the
 * processor, that affect the rate at which the FIFO flushes bytes in
 * the queue. The program defaults to the baudrate of 115200 and the
 * system frequency of 15MHz(the slowest sysfreq possible).
 */
#include "ch56x_stdio-common.h"

#ifndef STDIO_METAL_SYSFREQ
#define STDIO_METAL_SYSFREQ (SYSFREQ_15MHz)
#endif

int main(void)
{
	static const struct ch56x_stdio_desc stdio_desc[3] = {
		{ },
		{115200, STDIO_UART, 1, 1},
		{115200, STDIO_UART, 1, 1}
	};

	SystemInit(STDIO_METAL_SYSFREQ);
	ch56x_stdio_open(stdio_desc, STDIO_METAL_SYSFREQ * 1000000);

	fprintf(stderr, "Hello, world!\n");

	for (unsigned long i = 0; ; i++) {
		if (false) {
			/* Test fflush() implementations */
			fflush(stdout);
			fflush(stderr);
		}

		printf("%ld\t%2d %2d %2d %2d\n",
				i, R8_UART0_TFC, R8_UART1_TFC, R8_UART2_TFC, R8_UART3_TFC);
		fprintf(stderr, "---\n");
	}

	for (;;);
}

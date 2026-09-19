// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
/*
 * Measure stack usage of stdio-metal
 */

#include "ch56x_stdio-common.h"

#ifndef STDIO_METAL_SYSFREQ
#define STDIO_METAL_SYSFREQ (SYSFREQ_80MHz)
#endif

char str_a[64];
char str_b[64];

__attribute__((noinline))
static void inner(void)
{
	static const char *MSG = "Aquickbrownfoxjumpsoveralazydoggo.";
	uintptr_t min;
	register uintptr_t sp __asm__("sp");
	unsigned long a;
	unsigned int b;
	char c;

	/* Use stdio */

	printf("%x %u %c %s\n", 0xFFFFFFFF, 1, 'x', MSG);
	snprintf(str_a, sizeof(str_a), "%x %u %c %s\n", 0xFFFFFFFF, 1, 'x', MSG);
	sscanf(str_a, "%x %u %c %63s", &a, &b, &c, str_b);
	printf("%x %u %c %s\n", a, b, c, str_b);
	printf("\n==========\n");

	min = ch56x_stdio_minstack();
	fprintf(stderr, "stack usage: %x - %x\n", sp, min);

	fflush(stdout);
	fflush(stderr);
	mDelayuS(100);
}

int main(void)
{
	static const struct ch56x_stdio_desc stdio_desc[3] = {
		{ },
		{115200, STDIO_UART, 1, 1},
		{115200, STDIO_UART, 1, 1}
	};

	SystemInit(STDIO_METAL_SYSFREQ);
	Delay_Init(STDIO_METAL_SYSFREQ * 1000000);
	ch56x_stdio_open(stdio_desc, STDIO_METAL_SYSFREQ * 1000000);

	printf("\n");

	inner();

	SystemInit(SYSFREQ_15MHz);
	for(;;);
}

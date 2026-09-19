// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
/*
 * Simple demonstration of stdio-metal on CH56x
 */

 #include "ch56x_stdio-common.h"

#ifndef STDIO_METAL_SYSFREQ
#define STDIO_METAL_SYSFREQ (SYSFREQ_80MHz)
#endif

int main(void)
{
	static const struct ch56x_stdio_desc stdio_desc[3] = {
		{115200, STDIO_UART, 1, 1},
		{115200, STDIO_UART, 1, 1},
		{115200, STDIO_UART, 1, 1}
	};
	char linebuf[256];
	char op;
	int a, b, c;
	bool empty, ovf;

start:
	SystemInit(STDIO_METAL_SYSFREQ);
	Delay_Init(STDIO_METAL_SYSFREQ * 1000000);
	ch56x_stdio_open(stdio_desc, STDIO_METAL_SYSFREQ * 1000000);

	printf("\nI wake up 😫🔦\n");
	fflush(stdout);

	for(;;) {
		fprintf(stderr, "A ⨁ B > ");
		fflush(stderr);

		for (int i = 0; i < 10; i++)
			for (uint32_t i = 0; R8_UART1_RFC == 0 && i < STDIO_METAL_SYSFREQ * 1000000; i++);
		if (R8_UART1_RFC == 0)
			break;

		if (fgets(linebuf, sizeof(linebuf), stdin) == NULL)
			break;

		empty = true;
		for (char *ptr = linebuf; *ptr; ptr++) {
			if (!isspace(*ptr)) {
				empty = false;
				break;
			}
		}
		if (empty)
			continue;

		if (sscanf(linebuf, "%d %c %d", &a, &op, &b) != 3)
			goto inval;

		switch (op) {
		case '+': ovf = __builtin_add_overflow(a, b, &c); break;
		case '-': ovf = __builtin_sub_overflow(a, b, &c); break;
		case '*': ovf = __builtin_mul_overflow(a, b, &c); break;
		/* Division by zero on RISC-V is not an exception!! */
		case '/':
			c = a / b;
			ovf = false;
			break;
		default:
			goto inval;
		}

		printf("%d %c %d = %d%s\n", a, op, b, c, ovf ? " (OVERFLOW)" : "");

		continue;
inval:
		fprintf(stderr, "** invalid input **\n");
	}

	printf("\nI sleep   🗿\n");

	fflush(stdout);
	fflush(stderr);
	mDelayuS(100);

	ch56x_stdio_open(stdio_desc, SYSFREQ_15MHz * 1000000);
	while (R8_UART1_RFC == 0);

	goto start;
}

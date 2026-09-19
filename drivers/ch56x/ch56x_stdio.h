// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
#ifndef CH56X_STDIO_H_
#define CH56X_STDIO_H_
#include "stdio-metal.h"
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

struct ch56x_stdio_desc {
	/*
	 * Baudrate
	 *
	 * If set to zero, the corresponding stdio FILE object is
	 * disabled. Usually set to 115200.
	 */
	uint32_t baudrate;
	/*
	 * -1: use alternate UART0 pin(TXD0_(PA6)+RXD0_(PA5))
	 *  0: UART0 (TXD0(PB6)+RXD0(PB5))
	 *  1: UART1 (TXD1(PA8)+RXD1(PA7))
	 *  2: UART2 (TXD2(PA3)+RXD2(PA2))
	 *  3: UART3 (TXD3(PB4)+RXD3(PB3))
	 */
	int16_t uart_port;
	/*
	 * If set, configure the SMT registers as specified with
	 * `smt_flag`. Otherwise, ignore it and leave the register as
	 * is.
	 */
	bool smt_set:1;
	/*
	 * To set up the SMT registers of UART pins, `smt_set` needs to
	 * be set. Otherwise, the value is ignored and the registers
	 * will be left as is.
	 *
	 * If the flag is set, if the port is being configured for
	 * output(stdout and stderr), enable slow slew rate. If the port
	 * is being configured for input(stdin), enable Schmitt trigger
	 * input. If cleared, disable slow slew rate or Schmitt trigger
	 * input.
	 */
	bool smt_flag:1;
};

/*
 * Initialise the UART pins of the CH56x chip and the stdio-metal
 * library for C standard input and output
 *
 * `desc` must be an array of `struct ch56x_stdio_desc` in size 3
 * elements or more. Each element represents stdin(0), stdout(1) and
 * stderr(2). There is no restriction as to which UART ports are
 * configured for which stdio. For example,
 *
 *  - `{{115200, 1, 1, 1}, {115200, 1, 1, 1}, {115200, 1, 1, 1}}`: use
 *    UART1 for all stdio (the usual set up)
 *  - `{{115200, 1, 1, 1}, {115200, 1, 1, 1}, {115200, 1, 1, 1}}`:
 *    redirect stdin and stdout to UART0 and use UART1 for stderr
 *  - `{ }, {115200, 1, 1, 1}, {115200, 1, 1, 1}}`: use output only
 *
 * Set `sys_freq` to the system frequency in Hz.
 */
void ch56x_stdio_open(const struct ch56x_stdio_desc desc[3], uint32_t sys_freq);

/*
 * Disable the stdio set up of UART pins initialised in ch56x_open()
 *
 * The function merely prevents further use of stdio FILE objects. It is
 * not necessary to call the function before re-calling ch56x_open() to
 * reconfigure the settings.
 */
void ch56x_stdio_close(void);

size_t ch56x_stdio_minstack(void);

#endif

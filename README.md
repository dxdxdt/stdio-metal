# STDIO-METAL🤘: ultra-lightweight stdio.h implementation for microcontrollers

This library is a stripped-down C standard I/O(stdio.h) suitable for
microcontrollers with direct access to specialised dedicated
hardware(UART FIFO). It's been conceived as an alternative to general
purpose libc implementations for microcontrollers like Newlib and
Picolibc to workaround the problems outlined in [Design
Decisions](#design-decisions) below.

  1. Forked off https://github.com/TimPaterson/stdio-mini/ in September 2026
  2. stdio-mini forked off https://github.com/stevenj/avr-libc3 in April 2020

## Design Decisions

### No libc functions

stdio-metal is self-sufficient. It's implemented in C and has no
external dependency.

All of the common `<stdio.h>` implementations ship filesystem-related
leftover cruft like `open()` and `close()`. It really doesn't make sense
in having such functions since there's no filesystem on
microcontrollers. Also, stdio-metal works entirely in stack memory. No
`sbrk()` required. Microcontrollers usually feature specialised hardware
that handles asynchronous buffering which are called FIFOs. They are
just as fast as `memcpy()` equivalent. No need to sacrifice memory for
overhead from `sbrk()`.

With emerging arch like RISC-V, use of the libc functions mentioned
above may require the use of `ecall` for which a trap handler is
required in order to implement the syscalls. Doing such expensive
operations on a system running in M-mode makes absolutely no sense.

### Just a few source files to drop in

The project only consists of two output files:

  1. stdio-metal.o(`stdio-metal.h` and `stdio-metal.c` pair)
  2. the driver for your microcontroller

so you can integrate the library to your existing build system with
ease.

### GCC/Clang and C only

For simplicity, the library is written to only compiles with GCC/Clang.
The toolchains plagued with atrocities like MSVC are not supported. They
shouldn't be used for microcontrollers in the first place.

The library does not care about the compatibility with C++. Any
integration issues with C++ must be dealt with by the user. C++
shouldn't be used for microcontrollers in the first place.

### Optional floating-point support

For targets without dedicated FPU and floating-point instructions, if
supported by the toolchain, the compiler generates code to emulate
floating-point instructions with software("soft-float"). Almost no
microcontrollers are equipped with FPU because there isn't really a use
case for it. If, however, you decided to use floating point arithmetics
in the microcontoller code, the compiler will silently add code
necessary for emulating floating point arithmetics in software and that
bloats the size of the program significantly.

To prevent this, the library doesn't build with the floating point
support by default. To build the library with the floating point
support, see [Format Specifier Support
Configuration](#format-specifier-support-configuration).

## Functions Provided

The following standard C runtime functions are included. This is
basically the normal set except for functions to open,
close, or position a file.

| formatted out | formatted in | simple out | simple in | extras    |
|---------------|--------------|------------|-----------|-----------|
| fprintf()     | fscanf()     | fputc()    | fgetc()   | fflush()  |
| printf()      | scanf()      | fputs()    | fgets()   | strtod()  |
| snprintf()    | sscanf()     | fwrite()   | fread()   | strtof()  |
| sprintf()     | vfscanf()    | putc()     | getc()    | atof()    |
| vfprintf()    | vscanf()     | putchar()  | getchar() | tolower() |
| vprintf()     | vsscanf()    | puts()     | gets()    | isspace() |
| vsnprintf()   |              |            | ungetc()  | isdigit() |
| vsprintf()    |              |            |           |           |

To leverage the conversions from string to floating-point required by
the `scanf` family, implementations of `strtod`, `strtof`, `atof`,
`tolower()`, `isspace()` and `isdigit()` are also provided.

### Writing a A New Driver

```c
extern struct __file __iob[3];

typedef void _fdev_put_t(void *, char);
typedef int  _fdev_get_t(void *);
typedef int  _fdev_flush_t(void *);

struct __file {
	union {
		struct {
			_fdev_put_t *put;	/* function to write one char to device */
			_fdev_get_t	*get;	/* function to read one char from device */
			/*
			 * Output: function to wait for FIFO to be empty
			 * Input: function to clear FIFO
			 */
			_fdev_flush_t *flush;
		} dev;
// ...
	} u;
	void		*udata;			/* User defined and accessible data. */
	uint8_t		flags;			/* flags, see below */
// ...
};

#define fdev_setup_stream(stream, __p, __g, __flush, __flags, __ud) \
	do { \
		(stream)->u.dev.put = (__p); \
		(stream)->u.dev.get = (__g); \
		(stream)->u.dev.flush = (__flush); \
		(stream)->flags = (__flags); \
		(stream)->udata = (__ud); \
	} while(0)
```

The `void *` user data will be passed to the callback functions to
process I/O in and out of `stdin`, `stdout` and `stderr`. In the
functions, write hardware-specific code for writing to and reading from
the hardware. In `get()`, pop a byte from the FIFO and return it. In
`put()`, wait until there's free space in the FIFO and push the byte. In
`flush()`, if the corresponding file object is `stdin`, pop from the
FIFO until there's no more data to pop. If the corresponding file object
is `stdout` or `stderr`, wait until the FIFO becomes empty(ie. all the
bytes in the FIFO have been sent and cleared).

To achieve this, the user data should generally contain:

  1. flag to determine whether the file object is for either reading or
     writing
  2. information about the target serial port or hardware
  3. (maybe)other state data for the hardware

Generally, you should write an out-of-band initialiser function for
initialising hardware and the file objects. The function must be called
before using any of the stdio-metal functions. The general flow would
be:

```c
general_system_init();
/* ... */
stdio_metal_driver_init();
/* ... */
printf("hello, world!\n");
/* ... */
```

### Format Specifier Support Configuration

This library can be compiled into several different configurations
depending on the level of type specifier support. The minimum is the
support for integers up to size the `%ld`(long) without floating-point.
The maximum is the support for `%lld` and both `%f` and `%lf`.

The support can be configured separately for integer and floating-point
specifiers with the following macro definitions:

```c
// Values for INT_MATH_LEVEL
#define INT_MATH_MIN		0 /* Support up to "%ld" (default) */
#define INT_MATH_LONG_LONG	1 /* "%lld" support */

// Values for FP_MATH_LEVEL
#define FP_MATH_NONE		0 /* No floating-point support (default) */
#define FP_MATH_FLT		1 /* "%f" support */
#define FP_MATH_DBL		2 /* "%ld" support */
```

It is expected that the values for `INT_MATH_LEVEL` and `FP_MATH_LEVEL`
would be set with the compiler options or the build system.

The `*printf()` family all use variadic arguments, which means that
`float` is promoted to `double`. The library supports a non-stardard
hack in which only `float` support is present. In order to pass a
`float` without promoting it to `double`, the bits are passed in a
32-bit integer. This can be done using a union of `float` and `int32_t`.
The compiler may produce a warning that the type passed does not match
the type specified by the format string.

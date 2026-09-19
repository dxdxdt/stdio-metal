// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2002, 2005, 2007 Joerg Wunsch
 *
 * Copyright (c) 2023, 2024 Tim Paterson
 *
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
#ifndef _STDIO_METAL_H_
#define _STDIO_METAL_H_ 1

#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This is an internal structure of the library that is subject to be
 * changed without warnings at any time.  Please do *never* reference
 * elements of it beyond by using the official interfaces provided.
 */
typedef void _fdev_put_t(void *, char);
typedef int  _fdev_get_t(void *);
typedef int  _fdev_flush_t(void *);

struct __file {
	union {
		struct {
			_fdev_put_t *put;		/* function to write one char to device */
			_fdev_get_t	*get;		/* function to read one char from device */
			/*
			 * Output: function to wait for FIFO to be empty
			 * Input: function to clear FIFO
			 */
			_fdev_flush_t *flush;
		} dev;
		struct {
			char		*buf;		/* buffer pointer */
			int			size;		/* size of buffer */
		} mem;
	} u;
	void		*udata;		/* User defined and accessible data. */
	int			len;		/* characters read or written so far */
	uint8_t		flags;		/* flags, see below */
	unsigned char unget;	/* ungetc() buffer */
#define __SRD	0x0001		/* OK to read */
#define __SWR	0x0002		/* OK to write */
#define __SSTR	0x0004		/* this is an sprintf/snprintf string */
#define __SCRLF	0x0008		/* add CR before each LF */
#define __SERR	0x0010		/* found error */
#define __SEOF	0x0020		/* found EOF */
#define __SUNGET 0x040		/* ungetc() happened */
};

typedef struct __file FILE;
#define __FILE_defined

#define stdin	(__iob + 0)
#define stdout	(__iob + 1)
#define stderr	(__iob + 2)

#define EOF	(-1)

/** This macro inserts a pointer to user defined data into a FILE
    stream object.

    The user data can be useful for tracking state in the put and get
    functions of the FILE. */
#define fdev_set_udata(stream, u) do { (stream)->udata = u; } while(0)

/** This macro retrieves a pointer to user defined data from a FILE
    stream object. */
#define fdev_get_udata(stream) ((stream)->udata)

#define fdev_setup_stream(stream, __p, __g, __flush, __flags, __ud) \
	do { \
		(stream)->u.dev.put = (__p); \
		(stream)->u.dev.get = (__g); \
		(stream)->u.dev.flush = (__flush); \
		(stream)->flags = (__flags); \
		(stream)->udata = (__ud); \
	} while(0)

#define _FDEV_SETUP_READ  __SRD	/**< fdev_setup_stream() with read intent */
#define _FDEV_SETUP_WRITE __SWR	/**< fdev_setup_stream() with write intent */
#define _FDEV_SETUP_RW    (__SRD|__SWR)	/**< fdev_setup_stream() with read/write intent */
#define _FDEV_SETUP_CRLF  __SCRLF /**< fdev_setup_stream() with LF converted to CR/LF */

/**
 * Return code for an error condition during device read.
 *
 * To be used in the get function of fdevopen().
 */
#define _FDEV_ERR (-1)

/**
 * Return code for an end-of-file condition during device read.
 *
 * To be used in the get function of fdevopen().
 */
#define _FDEV_EOF (-2)

extern struct __file __iob[3];

int metal_vfprintf(FILE *__stream, const char *__fmt, va_list __ap);
int metal_fputc(int __c, FILE *__stream);
#define putc(__c, __stream) metal_fputc(__c, __stream)
#define putchar(__c) metal_fputc(__c, stdout)
int metal_printf(const char *__fmt, ...);

static inline int vprintf(const char *fmt, va_list ap)
{
	return metal_vfprintf(stdout, fmt, ap);
}

/* Don't get bullied by the C standard specs */
#define sprintf(__s, __fmt) static_assert(false)
int metal_snprintf(char *__s, size_t __n, const char *__fmt, ...);
int metal_vsprintf(char *__s, const char *__fmt, va_list ap);
int metal_vsnprintf(char *__s, size_t __n, const char *__fmt, va_list ap);
int metal_fprintf(FILE *__stream, const char *__fmt, ...);
int metal_fputs(const char *__str, FILE *__stream);
int metal_puts(const char *__str);
size_t metal_fwrite(const void *__ptr, size_t __size, size_t __nmemb, FILE *__stream);
int metal_fgetc(FILE *__stream);
#define getc(__stream) metal_fgetc(__stream)
#define getchar() metal_fgetc(stdin)
int metal_ungetc(int __c, FILE *__stream);
char *metal_fgets(char *__str, int __size, FILE *__stream);
/* Don't get bullied by the C standard specs */
#define gets(__str) static_assert(false)
size_t metal_fread(void *__ptr, size_t __size, size_t __nmemb, FILE *__stream);

static inline void clearerr(FILE *stream)
{
	stream->flags &= ~(__SERR | __SEOF);
}

#define clearerror(s) clearerr(s)

static inline int feof(FILE *stream)
{
	return stream->flags & __SEOF;
}

static inline int ferror(FILE *stream)
{
	return stream->flags & __SERR;
}

int metal_vfscanf(FILE *__stream, const char *__fmt, va_list __ap);
int metal_fscanf(FILE *__stream, const char *__fmt, ...);
int metal_scanf(const char *__fmt, ...);

static inline int vscanf(const char *fmt, va_list ap)
{
	return metal_vfscanf(stdin, fmt, ap);
}

int metal_sscanf(const char *__buf, const char *__fmt, ...);
int metal_vsscanf(const char *__buf, const char *__fmt, va_list __ap);
int metal_fflush(FILE *stream);

// This library can be compiled with different levels of math support.
// The minimum is level is long int. The following levels can be added:
//
// long long; float/double not required
// float; long long not required (no double)
// double; requires long long (no float)
// both float & double (scanf family only; double for printf family)
//
// WARNING FOR FLOAT!!: Variadic functions like printf cannot be
// directly passed a float; the compiler will automatically promote it
// to double. Since the objective of using float is to avoid
// pulling in the presumably larger runtime library for double, this
// would defeat the purpose. Instructions for passing a float
// are in stdio.h.
//
// Because of the overlap with integer and floating point, two
// compile-time variables are used, with these possible values:

// Values for INT_MATH_LEVEL
#define INT_MATH_MIN		0
#define INT_MATH_LONG_LONG	1

// Values for FP_MATH_LEVEL
#define FP_MATH_NONE    0
#define FP_MATH_FLT     1
#define FP_MATH_DBL     2

// If not set on the compiler command line, set defaults here:
#ifndef INT_MATH_LEVEL
#define INT_MATH_LEVEL	INT_MATH_MIN
#endif

/* Default to no FP support */
#ifndef FP_MATH_LEVEL
#define FP_MATH_LEVEL	FP_MATH_NONE
#endif

// And double always includes long long
#if FP_MATH_LEVEL >= FP_MATH_DBL
#undef INT_MATH_LEVEL
#define INT_MATH_LEVEL	INT_MATH_LONG_LONG
#endif

/* Following functions normally declared in stdlib.h are included here for simplicity */

double metal_strtod(const char *psz, char **ppend);
float metal_strtof(const char *psz, char **ppend);

static inline double atof(const char *psz)
{
	return metal_strtod(psz, NULL);
}

/* Functions in ctype.h */

static inline int tolower(int c)
{
	if ('A' <= c && c <= 'Z')
		return c + ('a' - 'A');
	return c;
}

static inline int isspace(int c)
{
	switch (c) {
	case ' ':
	case '\f':
	case '\n':
	case '\r':
	case '\t':
	case '\v':
		return c;
	}

	return 0;
}

static inline int isdigit(int c)
{
	if ('0' <= c && c <= '9')
		return c;
	return 0;
}

#define vfprintf	metal_vfprintf
#define fputc		metal_fputc
#define printf		metal_printf
#define snprintf	metal_snprintf
#define vsprintf	metal_vsprintf
#define vsnprintf	metal_vsnprintf
#define fprintf		metal_fprintf
#define fputs		metal_fputs
#define puts		metal_puts
#define fwrite		metal_fwrite
#define fgetc		metal_fgetc
#define ungetc		metal_ungetc
#define fgets		metal_fgets
#define fread		metal_fread
#define vfscanf		metal_vfscanf
#define fscanf		metal_fscanf
#define scanf		metal_scanf
#define sscanf		metal_sscanf
#define vsscanf		metal_vsscanf
#define fflush		metal_fflush
#define strtod		metal_strtod
#define strtof		metal_strtof

#ifdef __cplusplus
}
#endif

#endif /* _STDIO_METAL_H_ */

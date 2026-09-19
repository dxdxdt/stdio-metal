// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2002, 2005, 2007 Joerg Wunsch
 *
 * Copyright (c) 2023, 2024 Tim Paterson
 *
 * Copyright (c) 2026 David Timber <dxdt@dev.snart.me>
 */
#include "stdio-metal.h"
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(__array) (sizeof(__array) / sizeof(__array[0]))
#endif

#if INT_MATH_LEVEL == INT_MATH_MIN || INT_MATH_LEVEL == INT_MATH_LONG_LONG
 /* OK */
#else
# error "Not a known integer math level."
#endif

#if FP_MATH_LEVEL == FP_MATH_NONE || FP_MATH_LEVEL == FP_MATH_FLT || \
	FP_MATH_LEVEL == FP_MATH_DBL || FP_MATH_LEVEL == FP_MATH_FLT_DBL
 /* OK */
#else
# error "Not a known floating-point math level."
#endif

#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
typedef unsigned long long val_t;
#else
typedef unsigned long val_t;
#endif

 /* --------------------------------------------------------------------	*/

#ifdef __arm__

uint64_t __aeabi_uidivmod(uint32_t numerator, uint32_t denominator);
uint64_t __aeabi_uldivmod(uint64_t numerator, uint64_t denominator);

#define UL_DIVMOD(val, rem, base) \
 	do { \
		unsigned long long ull = __aeabi_uidivmod(val, base); \
		val = ull; \
		rem = (int)(ull >> 32); \
	 } while(0)

#define ULL_DIVMOD(val, rem, base) \
 	do { \
		val = __aeabi_uldivmod(val, base); \
		asm volatile \
		(	"movs	%[r],r2\n\t" \
			: [r] "=r" (rem)); \
	 } while(0)

#else

#define DO_DIV(val, rem, base)		do { rem = (val % base); val /= base; } while(0)
#define UL_DIVMOD(val, rem, base)	DO_DIV(val, rem, base)
#define ULL_DIVMOD(val, rem, base)	DO_DIV(val, rem, base)

#endif

// "h" & "hh" means FL_SHORT is set if FL_CHAR is set, so test FL_CHAR first
// "l" & "ll" means FL_LONG is set if FL_LL is set, so test FL_LL first
#define FL_STAR	    0x01	/* '*': skip assignment		*/
#define FL_WIDTH    0x02	/* width is present		*/
#define FL_CHAR	    0x08	/* 'char' type modifier		*/
#define FL_OCT	    0x10	/* octal number			*/
#define FL_DEC	    0x20	/* decimal number		*/
#define FL_HEX	    0x40	/* hexadecimal number		*/
#define FL_MINUS    0x80	/* minus flag (field or value)	*/
#define FL_SHORT    0x100	// 'short' type modifier
#define FL_LL	    0x200	/* 'long long' type modifier	*/

// Flags as used in __conv_flt and __conv_dbl only
#define FL_ERR      0x01    // input not valid
#define FL_ANY	    0x02	/* any digit was readed	*/
#define FL_OVFL	    0x04	/* overflow was		*/
#define FL_DOT	    0x08	/* decimal '.' was	*/
#define FL_MEXP	    0x10	/* exponent 'e' is neg.	*/
#define FL_NAN      0x20    // "NAN"
#define FL_INF      0x40    // "INF" or "INFINITY"
#define FL_SIGN     0x100   // sign present, count in width

#define FL_ZFILL	0x01
#define FL_PLUS		0x02
#define FL_SPACE	0x04
#define FL_LPAD		0x08
#define FL_ALT		0x10
#define FL_PREC		0x20
#define FL_H		0x40
#define FL_NEGATIVE	0x200

#define FL_ALTUPP	FL_PLUS
#define FL_ALTHEX	FL_SPACE

#define	FL_FLTUPP	FL_ALT
#define FL_FLTEXP	FL_PREC
#define	FL_FLTFIX	FL_LONG

#define FL_LONG	    0x04	/* 'long' type modifier		*/

static int __begin_fp(FILE* stream, int width)
{
	static const char pstr_nfinity[] = "nfinity";
	static const char pstr_an[] = "an";
	const char* p;
	int flag;
	int ch;

	ch = getc(stream);

	flag = 0;
	switch (ch)
	{
	case '-':
		flag = FL_MINUS;
		/* FALLTHROUGH */
	case '+':
		flag |= FL_SIGN;
		if (!--width || (ch = getc(stream)) < 0)
			return FL_ERR;
	}

	switch (tolower(ch))
	{

	case 'n':
		p = pstr_an;
		goto operate_pstr;

	case 'i':
		p = pstr_nfinity;
operate_pstr:
		{
			unsigned char c;

			while ((c = *p++) != 0)
			{
				if (!--width
					|| (ch = getc(stream)) < 0
					|| ((unsigned char)tolower(ch) != c
						&& (ungetc(ch, stream), 1)))
				{
					if (p == pstr_nfinity + 3)	// allow "inf"
						break;
					return FL_ERR;
				}
			}
		}
		flag |= (p == pstr_an + 3) ? FL_NAN : FL_INF;
		break;

	default:
		ungetc(ch, stream);
	}
	return flag;
}

static int __skip_spaces(FILE* stream)
{
	int i;
	do
	{
		if ((i = getc(stream)) < 0)
			return i;
	} while (isspace(i));
	ungetc(i, stream);
	return i;
}

#define MIN_TABLE_POWER		-36
#define MAX_TABLE_POWER		38

static float __mulpower100f(float flt, int power)
{
	static const float Power100table[] = {
		1e-36f, 1e-34f, 1e-32f, 1e-30f, 1e-28f, 1e-26f, 1e-24f, 1e-22f,
		1e-20f, 1e-18f, 1e-16f, 1e-14f, 1e-12f, 1e-10f, 1e-8f, 1e-6f,
		1e-4f, 1e-2f, /* 1e+0 skipped */ 1e+2f, 1e+4f, 1e+6f, 1e+8f,
		1e+10f, 1e+12f, 1e+14f, 1e+16f, 1e+18f, 1e+20f, 1e+22f, 1e+24f,
		1e+26f, 1e+28f, 1e+30f, 1e+32f, 1e+34f, 1e+36f, 1e+38f
	};

	if (power != 0)	// skip 100^0
	{
		while (power < MIN_TABLE_POWER/2)
		{
			flt *= Power100table[0];
			power -= MIN_TABLE_POWER/2;
		}

		while (power > MAX_TABLE_POWER/2)
		{
			flt *= Power100table[ARRAY_SIZE(Power100table) - 1];
			power -= MAX_TABLE_POWER/2;
		}

		flt *= Power100table[power - MIN_TABLE_POWER / 2 + (power < 0 ? 0 : -1)];
	}
	return flt;
}

#undef MIN_TABLE_POWER
#undef MAX_TABLE_POWER

#define SMALL_POWER_BITS	4
#define SMALL_POWER_MASK	((1 << SMALL_POWER_BITS) - 1)
#define MIN_TABLE_POWER		-288
#define MAX_TABLE_POWER		288
#define LARGE_POWER_BASE	((MAX_TABLE_POWER >> SMALL_POWER_BITS) / 2)

static double __mulpower100d(double dbl, int power)
{
	static const double Power100tableSmall[] = {
		1e+2, 1e+4, 1e+6, 1e+8, 1e+10, 1e+12, 1e+14, 1e+16, 1e+18,
		1e+20, 1e+22, 1e+24, 1e+26, 1e+28, 1e+30
	};

	static const double Power100tableLarge[] = {
		1e-288, 1e-256, 1e-224, 1e-192, 1e-160, 1e-128, 1e-96, 1e-64,
		1e-32, 1e+32, 1e+64, 1e+96, 1e+128, 1e+160, 1e+192, 1e+224,
		1e+256, 1e+288
	};
	int expPart;

	if (power != 0)	// skip 100^0
	{
		while (power <= MIN_TABLE_POWER/2)
		{
			dbl *= Power100tableLarge[0];
			power -= MIN_TABLE_POWER/2;
		}

		while (power >= MAX_TABLE_POWER/2)
		{
			dbl *= Power100tableLarge[ARRAY_SIZE(Power100tableLarge) - 1];
			power -= MAX_TABLE_POWER/2;
		}

		expPart = power & SMALL_POWER_MASK;
		if (expPart != 0)
			dbl *= Power100tableSmall[expPart - 1];

		expPart = power >> SMALL_POWER_BITS;
		if (expPart != 0)
			dbl *= Power100tableLarge[expPart + LARGE_POWER_BASE + (expPart < 0 ? 0 : -1)];
	}

	return dbl;
}

#undef SMALL_POWER_BITS
#undef SMALL_POWER_MASK
#undef MIN_TABLE_POWER
#undef MAX_TABLE_POWER
#undef LARGE_POWER_BASE

static bool __conv_flt(FILE* stream, int width, float* addr)
{
	unsigned long ul;
	float flt;
	int i;
	int exp;
	int flag;

	flag = __begin_fp(stream, width);
	if (flag & FL_ERR)
		return false;

	if (flag & FL_NAN)
		flt = NAN;
	else if (flag & FL_INF)
		flt = INFINITY;
	else
	{
		if (flag & FL_SIGN)
			width--;

		i = getc(stream);

		exp = 0;
		ul = 0;
		do
		{
			unsigned int c = i - '0';

			if (c <= 9)
			{
				flag |= FL_ANY;
				if (flag & FL_OVFL)
				{
					if (!(flag & FL_DOT))
						exp += 1;
				}
				else
				{
					if (flag & FL_DOT)
						exp -= 1;
					ul = ul * 10 + c;
					// Keep room for extra multiply by 10 if power is odd.
					if (ul >= (ULONG_MAX / 10 - 9) / 10)
						flag |= FL_OVFL;
				}
			}
			else if (c == (unsigned int)('.' - '0') && !(flag & FL_DOT))
			{
				flag |= FL_DOT;
			}
			else
				break;

		} while (--width && (i = getc(stream)) >= 0);

		if (!(flag & FL_ANY))
			return false;

		if ((unsigned char)i == 'e' || (unsigned char)i == 'E')
		{
			int expacc;

			if (!--width || (i = getc(stream)) < 0)
				return false;

			switch ((unsigned char)i)
			{
			case '-':
				flag |= FL_MEXP;
				/* FALLTHROUGH */
			case '+':
				if (!--width)
					return false;
				i = getc(stream);		/* test EOF will below	*/
			}

			if (!isdigit(i))
				return false;

			expacc = 0;
			do
			{
				expacc = expacc * 10 + i - '0';
			} while (--width && isdigit(i = getc(stream)));
			if (flag & FL_MEXP)
				expacc = -expacc;
			exp += expacc;
		}

		if (width && i >= 0)
			ungetc(i, stream);

		// Can only multiply by even powers of 10
		if (exp & 1)
			ul *= 10;
		flt = (float)ul;
		if (ul != 0)
			flt = __mulpower100f(flt, exp >> 1);
	}

	if (flag & FL_MINUS)
		flt = -flt;
	if (addr)
		*addr = flt;
	return true;
}

static bool __conv_dbl(FILE* stream, int width, double* addr)
{
	unsigned long long ull;
	double dbl;
	int i;
	int exp;
	int flag;

	flag = __begin_fp(stream, width);
	if (flag & FL_ERR)
		return false;

	if (flag & FL_NAN)
		dbl = NAN;
	else if (flag & FL_INF)
		dbl = INFINITY;
	else
	{
		if (flag & FL_SIGN)
			width--;

		i = getc(stream);

		exp = 0;
		ull = 0;
		do
		{
			unsigned int c = i - '0';

			if (c <= 9)
			{
				flag |= FL_ANY;
				if (flag & FL_OVFL)
				{
					if (!(flag & FL_DOT))
						exp += 1;
					if (c != 0)
						ull |= 1;	// sticky bit
				}
				else
				{
					if (flag & FL_DOT)
						exp -= 1;
					ull = ull * 10 + c;
					// Keep room for extra multiply by 10 if power is odd.
					// Still gives 58 bits, plenty for round & sticky bits.
					if (ull >= (ULLONG_MAX / 10 - 9) / 10)
						flag |= FL_OVFL;
				}
			}
			else if (c == (unsigned int)('.' - '0') && !(flag & FL_DOT))
			{
				flag |= FL_DOT;
			}
			else
				break;

		} while (--width && (i = getc(stream)) >= 0);

		if (!(flag & FL_ANY))
			return false;

		if ((unsigned char)i == 'e' || (unsigned char)i == 'E')
		{
			int expacc;

			if (!--width || (i = getc(stream)) < 0)
				return false;

			switch ((unsigned char)i)
			{
			case '-':
				flag |= FL_MEXP;
				/* FALLTHROUGH */
			case '+':
				if (!--width)
					return false;
				i = getc(stream);		/* test EOF will below	*/
			}

			if (!isdigit(i))
				return false;

			expacc = 0;
			do
			{
				expacc = expacc * 10 + i - '0';
			} while (--width && isdigit(i = getc(stream)));
			if (flag & FL_MEXP)
				expacc = -expacc;
			exp += expacc;
		}

		if (width && i >= 0)
			ungetc(i, stream);

		// Can only multiply by even powers of 10
		if (exp & 1)
			ull *= 10;
		dbl = (double)ull;
		if (ull != 0)
			dbl = __mulpower100d(dbl, exp >> 1);
	}

	if (flag & FL_MINUS)
		dbl = -dbl;
	if (addr)
		*addr = dbl;
	return true;
}

/*
 * convtoa
 *
 * Created: 4/25/2020 11:15:35 AM
 *  Author: Tim
 */

/* '__ftoa_engine' return next flags (in buf[0]):	*/
#define	FTOA_MINUS	1
#define	FTOA_ZERO	2
#define	FTOA_INF	4
#define	FTOA_NAN	8
#define	FTOA_CARRY	16	/* Carry was to master position.	*/

//*************************************************************************

#define Log10_2			0.30102999566398119521
#define Log10_2_shift	18
#define Log10_2_mask	((1 << (Log10_2_shift + 1)) - 1)
#define Exp2toExp10		(lround(Log10_2 * (double)(1 << Log10_2_shift)))

// IEEE single
#define EXP_BITS		8
#define MANTISSA_BITS	23
#define EXP_BIAS		127
#define MAX_EXP			((1 << EXP_BITS) - 1)
#define SIGN_BIT		(1 << 31)

//*************************************************************************

/*
	int __ftoa(float val, char *buf, int prec, int maxdgs)

 Input:
    val    - value to convert
    buf    - output buffer address
    prec   - precision: number of decimal digits is 'prec + 1'
    maxdgs - (0 if unused) precision restriction for "%f" specification

 Output:
    return     - decimal exponent of first digit
    buf[0]     - flags (FTOA_***)
    buf[1],... - decimal digits
    Number of digits:
	maxdgs == 0 ? prec+1 : aver(1, maxdgs+exp, prec+1)

 Notes:
    * Output string is not 0-terminated. For possibility of user's buffer
    usage in any case.
    * If used, 'maxdgs' is a number of digits for value with zero exponent.
*/
__attribute__ ((unused))
static int __ftoa(float val, char *buf, int prec, int maxdgs)
{
	int			exp;
	int			exp10;
	char		digit;
	char		flags;
	uint32_t	mant;
	union
	{
		float	f;
		int32_t	l;
		struct
		{
			uint32_t	mant:MANTISSA_BITS;
			uint32_t	exp:EXP_BITS;
			uint32_t	sign:1;
		} bits;
	} flt;

	// Multiply the input by a power of 10 so that 0.04 <= val < 8.
	// The ideal would be 0.05 <= val < 10, but we can't be that precise
	// using only the binary exponent. This means the true binary
	// exponent will be -5 <= exp <= 2.
	//
	// By multiplying by log10(2), we can directly calculate the power
	// of 10. Specifically, 10^-(ceil((exp - 2) * log10(2))).
	//
	// We only use even powers of 10 (10^2, 10^4...) to cut table space
	// in half. This can result in an additional multiply by 10 to get
	// the first digit. But by that time it's an integer multiply, so
	// it's fast and without loss of precision.

	flt.f = val;
	if ((flt.l & ~SIGN_BIT) == 0)
	{
		*buf++ = FTOA_ZERO;
		for (; prec > -2; prec--)
			*buf++ = '0';
		return 0;
	}

	exp = flt.bits.exp;
	flags = flt.bits.sign ? FTOA_MINUS : 0;
	if (exp == MAX_EXP)
	{
		// Infinity or NAN
		flags |= flt.bits.mant == 0 ? FTOA_INF : FTOA_NAN;
		*buf = flags;
		return 0;
	}
	*buf++ = flags;

	exp -= EXP_BIAS + 2;
	// Adding Log10_2_mask below is what performs the ceil() function.
	// Shift by Log10_2_shift + 1 because we only use even powers of 10.
	exp10 = (((long)exp * Exp2toExp10) + Log10_2_mask) >> (Log10_2_shift + 1);
	flt.f = __mulpower100f(flt.f, -exp10);
	exp10 *= 2;	// back to actual power of 10
	mant = flt.bits.mant | (1 << MANTISSA_BITS);
	exp = flt.bits.exp - EXP_BIAS;
	// Shift mantissa for actual binary exponent. Target range was
	// -5 <= exp <= 2, but it could be less if original number was
	// denormal. We add 5 so in normal case we only shift one way.
	// If exp were zero, the shift would put mantissa MSB at
	// MANTISSA_BITS + 5, and the bit would represent the
	// value 1. So our decimal digit will be formed from that bit
	// and the bits above it.
#define DIGIT_SHIFT	(MANTISSA_BITS + 5)
	exp += 5;	// exp <= 2, so max of 7
	if (exp < 0)
	{
		// Handle denormal case.
		uint32_t lsb, rnd;

		lsb = 1 << -exp;
		rnd = lsb >> 1;
		// If result LSB is zero (even) and all bits below rounding bits
		// are zero, skip round up.
		if ((mant & (lsb | (rnd - 1))) != 0)
			mant += rnd;
		mant >>= -exp;
	}
	else
		mant <<= exp;

	// Scan off leading zeros
	for(;;)
	{
		digit = mant >> DIGIT_SHIFT;
		if (digit != 0)
			break;

		mant *= 10;
		exp10--;
	}

	// Calculate the number of digits we want
	prec++;
	if (maxdgs == 0)
		maxdgs = prec;
	else
	{
		maxdgs += exp10;
		if (maxdgs < 1)
			maxdgs = 1;
		else if (maxdgs > prec)
			maxdgs = prec;
	}

	// pump out the digits
	prec = maxdgs;
	for(;;)
	{
		// upper four bits has digit
		digit += '0';
		*buf = digit;
		mant &= (1 << DIGIT_SHIFT) - 1;
		if (--maxdgs == 0)
			break;
		mant *= 10;
		digit = mant >> DIGIT_SHIFT;
		buf++;
	}

	// Round up if mantissa is > 0.5 or == 0.5 and digit is odd
#define ROUND_VALUE	(1 << (DIGIT_SHIFT - 1))
	if (mant > ROUND_VALUE || (mant == ROUND_VALUE && (digit & 1)))
	{
		// End with a zero in case we round from 9.9..9 to 10.0..0
		buf[1] = '0';

		for (;;)
		{
			digit++;
			if (digit > '9')
				digit = '0';
			*buf = digit;
			if (--prec == 0)
			{
				if (digit == '0')
				{
					*buf = '1';
					exp10++;
				}
				buf[-1] |= FTOA_CARRY;
				break;
			}
			if (digit != '0')
				break;
			digit = *--buf;
		}
	}

	return exp10;
}

#undef EXP_BITS
#undef MANTISSA_BITS
#undef EXP_BIAS
#undef MAX_EXP
#undef SIGN_BIT
#undef DIGIT_SHIFT
#undef ROUND_VALUE

// IEEE double
#define EXP_BITS		11
#define MANTISSA_BITS	52
#define EXP_BIAS		1023
#define MAX_EXP			((1 << EXP_BITS) - 1)
#define SIGN_BIT		(1LL << 63)

//*************************************************************************
/*
	int __dtoa(double val, char *buf, int prec, int maxdgs)

 Input:
	val    - value to convert
	buf    - output buffer address
	prec   - precision: number of decimal digits is 'prec + 1'
	maxdgs - (0 if unused) precision restriction for "%f" specification

 Output:
	return     - decimal exponent of first digit
	buf[0]     - flags (FTOA_***)
	buf[1],... - decimal digits
	Number of digits:
	maxdgs == 0 ? prec+1 : aver(1, maxdgs+exp, prec+1)

 Notes:
	* Output string is not 0-terminated. For possibility of user's buffer
	usage in any case.
	* If used, 'maxdgs' is a number of digits for value with zero exponent.
*/

__attribute__ ((unused))
static int __dtoa(double val, char* buf, int prec, int maxdgs)
{
	int		exp;
	int		exp10;
	char		digit;
	char		flags;
	uint64_t	mant;
	union
	{
		double	d;
		int64_t	ll;
		struct
		{
			uint64_t	mant:MANTISSA_BITS;
			uint64_t	exp:EXP_BITS;
			uint64_t	sign:1;
		} bits;
	} dbl;

	// Multiply the input by a power of 10 so that 0.04 <= val < 8.
	// The ideal would be 0.05 <= val < 10, but we can't be that precise
	// using only the binary exponent. This means the true binary
	// exponent will be -5 <= exp <= 2.
	//
	// By multiplying by log10(2), we can directly calculate the power
	// of 10. Specifically, 10^-(ceil((exp - 2) * log10(2))).
	//
	// We only use even powers of 10 (10^2, 10^4...) to cut table space
	// in half. This can result in an additional multiply by 10 to get
	// the first digit. But by that time it's an integer multiply, so
	// it's fast and without loss of precision.

	dbl.d = val;
	if ((dbl.ll & ~SIGN_BIT) == 0)
	{
		*buf++ = FTOA_ZERO;
		for (; prec > -2; prec--)
			*buf++ = '0';
		return 0;
	}

	exp = (int)dbl.bits.exp;
	flags = dbl.bits.sign ? FTOA_MINUS : 0;
	if (exp == MAX_EXP)
	{
		// Infinity or NAN
		flags |= dbl.bits.mant == 0 ? FTOA_INF : FTOA_NAN;
		*buf = flags;
		return 0;
	}
	*buf++ = flags;

	exp -= EXP_BIAS + 2;
	// Adding Log10_2_mask below is what performs the ceil() function.
	// Shift by Log10_2_shift + 1 because we only use even powers of 10.
	exp10 = (((long)exp * Exp2toExp10) + Log10_2_mask) >> (Log10_2_shift + 1);
	dbl.d = __mulpower100d(dbl.d, -exp10);
	exp10 *= 2;	// back to actual power of 10
	mant = dbl.bits.mant | (1LL << MANTISSA_BITS);
	exp = (int)dbl.bits.exp - EXP_BIAS;
	// Shift mantissa for actual binary exponent. Target range was
	// -5 <= exp <= 2, but it could be less if original number was
	// denormal. We add 5 so in normal case we only shift one way.
	// If exp were zero, the shift would put mantissa MSB at
	// MANTISSA_BITS + 5, and the bit would represent the
	// value 1. So our decimal digit will be formed from that bit
	// and the bits above it.
#define DIGIT_SHIFT	(MANTISSA_BITS + 5)
	exp += 5;	// exp <= 2, so max of 7
	if (exp < 0)
	{
		// Handle denormal case.
		uint64_t lsb, rnd;

		lsb = 1LL << -exp;
		rnd = lsb >> 1;
		// If result LSB is zero (even) and all bits below rounding bits
		// are zero, skip round up.
		if ((mant & (lsb | (rnd - 1))) != 0)
			mant += rnd;
		mant >>= -exp;
	}
	else
		mant <<= exp;

	// Scan off leading zeros
	for (;;)
	{
		digit = mant >> DIGIT_SHIFT;
		if (digit != 0)
			break;

		mant *= 10;
		exp10--;
	}

	// Calculate the number of digits we want
	prec++;
	if (maxdgs == 0)
		maxdgs = prec;
	else
	{
		maxdgs += exp10;
		if (maxdgs < 1)
			maxdgs = 1;
		else if (maxdgs > prec)
			maxdgs = prec;
	}

	// pump out the digits
	prec = maxdgs;
	for (;;)
	{
		// upper four bits has digit
		digit += '0';
		*buf = digit;
		mant &= (1LL << DIGIT_SHIFT) - 1;
		if (--maxdgs == 0)
			break;
		mant *= 10;
		digit = mant >> DIGIT_SHIFT;
		buf++;
	}

	// Round up if mantissa is > 0.5 or == 0.5 and digit is odd
#define ROUND_VALUE	(1LL << (DIGIT_SHIFT - 1))
	if (mant > ROUND_VALUE || (mant == ROUND_VALUE && (digit & 1)))
	{
		// End with a zero in case we round from 9.9..9 to 10.0..0
		buf[1] = '0';

		for (;;)
		{
			digit++;
			if (digit > '9')
				digit = '0';
			*buf = digit;
			if (--prec == 0)
			{
				if (digit == '0')
				{
					*buf = '1';
					exp10++;
				}
				buf[-1] |= FTOA_CARRY;
				break;
			}
			if (digit != '0')
				break;
			digit = *--buf;
		}
	}

	return exp10;
}

#undef EXP_BITS
#undef MANTISSA_BITS
#undef EXP_BIAS
#undef MAX_EXP
#undef SIGN_BIT
#undef DIGIT_SHIFT
#undef ROUND_VALUE

// Values for cap argument of __ultoa_rev
#define CONV_UPPER	('A' - '9' - 1)
#define CONV_LOWER	('a' - '9' - 1)

static inline unsigned char* __ultoa_rev(unsigned long val, unsigned char* str, unsigned base, int cap)
{
	unsigned	ch;

	do
	{
		UL_DIVMOD(val, ch, base);
		if (ch >= 10)
			ch += cap;
		*str++ = ch + '0';
	} while (val != 0);

	return str;
}

/* --------------------------------------------------------------------	*/

// Size of conversion buffer on stack
#if INT_MATH_LEVEL == INT_MATH_MIN
#define MAX_OCT_DIGITS	((sizeof(long) * 8 + 2) / 3)
#else
#define MAX_OCT_DIGITS	((sizeof(long long) * 8 + 2) / 3)
#endif

#if  FP_MATH_LEVEL >= FP_MATH_DBL
#define MAX_FP_DIGITS	17
#define MIN_EXP_WIDTH	6	/* 1e+000 */
#else
#define MAX_FP_DIGITS	7
#define MIN_EXP_WIDTH	5	/* 1e+00 */
#endif

#define BUF_SIZE (MAX_FP_DIGITS > MAX_OCT_DIGITS ? MAX_FP_DIGITS : MAX_OCT_DIGITS)

int metal_vfprintf(FILE * stream, const char* fmt, va_list ap)
{
	unsigned char c;		/* holds a char from the format string */
	unsigned flags;
	int width;
	int prec;
	unsigned char buf[BUF_SIZE];
	unsigned char *str;
	long x;
	bool fStar;
#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
	long long ll;
#endif

	stream->len = 0;

	if ((stream->flags & __SWR) == 0)
		return EOF;

	for (;;)
	{
		// Pass through non-format characters
		for (;;)
		{
			c = *fmt++;
			if (!c) goto ret;
			if (c == '%')
			{
				c = *fmt++;
				if (c != '%') break;
			}
			putc(c, stream);
		}

		flags = 0;
		width = 0;
		prec = 0;
		fStar = false;

		// Read format flags
		for (;; c = *fmt++)
		{
			switch (c)
			{
			case '0':
				flags |= FL_ZFILL;
				continue;
			case '+':
				flags |= FL_PLUS;
				/* FALLTHROUGH */
			case ' ':
				flags |= FL_SPACE;
				continue;
			case '-':
				flags |= FL_LPAD;
				continue;
			case '#':
				flags |= FL_ALT;
				continue;
			}
			break;
		}

		// Read format width & precision
		do
		{
			if (c >= '0' && c <= '9')
			{
				if (fStar)
					goto ret;
				c -= '0';
				if (flags & FL_PREC)
				{
					prec = 10 * prec + c;
					continue;
				}
				width = 10 * width + c;
				continue;
			}
			if (c == '.')
			{
				if (flags & FL_PREC)
					goto ret;
				fStar = 0;
				flags |= FL_PREC;
				continue;
			}
			if (c == '*')
			{
				if (fStar)
					goto ret;
				if (flags & FL_PREC)
				{
					if (prec != 0)
						goto ret;
					prec = va_arg(ap, int);
				}
				else
				{
					if (width != 0)
						goto ret;
					width = va_arg(ap, int);
				}
				fStar = true;
				continue;
			}
			break;
		} while ((c = *fmt++) != 0);

		// Read format length
		if (c == 'l')
		{
			flags |= FL_LONG;
			c = *fmt++;
			if (c == 'l')
			{
				flags |= FL_LL;
				c = *fmt++;
			}
		}
		else if (c == 'h')
		{
			flags |= FL_H;
			c = *fmt++;
		}

		/* Only a format character is valid.	*/

#if	'F' != 'E'+1  ||  'G' != 'F'+1  ||  'f' != 'e'+1  ||  'g' != 'f'+1
# error
#endif

#if FP_MATH_LEVEL > FP_MATH_NONE
		if (c >= 'E' && c <= 'G')
		{
			flags |= FL_FLTUPP;
			c += 'e' - 'E';
			goto flt_oper;

		}
		else if (c >= 'e' && c <= 'g')
		{

			/* exponent of master decimal digit	*/
			int exp;
			int n;
			/* result of float value parse	*/
			unsigned char vtype;
			/* sign character (or 0)	*/
			unsigned char sign;
# define ndigs	c	/* only for this block, undef is below	*/

			flags &= ~FL_FLTUPP;

flt_oper:
			if (!(flags & FL_PREC))
				prec = 6;
			flags &= ~(FL_FLTEXP | FL_FLTFIX);
			if (c == 'e')
				flags |= FL_FLTEXP;
			else if (c == 'f')
				flags |= FL_FLTFIX;
			else if (prec > 0)	// c == 'g'
				prec -= 1;

			if (flags & FL_FLTFIX)
			{
				/* 'prec' arg for 'ftoa_engine'	*/
				vtype = MAX_FP_DIGITS;
				ndigs = prec < 60 ? prec + 1 : 60;
			}
			else
			{
				if (prec > MAX_FP_DIGITS)
					prec = MAX_FP_DIGITS;
				vtype = prec;
				ndigs = 0;
			}
#if FP_MATH_LEVEL == FP_MATH_FLT
			union { uint32_t l; float f; } u;
			u.l = va_arg(ap, uint32_t);
			exp = __ftoa(u.f, (char*)buf, vtype, ndigs);
#else
			exp = __dtoa(va_arg(ap, double), (char*)buf, vtype, ndigs);
#endif
			vtype = buf[0];

			sign = 0;
			if ((vtype & FTOA_MINUS) && !(vtype & FTOA_NAN))
				sign = '-';
			else if (flags & FL_PLUS)
				sign = '+';
			else if (flags & FL_SPACE)
				sign = ' ';

			if (vtype & (FTOA_NAN | FTOA_INF))
			{
				const char* p;
				ndigs = sign ? 4 : 3;
				if (width > ndigs)
				{
					width -= ndigs;
					if (!(flags & FL_LPAD))
					{
						do
						{
							putc(' ', stream);
						} while (--width);
					}
				}
				else
				{
					width = 0;
				}
				if (sign)
					putc(sign, stream);
				p = "inf";
				if (vtype & FTOA_NAN)
					p = "nan";
# if ('I'-'i' != 'N'-'n') || ('I'-'i' != 'F'-'f') || ('I'-'i' != 'A'-'a')
#  error
# endif
				while ((ndigs = *p) != 0)
				{
					if (flags & FL_FLTUPP)
						ndigs += 'I' - 'i';
					putc(ndigs, stream);
					p++;
				}
				goto tail;
			}

			/* Output format adjustment, number of decimal digits in buf[] */
			if (flags & FL_FLTFIX)
			{
				ndigs += exp;
				if ((vtype & FTOA_CARRY) && buf[1] == '1')
					ndigs -= 1;
				if ((signed char)ndigs < 1)
					ndigs = 1;
				else if (ndigs > MAX_FP_DIGITS + 1)
					ndigs = MAX_FP_DIGITS + 1;
			}
			else if (!(flags & FL_FLTEXP))
			{		/* 'g(G)' format */
				if (exp <= prec && exp >= -4)
					flags |= FL_FLTFIX;
				while (prec && buf[1 + prec] == '0')
					prec--;
				if (flags & FL_FLTFIX)
				{
					/* number of digits in buf */
					ndigs = prec + 1;
					prec = prec > exp
						/* fractional part length */
						? prec - exp : 0;
				}
			}

			/* Conversion result length, width := free space length	*/
			if (flags & FL_FLTFIX)
				n = (exp > 0 ? exp + 1 : 1);
			else
				n = MIN_EXP_WIDTH;
			if (sign) n += 1;
			if (prec) n += prec + 1;
			width = width > n ? width - n : 0;

			/* Output before first digit	*/
			if (!(flags & (FL_LPAD | FL_ZFILL)))
			{
				while (width)
				{
					putc(' ', stream);
					width--;
				}
			}
			if (sign) putc(sign, stream);
			if (!(flags & FL_LPAD))
			{
				while (width)
				{
					putc('0', stream);
					width--;
				}
			}

			if (flags & FL_FLTFIX)
			{
				/* 'f' format	*/

				/* exponent of left digit */
				n = exp > 0 ? exp : 0;
				do
				{
					if (n == -1)
						putc('.', stream);
					flags = (n <= exp && n > exp - ndigs)
						? buf[exp - n + 1] : '0';
					if (--n < -prec)
						break;
					putc(flags, stream);
				} while (1);
				if (n == exp
					&& (buf[1] > '5'
						|| (buf[1] == '5' && !(vtype & FTOA_CARRY))))
				{
					flags = '1';
				}
				putc(flags, stream);

			}
			else
			{
				/* 'e(E)' format	*/

				/* mantissa	*/
				if (buf[1] != '1')
					vtype &= ~FTOA_CARRY;
				putc(buf[1], stream);
				if (prec)
				{
					putc('.', stream);
					sign = 2;
					do
					{
						putc(buf[sign++], stream);
					} while (--prec);
				}

				/* exponent	*/
				putc(flags & FL_FLTUPP ? 'E' : 'e', stream);
				ndigs = '+';
				if (exp < 0 || (exp == 0 && (vtype & FTOA_CARRY) != 0))
				{
					exp = -exp;
					ndigs = '-';
				}
				putc(ndigs, stream);
#if  FP_MATH_LEVEL >= FP_MATH_DBL
				for (ndigs = '0'; exp >= 100; exp -= 100)
					ndigs += 1;
				putc(ndigs, stream);
#endif
				for (ndigs = '0'; exp >= 10; exp -= 10)
					ndigs += 1;
				putc(ndigs, stream);
				putc('0' + exp, stream);
			}

			goto tail;
# undef ndigs
		}

#else		/* to: FP_MATH_LEVEL > FP_MATH_NONE */
		if ((c >= 'E' && c <= 'G') || (c >= 'e' && c <= 'g'))
		{
			(void)va_arg(ap, double);
			buf[0] = '?';
			goto buf_addr;
		}

#endif

		{
			const char* pnt;
			int size;

			switch (c)
			{

			case 'c':
				buf[0] = va_arg(ap, int);
#if FP_MATH_LEVEL == FP_MATH_NONE || INT_MATH_LEVEL == INT_MATH_MIN
buf_addr:
#endif
				pnt = (char*)buf;
				size = 1;
				goto str_lpad;

			case 's':
				pnt = va_arg(ap, char*);
				size = strnlen(pnt, (flags & FL_PREC) ? prec : ~0);
				goto str_lpad;

str_lpad:
				if (!(flags & FL_LPAD))
				{
					while (size < width)
					{
						putc(' ', stream);
						width--;
					}
				}
				while (size)
				{
					putc(*pnt++, stream);
					if (width) width -= 1;
					size -= 1;
				}
				goto tail;
			}
		}

		unsigned base = 10;
		int cap = CONV_LOWER;

		if (c == 'd' || c == 'i')
		{
			flags &= ~FL_ALT;

			if (flags & FL_LL)
			{
#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
				ll = va_arg(ap, long long);
				if (ll < 0)
				{
					ll = -ll;
					flags |= FL_NEGATIVE;
				}
				goto ulltoa;
#else
				x = va_arg(ap, long);
				buf[0] = '?';
				goto buf_addr;
#endif
			}
			if (flags & FL_LONG)
				x = va_arg(ap, long);
			else
				x = va_arg(ap, int);

			if (x < 0)
			{
				x = -x;
				flags |= FL_NEGATIVE;
			}
			goto ultoa;
		}
		else if (c == 'u')
		{
			flags &= ~FL_ALT;
		}
		else
		{
			base = 16;
			flags &= ~(FL_PLUS | FL_SPACE);

			switch (c)
			{
			case 'o':
				base = 8;
				break;
			case 'P':
				if (flags & FL_ALT)
					flags |= (FL_ALTHEX | FL_ALTUPP);
				cap = CONV_UPPER;
				/* fall-through */
			case 'p':
				if (flags & FL_ALT)
					flags |= FL_ALTHEX;
#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
				if (sizeof(void *) > sizeof(long))
					goto ullRead;
#endif
				if (sizeof(void *) > sizeof(int))
					x = va_arg(ap, unsigned long);
				else
					x = va_arg(ap, unsigned int);
				goto ultoa;
			case 'x':
				if (flags & FL_ALT)
					flags |= FL_ALTHEX;
				break;
			case 'X':
				if (flags & FL_ALT)
					flags |= (FL_ALTHEX | FL_ALTUPP);
				cap = CONV_UPPER;
				break;

			default:
				goto ret;
			}
		}

		if (flags & FL_LL)
		{
#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
			int	ch;
			unsigned long long ull;
ullRead:
			ll = va_arg(ap, unsigned long long);
ulltoa:
			// Compute digits until shrunk to ulong
			ull = ll;
			str = buf;
			while (ull > ULONG_MAX)
			{
				ULL_DIVMOD(ull, ch, base);
				if (ch >= 10)
					ch += cap;
				*str++ = ch + '0';
			}
			x = (unsigned long)ull;
			goto ultoaStr;
#else
			x = va_arg(ap, unsigned long);
			buf[0] = '?';
			goto buf_addr;
#endif
		}
		if (sizeof(long) > sizeof(int) && (flags & FL_LONG))
			x = va_arg(ap, unsigned long);
		else
		{
			x = va_arg(ap, unsigned int);
			if (flags & FL_H)
				x = (unsigned short)x;
		}

ultoa:
		str = buf;
#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
ultoaStr:
#endif
		c = __ultoa_rev(x, str, base, cap) - buf;

		{
			unsigned char len;

			len = c;
			if (flags & FL_PREC)
			{
				flags &= ~FL_ZFILL;
				if (len < prec)
				{
					len = prec;
					if ((flags & FL_ALT) && !(flags & FL_ALTHEX))
						flags &= ~FL_ALT;
				}
			}
			if (flags & FL_ALT)
			{
				if (buf[c - 1] == '0')
				{
					flags &= ~(FL_ALT | FL_ALTHEX | FL_ALTUPP);
				}
				else
				{
					len += 1;
					if (flags & FL_ALTHEX)
						len += 1;
				}
			}
			else if (flags & (FL_NEGATIVE | FL_PLUS | FL_SPACE))
			{
				len += 1;
			}

			if (!(flags & FL_LPAD))
			{
				if (flags & FL_ZFILL)
				{
					prec = c;
					if (len < width)
					{
						prec += width - len;
						len = width;
					}
				}
				while (len < width)
				{
					putc(' ', stream);
					len++;
				}
			}

			width = (len < width) ? width - len : 0;

			if (flags & FL_ALT)
			{
				putc('0', stream);
				if (flags & FL_ALTHEX)
					putc(flags & FL_ALTUPP ? 'X' : 'x', stream);
			}
			else if (flags & (FL_NEGATIVE | FL_PLUS | FL_SPACE))
			{
				unsigned char z = ' ';
				if (flags & FL_PLUS) z = '+';
				if (flags & FL_NEGATIVE) z = '-';
				putc(z, stream);
			}

			while (prec > c)
			{
				putc('0', stream);
				prec--;
			}

			do
			{
				putc(buf[--c], stream);
			} while (c);
		}

tail:
		/* Tail is possible.	*/
		while (width)
		{
			putc(' ', stream);
			width--;
		}
	} /* for (;;) */

ret:
	return stream->len;
}

int metal_fputc(int c, FILE *stream)
{
	if ((stream->flags & __SWR) == 0)
		return EOF;

	// Turn LF into CRLF if flagged
	if ((stream->flags & __SCRLF ) && c == '\n')
		fputc('\r', stream);

	if (stream->flags & __SSTR) {
		if (stream->len < stream->u.mem.size)
			*stream->u.mem.buf++ = c;
	} else {
		stream->u.dev.put(stream->udata, c);
	}
	stream->len++;
	return c;
}

int metal_printf(const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = vfprintf(stdout, fmt, ap);
	va_end(ap);

	return i;
}

int metal_snprintf(char *s, size_t n, const char *fmt, ...)
{
	va_list ap;
	FILE f;
	int i;

	f.flags = __SWR | __SSTR;
	f.u.mem.buf = s;
	/* Restrict max output length to INT_MAX, as snprintf() return
	   signed int. The fputc() function uses a signed comparison
	   between estimated len and f.u.mem.size field. So we can write a
	   negative value into f.u.mem.size in the case of n was 0. Note,
	   that f.u.mem.size will be a max number of nonzero symbols.	*/
	if ((int)n < 0)
		n = (unsigned)INT_MAX + 1;
	f.u.mem.size = n - 1;				/* -1,0,...INT_MAX */

	va_start(ap, fmt);
	i = vfprintf(&f, fmt, ap);
	va_end(ap);

	if (f.u.mem.size >= 0)
		s[f.len < f.u.mem.size ? f.len : f.u.mem.size] = 0;

	return i;
}

int metal_vsprintf(char *s, const char *fmt, va_list ap)
{
	FILE f;
	int i;

	f.flags = __SWR | __SSTR;
	f.u.mem.buf = s;
	f.u.mem.size = INT_MAX;
	i = vfprintf(&f, fmt, ap);
	s[f.len] = 0;

	return i;
}

int metal_vsnprintf(char *s, size_t n, const char *fmt, va_list ap)
{
	FILE f;
	int i;

	f.flags = __SWR | __SSTR;
	f.u.mem.buf = s;
	/* Restrict max output length to INT_MAX, as snprintf() return
	   signed int. The fputc() function uses a signed comparison
	   between estimated len and f.u.mem.size field. So we can write a
	   negative value into f.u.mem.size in the case of n was 0. Note,
	   that f.u.mem.size will be a max number of nonzero symbols.	*/
	if ((int)n < 0)
		n = (unsigned)INT_MAX + 1;
	f.u.mem.size = n - 1;				/* -1,0,...INT_MAX */

	i = vfprintf(&f, fmt, ap);

	if (f.u.mem.size >= 0)
		s[f.len < f.u.mem.size ? f.len : f.u.mem.size] = 0;

	return i;
}

int metal_fprintf(FILE *stream, const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = vfprintf(stream, fmt, ap);
	va_end(ap);

	return i;
}

int metal_fputs(const char *str, FILE *stream)
{
	char c;

	if ((stream->flags & __SWR) == 0)
		return EOF;

	while ((c = *str++) != '\0')
		fputc(c, stream);

	return 0;
}

int metal_puts(const char *str)
{
	char c;

	if ((stdout->flags & __SWR) == 0)
		return EOF;

	while ((c = *str++) != '\0')
		fputc(c, stdout);

	fputc('\n', stdout);
	return 0;
}

size_t metal_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t i, j;
	const uint8_t *cp;

	if ((stream->flags & __SWR) == 0)
		return 0;

	for (i = 0, cp = (const uint8_t *)ptr; i < nmemb; i++)
		for (j = 0; j < size; j++)
			fputc(*cp++, stream);

	return i;
}

int metal_fgetc(FILE *stream)
{
	int rv;

	if ((stream->flags & __SRD) == 0)
		return EOF;

	if ((stream->flags & __SUNGET) != 0) {
		stream->flags &= ~__SUNGET;
		stream->len++;
		return stream->unget;
	}

	if (stream->flags & __SSTR) {
		rv = *stream->u.mem.buf;
		if (rv == '\0') {
			stream->flags |= __SEOF;
			return EOF;
		} else {
			stream->u.mem.buf++;
		}
	} else {
		rv = stream->u.dev.get(stream->udata);
		if (rv < 0) {
			/* if != _FDEV_ERR, assume it's _FDEV_EOF */
			stream->flags |= (rv == _FDEV_ERR)? __SERR: __SEOF;
			return EOF;
		}
	}

	stream->len++;
	return (unsigned char)rv;
}

int metal_ungetc(int c, FILE *stream)
{
	/*
	 * Streams that are not readable, or streams that already had
	 * had an ungetc() before will cause an error.
	 *
	 * ungetc(EOF, ...) causes an error per definition.
	 */
	if ((stream->flags & __SRD) == 0 ||
	    (stream->flags & __SUNGET) != 0 ||
	    c == EOF)
		return EOF;

	stream->unget = c;
	stream->flags |= __SUNGET;
	stream->flags &= ~__SEOF;
	stream->len--;

	return stream->unget;
}

char * metal_fgets(char *str, int size, FILE *stream)
{
	char *cp;
	int c;

	if ((stream->flags & __SRD) == 0 || size <= 0)
		return NULL;

	size--;
	for (c = 0, cp = str; c != '\n' && size > 0; size--, cp++) {
		if ((c = getc(stream)) == EOF)
			return NULL;
		*cp = (char)c;
	}
	*cp = '\0';

	return str;
}

size_t metal_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	size_t i, j;
	uint8_t *cp;
	int c;

	if ((stream->flags & __SRD) == 0)
		return 0;

	for (i = 0, cp = (uint8_t *)ptr; i < nmemb; i++)
		for (j = 0; j < size; j++) {
			c = getc(stream);
			if (c == EOF)
				return i;
			*cp++ = (uint8_t)c;
		}

	return i;
}

#undef FL_LONG
#undef FL_LL
#define FL_LONG		0x80
#define FL_LL		0x100

// "h" & "hh" means FL_SHORT is set if FL_CHAR is set, so test FL_CHAR first
// "l" & "ll" means FL_LONG is set if FL_LL is set, so test FL_LL first
static void putval(void* addr, unsigned flags, val_t val)
{
	if (!(flags & FL_STAR))
	{
		if (flags & FL_CHAR)
			*(char*)addr = (char)val;
#if INT_MATH_LEVEL >= INT_MATH_LONG_LONG
		else if (flags & FL_LL)
			*(val_t*)addr = val;
#endif
		else if (flags & FL_LONG)
			*(long*)addr = (long)val;
		else if (flags & FL_SHORT)
			*(short*)addr = (short)val;
		else
			*(int*)addr = (int)val;
	}
}

__attribute__((noinline))
static unsigned char
conv_int(FILE* stream, int width, void* addr, unsigned flags)
{
	val_t val;
	int i;

	i = getc(stream);			/* after ungetc()	*/

	switch ((unsigned char)i)
	{
	case '-':
		flags |= FL_MINUS;
		/* FALLTHROUGH */
	case '+':
		if (!--width || (i = getc(stream)) < 0)
			goto err;
	}

	val = 0;
	flags &= ~FL_WIDTH;

	if (!(flags & (FL_DEC | FL_OCT)) && i == '0')
	{
		if (!--width || (i = getc(stream)) < 0)
			goto putval;
		flags |= FL_WIDTH;
		if (i == 'x' || i == 'X')
		{
			flags |= FL_HEX;
			if (!--width || (i = getc(stream)) < 0)
				goto putval;
		}
		else
		{
			if (!(flags & FL_HEX))
				flags |= FL_OCT;
		}
	}

	/* This fact is used below to parse hexadecimal digit.	*/
#if	('A' - '0') != (('a' - '0') & ~('A' ^ 'a'))
# error
#endif

	do
	{
		unsigned u = i;
		u -= '0';
		if (flags & FL_OCT)
		{
			if (u > 7) goto unget;
			val = val * 8 + u;
		}
		else if (flags & FL_HEX)
		{
			if (u > 9)
			{
				u &= ~('A' ^ 'a');
				u += '0' - 'A';
				if (u > 5) goto unget;
				u += 10;
			}
			val = val * 16 + u;
		}
		else
		{
			if (u > 9)
			{
unget:
				ungetc(i, stream);
				break;
			}
			val = val * 10 + u;
		}
		flags |= FL_WIDTH;
		if (!--width) goto putval;
	} while ((i = getc(stream)) >= 0);
	if (!(flags & FL_WIDTH))
		goto err;

putval:
	if (flags & FL_MINUS) val = -(long)val;
	putval(addr, flags, val);
	return 1;

err:
	return 0;
}

static const char*
conv_brk(FILE* stream, int width, char* addr, const char* fmt)
{
	unsigned char msk[32];
	unsigned char fnegate;
	unsigned char frange;
	unsigned char cabove;
	int i;

	memset(msk, 0, sizeof(msk));
	fnegate = 0;
	frange = 0;
	cabove = 0;			/* init to avoid compiler warning	*/

	for (i = 0; ; i++)
	{
		unsigned char c = *fmt++;

		if (c == 0)
		{
			return 0;
		}
		else if (c == '^' && !i)
		{
			fnegate = 1;
			continue;
		}
		else if (i > fnegate)
		{
			if (c == ']') break;
			if (c == '-' && !frange)
			{
				frange = 1;
				continue;
			}
		}

		if (!frange) cabove = c;

		for (;;)
		{
			msk[c >> 3] |= 1 << (c & 7);
			if (c == cabove) break;
			if (c < cabove)
				c++;
			else
				c--;
		}

		frange = 0;
	}
	if (frange)
		msk['-' / 8] |= 1 << ('-' & 7);

	if (fnegate)
	{
		unsigned char* p = msk;
		do
		{
			unsigned char c = *p;
			*p++ = ~c;
		} while (p != msk + sizeof(msk));
	}

	/* And now it is a flag of fault.	*/
	fnegate = 1;

	/* NUL ('\0') is considered as normal character. This is match to Glibc.
	   Note, there is no method to include NUL into symbol list.	*/
	do
	{
		i = getc(stream);
		if (i < 0) break;
		if (!((msk[(unsigned char)i >> 3] >> (i & 7)) & 1))
		{
			ungetc(i, stream);
			break;
		}
		if (addr) *addr++ = i;
		fnegate = 0;
	} while (--width);

	if (fnegate)
	{
		return 0;
	}
	else
	{
		if (addr) *addr = 0;
		return fmt;
	}
}

int metal_vfscanf(FILE* stream, const char* fmt, va_list ap)
{
	int  nconvs;
	unsigned char c;
	int width;
	char* addr;
	unsigned flags;
	int i;

	nconvs = 0;
	stream->len = 0;

	while ((c = *fmt++) != 0)
	{

		if (isspace(c))
		{
			__skip_spaces(stream);

		}
		else if (c != '%'
			|| (c = *fmt++) == '%')
		{
			/* Ordinary character.	*/
			if ((i = getc(stream)) < 0)
				goto eof;
			if ((unsigned char)i != c)
			{
				ungetc(i, stream);
				break;
			}

		}
		else
		{
			flags = 0;

			if (c == '*')
			{
				flags = FL_STAR;
				c = *fmt++;
			}

			width = 0;
			while ((c -= '0') < 10)
			{
				flags |= FL_WIDTH;
				width = 10 * width + c;
				c = *fmt++;
			}
			c += '0';
			if (flags & FL_WIDTH)
			{
				/* C99 says that width must be greater than zero.
				   To simplify program do treat 0 as error in format.	*/
				if (!width) break;
			}
			else
			{
				width = ~0;
			}

			switch (c)
			{
			case 'h':
				flags |= FL_SHORT;
				if ((c = *fmt++) != 'h')
					break;
				flags |= FL_CHAR;
				c = *fmt++;
				break;

			case 'l':
				flags |= FL_LONG;
				if ((c = *fmt++) != 'l')
					break;
				flags |= FL_LL;
				c = *fmt++;
			}

#define CNV_BASE	"cdinopsuxX["
#if FP_MATH_LEVEL >= FP_MATH_FLT
# define CNV_FLOAT	"efgEFG"
#else
# define CNV_FLOAT	""
#endif
#define CNV_LIST	CNV_BASE CNV_FLOAT
			if (!c || !strchr(CNV_LIST, c))
				break;

			addr = (flags & FL_STAR) ? 0 : va_arg(ap, char*);

			if (c == 'n')
			{
				putval(addr, flags, (unsigned)(stream->len));
				continue;
			}

			if (c == 'c')
			{
				if (!(flags & FL_WIDTH)) width = 1;
				do
				{
					if ((i = getc(stream)) < 0)
						goto eof;
					if (addr) *addr++ = i;
				} while (--width);
				c = 1;			/* no matter with smart GCC	*/

			}
			else if (c == '[')
			{
				fmt = conv_brk(stream, width, addr, fmt);
				c = (fmt != 0);
			}
			else
			{
				if (__skip_spaces(stream) < 0)
					goto eof;

				switch (c)
				{

				case 's':
					/* Now we have 1 nospace symbol.	*/
					do
					{
						if ((i = getc(stream)) < 0)
							break;
						if (isspace(i))
						{
							ungetc(i, stream);
							break;
						}
						if (addr) *addr++ = i;
					} while (--width);
					if (addr) *addr = 0;
					c = 1;		/* no matter with smart GCC	*/
					break;

#if FP_MATH_LEVEL >= FP_MATH_FLT
				case 'p':
				case 'x':
				case 'X':
					flags |= FL_HEX;
					goto conv_int;

				case 'd':
				case 'u':
					flags |= FL_DEC;
					goto conv_int;

				case 'o':
					flags |= FL_OCT;
					/* FALLTHROUGH */
				case 'i':
conv_int:
					c = conv_int(stream, width, addr, flags);
					break;

				default:		/* e,E,f,F,g,G	*/
					if (flags & FL_LONG)
					{
#if FP_MATH_LEVEL >= FP_MATH_DBL
						c = __conv_dbl(stream, width, (double*)addr);
#else
						goto eof;
#endif
					}
					else
					{
#if FP_MATH_LEVEL != FP_MATH_DBL
						c = __conv_flt(stream, width, (float*)addr);
#else
						goto eof;
#endif
					}

#else	// FP_MATH_LEVEL >= FP_MATH_FLT
				case 'd':
				case 'u':
					flags |= FL_DEC;
					goto conv_int;

				case 'o':
					flags |= FL_OCT;
					/* FALLTHROUGH */
				case 'i':
					goto conv_int;

				default:			/* p,x,X	*/
					flags |= FL_HEX;
conv_int:
					c = conv_int(stream, width, addr, flags);
#endif	// #else FP_MATH_LEVEL >= FP_MATH_FLT
				}
			} /* else */

			if (!c)
			{
				if (stream->flags & (__SERR | __SEOF))
					goto eof;
				break;
			}
			if (!(flags & FL_STAR)) nconvs += 1;
		} /* else */
	} /* while */
	return nconvs;

eof:
	return nconvs ? nconvs : EOF;
}

int metal_fscanf(FILE *stream, const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = vfscanf(stream, fmt, ap);
	va_end(ap);

	return i;
}

int metal_scanf(const char *fmt, ...)
{
	va_list ap;
	int i;

	va_start(ap, fmt);
	i = vfscanf(stdin, fmt, ap);
	va_end(ap);

	return i;
}

int metal_sscanf(const char *s, const char *fmt, ...)
{
	va_list ap;
	FILE f;
	int i;

	f.flags = __SRD | __SSTR;
	/*
	 * It is OK to discard the "const" qualifier here.  The buffer is
	 * really only be read (by getc()), and as this our FILE f we
	 * be discarded upon exiting sscanf(), nobody will ever get
	 * a chance to get write access to it again.
	 */
	f.u.mem.buf = (char *)s;
	va_start(ap, fmt);
	i = vfscanf(&f, fmt, ap);
	va_end(ap);

	return i;
}

int metal_vsscanf(const char *s, const char *fmt, va_list ap)
{
	FILE f;
	int i;

	f.flags = __SRD | __SSTR;
	/*
	 * It is OK to discard the "const" qualifier here.  The buffer is
	 * really only be read (by getc()), and as this our FILE f we
	 * be discarded upon exiting sscanf(), nobody will ever get
	 * a chance to get write access to it again.
	 */
	f.u.mem.buf = (char *)s;
	i = vfscanf(&f, fmt, ap);

	return i;
}

int metal_fflush(FILE *stream)
{
	if (stream->flags & __SSTR) {
		stream->flags |= __SEOF;
		stream->len = stream->u.mem.size;
		return 0;
	}

	if (stream->u.dev.flush != NULL)
		return stream->u.dev.flush(stream->udata);

	return 0;
}

float metal_strtof(const char *psz, char **ppend)
{
	FILE	f;
	float	flt;
	char*	pend;

	f.flags = __SRD | __SSTR;
	f.u.mem.buf = (char *)psz;

	if (__skip_spaces(&f) >= 0 && __conv_flt(&f, INT_MAX, &flt))
	{
		pend = f.u.mem.buf - (f.flags & __SUNGET ? 1 : 0);
	}
	else
	{
		flt = 0.0;
		pend = (char *)psz;
	}
	if (ppend != NULL)
		*ppend = pend;
	return flt;
}

double metal_strtod(const char *psz, char **ppend)
{
	FILE	f;
	double  dbl;
	char*   pend;

	f.flags = __SRD | __SSTR;
	f.u.mem.buf = (char *)psz;

	if (__skip_spaces(&f) >= 0 && __conv_dbl(&f, INT_MAX, &dbl))
	{
		pend = f.u.mem.buf - (f.flags & __SUNGET ? 1 : 0);
	}
	else
	{
		dbl = 0.0;
		pend = (char *)psz;
	}
	if (ppend != NULL)
		*ppend = pend;
	return dbl;
}

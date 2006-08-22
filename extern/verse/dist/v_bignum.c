/*
 * Routines for big (thousands of bits) unsigned integers, and
 * doing simple maths operations on them. Written by Emil Brink.
 * 
 * Part of the Verse core, see license details elsewhere.
 * 
 * Bignums are represented as vectors of VBigDig (unsigned short),
 * where the first element holds the length of the number in such
 * digits. So a 32-bit number would be { 2, low, high }; digits are
 * in little-endian format.
 * 
 * Verse's uint16 and uint32 types are *not* used, since there is no
 * need to limit the bits. If your machine has 32-bit shorts and 64-
 * bit ints, this code should cope.
 * 
 * By using unsigned shorts, which are assumed to be half the size of
 * an unsigned int, we can easily do intermediary steps in int-sized
 * variables, and thus get space for manual carry-management.
 * 
 * This is the second incarnation of this code, the first one used
 * a fixed 2,048-bit VBigNum structure passed by value. It had to be
 * replaced since it was too weak for the desired functionality. Now,
 * there's roughly 1,5 weeks of time gone into this code, which still
 * means it's optimized for simplicity rather than speed.
 * 
 * There has been neither time nor interest to meditate over FFTs and
 * Karatsubas. Reasonable improvements are of course welcome, although
 * this code *should* not be a bottleneck. Famous last words...
 * 
 * In general, these routines do not do a lot of error checking, they
 * assume you know what you're doing. Numbers must have >0 digits.
 * Shifts should not be overly large (1e3 bits: safe, ~2e9+: avoid).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v_randgen.h"

#include "v_bignum.h"

#define	MAX_DIG	((1UL << V_BIGBITS) - 1)

/* ----------------------------------------------------------------------------------------- */

/* Some routines need temporary storage to hold a term or two (the multi-
 * plier, for instance). Since we don't want to use malloc()/free(), let's
 * just have a bunch of digits that it's possible to allocate from in a
 * stack-like manner.
*/
static VBigDig		heap[2048 + 32];
static unsigned int	heap_pos;

/* Allocate a number of <n> digits, returning it un-initialized. */
static VBigDig * bignum_alloc(unsigned int n)
{
	VBigDig	*y;

	if(heap_pos + n > sizeof heap / sizeof *heap)
	{
		printf("Out of memory in bignum heap -- unbalanced calls?\n");
		return NULL;
	}
	y = heap + heap_pos;
	heap_pos += n + 1;
	*y = n;
	return y;
}

/* Free a number previously allocated by bignum_allow() above. MUST match in sequences. */
static void bignum_free(const VBigDig *x)
{
	heap_pos -= *x + 1;
}

/* ----------------------------------------------------------------------------------------- */

/* Set x from bits. External representation is big-endian byte array. */
void v_bignum_raw_import(VBigDig *x, const void *bits)
{
	const unsigned char	*bytes = bits;
	int			i;

	for(i = *x++ - 1; i >= 0; i--)
	{
		x[i] =  ((VBigDig) *bytes++) << 8;
		x[i] |= *bytes++;
	}
}

/* Set bits to value of x. External representation is big-endian byte array. */
void v_bignum_raw_export(const VBigDig *x, void *bits)
{
	unsigned char	*bytes = bits;
	int		i;

	for(i = *x++ - 1; i >= 0; i--)
	{
		*bytes++ = x[i] >> 8;
		*bytes++ = (unsigned char) x[i];
	}
}

/* ----------------------------------------------------------------------------------------- */

/* Assigns x = 0. */
void v_bignum_set_zero(VBigDig *x)
{
	memset(x + 1, 0, *x * sizeof *x);
}

/* Assigns x = 1. */
void v_bignum_set_one(VBigDig *x)
{
	int	i;

	for(i = *x++ - 1, *x++ = 1; i > 0; i--)
		*x++ = 0;
}

/* Assigns x = y. */
void v_bignum_set_digit(VBigDig *x, VBigDig y)
{
	v_bignum_set_zero(x);
	x[1] = y;
}

/* Assigns x = <string>, with string in decimal ASCII. Kind of slow. */
void v_bignum_set_string(VBigDig *x, const char *string)
{
	unsigned int	d;

	v_bignum_set_zero(x);
	for(; *string && isdigit(*string); string++)
	{
		v_bignum_mul_digit(x, 10);
		d = *string - '0';
		v_bignum_add_digit(x, d);
	}
}

/* Assigns x = <string>, with string in hexadecimal ASCII. */
void v_bignum_set_string_hex(VBigDig *x, const char *string)
{
	unsigned int	d;

	if(string[0] == '0' && (string[1] == 'x' || string[1] == 'X'))
		string += 2;
	v_bignum_set_zero(x);
	for(; *string && isxdigit(*string); string++)
	{
		v_bignum_bit_shift_left(x, 4);
		d = tolower(*string) - '0';
		if(d > 9)
			d -= ('a' - '0') - 10;
		x[1] |= (d & 0xF);
	}
}

/* Performs x = y, taking care to handle different precisions correctly by truncating. */
void v_bignum_set_bignum(VBigDig *x, const VBigDig *y)
{
	int	xs, ys, i, s;

	xs = x[0];
	ys = y[0];
	if(xs == ys)	/* For same sizes, just memcpy() and be done. */
	{
		memcpy(x + 1, y + 1, xs * sizeof *x);
		return;
	}
	else if(ys > xs)
		s = xs;
	else
		s = ys;
	/* Copy as many digits as will fit, and clear any remaining high digits. */
	for(i = 1; i <= s; i++)
		x[i] = y[i];
	for(; i <= xs; i++)
		x[i] = 0;
}

/* Performs x = y[msb:msb-bits], right-adjusting the result. */
void v_bignum_set_bignum_part(VBigDig *x, const VBigDig *y, unsigned int msb, unsigned int bits)
{
	unsigned int	i, bit;

	v_bignum_set_zero(x);
	if(y == NULL || msb > (y[0] * (CHAR_BIT * sizeof *x)))
		return;
	for(i = 0; i < bits; i++)
	{
		bit = msb - (bits - 1) + i;
		if(v_bignum_bit_test(y, bit))
			v_bignum_bit_set(x, i);
	}
}

/* Set x to a random bunch of bits. Should use a real random source. */
void v_bignum_set_random(VBigDig *x, VRandGen *gen)
{
	unsigned int	s = *x++;

	if(gen != NULL)
		v_randgen_get(gen, x, s * sizeof *x);
	else
	{
		fprintf(stderr, "** Warning: Calling v_bignum_set_random() without VRandGen is potentially expensive\n");
		if((gen = v_randgen_new()) != NULL)
		{
			v_randgen_get(gen, x, s * sizeof *x);
			v_randgen_destroy(gen);
		}
		else
			fprintf(stderr, __FILE__ ":  Couldn't create random number generator\n");
	}
}

/* Print x in hexadecimal, with 0x prefix but no linefeed. */
void v_bignum_print_hex(const VBigDig *x)
{
	int	i, s = *x;

	printf("0x");
	for(i = 0; i < s; i++)
		printf("%04X", x[s - i]);
}

/* Print x in hexadecimal, with linefeed. */
void v_bignum_print_hex_lf(const VBigDig *x)
{
	v_bignum_print_hex(x);
	printf("\n");
}

/* ----------------------------------------------------------------------------------------- */

/* x = ~x. */
void v_bignum_not(VBigDig *x)
{
	unsigned int	i, s = *x++;

	for(i = 0; i < s; i++)
		x[i] = ~x[i];
}

int v_bignum_bit_test(const VBigDig *x, unsigned int bit)
{
	unsigned int	slot = bit / (CHAR_BIT * sizeof *x), m = 1 << (bit % (CHAR_BIT * sizeof *x));

	if(slot < x[0])
		return (x[slot + 1] & m) != 0;
	return 0;
}

/* Compute x |= (1 << bit). */
void v_bignum_bit_set(VBigDig *x, unsigned int bit)
{
	unsigned int	slot, m;

	if(bit >= (*x * (CHAR_BIT * sizeof *x)))
		return;
	slot = bit / (CHAR_BIT * sizeof *x);
	m    = 1 << (bit % (CHAR_BIT * sizeof *x));
	x[1 + slot] |= m;
}

/* Returns index of most signifant '1' bit of x, or -1 if x == 0. */
int v_bignum_bit_msb(const VBigDig *x)
{
	int		i;
	unsigned int	s = *x++;

	for(i = s - 1; i >= 0; i--)
	{
		if(x[i] != 0)
		{
			int	bit = (i + 1) * (CHAR_BIT * sizeof *x) - 1;
			VBigDig	d = x[i], mask;

			for(mask = 1 << (CHAR_BIT * sizeof *x - 1); mask != 0; mask >>= 1, bit--)
			{
				if(d & mask)
					return bit;
			}
		}
	}
	return -1;
}

int v_bignum_bit_size(const VBigDig *x)
{
	return *x * V_BIGBITS;
}

/* Perform x <<= count. */
void v_bignum_bit_shift_left(VBigDig *x, unsigned int count)
{
	unsigned int	t, carry, s = *x++;
	register int	i;

	if(count >= CHAR_BIT * sizeof *x)	/* Shift whole digits. */
	{
		unsigned int	places = count / (CHAR_BIT * sizeof *x);

		for(i = s - 1; i >= (int) places; i--)
			x[i] = x[i - places];
		for(; i >= 0; i--)		/* Clear out the LSBs. */
			x[i] = 0;
		count -= places * (CHAR_BIT * sizeof *x);
		if(count == 0)
			return;
	}
	/* Shift bits. */
	for(i = carry = 0; i < (int) s; i++)
	{
		t = (x[i] << count) | carry;
		x[i] = t;
		carry = t >> (CHAR_BIT * sizeof *x);
	}
}

/* Perform x <<= 1. This is a frequent operation so it can have its own function. */
void v_bignum_bit_shift_left_1(VBigDig *x)
{
	register unsigned int	t, carry, s = *x++, i;

	/* Shift bits. */
	for(i = carry = 0; i < s; i++)
	{
		t = (x[i] << 1) | carry;
		x[i] = t;
		carry = t >> (CHAR_BIT * sizeof *x);
	}
}

/* Perform x >>= count. */
void v_bignum_bit_shift_right(VBigDig *x, unsigned int count)
{
	unsigned int	t, carry, s = *x++;
	int	i;

	/* Shift entire digits first. */
	if(count >= CHAR_BIT * sizeof *x)
	{
		unsigned int	places = count / (CHAR_BIT * sizeof *x);

		if(places > s)
		{
			memset(x, 0, s * sizeof *x);
			return;
		}
		for(i = 0; i < (int) (s - places); i++)
			x[i] = x[i + places];
		for(; i < (int) s; i++)
			x[i] = 0;
		count -= places * CHAR_BIT * sizeof *x;
		if(count == 0)
			return;
	}
	/* Shift any remaining bits. */
	for(i = s - 1, carry = 0; i >= 0; i--)
	{
		t = x[i] << (CHAR_BIT * sizeof *x);
		t >>= count;
		t |= carry;
		carry = (t & MAX_DIG) << (CHAR_BIT * sizeof *x);
		x[i] = t >> (CHAR_BIT * sizeof *x);
	}
}

/* ----------------------------------------------------------------------------------------- */

/* Return x == 0. */
int v_bignum_eq_zero(const VBigDig *x)
{
	unsigned int	i, s = *x++;

	for(i = 0; i < s; i++)
		if(x[i])
			return 0;
	return 1;
}

/* Return x == 1. */
int v_bignum_eq_one(const VBigDig *x)
{
	unsigned int	i, s = *x++;

	if(x[0] != 1)
		return 0;
	for(i = 1; i < s; i++)
		if(x[i])
			return 0;
	return 1;
}

/* Returns x == y, handling different lengths properly. */
int v_bignum_eq(const VBigDig *x, const VBigDig *y)
{
	unsigned int	i, xs, ys, cs;

	if(x == y)		/* Quick test thanks to pointer representation. */
		return 1;
	xs = *x++;
	ys = *y++;

	if(xs == ys)		/* Same size? Then let's be quick about this. */
		return memcmp(x, y, xs * sizeof *x) == 0;
	else
	{
		cs = xs < ys ? xs : ys;		/* Common size. */
		if(memcmp(x, y, cs * sizeof *x) == 0)
		{
			const VBigDig	*l;

			if(cs == xs)		/* y is longer. */
				l = y, i = ys - 1;
			else
				l = x, i = xs - 1;
			for(; i > cs; i--)
				if(l[i])
					return 0;
			return 1;
		}
	}
	return 0;
}

/* Returns x >= y. */
int v_bignum_gte(const VBigDig *x, const VBigDig *y)
{
	unsigned int	xs, ys;
	int		i, j, k;

	if(x == y)
		return 1;
	/* Find indexes of highest-most used digit in each of the numbers. */
	xs = *x++;
	ys = *y++;
	for(i = xs - 1; i >= 0; i--)
		if(x[i] != 0)
			break;
	for(j = ys - 1; j >= 0; j--)
		if(y[j] != 0)
			break;
	/* Both zero? */
	if(i < 0 && j < 0)
		return 1;
	/* Quick answers exists for different-sized numbers. Find them. */
	if(i < j)
		return 0;
	if(i > j)
		return 1;
	/* Compare digit by digit. */
	for(k = i; k >= 0; k--)
	{
		if(x[k] < y[k])
			return 0;
		if(x[k] > y[k])
			return 1;
	}
	return x[k] >= y[k];
}

/* ----------------------------------------------------------------------------------------- */

/* Computes x += y. */
void v_bignum_add_digit(VBigDig *x, VBigDig y)
{
	unsigned int	i, s = *x++, t;

	t = x[0] + y;
	x[0] = t;
	if(t > MAX_DIG)
	{
		for(i = 1; i < s; i++)
		{
			if(++x[i])
				break;
		}
	}
}

/* Computes x -= y. */
void v_bignum_sub_digit(VBigDig *x, VBigDig y)
{
	unsigned int	i, s = *x++, t;

	t = x[0] - y;
	x[0] = t;
	if(t > MAX_DIG)
	{
		for(i = 1; i < s; i++)
		{
			x[i]--;
			if(x[i] < MAX_DIG)
				break;
		}
	}
}

/* Computes x *= y. */
void v_bignum_mul_digit(VBigDig *x, VBigDig y)
{
	unsigned int	i, s = *x++, carry, t;

	for(i = carry = 0; i < s; i++)
	{
		t = x[i] * y + carry;
		x[i] = t;
		carry = t >> (CHAR_BIT * sizeof *x);
	}
}

/* ----------------------------------------------------------------------------------------- */

/* Computes x += y. */
void v_bignum_add(VBigDig *x, const VBigDig *y)
{
	unsigned int	i, xs = *x++, ys = *y++, s, carry, t;

	s = xs < ys ? xs : ys;
	for(i = carry = 0; i < s; i++)
	{
		t = x[i] + y[i] + carry;
		x[i] = t;
		carry = t > MAX_DIG;
	}
	for(; carry && i < xs; i++)
	{
		t = x[i] + carry;
		x[i] = t;
		carry = t > MAX_DIG;
	}
}

/* Computes x -= y. */
void v_bignum_sub(VBigDig *x, const VBigDig *y)
{
	unsigned int	i, xs = *x++, ys = *y++, s, carry, t;

	if(x == y)
	{
		v_bignum_set_zero(x - 1);
		return;
	}
	s = xs < ys ? xs : ys;
	for(i = carry = 0; i < s; i++)
	{
		t = x[i] - y[i] - carry;
		x[i] = t;
		carry = t > MAX_DIG;
	}
	for(; carry && i < xs; i++)
	{
		t = x[i] - carry;
		x[i] = t;
		carry = t > MAX_DIG;
	}
}

/* Compute x *= y, using as many digits as is necessary, then truncating the
 * result down. This is Algorithm 14.12 from "Handbook of Applied Cryptography".
*/
void v_bignum_mul(VBigDig *x, const VBigDig *y)
{
	int		n = *x, t = *y, i, j;
	VBigDigs	uv = 0, c, w[2048];

	memset(w, 0, (n + t + 1) * sizeof *w);
	for(i = 0; i < t; i++)
	{
		c = 0;
		for(j = 0; j < n; j++)
		{
			uv = w[i + j] + x[1 + j] * y[1 + i] + c;
			w[i + j] = uv & ((1 << V_BIGBITS) - 1);
			c = uv >> V_BIGBITS;
		}
		w[i + n + 1] = uv >> V_BIGBITS;
	}
	/* Write low words of w back into x. */
	for(i = 0; i < *x; i++)
		x[1 + i] = w[i];
}

/* Computes x /= y and remainder = x % y. */
void v_bignum_div(VBigDig *x, const VBigDig *y, VBigDig *remainder)
{
	VBigDig	*q, *work;
	int	msbx = v_bignum_bit_msb(x), msby = v_bignum_bit_msb(y), next;

	/* Compare magnitudes of inputs, allows quick exits. */
	if(msby > msbx)
	{
		if(remainder != NULL)
			v_bignum_set_bignum(remainder, x);
		v_bignum_set_zero(x);
		return;
	}
	if(msby < 0)
	{
		v_bignum_set_zero(x);
		return;
	}
	q = bignum_alloc(*x);
	v_bignum_set_zero(q);
	work = bignum_alloc(*x);
	v_bignum_set_bignum_part(work, x, msbx, msby + 1);

	for(next = msbx - (msby + 1); next >= -1; next--)
	{
		v_bignum_bit_shift_left_1(q);
		if(v_bignum_gte(work, y))
		{
			q[1] |= 1;
			v_bignum_sub(work, y);
		}
		v_bignum_bit_shift_left_1(work);
		if(v_bignum_bit_test(x, next))
			work[1] |= 1;
	}
	v_bignum_bit_shift_right(work, 1);	/* Undo the last shift (when next==-1). */
	
	if(remainder != NULL)
	{
/*		printf("div() got remainder ");
		v_bignum_print_hex_lf(work);
*/
		v_bignum_set_bignum(remainder, work);
	}
	bignum_free(work);
	v_bignum_set_bignum(x, q);
	bignum_free(q);
}

/* Computes x %= y. */
void v_bignum_mod(VBigDig *x, const VBigDig *y)
{
	int	digs;
	VBigDig	*tmp;

/*	printf("computing ");
	v_bignum_print_hex(x);
	printf("L %% ");
	v_bignum_print_hex(y);
*/
	digs = *x > *y ? *x : *y;
	tmp = bignum_alloc(digs);
	v_bignum_div(x, y, tmp);
	v_bignum_set_bignum(x, tmp);
	bignum_free(tmp);
/*	printf("L = ");
	v_bignum_print_hex_lf(x);
*/
}

/* Initialize Barrett reduction by computing the "mu" helper value. Defined in
 * Handbook of Applied Cryptography algorithm 14.42 as floor(b^2k / m).
*/
const VBigDig * v_bignum_reduce_begin(const VBigDig *m)
{
	VBigDig	*mu;
	int	k;

	for(k = *m; m[k] == 0; k--)
		;
/*	printf("k=%d -> digits are 0..%u\n", k, k - 1);
	printf("computing mu=floor(65536^%d/", 2 * k);
	v_bignum_print_hex(m);
	printf(")\n");
*/	mu = bignum_alloc(2 * k + 1);
	/* b ^ 2k is just 65536 << 2k, i.e. set bit 16 * 2k. */
	v_bignum_set_zero(mu);
	v_bignum_bit_set(mu, V_BIGBITS * 2 * k);
/*	v_bignum_print_hex_lf(mu);*/
	v_bignum_div(mu, m, NULL);

	return mu;
}

void v_bignum_reduce_end(const VBigDig *mu)
{
	bignum_free(mu);
}

/* Compute x % m, using mu as the helper quantity mu, precomputed by the
 * routine above.
*/
void v_bignum_reduce(VBigDig *x, const VBigDig *m, const VBigDig *mu)
{
	VBigDig	*q, *r1, *r2, *r;
	int	i, k;

	for(k = *m; m[k] == 0; k--)
		;
	/* Step 1, compute the q helper. */
	q = bignum_alloc(*x + *mu - (k - 1));	/* Tighter bound number length (was 2 * *x). */
	v_bignum_set_bignum(q, x);
	v_bignum_bit_shift_right(q, V_BIGBITS * (k - 1));
	v_bignum_mul(q, mu);
	v_bignum_bit_shift_right(q, V_BIGBITS * (k + 1));

	/* Step 2, initialize. */
	r1 = bignum_alloc(*x);
	r2 = bignum_alloc(*x);
	v_bignum_set_bignum(r1, x);
	for(i = k + 1; i < *r1; i++)
		r1[i + 1] = 0;
	v_bignum_set_bignum(r2, q);
	v_bignum_mul(r2, m);
	for(i = k + 1; i < *r2; i++)
		r2[i + 1] = 0;
	r = x;
	v_bignum_set_bignum(r, r1);
	v_bignum_sub(r, r2);
	/* Step 3, make sure r is positive. */
	if(v_bignum_bit_test(r, V_BIGBITS * *r - 1))
	{
		VBigDig	*term;
		
		term = bignum_alloc(k + 1 * V_BIGBITS);
		v_bignum_set_zero(term);
		v_bignum_bit_set(term, V_BIGBITS * (k + 1));
		v_bignum_add(r, term);
		bignum_free(term);
	}
	/* Step 4, loop. */
	while(v_bignum_gte(r, m))
		v_bignum_sub(r, m);

	bignum_free(r2);
	bignum_free(r1);
	bignum_free(q);
}

/* Compute x * x using the algorithm 14.16 from "Handbook of Applied Cryptography".
 * Note that since 'w' needs to be double-precision (i.e., 32-bit), we cannot allocate
 * it using bignum_alloc() cleanly. Thus the static limit, which should be enough here.
 * NOTE: This very much assumes V_BIGBITS == 16.
*/
void v_bignum_square_half(VBigDig *x)
{
	VBigDigs	w[256], uv, c, ouv;
	int		t = *x / 2, i, j, high;

	if(t == 0)
		return;
	for(; x[t] == 0; t--)
		;
	memset(w, 0, 2 * t * sizeof *w);	/* Clear digits of w. */
/*	printf("print %lu, ", ++count);
	v_bignum_print_hex(x);
	printf("*");
	v_bignum_print_hex(x);
*/	for(i = 0; i < t; i++)
	{
/*		printf("computing w[%d]: %lX + %lX * %lX\n", 2 * i, w[2 * i], x[1 + i], x[1 + i]);*/
		uv = w[2 * i] + x[1 + i] * x[1 + i];
/*		printf("setting w[%d]=%X [before]\n", 2 * i, uv & 0xffff);*/
		w[2 * i] = uv & 0xffff;
		c = uv >> V_BIGBITS;
/*		printf("uv before=%X, c=%X\n", uv, c);*/
		high = 0;
		for(j = i + 1; j < t; j++)
		{
/*			printf("computing uv=%X+2*%X*%X+%X\n", w[i + j], x[1 + j], x[1 + i], c);*/
			uv = x[1 + j] * x[1 + i];
			high = (uv & 0x80000000) != 0;
			uv *= 2;
			ouv = uv;	/* Addition below might wrap and generate high bit. */
			uv += w[i + j] + c;
/*			printf("ouv=0x%lX uv=0x%lX\n", ouv, uv);*/
			high |= uv < ouv;
/*			printf("setting w[%d]=%lX [inner] uv=%lX high=%d c=%X\n", i + j, uv & 0xffff, uv, high, c);*/
			w[i + j] = uv & 0xffff;
			c = (uv >> V_BIGBITS) | (high << V_BIGBITS);
		}
/*		printf("setting w[%d] to %X [after]\n", i + t, (uv >> 16) | (high << 16));*/
		w[i + t] = (uv >> V_BIGBITS) | (high << V_BIGBITS);
	}
/*	printf("w=0x");
	for(i = *x - 1; i >= 0; i--)
		printf("%04X.", w[i]);
	printf("\n");
*/	/* Write low words of w back into x, trashing it with the square. */
	for(i = 0; i < 2 * t; i++)
		x[1 + i] = w[i];
	for(; i < *x; i++)
		x[1 + i] = 0;
/*	printf("==");
	v_bignum_print_hex_lf(x);
*/
}

/* Computes x = (x^y) % n, where ^ denotes exponentiation. */
void v_bignum_pow_mod(VBigDig *x, const VBigDig *y, const VBigDig *n)
{
	VBigDig		*tmp;
	const VBigDig	*mu;
	int		i, k;

/*	printf("computing pow(");
	v_bignum_print_hex(x);
	printf("L,");
	v_bignum_print_hex(y);
	printf("L,");
	v_bignum_print_hex(n);
	printf("L)\n");
*/
	tmp = bignum_alloc(2 * *x);	/* Squaring needs twice the bits, or lossage occurs. */
	v_bignum_set_bignum(tmp, x);
	k = v_bignum_bit_msb(y);
	mu = v_bignum_reduce_begin(n);
	for(i = k - 1; i >= 0; i--)
	{
		v_bignum_square_half(tmp);
		v_bignum_reduce(tmp, n, mu);
		if(v_bignum_bit_test(y, i))
		{
			v_bignum_mul(tmp, x);
			v_bignum_reduce(tmp, n, mu);
		}
	}
	v_bignum_set_bignum(x, tmp);
	v_bignum_reduce_end(mu);
	bignum_free(tmp);
}

/* ----------------------------------------------------------------------------------------- */

#if defined STANDALONE

int main(void)
{
	VBigDig	VBIGNUM(x, 3648), VBIGNUM(y, 128), VBIGNUM(z, 128);

	printf("MAX_DIG=%u\n", MAX_DIG);
	
	v_bignum_set_string_hex(x, "0x433864FE0F8FAC180FF1BC3A5BFD0C5566F6B11679E27294EDCC43056EB73EE118415E0CD6E6519509476EB21341ED0328BA7B14E0ED80D5E100A4549C5202B57B4CF17A74987631B6BA896C0DBA2095A7EDE5B9C4B4EEFCD1B9EF8474BCB7FBD0F64B549625D444847ED1FCB7F8050EB4F22794F694A0FAC6DFFB781C264B227966840185F9216484F6A7954741CB11FC14DEC2937EAD2CE640FD9A4339706BDB5BC355079C2F2F7994669DFA5B20C50D957A676E67C86835037078323A0BDAD3686B8E638749F327A7AD433C0D18BCD2FC970D125914C7FBEE061290A0F0F3572E207");
	v_bignum_set_bignum(y, x);
	v_bignum_set_digit(z, 77);

	printf("x:");
	v_bignum_print_hex_lf(x);
	printf("y:");
	v_bignum_print_hex_lf(y);
	printf("r:");
	v_bignum_print_hex_lf(z);
	v_bignum_pow_mod(x, y, z);
	printf(" =");
	v_bignum_print_hex_lf(x);

	return 0;
}

#endif		/* STANDALONE */

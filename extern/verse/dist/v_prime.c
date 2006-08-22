/*
 * Program to generate primes of the form p = 2 * q + 1,
 * where p and q are both primes.
 *
 * Originally written by Pontus Nyman <f97-pny@nada.kth.se>,
 * ported to Verse's bignums and rewritten from scratch by
 * Emil Brink.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "v_bignum.h"
#include "v_encryption.h"
#include "verse_header.h"

#define	BITS		V_ENCRYPTION_LOGIN_KEY_BITS	/* Save some typing. */

#define CYCLES	10	/* Number of times to apply Miller-Rabin test. */

/* Test divisibility of <n> against table of small known primes. Returns 1 if n looks prime, 0 if it IS not. */
static int quick_filter(const VBigDig *n)
{
	VBigDig VBIGNUM(m, 16), VBIGNUM(tmp, BITS / 2);
	const unsigned int	prime[] = { 3, 5, 7, 11, 13, 17, 19, 23, 39, 31, 37, 41, 43, 47, 53 };
	unsigned int	i;

	for(i = 0; i < sizeof prime / sizeof *prime; i++)
	{
		v_bignum_set_bignum(tmp, n);
		v_bignum_set_digit(m, prime[i]);
		v_bignum_mod(tmp, m);
		if(v_bignum_eq_zero(tmp))
			return 0;
	}
	return 1;
}

/* The Miller-Rabin primality test. Returns 1 if the candidate looks prime, 0 if
 * it IS NOT prime. Assumes that n is BITS / 2 bits, so that its square fits in BITS.
*/
static int miller_rabin(const VBigDig *n, VRandGen *gen)
{
	int		i, k;
	VBigDig		VBIGNUM(a, BITS / 2), VBIGNUM(d, BITS), VBIGNUM(nmo, BITS / 2), VBIGNUM(x, BITS);
	const VBigDig	*mu;

	mu = v_bignum_reduce_begin(n);

	/* Pick a "witness", a number in the [1, n) range. */
	v_bignum_set_random(a, gen);
	v_bignum_reduce(a, n, mu);

	v_bignum_set_one(d);
	v_bignum_set_bignum(nmo, n);
	v_bignum_sub_digit(nmo, 1);	/* nmo = n - 1 (say it). */
	k = v_bignum_bit_msb(nmo);
	for(i = k; i >= 0; i--)
	{
		v_bignum_set_bignum(x, d);
		v_bignum_square_half(d);
		v_bignum_reduce(d, n, mu);
		if(v_bignum_eq_one(d) && !v_bignum_eq_one(x) && !v_bignum_eq(x, nmo))
		{
			v_bignum_reduce_end(mu);
			return 0;	/* Composite found. */
		}
		if(v_bignum_bit_test(nmo, i))
		{
			v_bignum_mul(d, a);
			v_bignum_reduce(d, n, mu);
		}
	}
	v_bignum_reduce_end(mu);
	return v_bignum_eq_one(d);	/* It might be prime. */
}

/* Test q for primality, returning 1 if it seems prime, 0 if it certainly IS not. */
int v_prime_test(const VBigDig *q, VRandGen *gen)
{
	int	i;

	if(!quick_filter(q))
		return 0;

	for(i = 0; i < CYCLES; i++)
	{
		if(!miller_rabin(q, gen))
			return 0;
	}
	return 1;
}

void v_prime_set_random(VBigDig *x)
{
	int		bits = v_bignum_bit_size(x);
	VRandGen	*gen;

	gen = v_randgen_new();
	do
	{
		/* Create candidate, making sure it's both odd and non-zero. */
		v_bignum_set_random(x, gen);
		/* Set topmost two bits, makes sure products are big. */
		v_bignum_bit_set(x, bits - 1);
		v_bignum_bit_set(x, bits - 2);
		/* Set lowermost bit, makes sure it is odd (better prime candidate that way). */
		v_bignum_bit_set(x, 0);
	} while(!v_prime_test(x, gen));
/*	printf("Prime found after %d iterations: ", count);
	v_bignum_print_hex_lf(x);
*/
	v_randgen_destroy(gen);
}

/* Big (small?) primes from <http://www.utm.edu/research/primes/lists/small/small3.html#300>. */
void v_prime_set_table(VBigDig *x, unsigned int i)
{
	if(i == 0)
		v_bignum_set_string_hex(x, "0xCBC2C5536E3D6283FDAF36B1D0F91C3EAAB1D12892B961B866907930F6471851");
	else if(i == 1)
		v_bignum_set_string_hex(x, "0xC14F93E7A1543BD57C1DFBE98C29F9E4C13077FD27A0FEC05CCBC913CD213F19");
	else
		v_bignum_set_string(x, "65537");	/* It ain't big, but it's prime. */
}

#if PRIMEALONE
#include <sys/time.h>

#define	REPS	300

static double elapsed(const struct timeval *t1, const struct timeval *t2)
{
	return t2->tv_sec - t1->tv_sec + 1E-6 * (t2->tv_usec - t1->tv_usec);
}

int main(void)
{
	struct timeval	now, then;
	VBigDig	VBIGNUM(x, BITS / 2);
	int	i;

	srand(clock());

/*	gettimeofday(&then, NULL);
	for(i = 0; i < REPS; i++)
	{
		v_prime_set_random_incr(x);
	}
	gettimeofday(&now, NULL);
	printf("incr: %g\n", elapsed(&then, &now));
*/
	gettimeofday(&then, NULL);
	for(i = 0; i < REPS; i++)
	{
		v_prime_set_random(x);
	}
	gettimeofday(&now, NULL);
	printf("rand: %g\n", elapsed(&then, &now));

	return EXIT_SUCCESS;
}

#endif

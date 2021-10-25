/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Copyright (C) 1995 Software Foundation, Inc.
 *
 * Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>.
 */

/** \file blender/blenlib/intern/hash_md5.c
 *  \ingroup bli
 *
 *  Functions to compute MD5 message digest of files or memory blocks
 *  according to the definition of MD5 in RFC 1321 from April 1992.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

#include "BLI_hash_md5.h"  /* own include */

#if defined HAVE_LIMITS_H || defined _LIBC
#  include <limits.h>
#endif

/* The following contortions are an attempt to use the C preprocessor to determine an unsigned integral type
 * that is 32 bits wide. An alternative approach is to use autoconf's AC_CHECK_SIZEOF macro, but doing that
 * would require that the configure script compile and *run* the resulting executable.
 * Locally running cross-compiled executables is usually not possible.
 */

#if defined __STDC__ && __STDC__
#  define UINT_MAX_32_BITS 4294967295U
#else
#  define UINT_MAX_32_BITS 0xFFFFFFFF
#endif

/* If UINT_MAX isn't defined, assume it's a 32-bit type.
 * This should be valid for all systems GNU cares about because that doesn't include 16-bit systems,
 * and only modern systems (that certainly have <limits.h>) have 64+-bit integral types.
 */

#ifndef UINT_MAX
#  define UINT_MAX UINT_MAX_32_BITS
#endif

#if UINT_MAX == UINT_MAX_32_BITS
   typedef unsigned int md5_uint32;
#else
#  if USHRT_MAX == UINT_MAX_32_BITS
     typedef unsigned short md5_uint32;
#  else
#    if ULONG_MAX == UINT_MAX_32_BITS
       typedef unsigned long md5_uint32;
#    else
       /* The following line is intended to evoke an error. Using #error is not portable enough. */
       "Cannot determine unsigned 32-bit data type."
#    endif
#  endif
#endif


/* Following code is low level, upon which are built up the functions
 * 'BLI_hash_md5_stream' and 'BLI_hash_md5_buffer'. */

/* Structure to save state of computation between the single steps. */
struct md5_ctx {
	md5_uint32 A;
	md5_uint32 B;
	md5_uint32 C;
	md5_uint32 D;
};

#ifdef __BIG_ENDIAN__
#  define SWAP(n) (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
#  define SWAP(n) (n)
#endif

/* This array contains the bytes used to pad the buffer to the next 64-byte boundary.  (RFC 1321, 3.1: Step 1) */
static const unsigned char fillbuf[64] = {0x80, 0 /* , 0, 0, ...  */};

/** Initialize structure containing state of computation.
 *  (RFC 1321, 3.3: Step 3)
 */
static void md5_init_ctx(struct md5_ctx *ctx)
{
	ctx->A = 0x67452301;
	ctx->B = 0xefcdab89;
	ctx->C = 0x98badcfe;
	ctx->D = 0x10325476;
}

/** Starting with the result of former calls of this function (or the initialization), this function updates
 *  the 'ctx' context for the next 'len' bytes starting at 'buffer'.
 *  It is necessary that 'len' is a multiple of 64!!!
 */
static void md5_process_block(const void *buffer, size_t len, struct md5_ctx *ctx)
{
/* These are the four functions used in the four steps of the MD5 algorithm and defined in the RFC 1321.
 * The first function is a little bit optimized (as found in Colin Plumbs public domain implementation).
 */
/* #define FF(b, c, d) ((b & c) | (~b & d)) */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/* It is unfortunate that C does not provide an operator for cyclic rotation.  Hope the C compiler is smart enough. */
#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

	md5_uint32 correct_words[16];
	const md5_uint32 *words = buffer;
	size_t nwords = len / sizeof(md5_uint32);
	const md5_uint32 *endp = words + nwords;
	md5_uint32 A = ctx->A;
	md5_uint32 B = ctx->B;
	md5_uint32 C = ctx->C;
	md5_uint32 D = ctx->D;

	/* Process all bytes in the buffer with 64 bytes in each round of the loop.  */
	while (words < endp) {
		md5_uint32 *cwp = correct_words;
		md5_uint32 A_save = A;
		md5_uint32 B_save = B;
		md5_uint32 C_save = C;
		md5_uint32 D_save = D;

		/* First round: using the given function, the context and a constant the next context is computed.
		 * Because the algorithms processing unit is a 32-bit word and it is determined to work on words in
		 * little endian byte order we perhaps have to change the byte order before the computation.
		 * To reduce the work for the next steps we store the swapped words in the array CORRECT_WORDS.
		 */
#define OP(a, b, c, d, s, T)                                   \
		a += FF(b, c, d) + (*cwp++ = SWAP(*words)) + T;        \
		++words;                                               \
		CYCLIC(a, s);                                          \
		a += b;                                                \
		(void)0

		/* Before we start, one word to the strange constants. They are defined in RFC 1321 as:
		 *     T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
		 */

		/* Round 1.  */
		OP(A, B, C, D,  7, 0xd76aa478);
		OP(D, A, B, C, 12, 0xe8c7b756);
		OP(C, D, A, B, 17, 0x242070db);
		OP(B, C, D, A, 22, 0xc1bdceee);
		OP(A, B, C, D,  7, 0xf57c0faf);
		OP(D, A, B, C, 12, 0x4787c62a);
		OP(C, D, A, B, 17, 0xa8304613);
		OP(B, C, D, A, 22, 0xfd469501);
		OP(A, B, C, D,  7, 0x698098d8);
		OP(D, A, B, C, 12, 0x8b44f7af);
		OP(C, D, A, B, 17, 0xffff5bb1);
		OP(B, C, D, A, 22, 0x895cd7be);
		OP(A, B, C, D,  7, 0x6b901122);
		OP(D, A, B, C, 12, 0xfd987193);
		OP(C, D, A, B, 17, 0xa679438e);
		OP(B, C, D, A, 22, 0x49b40821);

#undef OP

		/* For the second to fourth round we have the possibly swapped words in CORRECT_WORDS.
		 * Redefine the macro to take an additional first argument specifying the function to use.
		 */
#define OP(f, a, b, c, d, k, s, T)                             \
		a += f(b, c, d) + correct_words[k] + T;                \
		CYCLIC(a, s);                                          \
		a += b;                                                \
		(void)0

		/* Round 2.  */
		OP(FG, A, B, C, D,  1,  5, 0xf61e2562);
		OP(FG, D, A, B, C,  6,  9, 0xc040b340);
		OP(FG, C, D, A, B, 11, 14, 0x265e5a51);
		OP(FG, B, C, D, A,  0, 20, 0xe9b6c7aa);
		OP(FG, A, B, C, D,  5,  5, 0xd62f105d);
		OP(FG, D, A, B, C, 10,  9, 0x02441453);
		OP(FG, C, D, A, B, 15, 14, 0xd8a1e681);
		OP(FG, B, C, D, A,  4, 20, 0xe7d3fbc8);
		OP(FG, A, B, C, D,  9,  5, 0x21e1cde6);
		OP(FG, D, A, B, C, 14,  9, 0xc33707d6);
		OP(FG, C, D, A, B,  3, 14, 0xf4d50d87);
		OP(FG, B, C, D, A,  8, 20, 0x455a14ed);
		OP(FG, A, B, C, D, 13,  5, 0xa9e3e905);
		OP(FG, D, A, B, C,  2,  9, 0xfcefa3f8);
		OP(FG, C, D, A, B,  7, 14, 0x676f02d9);
		OP(FG, B, C, D, A, 12, 20, 0x8d2a4c8a);

		/* Round 3.  */
		OP(FH, A, B, C, D,  5,  4, 0xfffa3942);
		OP(FH, D, A, B, C,  8, 11, 0x8771f681);
		OP(FH, C, D, A, B, 11, 16, 0x6d9d6122);
		OP(FH, B, C, D, A, 14, 23, 0xfde5380c);
		OP(FH, A, B, C, D,  1,  4, 0xa4beea44);
		OP(FH, D, A, B, C,  4, 11, 0x4bdecfa9);
		OP(FH, C, D, A, B,  7, 16, 0xf6bb4b60);
		OP(FH, B, C, D, A, 10, 23, 0xbebfbc70);
		OP(FH, A, B, C, D, 13,  4, 0x289b7ec6);
		OP(FH, D, A, B, C,  0, 11, 0xeaa127fa);
		OP(FH, C, D, A, B,  3, 16, 0xd4ef3085);
		OP(FH, B, C, D, A,  6, 23, 0x04881d05);
		OP(FH, A, B, C, D,  9,  4, 0xd9d4d039);
		OP(FH, D, A, B, C, 12, 11, 0xe6db99e5);
		OP(FH, C, D, A, B, 15, 16, 0x1fa27cf8);
		OP(FH, B, C, D, A,  2, 23, 0xc4ac5665);

		/* Round 4.  */
		OP(FI, A, B, C, D,  0,  6, 0xf4292244);
		OP(FI, D, A, B, C,  7, 10, 0x432aff97);
		OP(FI, C, D, A, B, 14, 15, 0xab9423a7);
		OP(FI, B, C, D, A,  5, 21, 0xfc93a039);
		OP(FI, A, B, C, D, 12,  6, 0x655b59c3);
		OP(FI, D, A, B, C,  3, 10, 0x8f0ccc92);
		OP(FI, C, D, A, B, 10, 15, 0xffeff47d);
		OP(FI, B, C, D, A,  1, 21, 0x85845dd1);
		OP(FI, A, B, C, D,  8,  6, 0x6fa87e4f);
		OP(FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
		OP(FI, C, D, A, B,  6, 15, 0xa3014314);
		OP(FI, B, C, D, A, 13, 21, 0x4e0811a1);
		OP(FI, A, B, C, D,  4,  6, 0xf7537e82);
		OP(FI, D, A, B, C, 11, 10, 0xbd3af235);
		OP(FI, C, D, A, B,  2, 15, 0x2ad7d2bb);
		OP(FI, B, C, D, A,  9, 21, 0xeb86d391);

#undef OP

		/* Add the starting values of the context.  */
		A += A_save;
		B += B_save;
		C += C_save;
		D += D_save;
	}

	/* Put checksum in context given as argument.  */
	ctx->A = A;
	ctx->B = B;
	ctx->C = C;
	ctx->D = D;

#undef FF
#undef FG
#undef FH
#undef FI
#undef CYCLIC
}

/** Put result from 'ctx' in first 16 bytes of 'resbuf'. The result is always in little endian byte order,
 *  so that a byte-wise output yields to the wanted ASCII representation of the message digest.
 */
static void *md5_read_ctx(const struct md5_ctx *ctx, void *resbuf)
{
	md5_uint32 *digest = resbuf;
	digest[0] = SWAP(ctx->A);
	digest[1] = SWAP(ctx->B);
	digest[2] = SWAP(ctx->C);
	digest[3] = SWAP(ctx->D);

	return resbuf;
}

/* Top level public functions. */

/** Compute MD5 message digest for bytes read from 'stream'.
 *  The resulting message digest number will be written into the 16 bytes beginning at 'resblock'.
 *  \return Non-zero if an error occurred.
 */
int BLI_hash_md5_stream(FILE *stream, void *resblock)
{
#define BLOCKSIZE 4096  /* Important: must be a multiple of 64. */
	struct md5_ctx ctx;
	md5_uint32 len[2];
	char buffer[BLOCKSIZE + 72];
	size_t pad, sum;

	/* Initialize the computation context. */
	md5_init_ctx(&ctx);

	len[0] = 0;
	len[1] = 0;

	/* Iterate over full file contents. */
	while (1) {
		/* We read the file in blocks of BLOCKSIZE bytes. One call of the computation function processes
		 * the whole buffer so that with the next round of the loop another block can be read.
		 */
		size_t n;
		sum = 0;

		/* Read block. Take care for partial reads. */
		do {
			n = fread(buffer, 1, BLOCKSIZE - sum, stream);
			sum += n;
		} while (sum < BLOCKSIZE && n != 0);

		if (n == 0 && ferror(stream))
			return 1;

		/* RFC 1321 specifies the possible length of the file up to 2^64 bits.
		 * Here we only compute the number of bytes. Do a double word increment.
		 */
		len[0] += sum;
		if (len[0] < sum)
			++len[1];

		/* If end of file is reached, end the loop.  */
		if (n == 0)
			break;

		/* Process buffer with BLOCKSIZE bytes. Note that BLOCKSIZE % 64 == 0. */
		md5_process_block(buffer, BLOCKSIZE, &ctx);
	}

	/* We can copy 64 bytes because the buffer is always big enough. 'fillbuf' contains the needed bits. */
	memcpy(&buffer[sum], fillbuf, 64);

	/* Compute amount of padding bytes needed. Alignment is done to (N + PAD) % 64 == 56.
	 * There is always at least one byte padded, i.e. if the alignment is correctly aligned,
	 * 64 padding bytes are added.
	 */
	pad = sum & 63;
	pad = pad >= 56 ? 64 + 56 - pad : 56 - pad;

	/* Put the 64-bit file length in *bits* at the end of the buffer. */
	*(md5_uint32 *) &buffer[sum + pad] = SWAP(len[0] << 3);
	*(md5_uint32 *) &buffer[sum + pad + 4] = SWAP((len[1] << 3) | (len[0] >> 29));

	/* Process last bytes.  */
	md5_process_block(buffer, sum + pad + 8, &ctx);

	/* Construct result in desired memory.  */
	md5_read_ctx(&ctx, resblock);
	return 0;
}

/** Compute MD5 message digest for 'len' bytes beginning at 'buffer'.
 *  The result is always in little endian byte order, so that a byte-wise output yields to the wanted
 *  ASCII representation of the message digest.
 */
void *BLI_hash_md5_buffer(const char *buffer, size_t len, void *resblock)
{
	struct md5_ctx ctx;
	char restbuf[64 + 72];
	size_t blocks = len & ~63;
	size_t pad, rest;

	/* Initialize the computation context.  */
	md5_init_ctx(&ctx);

	/* Process whole buffer but last len % 64 bytes.  */
	md5_process_block(buffer, blocks, &ctx);

	/* REST bytes are not processed yet.  */
	rest = len - blocks;
	/* Copy to own buffer.  */
	memcpy(restbuf, &buffer[blocks], rest);
	/* Append needed fill bytes at end of buffer. We can copy 64 bytes because the buffer is always big enough. */
	memcpy(&restbuf[rest], fillbuf, 64);

	/* PAD bytes are used for padding to correct alignment. Note that always at least one byte is padded. */
	pad = rest >= 56 ? 64 + 56 - rest : 56 - rest;

	/* Put length of buffer in *bits* in last eight bytes. */
	*(md5_uint32 *) &restbuf[rest + pad] = (md5_uint32) SWAP(len << 3);
	*(md5_uint32 *) &restbuf[rest + pad + 4] = (md5_uint32) SWAP(len >> 29);

	/* Process last bytes. */
	md5_process_block(restbuf, rest + pad + 8, &ctx);

	/* Put result in desired memory area. */
	return md5_read_ctx(&ctx, resblock);
}

char *BLI_hash_md5_to_hexdigest(void *resblock, char r_hex_digest[33])
{
	static const char hex_map[17] = "0123456789abcdef";
	const unsigned char *p;
	char *q;
	short len;

	for (q = r_hex_digest, p = (const unsigned char *)resblock, len = 0; len < 16; ++p, ++len) {
		const unsigned char c = *p;
		*q++ = hex_map[c >> 4];
		*q++ = hex_map[c & 15];
	}
	*q = '\0';

	return r_hex_digest;
}

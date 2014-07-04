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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * Code from gutf8.c Copyright (C) 1999 Tom Tromey
 *                   Copyright (C) 2000 Red Hat, Inc.
 * All rights reserved.
 *
 * Contributor(s): Campbell Barton.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

 /** \file blender/blenlib/intern/string_utf8.c
  *  \ingroup bli
  */

#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include <wcwidth.h>
#include <stdio.h>
#include <stdlib.h>

#include "BLI_utildefines.h"

#include "BLI_string_utf8.h"  /* own include */

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

// #define DEBUG_STRSIZE

/* from libswish3, originally called u8_isvalid(),
 * modified to return the index of the bad character (byte index not utf).
 * http://svn.swish-e.org/libswish3/trunk/src/libswish3/utf8.c r3044 - campbell */

/* based on the valid_utf8 routine from the PCRE library by Philip Hazel
 *
 * length is in bytes, since without knowing whether the string is valid
 * it's hard to know how many characters there are! */

static const char trailingBytesForUTF8[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
};

int BLI_utf8_invalid_byte(const char *str, int length)
{
	const unsigned char *p, *pend = (unsigned char *)str + length;
	unsigned char c;
	int ab;

	for (p = (unsigned char *)str; p < pend; p++) {
		c = *p;
		if (c < 128)
			continue;
		if ((c & 0xc0) != 0xc0)
			goto utf8_error;
		ab = trailingBytesForUTF8[c];
		if (length < ab)
			goto utf8_error;
		length -= ab;

		p++;
		/* Check top bits in the second byte */
		if ((*p & 0xc0) != 0x80)
			goto utf8_error;

		/* Check for overlong sequences for each different length */
		switch (ab) {
			/* Check for xx00 000x */
		case 1:
			if ((c & 0x3e) == 0) goto utf8_error;
			continue;   /* We know there aren't any more bytes to check */

			/* Check for 1110 0000, xx0x xxxx */
		case 2:
			if (c == 0xe0 && (*p & 0x20) == 0) goto utf8_error;
			break;

			/* Check for 1111 0000, xx00 xxxx */
		case 3:
			if (c == 0xf0 && (*p & 0x30) == 0) goto utf8_error;
			break;

			/* Check for 1111 1000, xx00 0xxx */
		case 4:
			if (c == 0xf8 && (*p & 0x38) == 0) goto utf8_error;
			break;

			/* Check for leading 0xfe or 0xff,
			 * and then for 1111 1100, xx00 00xx */
		case 5:
			if (c == 0xfe || c == 0xff ||
			    (c == 0xfc && (*p & 0x3c) == 0)) goto utf8_error;
			break;
		}

		/* Check for valid bytes after the 2nd, if any; all must start 10 */
		while (--ab > 0) {
			if ((*(p + 1) & 0xc0) != 0x80) goto utf8_error;
			p++; /* do this after so we get usable offset - campbell */
		}
	}

	return -1;

utf8_error:

	return (int)((char *)p - (char *)str) - 1;
}

int BLI_utf8_invalid_strip(char *str, int length)
{
	int bad_char, tot = 0;

	BLI_assert(str[length] == '\0');

	while ((bad_char = BLI_utf8_invalid_byte(str, length)) != -1) {
		str += bad_char;
		length -= bad_char;

		if (length == 0) {
			/* last character bad, strip it */
			*str = '\0';
			tot++;
			break;
		}
		else {
			/* strip, keep looking */
			memmove(str, str + 1, (size_t)length);
			tot++;
		}
	}

	return tot;
}


/* compatible with BLI_strncpy, but esnure no partial utf8 chars */

/* array copied from glib's gutf8.c,
 * note: this looks to be at odd's with 'trailingBytesForUTF8',
 * need to find out what gives here! - campbell */
static const size_t utf8_skip_data[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1
};

#define BLI_STR_UTF8_CPY(dst, src, maxncpy)                                   \
	{                                                                         \
		size_t utf8_size;                                                     \
		while (*src != '\0' && (utf8_size = utf8_skip_data[*src]) < maxncpy) {\
			maxncpy -= utf8_size;                                             \
			switch (utf8_size) {                                              \
				case 6: *dst ++ = *src ++;                                    \
				case 5: *dst ++ = *src ++;                                    \
				case 4: *dst ++ = *src ++;                                    \
				case 3: *dst ++ = *src ++;                                    \
				case 2: *dst ++ = *src ++;                                    \
				case 1: *dst ++ = *src ++;                                    \
			}                                                                 \
		}                                                                     \
		*dst = '\0';                                                          \
	} (void)0

char *BLI_strncpy_utf8(char *__restrict dst, const char *__restrict src, size_t maxncpy)
{
	char *r_dst = dst;

	BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	/* note: currently we don't attempt to deal with invalid utf8 chars */
	BLI_STR_UTF8_CPY(dst, src, maxncpy);

	return r_dst;
}

char *BLI_strncat_utf8(char *__restrict dst, const char *__restrict src, size_t maxncpy)
{
	while (*dst && maxncpy > 0) {
		dst++;
		maxncpy--;
	}

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	BLI_STR_UTF8_CPY(dst, src, maxncpy);

	return dst;
}

#undef BLI_STR_UTF8_CPY

/* --------------------------------------------------------------------------*/
/* wchar_t / utf8 functions  */

size_t BLI_strncpy_wchar_as_utf8(char *__restrict dst, const wchar_t *__restrict src, const size_t maxncpy)
{
	const size_t maxlen = maxncpy - 1;
	size_t len = 0;

	BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	while (*src && len < maxlen) { /* XXX can still run over the buffer because utf8 size isn't known :| */
		len += BLI_str_utf8_from_unicode((unsigned int)*src++, dst + len);
	}

	dst[len] = '\0';

	return len;
}

/* wchar len in utf8 */
size_t BLI_wstrlen_utf8(const wchar_t *src)
{
	size_t len = 0;

	while (*src) {
		len += BLI_str_utf8_from_unicode((unsigned int)*src++, NULL);
	}

	return len;
}

size_t BLI_strlen_utf8_ex(const char *strc, size_t *r_len_bytes)
{
	size_t len;
	const char *strc_orig = strc;

	for (len = 0; *strc; len++)
		strc += BLI_str_utf8_size_safe(strc);

	*r_len_bytes = (size_t)(strc - strc_orig);
	return len;
}

size_t BLI_strlen_utf8(const char *strc)
{
	size_t len;

	for (len = 0; *strc; len++)
		strc += BLI_str_utf8_size_safe(strc);

	return len;
}

size_t BLI_strnlen_utf8_ex(const char *strc, const size_t maxlen, size_t *r_len_bytes)
{
	size_t len;
	const char *strc_orig = strc;
	const char *strc_end = strc + maxlen;

	for (len = 0; *strc && strc < strc_end; len++) {
		strc += BLI_str_utf8_size_safe(strc);
	}

	*r_len_bytes = (size_t)(strc - strc_orig);
	return len;
}

/**
 * \param start the string to measure the length.
 * \param maxlen the string length (in bytes)
 * \return the unicode length (not in bytes!)
 */
size_t BLI_strnlen_utf8(const char *strc, const size_t maxlen)
{
	size_t len;
	const char *strc_end = strc + maxlen;

	for (len = 0; *strc && strc < strc_end; len++) {
		strc += BLI_str_utf8_size_safe(strc);
	}

	return len;
}

size_t BLI_strncpy_wchar_from_utf8(wchar_t *__restrict dst_w, const char *__restrict src_c, const size_t maxncpy)
{
	const size_t maxlen = maxncpy - 1;
	size_t len = 0;

	BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
	memset(dst_w, 0xff, sizeof(*dst_w) * maxncpy);
#endif

	while (*src_c && len != maxlen) {
		size_t step = 0;
		unsigned int unicode = BLI_str_utf8_as_unicode_and_size(src_c, &step);
		if (unicode != BLI_UTF8_ERR) {
			*dst_w = (wchar_t)unicode;
			src_c += step;
		}
		else {
			*dst_w = '?';
			src_c = BLI_str_find_next_char_utf8(src_c, NULL);
		}
		dst_w++;
		len++;
	}

	*dst_w = 0;

	return len;
}

/* end wchar_t / utf8 functions  */
/* --------------------------------------------------------------------------*/

/* count columns that character/string occupies, based on wcwidth.c */

int BLI_wcwidth(wchar_t ucs)
{
	return mk_wcwidth(ucs);
}

int BLI_wcswidth(const wchar_t *pwcs, size_t n)
{
	return mk_wcswidth(pwcs, n);
}

int BLI_str_utf8_char_width(const char *p)
{
	unsigned int unicode = BLI_str_utf8_as_unicode(p);
	if (unicode == BLI_UTF8_ERR)
		return -1;

	return BLI_wcwidth((wchar_t)unicode);
}

int BLI_str_utf8_char_width_safe(const char *p)
{
	int columns;

	unsigned int unicode = BLI_str_utf8_as_unicode(p);
	if (unicode == BLI_UTF8_ERR)
		return 1;

	columns = BLI_wcwidth((wchar_t)unicode);

	return (columns < 0) ? 1 : columns;
}

/* --------------------------------------------------------------------------*/

/* copied from glib's gutf8.c, added 'Err' arg */

/* note, glib uses unsigned int for unicode, best we do the same,
 * though we don't typedef it - campbell */

#define UTF8_COMPUTE(Char, Mask, Len, Err)                                    \
	if (Char < 128) {                                                         \
		Len = 1;                                                              \
		Mask = 0x7f;                                                          \
	}                                                                         \
	else if ((Char & 0xe0) == 0xc0) {                                         \
		Len = 2;                                                              \
		Mask = 0x1f;                                                          \
	}                                                                         \
	else if ((Char & 0xf0) == 0xe0) {                                         \
		Len = 3;                                                              \
		Mask = 0x0f;                                                          \
	}                                                                         \
	else if ((Char & 0xf8) == 0xf0) {                                         \
		Len = 4;                                                              \
		Mask = 0x07;                                                          \
	}                                                                         \
	else if ((Char & 0xfc) == 0xf8) {                                         \
		Len = 5;                                                              \
		Mask = 0x03;                                                          \
	}                                                                         \
	else if ((Char & 0xfe) == 0xfc) {                                         \
		Len = 6;                                                              \
		Mask = 0x01;                                                          \
	}                                                                         \
	else {                                                                    \
		Len = Err;  /* -1 is the typical error value or 1 to skip */          \
	} (void)0

/* same as glib define but added an 'Err' arg */
#define UTF8_GET(Result, Chars, Count, Mask, Len, Err)                        \
	(Result) = (Chars)[0] & (Mask);                                           \
	for ((Count) = 1; (Count) < (Len); ++(Count)) {                           \
		if (((Chars)[(Count)] & 0xc0) != 0x80) {                              \
			(Result) = Err;                                                   \
			break;                                                            \
		}                                                                     \
		(Result) <<= 6;                                                       \
		(Result) |= ((Chars)[(Count)] & 0x3f);                                \
	} (void)0


/* uses glib functions but not from glib */
/* gets the size of a single utf8 char */
int BLI_str_utf8_size(const char *p)
{
	int mask = 0, len;
	const unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE(c, mask, len, -1);

	(void)mask; /* quiet warning */

	return len;
}

/* use when we want to skip errors */
int BLI_str_utf8_size_safe(const char *p)
{
	int mask = 0, len;
	const unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE(c, mask, len, 1);

	(void)mask; /* quiet warning */

	return len;
}

/* was g_utf8_get_char */
/**
 * BLI_str_utf8_as_unicode:
 * \param p a pointer to Unicode character encoded as UTF-8
 *
 * Converts a sequence of bytes encoded as UTF-8 to a Unicode character.
 * If \a p does not point to a valid UTF-8 encoded character, results are
 * undefined. If you are not sure that the bytes are complete
 * valid Unicode characters, you should use g_utf8_get_char_validated()
 * instead.
 *
 * Return value: the resulting character
 **/
unsigned int BLI_str_utf8_as_unicode(const char *p)
{
	int i, len;
	unsigned int mask = 0;
	unsigned int result;
	const unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE(c, mask, len, -1);
	if (UNLIKELY(len == -1))
		return BLI_UTF8_ERR;
	UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);

	return result;
}

/* variant that increments the length */
unsigned int BLI_str_utf8_as_unicode_and_size(const char *__restrict p, size_t *__restrict index)
{
	int i, len;
	unsigned mask = 0;
	unsigned int result;
	const unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE(c, mask, len, -1);
	if (UNLIKELY(len == -1))
		return BLI_UTF8_ERR;
	UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
	*index += (size_t)len;
	return result;
}

unsigned int BLI_str_utf8_as_unicode_and_size_safe(const char *__restrict p, size_t *__restrict index)
{
	int i, len;
	unsigned int mask = 0;
	unsigned int result;
	const unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE(c, mask, len, -1);
	if (UNLIKELY(len == -1)) {
		*index += 1;
		return c;
	}
	UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
	*index += (size_t)len;
	return result;
}

/* another variant that steps over the index,
 * note, currently this also falls back to latin1 for text drawing. */
unsigned int BLI_str_utf8_as_unicode_step(const char *__restrict p, size_t *__restrict index)
{
	int i, len;
	unsigned int mask = 0;
	unsigned int result;
	unsigned char c;

	p += *index;
	c = (unsigned char) *p;

	UTF8_COMPUTE(c, mask, len, -1);
	if (UNLIKELY(len == -1)) {
		/* when called with NULL end, result will never be NULL,
		 * checks for a NULL character */
		const char *p_next = BLI_str_find_next_char_utf8(p, NULL);
		/* will never return the same pointer unless '\0',
		 * eternal loop is prevented */
		*index += (size_t)(p_next - p);
		return BLI_UTF8_ERR;
	}

	/* this is tricky since there are a few ways we can bail out of bad unicode
	 * values, 3 possible solutions. */
#if 0
	UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
#elif 1
	/* WARNING: this is NOT part of glib, or supported by similar functions.
	 * this is added for text drawing because some filepaths can have latin1
	 * characters */
	UTF8_GET(result, p, i, mask, len, BLI_UTF8_ERR);
	if (result == BLI_UTF8_ERR) {
		len = 1;
		result = *p;
	}
	/* end warning! */
#else
	/* without a fallback like '?', text drawing will stop on this value */
	UTF8_GET(result, p, i, mask, len, '?');
#endif

	*index += (size_t)len;
	return result;
}

/* was g_unichar_to_utf8 */
/**
 * BLI_str_utf8_from_unicode:
 * @c a Unicode character code
 * \param outbuf output buffer, must have at least 6 bytes of space.
 *       If %NULL, the length will be computed and returned
 *       and nothing will be written to outbuf.
 *
 * Converts a single character to UTF-8.
 *
 * Return value: number of bytes written
 **/
size_t BLI_str_utf8_from_unicode(unsigned int c, char *outbuf)
{
	/* If this gets modified, also update the copy in g_string_insert_unichar() */
	unsigned int len = 0;
	unsigned int first;
	unsigned int i;

	if (c < 0x80) {
		first = 0;
		len = 1;
	}
	else if (c < 0x800) {
		first = 0xc0;
		len = 2;
	}
	else if (c < 0x10000) {
		first = 0xe0;
		len = 3;
	}
	else if (c < 0x200000) {
		first = 0xf0;
		len = 4;
	}
	else if (c < 0x4000000) {
		first = 0xf8;
		len = 5;
	}
	else {
		first = 0xfc;
		len = 6;
	}

	if (outbuf) {
		for (i = len - 1; i > 0; --i) {
			outbuf[i] = (c & 0x3f) | 0x80;
			c >>= 6;
		}
		outbuf[0] = c | first;
	}

	return len;
}

/* was g_utf8_find_prev_char */
/**
 * BLI_str_find_prev_char_utf8:
 * \param str pointer to the beginning of a UTF-8 encoded string
 * \param p pointer to some position within \a str
 *
 * Given a position \a p with a UTF-8 encoded string \a str, find the start
 * of the previous UTF-8 character starting before. \a p Returns %NULL if no
 * UTF-8 characters are present in \a str before \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL.
 **/
char *BLI_str_find_prev_char_utf8(const char *str, const char *p)
{
	for (--p; p >= str; --p) {
		if ((*p & 0xc0) != 0x80) {
			return (char *)p;
		}
	}
	return NULL;
}

/* was g_utf8_find_next_char */
/**
 * BLI_str_find_next_char_utf8:
 * \param p a pointer to a position within a UTF-8 encoded string
 * \param end a pointer to the byte following the end of the string,
 * or %NULL to indicate that the string is nul-terminated.
 *
 * Finds the start of the next UTF-8 character in the string after \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL
 **/
char *BLI_str_find_next_char_utf8(const char *p, const char *end)
{
	if (*p) {
		if (end) {
			for (++p; p < end && (*p & 0xc0) == 0x80; ++p) {
				/* do nothing */
			}
		}
		else {
			for (++p; (*p & 0xc0) == 0x80; ++p) {
				/* do nothing */
			}
		}
	}
	return (p == end) ? NULL : (char *)p;
}

/* was g_utf8_prev_char */
/**
 * BLI_str_prev_char_utf8:
 * \param p a pointer to a position within a UTF-8 encoded string
 *
 * Finds the previous UTF-8 character in the string before \a p
 *
 * \a p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte. If \a p might be the first
 * character of the string, you must use g_utf8_find_prev_char() instead.
 *
 * Return value: a pointer to the found character.
 **/
char *BLI_str_prev_char_utf8(const char *p)
{
	while (1) {
		p--;
		if ((*p & 0xc0) != 0x80) {
			return (char *)p;
		}
	}
}
/* end glib copy */

size_t BLI_str_partition_utf8(const char *str, const unsigned int delim[], char **sep, char **suf)
{
	return BLI_str_partition_ex_utf8(str, delim, sep, suf, false);
}

size_t BLI_str_rpartition_utf8(const char *str, const unsigned int delim[], char **sep, char **suf)
{
	return BLI_str_partition_ex_utf8(str, delim, sep, suf, true);
}

size_t BLI_str_partition_ex_utf8(const char *str, const unsigned int delim[], char **sep, char **suf,
                                 const bool from_right)
{
	const unsigned int *d;
	const size_t str_len = strlen(str);
	size_t index;

	*suf = (char *)(str + str_len);

	for (*sep = (char *)(from_right ? BLI_str_find_prev_char_utf8(str, str + str_len) : str), index = 0;
	     *sep != NULL && **sep != '\0';
	     *sep = (char *)(from_right ? (char *)BLI_str_find_prev_char_utf8(str, *sep) : str + index))
	{
		const unsigned int c = BLI_str_utf8_as_unicode_and_size(*sep, &index);

		if (c == BLI_UTF8_ERR) {
			*suf = *sep = NULL;
			break;
		}

		for (d = delim; *d != '\0'; ++d) {
			if (*d == c) {
				/* *suf is already correct in case from_right is true. */
				if (!from_right)
					*suf = (char *)(str + index);
				return (size_t)(*sep - str);
			}
		}

		*suf = *sep;  /* Useful in 'from_right' case! */
	}

	*suf = *sep = NULL;
	return str_len;
}

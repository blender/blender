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

#include "BLI_string_utf8.h"

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
	const unsigned char *p, *pend = (unsigned char*)str + length;
	unsigned char c;
	int ab;

	for (p = (unsigned char*)str; p < pend; p++) {
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
			if ((*(p+1) & 0xc0) != 0x80) goto utf8_error;
			p++; /* do this after so we get usable offset - campbell */
		}
	}

	return -1;

utf8_error:

	return (int)((char *)p - (char *)str) - 1;
}

int BLI_utf8_invalid_strip(char *str, int length)
{
	int bad_char, tot= 0;

	while ((bad_char= BLI_utf8_invalid_byte(str, length)) != -1) {
		str += bad_char;
		length -= bad_char;

		if (length == 0) {
			/* last character bad, strip it */
			*str= '\0';
			tot++;
			break;
		}
		else {
			/* strip, keep looking */
			memmove(str, str + 1, length);
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
		while (*src != '\0' && (utf8_size= utf8_skip_data[*src]) < maxncpy) {  \
			maxncpy -= utf8_size;                                             \
			switch (utf8_size) {                                               \
				case 6: *dst ++ = *src ++;                                    \
				case 5: *dst ++ = *src ++;                                    \
				case 4: *dst ++ = *src ++;                                    \
				case 3: *dst ++ = *src ++;                                    \
				case 2: *dst ++ = *src ++;                                    \
				case 1: *dst ++ = *src ++;                                    \
			}                                                                 \
		}                                                                     \
		*dst= '\0';                                                           \
	} (void)0

char *BLI_strncpy_utf8(char *dst, const char *src, size_t maxncpy)
{
	char *dst_r= dst;

	/* note: currently we don't attempt to deal with invalid utf8 chars */
	BLI_STR_UTF8_CPY(dst, src, maxncpy);

	return dst_r;
}

char *BLI_strncat_utf8(char *dst, const char *src, size_t maxncpy)
{
	while (*dst && maxncpy > 0) {
		dst++;
		maxncpy--;
	}

	BLI_STR_UTF8_CPY(dst, src, maxncpy);

	return dst;
}

#undef BLI_STR_UTF8_CPY

/* --------------------------------------------------------------------------*/
/* wchar_t / utf8 functions  */

size_t BLI_strncpy_wchar_as_utf8(char *dst, const wchar_t *src, const size_t maxcpy)
{
	size_t len = 0;
	while (*src && len < maxcpy) { /* XXX can still run over the buffer because utf8 size isn't known :| */
		len += BLI_str_utf8_from_unicode(*src++, dst+len);
	}

	dst[len]= '\0';

	return len;
}

/* wchar len in utf8 */
size_t BLI_wstrlen_utf8(const wchar_t *src)
{
	size_t len = 0;

	while (*src) {
		len += BLI_str_utf8_from_unicode(*src++, NULL);
	}

	return len;
}

// utf8slen
size_t BLI_strlen_utf8(const char *strc)
{
	int len=0;

	while (*strc) {
		if ((*strc & 0xe0) == 0xc0) {
			if ((strc[1] & 0x80) && (strc[1] & 0x40) == 0x00)
				strc++;
		}
		else if ((*strc & 0xf0) == 0xe0) {
			if ((strc[1] & strc[2] & 0x80) && ((strc[1] | strc[2]) & 0x40) == 0x00)
				strc += 2;
		}
		else if ((*strc & 0xf8) == 0xf0) {
			if ((strc[1] & strc[2] & strc[3] & 0x80) && ((strc[1] | strc[2] | strc[3]) & 0x40) == 0x00)
				strc += 3;
		}

		strc++;
		len++;
	}

	return len;
}

size_t BLI_strncpy_wchar_from_utf8(wchar_t *dst_w, const char *src_c, const size_t maxcpy)
{
	int len=0;

	if (dst_w==NULL || src_c==NULL) return(0);

	while (*src_c && len < maxcpy) {
		size_t step= 0;
		unsigned int unicode= BLI_str_utf8_as_unicode_and_size(src_c, &step);
		if (unicode != BLI_UTF8_ERR) {
			*dst_w= (wchar_t)unicode;
			src_c += step;
		}
		else {
			*dst_w = '?';
			src_c= BLI_str_find_next_char_utf8(src_c, NULL);
		}
		dst_w++;
		len++;
	}
	return len;
}

/* end wchar_t / utf8 functions  */
/* --------------------------------------------------------------------------*/

/* copied from glib's gutf8.c */

/* note, glib uses unsigned int for unicode, best we do the same,
 * though we don't typedef it - campbell */

#define UTF8_COMPUTE(Char, Mask, Len)                                         \
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
		Len = -1;                                                             \
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
    unsigned char c = (unsigned char) *p;

    UTF8_COMPUTE (c, mask, len);

	(void)mask; /* quiet warning */

	return len;
}

/* was g_utf8_get_char */
/**
 * BLI_str_utf8_as_unicode:
 * @p a pointer to Unicode character encoded as UTF-8
 *
 * Converts a sequence of bytes encoded as UTF-8 to a Unicode character.
 * If @p does not point to a valid UTF-8 encoded character, results are
 * undefined. If you are not sure that the bytes are complete
 * valid Unicode characters, you should use g_utf8_get_char_validated()
 * instead.
 *
 * Return value: the resulting character
 **/
unsigned int BLI_str_utf8_as_unicode(const char *p)
{
	int i, mask = 0, len;
	unsigned int result;
	unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE (c, mask, len);
	if (len == -1)
		return BLI_UTF8_ERR;
	UTF8_GET (result, p, i, mask, len, BLI_UTF8_ERR);

	return result;
}

/* variant that increments the length */
unsigned int BLI_str_utf8_as_unicode_and_size(const char *p, size_t *index)
{
	int i, mask = 0, len;
	unsigned int result;
	unsigned char c = (unsigned char) *p;

	UTF8_COMPUTE (c, mask, len);
	if (len == -1)
		return BLI_UTF8_ERR;
	UTF8_GET (result, p, i, mask, len, BLI_UTF8_ERR);
	*index += len;
	return result;
}

/* another variant that steps over the index,
 * note, currently this also falls back to latin1 for text drawing. */
unsigned int BLI_str_utf8_as_unicode_step(const char *p, size_t *index)
{
	int i, mask = 0, len;
	unsigned int result;
	unsigned char c;

	p += *index;
	c= (unsigned char) *p;

	UTF8_COMPUTE (c, mask, len);
	if (len == -1) {
		/* when called with NULL end, result will never be NULL,
		 * checks for a NULL character */
		char *p_next= BLI_str_find_next_char_utf8(p, NULL);
		/* will never return the same pointer unless '\0',
		 * eternal loop is prevented */
		*index += (size_t)(p_next - p);
		return BLI_UTF8_ERR;
	}

	/* this is tricky since there are a few ways we can bail out of bad unicode
	 * values, 3 possible solutions. */
#if 0
	UTF8_GET (result, p, i, mask, len, BLI_UTF8_ERR);
#elif 1
	/* WARNING: this is NOT part of glib, or supported by similar functions.
	 * this is added for text drawing because some filepaths can have latin1
	 * characters */
	UTF8_GET (result, p, i, mask, len, BLI_UTF8_ERR);
	if (result == BLI_UTF8_ERR) {
		len= 1;
		result= *p;
	}
	/* end warning! */
#else
	/* without a fallback like '?', text drawing will stop on this value */
	UTF8_GET (result, p, i, mask, len, '?');
#endif

	*index += len;
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
	int first;
	int i;

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
 * @str: pointer to the beginning of a UTF-8 encoded string
 * @p pointer to some position within @str
 *
 * Given a position @p with a UTF-8 encoded string @str, find the start
 * of the previous UTF-8 character starting before. @p Returns %NULL if no
 * UTF-8 characters are present in @str before @p
 *
 * @p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL.
 **/
char * BLI_str_find_prev_char_utf8(const char *str, const char *p)
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
 * @p a pointer to a position within a UTF-8 encoded string
 * @end a pointer to the byte following the end of the string,
 * or %NULL to indicate that the string is nul-terminated.
 *
 * Finds the start of the next UTF-8 character in the string after @p
 *
 * @p does not have to be at the beginning of a UTF-8 character. No check
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
 * @p a pointer to a position within a UTF-8 encoded string
 *
 * Finds the previous UTF-8 character in the string before @p
 *
 * @p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte. If @p might be the first
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

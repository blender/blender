/*
 * $Id$
 *
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

#include "BLI_string.h"

/* from libswish3, originally called u8_isvalid(),
 * modified to return the index of the bad character (byte index not utf).
 * http://svn.swish-e.org/libswish3/trunk/src/libswish3/utf8.c r3044 - campbell */

/* based on the valid_utf8 routine from the PCRE library by Philip Hazel

   length is in bytes, since without knowing whether the string is valid
   it's hard to know how many characters there are! */

static const char trailingBytesForUTF8[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5
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
			   and then for 1111 1100, xx00 00xx */
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

	while((bad_char= BLI_utf8_invalid_byte(str, length)) != -1) {
		str += bad_char;
		length -= bad_char;

		if(length == 0) {
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
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,1,1
};

char *BLI_strncpy_utf8(char *dst, const char *src, size_t maxncpy)
{
	char *dst_r= dst;
	size_t utf8_size;

	/* note: currently we dont attempt to deal with invalid utf8 chars */

	while(*src != '\0' && (utf8_size= utf8_skip_data[*src]) < maxncpy) {
		maxncpy -= utf8_size;
		switch(utf8_size) {
			case 6: *dst ++ = *src ++;
			case 5: *dst ++ = *src ++;
			case 4: *dst ++ = *src ++;
			case 3: *dst ++ = *src ++;
			case 2: *dst ++ = *src ++;
			case 1: *dst ++ = *src ++;
		}
	}
	*dst= '\0';
	return dst_r;
}

/* copied from glib */
/**
 * g_utf8_find_prev_char:
 * @str: pointer to the beginning of a UTF-8 encoded string
 * @p: pointer to some position within @str
 *
 * Given a position @p with a UTF-8 encoded string @str, find the start
 * of the previous UTF-8 character starting before @p. Returns %NULL if no
 * UTF-8 characters are present in @str before @p.
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

/**
 * g_utf8_find_next_char:
 * @p: a pointer to a position within a UTF-8 encoded string
 * @end: a pointer to the byte following the end of the string,
 * or %NULL to indicate that the string is nul-terminated.
 *
 * Finds the start of the next UTF-8 character in the string after @p.
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

/**
 * g_utf8_prev_char:
 * @p: a pointer to a position within a UTF-8 encoded string
 *
 * Finds the previous UTF-8 character in the string before @p.
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

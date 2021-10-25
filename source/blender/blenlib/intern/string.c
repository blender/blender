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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

/** \file blender/blenlib/intern/string.c
 *  \ingroup bli
 */


#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_dynstr.h"
#include "BLI_string.h"

#include "BLI_utildefines.h"

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

// #define DEBUG_STRSIZE

/**
 * Duplicates the first \a len bytes of cstring \a str
 * into a newly mallocN'd string and returns it. \a str
 * is assumed to be at least len bytes long.
 *
 * \param str The string to be duplicated
 * \param len The number of bytes to duplicate
 * \retval Returns the duplicated string
 */
char *BLI_strdupn(const char *str, const size_t len)
{
	char *n = MEM_mallocN(len + 1, "strdup");
	memcpy(n, str, len);
	n[len] = '\0';
	
	return n;
}

/**
 * Duplicates the cstring \a str into a newly mallocN'd
 * string and returns it.
 *
 * \param str The string to be duplicated
 * \retval Returns the duplicated string
 */
char *BLI_strdup(const char *str)
{
	return BLI_strdupn(str, strlen(str));
}

/**
 * Appends the two strings, and returns new mallocN'ed string
 * \param str1 first string for copy
 * \param str2 second string for append
 * \retval Returns dst
 */
char *BLI_strdupcat(const char *__restrict str1, const char *__restrict str2)
{
	/* include the NULL terminator of str2 only */
	const size_t str1_len = strlen(str1);
	const size_t str2_len = strlen(str2) + 1;
	char *str, *s;
	
	str = MEM_mallocN(str1_len + str2_len, "strdupcat");
	s = str;

	memcpy(s, str1, str1_len); s += str1_len;
	memcpy(s, str2, str2_len);

	return str;
}

/**
 * Like strncpy but ensures dst is always
 * '\0' terminated.
 *
 * \param dst Destination for copy
 * \param src Source string to copy
 * \param maxncpy Maximum number of characters to copy (generally
 * the size of dst)
 * \retval Returns dst
 */
char *BLI_strncpy(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
{
	size_t srclen = BLI_strnlen(src, maxncpy - 1);
	BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	memcpy(dst, src, srclen);
	dst[srclen] = '\0';
	return dst;
}

/**
 * Like BLI_strncpy but ensures dst is always padded by given char, on both sides (unless src is empty).
 *
 * \param dst Destination for copy
 * \param src Source string to copy
 * \param pad the char to use for padding
 * \param maxncpy Maximum number of characters to copy (generally the size of dst)
 * \retval Returns dst
 */
char *BLI_strncpy_ensure_pad(char *__restrict dst, const char *__restrict src, const char pad, size_t maxncpy)
{
	BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	if (src[0] == '\0') {
		dst[0] = '\0';
	}
	else {
		/* Add heading/trailing wildcards if needed. */
		size_t idx = 0;
		size_t srclen;

		if (src[idx] != pad) {
			dst[idx++] = pad;
			maxncpy--;
		}
		maxncpy--;  /* trailing '\0' */

		srclen = BLI_strnlen(src, maxncpy);
		if ((src[srclen - 1] != pad) && (srclen == maxncpy)) {
			srclen--;
		}

		memcpy(&dst[idx], src, srclen);
		idx += srclen;

		if (dst[idx - 1] != pad) {
			dst[idx++] = pad;
		}
		dst[idx] = '\0';
	}

	return dst;
}

/**
 * Like strncpy but ensures dst is always
 * '\0' terminated.
 *
 * \note This is a duplicate of #BLI_strncpy that returns bytes copied.
 * And is a drop in replacement for 'snprintf(str, sizeof(str), "%s", arg);'
 *
 * \param dst Destination for copy
 * \param src Source string to copy
 * \param maxncpy Maximum number of characters to copy (generally
 * the size of dst)
 * \retval The number of bytes copied (The only difference from BLI_strncpy).
 */
size_t BLI_strncpy_rlen(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
{
	size_t srclen = BLI_strnlen(src, maxncpy - 1);
	BLI_assert(maxncpy != 0);

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	memcpy(dst, src, srclen);
	dst[srclen] = '\0';
	return srclen;
}

size_t BLI_strcpy_rlen(char *__restrict dst, const char *__restrict src)
{
	size_t srclen = strlen(src);
	memcpy(dst, src, srclen + 1);
	return srclen;
}

/**
 * Portable replacement for #vsnprintf
 */
size_t BLI_vsnprintf(char *__restrict buffer, size_t maxncpy, const char *__restrict format, va_list arg)
{
	size_t n;

	BLI_assert(buffer != NULL);
	BLI_assert(maxncpy > 0);
	BLI_assert(format != NULL);

	n = (size_t)vsnprintf(buffer, maxncpy, format, arg);

	if (n != -1 && n < maxncpy) {
		buffer[n] = '\0';
	}
	else {
		buffer[maxncpy - 1] = '\0';
	}

	return n;
}

/**
 * A version of #BLI_vsnprintf that returns ``strlen(buffer)``
 */
size_t BLI_vsnprintf_rlen(char *__restrict buffer, size_t maxncpy, const char *__restrict format, va_list arg)
{
	size_t n;

	BLI_assert(buffer != NULL);
	BLI_assert(maxncpy > 0);
	BLI_assert(format != NULL);

	n = (size_t)vsnprintf(buffer, maxncpy, format, arg);

	if (n != -1 && n < maxncpy) {
		/* pass */
	}
	else {
		n = maxncpy - 1;
	}
	buffer[n] = '\0';

	return n;
}

/**
 * Portable replacement for #snprintf
 */
size_t BLI_snprintf(char *__restrict dst, size_t maxncpy, const char *__restrict format, ...)
{
	size_t n;
	va_list arg;

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	va_start(arg, format);
	n = BLI_vsnprintf(dst, maxncpy, format, arg);
	va_end(arg);

	return n;
}

/**
 * A version of #BLI_snprintf that returns ``strlen(dst)``
 */
size_t BLI_snprintf_rlen(char *__restrict dst, size_t maxncpy, const char *__restrict format, ...)
{
	size_t n;
	va_list arg;

#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxncpy);
#endif

	va_start(arg, format);
	n = BLI_vsnprintf_rlen(dst, maxncpy, format, arg);
	va_end(arg);

	return n;
}

/**
 * Print formatted string into a newly #MEM_mallocN'd string
 * and return it.
 */
char *BLI_sprintfN(const char *__restrict format, ...)
{
	DynStr *ds;
	va_list arg;
	char *n;

	va_start(arg, format);

	ds = BLI_dynstr_new();
	BLI_dynstr_vappendf(ds, format, arg);
	n = BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	va_end(arg);

	return n;
}


/* match pythons string escaping, assume double quotes - (")
 * TODO: should be used to create RNA animation paths.
 * TODO: support more fancy string escaping. current code is primitive
 *    this basically is an ascii version of PyUnicode_EncodeUnicodeEscape()
 *    which is a useful reference. */
size_t BLI_strescape(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
{
	size_t len = 0;

	BLI_assert(maxncpy != 0);

	while (len < maxncpy) {
		switch (*src) {
			case '\0':
				goto escape_finish;
			case '\\':
			case '"':
				ATTR_FALLTHROUGH;

			/* less common but should also be support */
			case '\t':
			case '\n':
			case '\r':
				if (len + 1 < maxncpy) {
					*dst++ = '\\';
					len++;
				}
				else {
					/* not enough space to escape */
					break;
				}
				ATTR_FALLTHROUGH;
			default:
				*dst = *src;
				break;
		}
		dst++;
		src++;
		len++;
	}

escape_finish:

	*dst = '\0';

	return len;
}

/**
 * Makes a copy of the text within the "" that appear after some text 'blahblah'
 * i.e. for string 'pose["apples"]' with prefix 'pose[', it should grab "apples"
 *
 * - str: is the entire string to chop
 * - prefix: is the part of the string to leave out
 *
 * Assume that the strings returned must be freed afterwards, and that the inputs will contain
 * data we want...
 *
 * \return the offset and a length so as to avoid doing an allocation.
 */
char *BLI_str_quoted_substrN(const char *__restrict str, const char *__restrict prefix)
{
	const char *startMatch, *endMatch;

	/* get the starting point (i.e. where prefix starts, and add prefixLen+1 to it to get be after the first " */
	startMatch = strstr(str, prefix);
	if (startMatch) {
		const size_t prefixLen = strlen(prefix);
		startMatch += prefixLen + 1;
		/* get the end point (i.e. where the next occurance of " is after the starting point) */

		endMatch = startMatch;
		while ((endMatch = strchr(endMatch, '"'))) {
			if (LIKELY(*(endMatch - 1) != '\\')) {
				break;
			}
			else {
				endMatch++;
			}
		}

		if (endMatch) {
			/* return the slice indicated */
			return BLI_strdupn(startMatch, (size_t)(endMatch - startMatch));
		}
	}
	return BLI_strdupn("", 0);
}

/**
 * string with all instances of substr_old replaced with substr_new,
 * Returns a copy of the cstring \a str into a newly mallocN'd
 * and returns it.
 *
 * \note A rather wasteful string-replacement utility, though this shall do for now...
 * Feel free to replace this with an even safe + nicer alternative
 *
 * \param str The string to replace occurrences of substr_old in
 * \param substr_old The text in the string to find and replace
 * \param substr_new The text in the string to find and replace
 * \retval Returns the duplicated string
 */
char *BLI_str_replaceN(const char *__restrict str, const char *__restrict substr_old, const char *__restrict substr_new)
{
	DynStr *ds = NULL;
	size_t len_old = strlen(substr_old);
	const char *match;

	BLI_assert(substr_old[0] != '\0');

	/* while we can still find a match for the old substring that we're searching for, 
	 * keep dicing and replacing
	 */
	while ((match = strstr(str, substr_old))) {
		/* the assembly buffer only gets created when we actually need to rebuild the string */
		if (ds == NULL)
			ds = BLI_dynstr_new();
			
		/* if the match position does not match the current position in the string, 
		 * copy the text up to this position and advance the current position in the string
		 */
		if (str != match) {
			/* add the segment of the string from str to match to the buffer, then restore the value at match
			 */
			BLI_dynstr_nappend(ds, str, (match - str));
			
			/* now our current position should be set on the start of the match */
			str = match;
		}
		
		/* add the replacement text to the accumulation buffer */
		BLI_dynstr_append(ds, substr_new);
		
		/* advance the current position of the string up to the end of the replaced segment */
		str += len_old;
	}
	
	/* finish off and return a new string that has had all occurrences of */
	if (ds) {
		char *str_new;
		
		/* add what's left of the string to the assembly buffer 
		 * - we've been adjusting str to point at the end of the replaced segments
		 */
		BLI_dynstr_append(ds, str);
		
		/* convert to new c-string (MEM_malloc'd), and free the buffer */
		str_new = BLI_dynstr_get_cstring(ds);
		BLI_dynstr_free(ds);
		
		return str_new;
	}
	else {
		/* just create a new copy of the entire string - we avoid going through the assembly buffer 
		 * for what should be a bit more efficiency...
		 */
		return BLI_strdup(str);
	}
} 

/**
 * In-place replace every \a src to \a dst in \a str.
 *
 * \param str: The string to operate on.
 * \param src: The character to replace.
 * \param dst: The character to replace with.
 */
void BLI_str_replace_char(char *str, char src, char dst)
{
	while (*str) {
		if (*str == src) {
			*str = dst;
		}
		str++;
	}
}

/**
 * Compare two strings without regard to case.
 *
 * \retval True if the strings are equal, false otherwise.
 */
int BLI_strcaseeq(const char *a, const char *b) 
{
	return (BLI_strcasecmp(a, b) == 0);
}

/**
 * Portable replacement for #strcasestr (not available in MSVC)
 */
char *BLI_strcasestr(const char *s, const char *find)
{
	register char c, sc;
	register size_t len;
	
	if ((c = *find++) != 0) {
		c = tolower(c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
				sc = tolower(sc);
			} while (sc != c);
		} while (BLI_strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *) s);
}

/**
 * Variation of #BLI_strcasestr with string length limited to \a len
 */
char *BLI_strncasestr(const char *s, const char *find, size_t len)
{
	register char c, sc;

	if ((c = *find++) != 0) {
		c = tolower(c);
		if (len > 1) {
			do {
				do {
					if ((sc = *s++) == 0)
						return NULL;
					sc = tolower(sc);
				} while (sc != c);
			} while (BLI_strncasecmp(s, find, len - 1) != 0);
		}
		else {
			{
				do {
					if ((sc = *s++) == 0)
						return NULL;
					sc = tolower(sc);
				} while (sc != c);
			}
		}
		s--;
	}
	return ((char *)s);
}

int BLI_strcasecmp(const char *s1, const char *s2)
{
	register int i;
	register char c1, c2;

	for (i = 0;; i++) {
		c1 = tolower(s1[i]);
		c2 = tolower(s2[i]);

		if (c1 < c2) {
			return -1;
		}
		else if (c1 > c2) {
			return 1;
		}
		else if (c1 == 0) {
			break;
		}
	}

	return 0;
}

int BLI_strncasecmp(const char *s1, const char *s2, size_t len)
{
	register size_t i;
	register char c1, c2;

	for (i = 0; i < len; i++) {
		c1 = tolower(s1[i]);
		c2 = tolower(s2[i]);

		if (c1 < c2) {
			return -1;
		}
		else if (c1 > c2) {
			return 1;
		}
		else if (c1 == 0) {
			break;
		}
	}

	return 0;
}

/* compare number on the left size of the string */
static int left_number_strcmp(const char *s1, const char *s2, int *tiebreaker)
{
	const char *p1 = s1, *p2 = s2;
	int numdigit, numzero1, numzero2;

	/* count and skip leading zeros */
	for (numzero1 = 0; *p1 && (*p1 == '0'); numzero1++)
		p1++;
	for (numzero2 = 0; *p2 && (*p2 == '0'); numzero2++)
		p2++;

	/* find number of consecutive digits */
	for (numdigit = 0; ; numdigit++) {
		if (isdigit(*(p1 + numdigit)) && isdigit(*(p2 + numdigit)))
			continue;
		else if (isdigit(*(p1 + numdigit)))
			return 1; /* s2 is bigger */
		else if (isdigit(*(p2 + numdigit)))
			return -1; /* s1 is bigger */
		else
			break;
	}

	/* same number of digits, compare size of number */
	if (numdigit > 0) {
		int compare = (int)strncmp(p1, p2, (size_t)numdigit);

		if (compare != 0)
			return compare;
	}

	/* use number of leading zeros as tie breaker if still equal */
	if (*tiebreaker == 0) {
		if (numzero1 > numzero2)
			*tiebreaker = 1;
		else if (numzero1 < numzero2)
			*tiebreaker = -1;
	}

	return 0;
}

/* natural string compare, keeping numbers in order */
int BLI_natstrcmp(const char *s1, const char *s2)
{
	register int d1 = 0, d2 = 0;
	register char c1, c2;
	int tiebreaker = 0;

	/* if both chars are numeric, to a left_number_strcmp().
	 * then increase string deltas as long they are 
	 * numeric, else do a tolower and char compare */

	while (1) {
		c1 = tolower(s1[d1]);
		c2 = tolower(s2[d2]);
		
		if (isdigit(c1) && isdigit(c2)) {
			int numcompare = left_number_strcmp(s1 + d1, s2 + d2, &tiebreaker);
			
			if (numcompare != 0)
				return numcompare;

			d1++;
			while (isdigit(s1[d1]))
				d1++;
			d2++;
			while (isdigit(s2[d2]))
				d2++;
			
			c1 = tolower(s1[d1]);
			c2 = tolower(s2[d2]);
		}
	
		/* first check for '.' so "foo.bar" comes before "foo 1.bar" */
		if (c1 == '.' && c2 != '.')
			return -1;
		if (c1 != '.' && c2 == '.')
			return 1;
		else if (c1 < c2) {
			return -1;
		}
		else if (c1 > c2) {
			return 1;
		}
		else if (c1 == 0) {
			break;
		}
		d1++;
		d2++;
	}

	if (tiebreaker)
		return tiebreaker;
	
	/* we might still have a different string because of lower/upper case, in
	 * that case fall back to regular string comparison */
	return strcmp(s1, s2);
}

/**
 * Like strcmp, but will ignore any heading/trailing pad char for comparison.
 * So e.g. if pad is '*', '*world' and 'world*' will compare equal.
 */
int BLI_strcmp_ignore_pad(const char *str1, const char *str2, const char pad)
{
	size_t str1_len, str2_len;

	while (*str1 == pad) {
		str1++;
	}
	while (*str2 == pad) {
		str2++;
	}

	str1_len = strlen(str1);
	str2_len = strlen(str2);

	while (str1_len && (str1[str1_len - 1] == pad)) {
		str1_len--;
	}
	while (str2_len && (str2[str2_len - 1] == pad)) {
		str2_len--;
	}

	if (str1_len == str2_len) {
		return strncmp(str1, str2, str2_len);
	}
	else if (str1_len > str2_len) {
		int ret = strncmp(str1, str2, str2_len);
		if (ret == 0) {
			ret = 1;
		}
		return ret;
	}
	else {
		int ret = strncmp(str1, str2, str1_len);
		if (ret == 0) {
			ret = -1;
		}
		return ret;
	}
}

/* determine the length of a fixed-size string */
size_t BLI_strnlen(const char *s, const size_t maxlen)
{
	size_t len;

	for (len = 0; len < maxlen; len++, s++) {
		if (!*s)
			break;
	}
	return len;
}

void BLI_str_tolower_ascii(char *str, const size_t len)
{
	size_t i;

	for (i = 0; (i < len) && str[i]; i++)
		if (str[i] >= 'A' && str[i] <= 'Z')
			str[i] += 'a' - 'A';
}

void BLI_str_toupper_ascii(char *str, const size_t len)
{
	size_t i;

	for (i = 0; (i < len) && str[i]; i++)
		if (str[i] >= 'a' && str[i] <= 'z')
			str[i] -= 'a' - 'A';
}

/**
 * Strip trailing zeros from a float, eg:
 *   0.0000 -> 0.0
 *   2.0010 -> 2.001
 *
 * \param str
 * \param pad
 * \return The number of zeros stripped.
 */
int BLI_str_rstrip_float_zero(char *str, const char pad)
{
	char *p = strchr(str, '.');
	int totstrip = 0;
	if (p) {
		char *end_p;
		p++;  /* position at first decimal place */
		end_p = p + (strlen(p) - 1);  /* position at last character */
		if (end_p > p) {
			while (end_p != p && *end_p == '0') {
				*end_p = pad;
				end_p--;
			}
		}
	}

	return totstrip;
}

/**
 * Return index of a string in a string array.
 *
 * \param str The string to find.
 * \param str_array Array of strings.
 * \param str_array_len The length of the array, or -1 for a NULL-terminated array.
 * \return The index of str in str_array or -1.
 */
int BLI_str_index_in_array_n(const char *__restrict str, const char **__restrict str_array, const int str_array_len)
{
	int index;
	const char **str_iter = str_array;

	for (index = 0; index < str_array_len; str_iter++, index++) {
		if (STREQ(str, *str_iter)) {
			return index;
		}
	}
	return -1;
}

/**
 * Return index of a string in a string array.
 *
 * \param str The string to find.
 * \param str_array Array of strings, (must be NULL-terminated).
 * \return The index of str in str_array or -1.
 */
int BLI_str_index_in_array(const char *__restrict str, const char **__restrict str_array)
{
	int index;
	const char **str_iter = str_array;

	for (index = 0; *str_iter; str_iter++, index++) {
		if (STREQ(str, *str_iter)) {
			return index;
		}
	}
	return -1;
}

bool BLI_strn_endswith(const char *__restrict str, const char *__restrict end, size_t slength)
{
	size_t elength = strlen(end);
	
	if (elength < slength) {
		const char *iter = &str[slength - elength];
		while (*iter) {
			if (*iter++ != *end++) {
				return false;
			}
		}
		return true;
	}
	return false;
}

/**
 * Find if a string ends with another string.
 *
 * \param str The string to search within.
 * \param end The string we look for at the end.
 * \return If str ends with end.
 */
bool BLI_str_endswith(const char *__restrict str, const char * __restrict end)
{
	const size_t slength = strlen(str);
	return BLI_strn_endswith(str, end, slength);
}

/**
 * Find the first char matching one of the chars in \a delim, from left.
 *
 * \param str The string to search within.
 * \param delim The set of delimiters to search for, as unicode values.
 * \param sep Return value, set to the first delimiter found (or NULL if none found).
 * \param suf Return value, set to next char after the first delimiter found (or NULL if none found).
 * \return The length of the prefix (i.e. *sep - str).
 */
size_t BLI_str_partition(const char *str, const char delim[], const char **sep, const char **suf)
{
	return BLI_str_partition_ex(str, NULL, delim, sep, suf, false);
}

/**
 * Find the first char matching one of the chars in \a delim, from right.
 *
 * \param str The string to search within.
 * \param delim The set of delimiters to search for, as unicode values.
 * \param sep Return value, set to the first delimiter found (or NULL if none found).
 * \param suf Return value, set to next char after the first delimiter found (or NULL if none found).
 * \return The length of the prefix (i.e. *sep - str).
 */
size_t BLI_str_rpartition(const char *str, const char delim[], const char **sep, const char **suf)
{
	return BLI_str_partition_ex(str, NULL, delim, sep, suf, true);
}

/**
 * Find the first char matching one of the chars in \a delim, either from left or right.
 *
 * \param str The string to search within.
 * \param end If non-NULL, the right delimiter of the string.
 * \param delim The set of delimiters to search for, as unicode values.
 * \param sep Return value, set to the first delimiter found (or NULL if none found).
 * \param suf Return value, set to next char after the first delimiter found (or NULL if none found).
 * \param from_right If %true, search from the right of \a str, else, search from its left.
 * \return The length of the prefix (i.e. *sep - str).
 */
size_t BLI_str_partition_ex(
        const char *str, const char *end, const char delim[], const char **sep, const char **suf, const bool from_right)
{
	const char *d;
	char *(*func)(const char *str, int c) = from_right ? strrchr : strchr;

	BLI_assert(end == NULL || end > str);

	*sep = *suf = NULL;

	for (d = delim; *d != '\0'; ++d) {
		const char *tmp;

		if (end) {
			if (from_right) {
				for (tmp = end - 1; (tmp >= str) && (*tmp != *d); tmp--);
				if (tmp	< str) {
					tmp = NULL;
				}
			}
			else {
				tmp = func(str, *d);
				if (tmp	>= end) {
					tmp = NULL;
				}
			}
		}
		else {
			tmp = func(str, *d);
		}

		if (tmp && (from_right ? (*sep < tmp) : (!*sep || *sep > tmp))) {
			*sep = tmp;
		}
	}

	if (*sep) {
		*suf = *sep + 1;
		return (size_t)(*sep - str);
	}

	return end ? (size_t)(end - str) : strlen(str);
}

/**
 * Format ints with decimal grouping.
 * 1000 -> 1,000
 *
 * \param dst  The resulting string
 * \param num  Number to format
 * \return The length of \a dst
 */
size_t BLI_str_format_int_grouped(char dst[16], int num)
{
	char src[16];
	char *p_src = src;
	char *p_dst = dst;

	const char separator = ',';
	int num_len, commas;

	num_len = sprintf(src, "%d", num);

	if (*p_src == '-') {
		*p_dst++ = *p_src++;
		num_len--;
	}

	for (commas = 2 - num_len % 3; *p_src; commas = (commas + 1) % 3) {
		*p_dst++ = *p_src++;
		if (commas == 1) {
			*p_dst++ = separator;
		}
	}
	*--p_dst = '\0';

	return (size_t)(p_dst - dst);
}

/**
 * Find the ranges needed to split \a str into its individual words.
 *
 * \param str: The string to search for words.
 * \param len: Size of the string to search.
 * \param delim: Character to use as a delimiter.
 * \param r_words: Info about the words found. Set to [index, len] pairs.
 * \param words_max: Max number of words to find
 * \return The number of words found in \a str
 */
int BLI_string_find_split_words(
        const char *str, const size_t len,
        const char delim, int r_words[][2], int words_max)
{
	int n = 0, i;
	bool charsearch = true;

	/* Skip leading spaces */
	for (i = 0; (i < len) && (str[i] != '\0'); i++) {
		if (str[i] != delim) {
			break;
		}
	}

	for (; (i < len) && (str[i] != '\0') && (n < words_max); i++) {
		if ((str[i] != delim) && (charsearch == true)) {
			r_words[n][0] = i;
			charsearch = false;
		}
		else {
			if ((str[i] == delim) && (charsearch == false)) {
				r_words[n][1] = i - r_words[n][0];
				n++;
				charsearch = true;
			}
		}
	}

	if (charsearch == false) {
		r_words[n][1] = i - r_words[n][0];
		n++;
	}

	return n;
}

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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * $Id$ 
*/

#ifndef BLI_STRING_H
#define BLI_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

	/**
	 * Duplicates the cstring @a str into a newly mallocN'd
	 * string and returns it.
	 * 
	 * @param str The string to be duplicated
	 * @retval Returns the duplicated string
	 */
char *BLI_strdup(const char *str);

	/**
	 * Duplicates the first @a len bytes of cstring @a str 
	 * into a newly mallocN'd string and returns it. @a str
	 * is assumed to be at least len bytes long.
	 * 
	 * @param str The string to be duplicated
	 * @param len The number of bytes to duplicate
	 * @retval Returns the duplicated string
	 */
char *BLI_strdupn(const char *str, int len);

	/**
	 * Like strncpy but ensures dst is always
	 * '\0' terminated.
	 * 
	 * @param dst Destination for copy
	 * @param src Source string to copy
	 * @param maxncpy Maximum number of characters to copy (generally
	 *   the size of dst)
	 * @retval Returns dst
	 */
char *BLI_strdupcat(const char *str1, const char *str2);

	/**
	 * Appends the two strings, and returns new mallocN'ed string
	 * @param str1 first string for copy
	 * @param str2 second string for append
	 * @retval Returns dst
	 */
char *BLI_strncpy(char *dst, const char *src, int maxncpy);

	/* Makes a copy of the text within the "" that appear after some text 'blahblah'
	 * i.e. for string 'pose["apples"]' with prefix 'pose[', it should grab "apples"
	 * 
	 * 	- str: is the entire string to chop
	 *	- prefix: is the part of the string to leave out 
	 *
	 * Assume that the strings returned must be freed afterwards, and that the inputs will contain 
	 * data we want...
	 */
char *BLI_getQuotedStr(const char *str, const char *prefix);

	/**
	 * Returns a copy of the cstring @a str into a newly mallocN'd
	 * string with all instances of oldText replaced with newText,
	 * and returns it.
	 * 
	 * @param str The string to replace occurances of oldText in
	 * @param oldText The text in the string to find and replace
	 * @param newText The text in the string to find and replace
	 * @retval Returns the duplicated string
	 */
char *BLI_replacestr(char *str, const char *oldText, const char *newText);

	/* 
	 * Replacement for snprintf
	 */
int BLI_snprintf(char *buffer, size_t count, const char *format, ...);

	/* 
	 * Print formatted string into a newly mallocN'd string
	 * and return it.
	 */
char *BLI_sprintfN(const char *format, ...);

	/**
	 * Compare two strings
	 * 
	 * @retval True if the strings are equal, false otherwise.
	 */
int BLI_streq(const char *a, const char *b);

	/**
	 * Compare two strings without regard to case.
	 * 
	 * @retval True if the strings are equal, false otherwise.
	 */
int BLI_strcaseeq(const char *a, const char *b);

char *BLI_strcasestr(const char *s, const char *find);
int BLI_strcasecmp(const char *s1, const char *s2);
int BLI_strncasecmp(const char *s1, const char *s2, int n);
int BLI_natstrcmp(const char *s1, const char *s2);
size_t BLI_strnlen(const char *str, size_t maxlen);

void BLI_timestr(double _time, char *str); /* time var is global */

#ifdef __cplusplus
}
#endif

#endif

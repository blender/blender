/* util.c
 *
 * various string, file, list operations.
 *
 *
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

char *BLI_strdupn(const char *str, const size_t len) {
	char *n= MEM_mallocN(len+1, "strdup");
	memcpy(n, str, len);
	n[len]= '\0';
	
	return n;
}
char *BLI_strdup(const char *str) {
	return BLI_strdupn(str, strlen(str));
}

char *BLI_strdupcat(const char *str1, const char *str2)
{
	size_t len;
	char *n;
	
	len= strlen(str1)+strlen(str2);
	n= MEM_mallocN(len+1, "strdupcat");
	strcpy(n, str1);
	strcat(n, str2);
	
	return n;
}

char *BLI_strncpy(char *dst, const char *src, const size_t maxncpy) {
	size_t srclen= strlen(src);
	size_t cpylen= (srclen>(maxncpy-1))?(maxncpy-1):srclen;
	
	memcpy(dst, src, cpylen);
	dst[cpylen]= '\0';
	
	return dst;
}

size_t BLI_snprintf(char *buffer, size_t count, const char *format, ...)
{
	size_t n;
	va_list arg;

	va_start(arg, format);
	n = vsnprintf(buffer, count, format, arg);
	
	if (n != -1 && n < count) {
		buffer[n] = '\0';
	} else {
		buffer[count-1] = '\0';
	}
	
	va_end(arg);
	return n;
}

char *BLI_sprintfN(const char *format, ...)
{
	DynStr *ds;
	va_list arg;
	char *n;

	va_start(arg, format);

	ds= BLI_dynstr_new();
	BLI_dynstr_vappendf(ds, format, arg);
	n= BLI_dynstr_get_cstring(ds);
	BLI_dynstr_free(ds);

	va_end(arg);

	return n;
}

/* Makes a copy of the text within the "" that appear after some text 'blahblah'
 * i.e. for string 'pose["apples"]' with prefix 'pose[', it should grab "apples"
 * 
 * 	- str: is the entire string to chop
 *	- prefix: is the part of the string to leave out 
 *
 * Assume that the strings returned must be freed afterwards, and that the inputs will contain 
 * data we want...
 */
char *BLI_getQuotedStr (const char *str, const char *prefix)
{
	size_t prefixLen = strlen(prefix);
	char *startMatch, *endMatch;
	
	/* get the starting point (i.e. where prefix starts, and add prefixLen+1 to it to get be after the first " */
	startMatch= strstr(str, prefix) + prefixLen + 1;
	
	/* get the end point (i.e. where the next occurance of " is after the starting point) */
	endMatch= strchr(startMatch, '"'); // "  NOTE: this comment here is just so that my text editor still shows the functions ok...
	
	/* return the slice indicated */
	return BLI_strdupn(startMatch, (size_t)(endMatch-startMatch));
}

/* Replaces all occurances of oldText with newText in str, returning a new string that doesn't 
 * contain the 'replaced' occurances.
 */
// A rather wasteful string-replacement utility, though this shall do for now...
// Feel free to replace this with an even safe + nicer alternative 
char *BLI_replacestr(char *str, const char *oldText, const char *newText)
{
	DynStr *ds= NULL;
	size_t lenOld= strlen(oldText);
	char *match;
	
	/* sanity checks */
	if ((str == NULL) || (str[0]==0))
		return NULL;
	else if ((oldText == NULL) || (newText == NULL) || (oldText[0]==0))
		return BLI_strdup(str);
	
	/* while we can still find a match for the old substring that we're searching for, 
	 * keep dicing and replacing
	 */
	while ( (match = strstr(str, oldText)) ) {
		/* the assembly buffer only gets created when we actually need to rebuild the string */
		if (ds == NULL)
			ds= BLI_dynstr_new();
			
		/* if the match position does not match the current position in the string, 
		 * copy the text up to this position and advance the current position in the string
		 */
		if (str != match) {
			/* replace the token at the 'match' position with \0 so that the copied string will be ok,
			 * add the segment of the string from str to match to the buffer, then restore the value at match
			 */
			match[0]= 0;
			BLI_dynstr_append(ds, str);
			match[0]= oldText[0];
			
			/* now our current position should be set on the start of the match */
			str= match;
		}
		
		/* add the replacement text to the accumulation buffer */
		BLI_dynstr_append(ds, newText);
		
		/* advance the current position of the string up to the end of the replaced segment */
		str += lenOld;
	}
	
	/* finish off and return a new string that has had all occurances of */
	if (ds) {
		char *newStr;
		
		/* add what's left of the string to the assembly buffer 
		 *	- we've been adjusting str to point at the end of the replaced segments
		 */
		if (str != NULL)
			BLI_dynstr_append(ds, str);
		
		/* convert to new c-string (MEM_malloc'd), and free the buffer */
		newStr= BLI_dynstr_get_cstring(ds);
		BLI_dynstr_free(ds);
		
		return newStr;
	}
	else {
		/* just create a new copy of the entire string - we avoid going through the assembly buffer 
		 * for what should be a bit more efficiency...
		 */
		return BLI_strdup(str);
	}
} 

int BLI_streq(const char *a, const char *b) 
{
	return (strcmp(a, b)==0);
}

int BLI_strcaseeq(const char *a, const char *b) 
{
	return (BLI_strcasecmp(a, b)==0);
}

/* strcasestr not available in MSVC */
char *BLI_strcasestr(const char *s, const char *find)
{
	register char c, sc;
	register size_t len;
	
	if ((c = *find++) != 0) {
		c= tolower(c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
				sc= tolower(sc);
			} while (sc != c);
		} while (BLI_strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *) s);
}


int BLI_strcasecmp(const char *s1, const char *s2) {
	int i;

	for (i=0; ; i++) {
		char c1 = tolower(s1[i]);
		char c2 = tolower(s2[i]);

		if (c1<c2) {
			return -1;
		} else if (c1>c2) {
			return 1;
		} else if (c1==0) {
			break;
		}
	}

	return 0;
}

int BLI_strncasecmp(const char *s1, const char *s2, size_t len) {
	int i;

	for (i=0; i<len; i++) {
		char c1 = tolower(s1[i]);
		char c2 = tolower(s2[i]);

		if (c1<c2) {
			return -1;
		} else if (c1>c2) {
			return 1;
		} else if (c1==0) {
			break;
		}
	}

	return 0;
}

/* natural string compare, keeping numbers in order */
int BLI_natstrcmp(const char *s1, const char *s2)
{
	int d1= 0, d2= 0;
	
	/* if both chars are numeric, to a strtol().
	   then increase string deltas as long they are 
	   numeric, else do a tolower and char compare */
	
	while(1) {
		char c1 = tolower(s1[d1]);
		char c2 = tolower(s2[d2]);
		
		if( isdigit(c1) && isdigit(c2) ) {
			int val1, val2;
			
			val1= (int)strtol(s1+d1, (char **)NULL, 10);
			val2= (int)strtol(s2+d2, (char **)NULL, 10);
			
			if (val1<val2) {
				return -1;
			} else if (val1>val2) {
				return 1;
			}
			d1++;
			while( isdigit(s1[d1]) )
				d1++;
			d2++;
			while( isdigit(s2[d2]) )
				d2++;
			
			c1 = tolower(s1[d1]);
			c2 = tolower(s2[d2]);
		}
	
		/* first check for '.' so "foo.bar" comes before "foo 1.bar" */	
		if(c1=='.' && c2!='.')
			return -1;
		if(c1!='.' && c2=='.')
			return 1;
		else if (c1<c2) {
			return -1;
		} else if (c1>c2) {
			return 1;
		} else if (c1==0) {
			break;
		}
		d1++;
		d2++;
	}
	return 0;
}

void BLI_timestr(double _time, char *str)
{
	/* format 00:00:00.00 (hr:min:sec) string has to be 12 long */
	int  hr= ( (int)  _time) / (60*60);
	int min= (((int)  _time) / 60 ) % 60;
	int sec= ( (int) (_time)) % 60;
	int hun= ( (int) (_time   * 100.0)) % 100;
	
	if (hr) {
		sprintf(str, "%.2d:%.2d:%.2d.%.2d",hr,min,sec,hun);
	} else {
		sprintf(str, "%.2d:%.2d.%.2d",min,sec,hun);
	}
	
	str[11]=0;
}

/* determine the length of a fixed-size string */
size_t BLI_strnlen(const char *str, size_t maxlen)
{
	const char *end = memchr(str, '\0', maxlen);
	return end ? (size_t) (end - str) : maxlen;
}

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


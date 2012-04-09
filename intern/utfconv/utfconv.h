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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Alexandr Kuznetsov, Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>



#ifdef __cplusplus 
extern 	"C" {
#endif

/**
 * Counts how many bytes is requered for for future utf-8 string using utf-16
 * @param string-16 pointer to working utf-16 string
 * @return How many bytes must be allocated includeng NULL.
 */
size_t count_utf_8_from_16(const wchar_t * string16);

/**
 * Counts how many wchar_t (two byte) is requered for for future utf-16 string using utf-8
 * @param string-8 pointer to working utf-8 string
 * @return How many bytes must be allocated includeng NULL.
 */
size_t count_utf_16_from_8(const char * string8);

/**
 * conv_utf_*** errors
 */
#define UTF_ERROR_NULL_IN 1<<0 /* Error occures when requered parameter is missing*/
#define UTF_ERROR_ILLCHAR 1<<1 /* Error if character is in illigal UTF rage*/
#define UTF_ERROR_SMALL   1<<2 /* Passed size is to small. It gives legal string with character missing at the end*/
#define UTF_ERROR_ILLSEQ  1<<3 /* Error if sequence is broken and doesn't finish*/

/**
 * Converts utf-16 string to allocated utf-8 string
 * @params in16 utf-16 string to convert
 * @params out8 utf-8 string to string the conversion
 * @params size8 the allocated size in bytes of out8
 * @return Returns any errors occured during conversion. See the block above,
 */
int conv_utf_16_to_8(const wchar_t * in16, char * out8, size_t size8);

/**
 * Converts utf-8 string to allocated utf-16 string
 * @params in8 utf-8 string to convert
 * @params out16 utf-16 string to string the conversion
 * @params size16 the allocated size in wchar_t (two byte) of out16
 * @return Returns any errors occured during conversion. See the block above,
 */
int conv_utf_8_to_16(const char * in8, wchar_t * out16, size_t size16);


/**
 * Allocates and converts the utf-8 string from utf-16
 * @params in16 utf-16 string to convert
 * @params add any additional size which will be allocated for new utf-8 string in bytes
 * @return New allocated and converted utf-8 string or NULL if in16 is 0.
 */
char * alloc_utf_8_from_16(const wchar_t * in16, size_t add);

/**
 * Allocates and converts the utf-16 string from utf-8
 * @params in8 utf-8 string to convert
 * @params add any additional size which will be allocated for new utf-16 string in wchar_t (two bytes)
 * @return New allocated and converted utf-16 string or NULL if in8 is 0.
 */
wchar_t * alloc_utf16_from_8(const char * in8, size_t add);

/* Easy allocation and conversion of new utf-16 string. New string has _16 suffix. Must be deallocated with UTF16_UN_ENCODE in right order*/
#define UTF16_ENCODE(in8str) if(1){\
	wchar_t * in8str ## _16 = alloc_utf16_from_8((char*)in8str, 0);

#define UTF16_UN_ENCODE(in8str) \
	free(in8str ## _16 ); };

#ifdef __cplusplus 
}
#endif

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
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Alexandr Kuznetsov, Andrea Weikert
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "utfconv.h"

size_t count_utf_8_from_16(const wchar_t *string16)
{
	int i;
	size_t count = 0;
	wchar_t u = 0;
	if (!string16) {
		return 0;
	}

	for (i = 0; u = string16[i]; i++) {
		if (u < 0x0080) {
			count += 1;
		}
		else {
			if (u < 0x0800) {
				count += 2;
			}
			else {
				if (u < 0xD800) {
					count += 3;
				}
				else {
					if (u < 0xDC00) {
						i++;
						if ((u = string16[i]) == 0) {
							break;
						}
						if (u >= 0xDC00 && u < 0xE000) {
							count += 4;
						}
					}
					else {
						if (u < 0xE000) {
							/*illigal*/;
						}
						else {
							count += 3;
						}
					}
				}
			}
		}
	}

	return ++count;
}


size_t count_utf_16_from_8(const char *string8)
{
	size_t count = 0;
	char u;
	char type = 0;
	unsigned int u32 = 0;

	if (!string8) return 0;

	for (; (u = *string8); string8++) {
		if (type == 0) {
			if ((u & 0x01 << 7) == 0)     { count++; u32 = 0; continue; }       //1 utf-8 char
			if ((u & 0x07 << 5) == 0xC0)  { type = 1; u32 = u & 0x1F; continue; } //2 utf-8 char
			if ((u & 0x0F << 4) == 0xE0)  { type = 2; u32 = u & 0x0F; continue; } //3 utf-8 char
			if ((u & 0x1F << 3) == 0xF0)  { type = 3; u32 = u & 0x07; continue; } //4 utf-8 char
			continue;
		}
		else {
			if ((u & 0xC0) == 0x80) {
				u32 = (u32 << 6) | (u & 0x3F);
				type--;
			}
			else {
				u32 = 0;
				type = 0;
			}
		}

		if (type == 0) {
			if ((0 < u32 && u32 < 0xD800) || (0xE000 <= u32 && u32 < 0x10000)) count++;
			else if (0x10000 <= u32 && u32 < 0x110000) count += 2;
			u32 = 0;
		}
	}

	return ++count;
}


int conv_utf_16_to_8(const wchar_t *in16, char *out8, size_t size8)
{
	char *out8end = out8 + size8;
	wchar_t u = 0;
	int err = 0;
	if (!size8 || !in16 || !out8) return UTF_ERROR_NULL_IN;
	out8end--;

	for (; out8 < out8end && (u = *in16); in16++, out8++) {
		if (u < 0x0080) {
			*out8 = u;
		}
		else if (u < 0x0800) {
			if (out8 + 1 >= out8end) break;
			*out8++ = (0x3 << 6) | (0x1F & (u >> 6));
			*out8  = (0x1 << 7) | (0x3F & (u));
		}
		else if (u < 0xD800 || u >= 0xE000) {
			if (out8 + 2 >= out8end) break;
			*out8++ = (0x7 << 5) | (0xF & (u >> 12));
			*out8++ = (0x1 << 7) | (0x3F & (u >> 6));;
			*out8  = (0x1 << 7) | (0x3F & (u));
		}
		else if (u < 0xDC00) {
			wchar_t u2 = *++in16;

			if (!u2) break;
			if (u2 >= 0xDC00 && u2 < 0xE000) {
				if (out8 + 3 >= out8end) break; else {
					unsigned int uc = 0x10000 + (u2 - 0xDC00) + ((u - 0xD800) << 10);

					*out8++ = (0xF << 4) | (0x7 & (uc >> 18));
					*out8++ = (0x1 << 7) | (0x3F & (uc >> 12));
					*out8++ = (0x1 << 7) | (0x3F & (uc >> 6));
					*out8  = (0x1 << 7) | (0x3F & (uc));
				}
			}
			else {
				out8--; err |= UTF_ERROR_ILLCHAR;
			}
		}
		else if (u < 0xE000) {
			out8--; err |= UTF_ERROR_ILLCHAR;
		}
	}

	*out8 = *out8end = 0;

	if (*in16) err |= UTF_ERROR_SMALL;

	return err;
}


int conv_utf_8_to_16(const char *in8, wchar_t *out16, size_t size16)
{
	char u;
	char type = 0;
	wchar_t u32 = 0;
	wchar_t *out16end = out16 + size16;
	int err = 0;
	if (!size16 || !in8 || !out16) return UTF_ERROR_NULL_IN;
	out16end--;

	for (; out16 < out16end && (u = *in8); in8++) {
		if (type == 0) {
			if ((u & 0x01 << 7) == 0)     { *out16 = u; out16++; u32 = 0; continue; } //1 utf-8 char
			if ((u & 0x07 << 5) == 0xC0)  { type = 1; u32 = u & 0x1F; continue; }     //2 utf-8 char
			if ((u & 0x0F << 4) == 0xE0)  { type = 2; u32 = u & 0x0F; continue; }     //3 utf-8 char
			if ((u & 0x1F << 3) == 0xF0)  { type = 3; u32 = u & 0x07; continue; }     //4 utf-8 char
			err |= UTF_ERROR_ILLCHAR;
			continue;
		}
		else {
			if ((u & 0xC0) == 0x80) {
				u32 = (u32 << 6) | (u & 0x3F);
				type--;
			}
			else {
				u32 = 0; type = 0; err |= UTF_ERROR_ILLSEQ;
			}
		}
		if (type == 0) {
			if ((0 < u32 && u32 < 0xD800) || (0xE000 <= u32 && u32 < 0x10000)) {
				*out16 = u32;
				out16++;
			}
			else if (0x10000 <= u32 && u32 < 0x110000) {
				if (out16 + 1 >= out16end) break;
				u32 -= 0x10000;
				*out16 = 0xD800 + (u32 >> 10);
				out16++;
				*out16 = 0xDC00 + (u32 & 0x3FF);
				out16++;
			}
			u32 = 0;
		}

	}

	*out16 = *out16end = 0;

	if (*in8) err |= UTF_ERROR_SMALL;

	return err;
}

int is_ascii(const char *in8)
{
	for (; *in8; in8++)
		if (0x80 & *in8) return 0;

	return 1;
}

void utf_8_cut_end(char *inout8, size_t maxcutpoint)
{
	char *cur = inout8 + maxcutpoint;
	char cc;
	if (!inout8) return;

	cc = *cur;
}



char *alloc_utf_8_from_16(const wchar_t *in16, size_t add)
{
	size_t bsize = count_utf_8_from_16(in16);
	char *out8 = NULL;
	if (!bsize) return NULL;
	out8 = (char *)malloc(sizeof(char) * (bsize + add));
	conv_utf_16_to_8(in16, out8, bsize);
	return out8;
}

wchar_t *alloc_utf16_from_8(const char *in8, size_t add)
{
	size_t bsize = count_utf_16_from_8(in8);
	wchar_t *out16 = NULL;
	if (!bsize) return NULL;
	out16 = (wchar_t *) malloc(sizeof(wchar_t) * (bsize + add));
	conv_utf_8_to_16(in8, out16, bsize);
	return out16;
}

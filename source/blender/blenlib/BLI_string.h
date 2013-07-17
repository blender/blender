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
 */

#ifndef __BLI_STRING_H__
#define __BLI_STRING_H__

/** \file BLI_string.h
 *  \ingroup bli
 */

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

char *BLI_strdupn(const char *str, const size_t len)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

char *BLI_strdup(const char *str)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

char *BLI_strdupcat(const char *__restrict str1, const char *__restrict str2)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

char *BLI_strncpy(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

size_t BLI_strncpy_rlen(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

size_t BLI_strcpy_rlen(char *__restrict dst, const char *__restrict src)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

char *BLI_str_quoted_substrN(const char *__restrict str, const char *__restrict prefix)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

char *BLI_replacestr(char *__restrict str, const char *__restrict oldText, const char *__restrict newText)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

size_t BLI_snprintf(char *__restrict dst, size_t maxncpy, const char *__restrict format, ...)
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 4)))
__attribute__((nonnull))
#endif
;

size_t BLI_vsnprintf(char *__restrict dst, size_t maxncpy, const char *__restrict format, va_list arg)
#ifdef __GNUC__
__attribute__ ((format(printf, 3, 0)))
#endif
;

char *BLI_sprintfN(const char *__restrict format, ...)
#ifdef __GNUC__
__attribute__ ((format(printf, 1, 2)))
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

size_t BLI_strescape(char *__restrict dst, const char *__restrict src, const size_t maxncpy)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

int BLI_strcaseeq(const char *a, const char *b)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;

char *BLI_strcasestr(const char *s, const char *find)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
int BLI_strcasecmp(const char *s1, const char *s2)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
int BLI_strncasecmp(const char *s1, const char *s2, size_t len)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
int BLI_natstrcmp(const char *s1, const char *s2)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
size_t BLI_strnlen(const char *str, const size_t maxlen)
#ifdef __GNUC__
__attribute__((warn_unused_result))
__attribute__((nonnull))
#endif
;
void BLI_timestr(double _time, char *str, size_t maxlen)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

void BLI_ascii_strtolower(char *str, const size_t len)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
void BLI_ascii_strtoupper(char *str, const size_t len)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;
int BLI_str_rstrip_float_zero(char *str, const char pad)
#ifdef __GNUC__
__attribute__((nonnull))
#endif
;

#ifdef __cplusplus
}
#endif

#endif

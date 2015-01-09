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

#include "BLI_compiler_attrs.h"

char *BLI_strdupn(const char *str, const size_t len) ATTR_MALLOC ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

char *BLI_strdup(const char *str) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL() ATTR_MALLOC;

char *BLI_strdupcat(const char *__restrict str1, const char *__restrict str2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL() ATTR_MALLOC;

char *BLI_strncpy(char *__restrict dst, const char *__restrict src, const size_t maxncpy) ATTR_NONNULL();

char *BLI_strncpy_ensure_pad(char *__restrict dst, const char *__restrict src, const char pad, size_t maxncpy) ATTR_NONNULL();

size_t BLI_strncpy_rlen(char *__restrict dst, const char *__restrict src, const size_t maxncpy) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

size_t BLI_strcpy_rlen(char *__restrict dst, const char *__restrict src) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

char *BLI_str_quoted_substrN(const char *__restrict str, const char *__restrict prefix) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL() ATTR_MALLOC;

char *BLI_replacestrN(const char *__restrict str, const char *__restrict substr_old, const char *__restrict substr_new) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL() ATTR_MALLOC;

size_t BLI_snprintf(char *__restrict dst, size_t maxncpy, const char *__restrict format, ...) ATTR_NONNULL(1, 3) ATTR_PRINTF_FORMAT(3, 4);

size_t BLI_vsnprintf(char *__restrict dst, size_t maxncpy, const char *__restrict format, va_list arg) ATTR_PRINTF_FORMAT(3, 0);

char *BLI_sprintfN(const char *__restrict format, ...) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1) ATTR_MALLOC ATTR_PRINTF_FORMAT(1, 2);

size_t BLI_strescape(char *__restrict dst, const char *__restrict src, const size_t maxncpy) ATTR_NONNULL();

size_t BLI_str_format_int_grouped(char dst[16], int num) ATTR_NONNULL();

int BLI_strcaseeq(const char *a, const char *b) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
char *BLI_strcasestr(const char *s, const char *find) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_strcasecmp(const char *s1, const char *s2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_strncasecmp(const char *s1, const char *s2, size_t len) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_natstrcmp(const char *s1, const char *s2) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
int BLI_strcmp_ignore_pad(const char *str1, const char *str2, const char pad) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

size_t BLI_strnlen(const char *str, const size_t maxlen) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void BLI_timestr(double _time, char *str, size_t maxlen) ATTR_NONNULL();

void BLI_ascii_strtolower(char *str, const size_t len) ATTR_NONNULL();
void BLI_ascii_strtoupper(char *str, const size_t len) ATTR_NONNULL();
int BLI_str_rstrip_float_zero(char *str, const char pad) ATTR_NONNULL();

int BLI_str_index_in_array_n(const char *__restrict str, const char **__restrict str_array, const int str_array_len) ATTR_NONNULL();
int BLI_str_index_in_array(const char *__restrict str, const char **__restrict str_array) ATTR_NONNULL();

bool BLI_str_endswith(const char *__restrict str, const char *__restrict end) ATTR_NONNULL();
bool BLI_strn_endswith(const char *__restrict str, const char *__restrict end, int length) ATTR_NONNULL();

size_t BLI_str_partition(const char *str, const char delim[], char **sep, char **suf) ATTR_NONNULL();
size_t BLI_str_rpartition(const char *str, const char delim[], char **sep, char **suf) ATTR_NONNULL();
size_t BLI_str_partition_ex(const char *str, const char delim[], char **sep, char **suf, const bool from_right) ATTR_NONNULL();

#ifdef __cplusplus
}
#endif

#endif  /* __BLI_STRING_H__ */

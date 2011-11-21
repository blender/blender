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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_STRING_UTF8_H
#define BLI_STRING_UTF8_H

/** \file BLI_string_utf8.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

char        *BLI_strncpy_utf8(char *dst, const char *src, size_t maxncpy);
char        *BLI_strncat_utf8(char *dst, const char *src, size_t maxncpy);
int          BLI_utf8_invalid_byte(const char *str, int length);
int          BLI_utf8_invalid_strip(char *str, int length);

int          BLI_str_utf8_size(const char *p); /* warning, can return -1 on bad chars */
    /* copied from glib */
unsigned int BLI_str_utf8_as_unicode(const char *p);
unsigned int BLI_str_utf8_as_unicode_and_size(const char *p, size_t *index);
unsigned int BLI_str_utf8_as_unicode_step(const char *p, size_t *index);
size_t		 BLI_str_utf8_from_unicode(unsigned int c, char *outbuf);

char        *BLI_str_find_prev_char_utf8(const char *str, const char *p);
char        *BLI_str_find_next_char_utf8(const char *p, const char *end);
char        *BLI_str_prev_char_utf8(const char *p);

    /* wchar_t functions, copied from blenders own font.c originally */
size_t       BLI_wstrlen_utf8(const wchar_t *src);
size_t       BLI_strlen_utf8(const char *strc);
size_t       BLI_strncpy_wchar_as_utf8(char *dst, const wchar_t *src, const size_t maxcpy);
size_t       BLI_strncpy_wchar_from_utf8(wchar_t *dst, const char *src, const size_t maxcpy);

#define      BLI_UTF8_MAX 6
#define      BLI_UTF8_ERR ((unsigned int)-1)

#ifdef __cplusplus
}
#endif

#endif

/*
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
 */

/** \file \ingroup blt
 */

#ifndef __BLT_LANG_H__
#define __BLT_LANG_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Search the path directory to the locale files, this try all
 * the case for Linux, Win and Mac.
 * Also dynamically builds locales and locales' menu from "languages" text file.
 */
void BLT_lang_init(void);

/* Free languages and locales_menu arrays created by BLT_lang_init. */
void BLT_lang_free(void);

/* Set the current locale. */
void BLT_lang_set(const char *);
/* Get the current locale ([partial] ISO code, e.g. es_ES). */
const char *BLT_lang_get(void);

/* Get locale's elements (if relevant pointer is not NULL and element actually exists, e.g. if there is no variant,
 * *variant and *language_variant will always be NULL).
 * Non-null elements are always MEM_mallocN'ed, it's the caller's responsibility to free them.
 * NOTE: Always available, even in non-WITH_INTERNATIONAL builds.
 */
void BLT_lang_locale_explode(
        const char *locale, char **language, char **country, char **variant,
        char **language_country, char **language_variant);

/* Get EnumPropertyItem's for translations menu. */
struct EnumPropertyItem *BLT_lang_RNA_enum_properties(void);

#ifdef __cplusplus
};
#endif

#endif /* __BLT_LANG_H__ */

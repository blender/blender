/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation */

/** \file
 * \ingroup blt
 */

#pragma once

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

/* Get locale's elements (if relevant pointer is not NULL and element actually exists, e.g.
 * if there is no variant, *variant and *language_variant will always be NULL).
 * Non-null elements are always MEM_mallocN'ed, it's the caller's responsibility to free them.
 * NOTE: Always available, even in non-WITH_INTERNATIONAL builds.
 */
/**
 * Get locale's elements (if relevant pointer is not NULL and element actually exists, e.g.
 * if there is no variant,
 * *variant and *language_variant will always be NULL).
 * Non-null elements are always MEM_mallocN'ed, it's the caller's responsibility to free them.
 *
 * \note Keep that one always available, you never know,
 * may become useful even in no #WITH_INTERNATIONAL context.
 */
void BLT_lang_locale_explode(const char *locale,
                             char **language,
                             char **country,
                             char **variant,
                             char **language_country,
                             char **language_variant);

/* Get EnumPropertyItem's for translations menu. */
struct EnumPropertyItem *BLT_lang_RNA_enum_properties(void);

#ifdef __cplusplus
};
#endif

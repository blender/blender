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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenfont/intern/blf_lang.c
 *  \ingroup blf
 *
 * Main internationalization functions to set the locale and query available languages.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "RNA_types.h"

#include "BLF_translation.h" /* own include */

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "BKE_global.h"

#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#ifdef WITH_INTERNATIONAL

#include "boost_locale_wrapper.h"

/* Locale options. */
static const char **locales = NULL;
static int num_locales = 0;
static EnumPropertyItem *locales_menu = NULL;
static int num_locales_menu = 0;

static void free_locales(void)
{
	if (locales) {
		int idx = num_locales_menu - 1; /* Last item does not need to be freed! */
		while (idx--) {
			MEM_freeN((void *)locales_menu[idx].identifier);
			MEM_freeN((void *)locales_menu[idx].name);
			MEM_freeN((void *)locales_menu[idx].description); /* Also frees locales's relevant value! */
		}

		MEM_freeN((void *)locales);
		locales = NULL;
	}
	if (locales_menu) {
		MEM_freeN(locales_menu);
		locales_menu = NULL;
	}
	num_locales = num_locales_menu = 0;
}

static void fill_locales(void)
{
	const char * const languages_path = BLI_get_folder(BLENDER_DATAFILES, "locale");
	char languages[FILE_MAX];
	LinkNode *lines = NULL, *line;
	char *str;
	int idx = 0;

	free_locales();

	BLI_join_dirfile(languages, FILE_MAX, languages_path, "languages");
	line = lines = BLI_file_read_as_lines(languages);

	/* This whole "parsing" code is a bit weak, in that it expects strictly formated input file...
	 * Should not be a problem, though, as this file is script-generated! */

	/* First loop to find highest locale ID */
	while (line) {
		int t;
		str = (char *)line->link;
		if (str[0] == '#' || str[0] == '\0') {
			line = line->next;
			continue; /* Comment or void... */
		}
		t = atoi(str);
		if (t >= num_locales)
			num_locales = t + 1;
		num_locales_menu++;
		line = line->next;
	}
	num_locales_menu++; /* The "closing" void item... */

	/* And now, build locales and locale_menu! */
	locales_menu = MEM_callocN(num_locales_menu * sizeof(EnumPropertyItem), __func__);
	line = lines;
	/* Do not allocate locales with zero-sized mem, as LOCALE macro uses NULL locales as invalid marker! */
	if (num_locales > 0) {
		locales = MEM_callocN(num_locales * sizeof(char *), __func__);
		while (line) {
			int id;
			char *loc, *sep1, *sep2, *sep3;

			str = (char *)line->link;
			if (str[0] == '#' || str[0] == '\0') {
				line = line->next;
				continue;
			}

			id = atoi(str);
			sep1 = strchr(str, ':');
			if (sep1) {
				sep1++;
				sep2 = strchr(sep1, ':');
				if (sep2) {
					locales_menu[idx].value = id;
					locales_menu[idx].icon = 0;
					locales_menu[idx].name = BLI_strdupn(sep1, sep2 - sep1);

					sep2++;
					sep3 = strchr(sep2, ':');

					if (sep3) {
						locales_menu[idx].identifier = loc = BLI_strdupn(sep2, sep3 - sep2);
					}
					else {
						locales_menu[idx].identifier = loc = BLI_strdup(sep2);
					}

					if (id == 0) {
						/* The DEFAULT item... */
						if (BLI_strnlen(loc, 2)) {
							locales[id] = locales_menu[idx].description = BLI_strdup("");
						}
						/* Menu "label", not to be stored in locales! */
						else {
							locales_menu[idx].description = BLI_strdup("");
						}
					}
					else {
						locales[id] = locales_menu[idx].description = BLI_strdup(loc);
					}
					idx++;
				}
			}

			line = line->next;
		}
	}

	/* Add closing item to menu! */
	locales_menu[idx].identifier = NULL;
	locales_menu[idx].value = locales_menu[idx].icon = 0;
	locales_menu[idx].name = locales_menu[idx].description = "";

	BLI_file_free_lines(lines);
}
#endif  /* WITH_INTERNATIONAL */

EnumPropertyItem *BLF_RNA_lang_enum_properties(void)
{
#ifdef WITH_INTERNATIONAL
	return locales_menu;
#else
	return NULL;
#endif
}

void BLF_lang_init(void)
{
#ifdef WITH_INTERNATIONAL
	const char * const messagepath = BLI_get_folder(BLENDER_DATAFILES, "locale");

	if (messagepath) {
		bl_locale_init(messagepath, TEXT_DOMAIN_NAME);
		fill_locales();
	}
	else {
		printf("%s: 'locale' data path for translations not found, continuing\n", __func__);
	}
#else
#endif
}

void BLF_lang_free(void)
{
#ifdef WITH_INTERNATIONAL
	free_locales();
#else
#endif
}

#ifdef WITH_INTERNATIONAL
#  define ULANGUAGE ((U.language >= 0 && U.language < num_locales) ? U.language : 0)
#  define LOCALE(_id) (locales ? locales[(_id)] : "")
#endif

void BLF_lang_set(const char *str)
{
#ifdef WITH_INTERNATIONAL
	int ulang = ULANGUAGE;
	const char *short_locale = str ? str : LOCALE(ulang);
	const char *short_locale_utf8 = NULL;

	if ((U.transopts & USER_DOTRANSLATE) == 0)
		return;

	/* We want to avoid locales like '.UTF-8'! */
	if (short_locale[0]) {
		/* Hurrey! encoding needs to be placed *before* variant! */
		char *variant = strchr(short_locale, '@');
		if (variant) {
			char *locale = BLI_strdupn(short_locale, variant - short_locale);
			short_locale_utf8 = BLI_sprintfN("%s.UTF-8%s", locale, variant);
			MEM_freeN(locale);
		}
		else {
			short_locale_utf8 = BLI_sprintfN("%s.UTF-8", short_locale);
		}
	}
	else {
		short_locale_utf8 = short_locale;
	}

	bl_locale_set(short_locale_utf8);

	if (short_locale[0]) {
		MEM_freeN((void *)short_locale_utf8);
	}
#else
	(void)str;
#endif
}

/* Get the current locale (short code, e.g. es_ES). */
const char *BLF_lang_get(void)
{
#ifdef WITH_INTERNATIONAL
	const char *locale = LOCALE(ULANGUAGE);
	if (locale[0] == '\0') {
		/* Default locale, we have to find which one we are actually using! */
		locale = bl_locale_get();
	}
	return locale;
#else
	return "";
#endif
}

#undef LOCALE
#undef ULANGUAGE

/* Get locale's elements (if relevant pointer is not NULL and element actually exists, e.g. if there is no variant,
 * *variant and *language_variant will always be NULL).
 * Non-null elements are always MEM_mallocN'ed, it's the caller's responsibility to free them.
 * NOTE: Keep that one always available, you never know, may become useful even in no-WITH_INTERNATIONAL context...
 */
void BLF_locale_explode(const char *locale, char **language, char **country, char **variant,
                        char **language_country, char **language_variant)
{
	char *m1, *m2, *_t = NULL;

	m1 = strchr(locale, '_');
	m2 = strchr(locale, '@');

	if (language || language_variant) {
		if (m1 || m2) {
			_t = m1 ? BLI_strdupn(locale, m1 - locale) : BLI_strdupn(locale, m2 - locale);
			if (language)
				*language = _t;
		}
		else if (language) {
			*language = BLI_strdup(locale);
		}
	}
	if (country) {
		if (m1)
			*country = m2 ? BLI_strdupn(m1 + 1, m2 - (m1 + 1)) : BLI_strdup(m1 + 1);
		else
			*country = NULL;
	}
	if (variant) {
		if (m2)
			*variant = BLI_strdup(m2 + 1);
		else
			*variant = NULL;
	}
	if (language_country) {
		if (m1)
			*language_country = m2 ? BLI_strdupn(locale, m2 - locale) : BLI_strdup(locale);
		else
			*language_country = NULL;
	}
	if (language_variant) {
		if (m2)
			*language_variant = m1 ? BLI_strdupcat(_t, m2) : BLI_strdup(locale);
		else
			*language_variant = NULL;
	}
	if (_t && !language) {
		MEM_freeN(_t);
	}
}

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
 */


#include "BLF_translation.h" /* own include */

#include "BLI_utildefines.h"

#ifdef WITH_INTERNATIONAL

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "boost_locale_wrapper.h"

#include "BKE_global.h"

#include "DNA_userdef_types.h"

#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

/* Locale options. */
static const char **locales = NULL;
static int num_locales = 0;
static EnumPropertyItem *locales_menu = NULL;
static int num_locales_menu = 0;

#define ULANGUAGE ((U.language >= 0 && U.language < num_locales) ? U.language : 0)
#define LOCALE(_id) (locales ? locales[_id] : "")

static void free_locales(void)
{
	if (locales) {
		int idx = num_locales_menu - 1; /* Last item does not need to be freed! */
		while (idx--) {
			MEM_freeN((void *)locales_menu[idx].identifier);
			MEM_freeN((void *)locales_menu[idx].name);
			MEM_freeN((void *)locales_menu[idx].description); /* Also frees locales's relevant value! */
		}

		MEM_freeN(locales);
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
	char *languages_path = BLI_get_folder(BLENDER_DATAFILES, "locale");
	LinkNode *lines = NULL, *line;
	char *str;
	int idx = 0;

	free_locales();

	BLI_join_dirfile(languages_path, FILE_MAX, languages_path, "languages");
	line = lines = BLI_file_read_as_lines(languages_path);

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

	/* And now, buil locales and locale_menu! */
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

EnumPropertyItem *BLF_RNA_lang_enum_properties(void)
{
	return locales_menu;
}

void BLF_lang_init(void)
{
	char *messagepath = BLI_get_folder(BLENDER_DATAFILES, "locale");

	if (messagepath) {
		bl_locale_init(messagepath, TEXT_DOMAIN_NAME);
		fill_locales();
	}
	else {
		printf("%s: 'locale' data path for translations not found, continuing\n", __func__);
	}
}

void BLF_lang_free(void)
{
	free_locales();
}

void BLF_lang_set(const char *str)
{
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
}

const char *BLF_lang_get(void)
{
	int uilang = ULANGUAGE;
	return LOCALE(uilang);
}

#undef LOCALE
#undef ULANGUAGE

#else /* ! WITH_INTERNATIONAL */

void BLF_lang_init(void)
{
	return;
}

void BLF_lang_free(void)
{
	return;
}

void BLF_lang_set(const char *UNUSED(str))
{
	return;
}

const char *BLF_lang_get(void)
{
	return "";
}

#endif /* WITH_INTERNATIONAL */

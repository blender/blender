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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BKE_global.h"

#include "BLF_api.h"
#include "BLF_translation.h" /* own include */

#ifdef WITH_INTERNATIONAL

#include <locale.h>

#include "libintl.h"

#include "DNA_userdef_types.h"

#include "RNA_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_linklist.h"
#include "BLI_string.h"

#define SYSTEM_ENCODING_DEFAULT "UTF-8"
#define FONT_SIZE_DEFAULT 12

/* Locale options. */
static char global_messagepath[1024];
static char global_language[32];
static char global_encoding_name[32];

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
			MEM_freeN((void*)locales_menu[idx].identifier);
			MEM_freeN((void*)locales_menu[idx].name);
			MEM_freeN((void*)locales_menu[idx].description); /* Also frees locales's relevant value! */
		}
		MEM_freeN(locales);
		MEM_freeN(locales_menu);
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
		str = (char*) line->link;
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
	locales = MEM_callocN(num_locales * sizeof(char*), __func__);
	locales_menu = MEM_callocN(num_locales_menu * sizeof(EnumPropertyItem), __func__);
	line = lines;
	while (line) {
		int id;
		char *loc, *sep1, *sep2;

		str = (char*) line->link;
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
					locales_menu[idx].identifier = loc = BLI_strdup(sep2 + 1);
					if (id == 0) {
						/* The DEFAULT item... */
						if (BLI_strnlen(loc, 2))
							locales[id] = locales_menu[idx].description = BLI_strdup("");
						/* Menu "label", not to be stored in locales! */
						else
							locales_menu[idx].description = BLI_strdup("");
					}
					else
						locales[id] = locales_menu[idx].description = BLI_strdup(loc);
					idx++;
				
			}
		}

		line = line->next;
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

	BLI_strncpy(global_encoding_name, SYSTEM_ENCODING_DEFAULT, sizeof(global_encoding_name));

	if (messagepath) {
		BLI_strncpy(global_messagepath, messagepath, sizeof(global_messagepath));
		fill_locales();
	}
	else {
		printf("%s: 'locale' data path for translations not found, continuing\n", __func__);
		global_messagepath[0] = '\0';
	}
}

void BLF_lang_free(void)
{
	free_locales();
}

/* Get LANG/LANGUAGE environment variable. */
static void get_language_variable(const char *varname, char *var, const size_t maxlen)
{
	char *env = getenv(varname);

	if (env) {
		char *s;

		/* Store defaul locale. */
		BLI_strncpy(var, env, maxlen);

		/* Use first language as default. */
		s = strchr(var, ':');
		if (s)
			s[0] = 0;
	}
}

/* Get language to be used based on locale (which might be empty when using default language) and
 * LANG environment variable.
 */
static void get_language(const char *locale, const char *lang, char *language, const size_t maxlen)
{
	if (locale[0]) {
		BLI_strncpy(language, locale, maxlen);
	}
	else {
		char *s;

		BLI_strncpy(language, lang, maxlen);

		s = strchr(language, '.');
		if (s)
			s[0] = 0;
	}
}

/* XXX WARNING!!! In osx somehow the previous function call jumps in this one??? (ton, ppc) */
void BLF_lang_set(const char *str)
{
	char *locreturn;
	const char *short_locale;
	int ok = TRUE;
	int ulang = ULANGUAGE;

	if ((U.transopts & USER_DOTRANSLATE) == 0)
		return;

	if (str)
		short_locale = str;
	else
		short_locale = LOCALE(ulang);

#if defined(_WIN32) && !defined(FREE_WINDOWS)
	{
		if (short_locale) {
			char *envStr;

			if (ulang) /* Use system setting. */
				envStr = BLI_sprintfN("LANG=%s", getenv("LANG"));
			else
				envStr = BLI_sprintfN("LANG=%s", short_locale);

			gettext_putenv(envStr);
			MEM_freeN(envStr);
		}

		locreturn = setlocale(LC_ALL, short_locale);

		if (locreturn == NULL) {
			if (G.debug & G_DEBUG)
				printf("Could not change locale to %s\n", short_locale);

			ok = FALSE;
		}
	}
#else
	{
		static char default_lang[64] = "\0";
		static char default_language[64] = "\0";

		if (default_lang[0] == 0)
			get_language_variable("LANG", default_lang, sizeof(default_lang));

		if (default_language[0] == 0)
			get_language_variable("LANGUAGE", default_language, sizeof(default_language));

		if (short_locale[0]) {
			char *short_locale_utf8 = BLI_sprintfN("%s.UTF-8", short_locale);

			if (G.debug & G_DEBUG)
				printf("Setting LANG and LANGUAGE to %s\n", short_locale_utf8);

			locreturn = setlocale(LC_ALL, short_locale_utf8);

			if (locreturn != NULL) {
				BLI_setenv("LANG", short_locale_utf8);
				BLI_setenv("LANGUAGE", short_locale_utf8);
			}
			else {
				if (G.debug & G_DEBUG)
					printf("Setting LANG and LANGUAGE to %s\n", short_locale);

				locreturn = setlocale(LC_ALL, short_locale);

				if (locreturn != NULL) {
					BLI_setenv("LANG", short_locale);
					BLI_setenv("LANGUAGE", short_locale);
				}
			}

			if (G.debug & G_DEBUG && locreturn == NULL)
				printf("Could not change locale to %s nor %s\n", short_locale, short_locale_utf8);

			MEM_freeN(short_locale_utf8);
		}
		else {
			if (G.debug & G_DEBUG)
				printf("Setting LANG=%s and LANGUAGE=%s\n", default_lang, default_language);

			BLI_setenv("LANG", default_lang);
			BLI_setenv("LANGUAGE", default_language);
			locreturn = setlocale(LC_ALL, "");

			if (G.debug & G_DEBUG && locreturn == NULL)
				printf("Could not reset locale\n");
		}

		if (locreturn == NULL) {
			char language[65];

			get_language(short_locale, default_lang, language, sizeof(language));

			if (G.debug & G_DEBUG)
				printf("Fallback to LANG=%s and LANGUAGE=%s\n", default_lang, language);

			/* Fallback to default settings. */
			BLI_setenv("LANG", default_lang);
			BLI_setenv("LANGUAGE", language);

			locreturn = setlocale(LC_ALL, "");

			ok = FALSE;
		}
	}
#endif

	if (ok) {
		/*printf("Change locale to %s\n", locreturn ); */
		BLI_strncpy(global_language, locreturn, sizeof(global_language));
	}

	setlocale(LC_NUMERIC, "C");

	textdomain(TEXT_DOMAIN_NAME);
	bindtextdomain(TEXT_DOMAIN_NAME, global_messagepath);
	bind_textdomain_codeset(TEXT_DOMAIN_NAME, global_encoding_name);
}

const char *BLF_lang_get(void)
{
	int uilang = ULANGUAGE;
	return LOCALE(uilang);
}

void BLF_lang_encoding(const char *str)
{
	BLI_strncpy(global_encoding_name, str, sizeof(global_encoding_name));
	/* bind_textdomain_codeset(TEXT_DOMAIN_NAME, encoding_name); */
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

void BLF_lang_encoding(const char *str)
{
	(void)str;
	return;
}

void BLF_lang_set(const char *str)
{
	(void)str;
	return;
}

const char *BLF_lang_get(void)
{
	return "";
}

#endif /* WITH_INTERNATIONAL */

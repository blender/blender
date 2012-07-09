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

#if defined(_WIN32)
#include <windows.h>
#endif

#include "libintl.h"

#include "DNA_userdef_types.h"

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h" /* linknode */
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#define SYSTEM_ENCODING_DEFAULT "UTF-8"
#define FONT_SIZE_DEFAULT 12

/* locale options. */
static char global_messagepath[1024];
static char global_language[32];
static char global_encoding_name[32];

/* map from the rna_userdef.c:rna_def_userdef_system(BlenderRNA *brna):language_items */
static const char *locales[] = {
	"", "",
	"english", "en_US",
	"japanese", "ja_JP",
	"dutch", "nl_NL",
	"italian", "it_IT",
	"german", "de_DE",
	"finnish", "fi_FI",
	"swedish", "sv_SE",
	"french", "fr_FR",
	"spanish", "es",
	"catalan", "ca_AD",
	"czech", "cs_CZ",
	"ptb", "pt",
#if defined(_WIN32) && !defined(FREE_WINDOWS)
	"Chinese (Simplified)_China.1252", "zh_CN",
	"Chinese (Traditional)_China.1252", "zh_TW",
#else
	"chs", "zh_CN",
	"cht", "zh_TW",
#endif
	"russian", "ru_RU",
	"croatian", "hr_HR",
	"serbian", "sr_RS",
	"ukrainian", "uk_UA",
	"polish", "pl_PL",
	"romanian", "ro_RO",
	"arabic", "ar_EG",
	"bulgarian", "bg_BG",
	"greek", "el_GR",
	"korean", "ko_KR",
	"nepali", "ne_NP",
	"persian", "fa_IR",
	"indonesian", "id_ID",
	"serbian (latin)", "sr_RS@latin",
	"kyrgyz", "ky_KG",
	"turkish", "tr_TR",
	"hungarian", "hu_HU",
};

void BLF_lang_init(void)
{
	char *messagepath = BLI_get_folder(BLENDER_DATAFILES, "locale");
/*	printf("%s\n", messagepath);*/

	BLI_strncpy(global_encoding_name, SYSTEM_ENCODING_DEFAULT, sizeof(global_encoding_name));
	
	if (messagepath) {
		BLI_strncpy(global_messagepath, messagepath, sizeof(global_messagepath));
	}
	else {
		printf("%s: 'locale' data path for translations not found, continuing\n", __func__);
		global_messagepath[0] = '\0';
	}
	
}

/* get LANG/LANGUAGE environment variable */
static void get_language_variable(const char *varname, char *var, int maxlen)
{
	char *env = getenv(varname);

	if (env) {
		char *s;

		/* store defaul locale */
		BLI_strncpy(var, env, maxlen);

		/* use first language as default */
		s = strchr(var, ':');
		if (s)
			s[0] = 0;
	}
}

/* get language to be used based on locale(which might be empty when using default language) and
 * LANG environment variable
 */
static void get_language(const char *locale, const char *lang, char *language, int maxlen)
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

/* XXX WARNING!!! IN osx somehow the previous function call jumps in this one??? (ton, ppc) */
void BLF_lang_set(const char *str)
{
	char *locreturn;
	const char *short_locale;
	int ok = 1;
	const char *long_locale = locales[2 * U.language];

	if ((U.transopts & USER_DOTRANSLATE) == 0)
		return;

	if (str)
		short_locale = str;
	else
		short_locale = locales[2 * U.language + 1];

#if defined(_WIN32) && !defined(FREE_WINDOWS)
	if (short_locale) {
		char *envStr;

		if (U.language == 0) /* use system setting */
			envStr = BLI_sprintfN("LANG=%s", getenv("LANG"));
		else
			envStr = BLI_sprintfN("LANG=%s", short_locale);

		gettext_putenv(envStr);
		MEM_freeN(envStr);
	}

	locreturn = setlocale(LC_ALL, long_locale);

	if (locreturn == NULL) {
		if (G.debug & G_DEBUG)
			printf("Could not change locale to %s\n", long_locale);

		ok = 0;
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
			if (G.debug & G_DEBUG)
				printf("Setting LANG= and LANGUAGE to %s\n", short_locale);

			BLI_setenv("LANG", short_locale);
			BLI_setenv("LANGUAGE", short_locale);
		}
		else {
			if (G.debug & G_DEBUG)
				printf("Setting LANG=%s and LANGUAGE=%s\n", default_lang, default_language);

			BLI_setenv("LANG", default_lang);
			BLI_setenv("LANGUAGE", default_language);
		}

		locreturn = setlocale(LC_ALL, short_locale);

		if (locreturn == NULL) {
			char *short_locale_utf8 = NULL;

			if (short_locale[0]) {
				short_locale_utf8 = BLI_sprintfN("%s.UTF-8", short_locale);
				locreturn = setlocale(LC_ALL, short_locale_utf8);
			}

			if (locreturn == NULL) {
				char language[65];

				get_language(long_locale, default_lang, language, sizeof(language));

				if (G.debug & G_DEBUG) {
					if (short_locale[0])
						printf("Could not change locale to %s nor %s\n", short_locale, short_locale_utf8);
					else
						printf("Could not reset locale\n");

					printf("Fallback to LANG=%s and LANGUAGE=%s\n", default_lang, language);
				}

				/* fallback to default settings */
				BLI_setenv("LANG", default_lang);
				BLI_setenv("LANGUAGE", language);

				locreturn = setlocale(LC_ALL, "");

				ok = 0;
			}

			if (short_locale_utf8)
				MEM_freeN(short_locale_utf8);
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
	return locales[2 * U.language + 1];
}

void BLF_lang_encoding(const char *str)
{
	BLI_strncpy(global_encoding_name, str, sizeof(global_encoding_name));
	/* bind_textdomain_codeset(TEXT_DOMAIN_NAME, encoding_name); */
}

#else /* ! WITH_INTERNATIONAL */

void BLF_lang_init(void)
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

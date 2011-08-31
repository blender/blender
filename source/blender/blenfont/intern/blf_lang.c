/*
 * $Id$
 *
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

#include "BLF_api.h"

#ifdef INTERNATIONAL

#include <locale.h>
#include "libintl.h"

#include "DNA_userdef_types.h"

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_linklist.h"	/* linknode */
#include "BLI_string.h"
#include "BLI_path_util.h"


#ifdef __APPLE__

#endif

#define DOMAIN_NAME "blender"
#define SYSTEM_ENCODING_DEFAULT "UTF-8"
#define FONT_SIZE_DEFAULT 12

/* locale options. */
static char global_messagepath[1024];
static char global_language[32];
static char global_encoding_name[32];

/* map from the rna_userdef.c:rna_def_userdef_system(BlenderRNA *brna):language_items */

static char *long_locales[] = {
	"",
	"english",
	"japanese",
	"dutch",
	"italian",
	"german",
	"finnish",
	"swedish",
	"french",
	"spanish",
	"catalan",
	"czech",
	"ptb",
	"chs",
	"cht",
	"russian",
	"croatian",
	"serbian",
	"ukrainian",
	"polish",
	"romanian",
	"arabic",
	"bulgarian",
	"greek",
	"korean"
};

static char short_locale_default[] = "";
static char short_locale_english[] = "en_US";
static char short_locale_japanese[] = "ja_JP";
static char short_locale_dutch[] = "nl_NL";
static char short_locale_italian[] = "it_IT";
static char short_locale_german[] = "de_DE";
static char short_locale_finnish[] = "fi_FI";
static char short_locale_swedish[] = "sv_SE";
static char short_locale_french[] = "fr_FR";
static char short_locale_spanish[] = "es_ES";
static char short_locale_catalan[] = "ca_AD";
static char short_locale_czech[] = "cs_CZ";
static char short_locale_bra_portuguese[] = "pt_BR";
static char short_locale_sim_chinese[] = "zh_CN";
static char short_locale_tra_chinese[] = "zh_TW";
static char short_locale_russian[] = "ru_RU";
static char short_locale_croatian[] = "hr_HR";
static char short_locale_serbian[] = "sr_RS";
static char short_locale_ukrainian[] = "uk_UA";
static char short_locale_polish[] = "pl_PL";
static char short_locale_romanian[] = "ro_RO";
static char short_locale_arabic[] = "ar_EG";
static char short_locale_bulgarian[] = "bg_BG";
static char short_locale_greek[] = "el_GR";
static char short_locale_korean[] = "ko_KR";

static char *short_locales[] = {
		short_locale_default,
		short_locale_english, /* us english is the default language of blender */
		short_locale_japanese,
		short_locale_dutch,
		short_locale_italian,
		short_locale_german,
		short_locale_finnish,
		short_locale_swedish,
		short_locale_french,
		short_locale_spanish,
		short_locale_catalan,
		short_locale_czech,
		short_locale_bra_portuguese,
		short_locale_sim_chinese,
		short_locale_tra_chinese,
		short_locale_russian,
		short_locale_croatian,
		short_locale_serbian,
		short_locale_ukrainian,
		short_locale_polish,
		short_locale_romanian,
		short_locale_arabic,
		short_locale_bulgarian,
		short_locale_greek,
		short_locale_korean,
};

void BLF_lang_init(void)
{
	char *messagepath= BLI_get_folder(BLENDER_DATAFILES, "locale");
	
	BLI_strncpy(global_encoding_name, SYSTEM_ENCODING_DEFAULT, sizeof(global_encoding_name));
	
	if (messagepath)
		BLI_strncpy(global_messagepath, messagepath, sizeof(global_messagepath));
	else
		global_messagepath[0]= '\0';
	
}

/* XXX WARNING!!! IN osx somehow the previous function call jumps in this one??? (ton, ppc) */
void BLF_lang_set(const char *str)
{
	char *locreturn;
	char *short_locale;
#if defined (_WIN32)
	char *long_locale = long_locales[U.language];
#endif

	if(str)
		short_locale = str;
	else
		short_locale = short_locales[U.language];

	if(short_locale)
	{
		BLI_setenv("LANG", short_locale);
		BLI_setenv("LANGUAGE", short_locale);
	}

#if defined (_WIN32)
	locreturn= setlocale(LC_ALL, long_locale);
	if (locreturn == NULL) {
		printf("Could not change locale to %s\n", long_locale);
	}
#else
	locreturn= setlocale(LC_ALL, short_locale);
	if (locreturn == NULL) {
		char *short_locale_utf8 = BLI_sprintfN("%s.UTF-8", short_locale);

		locreturn= setlocale(LC_ALL, short_locale_utf8);
		if (locreturn == NULL) {
			printf("Could not change locale to %s nor %s\n", short_locale, short_locale_utf8);
		}

		MEM_freeN(short_locale_utf8);
	}
#endif
	else
	{
		printf("Change locale to %s\n", locreturn );
		BLI_strncpy(global_language, locreturn, sizeof(global_language));
	}
	setlocale(LC_NUMERIC, "C");

	textdomain(DOMAIN_NAME);
	bindtextdomain(DOMAIN_NAME, global_messagepath);
	/* bind_textdomain_codeset(DOMAIN_NAME, global_encoding_name); */	
}

void BLF_lang_encoding(const char *str)
{
	BLI_strncpy(global_encoding_name, str, sizeof(global_encoding_name));
	/* bind_textdomain_codeset(DOMAIN_NAME, encoding_name); */
}

#else /* ! INTERNATIONAL */

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

#endif /* INTERNATIONAL */

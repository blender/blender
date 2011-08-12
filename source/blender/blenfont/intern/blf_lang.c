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
static char locale_default[] = "";
static char locale_english[] = "en_US";
static char locale_japanese[] = "ja_JP";
static char locale_dutch[] = "nl_NL";
static char locale_italian[] = "it_IT";
static char locale_german[] = "de_DE";
static char locale_finnish[] = "fi_FI";
static char locale_swedish[] = "sv_SE";
static char locale_french[] = "fr_FR";
static char locale_spanish[] = "es_ES";
static char locale_catalan[] = "ca_AD";
static char locale_czech[] = "cs_CZ";
static char locale_bra_portuguese[] = "pt_BR";
static char locale_sim_chinese[] = "zh_CN";
static char locale_tra_chinese[] = "zh_TW";
static char locale_russian[] = "ru_RU";
static char locale_croatian[] = "hr_HR";
static char locale_serbian[] = "sr_RS";
static char locale_ukrainian[] = "uk_UA";
static char locale_polish[] = "pl_PL";
static char locale_romanian[] = "ro_RO";
static char locale_arabic[] = "ar_EG";
static char locale_bulgarian[] = "bg_BG";
static char locale_greek[] = "el_GR";
static char locale_korean[] = "ko_KR";

static char *lang_to_locale[] = {
		locale_default,
		locale_english, /* us english is the default language of blender */
		locale_japanese,
		locale_dutch,
		locale_italian,
		locale_german,
		locale_finnish,
		locale_swedish,
		locale_french,
		locale_spanish,
		locale_catalan,
		locale_czech,
		locale_bra_portuguese,
		locale_sim_chinese,
		locale_tra_chinese,
		locale_russian,
		locale_croatian,
		locale_serbian,
		locale_ukrainian,
		locale_polish,
		locale_romanian,
		locale_arabic,
		locale_bulgarian,
		locale_greek,
		locale_korean,
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
	if(str==NULL)
		str = lang_to_locale[U.language];
	if( str[0]!=0 )
	{
		BLI_setenv("LANG", str);
		BLI_setenv("LANGUAGE", str);
	}

	locreturn= setlocale(LC_ALL, str);
	if (locreturn == NULL) {
		char *lang= BLI_sprintfN("%s.UTF-8", str);

		locreturn= setlocale(LC_ALL, lang);
		if (locreturn == NULL) {
			printf("could not change language to %s nor %s\n", str, lang);
		}

		MEM_freeN(lang);
	}

	setlocale(LC_NUMERIC, "C");

	textdomain(DOMAIN_NAME);
	bindtextdomain(DOMAIN_NAME, global_messagepath);
	/* bind_textdomain_codeset(DOMAIN_NAME, global_encoding_name); */
	BLI_strncpy(global_language, str, sizeof(global_language));
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

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
static char *locales[] = {
	"", "",
	"english", "en_US",
	"japanese", "ja_JP",
	"dutch", "nl_NL",
	"italian", "it_IT",
	"german", "de_DE",
	"finnish", "fi_FI",
	"swedish", "sv_SE",
	"french", "fr_FR",
	"spanish", "es_ES",
	"catalan", "ca_AD",
	"czech", "cs_CZ",
	"ptb", "pt_BR",
	"chs", "zh_CN",
	"cht", "zh_TW",
	"russian", "ru_RU",
	"croatian", "hr_HR",
	"serbian", "sr_RS",
	"ukrainian", "uk_UA",
	"polish", "pl_PL",
	"romanian", "ro_RO",
	"arabic", "ar_EG",
	"bulgarian", "bg_BG",
	"greek", "el_GR",
	"korean" "ko_KR",
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
	char *long_locale = locales[ 2 * U.language];
#endif

	if(str)
		short_locale = str;
	else
		short_locale = locales[ 2 * U.language + 1];

#if defined (_WIN32)
	if(short_locale)
	{
		char *envStr;
		if( U.language==0 )/* use system setting */
			envStr = BLI_sprintfN( "LANG=%s", getenv("LANG") );
		else
			envStr = BLI_sprintfN( "LANG=%s", short_locale );
		gettext_putenv(envStr);
		MEM_freeN(envStr);
	}
	locreturn= setlocale(LC_ALL, long_locale);
	if (locreturn == NULL) {
		printf("Could not change locale to %s\n", long_locale);
	}
#else
	if(short_locale)
	{
		BLI_setenv("LANG", short_locale);
		BLI_setenv("LANGUAGE", short_locale);
	}

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

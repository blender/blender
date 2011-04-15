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
	if(str==NULL) {
		return;
	}
	else {
	
#if defined (_WIN32) || defined(__APPLE__)
		BLI_setenv("LANG", str);
#else
		char *locreturn= setlocale(LC_ALL, str);
		if (locreturn == NULL) {
			char *lang= BLI_sprintfN("%s.UTF-8", str);

			locreturn= setlocale(LC_ALL, lang);
			if (locreturn == NULL) {
				printf("could not change language to %s nor %s\n", str, lang);
			}

			MEM_freeN(lang);
		}

		setlocale(LC_NUMERIC, "C");
#endif
		textdomain(DOMAIN_NAME);
		bindtextdomain(DOMAIN_NAME, global_messagepath);
		/* bind_textdomain_codeset(DOMAIN_NAME, global_encoding_name); */
		BLI_strncpy(global_language, str, sizeof(global_language));
		
	}
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

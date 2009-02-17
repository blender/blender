/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 * 
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WITH_FREETYPE2
#include <ft2build.h>

#include FT_FREETYPE_H
#include FT_GLYPH_H
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */
#include "BLI_string.h"

#include "BIF_gl.h"

#include "blf_internal_types.h"

// XXX 2.50 Remove this later.
#ifdef WITH_FREETYPE2
#include "FTF_Api.h"
#endif

static ListBase global_lang= { NULL, NULL };
static int global_tot_lang= 0;
static int global_err_lang= 0;

int BLF_lang_error(void)
{
	return(global_err_lang);
}

char *BLF_lang_pup(void)
{
	LangBLF *lme;
	static char string[1024];
	static char tmp[1024];

	if(global_tot_lang == 0)
		sprintf(string, "Choose Language: %%t|Language:  English %%x0");
	else {
		lme= global_lang.first;
		sprintf(string, "Choose Language: %%t");
		while (lme) {
			sprintf(tmp, "|Language:  %s %%x%d", lme->language, lme->id);
			strcat(string, tmp);
			lme= lme->next;
		}
	}

	return(string);
}

LangBLF *blf_lang_find_by_id(short langid)
{
	LangBLF *p;

	p= global_lang.first;
	while (p) {
		if (p->id == langid)
			return(p);
		p= p->next;
	}
	return(NULL);
}

char *BLF_lang_find_code(short langid)
{
	LangBLF *p;

	p= blf_lang_find_by_id(langid);
	if (p)
		return(p->code);
	return(NULL);
}

void BLF_lang_set(int id)
{
#ifdef WITH_FREETYPE2
	LangBLF *lme;

	// XXX 2.50 Remove this later, with ftfont
	lme= blf_lang_find_by_id(id);
	if(lme) FTF_SetLanguage(lme->code);
	else FTF_SetLanguage("en_US");
#endif
}

static void blf_lang_split(char *line, LangBLF* lme)
{
	char *dpointchar= strchr(line, ':');

	if (dpointchar) {
		lme->code= BLI_strdup(dpointchar+1);
		*(dpointchar)=0;
		lme->language= BLI_strdup(line);
	} else {
		lme->code= NULL;
		lme->language= NULL;
		/* XXX 2.50 bad call error("Invalid language file");
		 * If we set this to NULL, the function blf_lang_new
		 * drop the line and increment the error lang value
		 * so the init code can call BLF_lang_error to get
		 * the number of invalid lines and show the error.
		 */
	}
}

LangBLF *blf_lang_find(char *s, int find_by)
{
	LangBLF *p;

	p= global_lang.first;
	while (p) {
		if (find_by == BLF_LANG_FIND_BY_LINE) {
			if (BLI_streq(s, p->line))
				return(p);
		}
		else if (find_by == BLF_LANG_FIND_BY_CODE) {
			if (BLI_streq(s, p->code))
				return(p);
		}
		else if (find_by == BLF_LANG_FIND_BY_LANGUAGE) {
			if (BLI_streq(s, p->language))
				return(p);
		}
		p= p->next;
	}
	return(NULL);
}

static void blf_lang_new(char *line)
{
	LangBLF *lme;

	lme= blf_lang_find(line, BLF_LANG_FIND_BY_LINE);
	if (!lme) {
		lme= MEM_mallocN(sizeof(LangBLF), "blf_lang_new");
		lme->next= NULL;
		lme->prev= NULL;
		lme->line = BLI_strdup(line);
		blf_lang_split(line, lme);
		
		if (lme->code && lme->language) {
			lme->id = global_tot_lang;
			global_tot_lang++;
			BLI_addhead(&global_lang, lme);
		}
		else {
			global_err_lang++;
			MEM_freeN(lme->line);
			MEM_freeN(lme);
		}
	}
}

int BLF_lang_init(void) 
{
	char name[FILE_MAXDIR+FILE_MAXFILE];
	LinkNode *l, *lines;
	
	/* .Blanguages, http://www.blender3d.org/cms/Installation_Policy.352.0.html*/
#if defined (__APPLE__) || (WIN32)
	BLI_make_file_string("/", name, BLI_gethome(), ".Blanguages");
#else
	BLI_make_file_string("/", name, BLI_gethome(), ".blender/.Blanguages");
#endif

	lines= BLI_read_file_as_lines(name);

	if(lines == NULL) {
		/* If not found in home, try current dir 
		 * (Resources folder of app bundle on OS X) */
#if defined (__APPLE__)
		char *bundlePath = BLI_getbundle();
		strcpy(name, bundlePath);
		strcat(name, "/Contents/Resources/.Blanguages");
#else
		/* Check the CWD. Takes care of the case where users
		 * unpack blender tarball; cd blender-dir; ./blender */
		strcpy(name, ".blender/.Blanguages");
#endif
		lines= BLI_read_file_as_lines(name);

		if(lines == NULL) {
			/* If not found in .blender, try current dir */
			strcpy(name, ".Blanguages");
			lines= BLI_read_file_as_lines(name);
			if(lines == NULL) {
// XXX 2.50				if(G.f & G_DEBUG)
				printf("File .Blanguages not found\n");
				return(0);
			}
		}
	}

	for (l= lines; l; l= l->next) {
		char *line= l->link;
			
		if (!BLI_streq(line, "")) {
			blf_lang_new(line);
		}
	}

	BLI_free_file_lines(lines);
	return(1);
}

void BLF_lang_exit(void)
{
	LangBLF *p;

	while (global_lang.first) {
		p= global_lang.first;
		BLI_remlink(&global_lang, p);
		MEM_freeN(p->line);
		MEM_freeN(p->language);
		MEM_freeN(p->code);
		MEM_freeN(p);
	}
}

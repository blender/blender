/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * The Original Code is written by Rob Haarsma (phase)
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */


#include <string.h>
#include <stdlib.h>

#include "DNA_userdef_types.h"

#include "BKE_global.h"		/* G */
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */

#include "BIF_language.h"
#include "BIF_space.h"		/* allqueue() */
#include "BIF_toolbox.h"	/* error() */

#include "MEM_guardedalloc.h"	/* vprintf, etc ??? */

#include "mydevice.h"		/* REDRAWALL */

#include "BMF_Api.h"

#ifdef INTERNATIONAL
#include "FTF_Api.h"

typedef struct _LANGMenuEntry LANGMenuEntry;
struct _LANGMenuEntry {
	LANGMenuEntry *next;

	char *line;
	char *language;
	char *code;
	int id;
};

static LANGMenuEntry *langmenu= 0;
static int tot_lang = 0;
#endif // INTERNATIONAL

int BIF_DrawString(BMF_Font* font, char *str, int translate, int col)
{

#ifdef INTERNATIONAL
	if(G.ui_international == TRUE)
		if(translate)
			return FTF_DrawString(str, FTF_USE_GETTEXT | FTF_INPUT_UTF8, col);
		else
			return FTF_DrawString(str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, col);
	else
		return BMF_DrawString(font, str);
#else
	return BMF_DrawString(font, str);
#endif

}

int BIF_DrawStringRGB(BMF_Font* font, char *str, int translate, float r, float g, float b)
{

#ifdef INTERNATIONAL
	if(G.ui_international == TRUE)
		if(translate)
			return FTF_DrawStringRGB(str, FTF_USE_GETTEXT | FTF_INPUT_UTF8, r, g, b);
		else
			return FTF_DrawStringRGB(str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8, r, g, b);
	else
		return BMF_DrawString(font, str);
#else
	return BMF_DrawString(font, str);
#endif

}


float BIF_GetStringWidth(BMF_Font* font, char *str, int translate)
{
	float rt;

#ifdef INTERNATIONAL
	if(G.ui_international == TRUE)	//versionnumber
		if(U.transopts & TR_BUTTONS)
			rt= FTF_GetStringWidth(str, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
		else
			rt= FTF_GetStringWidth(str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
	else
		rt= BMF_GetStringWidth(font, str);
#else
	rt= BMF_GetStringWidth(font, str);
#endif

	return rt;
}


#ifdef INTERNATIONAL

char *fontsize_pup(void)
{
	static char string[1024];
	char formatstring[1024];

	strcpy(formatstring, "Choose Font Size: %%t|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d|%s %%x%d");

	sprintf(string, formatstring,
			"Font Size:   8",		8,
			"Font Size:   9",		9,
			"Font Size:  10",		10,
			"Font Size:  11",		11,
			"Font Size:  12",		12,
			"Font Size:  13",		13,
			"Font Size:  14",		14,
			"Font Size:  15",		15,
			"Font Size:  16",		16
	       );

	return (string);
}


char *language_pup(void)
{
	LANGMenuEntry *lme = langmenu;
	static char string[1024];
	static char tmp[1024];

	if(tot_lang == 0)
		sprintf(string, "Choose Language: %%t|Language:  English %%x0");
	else {
		sprintf(string, "Choose Language: %%t");
		while(lme) {
			sprintf(tmp, "|Language:  %s %%x%d", lme->language, lme->id);
			strcat(string, tmp);
			lme= lme->next;
		}
	}

	return string;
}


LANGMenuEntry *find_language(short langid){
	LANGMenuEntry *lme = langmenu;

	while(lme) {
		if(lme->id == langid)
			return lme;

		lme=lme->next;
	}
	return NULL;
}


void lang_setlanguage(void) {
	LANGMenuEntry *lme;

	lme = find_language(U.language);
	if(lme) FTF_SetLanguage(lme->code);
	else FTF_SetLanguage("en_US");
}


void set_interface_font(char *str) {
	char di[FILE_MAXDIR];

	if(FTF_SetFont(str, U.fontsize)) {
		lang_setlanguage();
		BLI_split_dirfile(str, di, U.fontname);

		if(strlen(di) < FILE_MAXDIR) strcpy(U.fontdir, di);

		G.ui_international = TRUE;
	} else {
		sprintf(U.fontname, "Invalid font.");
		G.ui_international = FALSE;
	}
	allqueue(REDRAWALL, 0);
}


void start_interface_font(void) {
	char tstr[FILE_MAXDIR+FILE_MAXFILE];
	int result = 0;
#ifdef __APPLE__
	char *bundlepath;
#endif
	
	/* hack to find out if we have saved language/font settings.
	   if not, set defaults and try .bfont.tff --phase */
	
	if(U.fontsize != 0) { // we have saved user settings
		// try load the font from the font dir
		BLI_make_file_string("/", tstr, U.fontdir, U.fontname);
		result = FTF_SetFont(tstr, U.fontsize);

		if(!result) {	// else try loading font from current dir
			result = FTF_SetFont(U.fontname, U.fontsize);
		}
	} else {
		U.language= 0;
		U.fontsize= 11;
		U.encoding= 0;

#if defined (__APPLE__)
		bundlepath = BLI_getbundle();
		strcpy(tstr, bundlepath);
		strcat(tstr, "/Contents/Resources/");
		strcat(tstr, ".bfont.ttf");
		result = FTF_SetFont(tstr, U.fontsize);

		sprintf(U.fontname, ".blender/.bfont.ttf\0");
#elif defined (WIN32)
		BLI_getInstallationDir(tstr);
		strcat(tstr, "/.blender/.bfont.ttf\0");
		result = FTF_SetFont(tstr, U.fontsize);

		sprintf(U.fontname, ".blender/.bfont.ttf\0");
#else
		sprintf(U.fontname, ".blender/.bfont.ttf\0");

		result = FTF_SetFont(U.fontname, U.fontsize);
#endif
	}

	if(result) {
		lang_setlanguage();

		G.ui_international = TRUE;
	} else {
		printf("no font found for international support\n");
		G.ui_international = FALSE;
		U.transopts &= ~TR_ALL;
		U.fontsize = 0;
	}

	allqueue(REDRAWALL, 0);
}


char *first_dpointchar(char *string) {
	char *dpointchar;
	
	dpointchar= strchr(string, ':');	

	return dpointchar;
}


void splitlangline(char *line, LANGMenuEntry *lme)
{
	char *dpointchar= first_dpointchar(line);

	if (dpointchar) {
		lme->code= BLI_strdup(dpointchar+1);
		*(dpointchar)=0;
		lme->language= BLI_strdup(line);
	} else {
		error("Invalid language file");
	}
}


void puplang_insert_entry(char *line)
{
	LANGMenuEntry *lme, *prev;
	int sorted = 0;

	prev= NULL;
	lme= langmenu;

	for (; lme; prev= lme, lme= lme->next) {
		if (lme->line) {
			if (BLI_streq(line, lme->line)) {
				return;
			} else if (sorted && strcmp(line, lme->line)<0) {
				break;
			}
		}
	}
	
	lme= MEM_mallocN(sizeof(*lme), "lme");
	lme->line = BLI_strdup(line);
	splitlangline(line, lme);
	lme->id = tot_lang;
	tot_lang++;

	if (prev) {
		lme->next= prev->next;
		prev->next= lme;
	} else {
		lme->next= langmenu;
		langmenu= lme;
	}
}


int read_languagefile(void) {
	char name[FILE_MAXDIR+FILE_MAXFILE];
	LinkNode *l, *lines;
#ifdef WIN32
	int result;
#endif
	
	/* .Blanguages */
	BLI_make_file_string("/", name, BLI_gethome(), ".Blanguages");

	lines= BLI_read_file_as_lines(name);

	if(lines == NULL) {
		/* If not found in home, try current dir 
		 * (Resources folder of app bundle on OS X) */
#if defined (__APPLE__)
		char *bundlePath = BLI_getbundle();
		strcpy(name, bundlePath);
		strcat(name, "/Contents/Resources/.Blanguages");
#elif defined (WIN32)
		/* Check the installation dir in Windows */
		result = BLI_getInstallationDir(name);
		if (!result)
			strcpy(name,"/.blender/.Blanguages");
		else
			strcat(name,"/.blender/.Blanguages");
#else
		strcpy(name, ".blender/.Blanguages");
#endif
		lines= BLI_read_file_as_lines(name);

		if(lines == NULL) {
			/* If not found in .blender, try current dir */
			strcpy(name, ".Blanguages");
			lines= BLI_read_file_as_lines(name);
			if(lines == NULL) {
					error("File \".Blanguages\" not found");
					return 0;
			}
		}
	}

	for (l= lines; l; l= l->next) {
		char *line= l->link;
			
		if (!BLI_streq(line, "")) {
			puplang_insert_entry(line);
		}
	}

	BLI_free_file_lines(lines);

	return 1;
}


void free_languagemenu(void)
{
	LANGMenuEntry *lme= langmenu;

	while (lme) {
		LANGMenuEntry *n= lme->next;

		if (lme->line) MEM_freeN(lme->line);
		if (lme->language) MEM_freeN(lme->language);
		if (lme->code) MEM_freeN(lme->code);
		MEM_freeN(lme);

		lme= n;
	}
}

#endif /* INTERNATIONAL */

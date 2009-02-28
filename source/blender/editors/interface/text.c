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
 * The Original Code is written by Rob Haarsma (phase)
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* XXX 2.50 this file must be cleanup still, using globals etc. */

#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"
#include "DNA_vec_types.h"

#include "BKE_global.h"		/* G */
#include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_linklist.h"	/* linknode */

#include "BIF_gl.h"
#include "UI_text.h"
#include "BLF_api.h"

#include "ED_datafiles.h"

#include "BMF_Api.h"

#ifdef WITH_ICONV
#include "iconv.h"

void string_to_utf8(char *original, char *utf_8, char *code)
{
	size_t inbytesleft=strlen(original);
	size_t outbytesleft=512;
	size_t rv=0;
	iconv_t cd;
	
	cd=iconv_open("UTF-8", code);

	if (cd == (iconv_t)(-1)) {
		printf("iconv_open Error");
		*utf_8='\0';
		return ;
	}
	rv=iconv(cd, &original, &inbytesleft, &utf_8, &outbytesleft);
	if (rv == (size_t) -1) {
		printf("iconv Error\n");
		return ;
	}
	*utf_8 = '\0';
	iconv_close(cd);
}
#endif // WITH_ICONV

#ifdef INTERNATIONAL
#include "FTF_Api.h"
#endif

void UI_RasterPos(float x, float y)
{
#ifdef INTERNATIONAL
	FTF_SetPosition(x, y);
#endif // INTERNATIONAL
}

void UI_SetScale(float aspect)
{
#ifdef INTERNATIONAL
	FTF_SetScale(aspect);
#endif // INTERNATIONAL
}

void ui_text_init_userdef(void)
{
	int id;

	id= BLF_load_mem("default", (unsigned char*)datatoc_bfont_ttf, datatoc_bfont_ttf_size);
	if (id == -1)
		printf("Warning can't load built-in font ??\n");
	else {
		BLF_set(id);
		BLF_size(12, 72);
		BLF_size(11, 96);
		BLF_size(14, 96);
	}

#ifdef INTERNATIONAL
	if(U.transopts & USER_DOTRANSLATE)
		start_interface_font();
	else
		G.ui_international= FALSE;
#else // INTERNATIONAL
	G.ui_international= FALSE;
#endif
}

int UI_DrawString(BMF_Font* font, char *str, int translate)
{
#ifdef INTERNATIONAL
	if(G.ui_international == TRUE) {
		if(translate)
		{
#ifdef WITH_ICONV
			if(translate & CONVERT_TO_UTF8) {
				char utf_8[512];
				char *code;

				code= BLF_lang_find_code(U.language);
				if (lme) {
					if (!strcmp(code, "ja_JP"))
						string_to_utf8(str, utf_8, "Shift_JIS");	/* Japanese */
					else if (!strcmp(code, "zh_CN"))
						string_to_utf8(str, utf_8, "GB2312");		/* Chinese */
				}
				return FTF_DrawString(utf_8, FTF_INPUT_UTF8);
			}
			else
#endif // WITH_ICONV
				return FTF_DrawString(str, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
		}
		else
			return FTF_DrawString(str, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
	} else {
		return BMF_DrawString(font, str);
	}
#else // INTERNATIONAL
	return BMF_DrawString(font, str);
#endif
}

float UI_GetStringWidth(BMF_Font* font, char *str, int translate)
{
	float rt;

#ifdef INTERNATIONAL
	if(G.ui_international == TRUE)
		if(translate && (U.transopts & USER_TR_BUTTONS))
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

void UI_GetBoundingBox(struct BMF_Font* font, char* str, int translate, rctf *bbox)
{
#ifdef INTERNATIONAL
	float dummy;
	if(G.ui_international == TRUE)
		if(translate && (U.transopts & USER_TR_BUTTONS))
			FTF_GetBoundingBox(str, &bbox->xmin, &bbox->ymin, &dummy, &bbox->xmax, &bbox->ymax, &dummy, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
		else
			FTF_GetBoundingBox(str, &bbox->xmin, &bbox->ymin, &dummy, &bbox->xmax, &bbox->ymax, &dummy, FTF_NO_TRANSCONV | FTF_INPUT_UTF8);
	else
		BMF_GetStringBoundingBox(font, str, &bbox->xmin, &bbox->ymin, &bbox->xmax, &bbox->ymax);
#else
	BMF_GetStringBoundingBox(font, str, &bbox->xmin, &bbox->ymin, &bbox->xmax, &bbox->ymax);
#endif
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

/* called from fileselector */
void set_interface_font(char *str) 
{

	/* this test needed because fileselect callback can happen after disable AA fonts */
	if(U.transopts & USER_DOTRANSLATE) {
		if(FTF_SetFont((unsigned char*)str, 0, U.fontsize)) {
			BLF_lang_set(U.language);
			if(strlen(str) < FILE_MAXDIR) strcpy(U.fontname, str);
			G.ui_international = TRUE;
		} 
		else {
			U.fontname[0]= 0;
			FTF_SetFont((unsigned char*)datatoc_bfont_ttf, datatoc_bfont_ttf_size, U.fontsize);
			G.ui_international = TRUE;	// this case will switch to standard font
			/* XXX 2.50 bad call okee("Invalid font selection - reverting to built-in font."); */
		}
		/* XXX 2.50 bad call allqueue(REDRAWALL, 0); */
	}
}

void start_interface_font(void) 
{
	int result = 0;

	if(U.transopts & USER_USETEXTUREFONT)
		FTF_SetMode(FTF_TEXTUREFONT);
	else
		FTF_SetMode(FTF_PIXMAPFONT);
	
	if(U.fontsize && U.fontname[0] ) { // we have saved user settings + fontpath
		
		// try loading font from U.fontname = full path to font in usersettings
		result = FTF_SetFont((unsigned char*)U.fontname, 0, U.fontsize);
	}
	else if(U.fontsize) {	// user settings, default
		result = FTF_SetFont((unsigned char*)datatoc_bfont_ttf, datatoc_bfont_ttf_size, U.fontsize);
	}
	
	if(result==0) {		// use default
		U.language= 0;
		U.fontsize= 11;
		U.encoding= 0;
		U.fontname[0]= 0;
		result = FTF_SetFont((unsigned char*)datatoc_bfont_ttf, datatoc_bfont_ttf_size, U.fontsize);
	}

	if(result) {
		BLF_lang_set(U.language);
		G.ui_international = TRUE;
	} 
	else {
		printf("no font found for international support\n");
		G.ui_international = FALSE;
		U.transopts &= ~USER_DOTRANSLATE;
		U.fontsize = 0;
	}

	/* XXX 2.50 bad call allqueue(REDRAWALL, 0); */
}

#endif /* INTERNATIONAL */


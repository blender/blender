/**
 * $Id$
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
 * Copyright (C) 2002 Blender Foundation. All Rights Reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "libintl.h"
#include "BLI_blenlib.h"
#include "BKE_font.h"

#include "../FTF_Settings.h"

#include "FTF_TTFont.h"

#ifdef __APPLE__
#include "BKE_utildefines.h"
#endif

#define DOMAIN_NAME "blender"

#define SYSTEM_ENCODING_DEFAULT "UTF-8"
#define FONT_SIZE_DEFAULT 12
//#define FONT_PATH_DEFAULT ".bfont.ttf"

#define FTF_MAX_STR_SIZE 512

FTF_TTFont::FTF_TTFont(void)
{	
#ifdef __APPLE__
	char *bundlepath;
#endif

	font=NULL;
	fontm= fonts= fontl= NULL;
	font_size=FONT_SIZE_DEFAULT;
	mode = FTF_PIXMAPFONT;
	fsize = 1.0;
	strcpy(encoding_name, SYSTEM_ENCODING_DEFAULT);

	//set messagepath directory

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

	strcpy(messagepath, ".blender/locale");

	if ( !BLI_exist(messagepath) ) {	// locale not in current dir
		BLI_make_file_string("/", messagepath, BLI_gethome(), ".blender/locale");
		
		if( !BLI_exist(messagepath) ) {	// locale not in home dir

#ifdef WIN32 
			BLI_make_file_string("/", messagepath, BLI_gethome(), "/locale");
			if( !BLI_exist(messagepath) ) {
#endif
#ifdef __APPLE__
			/* message catalogs are stored inside the application bundle */
			bundlepath = BLI_getbundle();
			strcpy(messagepath, bundlepath);
			strcat(messagepath, "/Contents/Resources/locale");
			if( !BLI_exist(messagepath) ) { // locale not in bundle (now that's odd..)
#endif
				strcpy(messagepath, LOCALEDIR);

				if( !BLI_exist(messagepath) ) {	// locale not in LOCALEDIR
					strcpy(messagepath, "message");		// old compatibility as last
				}
#ifdef WIN32
			}
#endif
#ifdef __APPLE__
			}
#endif
		}
	}
}


FTF_TTFont::~FTF_TTFont(void)
{
	if (fonts) delete fonts;
	if (fontm) delete fontm;
	if (fontl) delete fontl;
}

void FTF_TTFont::SetFontSize(char size)
{
		if(size=='s') font=fonts;
		else if(size=='l') font=fontl;
		else font=fontm;
}

int FTF_TTFont::SetFont(const unsigned char* str, int datasize, int fontsize)
{
	int err = 0;
	bool success = 0;

	if (fonts) delete fonts;
	if (fontm) delete fontm;
	if (fontl) delete fontl;
	fonts= NULL;
	fontm= NULL;
	fontl= NULL;

	if(mode == FTF_PIXMAPFONT) {

		if(datasize) font = new FTGLPixmapFont(str, datasize);
		else font = new FTGLPixmapFont( (char *)str);

		err = font->Error();

		if(err) {
			printf("Failed to open font %s\n", str);
			return 0;
		} else {
			
			fontm= font;

			if(datasize) fonts = new FTGLPixmapFont(str, datasize);
			else fonts = new FTGLPixmapFont((char *)str);
			if(datasize) fontl = new FTGLPixmapFont(str, datasize);
			else fontl = new FTGLPixmapFont((char *)str);
			
			success = fonts->FaceSize(fontsize-2<8?8:fontsize-2);
			success = fontm->FaceSize(fontsize-1<8?8:fontsize-1);
			success = fontl->FaceSize(fontsize);
			if(!success) return 0;

			success = fonts->CharMap(ft_encoding_unicode);
			success = fontm->CharMap(ft_encoding_unicode);
			success = fontl->CharMap(ft_encoding_unicode);
			if(!success) return 0;

			return 1;
		}

	} else if(mode == FTF_TEXTUREFONT) {

		if(datasize) font = new FTGLTextureFont(str, datasize);
		else font = new FTGLTextureFont( (char *)str);

		err = font->Error();

		if(err) {
			printf("Failed to open font %s\n", str);
			return 0;
		} else {
			
			fontm= font;

			if(datasize) fonts = new FTGLTextureFont(str, datasize);
			else fonts = new FTGLTextureFont((char *)str);
			if(datasize) fontl = new FTGLTextureFont(str, datasize);
			else fontl = new FTGLTextureFont((char *)str);
			
			success = fonts->FaceSize(fontsize-2<8?8:fontsize-2);
			success = fontm->FaceSize(fontsize-1<8?8:fontsize-1);
			success = fontl->FaceSize(fontsize);
//			success = fonts->FaceSize(fontsize/2);
//			success = fontm->FaceSize(fontsize);
//			success = fontl->FaceSize(fontsize*2);
			if(!success) return 0;

			success = fonts->CharMap(ft_encoding_unicode);
			success = fontm->CharMap(ft_encoding_unicode);
			success = fontl->CharMap(ft_encoding_unicode);
			if(!success) return 0;

			return 1;
		}
	}
	return 0;
}

void FTF_TTFont::SetLanguage(char* str)
{

#if defined (_WIN32) || defined(__APPLE__)
	char envstr[12];

	sprintf(envstr, "LANG=%s", str);
	envstr[strlen(envstr)]='\0';
#ifdef _WIN32
	gettext_putenv(envstr);
#else
	putenv(envstr);
#endif
#else
	char *locreturn = setlocale(LC_ALL, str);
	if (locreturn == NULL) {
		char *lang;

		lang = (char*)malloc(sizeof(char)*(strlen(str)+7));

		lang[0] = '\0';
		strcat(lang, str);
		strcat(lang, ".UTF-8");

		locreturn = setlocale(LC_ALL, lang);
		if (locreturn == NULL) {
			printf("could not change language to %s nor %s\n", str, lang);
		}

		free(lang);
	}
	
	setlocale(LC_NUMERIC, "C");
#endif


	bindtextdomain(DOMAIN_NAME, messagepath);
//	bind_textdomain_codeset(DOMAIN_NAME, encoding_name);
	textdomain(DOMAIN_NAME);

	strcpy(language, str);
}


void FTF_TTFont::SetEncoding(char* str)
{
	strcpy(encoding_name, str);
//	bind_textdomain_codeset(DOMAIN_NAME, encoding_name);
}


void FTF_TTFont::SetSize(int size)
{
	fonts->FaceSize(size-2<8?8:size-2);
	fontm->FaceSize(size-1<8?8:size-1);
	fontl->FaceSize(size);

	font_size = size;
}

int FTF_TTFont::GetSize(void)
{
	return font_size;
}

/*
int FTF_TTFont::Ascender(void)
{
	return (int)font->Ascender();
}

int FTF_TTFont::Descender(void)
{
	return (int)font->Descender();
}

*/
int FTF_TTFont::TransConvString(char* str, char* ustr, unsigned int flag)
{
	return 0;
}


float FTF_TTFont::DrawString(char* str, unsigned int flag)
{
	float color[4];
	wchar_t wstr[FTF_MAX_STR_SIZE-1]={'\0'};
  
	/* note; this utf8towchar() function I totally don't understand... without using translations it 
	   removes special characters completely. So, for now we just skip that then. (ton) */
	if (FTF_USE_GETTEXT & flag) 
		utf8towchar(wstr, gettext(str));
	else if (FTF_INPUT_UTF8 & flag) 
		utf8towchar(wstr, str);

	glGetFloatv(GL_CURRENT_COLOR, color);
	
	if(mode == FTF_PIXMAPFONT) {

		glPixelTransferf(GL_RED_SCALE, color[0]);
		glPixelTransferf(GL_GREEN_SCALE, color[1]);
		glPixelTransferf(GL_BLUE_SCALE, color[2]);

		if ((FTF_USE_GETTEXT | FTF_INPUT_UTF8) & flag) 
			font->Render(wstr);
		else
			font->Render(str);
		
		glPixelTransferf(GL_RED_SCALE, 1.0);
		glPixelTransferf(GL_GREEN_SCALE, 1.0);
		glPixelTransferf(GL_BLUE_SCALE, 1.0);

	} else if(mode == FTF_TEXTUREFONT) {

		glEnable(GL_BLEND);
		glEnable(GL_TEXTURE_2D);
		
		glPushMatrix();
		glTranslatef(pen_x, pen_y, 0.0);
		glScalef(fsize, fsize, 1.0);

		if ((FTF_USE_GETTEXT | FTF_INPUT_UTF8) & flag) 
			font->Render(wstr);
		else
			font->Render(str);
		
		glPopMatrix();
  
		glDisable(GL_BLEND);
		glDisable(GL_TEXTURE_2D);
	}

	if ((FTF_USE_GETTEXT | FTF_INPUT_UTF8) & flag) 
		return font->Advance(wstr);
	else
		return font->Advance(str);
}


float FTF_TTFont::GetStringWidth(char* str, unsigned int flag)
{
	wchar_t wstr[FTF_MAX_STR_SIZE-1]={'\0'};
	int len=0;

	if (strlen(str)==0) return 0.0;
	
	/* note; this utf8towchar() function I totally don't understand... without using translations it 
		removes special characters completely. So, for now we just skip that then. (ton) */

	if (FTF_USE_GETTEXT & flag) {
		len=utf8towchar(wstr, gettext(str));

		if(mode == FTF_PIXMAPFONT) {
			return font->Advance(wstr);
		} else if(mode == FTF_TEXTUREFONT) {
			return font->Advance(wstr);// * fsize;
		}
	}
	else {
		if(mode == FTF_PIXMAPFONT) {
			return font->Advance(str);
		} else if(mode == FTF_TEXTUREFONT) {
			return font->Advance(str);// * fsize;
		}
	}
	
	return 0.0;
}


void FTF_TTFont::GetBoundingBox(char* str, float *llx, float *lly, float *llz, float *urx, float *ury, float *urz, unsigned int flag)
{
	wchar_t wstr[FTF_MAX_STR_SIZE-1]={'\0'};
	int len=0;
  
	if (FTF_USE_GETTEXT & flag) 
		len=utf8towchar(wstr,gettext(str));
	else 
		len=utf8towchar(wstr,str);

	font->BBox(wstr, *llx, *lly, *llz, *urx, *ury, *urz);
}


void FTF_TTFont::SetPosition(float x, float y)
{
	pen_x = x;
	pen_y = y;
}


void FTF_TTFont::SetMode(int m)
{
	mode = m;
}


void FTF_TTFont::SetScale(float size)
{
	fsize = size;
}



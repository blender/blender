/**
 * $Id$
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
 * Copyright (C) 2002 Blender Foundation. All Rights Reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "libintl.h"
#include "BLI_blenlib.h"

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

int utf8towchar(wchar_t *w, char *c)
{
  int len=0;
  if(w==NULL || c==NULL) return(0);
  //printf("%s\n",c);
  while(*c)
  {
    //Converts Unicode to wchar:

    if(*c & 0x80)
    {
      if(*c & 0x40)
      {
        if(*c & 0x20)
        {
          if(*c & 0x10)
          {
            *w=(c[0] & 0x0f)<<18 | (c[1]&0x1f)<<12 | (c[2]&0x3f)<<6 | (c[3]&0x7f);
            c++;
          }
          else
            *w=(c[0] & 0x1f)<<12 | (c[1]&0x3f)<<6 | (c[2]&0x7f);
          c++;
        }
        else
          *w=(c[0] &0x3f)<<6 | c[1]&0x7f;
        c++;
      }
      else
        *w=(c[0] & 0x7f);
    }
    else
      *w=(c[0] & 0x7f);

    c++;
    w++;
    len++;
  }
  return len;
}


FTF_TTFont::FTF_TTFont(void)
{	
#ifdef __APPLE__
	char *bundlepath;
#endif

	font=NULL;
	font_size=FONT_SIZE_DEFAULT;
	strcpy(encoding_name, SYSTEM_ENCODING_DEFAULT);

	//set messagepath directory

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

	strcpy(messagepath, ".blender/locale");

	if (BLI_exist(messagepath) == NULL) {	// locale not in current dir
		BLI_make_file_string("/", messagepath, BLI_gethome(), ".blender/locale");
		
		if(BLI_exist(messagepath) == NULL) {	// locale not in home dir

#ifdef WIN32 
			/* message catalogs are stored in the installation dir */
			BLI_getInstallationDir(messagepath);
			strcat(messagepath, "/.blender/locale");
			if(BLI_exist(messagepath) == NULL) {
#endif
#ifdef __APPLE__
			/* message catalogs are stored inside the application bundle */
			bundlepath = BLI_getbundle();
			strcpy(messagepath, bundlepath);
			strcat(messagepath, "/Contents/Resources/locale");
			if(BLI_exist(messagepath) == NULL) { // locale not in bundle (now that's odd..)
#endif
				strcpy(messagepath, LOCALEDIR);

				if(BLI_exist(messagepath) == NULL) {	// locale not in LOCALEDIR
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

int FTF_TTFont::SetFont(char* str, int size)
{
	int err = 0;
	bool success = 0;

	delete fonts;
	fonts= NULL;
	delete fontm;
	fontm= NULL;
	delete fontl;
	fontl= NULL;

	font = new FTGLPixmapFont(str);
	err = font->Error();

	if(err) {
		printf("Failed to open font %s\n", str);
		return 0;
	} else {
		
		fontm= font;
		fonts = new FTGLPixmapFont(str);
		fontl = new FTGLPixmapFont(str);
		
		success = fonts->FaceSize(size-2<8?8:size-2);
		success = fontm->FaceSize(size-1<8?8:size-1);
		success = fontl->FaceSize(size);
		if(!success) return 0;

		success = fonts->CharMap(ft_encoding_unicode);
		success = fontm->CharMap(ft_encoding_unicode);
		success = fontl->CharMap(ft_encoding_unicode);
		if(!success) return 0;

		return 1;
	}
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
		printf("could not change language to %s\n", str);
	}
	
	setlocale(LC_NUMERIC, "C");
#endif


	bindtextdomain(DOMAIN_NAME, messagepath);
	textdomain(DOMAIN_NAME);

	strcpy(language, str);
}


void FTF_TTFont::SetEncoding(char* str)
{
	strcpy(encoding_name, str);
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

int FTF_TTFont::Ascender(void)
{
	return (int)font->Ascender();
}

int FTF_TTFont::Descender(void)
{
	return (int)font->Descender();
}


int FTF_TTFont::TransConvString(char* str, char* ustr, unsigned int flag)
{
	return 0;
}


float FTF_TTFont::DrawString(char* str, unsigned int flag)
{
	float color[4];
	wchar_t wstr[FTF_MAX_STR_SIZE-1]={'\0'};
	int len=0;
  
	if (FTF_USE_GETTEXT & flag) 
		len=utf8towchar(wstr,gettext(str));
	else 
		len=utf8towchar(wstr,str);

	glGetFloatv(GL_CURRENT_COLOR, color);
	
	glPixelTransferf(GL_RED_SCALE, color[0]);
	glPixelTransferf(GL_GREEN_SCALE, color[1]);
	glPixelTransferf(GL_BLUE_SCALE, color[2]);
	
	font->Render(wstr);
  
	glPixelTransferf(GL_RED_SCALE, 1.0);
	glPixelTransferf(GL_GREEN_SCALE, 1.0);
	glPixelTransferf(GL_BLUE_SCALE, 1.0);

	return font->Advance(wstr);
}


float FTF_TTFont::GetStringWidth(char* str, unsigned int flag)
{
	wchar_t wstr[FTF_MAX_STR_SIZE-1]={'\0'};
	int len=0;

	if (FTF_USE_GETTEXT & flag) 
		len=utf8towchar(wstr,gettext(str));
	else 
		len=utf8towchar(wstr,str);

	return font->Advance(wstr);
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

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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 *
 * Implementation of the API of FTGL library.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../FTF_Api.h"
#include "FTF_TTFont.h"

#define FTF_EXPORT

static FTF_TTFont ttfont;


FTF_EXPORT void FTF_SetSize(int size)
{
	ttfont.SetSize(size);
}

FTF_EXPORT int FTF_GetSize(void)
{
	return ttfont.GetSize();
}


FTF_EXPORT int FTF_Ascender(void)
{
	return ttfont.Ascender();
}

FTF_EXPORT int FTF_Descender(void)
{
	return ttfont.Descender();
}


FTF_EXPORT void FTF_TransConvString(char* str, char* ustr, unsigned int flag)
{
	ttfont.TransConvString(str, ustr, flag);
}

/*
FTF_EXPORT float FTF_DrawCharacter(char c, unsigned int flag)
{
	char str[2] = {c, '\0'};
	return FTF_DrawString(str, flag);
}
*/


/* does color too, using glGet */
FTF_EXPORT float FTF_DrawString(char* str, unsigned int flag)
{
	return ttfont.DrawString(str, flag);
}


/**
  * not implemente yet.
  */
FTF_EXPORT float FTF_GetCharacterWidth(char c, unsigned int flag)
{
  char str[2] = {c, '\0'};
  return FTF_GetStringWidth(str, flag);
}


/**
  * not implemente yet.
  */
FTF_EXPORT float FTF_GetStringWidth(char* str, unsigned int flag)
{
  return ttfont.GetStringWidth(str, flag);
}


/**
  * not implemente yet.
  * ## This return string box!! ##
  */
FTF_EXPORT void FTF_GetBoundingBox(char* str, float *llx, float *lly, float *llz, float *urx, float *ury, float *urz, unsigned int flag)
{
  ttfont.GetBoundingBox(str, llx, lly, llz, urx, ury, urz, flag);
}

/**
  * added by phase
  *
  */
FTF_EXPORT int FTF_SetFont(char* str, int size)
{
  return ttfont.SetFont(str, size);
}

/* added bt ton */

FTF_EXPORT void FTF_SetFontSize(char size)
{
  ttfont.SetFontSize( size);
}

/**
  * added by phase
  *
  */
FTF_EXPORT void FTF_SetLanguage(char* str)
{
  ttfont.SetLanguage(str);
}

/**
  * added by phase
  *
  */
FTF_EXPORT void FTF_SetEncoding(char* str)
{
  ttfont.SetEncoding(str);
}

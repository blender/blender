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
 */

#ifndef __FTF_TRUETYPE_FONT_H
#define __FTF_TRUETYPE_FONT_H

#include "FTGLPixmapFont.h"
#include <stdio.h>
//#include <iconv.h>


/**
 * Base class for Using FTGL, iconv and gettext Library.
 */
class FTF_TTFont
{
public:
	/**
	 * Default constructor.
	 */
	FTF_TTFont(void);

	/**
	 * Destructor.
	 */
	virtual	~FTF_TTFont(void);


	void SetSize(int size);
	int GetSize(void);

	int Ascender(void);
	int Descender(void);

	int TransConvString(char* str, char* ustr, unsigned int flag);

	/**
	 * Draws a string at the current raster position.
	 * @param str	The string to draw.
	 * @param flag	Whether use gettext and UTF8 or system encoding.
	 */
	float DrawString(char* str, unsigned int flag, int select);
//	float DrawString(char* str, unsigned char r, unsigned char g, unsigned char b, unsigned int flag);
	float DrawStringRGB(char* str, unsigned int flag, float r, float g, float b);

	float GetStringWidth(char* str, unsigned int flag);

	void GetBoundingBox(char* str, float *llx, float *lly, float *llz, float *urx, float *ury, float *urz, unsigned int flag);

	/**
	 * added by phase
	 * functions to communicate with the preference menu
	 */
	int SetFont(char* str, int size);

	void SetLanguage(char* str);

	void SetEncoding(char* str);

protected:
	char language[32];
	char encoding_name[32];
	char font_name[128];
	int font_size;

	/** FTGL's */
	FTFont* font;

	/** from system encoding in .locale to UNICODE */
//	iconv_t cd;

	/** from UTF-8 to UNICODE */
//	iconv_t ucd;
};

#endif // __FTF_TRUETYPE_FONT_H

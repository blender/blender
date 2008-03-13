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
#include "FTGLTextureFont.h"

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

//	int Ascender(void);
//	int Descender(void);

	int TransConvString(char* str, char* ustr, unsigned int flag);

	/**
	 * Draws a string at the current raster position in current opengl color.
	 * @param str	The string to draw.
	 * @param flag	Whether use gettext and UTF8 or system encoding.
	 */
	float DrawString(char* str, unsigned int flag);

	float GetStringWidth(char* str, unsigned int flag);

	/**
	 * Get the bounding box for a string.
	 *
	 * @param str	The string
	 * @param llx   Lower left near x coord
	 * @param lly   Lower left near y coord
	 * @param llz   Lower left near z coord
	 * @param urx   Upper right far x coord
	 * @param ury   Upper right far y coord
	 * @param urz   Upper right far z coord
	 */
	void GetBoundingBox(char* str, float *llx, float *lly, float *llz, float *urx, float *ury, float *urz, unsigned int flag);

	/**
	 * added by phase, ton
	 * functions to communicate with the preference menu
	 */
	void SetFontSize(char size);

	int SetFont(const unsigned char* str, int datasize, int fontsize);

	void SetLanguage(char* str);

	void SetEncoding(char* str);

	/**
	 * functions to communicate with blender ui rasterpos
	 */
	void SetPosition(float x, float y);
	void SetMode(int mode);
	void SetScale(float fsize);

protected:
	char messagepath[1024];

	char language[32];
	char encoding_name[32];
	char font_name[128];
	int font_size;

	int	mode;			// 0 = pixmap, 1 = texture
	float pen_x, pen_y; //rasterpos
	float fsize;

	/** FTGL's */
	FTFont* font;	/* active */
	
	FTFont* fonts;	/* opened, small medium and large */
	FTFont* fontm;
	FTFont* fontl;

	/** from system encoding in .locale to UNICODE */
//	iconv_t cd;

	/** from UTF-8 to UNICODE */
//	iconv_t ucd;
};

#endif // __FTF_TRUETYPE_FONT_H

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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#ifndef __BMF_BITMAP_FONT_H
#define __BMF_BITMAP_FONT_H

#include "BMF_FontData.h"

/**
 * Base class for OpenGL bitmap fonts.
 */
class BMF_BitmapFont
{
public:
	/**
	 * Default constructor.
	 */
	BMF_BitmapFont(BMF_FontData* fontData);

	/**
	 * Destructor.
	 */
	virtual	~BMF_BitmapFont(void);

	/**
	 * Draws a string at the current raster position.
	 * @param str	The string to draw.
	 */
	void DrawString(char* str);

	/**
	 * Draws a string at the current raster position.
	 * @param str	The string to draw.
	 * @return The width of the string.
	 */
	int GetStringWidth(char* str);

	/**
	 * Returns the bounding box of the font. The width and
	 * height represent the bounding box of the union of
	 * all glyps. The minimum and maximum values of the
	 * box represent the extent of the font and its positioning
	 * about the origin.
	 */
	void GetFontBoundingBox(int & xMin, int & yMin, int & xMax, int & yMax);
	
	/**
	 * Return the bounding box height of the font.
	 */
	int GetFontHeight(void);
	
	/**
	 * Returns the bounding box of a string of characters.
	 * @param font	The font to use.
	 * @param str	The string.
	 * @param llx   Lower left x coord
	 * @param lly   Lower left y coord
	 * @param urx   Upper right x coord
	 * @param ury   Upper right y coord
	 */
	void GetStringBoundingBox(char* str, float*llx, float *lly, float *urx, float *ury);


	/**
	 * Convert the font to a texture, and return the GL texture
	 * ID of the texture. If the texture ID is bound, text can
	 * be drawn using the texture by calling DrawStringTexture.
	 * 
	 * @return The GL texture ID of the new texture, or -1 if unable
	 * to create.
	 */
	int GetTexture();
	
	/**
	 * Draw the given @a string at the point @a x, @a y, @a z, using
	 * texture coordinates. This assumes that an appropriate texture
	 * has been bound, see BMF_BitmapFont::GetTexture(). The string
	 * is drawn along the positive X axis.
	 * 
	 * @param string The c-string to draw.
	 * @param x The x coordinate to start drawing at.
	 * @param y The y coordinate to start drawing at.
	 * @param z The z coordinate to start drawing at.
	 */
	void DrawStringTexture(char* string, float x, float y, float z);
	
	/**
	 * Draw the given @a string at the point @a xpos, @a ypos using
	 * char and float buffers.
	 * 
	 * @param string The c-string to draw.
	 * @param xpos The x coordinate to start drawing at.
	 * @param ypos The y coordinate to start drawing at.
	 * @param col The forground color.
	 * @param buf Unsigned char image buffer, when NULL to not operate on it.
	 * @param fbuf float image buffer, when NULL to not operate on it.
	 * @param w image buffer width.
	 * @param h image buffer height.
	 */
	void DrawStringBuf(char *str, int posx, int posy, float *col, unsigned char *buf, float *fbuf, int w, int h);
	
protected:
	/** Pointer to the font data. */
	 BMF_FontData* m_fontData;
};

#endif // __BMF_BITMAP_FONT_H


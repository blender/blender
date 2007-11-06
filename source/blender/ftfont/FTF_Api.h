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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef _FTF_API_H
#define _FTF_API_H

#define FTF_EXPORT

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "FTF_Settings.h"

/**
 * Set Font Size
 * @param int size
 */
FTF_EXPORT void FTF_SetSize(int size);

/**
 * Get Font Size
 * @return Font size
 */
FTF_EXPORT int FTF_GetSize(void);

/**
 * Ascender
 * @return Ascend size
 */
FTF_EXPORT int FTF_Ascender(void);

/**
 * Descender
 * @return Descend size
 */
FTF_EXPORT int FTF_Descender(void);

/**
 * String Translation and Code Conversion
 * @param str source string
 * @param ustr distnation string
 * @param flag mode flag
 */
FTF_EXPORT void FTF_TransConvString(char* str, char* ustr, unsigned int flag);

/**
 * Draw a character at the current raster position.
 * @param c the character to draw
 * @param mode flag to forward to FTF_TransConvString()
 * @return Width drawing
 */
//FTF_EXPORT float FTF_DrawCharacter(char c, unsigned int flag);

/**
 * Draws a string at the current raster postion.
 * @param str The string to draw
 * @param mode flag to forward to FTF_TransConvString()
 * @return Width drawing
 */
FTF_EXPORT float FTF_DrawString(char* str, unsigned int flag);


/**
 * Get a character width
 * @param mode flag to forward to FTF_TransConvString()
 */
FTF_EXPORT float FTF_GetCharacterWidth(char c, unsigned int flag);


/**
 * Get a string width
 * @param mode flag to forward to FTF_TransConvString()
 */
FTF_EXPORT float FTF_GetStringWidth(char* str, unsigned int flag);

/**
 * Get Bounding Box
 * @param llx   Lower left near x coord
 * @param lly   Lower left near y coord
 * @param llz   Lower left near z coord
 * @param urx   Upper right far x coord
 * @param ury   Upper right far y coord
 * @param urz   Upper right far z coord
 * @param mode flag to forward to FTF_TransConvString()
 * not test yet.
 */
FTF_EXPORT void FTF_GetBoundingBox(char* str, float*llx, float *lly, float *llz, float *urx, float *ury, float *urz, unsigned int flag);

/**
 * Following stuff added by phase, ton
 */

/**
 * SetFontSize
 * @param size
 */
FTF_EXPORT void FTF_SetFontSize(char size);

/**
 * SetFont
 * @param str
 * @param size
 */
FTF_EXPORT int FTF_SetFont(const unsigned char* str, int datasize, int fontsize);

/**
 * SetLanguage
 * @param str
 * not test yet.
 */
FTF_EXPORT void FTF_SetLanguage(char* str);

/**
 * SetLanguage
 * @param str
 * not tested yet.
 */
FTF_EXPORT void FTF_SetEncoding(char* str);

FTF_EXPORT void FTF_SetPosition(float x, float y);
FTF_EXPORT void FTF_SetMode(int mode);
FTF_EXPORT void FTF_SetScale(float fsize);

FTF_EXPORT void FTF_End(void);

/* Font preview functions */
FTF_EXPORT int FTF_GetNewFont (const unsigned char *str, int datasize, int fontsize);
FTF_EXPORT float FTF_DrawNewFontString(char* str, unsigned int flag);

#ifdef __cplusplus
}
#endif

#endif /* __FTF_API_H */


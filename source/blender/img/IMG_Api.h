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
 * @author	Maarten Gribnau
 * @date	March 7, 2001
 */
#ifndef _H_IMG_API
#define _H_IMG_API

#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef void* IMG_BrushPtr;
typedef void* IMG_CanvasPtr;

#ifdef __cplusplus
extern "C" {
#endif


extern IMG_BrushPtr	IMG_BrushCreate(unsigned int width, unsigned int height, float red, float green, float blue, float alpha);
extern void			IMG_BrushDispose(IMG_BrushPtr brush);

extern IMG_CanvasPtr	IMG_CanvasCreate(unsigned int width, unsigned int height);
extern IMG_CanvasPtr	IMG_CanvasCreateFromPtr(void* imagePtr, unsigned int width, unsigned int height, size_t rowBytes);
extern void				IMG_CanvasDispose(IMG_CanvasPtr canvas);
extern void				IMG_CanvasDraw(IMG_CanvasPtr canvas, IMG_BrushPtr brush, unsigned int x, unsigned int y);
extern void				IMG_CanvasDrawUV(IMG_CanvasPtr canvas, IMG_BrushPtr brush, float u, float v);
extern void				IMG_CanvasDrawLine(IMG_CanvasPtr canvas, IMG_BrushPtr brush, unsigned int xStart, unsigned int yStart, unsigned int xEns, unsigned int yEnd);
extern void				IMG_CanvasDrawLineUV(IMG_CanvasPtr canvas, IMG_BrushPtr brush, float uStart, float vStart, float uEnd, float vEnd);

#ifdef __cplusplus
}
#endif

#endif // _H_IMG_API


/*
 * pixelblending_types.h
 * types pixelblending 
 *
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

#ifndef PIXELBLENDING_TYPES_H
#define PIXELBLENDING_TYPES_H

/*  #include "blender.h" */

/* Threshold for a 'full' pixel: pixels with alpha above this level are      */
/* considered opaque This is the decimal value for 0xFFF0 / 0xFFFF           */
#define RE_FULL_COLOUR_FLOAT 0.9998
/* Threshold for an 'empty' pixel: pixels with alpha above this level are    */
/* considered completely transparent. This is the decimal value              */
/* for 0x000F / 0xFFFF                                                       */
#define RE_EMPTY_COLOUR_FLOAT 0.0002
/* A 100% pixel. Sometimes, seems to be too little.... Hm.......             */
#define RE_UNITY_COLOUR_FLOAT 1.0
/* A 0% pixel. I wonder how 0 the 0.0 is...                                  */
#define RE_ZERO_COLOUR_FLOAT 0.0

/* threshold for alpha                                                       */
#define RE_FULL_ALPHA_FLOAT 0.9998

/* Same set of defines for shorts                                            */
#define RE_FULL_COLOUR_SHORT 0xFFF0
#define RE_EMPTY_COLOUR_SHORT 0x0000

#endif /* PIXELBLENDING_EXT_H */


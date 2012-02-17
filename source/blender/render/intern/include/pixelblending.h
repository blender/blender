/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006 Blender Foundation, full recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/** \file blender/render/intern/include/pixelblending.h
 *  \ingroup render
 */


#ifndef __PIXELBLENDING_H__
#define __PIXELBLENDING_H__ 


/**
* add 1 pixel to into filtered three lines 
 * (float vecs to float vec)
 */
void add_filt_fmask(unsigned int mask, float *col, float *rowbuf, int row_w);
void add_filt_fmask_pixsize(unsigned int mask, float *in, float *rowbuf, int row_w, int pixsize);
void add_filt_fmask_coord(float filt[][3], float *col, float *rowbuf, int row_w, int col_h, int x, int y);
void mask_array(unsigned int mask, float filt[][3]);

/**
 * Alpha-over blending for floats.
 */
void addAlphaOverFloat(float *dest, float *source);  

/**
 * Alpha-under blending for floats.
 */
void addAlphaUnderFloat(float *dest, float *source);  


/**
 * Same for floats
 */
void addalphaAddfacFloat(float *dest, float *source, char addfac);

/**
 * dest = dest + source
 */
void addalphaAddFloat(float *dest, float *source);

/**
 * Blend bron under doel, while doing gamma correction
 */
void addalphaUnderGammaFloat(float *doel, float *bron);



#endif /* __PIXELBLENDING_H__ */


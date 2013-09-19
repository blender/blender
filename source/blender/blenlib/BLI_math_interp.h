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
 * The Original Code is Copyright (C) 2012 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef __BLI_MATH_INTERP_H__
#define __BLI_MATH_INTERP_H__

/** \file BLI_math_interp.h
 *  \ingroup bli
 */

void BLI_bicubic_interpolation_fl(const float *buffer, float *output, int width, int height,
                                  int components, float u, float v);

void BLI_bicubic_interpolation_char(const unsigned char *buffer, unsigned char *output, int width, int height,
                                    int components, float u, float v);

void BLI_bilinear_interpolation_fl(const float *buffer, float *output, int width, int height,
                                   int components, float u, float v);

void BLI_bilinear_interpolation_char(const unsigned char *buffer, unsigned char *output, int width, int height,
                                     int components, float u, float v);

#endif  /* __BLI_MATH_INTERP_H__ */

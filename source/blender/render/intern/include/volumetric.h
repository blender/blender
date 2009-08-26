/**
 *
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
 * Contributor(s): Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

float vol_get_stepsize(struct ShadeInput *shi, int context);
float vol_get_density(struct ShadeInput *shi, float *co);
void vol_get_scattering(ShadeInput *shi, float *scatter_col, float *co, float stepsize, float density);

void shade_volume_outside(ShadeInput *shi, ShadeResult *shr);
void shade_volume_inside(ShadeInput *shi, ShadeResult *shr);
void shade_volume_shadow(struct ShadeInput *shi, struct ShadeResult *shr, struct Isect *last_is);

#define STEPSIZE_VIEW	0
#define STEPSIZE_SHADE	1

#define VOL_IS_BACKFACE			1
#define VOL_IS_SAMEMATERIAL		2

#define VOL_BOUNDS_DEPTH	0
#define VOL_BOUNDS_SS		1

#define VOL_SHADE_OUTSIDE	0
#define VOL_SHADE_INSIDE	1

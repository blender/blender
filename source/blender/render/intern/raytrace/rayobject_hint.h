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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/render/intern/raytrace/rayobject_hint.h
 *  \ingroup render
 */


#ifndef __RAYOBJECT_HINT_H__
#define __RAYOBJECT_HINT_H__

#define HINT_RECURSE	 1
#define HINT_ACCEPT		 0
#define HINT_DISCARD	-1

struct HintBB
{
	float bb[6];
};

inline int hint_test_bb(HintBB *obj, float *Nmin, float *Nmax)
{
	if (bb_fits_inside( Nmin, Nmax, obj->bb, obj->bb+3 ) )
		return HINT_RECURSE;
	else
		return HINT_ACCEPT;
}
#if 0
struct HintFrustum
{
	float co[3];
	float no[4][3];
};

inline int hint_test_bb(HintFrustum &obj, float *Nmin, float *Nmax)
{
	//if frustum inside BB
	{
		return HINT_RECURSE;
	}
	//if BB outside frustum
	{
		return HINT_DISCARD;
	}
	
	return HINT_ACCEPT;
}
#endif

#endif /* __RAYOBJECT_HINT_H__ */

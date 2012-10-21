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

/** \file blender/render/intern/raytrace/rayobject_raycounter.cpp
 *  \ingroup render
 */


#include "rayobject.h"
#include "raycounter.h"

#ifdef RE_RAYCOUNTER

void RE_RC_INFO(RayCounter *info)
{
	printf("----------- Raycast counter --------\n");
	printf("Rays total: %llu\n", info->raycast.test );
	printf("Rays hit: %llu\n",   info->raycast.hit  );
	printf("\n");
	printf("BB tests: %llu\n", info->bb.test );
	printf("BB hits: %llu\n", info->bb.hit );
	printf("\n");
	printf("SIMD BB tests: %llu\n", info->simd_bb.test );
	printf("SIMD BB hits: %llu\n", info->simd_bb.hit );
	printf("\n");
	printf("Primitives tests: %llu\n", info->faces.test );
	printf("Primitives hits: %llu\n", info->faces.hit );
	printf("------------------------------------\n");
	printf("Shadow last-hit tests per ray: %f\n", info->rayshadow_last_hit.test / ((float)info->raycast.test) );
	printf("Shadow last-hit hits per ray: %f\n",  info->rayshadow_last_hit.hit  / ((float)info->raycast.test) );
	printf("\n");
	printf("Hint tests per ray: %f\n", info->raytrace_hint.test / ((float)info->raycast.test) );
	printf("Hint hits per ray: %f\n",  info->raytrace_hint.hit  / ((float)info->raycast.test) );
	printf("\n");
	printf("BB tests per ray: %f\n", info->bb.test / ((float)info->raycast.test) );
	printf("BB hits per ray: %f\n", info->bb.hit / ((float)info->raycast.test) );
	printf("\n");
	printf("SIMD tests per ray: %f\n", info->simd_bb.test / ((float)info->raycast.test) );
	printf("SIMD hits per ray: %f\n", info->simd_bb.hit / ((float)info->raycast.test) );
	printf("\n");
	printf("Primitives tests per ray: %f\n", info->faces.test / ((float)info->raycast.test) );
	printf("Primitives hits per ray: %f\n", info->faces.hit / ((float)info->raycast.test) );
	printf("------------------------------------\n");
}

void RE_RC_MERGE(RayCounter *dest, RayCounter *tmp)
{
	dest->faces.test += tmp->faces.test;
	dest->faces.hit  += tmp->faces.hit;

	dest->bb.test += tmp->bb.test;
	dest->bb.hit  += tmp->bb.hit;

	dest->simd_bb.test += tmp->simd_bb.test;
	dest->simd_bb.hit  += tmp->simd_bb.hit;

	dest->raycast.test += tmp->raycast.test;
	dest->raycast.hit  += tmp->raycast.hit;
	
	dest->rayshadow_last_hit.test += tmp->rayshadow_last_hit.test;
	dest->rayshadow_last_hit.hit  += tmp->rayshadow_last_hit.hit;

	dest->raytrace_hint.test += tmp->raytrace_hint.test;
	dest->raytrace_hint.hit  += tmp->raytrace_hint.hit;
}

#endif

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
 * The Original Code is Copyright (C) 2014 by the Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/drawsimdebug.c
 *  \ingroup spview3d
 */

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_effect.h"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "view3d_intern.h"


static void draw_sim_debug_elements(SimDebugData *debug_data, float imat[4][4])
{
	VertexFormat *format = immVertexFormat();
	unsigned int pos = VertexFormat_add_attrib(format, "pos", COMP_F32, 3, KEEP_FLOAT);
	unsigned int color = VertexFormat_add_attrib(format, "color", COMP_F32, 3, KEEP_FLOAT);
	
	immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
	
	/* count element types */
	GHashIterator iter;
	int num_dots = 0;
	int num_circles = 0;
	int num_lines = 0;
	int num_vectors = 0;
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		switch (elem->type) {
			case SIM_DEBUG_ELEM_DOT: ++num_dots; break;
			case SIM_DEBUG_ELEM_CIRCLE: ++num_circles; break;
			case SIM_DEBUG_ELEM_LINE: ++num_lines; break;
			case SIM_DEBUG_ELEM_VECTOR: ++num_vectors; break;
		}
	}
	
	/**** dots ****/
	
	glPointSize(3.0f);
	immBegin(PRIM_POINTS, num_dots);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_DOT)
			continue;
		
		immAttrib3fv(color, elem->color);
		immVertex3fv(pos, elem->v1);
	}
	immEnd();
	
	/**** circles ****/
	
	{
#define CIRCLERES 16
		float circle[CIRCLERES][2] = {
		    {0.000000, 1.000000}, {0.382683, 0.923880}, {0.707107, 0.707107}, {0.923880, 0.382683},
		    {1.000000, -0.000000}, {0.923880, -0.382683}, {0.707107, -0.707107}, {0.382683, -0.923880},
		    {-0.000000, -1.000000}, {-0.382683, -0.923880}, {-0.707107, -0.707107}, {-0.923879, -0.382684},
		    {-1.000000, 0.000000}, {-0.923879, 0.382684}, {-0.707107, 0.707107}, {-0.382683, 0.923880} };
		
		immBegin(PRIM_LINES, num_circles * CIRCLERES * 2);
		
		for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
			SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
			float radius = elem->v2[0];
			float co[3], nco[3];
			int i;
			
			if (elem->type != SIM_DEBUG_ELEM_CIRCLE)
				continue;
			
			immAttrib3fv(color, elem->color);
			zero_v3(co);
			for (i = 0; i <= CIRCLERES; ++i) {
				int ni = i % CIRCLERES;
				nco[0] = radius * circle[ni][0];
				nco[1] = radius * circle[ni][1];
				nco[2] = 0.0f;
				mul_mat3_m4_v3(imat, nco);
				add_v3_v3(nco, elem->v1);
				
				if (i > 0) {
					immVertex3fv(pos, co);
					immVertex3fv(pos, nco);
				}
				
				copy_v3_v3(co, nco);
			}
		}
		
		immEnd();
#undef CIRCLERES
	}
	
	/**** lines ****/
	
	immBegin(PRIM_LINES, num_lines * 2);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_LINE)
			continue;
		
		immAttrib3fv(color, elem->color);
		immVertex3fv(pos, elem->v1);
		immVertex3fv(pos, elem->v2);
	}
	immEnd();
	
	/**** vectors ****/
	
	glPointSize(2.0f);
	immBegin(PRIM_POINTS, num_vectors);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_VECTOR)
			continue;
		
		immAttrib3fv(color, elem->color);
		immVertex3fv(pos, elem->v1);
	}
	immEnd();
	
	immBegin(PRIM_LINES, num_vectors * 2);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		float t[3];
		if (elem->type != SIM_DEBUG_ELEM_VECTOR)
			continue;
		
		immAttrib3fv(color, elem->color);
		immVertex3fv(pos, elem->v1);
		add_v3_v3v3(t, elem->v1, elem->v2);
		immVertex3fv(pos, t);
	}
	immEnd();
	
	immUnbindProgram();
	
	/**** strings ****/
	
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_STRING)
			continue;
		
		unsigned char col[4];
		rgb_float_to_uchar(col, elem->color);
		col[3] = 255;
		view3d_cached_text_draw_add(elem->v1, elem->str, strlen(elem->str),
		                            0, V3D_CACHE_TEXT_GLOBALSPACE, col);
	}
}

void draw_sim_debug_data(Scene *UNUSED(scene), View3D *v3d, ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	/*Object *ob = base->object;*/
	float imat[4][4];
	
	if (!_sim_debug_data)
		return;
	
	invert_m4_m4(imat, rv3d->viewmatob);
	
	gpuPushMatrix();
	gpuLoadMatrix3D(rv3d->viewmat);
	
	view3d_cached_text_draw_begin();
	draw_sim_debug_elements(_sim_debug_data, imat);
	view3d_cached_text_draw_end(v3d, ar, false, NULL);
	
	gpuPopMatrix();
}

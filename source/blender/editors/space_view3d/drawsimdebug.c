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

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_modifier.h"

#include "view3d_intern.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_resources.h"

static void draw_sim_debug_elements(SimDebugData *debug_data, float imat[4][4])
{
	GHashIterator iter;
	
	/**** dots ****/
	
	glPointSize(3.0f);
	glBegin(GL_POINTS);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_DOT)
			continue;
		
		glColor3f(elem->color[0], elem->color[1], elem->color[2]);
		glVertex3f(elem->v1[0], elem->v1[1], elem->v1[2]);
	}
	glEnd();
	glPointSize(1.0f);
	
	/**** circles ****/
	
	{
		float circle[16][2] = {
		    {0.000000, 1.000000}, {0.382683, 0.923880}, {0.707107, 0.707107}, {0.923880, 0.382683},
		    {1.000000, -0.000000}, {0.923880, -0.382683}, {0.707107, -0.707107}, {0.382683, -0.923880},
		    {-0.000000, -1.000000}, {-0.382683, -0.923880}, {-0.707107, -0.707107}, {-0.923879, -0.382684},
		    {-1.000000, 0.000000}, {-0.923879, 0.382684}, {-0.707107, 0.707107}, {-0.382683, 0.923880} };
		for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
			SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
			float radius = elem->v2[0];
			float co[3];
			int i;
			
			if (elem->type != SIM_DEBUG_ELEM_CIRCLE)
				continue;
			
			glColor3f(elem->color[0], elem->color[1], elem->color[2]);
			glBegin(GL_LINE_LOOP);
			for (i = 0; i < 16; ++i) {
				co[0] = radius * circle[i][0];
				co[1] = radius * circle[i][1];
				co[2] = 0.0f;
				mul_mat3_m4_v3(imat, co);
				add_v3_v3(co, elem->v1);
				
				glVertex3f(co[0], co[1], co[2]);
			}
			glEnd();
		}
	}
	
	/**** lines ****/
	
	glBegin(GL_LINES);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_LINE)
			continue;
		
		glColor3f(elem->color[0], elem->color[1], elem->color[2]);
		glVertex3f(elem->v1[0], elem->v1[1], elem->v1[2]);
		glVertex3f(elem->v2[0], elem->v2[1], elem->v2[2]);
	}
	glEnd();
	
	/**** vectors ****/
	
	glPointSize(2.0f);
	glBegin(GL_POINTS);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		if (elem->type != SIM_DEBUG_ELEM_VECTOR)
			continue;
		
		glColor3f(elem->color[0], elem->color[1], elem->color[2]);
		glVertex3f(elem->v1[0], elem->v1[1], elem->v1[2]);
	}
	glEnd();
	glPointSize(1.0f);
	
	glBegin(GL_LINES);
	for (BLI_ghashIterator_init(&iter, debug_data->gh); !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter)) {
		SimDebugElement *elem = BLI_ghashIterator_getValue(&iter);
		float t[3];
		if (elem->type != SIM_DEBUG_ELEM_VECTOR)
			continue;
		
		glColor3f(elem->color[0], elem->color[1], elem->color[2]);
		glVertex3f(elem->v1[0], elem->v1[1], elem->v1[2]);
		add_v3_v3v3(t, elem->v1, elem->v2);
		glVertex3f(t[0], t[1], t[2]);
	}
	glEnd();
}

void draw_sim_debug_data(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar)
{
	RegionView3D *rv3d = ar->regiondata;
	/*Object *ob = base->object;*/
	float imat[4][4];
	
	if (!_sim_debug_data)
		return;
	
	invert_m4_m4(imat, rv3d->viewmatob);
	
//	glDepthMask(GL_FALSE);
//	glEnable(GL_BLEND);
	
	glPushMatrix();
	
	glLoadMatrixf(rv3d->viewmat);
	draw_sim_debug_elements(_sim_debug_data, imat);
	
	glPopMatrix();
	
//	glDepthMask(GL_TRUE);
//	glDisable(GL_BLEND);
}

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

static void draw_sim_debug_elements(SimDebugData *debug_data)
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

void draw_sim_debug_data(Scene *UNUSED(scene), View3D *UNUSED(v3d), ARegion *ar, Base *UNUSED(base), SimDebugData *debug_data)
{
	RegionView3D *rv3d = ar->regiondata;
	/*Object *ob = base->object;*/
	/*float imat[4][4];*/
	
	/*invert_m4_m4(imat, rv3d->viewmatob);*/
	
//	glDepthMask(GL_FALSE);
//	glEnable(GL_BLEND);
	
	glPushMatrix();
	glLoadMatrixf(rv3d->viewmat);
	
	if (debug_data) {
		draw_sim_debug_elements(debug_data);
	}
	
	glPopMatrix();
	
//	glDepthMask(GL_TRUE);
//	glDisable(GL_BLEND);
}

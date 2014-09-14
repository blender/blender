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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/BPH_mass_spring.c
 *  \ingroup bph
 */

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BLI_math.h"
#include "BLI_linklist.h"
#include "BLI_utildefines.h"

#include "BKE_cloth.h"

#include "BPH_mass_spring.h"
#include "implicit.h"

int BPH_cloth_solver_init(Object *UNUSED(ob), ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	Implicit_Data *id;
	unsigned int i;
	
	cloth->implicit = id = BPH_mass_spring_solver_create(cloth->numverts, cloth->numsprings);
	
	for (i = 0; i < cloth->numverts; i++) {
		BPH_mass_spring_set_vertex_mass(id, i, verts[i].mass);
	}
	
	// init springs 
	LinkNode *link = cloth->springs;
	for (i = 0; link; link = link->next, ++i) {
		ClothSpring *spring = (ClothSpring *)link->link;
		
		spring->matrix_index = BPH_mass_spring_init_spring(id, i, spring->ij, spring->kl);
	}
	
	for (i = 0; i < cloth->numverts; i++) {
		BPH_mass_spring_set_motion_state(id, i, verts[i].x, ZERO);
	}
	
	return 1;
}

void BPH_cloth_solver_free(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	
	if (cloth->implicit) {
		BPH_mass_spring_solver_free(cloth->implicit);
		cloth->implicit = NULL;
	}
}

void BKE_cloth_solver_set_positions(ClothModifierData *clmd)
{
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = cloth->verts;
	unsigned int numverts = cloth->numverts, i;
	ClothHairRoot *cloth_roots = clmd->roots;
	Implicit_Data *id = cloth->implicit;
	const float ZERO[3] = {0.0f, 0.0f, 0.0f};
	
	for (i = 0; i < numverts; i++) {
		ClothHairRoot *root = &cloth_roots[i];
		
		BPH_mass_spring_set_root_motion(id, i, root->loc, ZERO, root->rot, ZERO);
		BPH_mass_spring_set_motion_state(id, i, verts[i].x, verts[i].v);
	}
}

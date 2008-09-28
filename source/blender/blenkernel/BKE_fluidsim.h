/**
 * BKE_fluidsim.h
 * 
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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim.h" // N_T
#include "DNA_object_types.h"

#include "BKE_DerivedMesh.h"

/* old interface */
FluidsimSettings *fluidsimSettingsNew(Object *srcob);

void initElbeemMesh(Object *ob, int *numVertices, float **vertices, int *numTriangles, int **triangles, int useGlobalCoords, int modifierIndex);


/* new fluid-modifier interface */
void fluidsim_init(FluidsimModifierData *fluidmd);
void fluidsim_free(FluidsimModifierData *fluidmd);

DerivedMesh *fluidsim_read_cache(Object *ob, DerivedMesh *orgdm, FluidsimModifierData *fluidmd, int framenr, int useRenderParams);
DerivedMesh *fluidsimModifier_do(FluidsimModifierData *fluidmd, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc);

// get bounding box of mesh
void fluid_get_bb(MVert *mvert, int totvert, float obmat[][4],
		 /*RET*/ float start[3], /*RET*/ float size[3] );




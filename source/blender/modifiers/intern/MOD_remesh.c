/*
* $Id$
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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2011 by Nicholas Bishop.
*
* ***** END GPL LICENSE BLOCK *****
*
*/

/** \file blender/modifiers/intern/MOD_remesh.c
 *  \ingroup modifiers
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h"

#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "MOD_modifiertypes.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "dualcon.h"

static void initData(ModifierData *md)
{
	RemeshModifierData *rmd = (RemeshModifierData*) md;

	rmd->scale = 0.9;
	rmd->depth = 4;
	rmd->hermite_num = 1;
	rmd->flag = MOD_REMESH_FLOOD_FILL;
	rmd->mode = MOD_REMESH_SHARP_FEATURES;
	rmd->threshold = 1;
}

static void copyData(ModifierData *md, ModifierData *target)
{
	RemeshModifierData *rmd = (RemeshModifierData*) md;
	RemeshModifierData *trmd = (RemeshModifierData*) target;

	trmd->threshold = rmd->threshold;
	trmd->scale = rmd->scale;
	trmd->hermite_num = rmd->hermite_num;
	trmd->depth = rmd->depth;
	trmd->flag = rmd->flag;
	trmd->mode = rmd->mode;
}

void init_dualcon_mesh(DualConInput *mesh, DerivedMesh *dm)
{
	memset(mesh, 0, sizeof(DualConInput));

	mesh->co = (void*)dm->getVertArray(dm);
	mesh->co_stride = sizeof(MVert);
	mesh->totco = dm->getNumVerts(dm);

	mesh->faces = (void*)dm->getFaceArray(dm);
	mesh->face_stride = sizeof(MFace);
	mesh->totface = dm->getNumFaces(dm);

	dm->getMinMax(dm, mesh->min, mesh->max);
}

/* simple structure to hold the output: a CDDM and two counters to
   keep track of the current elements */
typedef struct {
	DerivedMesh *dm;
	int curvert, curface;
} DualConOutput;

/* allocate and initialize a DualConOutput */
void *dualcon_alloc_output(int totvert, int totquad)
{
	DualConOutput *output;

	if(!(output = MEM_callocN(sizeof(DualConOutput),
							  "DualConOutput")))
		return NULL;
	
	output->dm = CDDM_new(totvert, 0, totquad);
	return output;
}

void dualcon_add_vert(void *output_v, const float co[3])
{
	DualConOutput *output = output_v;
	DerivedMesh *dm = output->dm;
	
	assert(output->curvert < dm->getNumVerts(dm));
	
	copy_v3_v3(CDDM_get_verts(dm)[output->curvert].co, co);
	output->curvert++;
}

void dualcon_add_quad(void *output_v, const int vert_indices[4])
{
	DualConOutput *output = output_v;
	DerivedMesh *dm = output->dm;
	MFace *mface;
	
	assert(output->curface < dm->getNumFaces(dm));

	mface = &CDDM_get_faces(dm)[output->curface];
	mface->v1 = vert_indices[0];
	mface->v2 = vert_indices[1];
	mface->v3 = vert_indices[2];
	mface->v4 = vert_indices[3];
	
	if(test_index_face(mface, NULL, 0, 4))
		output->curface++;
}

static DerivedMesh *applyModifier(ModifierData *md,
								  Object *UNUSED(ob),
								  DerivedMesh *dm,
								  int UNUSED(useRenderParams),
								  int UNUSED(isFinalCalc))
{
	RemeshModifierData *rmd;
	DualConOutput *output;
	DualConInput input;
	DerivedMesh *result;
	DualConFlags flags = 0;
	DualConMode mode;

	rmd = (RemeshModifierData*)md;

	init_dualcon_mesh(&input, dm);

	if(rmd->flag & MOD_REMESH_FLOOD_FILL)
		flags |= DUALCON_FLOOD_FILL;

	switch(rmd->mode) {
	case MOD_REMESH_CENTROID:
		mode = DUALCON_CENTROID;
		break;
	case MOD_REMESH_MASS_POINT:
		mode = DUALCON_MASS_POINT;
		break;
	case MOD_REMESH_SHARP_FEATURES:
		mode = DUALCON_SHARP_FEATURES;
		break;
	}
	
	output = dualcon(&input,
					 dualcon_alloc_output,
					 dualcon_add_vert,
					 dualcon_add_quad,
					 flags,
					 mode,
					 rmd->threshold,
					 rmd->hermite_num,
					 rmd->scale,
					 rmd->depth);
	result = output->dm;
	CDDM_lower_num_faces(result, output->curface);
	MEM_freeN(output);

	CDDM_calc_edges(result);
	CDDM_calc_normals(result);

	return result;
}

ModifierTypeInfo modifierType_Remesh = {
	/* name */              "Remesh",
	/* structName */        "RemeshModifierData",
	/* structSize */        sizeof(RemeshModifierData),
	/* type */              eModifierTypeType_Nonconstructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh |	eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,
	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,
	/* applyModifierEM */   NULL,
	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepgraph */    NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
};

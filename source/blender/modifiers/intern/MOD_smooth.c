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
* The Original Code is Copyright (C) 2005 by the Blender Foundation.
* All rights reserved.
*
* Contributor(s): Daniel Dunbar
*                 Ton Roosendaal,
*                 Ben Batt,
*                 Brecht Van Lommel,
*                 Campbell Barton
*
* ***** END GPL LICENSE BLOCK *****
*
*/

#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_particle.h"
#include "BKE_deform.h"

#include "MEM_guardedalloc.h"

#include "MOD_modifiertypes.h"
#include "MOD_util.h"


static void initData(ModifierData *md)
{
	SmoothModifierData *smd = (SmoothModifierData*) md;

	smd->fac = 0.5f;
	smd->repeat = 1;
	smd->flag = MOD_SMOOTH_X | MOD_SMOOTH_Y | MOD_SMOOTH_Z;
	smd->defgrp_name[0] = '\0';
}

static void copyData(ModifierData *md, ModifierData *target)
{
	SmoothModifierData *smd = (SmoothModifierData*) md;
	SmoothModifierData *tsmd = (SmoothModifierData*) target;

	tsmd->fac = smd->fac;
	tsmd->repeat = smd->repeat;
	tsmd->flag = smd->flag;
	strncpy(tsmd->defgrp_name, smd->defgrp_name, 32);
}

static int isDisabled(ModifierData *md, int useRenderParams)
{
	SmoothModifierData *smd = (SmoothModifierData*) md;
	short flag;

	flag = smd->flag & (MOD_SMOOTH_X|MOD_SMOOTH_Y|MOD_SMOOTH_Z);

	/* disable if modifier is off for X, Y and Z or if factor is 0 */
	if((smd->fac == 0.0f) || flag == 0) return 1;

	return 0;
}

static CustomDataMask requiredDataMask(Object *ob, ModifierData *md)
{
	SmoothModifierData *smd = (SmoothModifierData *)md;
	CustomDataMask dataMask = 0;

	/* ask for vertexgroups if we need them */
	if(smd->defgrp_name[0]) dataMask |= (1 << CD_MDEFORMVERT);

	return dataMask;
}

static void smoothModifier_do(
				  SmoothModifierData *smd, Object *ob, DerivedMesh *dm,
	 float (*vertexCos)[3], int numVerts)
{
	MDeformVert *dvert = NULL;
	MEdge *medges = NULL;

	int i, j, numDMEdges, defgrp_index;
	unsigned char *uctmp;
	float *ftmp, fac, facm;

	ftmp = (float*)MEM_callocN(3*sizeof(float)*numVerts,
		"smoothmodifier_f");
	if (!ftmp) return;
	uctmp = (unsigned char*)MEM_callocN(sizeof(unsigned char)*numVerts,
		 "smoothmodifier_uc");
	if (!uctmp) {
		if (ftmp) MEM_freeN(ftmp);
		return;
	}

	fac = smd->fac;
	facm = 1 - fac;

	medges = dm->getEdgeArray(dm);
	numDMEdges = dm->getNumEdges(dm);

	defgrp_index = defgroup_name_index(ob, smd->defgrp_name);

	if (defgrp_index >= 0)
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

	/* NOTICE: this can be optimized a little bit by moving the
	* if (dvert) out of the loop, if needed */
	for (j = 0; j < smd->repeat; j++) {
		for (i = 0; i < numDMEdges; i++) {
			float fvec[3];
			float *v1, *v2;
			unsigned int idx1, idx2;

			idx1 = medges[i].v1;
			idx2 = medges[i].v2;

			v1 = vertexCos[idx1];
			v2 = vertexCos[idx2];

			fvec[0] = (v1[0] + v2[0]) / 2.0;
			fvec[1] = (v1[1] + v2[1]) / 2.0;
			fvec[2] = (v1[2] + v2[2]) / 2.0;

			v1 = &ftmp[idx1*3];
			v2 = &ftmp[idx2*3];

			if (uctmp[idx1] < 255) {
				uctmp[idx1]++;
				add_v3_v3v3(v1, v1, fvec);
			}
			if (uctmp[idx2] < 255) {
				uctmp[idx2]++;
				add_v3_v3v3(v2, v2, fvec);
			}
		}

		if (dvert) {
			for (i = 0; i < numVerts; i++) {
				MDeformWeight *dw = NULL;
				float f, fm, facw, *fp, *v;
				int k;
				short flag = smd->flag;

				v = vertexCos[i];
				fp = &ftmp[i*3];

				for (k = 0; k < dvert[i].totweight; ++k) {
					if(dvert[i].dw[k].def_nr == defgrp_index) {
						dw = &dvert[i].dw[k];
						break;
					}
				}
				if (!dw) continue;

				f = fac * dw->weight;
				fm = 1.0f - f;

				/* fp is the sum of uctmp[i] verts, so must be averaged */
				facw = 0.0f;
				if (uctmp[i]) 
					facw = f / (float)uctmp[i];

				if (flag & MOD_SMOOTH_X)
					v[0] = fm * v[0] + facw * fp[0];
				if (flag & MOD_SMOOTH_Y)
					v[1] = fm * v[1] + facw * fp[1];
				if (flag & MOD_SMOOTH_Z)
					v[2] = fm * v[2] + facw * fp[2];
			}
		}
		else { /* no vertex group */
			for (i = 0; i < numVerts; i++) {
				float facw, *fp, *v;
				short flag = smd->flag;

				v = vertexCos[i];
				fp = &ftmp[i*3];

				/* fp is the sum of uctmp[i] verts, so must be averaged */
				facw = 0.0f;
				if (uctmp[i]) 
					facw = fac / (float)uctmp[i];

				if (flag & MOD_SMOOTH_X)
					v[0] = facm * v[0] + facw * fp[0];
				if (flag & MOD_SMOOTH_Y)
					v[1] = facm * v[1] + facw * fp[1];
				if (flag & MOD_SMOOTH_Z)
					v[2] = facm * v[2] + facw * fp[2];
			}

		}

		memset(ftmp, 0, 3*sizeof(float)*numVerts);
		memset(uctmp, 0, sizeof(unsigned char)*numVerts);
	}

	MEM_freeN(ftmp);
	MEM_freeN(uctmp);
}

static void deformVerts(
					   ModifierData *md, Object *ob, DerivedMesh *derivedData,
	   float (*vertexCos)[3], int numVerts, int useRenderParams, int isFinalCalc)
{
	DerivedMesh *dm= get_dm(md->scene, ob, NULL, derivedData, NULL, 0);

	smoothModifier_do((SmoothModifierData *)md, ob, dm,
			   vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}

static void deformVertsEM(
					 ModifierData *md, Object *ob, struct EditMesh *editData,
	  DerivedMesh *derivedData, float (*vertexCos)[3], int numVerts)
{
	DerivedMesh *dm= get_dm(md->scene, ob, editData, derivedData, NULL, 0);

	smoothModifier_do((SmoothModifierData *)md, ob, dm,
			   vertexCos, numVerts);

	if(dm != derivedData)
		dm->release(dm);
}


ModifierTypeInfo modifierType_Smooth = {
	/* name */              "Smooth",
	/* structName */        "SmoothModifierData",
	/* structSize */        sizeof(SmoothModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh
							| eModifierTypeFlag_SupportsEditmode,

	/* copyData */          copyData,
	/* deformVerts */       deformVerts,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  0,
	/* applyModifier */     0,
	/* applyModifierEM */   0,
	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          0,
	/* isDisabled */        isDisabled,
	/* updateDepgraph */    0,
	/* dependsOnTime */     0,
	/* foreachObjectLink */ 0,
	/* foreachIDLink */     0,
};

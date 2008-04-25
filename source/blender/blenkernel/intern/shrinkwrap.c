/**
 * shrinkwrap.c
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <string.h>
#include <float.h>
#include <assert.h>
//TODO: its late and I don't fill like adding ifs() printfs (I'll remove them on end)

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_shrinkwrap.h"
#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_arithb.h"

/* Projects the vertex on the normal direction over the target mesh */
static void shrinkwrap_calc_normal_projection(DerivedMesh *target, float *co, short *no)
{
}

/* Nearest surface point on target mesh */
static void shrinkwrap_calc_nearest_point(DerivedMesh *target, float *co, short *no)
{
	//TODO: For now its only doing a nearest vertex on target mesh (just for testing other things)
	float minDist = FLT_MAX;
	float orig_co[3];

	int i;
	int	numVerts = target->getNumVerts(target);
	MVert *vert = target->getVertDataArray(target, CD_MVERT);

	VECCOPY(orig_co, co);

	for (i = 0; i < numVerts; i++)
	{
		float diff[3], sdist;
		VECSUB(diff, orig_co, vert[i].co);
		sdist = Inpf(diff, diff);
		
		if(sdist < minDist)
		{
			minDist = sdist;
			VECCOPY(co, vert[i].co);
		}
	}
}

DerivedMesh *shrinkwrapModifier_do(ShrinkwrapModifierData *smd, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{

	DerivedMesh *result = CDDM_copy(dm);

	//Projecting target defined - lets work!
	if(smd->target)
	{
		int i, j;

		int vgroup		= get_named_vertexgroup_num(ob, smd->vgroup_name);
		int	numVerts	= 0;

		MDeformVert *dvert = NULL;
		MVert		*vert  = NULL;

		float local2target[4][4], target2local[4][4];

		DerivedMesh *target_dm = (DerivedMesh *)smd->target->derivedFinal;

		numVerts = result->getNumVerts(result);
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);	//TODO: cddm doens't supports vertex groups :S
		vert  = result->getVertDataArray(result, CD_MVERT);

		/* TODO: Check about editMesh stuff :S */
		/*
		if(G.obedit && G.editMesh)
			target_dm = CDDM_from_editmesh(G.editMesh, smd->target->data); // Needs release before returning
		else
		*/


		//Calculate matrixs for local <-> target

		//Update inverse matrixs
		Mat4Invert (ob->imat, ob->obmat);
		Mat4Invert (smd->target->imat, smd->target->obmat);

		Mat4MulSerie(local2target, ob->obmat, smd->target->imat, 0, 0, 0, 0, 0, 0);
		Mat4MulSerie(target2local, smd->target->obmat, ob->imat, 0, 0, 0, 0, 0, 0);


		//Shrink (calculate each vertex final position)
		for(i = 0; i<numVerts; i++)
		{
			float weight;
			float orig[3], final[3]; //Coords relative to target_dm

			if(dvert && vgroup >= 0)
			{
				weight = 0.0f;
				for(j = 0; j < dvert[i].totweight; j++)
					if(dvert[i].dw[j].def_nr == vgroup)
					{
						weight = dvert[i].dw[j].weight;
						break;
					}
			}
			else weight = 1.0f;

			VecMat4MulVecfl(orig, local2target, vert[i].co);
			
			VECCOPY(final, orig);
			shrinkwrap_calc_nearest_point(target_dm, final, vert[i].no);

			//TODO linear interpolation: theres probably somewhere a function for this
			final[0] = orig[0] + weight * (final[0] - orig[0]);
			final[1] = orig[1] + weight * (final[1] - orig[1]);
			final[2] = orig[2] + weight * (final[2] - orig[2]);

			VecMat4MulVecfl(vert[i].co, target2local, final);
		}

		//Destroy faces, edges and stuff
		//Since we aren't yet constructing/destructing geom nothing todo for now
	}
	
	return result;
}


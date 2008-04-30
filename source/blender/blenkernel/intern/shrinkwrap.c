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
#include <math.h>
#include <stdio.h>
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
#include "BKE_global.h"

#include "BLI_arithb.h"


#define CONST
typedef void ( *Shrinkwrap_ForeachVertexCallback) (DerivedMesh *target, float *co, float *normal);


static void normal_short2float(CONST short *ns, float *nf)
{
	nf[0] = ns[0] / 32767.0f;
	nf[1] = ns[1] / 32767.0f;
	nf[2] = ns[2] / 32767.0f;
}


/*
 * This calculates the distance (in dir units) that the ray must travel to intersect plane
 * It can return negative values
 *
 * TODO theres probably something like this on blender code
 *
 * Returns FLT_MIN in parallel case
 */
static float ray_intersect_plane(CONST float *point, CONST float *dir, CONST float *plane_point, CONST float *plane_normal)
{
		float pp[3];
		float a, pp_dist;

		a = INPR(dir, plane_normal);

		if(fabs(a) < 1e-5f) return FLT_MIN;

		VecSubf(pp, point, plane_point);
		pp_dist = INPR(pp, plane_normal);

		return -pp_dist/a;
}

/*
 * Returns the minimum distance between the point and a triangle surface
 * Writes the nearest surface point in the given nearest
 */
static float nearest_point_in_tri_surface(CONST float *co, CONST float *v0, CONST float *v1, CONST float *v2, float *nearest)
{
	//TODO: make this efficient (probably this can be made with something like 3 point_in_slice())
	if(point_in_tri_prism(co, v0, v1, v2))
	{
		float normal[3];
		float dist;

		CalcNormFloat(v0, v1, v2, normal);
		dist = ray_intersect_plane(co, normal, v0, normal);

		VECADDFAC(nearest, co, normal, dist);
		return fabs(dist);
	}
	else
	{
		float dist = FLT_MAX, tdist;
		float closest[3];

		PclosestVL3Dfl(closest, co, v0, v1);
		tdist = VecLenf(co, closest);
		if(tdist < dist)
		{
			dist = tdist;
			VECCOPY(nearest, closest);
		}

		PclosestVL3Dfl(closest, co, v1, v2);
		tdist = VecLenf(co, closest);
		if(tdist < dist)
		{
			dist = tdist;
			VECCOPY(nearest, closest);
		}

		PclosestVL3Dfl(closest, co, v2, v0);
		tdist = VecLenf(co, closest);
		if(tdist < dist)
		{
			dist = tdist;
			VECCOPY(nearest, closest);
		}

		return dist;
	}
}



/*
 * Shrink to nearest surface point on target mesh
 */
static void bruteforce_shrinkwrap_calc_nearest_surface_point(DerivedMesh *target, float *co, float *unused)
{
	//TODO: this should use raycast code probably existent in blender
	float minDist = FLT_MAX;
	float orig_co[3];

	int i;
	int	numFaces = target->getNumFaces(target);
	MVert *vert = target->getVertDataArray(target, CD_MVERT);
	MFace *face = target->getFaceDataArray(target, CD_MFACE);

	VECCOPY(orig_co, co);

	for (i = 0; i < numFaces; i++)
	{
		float *v0, *v1, *v2, *v3;

		v0 = vert[ face[i].v1 ].co;
		v1 = vert[ face[i].v2 ].co;
		v2 = vert[ face[i].v3 ].co;
		v3 = face[i].v4 ? vert[ face[i].v4 ].co : 0;

		while(v2)
		{
			float dist;
			float tmp[3];

			dist = nearest_point_in_tri_surface(orig_co, v0, v1, v2, tmp);

			if(dist < minDist)
			{
				minDist = dist;
				VECCOPY(co, tmp);
			}

			v1 = v2;
			v2 = v3;
			v3 = 0;
		}
	}
}

/*
 * Projects the vertex on the normal direction over the target mesh
 */
static void bruteforce_shrinkwrap_calc_normal_projection(DerivedMesh *target, float *co, float *vnormal)
{
	//TODO: this should use raycast code probably existent in blender
	float minDist = FLT_MAX;
	float orig_co[3];

	int i;
	int	numFaces = target->getNumFaces(target);
	MVert *vert = target->getVertDataArray(target, CD_MVERT);
	MFace *face = target->getFaceDataArray(target, CD_MFACE);

	VECCOPY(orig_co, co);

	for (i = 0; i < numFaces; i++)
	{
		float *v0, *v1, *v2, *v3;

		v0 = vert[ face[i].v1 ].co;
		v1 = vert[ face[i].v2 ].co;
		v2 = vert[ face[i].v3 ].co;
		v3 = face[i].v4 ? vert[ face[i].v4 ].co : 0;

		while(v2)
		{
			float dist;
			float pnormal[3];

			CalcNormFloat(v0, v1, v2, pnormal);
			dist =  ray_intersect_plane(orig_co, vnormal, v0, pnormal);

			if(fabs(dist) < minDist)
			{
				float tmp[3], nearest[3];
				VECADDFAC(tmp, orig_co, vnormal, dist);

				if( fabs(nearest_point_in_tri_surface(tmp, v0, v1, v2, nearest)) < 0.0001)
				{
					minDist = fabs(dist);
					VECCOPY(co, nearest);
				}
			}
			v1 = v2;
			v2 = v3;
			v3 = 0;
		}
	}
}

/*
 * Shrink to nearest vertex on target mesh
 */
static void bruteforce_shrinkwrap_calc_nearest_vertex(DerivedMesh *target, float *co, float *unused)
{
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
		sdist = INPR(diff, diff);
		
		if(sdist < minDist)
		{
			minDist = sdist;
			VECCOPY(co, vert[i].co);
		}
	}
}


static void shrinkwrap_calc_foreach_vertex(ShrinkwrapCalcData *calc, Shrinkwrap_ForeachVertexCallback callback)
{
	int i, j;
	int vgroup		= get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	int	numVerts	= 0;

	MDeformVert *dvert = NULL;
	MVert		*vert  = NULL;

	numVerts = calc->final->getNumVerts(calc->final);
	dvert = calc->final->getVertDataArray(calc->final, CD_MDEFORMVERT);
	vert  = calc->final->getVertDataArray(calc->final, CD_MVERT);

	//Shrink (calculate each vertex final position)
	for(i = 0; i<numVerts; i++)
	{
		float weight;

		float orig[3], final[3]; //Coords relative to target
		float normal[3];

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

		if(weight == 0.0f) continue;	//Skip vertexs where we have no influence

		VecMat4MulVecfl(orig, calc->local2target, vert[i].co);
		VECCOPY(final, orig);

		//We also need to apply the rotation to normal
		if(calc->smd->shrinkType == MOD_SHRINKWRAP_NORMAL)
		{
			normal_short2float(vert[i].no, normal);
			Mat4Mul3Vecfl(calc->local2target, normal);
			Normalize(normal);	//Watch out for scaling (TODO: do we really needed a unit-len normal?)
		}
		(callback)(calc->target, final, normal);

		VecLerpf(final, orig, final, weight);	//linear interpolation

		VecMat4MulVecfl(vert[i].co, calc->target2local, final);
	}
}

/* Main shrinkwrap function */
DerivedMesh *shrinkwrapModifier_do(ShrinkwrapModifierData *smd, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{

	ShrinkwrapCalcData calc;


	//Init Shrinkwrap calc data
	calc.smd = smd;

	calc.original = dm;
	calc.final = CDDM_copy(calc.original);

	if(smd->target)
	{
		calc.target = (DerivedMesh *)smd->target->derivedFinal;

		if(!calc.target)
		{
			printf("Target derived mesh is null! :S\n");
		}

		//TODO should we reduce the number of matrix mults? by choosing applying matrixs to target or to derived mesh?
		//Calculate matrixs for local <-> target
		Mat4Invert (smd->target->imat, smd->target->obmat);	//inverse is outdated
		Mat4MulSerie(calc.local2target, smd->target->imat, ob->obmat, 0, 0, 0, 0, 0, 0);
		Mat4Invert(calc.target2local, calc.local2target);

	}

	calc.moved = NULL;


	//Projecting target defined - lets work!
	if(calc.target)
	{
		switch(smd->shrinkType)
		{
			case MOD_SHRINKWRAP_NEAREST_SURFACE:
//				shrinkwrap_calc_nearest_vertex(&calc);
				shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_surface_point);
			break;

			case MOD_SHRINKWRAP_NORMAL:
				shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_surface_point);
			break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:
				shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_vertex);
			break;
		}

	}

	//Destroy faces, edges and stuff
	if(calc.moved)
	{
		//TODO
	}

	CDDM_calc_normals(calc.final);	

	return calc.final;
}



/**
 * deform_simple.c
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
 * Contributor(s): André Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_simple_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_deform.h"
#include "BKE_utildefines.h"
#include "BLI_arithb.h"

#include <string.h>
#include <math.h>


static void axis_limit(int axis, const float limits[2], float *co, float *dcut)
{
	float val = co[axis];
	if(limits[0] > val) val = limits[0];
	if(limits[1] < val) val = limits[1];

	dcut[axis] = co[axis] - val;
	co[axis] = val;
}

static void simpleDeform_tapperXY(const float factor, const float *dcut, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	float scale = z*factor;

	co[0] = x + x*scale;
	co[1] = y + y*scale;
	co[2] = z;

	if(dcut)
	{
		co[0] += dcut[0]*scale;
		co[1] += dcut[1]*scale;
		co[2] += dcut[2];
	}
}

static void simpleDeform_strech(const float factor, const float dcut[3], float *co)
{
	float x = co[0], y = co[1], z = co[2];
	float scale;

	scale = (z*z*factor-factor + 1.0);

	co[0] = x*scale;
	co[1] = y*scale;
	co[2] = z*(1.0+factor);


	if(dcut)
	{
		co[0] += dcut[0]*scale;
		co[1] += dcut[0]*scale;
		co[2] += dcut[2]; 
	}
}

static void simpleDeform_squash(const float factor, const float dcut[3], float *co)
{
	float x = co[0], y = co[1], z = co[2];
	float scale;

	scale = z*factor;
	scale = -scale*scale;

	co[0] += x+x*scale;
	co[1] += y+y*scale;

	if(dcut)
	{
		co[0] += dcut[0]*scale;
		co[1] += dcut[0]*scale;
		co[2] += dcut[2]; 
	}
}

static void simpleDeform_tapperX(const float factor, const float *dcut, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	float scale = z*factor;

	co[0] = x+ x*scale;
	co[1] = y;
	co[2] = z;

	if(dcut)
	{
		co[0] += dcut[0]*scale;
		co[1] += dcut[1];
		co[2] += dcut[2];
	}
}

static void simpleDeform_twist(const float factor, const float *dcut, float *co)
{
	float x = co[0], y = co[1], z = co[2];
	float theta, sint, cost;

	theta = z*factor;
	sint  = sin(theta);
	cost  = cos(theta);

	co[0] = x*cost - y*sint;
	co[1] = x*sint + y*cost;
	co[2] = z;

	if(dcut)
	{
		co[0] += dcut[0];
		co[1] += dcut[1];
		co[2] += dcut[2];
	}
}

static void simpleDeform_bend(const float factor, const float dcut[3], float *co)
{
	float x = co[0], y = co[1], z = co[2];
	float theta, sint, cost;

	theta = x*factor;
	sint = sin(theta);
	cost = cos(theta);

	if(fabs(factor) > 1e-7f)
	{
		co[0] = -(y-1.0f/factor)*sint;
		co[1] =  (y-1.0f/factor)*cost + 1.0f/factor;
		co[2] = z;
	}


	if(dcut)
	{
		co[0] += cost*dcut[0];
		co[1] += sint*dcut[0];
		co[2] += dcut[2]; 
	}

}


/* simple deform modifier */
void SimpleDeformModifier_do(SimpleDeformModifierData *smd, struct Object *ob, struct DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{
	int i;
	float (*ob2mod)[4] = NULL, (*mod2ob)[4] = NULL;
	float tmp_matrix[2][4][4];

	int vgroup = get_named_vertexgroup_num(ob, smd->vgroup_name);

	MDeformVert *dvert = NULL;

	//Calculate matrixs do convert between coordinate spaces
	if(smd->origin)
	{
		//inverse is outdated
		Mat4Invert(smd->origin->imat, smd->origin->obmat);
		Mat4Invert(ob->imat, ob->obmat);

		ob2mod = tmp_matrix[0];
		mod2ob = tmp_matrix[1];
		Mat4MulSerie(ob2mod, smd->origin->imat, ob->obmat, 0, 0, 0, 0, 0, 0);
		Mat4Invert(mod2ob, ob2mod);
	}


	if(dm)
		dvert	= dm->getVertDataArray(dm, CD_MDEFORMVERT);

	for(i=0; i<numVerts; i++)
	{
		float co[3], dcut[3];
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);

		if(weight == 0) continue;

		if(ob2mod)
			Mat4MulVecfl(ob2mod, vertexCos[i]);

		dcut[0] = dcut[1] = dcut[2] = 0.0f;
		VECCOPY(co, vertexCos[i]);

		switch(smd->mode)
		{
			case MOD_SIMPLEDEFORM_MODE_TWIST:
				axis_limit(2, smd->factor+1, co, dcut);
				simpleDeform_twist	 (smd->factor[0], dcut, co);
				break;

			case MOD_SIMPLEDEFORM_MODE_BEND:
				axis_limit(0, smd->factor+1, co, dcut);
				simpleDeform_bend	 (smd->factor[0], dcut, co);
				break;

			case MOD_SIMPLEDEFORM_MODE_TAPER_X:
				axis_limit(2, smd->factor+1, co, dcut);
				simpleDeform_tapperX (smd->factor[0], dcut, co);
				break;

			case MOD_SIMPLEDEFORM_MODE_TAPER_XY:
				axis_limit(2, smd->factor+1, co, dcut);
				simpleDeform_tapperXY(smd->factor[0], dcut, co);
				break;

			case MOD_SIMPLEDEFORM_MODE_STRECH_SQUASH:
				axis_limit(2, smd->factor+1, co, dcut);
				simpleDeform_strech(smd->factor[0], dcut, co);
				break;
		}

		//linear interpolation
		VecLerpf(vertexCos[i], vertexCos[i], co, weight);

		if(mod2ob)
			Mat4MulVecfl(mod2ob, vertexCos[i]);
	}
}



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
#include "BLI_arithb.h"

#include <string.h>
#include <math.h>




static void simpleDeform_tapperXY(const float factor, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	co[0] = x*(1.0f + z*factor);
	co[1] = y*(1.0f + z*factor);
	co[2] = z;
}

static void simpleDeform_tapperX(const float factor, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	co[0] = x*(1.0f + z*factor);
	co[1] = y;
	co[2] = z;
}

static void simpleDeform_twist(const float factor, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	float theta = z*factor;
	float sint = sin(theta);
	float cost = cos(theta);

	co[0] = x*cost - y*sint;
	co[1] = x*sint + y*cost;
	co[2] = z;

}

static void simpleDeform_bend(const float factor, const float axis_limit[2], float *co)
{
	float x = co[0], y = co[1], z = co[2];

	float x0 = 0.0f;
	float theta = x*factor, sint, cost;

	if(x > axis_limit[1])
	{
		x0 = axis_limit[1] - x;
		x = axis_limit[1];
	}
	else if(x < axis_limit[0])
	{
		x0 = axis_limit[0] - x;
		x = axis_limit[0];
	}

	theta = x*factor;
	sint = sin(theta);
	cost = cos(theta);

	co[0] = -y*sint - cost*x0;
	co[1] =  y*cost - sint*x0;
	co[2] =  z;
}

/* simple deform modifier */
void SimpleDeformModifier_do(SimpleDeformModifierData *smd, struct Object *ob, float (*vertexCos)[3], int numVerts)
{
	float (*ob2mod)[4] = NULL, (*mod2ob)[4] = NULL;
	float tmp[2][4][4];

	if(smd->origin)
	{
		//inverse is outdated
		Mat4Invert(smd->origin->imat, smd->origin->obmat);

		ob2mod = tmp[0];
		mod2ob = tmp[1];
		Mat4MulSerie(ob2mod, smd->origin->imat, ob->obmat, 0, 0, 0, 0, 0, 0);
		Mat4Invert(mod2ob, ob2mod);
	}


	for(; numVerts; numVerts--, vertexCos++)
	{
		if(ob2mod)
			Mat4MulVecfl(ob2mod, *vertexCos);

		switch(smd->mode)
		{
			case MOD_SIMPLEDEFORM_MODE_TWIST:		simpleDeform_twist(smd->factor[0], *vertexCos); break;
			case MOD_SIMPLEDEFORM_MODE_BEND:		simpleDeform_bend(smd->factor[0], smd->factor+1, *vertexCos); break;
			case MOD_SIMPLEDEFORM_MODE_TAPER_X:		simpleDeform_tapperX (smd->factor[0], *vertexCos); break;
			case MOD_SIMPLEDEFORM_MODE_TAPER_XY:	simpleDeform_tapperXY(smd->factor[0], *vertexCos); break;
		}

		if(mod2ob)
			Mat4MulVecfl(mod2ob, *vertexCos);
	}
}



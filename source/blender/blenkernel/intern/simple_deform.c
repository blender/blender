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


static void simpleDeform_tapper(const float factor, float *co)
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

static void simpleDeform_bend(const float factor, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	float x0 = 0.0f;
	float theta = (x - x0)*factor;
	float sint = sin(theta);
	float cost = cos(theta);

	co[0] = -sint*(y-1.0f/factor) + x0;
	co[1] =  cost*(y-1.0f/factor) + 1.0f/factor;
	co[2] =  z;
}

static void simpleDeform_shear(const float factor, float *co)
{
	float x = co[0], y = co[1], z = co[2];

	co[0] = x + factor;
	co[1] = y;
	co[2] = z;
}

/* simple deform modifier */
void SimpleDeformModifier_do(SimpleDeformModifierData *smd, float (*vertexCos)[3], int numVerts)
{
	float (*ob2mod)[4] = NULL, (*mod2ob)[4] = NULL;
	if(smd->origin)
	{
		Mat4Invert(smd->origin->imat, smd->origin->obmat);	//inverse is outdated

		ob2mod = smd->origin->imat;
		mod2ob = smd->origin->obmat;
	}


	for(; numVerts; numVerts--, vertexCos++)
	{
		if(ob2mod)
			Mat4MulVecfl(ob2mod, *vertexCos);

		switch(smd->mode)
		{
			case 0: simpleDeform_tapper	(smd->factor[0], *vertexCos); break;
			case 1: simpleDeform_twist	(smd->factor[0], *vertexCos); break;
			case 2: simpleDeform_bend	(smd->factor[0], *vertexCos); break;
			case 3: simpleDeform_shear	(smd->factor[0], *vertexCos); break;
		}

		if(mod2ob)
			Mat4MulVecfl(mod2ob, *vertexCos);
	}
}



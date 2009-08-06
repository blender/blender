/**
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2009 by Daniel Genrich
 * All rights reserved.
 *
 * Contributor(s): None
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "FLUID_3D.h"

#include <stdio.h>
#include <stdlib.h>

// y in smoke is z in blender
extern "C" FLUID_3D *smoke_init(int *res, int amplify, float *p0, float *p1, float dt)
{
	// smoke lib uses y as top-bottom/vertical axis where blender uses z
	FLUID_3D *fluid = new FLUID_3D(res, amplify, p0, dt);

	// printf("xres: %d, yres: %d, zres: %d\n", res[0], res[1], res[2]);

	return fluid;
}

extern "C" void smoke_free(FLUID_3D *fluid)
{
	delete fluid;
	fluid = NULL;
}

extern "C" void smoke_step(FLUID_3D *fluid)
{
	fluid->step();
}

extern "C" void smoke_initBlenderRNA(FLUID_3D *fluid, float *alpha, float *beta)
{
	fluid->initBlenderRNA(alpha, beta);
}

template < class T > inline T ABS( T a ) {
	return (0 < a) ? a : -a ;
}

extern "C" float *smoke_get_density(FLUID_3D *fluid)
{
	return fluid->_density;
}

extern "C" float *smoke_get_heat(FLUID_3D *fluid)
{
	return fluid->_heat;
}

extern "C" float *smoke_get_velocity_x(FLUID_3D *fluid)
{
	return fluid->_xVorticity;
}

extern "C" float *smoke_get_velocity_y(FLUID_3D *fluid)
{
	return fluid->_yVorticity;
}

extern "C" float *smoke_get_velocity_z(FLUID_3D *fluid)
{
	return fluid->_zVorticity;
}

extern "C" float *smoke_get_bigdensity(FLUID_3D *fluid)
{
	return fluid->_wTurbulence ? fluid->_wTurbulence->getDensityBig() : NULL;
}

extern "C" void smoke_get_bigres(FLUID_3D *fluid, int *res)
{
	if(fluid->_wTurbulence)
	{
		Vec3Int r = fluid->_wTurbulence->getResBig();
		res[0] = r[0];
		res[1] = r[1];
		res[2] = r[2];
	}
}

extern "C" unsigned char *smoke_get_obstacle(FLUID_3D *fluid)
{
	return fluid->_obstacles;
}

extern "C" size_t smoke_get_index(int x, int max_x, int y, int max_y, int z /*, int max_z */)
{
	// // const int index = x + y * smd->res[0] + z * smd->res[0]*smd->res[1];
	return x + y * max_x + z * max_x*max_y;
}

extern "C" size_t smoke_get_index2d(int x, int max_x, int y /*, int max_y, int z, int max_z */)
{
	return x + y * max_x;
}

extern "C" void smoke_set_noise(FLUID_3D *fluid, int type)
{
	fluid->_wTurbulence->setNoise(type);
}

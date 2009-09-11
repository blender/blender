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
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "FLUID_3D.h"
#include "WTURBULENCE.h"

#include <stdio.h>
#include <stdlib.h>

// y in smoke is z in blender
extern "C" FLUID_3D *smoke_init(int *res, float *p0, float dt)
{
	// smoke lib uses y as top-bottom/vertical axis where blender uses z
	FLUID_3D *fluid = new FLUID_3D(res, p0, dt);

	// printf("xres: %d, yres: %d, zres: %d\n", res[0], res[1], res[2]);

	return fluid;
}

extern "C" WTURBULENCE *smoke_turbulence_init(int *res, int amplify, int noisetype)
{
	// initialize wavelet turbulence
	if(amplify)
		return new WTURBULENCE(res[0],res[1],res[2], amplify, noisetype);
	else 
		return NULL;
}

extern "C" void smoke_free(FLUID_3D *fluid)
{
	delete fluid;
	fluid = NULL;
}

extern "C" void smoke_turbulence_free(WTURBULENCE *wt)
{
	 delete wt;
	 wt = NULL;
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

extern "C" void smoke_step(FLUID_3D *fluid, size_t framenr)
{
	fluid->step();
}

extern "C" void smoke_turbulence_step(WTURBULENCE *wt, FLUID_3D *fluid)
{
	if(wt) 
		wt->stepTurbulenceFull(fluid->_dt/fluid->_dx, fluid->_xVelocity, fluid->_yVelocity, fluid->_zVelocity, fluid->_obstacles); 
}

extern "C" void smoke_initBlenderRNA(FLUID_3D *fluid, float *alpha, float *beta)
{
	fluid->initBlenderRNA(alpha, beta);
}

extern "C" void smoke_dissolve(FLUID_3D *fluid, int speed, int log)
{
	float *density = fluid->_density;
	//float *densityOld = fluid->_densityOld;
	float *heat = fluid->_heat;

	if(log)
	{
		/* max density/speed = dydx */
		float dydx = 1.0 / (float)speed;
		size_t size= fluid->_xRes * fluid->_yRes * fluid->_zRes;

		for(size_t i = 0; i < size; i++)
		{
			density[i] *= (1.0 - dydx);

			if(density[i] < 0.0f)
				density[i] = 0.0f;

			heat[i] *= (1.0 - dydx);

			if(heat[i] < 0.0f)
				heat[i] = 0.0f;
		}
	}
	else // linear falloff
	{
		/* max density/speed = dydx */
		float dydx = 1.0 / (float)speed;
		size_t size= fluid->_xRes * fluid->_yRes * fluid->_zRes;

		for(size_t i = 0; i < size; i++)
		{
			density[i] -= dydx;

			if(density[i] < 0.0f)
				density[i] = 0.0f;

			heat[i] -= dydx;

			if(heat[i] < 0.0f)
				heat[i] = 0.0f;
				
		}
	}
}

extern "C" void smoke_dissolve_wavelet(WTURBULENCE *wt, int speed, int log)
{
	float *density = wt->getDensityBig();
	Vec3Int r = wt->getResBig();

	if(log)
	{
		/* max density/speed = dydx */
		float dydx = 1.0 / (float)speed;
		size_t size= r[0] * r[1] * r[2];

		for(size_t i = 0; i < size; i++)
		{
			density[i] *= (1.0 - dydx);

			if(density[i] < 0.0f)
				density[i] = 0.0f;
		}
	}
	else // linear falloff
	{
		/* max density/speed = dydx */
		float dydx = 1.0 / (float)speed;
		size_t size= r[0] * r[1] * r[2];

		for(size_t i = 0; i < size; i++)
		{
			density[i] -= dydx;

			if(density[i] < 0.0f)
				density[i] = 0.0f;				
		}
	}
}

extern "C" void smoke_initWaveletBlenderRNA(WTURBULENCE *wt, float *strength)
{
	wt->initBlenderRNA(strength);
}

template < class T > inline T ABS( T a ) {
	return (0 < a) ? a : -a ;
}

extern "C" void smoke_export(FLUID_3D *fluid, float *dt, float *dx, float **dens, float **densold, float **heat, float **heatold, float **vx, float **vy, float **vz, float **vxold, float **vyold, float **vzold, unsigned char **obstacles)
{
	*dens = fluid->_density;
	*densold = fluid->_densityOld;
	*heat = fluid->_heat;
	*heatold = fluid->_heatOld;
	*vx = fluid->_xVelocity;
	*vy = fluid->_yVelocity;
	*vz = fluid->_zVelocity;
	*vxold = fluid->_xVelocityOld;
	*vyold = fluid->_yVelocityOld;
	*vzold = fluid->_zVelocityOld;
	*obstacles = fluid->_obstacles;
	dt = &(fluid->_dt);
	dx = &(fluid->_dx);

}

extern "C" void smoke_turbulence_export(WTURBULENCE *wt, float **dens, float **densold, float **tcu, float **tcv, float **tcw)
{
	if(!wt)
		return;

	*dens = wt->_densityBig;
	*densold = wt->_densityBigOld;
	*tcu = wt->_tcU;
	*tcv = wt->_tcV;
	*tcw = wt->_tcW;
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
	return fluid->_xVelocity;
}

extern "C" float *smoke_get_velocity_y(FLUID_3D *fluid)
{
	return fluid->_yVelocity;
}

extern "C" float *smoke_get_velocity_z(FLUID_3D *fluid)
{
	return fluid->_zVelocity;
}

extern "C" float *smoke_turbulence_get_density(WTURBULENCE *wt)
{
	return wt ? wt->getDensityBig() : NULL;
}

extern "C" void smoke_turbulence_get_res(WTURBULENCE *wt, int *res)
{
	if(wt)
	{
		Vec3Int r = wt->getResBig();
		res[0] = r[0];
		res[1] = r[1];
		res[2] = r[2];
	}
}

extern "C" unsigned char *smoke_get_obstacle(FLUID_3D *fluid)
{
	return fluid->_obstacles;
}

extern "C" void smoke_turbulence_set_noise(WTURBULENCE *wt, int type)
{
	wt->setNoise(type);
}

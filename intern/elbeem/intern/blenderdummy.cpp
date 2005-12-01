/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Blender call for disabled fluidsim
 *
 *****************************************************************************/
 
#ifdef ELBEEM_DUMMIES

#include <stdlib.h>
#include "ntl_vector3dim.h"

extern "C" 
int performElbeemSimulation(char *cfgfilename) {
	return 1; // dummy
};

// dummies from intern/elbeem/intern/solver_interface.cpp
// for utilities.cpp

void initGridSizes(int &sizex, int &sizey, int &sizez,
		ntlVec3Gfx &geoStart, ntlVec3Gfx &geoEnd, 
		int mMaxRefine, bool parallel) 
{
	// dummy
}

void calculateMemreqEstimate( int resx,int resy,int resz, int refine,
		double *reqret, string *reqstr) {
	*reqret =  0.0; // dummy
}

#endif // ELBEEM_DUMMIES


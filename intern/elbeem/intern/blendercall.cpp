/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Blender call interface
 *
 *****************************************************************************/
 
#include "globals.h"
#include "ntl_raytracer.h"
#include "ntl_blenderdumper.h"

extern "C" 
int performElbeemSimulation(char *cfgfilename) {
	fprintf(GEN_userstream, "Running El'Beem from Blender with file '%s' ...\n",cfgfilename);
	// load given file in command line mode
	ntlBlenderDumper elbeem(cfgfilename, true);
	myTime_t timestart = getTime();
	elbeem.renderAnimation();
	myTime_t timeend = getTime();
	fprintf(GEN_userstream, "El'Beem simulation done, time: %f seconds.\n", ((timeend-timestart)/(double)1000.0) ); 
	return 1;
};



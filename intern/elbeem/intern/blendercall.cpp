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
#include <stdlib.h>

extern "C" 
int performElbeemSimulation(char *cfgfilename) {
	const char *strEnvName = "BLENDER_ELBEEMDEBUG";
	gWorldState = SIMWORLD_INVALID;
	strcpy(gWorldStringState,"[none]");
	if(getenv(strEnvName)) {
		gDebugLevel = atoi(getenv(strEnvName));
		if(gDebugLevel< 0) gDebugLevel =  0;
		if(gDebugLevel>10) gDebugLevel =  0; // only use valid values
		if(gDebugLevel>0) fprintf(stderr, "Using envvar '%s'='%s', debugLevel set to: %d\n",strEnvName, getenv(strEnvName), gDebugLevel);
	}
	if(gDebugLevel>0) fprintf(GEN_userstream, "Running El'Beem from Blender with file '%s', debugLevel:%d ...\n",cfgfilename,gDebugLevel);
	// load given file in command line mode
	ntlBlenderDumper elbeem(cfgfilename, true);
	if(SIMWORLD_OK()) {
		gWorldState = SIMWORLD_INITED;
		myTime_t timestart = getTime();
		elbeem.renderAnimation();
		myTime_t timeend = getTime();
		if(gDebugLevel>0) fprintf(GEN_userstream, "El'Beem simulation done, time: %f seconds.\n", ((timeend-timestart)/(double)1000.0) ); 
	} else {
	}
	return 1;
};



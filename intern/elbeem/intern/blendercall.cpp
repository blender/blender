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
#include "utilities.h"
#include "ntl_blenderdumper.h"
#include <stdlib.h>

// ELBEEM_BLENDER always =1 here
extern "C" void elbeemCheckDebugEnv(void);

extern "C" 
int performElbeemSimulation(char *cfgfilename) {
	gElbeemState = SIMWORLD_INVALID;
	strcpy(gElbeemErrorString,"[none]");

	//if(gDebugLevel>0) {
	elbeemCheckDebugEnv();
	debMsgStd("performElbeemSimulation",DM_NOTIFY,"Running El'Beem from Blender with file '"<< cfgfilename <<"', debugLevel:"<<gDebugLevel<<" ...\n", 2);
	//}
	// load given file in command line mode
	ntlBlenderDumper elbeem(cfgfilename, true);
	if(SIMWORLD_OK()) {
		gElbeemState = SIMWORLD_INITED;
		myTime_t timestart = getTime();
		elbeem.renderAnimation();
		myTime_t timeend = getTime();
		debMsgStd("performElbeemSimulation",DM_NOTIFY, "El'Beem simulation done, time: "<<((timeend-timestart)/(double)1000.0) <<" seconds.\n", 2 ); 
	} else {
		// signal there was an initialization problem
		setGlobalBakeState( -2 );
	}
	return 1;
};



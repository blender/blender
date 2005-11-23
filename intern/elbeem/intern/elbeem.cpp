/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Main program functions
 */

#include "elbeem.h"
#include "ntl_blenderdumper.h"
extern "C" void elbeemCheckDebugEnv(void);

#include "ntl_scene.h"
#include "ntl_geometrymodel.h"

/*****************************************************************************/
// region of interest global vars
// currently used by e.g. fsgr solver
double guiRoiSX = 0.0;
double guiRoiSY = 0.0;
double guiRoiSZ = 0.0;
double guiRoiEX = 1.0;
double guiRoiEY = 1.0;
double guiRoiEZ = 1.0;
int guiRoiMaxLev=6, guiRoiMinLev=0;

//! global raytracer pointer (=world)
ntlWorld *gpWorld = NULL;

//! debug output switch
bool myDebugOut = false;

//! global leave program / abort variable
bool gQuit = false;


// API

int elbeemInit(elbeemSimulationSettings *settings) {
	gElbeemState = SIMWORLD_INVALID;
	strcpy(gElbeemErrorString,"[none]");

	elbeemCheckDebugEnv();
	debMsgStd("performElbeemSimulation",DM_NOTIFY,"El'Beem Simulation Init Start as Plugin, debugLevel:"<<gDebugLevel<<" ...\n", 2);
	
	// create world object with initial settings
	ntlBlenderDumper *elbeem = new ntlBlenderDumper(settings);
	gpWorld = elbeem;
	return 0;
}

int elbeemAddMesh(elbeemMesh *mesh) {
	int initType = -1;
	switch(mesh->type) {
		case OB_FLUIDSIM_OBSTACLE: initType = FGI_BNDNO; break;
		case OB_FLUIDSIM_FLUID: initType = FGI_FLUID; break;
		case OB_FLUIDSIM_INFLOW: initType = FGI_MBNDINFLOW; break;
		case OB_FLUIDSIM_OUTFLOW: initType = FGI_MBNDOUTFLOW; break;
	}
	// invalid type?
	if(initType<0) return 1;
	
	ntlGeometryObjModel *obj = new ntlGeometryObjModel( );
	gpWorld->getRenderGlobals()->getScene()->addGeoClass( obj );
	obj->initModel(mesh->numVertices, mesh->vertices, mesh->numTriangles, mesh->triangles);
	obj->setGeoInitId(true);
	obj->setGeoInitIntersect(true);
	obj->setGeoInitType(initType);
	obj->setInitialVelocity( ntlVec3Gfx(mesh->iniVelx, mesh->iniVely, mesh->iniVelz) );
	return 0;
}

int elbeemSimulate(void) {
	if(!gpWorld) return 1;

	gpWorld->finishWorldInit();
	
	if(SIMWORLD_OK()) {
		gElbeemState = SIMWORLD_INITED;
		myTime_t timestart = getTime();
		gpWorld->renderAnimation();
		myTime_t timeend = getTime();
		debMsgStd("performElbeemSimulation",DM_NOTIFY, "El'Beem simulation done, time: "<<((timeend-timestart)/(double)1000.0) <<" seconds.\n", 2 ); 

		// ok, we're done...
		delete gpWorld;
		gpWorld = NULL;
		return 0;
	} 

	// failure...
	return 1;
}




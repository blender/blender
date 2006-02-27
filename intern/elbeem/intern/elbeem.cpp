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

#include "ntl_world.h"
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


// API

// reset elbeemSimulationSettings struct with defaults
extern "C" 
void elbeemResetSettings(elbeemSimulationSettings *set) {
	if(!set) return;
  set->version = 2;
	for(int i=0 ; i<3; i++) set->geoStart[i] = 0.0;
 	for(int i=0 ; i<3; i++) set->geoSize[i] = 1.0;
  set->resolutionxyz = 64;
  set->previewresxyz = 24;
  set->realsize = 1.0;
  set->viscosity = 0.000001;

 	for(int i=0 ; i<2; i++) set->gravity[i] = 0.0;
 	set->gravity[2] = -9.81;

  set->animStart = 0; 
	set->aniFrameTime = 0.01;
	set->noOfFrames = 10;
  set->gstar = 0.005;
  set->maxRefine = -1;
  set->generateParticles = 1.0;
  strcpy(set->outputPath,"./elbeemdata_");

	set->channelSizeFrameTime=0;
	set->channelFrameTime=NULL;
	set->channelSizeViscosity=0;
	set->channelViscosity=NULL;
	set->channelSizeGravity=0;
	set->channelGravity=NULL; 

	set->obstacleType= FLUIDSIM_OBSTACLE_NOSLIP;
	set->obstaclePartslip= 0.;
	set->generateVertexVectors = 0;
	set->surfaceSmoothing = 1.;

	// init identity
	for(int i=0; i<16; i++) set->surfaceTrafo[i] = 0.0;
	for(int i=0; i<4; i++) set->surfaceTrafo[i*4+i] = 1.0;
}

// start fluidsim init
extern "C" 
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

// reset elbeemMesh struct with zeroes
extern "C" 
void elbeemResetMesh(elbeemMesh *mesh) {
	if(!mesh) return;
  mesh->type = 0;
  mesh->numVertices = 0;
	mesh->vertices = NULL;
	mesh->numTriangles = 0;
  mesh->triangles = NULL;
	mesh->channelSizeTranslation = 0;
	mesh->channelTranslation = NULL;
	mesh->channelSizeRotation = 0;
	mesh->channelRotation = NULL;
	mesh->channelSizeScale = 0;
	mesh->channelScale = NULL;
	mesh->channelSizeActive = 0;
	mesh->channelActive = NULL;
	mesh->channelSizeInitialVel = 0;
	mesh->channelInitialVel = NULL;
	mesh->localInivelCoords = 0;
	mesh->obstacleType= FLUIDSIM_OBSTACLE_NOSLIP;
	mesh->obstaclePartslip= 0.;
	mesh->name = "[unnamed]";
}

// add mesh as fluidsim object
extern "C" 
int elbeemAddMesh(elbeemMesh *mesh) {
	int initType = -1;
	switch(mesh->type) {
		case OB_FLUIDSIM_OBSTACLE: 
			if     (mesh->obstacleType==FLUIDSIM_OBSTACLE_PARTSLIP) initType = FGI_BNDPART; 
			else if(mesh->obstacleType==FLUIDSIM_OBSTACLE_FREESLIP) initType = FGI_BNDFREE; 
			else /*if(mesh->obstacleType==FLUIDSIM_OBSTACLE_NOSLIP)*/ initType = FGI_BNDNO; 
			break;
		case OB_FLUIDSIM_FLUID: initType = FGI_FLUID; break;
		case OB_FLUIDSIM_INFLOW: initType = FGI_MBNDINFLOW; break;
		case OB_FLUIDSIM_OUTFLOW: initType = FGI_MBNDOUTFLOW; break;
	}
	// invalid type?
	if(initType<0) return 1;
	
	ntlGeometryObjModel *obj = new ntlGeometryObjModel( );
	gpWorld->getRenderGlobals()->getSimScene()->addGeoClass( obj );
	obj->initModel(mesh->numVertices, mesh->vertices, mesh->numTriangles, mesh->triangles);
	if(mesh->name) obj->setName(std::string(mesh->name));
	else obj->setName(std::string("[unnamed]"));
	obj->setGeoInitId(1);
	obj->setGeoInitIntersect(true);
	obj->setGeoInitType(initType);
	obj->setGeoPartSlipValue(mesh->obstaclePartslip);
	// use channel instead, obj->setInitialVelocity( ntlVec3Gfx(mesh->iniVelocity[0], mesh->iniVelocity[1], mesh->iniVelocity[2]) );
	obj->initChannels(
			mesh->channelSizeTranslation, mesh->channelTranslation, 
			mesh->channelSizeRotation,    mesh->channelRotation, 
			mesh->channelSizeScale,       mesh->channelScale,
			mesh->channelSizeActive,      mesh->channelActive,
			mesh->channelSizeInitialVel,  mesh->channelInitialVel
		);
	obj->setLocalCoordInivel( mesh->localInivelCoords );

	debMsgStd("elbeemAddMesh",DM_MSG,"Added elbeem mesh: "<<obj->getName()<<" type="<<initType<<" "<<obj->getIsAnimated(), 9 );
	return 0;
}

// do the actual simulation
extern "C" 
int elbeemSimulate(void) {
	if(!gpWorld) return 1;

	gpWorld->finishWorldInit();
	
	if(SIMWORLD_OK()) {
		gElbeemState = SIMWORLD_INITED;
		myTime_t timestart = getTime();
		gpWorld->renderAnimation();
		myTime_t timeend = getTime();
		debMsgStd("elbeemSimulate",DM_NOTIFY, "El'Beem simulation done, time: "<<getTimeString(timeend-timestart)<<".\n", 2 ); 

		// ok, we're done...
		delete gpWorld;
		gpWorld = NULL;
		return 0;
	} 

	// failure...
	return 1;
}


// global vector to flag values to remove
vector<int> gKeepVal;

#define SIMPLIFY_FLOAT_EPSILON (1e-6f)
#define SIMPLIFY_DOUBLE_EPSILON (1e-12f)
#define SFLOATEQ(x,y)  (ABS((x)-(y)) < SIMPLIFY_FLOAT_EPSILON)
#define SDOUBLEEQ(x,y) (ABS((x)-(y)) < SIMPLIFY_DOUBLE_EPSILON)
#define SVECFLOATEQ(x,y)  ( \
	(ABS((x)[0]-(y)[0]) < SIMPLIFY_FLOAT_EPSILON) && \
	(ABS((x)[1]-(y)[1]) < SIMPLIFY_FLOAT_EPSILON) && \
	(ABS((x)[2]-(y)[2]) < SIMPLIFY_FLOAT_EPSILON) )

// helper function - simplify animation channels
extern "C" 
int elbeemSimplifyChannelFloat(float *channel, int* size) {
	bool changed = false;
	int nsize = *size;
	int orgsize = *size;
	if(orgsize<1) return false;
	gKeepVal.resize( orgsize );
	for(int i=0; i<orgsize; i++) { gKeepVal[i] = true; }
	const bool debugSF = false;

	float last = channel[0 + 0];
	for(int i=1; i<orgsize; i++) {
		float curr = channel[2*i + 0];
		bool remove = false;
		if(SFLOATEQ(curr,last)) remove = true;
		// dont remove if next value is different
		if((remove)&&(i<orgsize-1)) {
			float next = channel[2*(i+1)+0];
			if(!SFLOATEQ(next,curr)) remove = false;
		}
		if(remove) {
			changed = true;
			gKeepVal[i] = false;
			nsize--;
		}
		if(debugSF) errMsg("elbeemSimplifyChannelFloat","i"<<i<<"/"<<orgsize<<" v"<<channel[ (i*2) + 0 ]<<" t"<<channel[ (i*2) + 1 ]<<"   nsize="<<nsize<<" r"<<remove );
		last = curr;
	}

	if(changed) {
		nsize = 1;
		for(int i=1; i<orgsize; i++) {
			if(gKeepVal[i]) {
				channel[ (nsize*2) + 0 ] = channel[ (i*2) + 0 ];
				channel[ (nsize*2) + 1 ] = channel[ (i*2) + 1 ];
				nsize++;
			}
		}
		*size = nsize;
	}

	if(debugSF) for(int i=1; i<nsize; i++) {
		errMsg("elbeemSimplifyChannelFloat","n i"<<i<<"/"<<nsize<<" v"<<channel[ (i*2) + 0 ]<<" t"<<channel[ (i*2) + 1 ] );
	}

	return changed;
}

extern "C" 
int elbeemSimplifyChannelVec3(float *channel, int* size) {
	bool changed = false;
	int nsize = *size;
	int orgsize = *size;
	if(orgsize<1) return false;
	gKeepVal.resize( orgsize );
	for(int i=0; i<orgsize; i++) { gKeepVal[i] = true; }
	const bool debugVF = false;

	ntlVec3f last( channel[0 + 0], channel[0 + 1], channel[0 + 2] );
	for(int i=1; i<orgsize; i++) {
		ntlVec3f curr( channel[4*i + 0], channel[4*i + 1], channel[4*i + 2]);
		bool remove = false;
		if(SVECFLOATEQ(curr,last)) remove = true;
		// dont remove if next value is different
		if((remove)&&(i<orgsize-1)) {
			ntlVec3f next( channel[4*(i+1)+0], channel[4*(i+1)+1], channel[4*(i+1)+2]);
			if(!SVECFLOATEQ(next,curr)) remove = false;
		}
		if(remove) {
			changed = true;
			gKeepVal[i] = false;
			nsize--;
		}
		if(debugVF) errMsg("elbeemSimplifyChannelVec3","i"<<i<<"/"<<orgsize<<" v"<<
				channel[ (i*4) + 0 ]<<","<< channel[ (i*4) + 1 ]<<","<< channel[ (i*4) + 2 ]<<
				" t"<<channel[ (i*4) + 3 ]<<"   nsize="<<nsize<<" r"<<remove );
		last = curr;
	}

	if(changed) {
		nsize = 1;
		for(int i=1; i<orgsize; i++) {
			if(gKeepVal[i]) {
				for(int j=0; j<4; j++){ channel[ (nsize*4) + j ] = channel[ (i*4) + j ]; }
				nsize++;
			}
		}
		*size = nsize;
	}

	if(debugVF) for(int i=1; i<nsize; i++) {
		errMsg("elbeemSimplifyChannelVec3","n i"<<i<<"/"<<nsize<<" v"<<
				channel[ (i*4) + 0 ]<<","<< channel[ (i*4) + 1 ]<<","<< channel[ (i*4) + 2 ]<<
				" t"<<channel[ (i*4) + 3 ] );
	}

	return changed;
}


#undef SIMPLIFY_FLOAT_EPSILON
#undef SIMPLIFY_DOUBLE_EPSILON
#undef SFLOATEQ
#undef SDOUBLEEQ


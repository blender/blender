/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
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
  set->version = 3;
  set->domainId = 0;
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
  set->generateParticles = 0.0;
  set->numTracerParticles = 0;
  strcpy(set->outputPath,"./elbeemdata_");

	set->channelSizeFrameTime=0;
	set->channelFrameTime=NULL;
	set->channelSizeViscosity=0;
	set->channelViscosity=NULL;
	set->channelSizeGravity=0;
	set->channelGravity=NULL; 

	set->domainobsType= FLUIDSIM_OBSTACLE_NOSLIP;
	set->domainobsPartslip= 0.;
	set->generateVertexVectors = 0;
	set->surfaceSmoothing = 1.;
	set->surfaceSubdivs = 1;

	set->farFieldSize = 0.;
	set->runsimCallback = NULL;
	set->runsimUserData = NULL;

	// init identity
	for(int i=0; i<16; i++) set->surfaceTrafo[i] = 0.0;
	for(int i=0; i<4; i++) set->surfaceTrafo[i*4+i] = 1.0;
}

// start fluidsim init
extern "C" 
int elbeemInit() {
	setElbeemState( SIMWORLD_INITIALIZING );
	setElbeemErrorString("[none]");
	resetGlobalColorSetting();

	elbeemCheckDebugEnv();
	debMsgStd("performElbeemSimulation",DM_NOTIFY,"El'Beem Simulation Init Start as Plugin, debugLevel:"<<gDebugLevel<<" ...\n", 2);
	
	// create world object with initial settings
	ntlBlenderDumper *elbeem = new ntlBlenderDumper(); 
	gpWorld = elbeem;
	return 0;
}

// fluidsim end
extern "C" 
int elbeemFree() {
	
	return 0;
}

// start fluidsim init
extern "C" 
int elbeemAddDomain(elbeemSimulationSettings *settings) {
	// has to be inited...
	if((getElbeemState() == SIMWORLD_INVALID) && (!gpWorld)) { elbeemInit(); }
	if(getElbeemState() != SIMWORLD_INITIALIZING) { errFatal("elbeemAddDomain","Unable to init simulation world",SIMWORLD_INITERROR); }
	// create domain with given settings
	gpWorld->addDomain(settings);
	return 0;
}

// error message access
extern "C" 
void elbeemGetErrorString(char *buffer) {
	if(!buffer) return;
	strncpy(buffer,getElbeemErrorString(),256);
}

// reset elbeemMesh struct with zeroes
extern "C" 
void elbeemResetMesh(elbeemMesh *mesh) {
	if(!mesh) return;
	// init typedef struct elbeemMesh
  mesh->type = 0;

	mesh->parentDomainId = 0;

	/* vertices */
  mesh->numVertices = 0;
	mesh->vertices = NULL;

	mesh->channelSizeVertices = 0;
	mesh->channelVertices = NULL;

	/* triangles */
	mesh->numTriangles = 0;
  mesh->triangles = NULL;

	/* animation channels */
	mesh->channelSizeTranslation = 0;
	mesh->channelTranslation = NULL;
	mesh->channelSizeRotation = 0;
	mesh->channelRotation = NULL;
	mesh->channelSizeScale = 0;
	mesh->channelScale = NULL;

	/* active channel */
	mesh->channelSizeActive = 0;
	mesh->channelActive = NULL;

	mesh->channelSizeInitialVel = 0;
	mesh->channelInitialVel = NULL;

	mesh->localInivelCoords = 0;

	mesh->obstacleType= FLUIDSIM_OBSTACLE_NOSLIP;
	mesh->obstaclePartslip= 0.;
	mesh->obstacleImpactFactor= 1.;

	mesh->volumeInitType= OB_VOLUMEINIT_VOLUME;

	/* name of the mesh, mostly for debugging */
	mesh->name = "[unnamed]";
	
	/* fluid control settings */
	mesh->cpsTimeStart = 0;
	mesh->cpsTimeEnd = 0;
	mesh->cpsQuality = 0;
	
	mesh->channelSizeAttractforceStrength = 0;
	mesh->channelAttractforceStrength = NULL;
	mesh->channelSizeAttractforceRadius = 0;
	mesh->channelAttractforceRadius = NULL;
	mesh->channelSizeVelocityforceStrength = 0;
	mesh->channelVelocityforceStrength = NULL;
	mesh->channelSizeVelocityforceRadius = 0;
	mesh->channelVelocityforceRadius = NULL;
}

int globalMeshCounter = 1;
// add mesh as fluidsim object
extern "C" 
int elbeemAddMesh(elbeemMesh *mesh) {
	int initType;
	if(getElbeemState() != SIMWORLD_INITIALIZING) { errFatal("elbeemAddMesh","World and domain not initialized, call elbeemInit and elbeemAddDomain before...", SIMWORLD_INITERROR); }

	switch(mesh->type) {
		case OB_FLUIDSIM_OBSTACLE: 
			if     (mesh->obstacleType==FLUIDSIM_OBSTACLE_PARTSLIP) initType = FGI_BNDPART; 
			else if(mesh->obstacleType==FLUIDSIM_OBSTACLE_FREESLIP) initType = FGI_BNDFREE; 
			else /*if(mesh->obstacleType==FLUIDSIM_OBSTACLE_NOSLIP)*/ initType = FGI_BNDNO; 
			break;
		case OB_FLUIDSIM_FLUID: initType = FGI_FLUID; break;
		case OB_FLUIDSIM_INFLOW: initType = FGI_MBNDINFLOW; break;
		case OB_FLUIDSIM_OUTFLOW: initType = FGI_MBNDOUTFLOW; break;
		case OB_FLUIDSIM_CONTROL: initType = FGI_CONTROL; break;
		default: return 1; // invalid type
	}
	
	ntlGeometryObjModel *obj = new ntlGeometryObjModel( );
	gpWorld->getRenderGlobals()->getSimScene()->addGeoClass( obj );
	obj->initModel(
			mesh->numVertices, mesh->vertices, mesh->numTriangles, mesh->triangles,
			mesh->channelSizeVertices, mesh->channelVertices );
	if(mesh->name) obj->setName(string(mesh->name));
	else {
		char meshname[100];
		snprintf(meshname,100,"mesh%04d",globalMeshCounter);
		obj->setName(string(meshname));
	}
	globalMeshCounter++;
	obj->setGeoInitId( mesh->parentDomainId+1 );
	obj->setGeoInitIntersect(true);
	obj->setGeoInitType(initType);
	
	// abuse partslip value for control fluid: reverse control keys or not
	if(initType == FGI_CONTROL)
		obj->setGeoPartSlipValue(mesh->obstacleType);
	else
		obj->setGeoPartSlipValue(mesh->obstaclePartslip);
	
	obj->setGeoImpactFactor(mesh->obstacleImpactFactor);
	
	/* fluid control features */
	obj->setCpsTimeStart(mesh->cpsTimeStart);
	obj->setCpsTimeEnd(mesh->cpsTimeEnd);
	obj->setCpsQuality(mesh->cpsQuality);
	
	if((mesh->volumeInitType<VOLUMEINIT_VOLUME)||(mesh->volumeInitType>VOLUMEINIT_BOTH)) mesh->volumeInitType = VOLUMEINIT_VOLUME;
	obj->setVolumeInit(mesh->volumeInitType);
	// use channel instead, obj->setInitialVelocity( ntlVec3Gfx(mesh->iniVelocity[0], mesh->iniVelocity[1], mesh->iniVelocity[2]) );
	
	obj->initChannels(
			mesh->channelSizeTranslation, mesh->channelTranslation, 
			mesh->channelSizeRotation,    mesh->channelRotation, 
			mesh->channelSizeScale,       mesh->channelScale,
			mesh->channelSizeActive,      mesh->channelActive,
			mesh->channelSizeInitialVel,  mesh->channelInitialVel,
			mesh->channelSizeAttractforceStrength,  mesh->channelAttractforceStrength,
			mesh->channelSizeAttractforceRadius,  mesh->channelAttractforceRadius,
			mesh->channelSizeVelocityforceStrength,  mesh->channelVelocityforceStrength,
			mesh->channelSizeVelocityforceRadius,  mesh->channelVelocityforceRadius
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
	if( isSimworldOk() ) {
		myTime_t timestart = getTime();
		gpWorld->renderAnimation();
		myTime_t timeend = getTime();

		if(getElbeemState() != SIMWORLD_STOP) {
			// ok, we're done...
			delete gpWorld;
			
			gpWorld = NULL;
			debMsgStd("elbeemSimulate",DM_NOTIFY, "El'Beem simulation done, time: "<<getTimeString(timeend-timestart)<<".\n", 2 ); 
		} else {
			debMsgStd("elbeemSimulate",DM_NOTIFY, "El'Beem simulation stopped, time so far: "<<getTimeString(timeend-timestart)<<".", 2 ); 
		}
		return 0;
	} 

	// failure...
	return 1;
}


// continue a previously stopped simulation
extern "C" 
int elbeemContinueSimulation(void) {

	if(getElbeemState() != SIMWORLD_STOP) {
		errMsg("elbeemContinueSimulation","No running simulation found! Aborting...");
		if(gpWorld) delete gpWorld;
		return 1;
	}

	myTime_t timestart = getTime();
	gpWorld->renderAnimation();
	myTime_t timeend = getTime();

	if(getElbeemState() != SIMWORLD_STOP) {
		// ok, we're done...
		delete gpWorld;
		gpWorld = NULL;
		debMsgStd("elbeemContinueSimulation",DM_NOTIFY, "El'Beem simulation done, time: "<<getTimeString(timeend-timestart)<<".\n", 2 ); 
	} else {
		debMsgStd("elbeemContinueSimulation",DM_NOTIFY, "El'Beem simulation stopped, time so far: "<<getTimeString(timeend-timestart)<<".", 2 ); 
	}
	return 0;
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


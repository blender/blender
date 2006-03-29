/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Basic interface for all simulation modules
 *
 *****************************************************************************/

#include "simulation_object.h"
#include "solver_interface.h"
#include "ntl_bsptree.h"
#include "ntl_ray.h"
#include "ntl_world.h"
#include "solver_interface.h"
#include "particletracer.h"
#include "elbeem.h"

#ifdef _WIN32
#else
#include <sys/time.h>
#endif


//! lbm factory functions
LbmSolverInterface* createSolver();


/******************************************************************************
 * Constructor
 *****************************************************************************/
SimulationObject::SimulationObject() :
	ntlGeometryShader(),
	mGeoStart(-100.0), mGeoEnd(100.0),
	mGeoInitId(-1), mpGiTree(NULL), mpGiObjects(NULL),
	mpGlob(NULL),
	mPanic( false ),
	mDebugType( 1 /* =FLUIDDISPNothing*/ ),
	mStepsPerFrame( 10 ),
	mpLbm(NULL), mpParam( NULL ),
	mShowSurface(true), mShowParticles(false),
	mSelectedCid( NULL ),
	mpElbeemSettings( NULL )

{
	mpParam = new Parametrizer();
	//for(int i=0; i<MAX_DEBDISPSET; i++) { mDebDispSet[i].type  = (i); mDebDispSet[i].on    = false; mDebDispSet[i].scale = 1.0; }

	// reset time
	mTime 						= 0.0;
	mDisplayTime 			= 0.0;
}


/******************************************************************************
 * Destructor
 *****************************************************************************/
SimulationObject::~SimulationObject()
{
	if(mpGiTree)         delete mpGiTree;
	if(mpElbeemSettings) delete mpElbeemSettings;
	if(mpLbm)            delete mpLbm;
  if(mpParam)          delete mpParam;
	debMsgStd("SimulationObject",DM_MSG,"El'Beem Done!\n",10);
}



/*****************************************************************************/
/*! init tree for certain geometry init */
/*****************************************************************************/
void SimulationObject::initGeoTree(int id) {
	if(mpGlob == NULL) { 
		errFatal("SimulationObject::initGeoTree error","Requires globals!", SIMWORLD_INITERROR); 
		return;
	}
	mGeoInitId = id;
	ntlScene *scene = mpGlob->getSimScene();
	mpGiObjects = scene->getObjects();

	if(mpGiTree != NULL) delete mpGiTree;
	char treeFlag = (1<<(mGeoInitId+4));
	mpGiTree = new ntlTree( 20, 4, // warning - fixed values for depth & maxtriangles here...
												scene, treeFlag );
}

/*****************************************************************************/
/*! destroy tree etc. when geometry init done */
/*****************************************************************************/
void SimulationObject::freeGeoTree() {
	if(mpGiTree != NULL) delete mpGiTree;
}



// copy & remember settings for later use
void SimulationObject::copyElbeemSettings(elbeemSimulationSettings *settings) {
	mpElbeemSettings = new elbeemSimulationSettings;
	*mpElbeemSettings = *settings;
}

/******************************************************************************
 * simluation interface: initialize simulation using the given configuration file 
 *****************************************************************************/
int SimulationObject::initializeLbmSimulation(ntlRenderGlobals *glob)
{
	if(!SIMWORLD_OK()) return 1;
	
	// already inited?
	if(mpLbm) return 0;
	
	mpGlob = glob;
	if(!getVisible()) {
		mpAttrs->setAllUsed();
		return 0;
	}


	//mDimension, mSolverType are deprecated
	string mSolverType(""); 
	mSolverType = mpAttrs->readString("solver", mSolverType, "SimulationObject","mSolverType", false ); 
	//errFatal("SimulationObject::initializeLbmSimulation","Invalid solver type - note that mDimension is deprecated, use the 'solver' keyword instead", SIMWORLD_INITERROR); return 1;

	mpLbm = createSolver(); 
  /* check lbm pointer */
	if(mpLbm == NULL) {
		errFatal("SimulationObject::initializeLbmSimulation","Unable to init LBM solver! ", SIMWORLD_INITERROR);
		return 1;
	}
	debMsgStd("SimulationObject::initialized",DM_MSG,"IdStr:"<<mpLbm->getIdString() <<" LBM solver! ", 2);

	// for non-param simulations
	mpLbm->setParametrizer( mpParam );
	mpParam->setAttrList( getAttributeList() );
	// not needed.. done in solver_init: mpParam->setSize ... in solver_interface
	mpParam->parseAttrList();

	mpLbm->setAttrList( getAttributeList() );
	mpLbm->parseAttrList();
	mpParts = new ParticleTracer();
	mpParts->parseAttrList( getAttributeList() );

	if(!SIMWORLD_OK()) return 1;
	mpParts->setName( getName() + "_part" );
	mpParts->initialize( glob );
	if(!SIMWORLD_OK()) return 1;
	
	// init material settings
	string matMc("default");
	matMc = mpAttrs->readString("material_surf", matMc, "SimulationObject","matMc", false );
	mShowSurface   = mpAttrs->readInt("showsurface", mShowSurface, "SimulationObject","mShowSurface", false ); 
	mShowParticles = mpAttrs->readInt("showparticles", mShowParticles, "SimulationObject","mShowParticles", false ); 

	checkBoundingBox( mGeoStart, mGeoEnd, "SimulationObject::initializeSimulation" );
	mpLbm->setGeoStart( mGeoStart );
	mpLbm->setGeoEnd( mGeoEnd );
	mpLbm->setRenderGlobals( mpGlob );
	mpLbm->setName( getName() + "_lbm" );
	mpLbm->setParticleTracer( mpParts );
	if(mpElbeemSettings) {
		// set further settings from API struct init
		mpLbm->setSmoothing(1.0 * mpElbeemSettings->surfaceSmoothing, 1.0 * mpElbeemSettings->surfaceSmoothing);
		mpLbm->setSizeX(mpElbeemSettings->resolutionxyz);
		mpLbm->setSizeY(mpElbeemSettings->resolutionxyz);
		mpLbm->setSizeZ(mpElbeemSettings->resolutionxyz);
		mpLbm->setPreviewSize(mpElbeemSettings->previewresxyz);
		mpLbm->setRefinementDesired(mpElbeemSettings->maxRefine);
		mpLbm->setGenerateParticles(mpElbeemSettings->generateParticles);

		string dinitType = std::string("no");
		if     (mpElbeemSettings->obstacleType==FLUIDSIM_OBSTACLE_PARTSLIP) dinitType = std::string("part"); 
		else if(mpElbeemSettings->obstacleType==FLUIDSIM_OBSTACLE_FREESLIP) dinitType = std::string("free"); 
		else /*if(mpElbeemSettings->obstacleType==FLUIDSIM_OBSTACLE_NOSLIP)*/ dinitType = std::string("no"); 
		mpLbm->setDomainBound(dinitType);
		mpLbm->setDomainPartSlip(mpElbeemSettings->obstaclePartslip);
		mpLbm->setDumpVelocities(mpElbeemSettings->generateVertexVectors);
		mpLbm->setFarFieldSize(mpElbeemSettings->farFieldSize);
		debMsgStd("SimulationObject::initialize",DM_MSG,"Added domain bound: "<<dinitType<<" ps="<<mpElbeemSettings->obstaclePartslip<<" vv"<<mpElbeemSettings->generateVertexVectors<<","<<mpLbm->getDumpVelocities(), 9 );

		debMsgStd("SimulationObject::initialize",DM_MSG,"Set ElbeemSettings values "<<mpLbm->getGenerateParticles(),10);
	}
	mpLbm->initializeSolver();

	// print cell type stats
	const int jmax = sizeof(CellFlagType)*8;
	int totalCells = 0;
	int flagCount[jmax];
	for(int j=0; j<jmax ; j++) flagCount[j] = 0;
	int diffInits = 0;
	LbmSolverInterface::CellIdentifier cid = mpLbm->getFirstCell();
	for(; mpLbm->noEndCell( cid );
	      mpLbm->advanceCell( cid ) ) {
		int flag = mpLbm->getCellFlag(cid,0);
		int flag2 = mpLbm->getCellFlag(cid,1);
		if(flag != flag2) {
			diffInits++;
		}
		for(int j=0; j<jmax ; j++) {
			if( flag&(1<<j) ) flagCount[j]++;
		}
		totalCells++;
	}
	mpLbm->deleteCellIterator( &cid );

#if ELBEEM_PLUGIN!=1
	char charNl = '\n';
	debugOutNnl("SimulationObject::initializeLbmSimulation celltype stats: " <<charNl, 5);
	debugOutNnl("no. of cells = "<<totalCells<<", "<<charNl ,5);
	for(int j=0; j<jmax ; j++) {
		std::ostringstream out;
		if(flagCount[j]>0) {
			out<<"\t" << flagCount[j] <<" x "<< convertCellFlagType2String( (CellFlagType)(1<<j) ) <<", " << charNl;
			debugOutNnl(out.str(), 5);
		}
	}
	// compute dist. of empty/bnd - fluid - if
	// cfEmpty   = (1<<0), cfBnd  = (1<< 2), cfFluid   = (1<<10), cfInter   = (1<<11),
	{
		std::ostringstream out;
		out.precision(2); out.width(4);
		int totNum = flagCount[1]+flagCount[2]+flagCount[7]+flagCount[8];
		double ebFrac = (double)(flagCount[1]+flagCount[2]) / totNum;
		double flFrac = (double)(flagCount[7]) / totNum;
		double ifFrac = (double)(flagCount[8]) / totNum;
		//???
		out<<"\tFractions: [empty/bnd - fluid - interface - ext. if]  =  [" << ebFrac<<" - " << flFrac<<" - " << ifFrac<<"] "<< charNl;

		if(diffInits > 0) {
			debMsgStd("SimulationObject::initializeLbmSimulation",DM_MSG,"celltype Warning: Diffinits="<<diffInits<<" !!!!!!!!!" , 5);
		}
		debugOutNnl(out.str(), 5);
	}
#endif // ELBEEM_PLUGIN==1

	// might be modified by mpLbm
	mpParts->setStart( mGeoStart );
	mpParts->setEnd( mGeoEnd );
	mpParts->setCastShadows( false );
	mpParts->setReceiveShadows( false );
	mpParts->searchMaterial( glob->getMaterials() );

	// this has to be inited here - before, the values might be unknown
	ntlGeometryObject *surf = mpLbm->getSurfaceGeoObj();
	if(surf) {
		surf->setName( "final" ); // final surface mesh 
		// warning - this might cause overwriting effects for multiple sims and geom dump...
		surf->setCastShadows( true );
		surf->setReceiveShadows( false );
		surf->searchMaterial( glob->getMaterials() );
		if(mShowSurface) mObjects.push_back( surf );
	}
	
#ifdef ELBEEM_PLUGIN
	mShowParticles=1;
#endif // ELBEEM_PLUGIN
	//if(getenv("ELBEEM_DUMPPARTICLE")) {   // DEBUG ENABLE!!!!!!!!!!
	if(mpLbm->getGenerateParticles()>0.0) {
		mShowParticles=1;
		mpParts->setDumpParts(true);
	}
		//debMsgStd("SimulationObject::init",DM_NOTIFY,"Using envvar ELBEEM_DUMPPARTICLE to set mShowParticles, DEBUG!",1);
	//}  // DEBUG ENABLE!!!!!!!!!!
	if(mShowParticles) {
		mObjects.push_back(mpParts);
	}

	// add objects to display for debugging (e.g. levelset particles)
	vector<ntlGeometryObject *> debugObjs = mpLbm->getDebugObjects();
	for(size_t i=0;i<debugObjs.size(); i++) {
		debugObjs[i]->setCastShadows( false );
		debugObjs[i]->setReceiveShadows( false );
		debugObjs[i]->searchMaterial( glob->getMaterials() );
		mObjects.push_back( debugObjs[i] );
	}
	return 0;
}

/*! set current frame */
void SimulationObject::setFrameNum(int num) {
	// advance parametrizer
	mpParam->setFrameNum(num);
}

/******************************************************************************
 * simluation interface: advance simulation another step (whatever delta time that might be) 
 *****************************************************************************/
void SimulationObject::step( void )
{
	if(mpParam->getCurrentAniFrameTime()>0.0) {
		// dont advance for stopped time
		mpLbm->step();
		mTime += mpParam->getTimestep();
	}
	if(mpLbm->getPanic()) mPanic = true;

  //debMsgStd("SimulationObject::step",DM_MSG," Sim '"<<mName<<"' stepped to "<<mTime<<" (stept="<<(mpParam->getTimestep())<<", framet="<<getFrameTime()<<") ", 10);
}
/*! prepare visualization of simulation for e.g. raytracing */
void SimulationObject::prepareVisualization( void ) {
	if(mPanic) return;
	mpLbm->prepareVisualization();
}


/******************************************************************************/
/* get current start simulation time */
double SimulationObject::getStartTime( void ) {
	//return mpParam->calculateAniStart();
	return mpParam->getAniStart();
}
/* get time for a single animation frame */
double SimulationObject::getFrameTime( int frame ) {
	return mpParam->getAniFrameTime(frame);
}
/* get time for a single time step  */
double SimulationObject::getTimestep( void ) {
	return mpParam->getTimestep();
}


/******************************************************************************
 * return a pointer to the geometry object of this simulation 
 *****************************************************************************/
//ntlGeometryObject *SimulationObject::getGeometry() { return mpMC; }
vector<ntlGeometryObject *>::iterator 
SimulationObject::getObjectsBegin()
{
	return mObjects.begin();
}
vector<ntlGeometryObject *>::iterator 
SimulationObject::getObjectsEnd()
{
	return mObjects.end();
}





/******************************************************************************
 * GUI - display debug info 
 *****************************************************************************/

void SimulationObject::drawDebugDisplay() {
#ifndef NOGUI
	if(!getVisible()) return;

	//if( mDebugType > (MAX_DEBDISPSET-1) ){ errFatal("SimulationObject::drawDebugDisplay","Invalid debug type!", SIMWORLD_GENERICERROR); return; }
	//mDebDispSet[ mDebugType ].on = true;
	//errorOut( mDebugType <<"//"<< mDebDispSet[mDebugType].type );
	mpLbm->debugDisplay( mDebugType );

	//::lbmMarkedCellDisplay<>( mpLbm );
	mpLbm->lbmMarkedCellDisplay();
#endif
}

/* GUI - display interactive info  */
void SimulationObject::drawInteractiveDisplay()
{
#ifndef NOGUI
	if(!getVisible()) return;
	if(mSelectedCid) {
		// in debugDisplayNode if dispset is on is ignored...
		mpLbm->debugDisplayNode( FLUIDDISPGrid, mSelectedCid );
	}
#endif
}


/*******************************************************************************/
// GUI - handle mouse movement for selection 
/*******************************************************************************/
void SimulationObject::setMousePos(int x,int y, ntlVec3Gfx org, ntlVec3Gfx dir)
{
	normalize( dir );
	// assume 2D sim is in XY plane...
	
	double zplane = (mGeoEnd[2]-mGeoStart[2])*0.5;
	double zt = (zplane-org[2]) / dir[2];
	ntlVec3Gfx pos(
			org[0]+ dir[0] * zt,
			org[1]+ dir[1] * zt, 0.0);

	mSelectedCid = mpLbm->getCellAt( pos );
	//errMsg("SMP ", mName<< x<<" "<<y<<" - "<<dir );
	x = y = 0; // remove warning
}
			

void SimulationObject::setMouseClick()
{
	if(mSelectedCid) {
		//::debugPrintNodeInfo<>( mpLbm, mSelectedCid, mpLbm->getNodeInfoString() );
		mpLbm->debugPrintNodeInfo( mSelectedCid );
	}
}

/*! notify object that dump is in progress (e.g. for field dump) */
void SimulationObject::notifyShaderOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename) {
	if(!mpLbm) return;
	mpLbm->notifySolverOfDump(dumptype, frameNr,frameNrStr,outfilename);
}


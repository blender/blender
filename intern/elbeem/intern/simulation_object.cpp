/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Basic interface for all simulation modules
 *
 *****************************************************************************/

#include "simulation_object.h"
#include "ntl_bsptree.h"
#include "ntl_scene.h"
#include "factory_lbm.h"
#include "lbmfunctions.h"

#ifdef _WIN32
#else
#include <sys/time.h>
#endif




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
	mSolverType("-"), mStepsPerFrame( 10 ),
	mpLbm(NULL),
	mpParam( NULL ),
	mShowSurface(true), mShowParticles(false),
	mSelectedCid( NULL ),

	stnOld("opt"),
	stnFsgr("fsgr")
{
	mpParam = new Parametrizer();

	for(int i=0; i<MAX_DEBDISPSET; i++) {
		mDebDispSet[i].type  = (i);
		mDebDispSet[i].on    = false;
		mDebDispSet[i].scale = 1.0;
	}

	// reset time
	mTime 						= 0.0;
	mDisplayTime 			= 0.0;
}


/******************************************************************************
 * Destructor
 *****************************************************************************/
SimulationObject::~SimulationObject()
{
	if(mpGiTree != NULL) delete mpGiTree;
	delete mpLbm;
  delete mpParam;
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
	ntlScene *scene = mpGlob->getScene();
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




/******************************************************************************
 * simluation interface: initialize simulation using the given configuration file 
 *****************************************************************************/
int SimulationObject::initializeLbmSimulation(ntlRenderGlobals *glob)
{
	mpGlob = glob;
	if(!getVisible()) {
		mpAttrs->setAllUsed();
		return 0;
	}

	//mDimension is deprecated
	mSolverType = mpAttrs->readString("solver", mSolverType, "SimulationObject","mSolverType", false ); 
	if(mSolverType == stnFsgr) {
		mpLbm = createSolverLbmFsgr(); 
	} else if(mSolverType == stnOld) {
#ifdef LBM_INCLUDE_TESTSOLVERS
		// old solver for gfx demo
		mpLbm = createSolverOld();
#endif // LBM_TESTSOLVER
	} else {
		errFatal("SimulationObject::initializeLbmSimulation","Invalid solver type - note that mDimension is deprecated, use the 'solver' keyword instead", SIMWORLD_INITERROR);
		return 1;
	}


  /* check lbm pointer */
	if(mpLbm == NULL) {
		errFatal("SimulationObject::initializeLbmSimulation","Unable to init dim"<<mSolverType<<" LBM solver! ", SIMWORLD_INITERROR);
		return 1;
	}
	debugOut("SimulationObject::initialized "<< mpLbm->getIdString() <<" LBM solver! ", 2);

	// for non-param simulations
	mpLbm->setParametrizer( mpParam );
	mpParam->setAttrList( getAttributeList() );
	mpParam->setSize( mpLbm->getSizeX(), mpLbm->getSizeY(), mpLbm->getSizeZ() );
	mpParam->parseAttrList();

	mpLbm->setAttrList( getAttributeList() );
	mpLbm->parseAttrList();
	mParts.parseAttrList( getAttributeList() );
	mParts.setName( getName() + "_part" );
	mParts.initialize( glob );
	
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
	mpLbm->initialize( NULL, mpGlob->getScene()->getObjects() );

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

#if ELBEEM_BLENDER!=1
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
		int totNum = flagCount[0]+flagCount[2]+flagCount[10]+flagCount[11];
		double ebFrac = (double)(flagCount[0]+flagCount[2]) / totNum;
		double flFrac = (double)(flagCount[10]) / totNum;
		double ifFrac = (double)(flagCount[11]) / totNum;
		double eifFrac = (double)(flagCount[11]+flagCount[26]+flagCount[27]) / totNum;
		//???
		out<<"\tFractions: [empty/bnd - fluid - interface - ext. if]  =  [" << ebFrac<<" - " << flFrac<<" - " << ifFrac<<" - " << eifFrac <<"] "<< charNl;

		if(diffInits > 0) {
			debugOut("SimulationObject::initializeLbmSimulation celltype Warning: Diffinits="<<diffInits<<" !!!!!!!!!" , 5);
		}
		debugOutNnl(out.str(), 5);
	}
#endif // ELBEEM_BLENDER==1

	mpLbm->initParticles( &mParts );

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
	
	mParts.setStart( mGeoStart );
	mParts.setEnd( mGeoEnd );
	mParts.setCastShadows( false );
	mParts.setReceiveShadows( false );
	mParts.searchMaterial( glob->getMaterials() );
	if(mShowParticles) mObjects.push_back( &mParts );

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


/******************************************************************************
 * simluation interface: advance simulation another step (whatever delta time that might be) 
 *****************************************************************************/
void SimulationObject::step( void )
{
	mpLbm->step();

	mParts.savePreviousPositions();
	mpLbm->advanceParticles( &mParts );
	mTime += mpParam->getStepTime();
	if(mpLbm->getPanic()) mPanic = true;

  //debMsgStd("SimulationObject::step",DM_MSG," Sim '"<<mName<<"' stepped to "<<mTime<<" (stept="<<(mpParam->getStepTime())<<", framet="<<getFrameTime()<<") ", 10);
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
double SimulationObject::getFrameTime( void ) {
	return mpParam->getAniFrameTime();
}
/* get time for a single time step  */
double SimulationObject::getStepTime( void ) {
	return mpParam->getStepTime();
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
	//debugOut(" SD: "<<mDebugType<<" v"<<getVisible()<<" don"<< (mDebDispSet[mDebugType].on) , 10);
	if(!getVisible()) return;

	if( mDebugType > (MAX_DEBDISPSET-1) ){
		errFatal("SimulationObject::drawDebugDisplay","Invalid debug type!", SIMWORLD_GENERICERROR);
		return;
	}

	mDebDispSet[ mDebugType ].on = true;
	//errorOut( mDebugType <<"//"<< mDebDispSet[mDebugType].type );
	mpLbm->debugDisplay( &mDebDispSet[ mDebugType ] );

	::lbmMarkedCellDisplay<>( mpLbm );
#endif
}

/* GUI - display interactive info  */
void SimulationObject::drawInteractiveDisplay()
{
#ifndef NOGUI
	if(!getVisible()) return;
	if(mSelectedCid) {
		// in debugDisplayNode if dispset is on is ignored...
		::debugDisplayNode<>( &mDebDispSet[ FLUIDDISPGrid ], mpLbm, mSelectedCid );
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
		::debugPrintNodeInfo<>( mpLbm, mSelectedCid, mpLbm->getNodeInfoString() );
	}
}



/** \file elbeem/intern/ntl_world.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Main renderer class
 *
 *****************************************************************************/


#include <sys/stat.h>
#include <sstream>
#include "utilities.h"
#include "ntl_world.h"
#include "parametrizer.h"

// for non-threaded renderViz
#ifndef NOGUI
#include "../gui/ntl_openglrenderer.h"
#include "../gui/guifuncs.h"
#include "../gui/frame.h"
#endif


/* external parser functions from cfgparser.cxx */
#ifndef ELBEEM_PLUGIN
/* parse given file as config file */
void parseFile(string filename);
/* set pointers for parsing */
void setPointers( ntlRenderGlobals *setglob);
#endif // ELBEEM_PLUGIN


/******************************************************************************
 * Constructor
 *****************************************************************************/

ntlWorld::ntlWorld() {
	initDefaults();
}

ntlWorld::ntlWorld(string filename, bool commandlineMode) 
{
#ifndef ELBEEM_PLUGIN

		initDefaults();
#	ifdef NOGUI
		commandlineMode = true; // remove warning...
#	endif // NOGUI

		// load config
		setPointers( getRenderGlobals() );
		parseFile( filename.c_str() );
#	ifndef NOGUI
		// setup opengl display, save first animation step for start time 
		// init after parsing file...
		if(!commandlineMode) {
			mpOpenGLRenderer = new ntlOpenGLRenderer( mpGlob );
		}
#	endif // NOGUI
		finishWorldInit();

#else // ELBEEM_PLUGIN
	errFatal("ntlWorld::init","Cfg file parsing not supported for API version! "<<filename<<" "<<commandlineMode, SIMWORLD_INITERROR);
#endif // ELBEEM_PLUGIN
}


int globalDomainCounter = 1;
int ntlWorld::addDomain(elbeemSimulationSettings *settings)
{
	// create domain obj
	SimulationObject *sim = new SimulationObject();
	char simname[100];
	snprintf(simname,100,"domain%04d",globalDomainCounter);
	globalDomainCounter++;
	sim->setName(string(simname));
	mpGlob->getSims()->push_back( sim );

	// important - add to both, only render scene objects are free'd 
	mpGlob->getRenderScene()->addGeoClass( sim );
	mpGlob->getSimScene()->addGeoClass( sim );
	sim->setGeoStart(ntlVec3Gfx(settings->geoStart[0],settings->geoStart[1],settings->geoStart[2]));
	sim->setGeoEnd(ntlVec3Gfx(
			settings->geoStart[0]+settings->geoSize[0],
			settings->geoStart[1]+settings->geoSize[1],
			settings->geoStart[2]+settings->geoSize[2] ));
	// further init in postGeoConstrInit/initializeLbmSimulation of SimulationObject
	sim->copyElbeemSettings(settings);

	Parametrizer *param = sim->getParametrizer();
	param->setSize( settings->resolutionxyz );
	param->setDomainSize( settings->realsize );
	param->setAniStart( settings->animStart );
	param->setNormalizedGStar( settings->gstar );

	// init domain channels
	vector<ParamFloat> valf; 
	vector<ParamVec> valv; 
	vector<double> time;

#define INIT_CHANNEL_FLOAT(channel,size) \
	valf.clear(); time.clear(); elbeemSimplifyChannelFloat(channel,&size); \
	for(int i=0; i<size; i++) { valf.push_back( channel[2*i+0] ); time.push_back( channel[2*i+1] ); } 
#define INIT_CHANNEL_VEC(channel,size) \
	valv.clear(); time.clear(); elbeemSimplifyChannelVec3(channel,&size); \
	for(int i=0; i<size; i++) { valv.push_back( ParamVec(channel[4*i+0],channel[4*i+1],channel[4*i+2]) ); time.push_back( channel[4*i+3] ); } 

	param->setViscosity( settings->viscosity );
	if((settings->channelViscosity)&&(settings->channelSizeViscosity>0)) {
		INIT_CHANNEL_FLOAT(settings->channelViscosity, settings->channelSizeViscosity);
		param->initViscosityChannel(valf,time); }

	param->setGravity( ParamVec(settings->gravity[0], settings->gravity[1], settings->gravity[2]) );
	if((settings->channelGravity)&&(settings->channelSizeGravity>0)) {
		INIT_CHANNEL_VEC(settings->channelGravity, settings->channelSizeGravity);
		param->initGravityChannel(valv,time); }

	param->setAniFrameTimeChannel( settings->aniFrameTime );
	if((settings->channelFrameTime)&&(settings->channelSizeFrameTime>0)) {
		INIT_CHANNEL_FLOAT(settings->channelFrameTime, settings->channelSizeFrameTime);
		param->initAniFrameTimeChannel(valf,time); }

#undef INIT_CHANNEL_FLOAT
#undef INIT_CHANNEL_VEC
	
	// might be set by previous domain
	if(mpGlob->getAniFrames() < settings->noOfFrames)	mpGlob->setAniFrames( settings->noOfFrames );
	// set additionally to SimulationObject->mOutFilename
	mpGlob->setOutFilename( settings->outputPath );

	return 0;
}

void ntlWorld::initDefaults()
{
	mStopRenderVisualization = false;
	mThreadRunning =  false;
	mSimulationTime = 0.0; 
	mFirstSim = 1;
	mSingleStepDebug =  false;
	mFrameCnt = 0;
	mSimFrameCnt = 0;
	mpOpenGLRenderer = NULL;

  /* create scene storage */
  mpGlob = new ntlRenderGlobals();
  mpLightList = new vector<ntlLightObject*>;
  mpPropList = new vector<ntlMaterial*>;
  mpSims = new vector<SimulationObject*>;

  mpGlob->setLightList(mpLightList);
  mpGlob->setMaterials(mpPropList);
  mpGlob->setSims(mpSims);

	/* init default material */
  ntlMaterial *def = GET_GLOBAL_DEFAULT_MATERIAL;
 	mpPropList->push_back( def );

	/* init the scene object */
 	ntlScene *renderscene = new ntlScene( mpGlob, true );
	mpGlob->setRenderScene( renderscene );
	// sim scene shouldnt delete objs, may only contain subset
 	ntlScene *simscene = new ntlScene( mpGlob, false );
	mpGlob->setSimScene( simscene );
}

void ntlWorld::finishWorldInit()
{
	if(! isSimworldOk() ) return;

	// init the scene for the first time
  long sstartTime = getTime();

	// first init sim scene for geo setup
	mpGlob->getSimScene()->buildScene(0.0, true);
	if(! isSimworldOk() ) return;
	mpGlob->getRenderScene()->buildScene(0.0, true);
	if(! isSimworldOk() ) return;
	long sstopTime = getTime();
	debMsgStd("ntlWorld::ntlWorld",DM_MSG,"Scene build time: "<< getTimeString(sstopTime-sstartTime) <<" ", 10);

	// TODO check simulations, run first steps
	mFirstSim = -1;
	if(mpSims->size() > 0) {

		// use values from first simulation as master time scale
		long startTime = getTime();
		
		// remember first active sim
		for(size_t i=0;i<mpSims->size();i++) {
			if(!(*mpSims)[i]->getVisible()) continue;
			if((*mpSims)[i]->getPanic())    continue;

			// check largest timestep
			if(mFirstSim>=0) {
				if( (*mpSims)[i]->getTimestep() > (*mpSims)[mFirstSim]->getTimestep() ) {
					mFirstSim = i;
					debMsgStd("ntlWorld::ntlWorld",DM_MSG,"First Sim changed: "<<i ,10);
				}
			}
			// check any valid sim
			if(mFirstSim<0) {
				mFirstSim = i;
				debMsgStd("ntlWorld::ntlWorld",DM_MSG,"First Sim: "<<i ,10);
			}
		}

		if(mFirstSim>=0) {
			debMsgStd("ntlWorld::ntlWorld",DM_MSG,"Anistart Time: "<<(*mpSims)[mFirstSim]->getStartTime() ,10);
			while(mSimulationTime < (*mpSims)[mFirstSim]->getStartTime() ) {
			debMsgStd("ntlWorld::ntlWorld",DM_MSG,"Anistart Time: "<<(*mpSims)[mFirstSim]->getStartTime()<<" simtime:"<<mSimulationTime ,10);
				advanceSims(-1);
			}
			long stopTime = getTime();

			debMsgStd("ntlWorld::ntlWorld",DM_MSG,"Time for start-sims:"<< getTimeString(stopTime-startTime) , 1);
#ifndef NOGUI
			guiResetSimulationTimeRange( mSimulationTime );
#endif
		} else {
			if(!mpGlob->getSingleFrameMode()) debMsgStd("ntlWorld::ntlWorld",DM_WARNING,"No active simulations!", 1);
		}
	}

	if(! isSimworldOk() ) return;
	setElbeemState( SIMWORLD_INITED );
}



/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlWorld::~ntlWorld()
{
	delete mpGlob->getRenderScene();
	delete mpGlob->getSimScene();
  
	delete mpGlob;
	
	
	// these get assigned to mpGlob but not freed there
	delete mpLightList;
	delete mpPropList; // materials
	delete mpSims;
  
#ifndef NOGUI
	if(mpOpenGLRenderer) delete mpOpenGLRenderer;
#endif // NOGUI
	debMsgStd("ntlWorld",DM_NOTIFY, "ntlWorld done", 10);
}

/******************************************************************************/
/*! set single frame rendering to filename */
void ntlWorld::setSingleFrameOut(string singleframeFilename) {
	mpGlob->setSingleFrameMode(true);
	mpGlob->setSingleFrameFilename(singleframeFilename);
}

/******************************************************************************
 * render a whole animation (command line mode) 
 *****************************************************************************/

int ntlWorld::renderAnimation( void )
{
	// only single pic currently
	//debMsgStd("ntlWorld::renderAnimation : Warning only simulating...",1);
 	if(mpGlob->getAniFrames() < 0) {
		debMsgStd("ntlWorld::renderAnimation",DM_NOTIFY,"No frames to render... ",1);
		return 1;
	}

	if(mFirstSim<0) {
		debMsgStd("ntlWorld::renderAnimation",DM_NOTIFY,"No reference animation found...",1);
		return 1;
	} 

	mThreadRunning = true; // not threaded, but still use the same flags
	if(getElbeemState() == SIMWORLD_INITED) {
		renderScene();
	} else if(getElbeemState() == SIMWORLD_STOP) {
		// dont render now, just continue
		setElbeemState( SIMWORLD_INITED );
		mFrameCnt--; // counted one too many from last abort...
	} else {
		debMsgStd("ntlWorld::renderAnimation",DM_NOTIFY,"Not properly inited, stopping...",1);
		return 1;
	}
	
	if(mpSims->size() <= 0) {
		debMsgStd("ntlWorld::renderAnimation",DM_NOTIFY,"No simulations found, stopping...",1);
		return 1;
	}

	bool simok = true;
	for( ; ((mFrameCnt<mpGlob->getAniFrames()) && (!getStopRenderVisualization() ) && (simok)); mFrameCnt++) {
		if(!advanceSims(mFrameCnt)) {
			renderScene();
		} // else means sim panicked, so dont render...
		else { simok=false; }
	}
	mThreadRunning = false;
	return 0;
}

/******************************************************************************
 * render a whole animation (visualization mode) 
 * this function is run in another thread, and communicates 
 * with the parent thread via a mutex 
 *****************************************************************************/
int ntlWorld::renderVisualization( bool multiThreaded ) 
{
#ifndef NOGUI
	if(getElbeemState() != SIMWORLD_INITED) { return 0; }

	if(multiThreaded) mThreadRunning = true;
	// TODO, check global state?
	while(!getStopRenderVisualization()) {

		if(mpSims->size() <= 0) {
			debMsgStd("ntlWorld::renderVisualization",DM_NOTIFY,"No simulations found, stopping...",1);
			stopSimulationThread();
			break;
		}

		// determine stepsize
		if(!mSingleStepDebug) {
			long startTime = getTime();
			advanceSims(mFrameCnt);
			mFrameCnt++;
			long stopTime = getTime();
			debMsgStd("ntlWorld::renderVisualization",DM_MSG,"Time for t="<<mSimulationTime<<": "<< getTimeString(stopTime-startTime) <<" ", 10);
		} else {
			double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getTimestep();
			singleStepSims(targetTime);

			// check paniced sims (normally done by advanceSims
			bool allPanic = true;
			for(size_t i=0;i<mpSims->size();i++) {
				if(!(*mpSims)[i]->getPanic()) allPanic = false;
			}
			if(allPanic) {
				warnMsg("ntlWorld::advanceSims","All sims panicked... stopping thread" );
				setStopRenderVisualization( true );
			}
			if(! isSimworldOk() ) {
				warnMsg("ntlWorld::advanceSims","World state error... stopping" );
				setStopRenderVisualization( true );
			}
		}

		// save frame
		if(mpOpenGLRenderer) mpOpenGLRenderer->saveAnimationFrame( mSimulationTime );
		
		// for non-threaded check events
		if(!multiThreaded) {
			Fl::check();
      gpElbeemFrame->SceneDisplay->doOnlyForcedRedraw();
		}

	}
	mThreadRunning = false;
	stopSimulationRestoreGui();
#else 
	multiThreaded = false; // remove warning
#endif
	return 0;
}
/*! render a single step for viz mode */
int ntlWorld::singleStepVisualization( void ) 
{
	mThreadRunning = true;
	double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getTimestep();
	singleStepSims(targetTime);
	mSimulationTime = (*mpSims)[0]->getCurrentTime();

#ifndef NOGUI
	if(mpOpenGLRenderer) mpOpenGLRenderer->saveAnimationFrame( mSimulationTime );
	Fl::check();
  gpElbeemFrame->SceneDisplay->doOnlyForcedRedraw();
	mThreadRunning = false;
	stopSimulationRestoreGui();
#else
	mThreadRunning = false;
#endif // NOGUI
	return 0;
}

// dont use LBM_EPSILON here, time is always double-precision!
#define LBM_TIME_EPSILON 1e-10

/******************************************************************************
 * advance simulations by time t 
 *****************************************************************************/
int ntlWorld::advanceSims(int framenum)
{
	bool done = false;
	bool allPanic = true;

	// stop/quit (abort), dont display/render
	if(!isSimworldOk()) { 
		return 1;
	}

	for(size_t i=0;i<mpSims->size();i++) { (*mpSims)[i]->setFrameNum(framenum); }

	// time stopped? nothing else to do...
	if( (*mpSims)[mFirstSim]->getFrameTime(framenum) <= 0.0 ){ 
		done=true; allPanic=false; 

		/* DG: Need to check for user cancel here (fix for [#30298]) */
		(*mpSims)[mFirstSim]->checkCallerStatus(FLUIDSIM_CBSTATUS_STEP, 0);
	}

	// Prevent bug [#29186] Object contribute to fluid sim animation start earlier than keyframe
	// Was: double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getFrameTime(framenum); - DG
	double totalTime = 0.0, targetTime = 0.0;
	for(size_t i = 0; i < mSimFrameCnt; i++)
	{
		/* We need an intermediate array "mSimFrameValue" because
		otherwise if we don't start with starttime = 0, 
		the sim gets out of sync - DG */
		totalTime += (*mpSims)[mFirstSim]->getFrameTime(mSimFrameValue[i]);	
	}
	targetTime = totalTime + (*mpSims)[mFirstSim]->getFrameTime(framenum);

	int gstate = 0;
	myTime_t advsstart = getTime();

	// step all the sims, and check for panic
	debMsgStd("ntlWorld::advanceSims",DM_MSG, " sims "<<mpSims->size()<<" t"<<targetTime<<" done:"<<done<<" panic:"<<allPanic<<" gstate:"<<gstate, 10); // debug // timedebug
	while(!done) {
		double nextTargetTime = (*mpSims)[mFirstSim]->getCurrentTime() + (*mpSims)[mFirstSim]->getTimestep();
		singleStepSims(nextTargetTime);

		// check target times
		done = true;
		allPanic = false;
		
		if((*mpSims)[mFirstSim]->getTimestep() <1e-9 ) { 
			// safety check, avoid timesteps that are too small
			errMsg("ntlWorld::advanceSims","Invalid time step, causing panic! curr:"<<(*mpSims)[mFirstSim]->getCurrentTime()<<" next:"<<nextTargetTime<<", stept:"<< (*mpSims)[mFirstSim]->getTimestep() );
			allPanic = true; 
		} else {
			for(size_t i=0;i<mpSims->size();i++) {
				if(!(*mpSims)[i]->getVisible()) continue;
				if((*mpSims)[i]->getPanic()) allPanic = true; // do any panic now!?
				debMsgStd("ntlWorld::advanceSims",DM_MSG, "Sim "<<i<<", currt:"<<(*mpSims)[i]->getCurrentTime()<<", nt:"<<nextTargetTime<<", panic:"<<(*mpSims)[i]->getPanic()<<", targett:"<<targetTime, 10); // debug // timedebug
			} 
		}
		if( (targetTime - (*mpSims)[mFirstSim]->getCurrentTime()) > LBM_TIME_EPSILON) done=false;
		if(allPanic) done = true;
	}

	if(allPanic) {
		warnMsg("ntlWorld::advanceSims","All sims panicked... stopping thread" );
		setStopRenderVisualization( true );
		return 1;
	}

	myTime_t advsend = getTime();
	debMsgStd("ntlWorld::advanceSims",DM_MSG,"Overall steps so far took:"<< getTimeString(advsend-advsstart)<<" for sim time "<<targetTime, 4);

	// finish step
	for(size_t i=0;i<mpSims->size();i++) {
		SimulationObject *sim = (*mpSims)[i];
		if(!sim->getVisible()) continue;
		if(sim->getPanic()) continue;
		sim->prepareVisualization();
	}

	mSimFrameValue.push_back(framenum);
	mSimFrameCnt++;

	return 0;
}

/* advance simulations by a single step */
/* dont check target time, if *targetTime==NULL */
void ntlWorld::singleStepSims(double targetTime) {
	const bool debugTime = false;
	//double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getTimestep();
	if(debugTime) errMsg("ntlWorld::singleStepSims","Target time: "<<targetTime);

	for(size_t i=0;i<mpSims->size();i++) {
		SimulationObject *sim = (*mpSims)[i];
		if(!sim->getVisible()) continue;
		if(sim->getPanic()) continue;
		bool done = false;
		while(!done) {
			// try to prevent round off errs
			if(debugTime) errMsg("ntlWorld::singleStepSims","Test sim "<<i<<" curt:"<< sim->getCurrentTime()<<" target:"<<targetTime<<" delta:"<<(targetTime - sim->getCurrentTime())<<" stept:"<<sim->getTimestep()<<" leps:"<<LBM_TIME_EPSILON ); // timedebug
			if( (targetTime - sim->getCurrentTime()) > LBM_TIME_EPSILON) {
				if(debugTime) errMsg("ntlWorld::singleStepSims","Stepping sim "<<i<<" t:"<< sim->getCurrentTime()); // timedebug
				sim->step();
			} else {
				done = true;
			}
		}
	}

	mSimulationTime = (*mpSims)[mFirstSim]->getCurrentTime();
#ifndef NOGUI
	if(mpOpenGLRenderer) mpOpenGLRenderer->notifyOfNextStep(mSimulationTime);
#endif // NOGUI
}



/******************************************************************************
 * Render the current scene
 * uses the global variables from the parser
 *****************************************************************************/
int ntlWorld::renderScene( void )
{
#ifndef ELBEEM_PLUGIN
	char nrStr[5];														// nr conversion 
	std::ostringstream outfn_conv("");  			// converted ppm with other suffix 
  ntlRenderGlobals *glob;                  	// storage for global rendering parameters 
  myTime_t timeStart,totalStart,timeEnd; 		// measure user running time 
  myTime_t rendStart,rendEnd;            		// measure user rendering time 
  glob = mpGlob;

	// deactivate for all with index!=0 
	if((glob_mpactive)&&(glob_mpindex>0)) return(0);

	/* check if picture already exists... */
	if(!glob->getSingleFrameMode() ) {
		snprintf(nrStr, 5, "%04d", glob->getAniCount() );

		if(glob_mpactive) {
			outfn_conv  << glob->getOutFilename() <<"_"<<glob_mpindex<<"_" << nrStr << ".png"; /// DEBUG!
		} else {
			// ORG
			outfn_conv  << glob->getOutFilename() <<"_" << nrStr << ".png";
		}
		
		//if((mpGlob->getDisplayMode() == DM_RAY)&&(mpGlob->getFrameSkip())) {
		if(mpGlob->getFrameSkip()) {
			struct stat statBuf;
			if(stat(outfn_conv.str().c_str(),&statBuf) == 0) {
				errorOut("ntlWorld::renderscene Warning: file "<<outfn_conv.str()<<" already exists - skipping frame..."); 
				glob->setAniCount( glob->getAniCount() +1 );
				return(2);
			}
		} // RAY mode
	} else {
		// single frame rendering, overwrite if necessary...
		outfn_conv << glob->getSingleFrameFilename();
	}

  /* start program */
	timeStart = getTime();

	/* build scene geometry, calls buildScene(t,false) */
	glob->getRenderScene()->prepareScene(mSimulationTime);

  /* start program */
	totalStart = getTime();


	/* view parameters are currently not animated */
	/* calculate rays through projection plane */
	ntlVec3Gfx direction = glob->getLookat() - glob->getEye();
	/* calculate width of screen using perpendicular triangle diven by
	 * viewing direction and screen plane */
	gfxReal screenWidth = norm(direction)*tan( (glob->getFovy()*0.5/180.0)*M_PI );

	/* calculate vector orthogonal to up and viewing direction */
	ntlVec3Gfx upVec = glob->getUpVec();
	ntlVec3Gfx rightVec( cross(upVec,direction) );
	normalize(rightVec);

	/* calculate screen plane up vector, perpendicular to viewdir and right vec */
	upVec = ntlVec3Gfx( cross(rightVec,direction) );
	normalize(upVec);

	/* check if vectors are valid */
	if( (equal(upVec,ntlVec3Gfx(0.0))) || (equal(rightVec,ntlVec3Gfx(0.0))) ) {
		errMsg("ntlWorld::renderScene","Invalid viewpoint vectors! up="<<upVec<<" right="<<rightVec);
		return(1);
	}

	/* length from center to border of screen plane */
	rightVec *= (screenWidth*glob->getAspect() * -1.0);
	upVec *= (screenWidth * -1.0);

	/* screen traversal variables */
	ntlVec3Gfx screenPos;                          /* current position on virtual screen */
	int Xres = glob->getResX();                  /* X resolution */
	int Yres = glob->getResY();                  /* Y resolution */
	ntlVec3Gfx rightStep = (rightVec/(Xres/2.0));  /* one step right for a pixel */
	ntlVec3Gfx upStep    = (upVec/(Yres/2.0));     /* one step up for a pixel */
    

	/* anti alias init */
	char  showAAPic = 0;
	int   aaDepth = glob->getAADepth();
	int   aaLength;
	if(aaDepth>=0) aaLength = (2<<aaDepth);
	else           aaLength = 0;
	float aaSensRed   = 0.1;
	float aaSensGreen = 0.1;
	float aaSensBlue  = 0.1;
	int   aaArrayX = aaLength*Xres+1;
	int   aaArrayY = ( aaLength+1 );
	ntlColor *aaCol = new ntlColor[ aaArrayX*aaArrayY ];
	char  *aaUse = new char[ aaArrayX*aaArrayY ];

	/* picture storage */
	int picX = Xres;
	int picY = Yres;
	if(showAAPic) {
		picX = Xres *aaLength+1;
		picY = Yres *aaLength+1;
	}
	ntlColor *finalPic = new ntlColor[picX * picY];


	/* reset picture vars */
	for(int j=0;j<aaArrayY;j++) {
		for(int i=0;i<aaArrayX;i++) {
			aaCol[j*aaArrayX+i] = ntlColor(0.0, 0.0, 0.0);
			aaUse[j*aaArrayX+i] = 0;
		}
	}
	for(int j=0;j<picY;j++) {
		for(int i=0;i<picX;i++) {
			finalPic[j*picX+i] = ntlColor(0.0, 0.0, 0.0);
		}
	}

	/* loop over all y lines in screen, from bottom to top because
	 * ppm format wants 0,0 top left */
	rendStart = getTime();
	glob->setCounterShades(0);
	glob->setCounterSceneInter(0);
	for (int scanline=Yres ; scanline > 0 ; --scanline) {
    
		debugOutInter( "ntlWorld::renderScene: Line "<<scanline<<
								 " ("<< ((Yres-scanline)*100/Yres) <<"%) ", 2, 2000 );
		screenPos = glob->getLookat() + upVec*((2.0*scanline-Yres)/Yres)
			- rightVec;

		/* loop over all pixels in line */
		for (int sx=0 ; sx < Xres ; ++sx) {

			if((sx==glob->getDebugPixelX())&&(scanline==(Yres-glob->getDebugPixelY()) )) {
				// DEBUG!!!
				glob->setDebugOut(10);
			} else glob->setDebugOut(0);
			
			/* compute ray from eye through current pixel into scene... */
			ntlColor col;
			if(aaDepth<0) {
				ntlVec3Gfx dir(screenPos - glob->getEye());
				ntlRay the_ray(glob->getEye(), getNormalized(dir), 0, 1.0, glob );

				/* ...and trace it */
				col = the_ray.shade();
			} else {
				/* anti alias */
				int ai,aj;                   /* position in grid */
				int aOrg = sx*aaLength;      /* grid offset x */
				int currStep = aaLength;     /* step size */
				char colDiff = 1;            /* do colors still differ too much? */
				ntlColor minCol,maxCol;         /* minimum and maximum Color Values */
				minCol = ntlColor(1.0,1.0,1.0);
				maxCol = ntlColor(0.0,0.0,0.0);

				while((colDiff) && (currStep>0)) {
					colDiff = 0;
	    
					for(aj = 0;aj<=aaLength;aj+= currStep) {
						for(ai = 0;ai<=aaLength;ai+= currStep) {

							/* shade pixel if not done */
							if(aaUse[aj*aaArrayX +ai +aOrg] == 0) {
								aaUse[aj*aaArrayX +ai +aOrg] = 1;
								ntlVec3Gfx aaPos( screenPos +
																(rightStep * (ai- aaLength/2)/(gfxReal)aaLength ) +
																(upStep    * (aj- aaLength/2)/(gfxReal)aaLength ) );

								ntlVec3Gfx dir(aaPos - glob->getEye());
								ntlRay the_ray(glob->getEye(), getNormalized(dir), 0, 1.0, glob );

								/* ...and trace it */
								ntlColor newCol= the_ray.shade();
								aaCol[aj*aaArrayX +ai +aOrg]= newCol;
							} /* not used? */

						}
					}

					/* check color differences */
					for(aj = 0;aj<aaLength;aj+= currStep) {
						for(ai = 0;ai<aaLength;ai+= currStep) {

							char thisColDiff = 0;
							if( 
								 (fabs(aaCol[aj*aaArrayX +ai +aOrg][0] - 
											 aaCol[(aj+0)*aaArrayX +(ai+currStep) +aOrg][0])> aaSensRed ) ||
								 (fabs(aaCol[aj*aaArrayX +ai +aOrg][1] - 
											 aaCol[(aj+0)*aaArrayX +(ai+currStep) +aOrg][1])> aaSensGreen ) ||
								 (fabs(aaCol[aj*aaArrayX +ai +aOrg][2] - 
											 aaCol[(aj+0)*aaArrayX +(ai+currStep) +aOrg][2])> aaSensBlue ) ) {
								thisColDiff = 1;
							} else
								if( 
									 (fabs(aaCol[aj*aaArrayX +ai +aOrg][0] - 
												 aaCol[(aj+currStep)*aaArrayX +(ai+0) +aOrg][0])> aaSensRed ) ||
									 (fabs(aaCol[aj*aaArrayX +ai +aOrg][1] - 
												 aaCol[(aj+currStep)*aaArrayX +(ai+0) +aOrg][1])> aaSensGreen ) ||
									 (fabs(aaCol[aj*aaArrayX +ai +aOrg][2] - 
												 aaCol[(aj+currStep)*aaArrayX +(ai+0) +aOrg][2])> aaSensBlue ) ) {
									thisColDiff = 1;
								} else
									if( 
										 (fabs(aaCol[aj*aaArrayX +ai +aOrg][0] - 
													 aaCol[(aj+currStep)*aaArrayX +(ai+currStep) +aOrg][0])> aaSensRed ) ||
										 (fabs(aaCol[aj*aaArrayX +ai +aOrg][1] - 
													 aaCol[(aj+currStep)*aaArrayX +(ai+currStep) +aOrg][1])> aaSensGreen ) ||
										 (fabs(aaCol[aj*aaArrayX +ai +aOrg][2] - 
													 aaCol[(aj+currStep)*aaArrayX +(ai+currStep) +aOrg][2])> aaSensBlue ) ) {
										thisColDiff = 1;
									} 

							//colDiff =1;
							if(thisColDiff) {
								/* set diff flag */
								colDiff = thisColDiff;
								for(int bj=aj;bj<=aj+currStep;bj++) {
									for(int bi=ai;bi<=ai+currStep;bi++) {
										if(aaUse[bj*aaArrayX +bi +aOrg]==2) {
											//if(showAAPic) 
											aaUse[bj*aaArrayX +bi +aOrg] = 0;
										}
									}
								}
							} else {
								/* set all values */
								ntlColor avgCol = (
																	 aaCol[(aj+0       )*aaArrayX +(ai+0       ) +aOrg] +
																	 aaCol[(aj+0       )*aaArrayX +(ai+currStep) +aOrg] +
																	 aaCol[(aj+currStep)*aaArrayX +(ai+0       ) +aOrg] +
																	 aaCol[(aj+currStep)*aaArrayX +(ai+currStep) +aOrg] ) *0.25;
								for(int bj=aj;bj<=aj+currStep;bj++) {
									for(int bi=ai;bi<=ai+currStep;bi++) {
										if(aaUse[bj*aaArrayX +bi +aOrg]==0) {
											aaCol[bj*aaArrayX +bi +aOrg] = avgCol; 
											aaUse[bj*aaArrayX +bi +aOrg] = 2;
										}
									}
								}
							} /* smaller values set */

						}
					}

					/* half step size */
					currStep /= 2;

				} /* repeat until diff not too big */

				/* get average color */
				gfxReal colNum = 0.0;
				col = ntlColor(0.0, 0.0, 0.0);
				for(aj = 0;aj<=aaLength;aj++) {
					for(ai = 0;ai<=aaLength;ai++) {
						col += aaCol[aj*aaArrayX +ai +aOrg];
						colNum += 1.0;
					}
				}
				col /= colNum;

			}

		  /* mark pixels with debugging */
			if( glob->getDebugOut() > 0) col = ntlColor(0,1,0);

			/* store pixel */
			if(!showAAPic) {
				finalPic[(scanline-1)*picX+sx] = col; 
			}
			screenPos +=  rightStep;

		} /* foreach x */

		/* init aa array */
		if(showAAPic) {
			for(int j=0;j<=aaArrayY-1;j++) {
				for(int i=0;i<=aaArrayX-1;i++) {
					if(aaUse[j*aaArrayX +i]==1) finalPic[((scanline-1)*aaLength +j)*picX+i][0] = 1.0;
				}
			}
		}

		for(int i=0;i<aaArrayX;i++) {
			aaCol[(aaArrayY-1)*aaArrayX+i] = aaCol[0*aaArrayX+i];
			aaUse[(aaArrayY-1)*aaArrayX+i] = aaUse[0*aaArrayX+i];
		}
		for(int j=0;j<aaArrayY-1;j++) {
			for(int i=0;i<aaArrayX;i++) {
				aaCol[j*aaArrayX+i] = ntlColor(0.0, 0.0, 0.0);
				aaUse[j*aaArrayX+i] = 0;
			}
		}

	} /* foreach y */
	rendEnd = getTime();


	/* write png file */
	{
		int w = picX;
		int h = picY;

		unsigned rowbytes = w*4;
		unsigned char *screenbuf, **rows;
		screenbuf = (unsigned char*)malloc( h*rowbytes );
		rows = (unsigned char**)malloc( h*sizeof(unsigned char*) );
		unsigned char *filler = screenbuf;

		// cutoff color values 0..1
		for(int j=0;j<h;j++) {
			for(int i=0;i<w;i++) {
				ntlColor col = finalPic[j*w+i];
				for (unsigned int cc=0; cc<3; cc++) {
					if(col[cc] <= 0.0) col[cc] = 0.0;
					if(col[cc] >= 1.0) col[cc] = 1.0;
				}
				*filler = (unsigned char)( col[0]*255.0 ); 
				filler++;
				*filler = (unsigned char)( col[1]*255.0 ); 
				filler++;
				*filler = (unsigned char)( col[2]*255.0 ); 
				filler++;
				*filler = (unsigned char)( 255.0 ); 
				filler++; // alpha channel
			}
		}

		for(int i = 0; i < h; i++) rows[i] = &screenbuf[ (h - i - 1)*rowbytes ];
		writePng(outfn_conv.str().c_str(), rows, w, h);
	}


	// next frame 
	glob->setAniCount( glob->getAniCount() +1 );

	// done 
	timeEnd = getTime();

	char resout[1024];
	snprintf(resout,1024, "NTL Done %s, frame %d/%d (took %s scene, %s raytracing, %s total, %d shades, %d i.s.'s)!\n", 
				 outfn_conv.str().c_str(), (glob->getAniCount()), (glob->getAniFrames()+1),
				 getTimeString(totalStart-timeStart).c_str(), getTimeString(rendEnd-rendStart).c_str(), getTimeString(timeEnd-timeStart).c_str(),
				 glob->getCounterShades(),
				 glob->getCounterSceneInter() );
	debMsgStd("ntlWorld::renderScene",DM_MSG, resout, 1 );

	/* clean stuff up */
	delete [] aaCol;
	delete [] aaUse;
	delete [] finalPic;
	glob->getRenderScene()->cleanupScene();

	if(mpGlob->getSingleFrameMode() ) {
		debMsgStd("ntlWorld::renderScene",DM_NOTIFY, "Single frame mode done...", 1 );
		return 1;
	}
#endif // ELBEEM_PLUGIN
	return 0;
}


/******************************************************************************
 * renderglobals
 *****************************************************************************/


/*****************************************************************************/
/* Constructor with standard value init */
ntlRenderGlobals::ntlRenderGlobals() :
	mpRenderScene(NULL), mpSimScene(NULL),
  mpLightList( NULL ), mpMaterials( NULL ), mpSims( NULL ),
  mResX(320), mResY(200), mAADepth(-1), mMaxColVal(255), 
  mRayMaxDepth( 5 ),
  mvEye(0.0,0.0,5.0), mvLookat(0.0,0.0,0.0), mvUpvec(0.0,1.0,0.0), 
  mAspect(320.0/200.0), 
  mFovy(45), mcBackgr(0.0,0.0,0.0), mcAmbientLight(0.0,0.0,0.0), 
  mDebugOut( 0 ),
  mAniStart(0), mAniFrames( -1 ), mAniCount( 0 ),
	mFrameSkip( 0 ),
  mCounterRays( 0 ), mCounterShades( 0 ), mCounterSceneInter( 0 ),
	mOutFilename( "pic" ),
	mTreeMaxDepth( 30 ), mTreeMaxTriangles( 30 ),
	mpOpenGlAttr(NULL),
	mpBlenderAttr(NULL),
	mTestSphereEnabled( false ),
	mDebugPixelX( -1 ), mDebugPixelY( -1 ), mTestMode(false),
	mSingleFrameMode(false), mSingleFrameFilename("")
	//,mpRndDirections( NULL ), mpRndRoulette( NULL )
{ 
	// create internal attribute list for opengl renderer
	mpOpenGlAttr = new AttributeList("__ntlOpenGLRenderer");
	mpBlenderAttr = new AttributeList("__ntlBlenderAttr");
};


/*****************************************************************************/
/* Destructor */
ntlRenderGlobals::~ntlRenderGlobals() {
	if(mpOpenGlAttr) delete mpOpenGlAttr;
	if(mpBlenderAttr) delete mpBlenderAttr;
	
	
}


/*****************************************************************************/
//! get the next random photon direction
//ntlVec3Gfx ntlRenderGlobals::getRandomDirection( void ) { 
	//return ntlVec3Gfx( 
			//(mpRndDirections->getGfxReal()-0.5), 
			//(mpRndDirections->getGfxReal()-0.5),  
			//(mpRndDirections->getGfxReal()-0.5) ); 
//} 



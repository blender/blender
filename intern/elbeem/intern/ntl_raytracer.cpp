/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Main renderer class
 *
 *****************************************************************************/


#include <sys/stat.h>
#include <sstream>
#include "utilities.h"
#include "ntl_raytracer.h"
#include "ntl_scene.h"
#include "parametrizer.h"
#include "globals.h"

// for non-threaded renderViz
#ifndef NOGUI
#include "../gui/ntl_openglrenderer.h"
#include "../gui/guifuncs.h"
#include "../gui/frame.h"
#endif


/* external parser functions from cfgparser.cxx */
//#include "cfgparse_functions.h"

/* parse given file as config file */
void parseFile(string filename);

/* set pointers for parsing */
void setPointers( ntlRenderGlobals *setglob);


/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlRaytracer::ntlRaytracer(string filename, bool commandlineMode) :
	mpGlob(NULL), 
  mpLightList(NULL), mpPropList(NULL), mpSims(NULL),
	mpOpenGLRenderer(NULL),
	mStopRenderVisualization( false ),
	mThreadRunning( false ), 
	mSimulationTime(0.0), mFirstSim(-1),
	mSingleStepDebug( false )
{
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
 	ntlScene *scene;
	scene = new ntlScene( mpGlob );
	mpGlob->setScene( scene );

	// load config
  setPointers( getRenderGlobals() );
  parseFile( filename.c_str() );
	if(!SIMWORLD_OK()) return;

	// init the scene for the first time
  long sstartTime = getTime();
	scene->buildScene();
	long sstopTime = getTime();
	debMsgStd("ntlRaytracer::ntlRaytracer",DM_MSG,"Scene build time: "<< getTimeString(sstopTime-sstartTime) <<" ", 10);

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
				if( (*mpSims)[i]->getStepTime() > (*mpSims)[mFirstSim]->getStepTime() ) {
					mFirstSim = i;
					debMsgStd("ntlRaytracer::ntlRaytracer",DM_MSG,"First Sim changed: "<<i ,10);
				}
			}
			// check any valid sim
			if(mFirstSim<0) {
				mFirstSim = i;
				debMsgStd("ntlRaytracer::ntlRaytracer",DM_MSG,"First Sim: "<<i ,10);
			}
		}

		if(mFirstSim>=0) {
			debMsgStd("ntlRaytracer::ntlRaytracer",DM_MSG,"Anistart Time: "<<(*mpSims)[mFirstSim]->getStartTime() ,10);
			while(mSimulationTime < (*mpSims)[mFirstSim]->getStartTime() ) {
			debMsgStd("ntlRaytracer::ntlRaytracer",DM_MSG,"Anistart Time: "<<(*mpSims)[mFirstSim]->getStartTime()<<" simtime:"<<mSimulationTime ,10);
				advanceSims();
			}
			long stopTime = getTime();

			mSimulationTime += (*mpSims)[mFirstSim]->getStartTime();
			debMsgStd("ntlRaytracer::ntlRaytracer",DM_MSG,"Time for start simulations times "<<": "<< getTimeString(stopTime-startTime) <<"s ", 1);
#ifndef NOGUI
			guiResetSimulationTimeRange( mSimulationTime );
#endif
		} else {
			if(!mpGlob->getSingleFrameMode()) debMsgStd("ntlRaytracer::ntlRaytracer",DM_WARNING,"No active simulations!", 1);
		}
	}

#ifndef NOGUI
	// setup opengl display, save first animation step for start time 
	if(!commandlineMode) {
		mpOpenGLRenderer = new ntlOpenGLRenderer( mpGlob, scene );
	}
#else // NOGUI
	commandlineMode = true; // remove warning...
#endif // NOGUI
}



/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlRaytracer::~ntlRaytracer()
{
	delete mpGlob->getScene();
  delete mpGlob;
  delete mpLightList;
  delete mpPropList;
  delete mpSims;
#ifndef NOGUI
	if(mpOpenGLRenderer) delete mpOpenGLRenderer;
#endif // NOGUI
}

/******************************************************************************/
/*! set single frame rendering to filename */
void ntlRaytracer::setSingleFrameOut(string singleframeFilename) {
	mpGlob->setSingleFrameMode(true);
	mpGlob->setSingleFrameFilename(singleframeFilename);
}

/******************************************************************************
 * render a whole animation (command line mode) 
 *****************************************************************************/
#if ELBEEM_BLENDER==1
extern "C" {
	void simulateThreadIncreaseFrame(void);
}
#endif // ELBEEM_BLENDER==1
int ntlRaytracer::renderAnimation( void )
{
	// only single pic currently
	//debMsgStd("ntlRaytracer::renderAnimation : Warning only simulating...",1);
 	if(mpGlob->getAniFrames() < 0) {
		debMsgStd("ntlRaytracer::renderAnimation",DM_NOTIFY,"No frames to render... ",1);
		return 1;
	}

	if(mFirstSim<0) {
		debMsgStd("ntlRaytracer::renderAnimation",DM_NOTIFY,"No reference animation found...",1);
		return 1;
	} 

	mThreadRunning = true; // not threaded, but still use the same flags
	renderScene();
	for(int i=0; ((i<mpGlob->getAniFrames()) && (!getStopRenderVisualization() )); i++) {
		advanceSims();
		renderScene();
#if ELBEEM_BLENDER==1
		// update gui display
		simulateThreadIncreaseFrame();
#endif // ELBEEM_BLENDER==1

		if(mpSims->size() <= 0) {
			debMsgStd("ntlRaytracer::renderAnimation",DM_NOTIFY,"No simulations found, stopping...",1);
			return 1;
		}
	}
	mThreadRunning = false;
	
	return 0;
}

/******************************************************************************
 * render a whole animation (visualization mode) 
 * this function is run in another thread, and communicates 
 * with the parent thread via a mutex 
 *****************************************************************************/
int ntlRaytracer::renderVisualization( bool multiThreaded ) 
{
#ifndef NOGUI
	//gfxReal deltat = 0.0015;
	if(multiThreaded) mThreadRunning = true;
	while(!getStopRenderVisualization()) {

		if(mpSims->size() <= 0) {
			debMsgStd("ntlRaytracer::renderVisualization",DM_NOTIFY,"No simulations found, stopping...",1);
			stopSimulationThread();
			break;
		}

		// determine stepsize
		if(!mSingleStepDebug) {
			long startTime = getTime();
			advanceSims();
			long stopTime = getTime();
			debMsgStd("ntlRaytracer::renderVisualization",DM_MSG,"Time for "<<mSimulationTime<<": "<< getTimeString(stopTime-startTime) <<"s ", 10);
		} else {
			double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getStepTime();
			singleStepSims(targetTime);

			// check paniced sims (normally done by advanceSims
			bool allPanic = true;
			for(size_t i=0;i<mpSims->size();i++) {
				if(!(*mpSims)[i]->getPanic()) allPanic = false;
			}
			if(allPanic) {
				warnMsg("ntlRaytracer::advanceSims","All sims panicked... stopping thread" );
				setStopRenderVisualization( true );
			}
			if(!SIMWORLD_OK()) {
				warnMsg("ntlRaytracer::advanceSims","World state error... stopping" );
				setStopRenderVisualization( true );
			}
			//? mSimulationTime = (*mpSims)[mFirstSim]->getCurrentTime();
			//debMsgStd("ntlRaytracer::renderVisualization : single step mode ", 10);
			//debMsgStd("", 10 );
		}

#ifndef NOGUI
		// save frame
		if(mpOpenGLRenderer) mpOpenGLRenderer->saveAnimationFrame( mSimulationTime );
#endif // NOGUI
		
		// for non-threaded check events
		if(!multiThreaded) {
			//if(gpElbeemFrame->ElbeemWindow->visible()) {
				//if (!Fl::check()) break;  // returns immediately
			//} 
			Fl::check();
      //gpElbeemFrame->SceneDisplay->doIdleRedraw();
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
int ntlRaytracer::singleStepVisualization( void ) 
{
	mThreadRunning = true;
	double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getStepTime();
	singleStepSims(targetTime);
	mSimulationTime = (*mpSims)[0]->getCurrentTime();

#ifndef NOGUI
	if(mpOpenGLRenderer) mpOpenGLRenderer->saveAnimationFrame( mSimulationTime );
	Fl::check();
  gpElbeemFrame->SceneDisplay->doOnlyForcedRedraw();
#endif // NOGUI

	mThreadRunning = false;
#ifndef NOGUI
	stopSimulationRestoreGui();
#endif
	return 0;
}

// dont use LBM_EPSILON here, time is always double-precision!
#define LBM_TIME_EPSILON 1e-10

/******************************************************************************
 * advance simulations by time t 
 *****************************************************************************/
int ntlRaytracer::advanceSims()
{
	//gfxReal currTime[ mpSims->size() ]; 
	
	bool done = false;
	bool allPanic = true;
	double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getFrameTime();
	//debMsgStd("ntlRaytracer::advanceSims",DM_MSG,"Advancing sims to "<<targetTime, 10 ); // timedebug

	while(!done) {
		double nextTargetTime = (*mpSims)[mFirstSim]->getCurrentTime() + (*mpSims)[mFirstSim]->getStepTime();
		singleStepSims(nextTargetTime);

		// check target times
		done = true;
		allPanic = false;
		for(size_t i=0;i<mpSims->size();i++) {
			if(!(*mpSims)[i]->getVisible()) continue;
			if((*mpSims)[i]->getPanic()) allPanic = true; // do any panic now!?
			//debMsgStd("ntlRaytracer::advanceSims",DM_MSG, " sim "<<i<<" c"<<(*mpSims)[i]->getCurrentTime()<<" p"<<(*mpSims)[i]->getPanic()<<" t"<<targetTime, 10); // debug // timedebug
		}
		//if((*mpSims)[mFirstSim]->getCurrentTime() < targetTime) done = false;
		if( (targetTime - (*mpSims)[mFirstSim]->getCurrentTime()) > LBM_TIME_EPSILON) done=false;
		if(allPanic) done = true;
	}

	if(allPanic) {
		warnMsg("ntlRaytracer::advanceSims","All sims panicked... stopping thread" );
		setStopRenderVisualization( true );
	}
	for(size_t i=0;i<mpSims->size();i++) {
		SimulationObject *sim = (*mpSims)[i];
		if(!sim->getVisible()) continue;
		if(sim->getPanic()) continue;
		sim->prepareVisualization();
	}

	return 0;
}

/* advance simulations by a single step */
/* dont check target time, if *targetTime==NULL */
void ntlRaytracer::singleStepSims(double targetTime) {
	const bool debugTime = false;
	//double targetTime = mSimulationTime + (*mpSims)[mFirstSim]->getStepTime();
	if(debugTime) errMsg("ntlRaytracer::singleStepSims","Target time: "<<targetTime);

	for(size_t i=0;i<mpSims->size();i++) {
		SimulationObject *sim = (*mpSims)[i];
		if(!sim->getVisible()) continue;
		if(sim->getPanic()) continue;
		bool done = false;
		while(!done) {
			// try to prevent round off errs
			if(debugTime) errMsg("ntlRaytracer::singleStepSims","Test sim "<<i<<" curt:"<< sim->getCurrentTime()<<" target:"<<targetTime<<" delta:"<<(targetTime - sim->getCurrentTime())<<" stept:"<<sim->getStepTime()<<" leps:"<<LBM_TIME_EPSILON ); // timedebug
			if( (targetTime - sim->getCurrentTime()) > LBM_TIME_EPSILON) {
				if(debugTime) errMsg("ntlRaytracer::singleStepSims","Stepping sim "<<i<<" t:"<< sim->getCurrentTime()); // timedebug
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
int ntlRaytracer::renderScene( void )
{
	char nrStr[5];														/* nr conversion */
	//std::ostringstream outfilename(""); 					  /* ppm file */
	std::ostringstream outfn_conv("");  						/* converted ppm with other suffix */
  ntlRenderGlobals *glob;                  	/* storage for global rendering parameters */
  myTime_t timeStart,totalStart,timeEnd; 		/* measure user running time */
  myTime_t rendStart,rendEnd;            		/* measure user rendering time */
  glob = mpGlob;

	/* check if picture already exists... */
	if(!glob->getSingleFrameMode() ) {
		snprintf(nrStr, 5, "%04d", glob->getAniCount() );
		//outfilename << glob->getOutFilename() <<"_" << nrStr << ".ppm";
		outfn_conv  << glob->getOutFilename() <<"_" << nrStr << ".png";
		
		//if((mpGlob->getDisplayMode() == DM_RAY)&&(mpGlob->getFrameSkip())) {
		if(mpGlob->getFrameSkip()) {
			struct stat statBuf;
			if(stat(outfn_conv.str().c_str(),&statBuf) == 0) {
				errorOut("ntlRaytracer::renderscene Warning: file "<<outfn_conv.str()<<" already exists - skipping frame..."); 
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

	/* build scene geometry */
	glob->getScene()->prepareScene();

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
		errorOut("NTL ERROR: Invalid viewpoint vectors!\n");
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
    
		debugOutInter( "ntlRaytracer::renderScene: Line "<<scanline<<
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
#ifndef NOPNG
		writePng(outfn_conv.str().c_str(), rows, w, h);
#else // NOPNG
		debMsgStd("ntlRaytracer::renderScene",DM_NOTIFY, "No PNG linked, no picture...", 1);
#endif // NOPNG
	}


	// next frame 
	glob->setAniCount( glob->getAniCount() +1 );

	// done 
	timeEnd = getTime();

	char resout[1024];
	snprintf(resout,1024, "NTL Done %s, frame %d/%d (%s scene, %s raytracing, %s total, %d shades, %d i.s.'s)!\n", 
				 outfn_conv.str().c_str(), (glob->getAniCount()), (glob->getAniFrames()+1),
				 getTimeString(totalStart-timeStart).c_str(), getTimeString(rendEnd-rendStart).c_str(), getTimeString(timeEnd-timeStart).c_str(),
				 glob->getCounterShades(),
				 glob->getCounterSceneInter() );
	debMsgStd("ntlRaytracer::renderScene",DM_MSG, resout, 1 );

	/* clean stuff up */
	delete [] aaCol;
	delete [] aaUse;
	delete [] finalPic;
	glob->getScene()->cleanupScene();

	if(mpGlob->getSingleFrameMode() ) {
		debMsgStd("ntlRaytracer::renderScene",DM_NOTIFY, "Single frame mode done...", 1 );
		return 1;
	}
	return 0;
}




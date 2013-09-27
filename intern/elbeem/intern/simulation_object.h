/** \file elbeem/intern/simulation_object.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Basic interface for all simulation modules
 *
 *****************************************************************************/

#ifndef SIMULATION_OBJECT_H
#define SIMULATION_OBJECT_H


#define USE_GLUTILITIES
#include "ntl_geometryshader.h"
#include "parametrizer.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

class LbmSolverInterface;
class CellIdentifierInterface;
class ntlTree;
class ntlRenderGlobals;
class ntlRenderGlobals;
class ParticleTracer;
struct elbeemSimulationSettings;


//! type fluid geometry init 
//  warning : should match typeslbm.h values!
const int cFgiFlagstart = 16;
typedef enum {
	fgiFluid     = (1<<(cFgiFlagstart+0)),
	fgiNoFluid   = (1<<(cFgiFlagstart+1)),
	fgiSlipNo    = (1<<(cFgiFlagstart+2)),
	fgiSlipFree  = (1<<(cFgiFlagstart+3)),
	fgiNoBnd     = (1<<(cFgiFlagstart+4)),
	fgiAcc       = (1<<(cFgiFlagstart+5)),
	fgiNoAcc     = (1<<(cFgiFlagstart+6)),

	fgiBndAll    = (fgiSlipNo | fgiSlipFree)
} FgiFlagType;


/*! interface for different simluation models to visualize */
class SimulationObject :
	public ntlGeometryShader {

	public:

		/*! Constructor */
		SimulationObject();
		/*! Destructor */
		virtual ~SimulationObject();
		/*! for init from API */
		void copyElbeemSettings(elbeemSimulationSettings *settings);


		/*! init tree for certain geometry init */
		void initGeoTree();
		/*! destroy tree etc. when geometry init done */
		void freeGeoTree();
		/*! get fluid init type at certain position */
		int geoInitGetPointType(ntlVec3Gfx org, int &OId);
		/*! check for a certain flag type at position org */
		bool geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId);

		// access functions

		/*! get current (max) simulation time */
		double getCurrentTime( void ) { return mTime; }
		/*! set geometry generation start point */
		virtual void setGeoStart(ntlVec3Gfx set) { mGeoStart = set; }
		/*! set geometry generation end point */
		virtual void setGeoEnd(ntlVec3Gfx set) { mGeoEnd = set; }

		/*! set sim panic flag */
		void setPanic(bool set) { mPanic = set; }
		/*! get sim panic flag */
		bool getPanic( void ) { return mPanic; }

		/*! simluation interface: initialize simulation */
		int initializeLbmSimulation(ntlRenderGlobals *glob);

		/*! set current frame */
		void setFrameNum(int num);

		/*! Do geo etc. init */
		virtual int postGeoConstrInit(ntlRenderGlobals *glob) { return initializeLbmSimulation(glob); };
		virtual int initializeShader() { /* ... */ return true; };
		/*! notify object that dump is in progress (e.g. for field dump) */
		virtual void notifyShaderOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename);
		/*! simluation interface: draw the simulation with OpenGL */
		virtual void draw( void ) {};
		virtual vector<ntlGeometryObject *>::iterator getObjectsBegin();
		virtual vector<ntlGeometryObject *>::iterator getObjectsEnd();


		/*! simluation interface: advance simulation another step (whatever delta time that might be) */
		virtual void step( void );
		/*! prepare visualization of simulation for e.g. raytracing */
		virtual void prepareVisualization( void );

		/*! GUI - display debug info */
		virtual void drawDebugDisplay();
		/*! GUI - display interactive info  */
		virtual void drawInteractiveDisplay();
		/*! GUI - handle mouse movement for selection  */
		virtual void setMousePos(int x,int y, ntlVec3Gfx org, ntlVec3Gfx dir);
		virtual void setMouseClick();

		/*! get current start simulation time */
		double getStartTime( void );
		/*! get time for a single animation frame */
		double getFrameTime( int frame );
		/*! get time for a single time step in the simulation */
		double getTimestep( void );

		//! access solver
		LbmSolverInterface *getSolver(){ return mpLbm; }

	protected:

		/*! current time in the simulation */
		double mTime;

		/*! for display - start and end vectors for geometry */
		ntlVec3Gfx mGeoStart, mGeoEnd;

		/*! geometry init id */
		//? int mGeoInitId;
		/*! tree object for geomerty initialization */
		ntlTree *mpGiTree;
		/*! object vector for geo init */
		vector<ntlGeometryObject*> *mpGiObjects;
		/*! remember globals */
		ntlRenderGlobals *mpGlob;
		
		/*! simulation panic on/off */
		bool mPanic;

		/*! debug info to display */
		int mDebugType;

		/*! pointer to the lbm solver */
		LbmSolverInterface *mpLbm;

		/*! parametrizer for lbm solver */
		Parametrizer *mpParam;

		/*! particle tracing object */
		ParticleTracer *mpParts;

		/*! show parts of the simulation toggles */
		bool mShowSurface;
		bool mShowParticles;

		/*! debug display settings */
		int mDebDispSetting;

		/*! pointer to identifier of selected node */
		CellIdentifierInterface *mSelectedCid;

		/*! storage of API settings */
		elbeemSimulationSettings *mpElbeemSettings;

	public:

		// debug display setting funtions

		/*! set type of info to display */
		inline void setDebugDisplay(int disp) { mDebugType = disp; }
		inline int getDebugDisplay() { return mDebugType; }

		/* miscelleanous access functions */

		/*! init parametrizer for anim step length */
		void initParametrizer(Parametrizer *set) { mpParam = set; }
		/*! init parametrizer for anim step length */
		Parametrizer *getParametrizer() { return mpParam; }

		/*! check status (e.g. stop/abort) from calling program, returns !=0 if sth. happened... */
		// parameters same as elbeem runsimCallback
		int checkCallerStatus(int status, int frame);

		/*! get bounding box of fluid for GUI */
		virtual inline ntlVec3Gfx *getBBStart() 	{ return &mGeoStart; }
		virtual inline ntlVec3Gfx *getBBEnd() 		{ return &mGeoEnd; }


private:
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("ELBEEM:SimulationObject")
#endif
};


#endif




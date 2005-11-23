/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Main renderer class
 *
 *****************************************************************************/
#ifndef NTL_RAYTRACER_HH
#define NTL_RAYTRACER_HH

#include "ntl_vector3dim.h"
#include "ntl_ray.h"
#include "ntl_geometryobject.h"
#include "ntl_lightobject.h"
#include "ntl_renderglobals.h"
#include "ntl_material.h"
#include "simulation_object.h"
#include "elbeem.h"
class ntlOpenGLRenderer;

class ntlWorld
{
	public:
		/*! Constructor */
		ntlWorld(string filename, bool commandlineMode);
		/*! Constructor for API init */
		ntlWorld(elbeemSimulationSettings *simSettings);
		/*! Destructor */
		virtual ~ntlWorld( void );
		/*! default init for all contructors */
		void initDefaults();
		/*! common world contruction stuff once the scene is set up */
		void finishWorldInit();

		/*! render a whole animation (command line mode) */
		int renderAnimation( void );
		/*! render a whole animation (visualization mode) */
		int renderVisualization( bool );
		/*! render a single step for viz mode */
		int singleStepVisualization( void );
		/*! advance simulations by time frame time */
		int advanceSims(int framenum);
		/*! advance simulations by a single step */
		void singleStepSims(double targetTime);

		/*! set stop rend viz flag */
		void setStopRenderVisualization(bool set) { mStopRenderVisualization = set; }
		/*! should the rendering viz thread be stopped? */
		bool getStopRenderVisualization() { return mStopRenderVisualization; }

		/*! render scene (a single pictures) */
		virtual int renderScene( void );

		/*! set single frame rendering to filename */
		void setSingleFrameOut( string singleframeFilename );

		/* access functions */

		/*! set&get render globals */
		inline void setRenderGlobals( ntlRenderGlobals *set) { mpGlob = set; }
		inline ntlRenderGlobals *getRenderGlobals( void )    { return mpGlob; }

		/*! set&get render globals */
		inline void setSimulationTime( double set) { mSimulationTime = set; }
		inline double getSimulationTime( void ) { return mSimulationTime; }

		/*! set&get single step debug mode */
		inline void setSingleStepDebug( bool set) { mSingleStepDebug = set; }
		inline bool getSingleStepDebug( void ) { return mSingleStepDebug; }

		/*! &get simulation object vector (debugging) */
		inline vector<SimulationObject*> *getSimulations( void ) { return mpSims; }

		/*! get opengl renderer */
		inline ntlOpenGLRenderer *getOpenGLRenderer() { return mpOpenGLRenderer; }

	private:

	protected:

		/*! global render settings needed almost everywhere */
		ntlRenderGlobals        *mpGlob;

		/*! a list of lights in the scene (geometry is store in ntl_scene) */
		vector<ntlLightObject*> *mpLightList;
		/*! surface materials */
		vector<ntlMaterial*>    *mpPropList;
		/*! sims list */
		vector<SimulationObject*> *mpSims;

		/*! opengl display */
		ntlOpenGLRenderer *mpOpenGLRenderer;

		/*! stop rend viz? */
		bool mStopRenderVisualization;

		/*! rend viz thread currently running? */
		bool mThreadRunning;

		/*! remember the current simulation time */
		double mSimulationTime;
		/*! first simulation that is valid */
		int mFirstSim;

		/*! single step mode for debugging */
		bool mSingleStepDebug;

		/*! count no. of frame for viz render */
		int mFrameCnt;
};

#endif

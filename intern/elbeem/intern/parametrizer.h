/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Parameter calculator for the LBM Solver class
 *
 *****************************************************************************/
#ifndef MFFSLBM_PARAMETRIZER
#define MFFSLBM_PARAMETRIZER  


/* LBM Files */
#include "utilities.h"
#include "attributes.h"

/* parametrizer accuracy */
typedef double ParamFloat;
typedef ntlVec3d ParamVec;

/*! flags to check which values are known */
#define PARAM_RELAXTIME  (1<< 0)
#define PARAM_REYNOLDS   (1<< 1)
#define PARAM_VISCOSITY  (1<< 2)
#define PARAM_SOUNDSPEED (1<< 3)
#define PARAM_DOMAINSIZE (1<< 4)
#define PARAM_GRAVITY		 (1<< 5)
#define PARAM_STEPTIME	 (1<< 7)
#define PARAM_SIZE    	 (1<< 8)
#define PARAM_TIMEFACTOR (1<< 9)
#define PARAM_ANIFRAMETIME 		(1<<11)
#define PARAM_ANISTART	 			(1<<12)
#define PARAM_SURFACETENSION 	(1<<13)
#define PARAM_DENSITY				 	(1<<14)
#define PARAM_CELLSIZE			 	(1<<15)
#define PARAM_GSTAR					 	(1<<16)
#define PARAM_MAXSPEED			 	(1<<17)
#define PARAM_SIMMAXSPEED			(1<<18)
#define PARAM_FLUIDVOLHEIGHT  (1<<19)
#define PARAM_NORMALIZEDGSTAR (1<<20)
#define PARAM_NUMIDS					21

//! output parameter debug message?
//#define PARAM_DEBUG      1



/*! Parameter calculator for the LBM Solver class */
class Parametrizer {

	public:
		/*! default contructor */
		Parametrizer();

		/*! destructor */
		~Parametrizer();

		/*! Initilize variables fom attribute list */
		void parseAttrList( void );

		/*! function that tries to calculate all the missing values from the given ones
		 *  prints errors and returns false if thats not possible */
		bool calculateAllMissingValues( bool silent = false );
		bool oldCalculateAllMissingValues( void );
		/*! is the parametrizer used at all? */
		bool isUsed() { return true; }

		/*! add this flag to the seen values */
		void seenThis(int seen) { mSeenValues = (mSeenValues | seen); 
#ifdef PARAM_DEBUG		
			errorOut(" seen "<<seen<<endl); 
#endif
		}

		/*! set the flags integer */
		void setSeenValues(int set) { mSeenValues = set; }
		/*! check if the flags are set in the values int */
		bool checkSeenValues(int check) { /*errorOut( " b"<<((mSeenValues&check)==check) );*/ return ((mSeenValues&check)==check); }

		/*! add this flag to the calculated values */
		void calculatedThis(int cac) { mCalculatedValues = (mCalculatedValues | cac); /*errorOut(" a "<<seen);*/ }
		/*! set the calculated flags integer */
		void setCalculatedValues(int set) { mCalculatedValues = set; }
		/*! check if the calculated flags are set in the values int */
		bool checkCalculatedValues(int check) { /*errorOut( " b"<<((mSeenValues&check)==check) );*/ return ((mCalculatedValues&check)==check); }

		/*! scale a given speed vector in m/s to lattice values 
		 *  usage string is only needed for debugging */
		ParamVec calculateAddForce(ParamVec vec, string usage);

		/* simple calulation functions */
		/*! get omega for LBM */
		ParamFloat calculateOmega( void );
		/*! get no. of timesteps for LBM */
		int calculateNoOfSteps( ParamFloat timelen );
		/*! get external force x component */
		ParamVec calculateGravity( void );
		/*! get no of steps for the given length in seconds */
		int calculateStepsForSecs( ParamFloat s );
		/*! get start time of animation */
		int calculateAniStart( void );
		/*! get no of steps for a singel animation frame */
		int calculateAniStepsPerFrame( void );
		/*! get extent of the domain = (1,1,1) if parametrizer not used, (x,y,z) [m] otherwise */
		ParamVec calculateExtent( void );
		/*! get (scaled) surface tension */
		ParamFloat calculateSurfaceTension( void );
		/*! get time step size for lbm (equals mTimeFactor) */
		// unused ParamFloat calculateTimestep( void );
 		/*! calculate size of a single cell */
		ParamFloat calculateCellSize(void);
 		/*! calculate the lattice viscosity */
		ParamFloat calculateLatticeViscosity(void);

		/*! calculate lattice velocity from real world value [m/s] */
		ParamVec calculateLattVelocityFromRw( ParamVec ivel );
		/*! calculate real world [m/s] velocity from lattice value */
		ParamVec calculateRwVelocityFromLatt( ParamVec ivel );


		/*! set relaxation time */
		void setRelaxTime(ParamFloat set) { mRelaxTime = set; seenThis( PARAM_RELAXTIME ); }
		/*! get relaxation time */
		ParamFloat getRelaxTime( void )   { return mRelaxTime; }

		/*! set reynolds number */
		void setReynolds(ParamFloat set) { mReynolds = set; seenThis( PARAM_REYNOLDS ); }
		/*! get reynolds number */
		ParamFloat getReynolds( void )   { return mReynolds; }

		/*! set kinematic viscosity */
		void setViscosity(ParamFloat set) { mViscosity = set; seenThis( PARAM_VISCOSITY ); }
		/*! get kinematic viscosity */
		ParamFloat getViscosity( void )   { return mViscosity; }

		/*! set speed of sound */
		void setSoundSpeed(ParamFloat set) { mSoundSpeed = set; seenThis( PARAM_SOUNDSPEED ); }
		/*! get speed of sound */
		ParamFloat getSoundSpeed( void )   { return mSoundSpeed; }

		/*! set the external force */
		void setGravity(ParamFloat setx, ParamFloat sety, ParamFloat setz) { mGravity = ParamVec(setx,sety,setz); seenThis( PARAM_GRAVITY ); }
		void setGravity(ParamVec set) { mGravity = set; seenThis( PARAM_GRAVITY ); }

		/*! set the length of a single time step */
		void setStepTime(ParamFloat set) { mStepTime = set; seenThis( PARAM_STEPTIME ); }
		/*! get the length of a single time step */
		ParamFloat getStepTime( void);
		/*! set a desired step time for rescaling/adaptive timestepping */
		void setDesiredStepTime(ParamFloat set) { mDesiredStepTime = set; }
		/*! get the length of a single time step */
		ParamFloat getMaxStepTime( void )   { return mMaxStepTime; }
		/*! get the length of a single time step */
		ParamFloat getMinStepTime( void )   { return mMinStepTime; }

		/*! set the time scaling factor */
		void setTimeFactor(ParamFloat set) { mTimeFactor = set; seenThis( PARAM_TIMEFACTOR ); }
		/*! get the time scaling factor */
		ParamFloat getTimeFactor( void )   { return mTimeFactor; }

		/*! init domain resoultion */
		void setSize(int ijk)            { mSizex = ijk; mSizey = ijk; mSizez = ijk; seenThis( PARAM_SIZE ); }
		void setSize(int i,int j, int k) { mSizex = i; mSizey = j; mSizez = k; seenThis( PARAM_SIZE ); }

		/*! set time of an animation frame (renderer)  */
		void setAniFrameTime(ParamFloat set) { mAniFrameTime = set; seenThis( PARAM_ANIFRAMETIME ); }
		/*! get time of an animation frame (renderer)  */
		ParamFloat getAniFrameTime( void )   { return mAniFrameTime; }

		/*! set starting time of the animation (renderer) */
		void setAniStart(ParamFloat set) { mAniStart = set; seenThis( PARAM_ANISTART ); }
		/*! get starting time of the animation (renderer) */
		ParamFloat getAniStart( void )   { return mAniStart; }

		/*! set starting time of the animation (renderer) */
		void setSurfaceTension(ParamFloat set) { mSurfaceTension = set; seenThis( PARAM_SURFACETENSION ); }
		/*! get starting time of the animation (renderer) */
		ParamFloat getSurfaceTension( void )   { return mSurfaceTension; }

		/*! set fluid density */
		void setDensity(ParamFloat set) { mDensity = set; seenThis( PARAM_DENSITY ); }
		/*! get fluid density */
		ParamFloat getDensity( void )   { return mDensity; }

		/*! set g star value */
		void setGStar(ParamFloat set) { mGStar = set; seenThis( PARAM_GSTAR ); }
		/*! get g star value */
		ParamFloat getGStar( void )   { return mGStar; }
		/*! get g star value with fhvol calculations */
		ParamFloat getCurrentGStar( void );
		/*! set normalized g star value */
		void setNormalizedGStar(ParamFloat set) { mNormalizedGStar = set; seenThis( PARAM_NORMALIZEDGSTAR ); }
		/*! get normalized g star value */
		ParamFloat getNormalizedGStar( void ) { return mNormalizedGStar; }

		/*! set g star value */
		void setFluidVolumeHeight(ParamFloat set) { mFluidVolumeHeight = set; seenThis( PARAM_FLUIDVOLHEIGHT ); }
		/*! get g star value */
		ParamFloat getFluidVolumeHeight( void )   { return mFluidVolumeHeight; }

		/*! set the size of a single lbm cell */
		void setDomainSize(ParamFloat set) { mDomainSize = set; seenThis( PARAM_DOMAINSIZE ); }
		/*! get the size of a single lbm cell */
		ParamFloat getDomainSize( void )   { return mDomainSize; }

		/*! set the size of a single lbm cell */
		void setCellSize(ParamFloat set) { mCellSize = set; seenThis( PARAM_CELLSIZE ); }
		/*! get the size of a single lbm cell */
		ParamFloat getCellSize( void )   { return mCellSize; }

		/*! set active flag for parametrizer */
		//void setActive(bool set) { mActive = set; }

		/*! set attr list pointer */
		void setAttrList(AttributeList *set) { mpAttrs = set; }
		/*! Returns the attribute list pointer */
		inline AttributeList *getAttributeList() { return mpAttrs; }

		/*! set maximum allowed speed for maxspeed setup */
		void setMaxSpeed(ParamFloat set) { mMaxSpeed = set; seenThis( PARAM_MAXSPEED ); }
		/*! get maximum allowed speed for maxspeed setup */
		ParamFloat getMaxSpeed( void )   { return mMaxSpeed; }

		/*! set maximum allowed speed for maxspeed setup */
		void setSimulationMaxSpeed(ParamFloat set) { mSimulationMaxSpeed = set; seenThis( PARAM_SIMMAXSPEED ); }
		/*! get maximum allowed speed for maxspeed setup */
		ParamFloat getSimulationMaxSpeed( void )   { return mSimulationMaxSpeed; }

		/*! set maximum allowed omega for time adaptivity */
		void setTadapMaxOmega(ParamFloat set) { mTadapMaxOmega = set; }
		/*! get maximum allowed omega for time adaptivity */
		ParamFloat getTadapMaxOmega( void )   { return mTadapMaxOmega; }

		/*! set maximum allowed speed for time adaptivity */
		void setTadapMaxSpeed(ParamFloat set) { mTadapMaxSpeed = set; }
		/*! get maximum allowed speed for time adaptivity */
		ParamFloat getTadapMaxSpeed( void )   { return mTadapMaxSpeed; }

		/*! set maximum allowed omega for time adaptivity */
		void setTadapLevels(int set) { mTadapLevels = set; }
		/*! get maximum allowed omega for time adaptivity */
		int getTadapLevels( void )   { return mTadapLevels; }


		/*! set */
		//	void set(ParamFloat set) { m = set; seenThis( PARAM_ ); }
		/*! get */
		//	ParamFloat get( void )   { return m; }



	private:

		/*! type of parameter setup to use */
		string mSetupType;

		/*! relaxation time [s] */
		ParamFloat mRelaxTime;

		/*! reynolds number (calculated from domain length and max. speed [dimensionless] */
		ParamFloat mReynolds;

		/*! kinematic viscosity of the fluid [m^2/s] */
		ParamFloat mViscosity;

		/*! speed of sound of the fluid [m/s] */
		ParamFloat mSoundSpeed;

		/*! size of the domain [m] */
		ParamFloat mDomainSize;

		/*! size of a single cell in the grid [m] */
		ParamFloat mCellSize;

		/*! time step length [s] */
		ParamFloat mTimeStep;

		/*! external force as acceleration [m/s^2] */
		ParamVec mGravity;
		/*! force converted to lattice units (returned by calc gravity) */
		ParamVec mLatticeGravity;


		/*! length of one time step in the simulation */
		ParamFloat mStepTime;
		/*! desired step time for rescaling/adaptive timestepping, only regarded if >0.0, reset after usage */
		ParamFloat mDesiredStepTime;
		/*! minimal and maximal step times for current setup */
		ParamFloat mMaxStepTime, mMinStepTime;

		/*! domain resoultion, the same values as in lbmsolver */
		int mSizex, mSizey, mSizez;

		/*! time scaling factor (auto calc from accel, or set), equals the delta t in LBM */
		ParamFloat mTimeFactor;

		/*! for renderer - length of an animation step [s] */
		ParamFloat mAniFrameTime;

		/*! for renderer - start time of the animation [s] */
		ParamFloat mAniStart;

		/*! extent of the domain in meters */
		ParamVec mExtent;

		/*! surface tension, [kg/s^2] */
		ParamFloat mSurfaceTension;

		/*! fluid density [kg/m^3], default 1.0 g/cm^3 */
		ParamFloat mDensity;

		/*! max difference due to gravity (for caro setup) */
		ParamFloat mGStar;
		/*! set gstar normalized! */
		ParamFloat mNormalizedGStar;
		/*! fluid volume/height multiplier for GStar */
		ParamFloat mFluidVolumeHeight;

		/*! max speed (for maxspeed setup) */
		ParamFloat mMaxSpeed;

		/*! current max speed of the simulation (for adaptive time steps) */
		ParamFloat mSimulationMaxSpeed;
		/*! maximum omega (for adaptive time steps) */
		ParamFloat mTadapMaxOmega;
		/*! maximum allowed speed in lattice units e.g. 0.1 (for adaptive time steps, not directly used in parametrizer) */
		ParamFloat mTadapMaxSpeed;
		/*! no. of levels for max omega (set by fsgr, not in cfg file) */
		int mTadapLevels;


		/*! values that are seen for this simulation */
		int mSeenValues;

		/*! values that are calculated from the seen ones for this simulation */
		int mCalculatedValues;

		/*! is the parametrizer active? */
		//bool mActive;

		/*! pointer to the attribute list */
		AttributeList *mpAttrs;
};


#endif


/** \file elbeem/intern/parametrizer.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Parameter calculator for the LBM Solver class
 *
 *****************************************************************************/

#include <sstream>
#include "parametrizer.h"

// debug output flag, has to be off for win32 for some reason...
#define DEBUG_PARAMCHANNELS 0

/*! param seen debug string array */
const char *ParamStrings[] = {
	"RelaxTime",
	"Reynolds",
	"Viscosity",
	"SoundSpeed",
	"DomainSize",
	"GravityForce",
	"TimeLength",
	"Timestep",
	"Size",
	"TimeFactor",
	"AniFrames",
	"AniFrameTime",
	"AniStart",
	"SurfaceTension",
	"Density",
	"CellSize",
	"GStar",
	"MaxSpeed",
	"SimMaxSpeed",
	"FluidVolHeight",
	"NormalizedGStar",
	"PSERR", "PSERR", "PSERR", "PSERR"
};



/******************************************************************************
 * Default constructor
 *****************************************************************************/
Parametrizer::Parametrizer( void ) :
	mcViscosity( 8.94e-7 ), 
	mSoundSpeed( 1500 ),
	mDomainSize( 0.1 ), mCellSize( 0.01 ),
	mcGravity( ParamVec(0.0) ),
	mTimestep(0.0001), mDesiredTimestep(-1.0),
	mMaxTimestep(-1.0),
	mMinTimestep(-1.0), 
	mSizex(50), mSizey(50), mSizez(50),
	mTimeFactor( 1.0 ),
	mcAniFrameTime(0.0001),
	mTimeStepScale(1.0),
	mAniStart(0.0),
	//mExtent(1.0, 1.0, 1.0), //mSurfaceTension( 0.0 ),
	mDensity(1000.0), mGStar(0.0001), mFluidVolumeHeight(0.0),
	mSimulationMaxSpeed(0.0),
	mTadapMaxOmega(2.0), mTadapMaxSpeed(0.1), mTadapLevels(1),
	mFrameNum(0),
	mSeenValues( 0 ), mCalculatedValues( 0 )
{
}


/******************************************************************************
 * Destructor
 *****************************************************************************/
Parametrizer::~Parametrizer() 
{
	/* not much to do... */
}

/******************************************************************************
 * Init from attr list
 *****************************************************************************/
void Parametrizer::parseAttrList() 
{
	if(!mpAttrs) {
		errFatal("Parametrizer::parseAttrList", "mpAttrs pointer not initialized!", SIMWORLD_INITERROR);
		return;
	}

	// unused
	string  mSetupType = "";
	mSetupType = mpAttrs->readString("p_setup",mSetupType, "Parametrizer","mSetupType", false); 

	// real params
	if(getAttributeList()->exists("p_viscosity")) {
			mcViscosity = mpAttrs->readChannelFloat("p_viscosity"); seenThis( PARAM_VISCOSITY ); }

	mSoundSpeed = mpAttrs->readFloat("p_soundspeed",mSoundSpeed, "Parametrizer","mSoundSpeed", false); 
	if(getAttributeList()->exists("p_soundspeed")) seenThis( PARAM_SOUNDSPEED );

	mDomainSize = mpAttrs->readFloat("p_domainsize",mDomainSize, "Parametrizer","mDomainSize", false); 
	if(getAttributeList()->exists("p_domainsize")) seenThis( PARAM_DOMAINSIZE );
	if(mDomainSize<=0.0) {
		errMsg("Parametrizer::parseAttrList","Invalid real world domain size:"<<mDomainSize<<", resetting to 0.1");
		mDomainSize = 0.1;
	}

	if(getAttributeList()->exists("p_gravity")) { // || (!mcGravity.isInited()) ) {
		mcGravity = mpAttrs->readChannelVec3d("p_gravity"); seenThis( PARAM_GRAVITY );
	}

	mTimestep = mpAttrs->readFloat("p_steptime",mTimestep, "Parametrizer","mTimestep", false); 
	if(getAttributeList()->exists("p_steptime")) seenThis( PARAM_STEPTIME );

	mTimeFactor = mpAttrs->readFloat("p_timefactor",mTimeFactor, "Parametrizer","mTimeFactor", false); 
	if(getAttributeList()->exists("p_timefactor")) seenThis( PARAM_TIMEFACTOR );

	if(getAttributeList()->exists("p_aniframetime")) { //|| (!mcAniFrameTime.isInited()) ) {
		mcAniFrameTime = mpAttrs->readChannelFloat("p_aniframetime");seenThis( PARAM_ANIFRAMETIME ); 
	}
	mTimeStepScale = mpAttrs->readFloat("p_timestepscale",mTimeStepScale, "Parametrizer","mTimeStepScale", false); 

	mAniStart = mpAttrs->readFloat("p_anistart",mAniStart, "Parametrizer","mAniStart", false); 
	if(getAttributeList()->exists("p_anistart")) seenThis( PARAM_ANISTART );
	if(mAniStart<0.0) {
		errMsg("Parametrizer::parseAttrList","Invalid start time:"<<mAniStart<<", resetting to 0.0");
		mAniStart = 0.0;
	}

	//mSurfaceTension = mpAttrs->readFloat("p_surfacetension",mSurfaceTension, "Parametrizer","mSurfaceTension", false); 
	//if(getAttributeList()->exists("p_surfacetension")) seenThis( PARAM_SURFACETENSION );

	mDensity = mpAttrs->readFloat("p_density",mDensity, "Parametrizer","mDensity", false); 
	if(getAttributeList()->exists("p_density")) seenThis( PARAM_DENSITY );

	ParamFloat cellSize = 0.0; // unused, deprecated
	cellSize = mpAttrs->readFloat("p_cellsize",cellSize, "Parametrizer","cellSize", false); 

	mGStar = mpAttrs->readFloat("p_gstar",mGStar, "Parametrizer","mGStar", false); 
	if(getAttributeList()->exists("p_gstar")) seenThis( PARAM_GSTAR );

	mNormalizedGStar = mpAttrs->readFloat("p_normgstar",mNormalizedGStar, "Parametrizer","mNormalizedGStar", false); 
	if(getAttributeList()->exists("p_normgstar")) seenThis( PARAM_NORMALIZEDGSTAR );

	mTadapMaxOmega = mpAttrs->readFloat("p_tadapmaxomega",mTadapMaxOmega, "Parametrizer","mTadapMaxOmega", false); 
	mTadapMaxSpeed = mpAttrs->readFloat("p_tadapmaxspeed",mTadapMaxSpeed, "Parametrizer","mTadapMaxSpeed", false); 
}

/******************************************************************************
 *! advance to next render/output frame 
 *****************************************************************************/
void Parametrizer::setFrameNum(int frame) {
	mFrameNum = frame;
#if DEBUG_PARAMCHANNELS>0
	errMsg("DEBUG_PARAMCHANNELS","setFrameNum frame-num="<<mFrameNum);
#endif // DEBUG_PARAMCHANNELS>0
}
/*! get time of an animation frame (renderer)  */
// also used by: mpParam->getCurrentAniFrameTime() , e.g. for velocity dump
ParamFloat Parametrizer::getAniFrameTime( int frame )   { 
	double frametime = (double)frame;
	ParamFloat anift = mcAniFrameTime.get(frametime);
	if(anift<0.0) {
		ParamFloat resetv = 0.;
		errMsg("Parametrizer::setFrameNum","Invalid frame time:"<<anift<<" at frame "<<frame<<", resetting to "<<resetv);
		anift = resetv; 
	}
#if DEBUG_PARAMCHANNELS>0
	if((0)|| (DEBUG_PARAMCHANNELS)) errMsg("DEBUG_PARAMCHANNELS","getAniFrameTime frame="<<frame<<", frametime="<<anift<<" ");
#endif // DEBUG_PARAMCHANNELS>0
	return anift; 
}

/******************************************************************************
 * scale a given speed vector in m/s to lattice values 
 *****************************************************************************/
ParamVec Parametrizer::calculateAddForce(ParamVec vec, string usage)
{
	ParamVec ret = vec * (mTimestep*mTimestep) /mCellSize;
	debMsgStd("Parametrizer::calculateVector", DM_MSG, "scaled vector = "<<ret<<" for '"<<usage<<"', org = "<<vec<<" dt="<<mTimestep ,10);
	return ret;
}


/******************************************************************************
 * calculate size of a single cell
 *****************************************************************************/
ParamFloat Parametrizer::calculateCellSize(void)
{
	int maxsize = mSizex; // get max size
	if(mSizey>maxsize) maxsize = mSizey;
	if(mSizez>maxsize) maxsize = mSizez;
	maxsize = mSizez; // take along gravity dir for now!
	ParamFloat cellSize = 1.0 / (ParamFloat)maxsize;
	return cellSize;
}


/*****************************************************************************/
/* simple calulation functions */
/*****************************************************************************/

/*! get omega for LBM from channel */
ParamFloat Parametrizer::calculateOmega( double time ) { 
	ParamFloat viscStar = calculateLatticeViscosity(time);
	ParamFloat relaxTime = (6.0 * viscStar + 1) * 0.5;
#if DEBUG_PARAMCHANNELS>0
	errMsg("DEBUG_PARAMCHANNELS","calculateOmega viscStar="<<viscStar<<" relaxtime="<<relaxTime);
#endif // DEBUG_PARAMCHANNELS>0
	return (1.0/relaxTime); 
}

/*! get external force x component */
ParamVec Parametrizer::calculateGravity( double time ) { 
	ParamVec grav = mcGravity.get(time);
	ParamFloat forceFactor = (mTimestep *mTimestep)/mCellSize;
	ParamVec latticeGravity = grav * forceFactor;
#if DEBUG_PARAMCHANNELS>0
	errMsg("DEBUG_PARAMCHANNELS","calculateGravity grav="<<grav<<" ff"<<forceFactor<<" lattGrav="<<latticeGravity);
#endif // DEBUG_PARAMCHANNELS>0
	return latticeGravity; 
}

/*! calculate the lattice viscosity */
ParamFloat Parametrizer::calculateLatticeViscosity( double time ) { 
	// check seen values
	int reqValues = PARAM_VISCOSITY | PARAM_STEPTIME;
	if(!checkSeenValues( reqValues ) ){
		errMsg("Parametrizer::calculateLatticeViscosity"," Missing arguments!");
	}
	ParamFloat viscStar = mcViscosity.get(time) * mTimestep / (mCellSize*mCellSize);
#if DEBUG_PARAMCHANNELS>0
	errMsg("DEBUG_PARAMCHANNELS","calculateLatticeViscosity viscStar="<<viscStar);
#endif // DEBUG_PARAMCHANNELS>0
	return viscStar; 
}

/*! get no of steps for the given length in seconds */
int Parametrizer::calculateStepsForSecs( ParamFloat s ) { 
	return (int)(s/mTimestep); 
}

/*! get start time of animation */
int Parametrizer::calculateAniStart( void )   { 
	return (int)(mAniStart/mTimestep); 
}

/*! get no of steps for a single animation frame */
int Parametrizer::calculateAniStepsPerFrame(int frame)   { 
	if(!checkSeenValues(PARAM_ANIFRAMETIME)) {
		errFatal("Parametrizer::calculateAniStepsPerFrame", "Missing ani frame time argument!", SIMWORLD_INITERROR);
		return 1;
	}
	int value = (int)(getAniFrameTime(frame)/mTimestep); 
	if((value<0) || (value>1000000)) {
		errFatal("Parametrizer::calculateAniStepsPerFrame", "Invalid step-time (="<<value<<") <> ani-frame-time ("<<mTimestep<<") settings, aborting...", SIMWORLD_INITERROR);
		return 1;
	}
	return value;
}

/*! get no. of timesteps for LBM */
int Parametrizer::calculateNoOfSteps( ParamFloat timelen ) { 
	return (int)(timelen/mTimestep); 
}

/*! get extent of the domain = (1,1,1) if parametrizer not used, (x,y,z) [m] otherwise */
//ParamVec Parametrizer::calculateExtent( void ) { 
	//return mExtent; 
//}

/*! get (scaled) surface tension */
//ParamFloat Parametrizer::calculateSurfaceTension( void ) { return mSurfaceTension; }

/*! get the length of a single time step */
// explicity scaled by time factor for refinement 
ParamFloat Parametrizer::getTimestep( void ) { 
	return mTimestep; 
}

/*! calculate lattice velocity from real world value [m/s] */
ParamVec Parametrizer::calculateLattVelocityFromRw( ParamVec ivel ) { 
	ParamVec velvec = ivel;
	velvec /= mCellSize;
	velvec *= mTimestep;
	return velvec; 
}
/*! calculate real world [m/s] velocity from lattice value */
ParamVec Parametrizer::calculateRwVelocityFromLatt( ParamVec ivel ) { 
	ParamVec velvec = ivel;
	velvec *= mCellSize;
	velvec /= mTimestep;
	return velvec; 
}


/*! get g star value with fhvol calculations */
ParamFloat Parametrizer::getCurrentGStar( void ) {
	ParamFloat gStar = mGStar; // check? TODO get from mNormalizedGStar?
	if(mFluidVolumeHeight>0.0) { gStar = mGStar/mFluidVolumeHeight; }
	return gStar;
}

/******************************************************************************
 * function that tries to calculate all the missing values from the given ones
 * prints errors and returns false if thats not possible 
 *****************************************************************************/
bool Parametrizer::calculateAllMissingValues( double time, bool silent )
{
	bool init = false;  // did we init correctly?
	int  valuesChecked = 0;
	int reqValues;

	// we always need the sizes
	reqValues = PARAM_SIZE;
	valuesChecked |= reqValues;
	if(!checkSeenValues(reqValues)) {
		errMsg("Parametrizer::calculateAllMissingValues"," Missing size argument!");
		return false;
	}

	if(!checkSeenValues(PARAM_DOMAINSIZE)) {
		errMsg("Parametrizer::calculateAllMissingValues"," Missing domain size argument!");
		return false;
	}
	int maxsize = mSizex; // get max size
	if(mSizey>maxsize) maxsize = mSizey;
	if(mSizez>maxsize) maxsize = mSizez;
	maxsize = mSizez; // take along gravity dir for now!
	mCellSize = ( mDomainSize * calculateCellSize() ); // sets mCellSize
	if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," max domain resolution="<<(maxsize)<<" cells , cellsize="<<mCellSize ,10);

			
	/* Carolin init , see DA for details */
	ParamFloat maxDeltaT = 0.0;

	/* normalized gstar init */
	reqValues = PARAM_NORMALIZEDGSTAR;
	valuesChecked |= reqValues;
	if(checkSeenValues( reqValues ) ){
		//if(checkSeenValues( PARAM_GSTAR ) ){ if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_WARNING," g star value override by normalizedGStar!",1); }
		const ParamFloat normgstarReset = 0.005;
		if(mNormalizedGStar<=1e-6) {
			errMsg("Parametrizer::calculateAllMissingValues","Invalid NormGstar: "<<mNormalizedGStar<<"... resetting to "<<normgstarReset);
			mNormalizedGStar = normgstarReset;
		}

		mGStar = mNormalizedGStar/maxsize;

// TODO FIXME add use testdata check!
mGStar = mNormalizedGStar/mSizez;
errMsg("Warning","Used z-dir for gstar!");

		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," g star set to "<<mGStar<<" from normalizedGStar="<<mNormalizedGStar ,1);
		seenThis(PARAM_GSTAR);
	}

	reqValues = PARAM_GSTAR | PARAM_VISCOSITY;
	if((checkSeenValues(PARAM_SURFACETENSION))) reqValues |= PARAM_DENSITY; // surface tension optional now...
	valuesChecked |= reqValues;
	if(checkSeenValues( reqValues ) ){
		const ParamFloat gstarReset = 0.0005;
		if(getCurrentGStar()<=1e-6) {
			errMsg("Parametrizer::calculateAllMissingValues","Invalid Gstar: "<<getCurrentGStar()<<" (set to "<<mGStar<<") ... resetting to "<<gstarReset);
			mGStar = gstarReset;
		}

		ParamFloat gStar = getCurrentGStar(); // mGStar
		if(mFluidVolumeHeight>0.0) {
			debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," height"<<mFluidVolumeHeight<<" resGStar = "<<gStar, 10);
		}
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," g star = "<<gStar, 10);

		ParamFloat forceStrength = 0.0;
		//if(checkSeenValues(PARAM_GRAVITY)) { forceStrength = norm( calculateGravity(time) ); }
		if(checkSeenValues(PARAM_GRAVITY)) { forceStrength = norm( mcGravity.get(time) ); }

		// determine max. delta density per timestep trough gravity force
		if(forceStrength>0.0) {
			maxDeltaT = sqrt( gStar*mCellSize *mTimeStepScale /forceStrength );
		} else {
			// use 1 lbm setp = 1 anim step as max
			maxDeltaT = getAniFrameTime(0);
		}

		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," targeted step time = "<<maxDeltaT, 10);

		//ParamFloat viscStarFac = mViscosity/(mCellSize*mCellSize);
		//if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," viscStarFac = "<<viscStarFac<<" viscosity:"<<mViscosity, 10);

		// time step adaptivty, only for caro with max sim speed
		ParamFloat setDeltaT = maxDeltaT;
		if(mDesiredTimestep>0.0) {
			// explicitly set step time according to max velocity in sim
			setDeltaT = mDesiredTimestep;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," desired step time = "<<setDeltaT, 10);
			mDesiredTimestep = -1.0;
		} else {
			// just use max delta t as current
		}
		
		// and once for init determine minimal delta t by omega max. 
		if((mMinTimestep<0.0) || (mMaxTimestep<0.0)) {
			ParamFloat minDeltaT; 
			ParamFloat maxOmega = mTadapMaxOmega;
			ParamFloat minRelaxTime = 1.0/maxOmega;
			for(int lev=1; lev<mTadapLevels; lev++) {
				// make minRelaxTime larger for each level that exists...
				minRelaxTime = 2.0 * (minRelaxTime-0.5) + 0.5;
			}
			maxOmega = 1.0/minRelaxTime;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," maxOmega="<<maxOmega<<" minRelaxTime="<<minRelaxTime<<" levels="<<mTadapLevels, 1);
			// visc-star for min relax time to calculate min delta ta
			if(mcViscosity.get(time)>0.0) {
				minDeltaT = ((2.0*minRelaxTime-1.0)/6.0) * mCellSize * mCellSize / mcViscosity.get(time);
			} else {
				// visc=0, this is not physical, but might happen
				minDeltaT = 0.0;
			}
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," min delta t = "<<minDeltaT<<" , range = " << (maxDeltaT/minDeltaT) ,1);

			// sim speed + accel shouldnt exceed 0.1?
			mMaxTimestep = maxDeltaT;
			mMinTimestep = minDeltaT;
			// only use once...  
		} 

		setTimestep( setDeltaT ); // set mTimestep to new value
		init = true;	
	}

	// finish init
	if(init) {
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," omega = "<<calculateOmega(0.0)<<", delt="<<mTimestep,1);

		if(checkSeenValues(PARAM_GRAVITY)) {
			ParamFloat forceFactor = (mTimestep *mTimestep)/mCellSize; // only used for printing...
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," gravity force = "<<PRINT_NTLVEC(mcGravity.get(time))<<", scaled with "<<forceFactor<<" to "<<calculateGravity(time),1);
		}
		
		//mExtent = ParamVec( mCellSize*mSizex, mCellSize*mSizey, mCellSize*mSizez );
		//if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," domain extent = "<<PRINT_NTLVEC(mExtent)<<"m , gs:"<<PRINT_VEC(mSizex,mSizey,mSizez)<<" cs:"<<mCellSize,1);
		
		if(!checkSeenValues(PARAM_ANIFRAMETIME)) {
			errFatal("Parametrizer::calculateAllMissingValues"," Warning no ani frame time given!", SIMWORLD_INITERROR);
			setAniFrameTimeChannel( mTimestep );
		} 

		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," ani frame steps = "<<calculateAniStepsPerFrame(mFrameNum)<<" for frame "<<mFrameNum, 1);

		if((checkSeenValues(PARAM_ANISTART))&&(calculateAniStart()>0)) {
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," ani start steps = "<<calculateAniStart()<<" ",1); 
		}
			
		if(! isSimworldOk() ) return false;
		// everything ok
		return true;
	}

	// init failed ... failure:
	errMsg("Parametrizer::calculateAllMissingValues "," invalid configuration!");
	if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues ",DM_WARNING, " values seen:", 1);
	for(int i=0;i<PARAM_NUMIDS;i++) {
		if(checkSeenValues( 1<<i )) {
			if(!silent) debMsgStd("  ",DM_NOTIFY, ParamStrings[i], 1);
		}
	}
	if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues ",DM_WARNING, "values checked but missing:", 1);
	for(int i=0;i<PARAM_NUMIDS;i++) {
		if((!checkSeenValues( 1<<i ))&&
			 ( (valuesChecked&(1<<i))==(1<<i)) ) {
			debMsgStd("  ",DM_IMPORTANT, ParamStrings[i], 1);
		}
	}

	// print values?
	return false;
}


/******************************************************************************
 * init debug functions
 *****************************************************************************/


/*! set kinematic viscosity */
void Parametrizer::setViscosity(ParamFloat set) { 
	mcViscosity = AnimChannel<ParamFloat>(set); 
	seenThis( PARAM_VISCOSITY ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcViscosity set = "<< mcViscosity.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}
void Parametrizer::initViscosityChannel(vector<ParamFloat> val, vector<double> time) { 
	mcViscosity = AnimChannel<ParamFloat>(val,time); 
	seenThis( PARAM_VISCOSITY ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcViscosity initc = "<< mcViscosity.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}

/*! set the external force */
void Parametrizer::setGravity(ParamFloat setx, ParamFloat sety, ParamFloat setz) { 
	mcGravity = AnimChannel<ParamVec>(ParamVec(setx,sety,setz)); 
	seenThis( PARAM_GRAVITY ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcGravity set = "<< mcGravity.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}
void Parametrizer::setGravity(ParamVec set) { 
	mcGravity = AnimChannel<ParamVec>(set); 
	seenThis( PARAM_GRAVITY ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcGravity set = "<< mcGravity.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}
void Parametrizer::initGravityChannel(vector<ParamVec> val, vector<double> time) { 
	mcGravity = AnimChannel<ParamVec>(val,time); 
	seenThis( PARAM_GRAVITY ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcGravity initc = "<< mcGravity.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}

/*! set time of an animation frame (renderer)  */
void Parametrizer::setAniFrameTimeChannel(ParamFloat set) { 
	mcAniFrameTime = AnimChannel<ParamFloat>(set); 
	seenThis( PARAM_ANIFRAMETIME ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcAniFrameTime set = "<< mcAniFrameTime.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}
void Parametrizer::initAniFrameTimeChannel(vector<ParamFloat> val, vector<double> time) { 
	mcAniFrameTime = AnimChannel<ParamFloat>(val,time); 
	seenThis( PARAM_ANIFRAMETIME ); 
#if DEBUG_PARAMCHANNELS>0
	{ errMsg("DebugChannels","Parametrizer::mcAniFrameTime initc = "<< mcAniFrameTime.printChannel() ); }
#endif // DEBUG_PARAMCHANNELS>0
}

// OLD interface stuff
// reactivate at some point?

		/*! surface tension, [kg/s^2] */
		//ParamFloat mSurfaceTension;
		/*! set starting time of the animation (renderer) */
		//void setSurfaceTension(ParamFloat set) { mSurfaceTension = set; seenThis( PARAM_SURFACETENSION ); }
		/*! get starting time of the animation (renderer) */
		//ParamFloat getSurfaceTension( void )   { return mSurfaceTension; }
		/*if((checkSeenValues(PARAM_SURFACETENSION))&&(mSurfaceTension>0.0)) {
			ParamFloat massDelta = 1.0;
			ParamFloat densityStar = 1.0;
			massDelta = mDensity / densityStar *mCellSize*mCellSize*mCellSize;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," massDelta = "<<massDelta, 10);

			mSurfaceTension = mSurfaceTension*mTimestep*mTimestep/massDelta;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," surface tension = "<<mSurfaceTension<<" ",1);
		} // */

// probably just delete:

		/*! reynolds number (calculated from domain length and max. speed [dimensionless] */
		//ParamFloat mReynolds;

		/*! set relaxation time */
		//void setRelaxTime(ParamFloat set) { mRelaxTime = set; seenThis( PARAM_RELAXTIME ); }
		/*! get relaxation time */
		//ParamFloat getRelaxTime( void )   { return mRelaxTime; }
		/*! set reynolds number */
		//void setReynolds(ParamFloat set) { mReynolds = set; seenThis( PARAM_REYNOLDS ); }
		/*! get reynolds number */
		//ParamFloat getReynolds( void )   { return mReynolds; }

		// calculate reynolds number
		/*if(mViscosity>0.0) {
			ParamFloat maxSpeed  = 1.0/6.0; // for rough reynolds approx
			ParamFloat reynoldsApprox = -1.0;
			ParamFloat gridSpeed = (maxSpeed*mCellSize/mTimestep);
			reynoldsApprox = (mDomainSize*gridSpeed) / mViscosity;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," reynolds number (D="<<mDomainSize<<", assuming V="<<gridSpeed<<")= "<<reynoldsApprox<<" ", 1);
		} // */

	//? mRelaxTime = mpAttrs->readFloat("p_relaxtime",mRelaxTime, "Parametrizer","mRelaxTime", false); 
	//if(getAttributeList()->exists("p_relaxtime")) seenThis( PARAM_RELAXTIME );
	//? mReynolds = mpAttrs->readFloat("p_reynolds",mReynolds, "Parametrizer","mReynolds", false); 
	//if(getAttributeList()->exists("p_reynolds")) seenThis( PARAM_REYNOLDS );

	//mViscosity = mpAttrs->readFloat("p_viscosity",mViscosity, "Parametrizer","mViscosity", false); 
	//if(getAttributeList()->exists("p_viscosity") || (!mcViscosity.isInited()) ) { }
	//if(getAttributeList()->exists("p_viscosity")) 


		//ParamFloat viscStar = calculateLatticeViscosity(time);
		//RelaxTime = (6.0 * viscStar + 1) * 0.5;



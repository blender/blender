/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Parameter calculator for the LBM Solver class
 *
 *****************************************************************************/

#include <sstream>
#include "parametrizer.h"

/*! param seen debug string array */
char *ParamStrings[] = {
	"RelaxTime",
	"Reynolds",
	"Viscosity",
	"SoundSpeed",
	"DomainSize",
	"GravityForce",
	"TimeLength",
	"StepTime",
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
	mSetupType("caro"),
  mRelaxTime( 1.0 ), mReynolds( 0.0 ),
	mViscosity( 8.94e-7 ), mSoundSpeed( 1500 ),
	mDomainSize( 0.1 ), mCellSize( 0.01 ),
	mGravity(0.0, 0.0, 0.0), mLatticeGravity(0.0, 0.0, 0.0),
	mStepTime(0.01), mDesiredStepTime(-1.0),
	mSizex(50), mSizey(50), mSizez(50),
	mTimeFactor( 1.0 ),
	//mAniFrames(0), 
	mAniFrameTime(0.0), mAniStart(0.0),
	mExtent(1.0, 1.0, 1.0), mSurfaceTension( 0.0 ),
	mDensity(1000.0), mGStar(0.0001), mFluidVolumeHeight(0.0),
	mMaxSpeed(0.0), mSimulationMaxSpeed(0.0),
	mTadapMaxOmega(1.95), mTadapMaxSpeed(0.1), mTadapLevels(1),
	mSeenValues( 0 ), mCalculatedValues( 0 )
	//mActive( false )
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

	//mActive = mpAttrs->readBool("p_active",mActive, "Parametrizer","mActive", false);
	mSetupType = mpAttrs->readString("p_setup",mSetupType, "Parametrizer","mSetupType", false); 
	mRelaxTime = mpAttrs->readFloat("p_relaxtime",mRelaxTime, "Parametrizer","mRelaxTime", false); 
	if(getAttributeList()->exists("p_relaxtime")) seenThis( PARAM_RELAXTIME );

	mReynolds = mpAttrs->readFloat("p_reynolds",mReynolds, "Parametrizer","mReynolds", false); 
	if(getAttributeList()->exists("p_reynolds")) seenThis( PARAM_REYNOLDS );

	mViscosity = mpAttrs->readFloat("p_viscosity",mViscosity, "Parametrizer","mViscosity", false); 
	if(getAttributeList()->exists("p_viscosity")) seenThis( PARAM_VISCOSITY );

	mSoundSpeed = mpAttrs->readFloat("p_soundspeed",mSoundSpeed, "Parametrizer","mSoundSpeed", false); 
	if(getAttributeList()->exists("p_soundspeed")) seenThis( PARAM_SOUNDSPEED );

	mDomainSize = mpAttrs->readFloat("p_domainsize",mDomainSize, "Parametrizer","mDomainSize", false); 
	if(getAttributeList()->exists("p_domainsize")) seenThis( PARAM_DOMAINSIZE );
	if(mDomainSize<=0.0) {
		errMsg("Parametrizer::parseAttrList","Invalid real world domain size:"<<mAniFrameTime<<", resetting to 0.1");
		mDomainSize = 0.1;
	}

	mGravity = mpAttrs->readVec3d("p_gravity",mGravity, "Parametrizer","mGravity", false); 
	if(getAttributeList()->exists("p_gravity")) seenThis( PARAM_GRAVITY );

	mStepTime = mpAttrs->readFloat("p_steptime",mStepTime, "Parametrizer","mStepTime", false); 
	if(getAttributeList()->exists("p_steptime")) seenThis( PARAM_STEPTIME );

	mTimeFactor = mpAttrs->readFloat("p_timefactor",mTimeFactor, "Parametrizer","mTimeFactor", false); 
	if(getAttributeList()->exists("p_timefactor")) seenThis( PARAM_TIMEFACTOR );

	mAniFrameTime = mpAttrs->readFloat("p_aniframetime",mAniFrameTime, "Parametrizer","mAniFrameTime", false); 
	if(getAttributeList()->exists("p_aniframetime")) seenThis( PARAM_ANIFRAMETIME );
	if(mAniFrameTime<=0.0) {
		errMsg("Parametrizer::parseAttrList","Invalid frame time:"<<mAniFrameTime<<", resetting to 0.0001");
		mAniFrameTime = 0.0001;
	}

	mAniStart = mpAttrs->readFloat("p_anistart",mAniStart, "Parametrizer","mAniStart", false); 
	if(getAttributeList()->exists("p_anistart")) seenThis( PARAM_ANISTART );
	if(mAniStart<0.0) {
		errMsg("Parametrizer::parseAttrList","Invalid start time:"<<mAniStart<<", resetting to 0.0");
		mAniStart = 0.0;
	}

	mSurfaceTension = mpAttrs->readFloat("p_surfacetension",mSurfaceTension, "Parametrizer","mSurfaceTension", false); 
	if(getAttributeList()->exists("p_surfacetension")) seenThis( PARAM_SURFACETENSION );

	mDensity = mpAttrs->readFloat("p_density",mDensity, "Parametrizer","mDensity", false); 
	if(getAttributeList()->exists("p_density")) seenThis( PARAM_DENSITY );

	mCellSize = mpAttrs->readFloat("p_cellsize",mCellSize, "Parametrizer","mCellSize", false); 
	if(getAttributeList()->exists("p_cellsize")) seenThis( PARAM_CELLSIZE );

	mGStar = mpAttrs->readFloat("p_gstar",mGStar, "Parametrizer","mGStar", false); 
	if(getAttributeList()->exists("p_gstar")) seenThis( PARAM_GSTAR );

	mNormalizedGStar = mpAttrs->readFloat("p_normgstar",mNormalizedGStar, "Parametrizer","mNormalizedGStar", false); 
	if(getAttributeList()->exists("p_normgstar")) seenThis( PARAM_NORMALIZEDGSTAR );

	mMaxSpeed = mpAttrs->readFloat("p_maxspeed",mMaxSpeed, "Parametrizer","mMaxSpeed", false); 
	if(getAttributeList()->exists("p_maxspeed")) seenThis( PARAM_MAXSPEED );

	mTadapMaxOmega = mpAttrs->readFloat("p_tadapmaxomega",mTadapMaxOmega, "Parametrizer","mTadapMaxOmega", false); 
	mTadapMaxSpeed = mpAttrs->readFloat("p_tadapmaxspeed",mTadapMaxSpeed, "Parametrizer","mTadapMaxSpeed", false); 
}

/******************************************************************************
 * scale a given speed vector in m/s to lattice values 
 *****************************************************************************/
ParamVec Parametrizer::calculateAddForce(ParamVec vec, string usage)
{
	ParamVec ret = vec * (mStepTime*mStepTime) /mCellSize;
	debMsgStd("Parametrizer::calculateVector", DM_MSG, "scaled vector = "<<ret<<" for '"<<usage<<"', org = "<<vec<<" dt="<<mStepTime ,10);
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
	ParamFloat cellSize = 1.0 / (ParamFloat)maxsize;
	return cellSize;
}


/*****************************************************************************/
/* simple calulation functions */
/*****************************************************************************/

/*! get omega for LBM */
ParamFloat Parametrizer::calculateOmega( void ) { 
	//return (mTimeFactor/mRelaxTime); 
	return (1.0/mRelaxTime); 
}

/*! get no. of timesteps for LBM */
//int calculateNoOfSteps( void ) { 
int Parametrizer::calculateNoOfSteps( ParamFloat timelen ) { 
	return (int)(timelen/mStepTime); 
}

/*! get external force x component */
ParamVec Parametrizer::calculateGravity( void ) { 
	return mLatticeGravity; 
}

/*! get no of steps for the given length in seconds */
int Parametrizer::calculateStepsForSecs( ParamFloat s ) { 
	return (int)(s/mStepTime); 
}

/*! get start time of animation */
int Parametrizer::calculateAniStart( void )   { 
	return (int)(mAniStart/mStepTime); 
}

/*! get no of steps for a single animation frame */
int Parametrizer::calculateAniStepsPerFrame( void )   { 
	if(!checkSeenValues(PARAM_ANIFRAMETIME)) {
		errFatal("Parametrizer::calculateAniStepsPerFrame", " Missing ani frame time argument!", SIMWORLD_INITERROR);
		return 1;
	}
	return (int)(mAniFrameTime/mStepTime); 
}

/*! get extent of the domain = (1,1,1) if parametrizer not used, (x,y,z) [m] otherwise */
ParamVec Parametrizer::calculateExtent( void ) { 
	return mExtent; 
}

/*! get (scaled) surface tension */
ParamFloat Parametrizer::calculateSurfaceTension( void ) { 
	return mSurfaceTension; 
}

/*! calculate lattice velocity from real world value [m/s] */
ParamVec Parametrizer::calculateLattVelocityFromRw( ParamVec ivel ) { 
	ParamVec velvec = ivel;
	velvec /= mCellSize;
	velvec *= mStepTime;
	return velvec; 
}
/*! calculate real world [m/s] velocity from lattice value */
ParamVec Parametrizer::calculateRwVelocityFromLatt( ParamVec ivel ) { 
	ParamVec velvec = ivel;
	velvec *= mCellSize;
	velvec /= mStepTime;
	return velvec; 
}


/*! get the length of a single time step */
// explicity scaled by time factor for refinement 
// testing purposes (e.g. fsgr solver)
// not working... done manually in solver
ParamFloat Parametrizer::getStepTime( void ) { 
	//return mTimeFactor * mStepTime; 
	return mStepTime; 
}

/*! calculate the lattice viscosity */
ParamFloat Parametrizer::calculateLatticeViscosity( void ) { 
	// check seen values
	int reqValues = PARAM_VISCOSITY | PARAM_STEPTIME; // |PARAM_CELLSIZE |  PARAM_GRAVITY;
	if(!checkSeenValues( reqValues ) ){
		errMsg("Parametrizer::calculateLatticeViscosity"," Missing arguments!");
	}
	ParamFloat viscStar = mViscosity * mStepTime / (mCellSize*mCellSize);
	return viscStar; 
}

/*! get g star value with fhvol calculations */
ParamFloat Parametrizer::getCurrentGStar( void ) {
	ParamFloat gStar = mGStar; // check? TODO get from mNormalizedGStar?
	if(mFluidVolumeHeight>0.0) {
		gStar = mGStar/mFluidVolumeHeight;
	}
	return gStar;
}

/******************************************************************************
 * function that tries to calculate all the missing values from the given ones
 * prints errors and returns false if thats not possible 
 *****************************************************************************/
bool Parametrizer::calculateAllMissingValues( bool silent )
{
	bool init = false;  // did we init correctly?
	int  valuesChecked = 0;
	int reqValues;

	// are we active anyway?
	//if(!mActive) {
		// not active - so there's nothing to calculate
		//return true;
	//}

	// we always need the sizes
	reqValues = PARAM_SIZE;
	valuesChecked |= reqValues;
	if(!checkSeenValues(reqValues)) {
		errMsg("Parametrizer::calculateAllMissingValues"," Missing size argument!");
		return false;
	}

	if(checkSeenValues(PARAM_CELLSIZE)) {
		errMsg("Parametrizer::calculateAllMissingValues"," Dont explicitly set cell size (use domain size instead)");
		return false;
	}
	if(!checkSeenValues(PARAM_DOMAINSIZE)) {
		errMsg("Parametrizer::calculateAllMissingValues"," Missing domain size argument!");
		return false;
	}
	int maxsize = mSizex; // get max size
	if(mSizey>maxsize) maxsize = mSizey;
	if(mSizez>maxsize) maxsize = mSizez;
	mCellSize = ( mDomainSize * calculateCellSize() ); // sets mCellSize
	if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," max domain resolution="<<(maxsize)<<" cells , cellsize="<<mCellSize ,10);

			
	/* Carolin init , see DA for details */
	//ParamFloat viscMax = 0.7600;    // max lattice viscosity
	//ParamFloat viscMin = 0.0033;  // min lattice viscosity
	ParamFloat maxDeltaT = 0.0;
	ParamFloat maxSpeed = 0.1; // for reynolds approx

	/* normalized gstar init */
	reqValues = PARAM_NORMALIZEDGSTAR;
	valuesChecked |= reqValues;
	if(checkSeenValues( reqValues ) ){
		//if(checkSeenValues( PARAM_GSTAR ) ){ if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_WARNING," g star value override by normalizedGStar!",1); }
		mGStar = mNormalizedGStar/maxsize;
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," g star set to "<<mGStar<<" from normalizedGStar="<<mNormalizedGStar ,1);
		seenThis(PARAM_GSTAR);
	}

	reqValues = PARAM_GSTAR | PARAM_VISCOSITY;
	if((checkSeenValues(PARAM_SURFACETENSION))) reqValues |= PARAM_DENSITY; // surface tension optional now...
	valuesChecked |= reqValues;
	if(checkSeenValues( reqValues ) ){
		const ParamFloat gstarReset = 0.0005;
		if(getCurrentGStar()<=0.0) {
			errMsg("Parametrizer::calculateAllMissingValues","Invalid Gstar: "<<getCurrentGStar()<<" (set to "<<mGStar<<") ... resetting to "<<gstarReset);
			mGStar = gstarReset;
		}

		ParamFloat gStar = getCurrentGStar();
		if(mFluidVolumeHeight>0.0) {
			debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," height"<<mFluidVolumeHeight<<" resGStar = "<<gStar, 10);
		}
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," g star = "<<gStar, 10);

		if(mSetupType=="caro") {
			if(!checkSeenValues(PARAM_GRAVITY)) {
				errMsg("Parametrizer::calculateAllMissingValues","Setup type '"<<mSetupType<<"' requires gravity force!");
				goto failure;
			}
			ParamFloat forceStrength = norm(mGravity);
			if(forceStrength<=0) {
				errMsg("Parametrizer::calculateAllMissingValues"," Init failed - forceStrength = "<<forceStrength);
				goto failure;
			}

			// determine max. delta density per timestep trough gravity force
			maxDeltaT = sqrt( gStar*mCellSize/forceStrength );
		} else if(mSetupType=="maxspeed") {
			// determine max. delta t from maximum speed (explicity set)
			if((!checkSeenValues(PARAM_MAXSPEED))||(mMaxSpeed<=0.0)) {
				errMsg("Parametrizer::calculateAllMissingValues","Setup type '"<<mSetupType<<"' requires maximum speed ("<<mMaxSpeed<<") !");
				goto failure;
			}
			ParamFloat maxLatticeSpeed = 0.0333333; //?
			maxDeltaT = ( maxLatticeSpeed * mCellSize) / mMaxSpeed;
			maxSpeed = mMaxSpeed;
		} else if(mSetupType=="falling") {
			// determine max. delta t from maximum speed that can be caused by falling through the domain
			errMsg("Parametrizer::calculateAllMissingValues"," NYI setup falling");
		} else {
			errMsg("Parametrizer::calculateAllMissingValues","Setup type '"<<mSetupType<<"' unknown!");
			goto failure;
		}

		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," targeted step time = "<<maxDeltaT, 10);

		ParamFloat viscStarFac = mViscosity/(mCellSize*mCellSize);
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," viscStarFac = "<<viscStarFac, 10);

		// FIXME remove for LES?
		//if( (viscStarFac*maxDeltaT>=viscMin) && (viscStarFac*maxDeltaT<=viscMax) ) {
			//if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," delta t: "<<viscMin<<" <? "<<maxDeltaT*viscStarFac<<" <? "<<viscMax, 1);
		//} else {
			//if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_WARNING," delta t not in valid range: "<<viscMin<<" <? "<<maxDeltaT*viscStarFac<<" <? "<<viscMax, 1);
		//}

		// time step adaptivty, only for caro with max sim speed
		ParamFloat setDeltaT = maxDeltaT;
		if(mDesiredStepTime>0.0) {
			setDeltaT = mDesiredStepTime;
			mDesiredStepTime = -1.0;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," desired step time = "<<setDeltaT, 10);
		} else if((mSetupType=="caro") && (checkSeenValues( PARAM_SIMMAXSPEED )) ) {
			// determine minimal delta t by omega max.
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
			if(mViscosity>0.0) {
				minDeltaT = ((2.0*minRelaxTime-1.0)/6.0) * mCellSize * mCellSize / mViscosity;
			} else {
				// visc=0, this is not physical, but might happen
				minDeltaT = 0.0;
			}
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," min delta t = "<<minDeltaT<<" , range = " << (maxDeltaT/minDeltaT) ,1);

			// sim speed + accel shouldnt exceed 0.1?
			//if(mSimulationMaxSpeed + norm(mGravity*)) { ParamFloat nextmax = 0.1-mSimulationMaxSpeed }
			mMaxStepTime = maxDeltaT;
			mMinStepTime = minDeltaT;
			// only use once...
		} else {
			debMsgStd("Parametrizer::calculateAllMissingValues",DM_WARNING,"Warning - setup type set to '"<<mSetupType<<"' ",1);
			mMaxStepTime = mMinStepTime = setDeltaT;
		}

		setStepTime( setDeltaT ); // set mStepTime to new value

		//ParamFloat viscStar = mViscosity * mStepTime / (mCellSize*mCellSize);
		ParamFloat viscStar = calculateLatticeViscosity();
		mRelaxTime = (6.0 * viscStar + 1) * 0.5;
		init = true;	

	}

	// finish init
	if(init) {
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," omega = "<<calculateOmega()<<", relax time = "<<mRelaxTime<<", delt="<<mStepTime,1);
		//debMsgStd("Parametrizer::calculateAllMissingValues: lbm steps = "<<calculateNoOfSteps()<<" ",1);

		if(checkSeenValues(PARAM_GRAVITY)) {
			ParamFloat forceFactor = (mStepTime *mStepTime)/mCellSize;
			//if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," given force = "<<PRINT_NTLVEC(mGravity),1);
			mLatticeGravity = mGravity * forceFactor;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," gravity force = "<<PRINT_NTLVEC(mGravity)<<", scaled with "<<forceFactor<<" to "<<mLatticeGravity,1);
		}

		if((checkSeenValues(PARAM_SURFACETENSION))&&(mSurfaceTension>0.0)) {
			ParamFloat massDelta = 1.0;
			ParamFloat densityStar = 1.0;
			massDelta = mDensity / densityStar *mCellSize*mCellSize*mCellSize;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," massDelta = "<<massDelta, 10);

			mSurfaceTension = mSurfaceTension*mStepTime*mStepTime/massDelta;
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," surface tension = "<<mSurfaceTension<<" ",1);
		}
		
		mExtent = ParamVec( mCellSize*mSizex, mCellSize*mSizey, mCellSize*mSizez );
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," domain extent = "<<PRINT_NTLVEC(mExtent)<<"m ",1);
		
		if(!checkSeenValues(PARAM_ANIFRAMETIME)) {
			errFatal("Parametrizer::calculateAllMissingValues"," Warning no ani frame time given!", SIMWORLD_INITERROR);
			mAniFrameTime = mStepTime;
		} 
		//mAniFrameTime = mAniFrames * mStepTime;
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," ani frame steps = "<<calculateAniStepsPerFrame()<<" ", 1);

		if((checkSeenValues(PARAM_ANISTART))&&(calculateAniStart()>0)) {
			if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," ani start steps = "<<calculateAniStart()<<" ",1); 
		}

		// calculate reynolds number
		ParamFloat reynoldsApprox = -1.0;
		ParamFloat gridSpeed = (maxSpeed*mCellSize/mStepTime);
		reynoldsApprox = (mDomainSize*gridSpeed) / mViscosity;
		if(!silent) debMsgStd("Parametrizer::calculateAllMissingValues",DM_MSG," reynolds number (D="<<mDomainSize<<", assuming V="<<gridSpeed<<")= "<<reynoldsApprox<<" ", 1);

		// everything ok
		return true;
	}

failure:
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








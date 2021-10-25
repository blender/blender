/** \file elbeem/intern/solver_control.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - the visual lattice boltzmann freesurface simulator
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.  
 *
 * Copyright 2003-2008 Nils Thuerey 
 *
 * control extensions
 *
 *****************************************************************************/
#include "solver_class.h"
#include "solver_relax.h"
#include "particletracer.h"

#include "solver_control.h"

#include "controlparticles.h"

#include "elbeem.h"

#include "ntl_geometrymodel.h"

/******************************************************************************
 * LbmControlData control set
 *****************************************************************************/

LbmControlSet::LbmControlSet() :
	mCparts(NULL), mCpmotion(NULL), mContrPartFile(""), mCpmotionFile(""),
	mcForceAtt(0.), mcForceVel(0.), mcForceMaxd(0.), 
	mcRadiusAtt(0.), mcRadiusVel(0.), mcRadiusMind(0.), mcRadiusMaxd(0.), 
	mcCpScale(1.), mcCpOffset(0.)
{
}
LbmControlSet::~LbmControlSet() {
	if(mCparts) delete mCparts;
	if(mCpmotion) delete mCpmotion;
}
void LbmControlSet::initCparts() {
	mCparts = new ControlParticles();
	mCpmotion = new ControlParticles();
}



/******************************************************************************
 * LbmControlData control
 *****************************************************************************/

LbmControlData::LbmControlData() : 
	mSetForceStrength(0.),
	mCons(), 
	mCpUpdateInterval(8), // DG: was 16 --> causes problems (big sphere after some time), unstable
	mCpOutfile(""), 
	mCpForces(), mCpKernel(), mMdKernel(),
	mDiffVelCon(1.),
	mDebugCpscale(0.), 
	mDebugVelScale(0.), 
	mDebugCompavScale(0.), 
	mDebugAttScale(0.), 
	mDebugMaxdScale(0.),
	mDebugAvgVelScale(0.)
{
}

LbmControlData::~LbmControlData() 
{
	while (!mCons.empty()) {
		delete mCons.back();  mCons.pop_back();
	}
}


void LbmControlData::parseControldataAttrList(AttributeList *attr) {
	// controlpart vars
	mSetForceStrength = attr->readFloat("tforcestrength", mSetForceStrength,"LbmControlData", "mSetForceStrength", false);
	//errMsg("tforcestrength set to "," "<<mSetForceStrength);
	mCpUpdateInterval = attr->readInt("controlparticle_updateinterval", mCpUpdateInterval,"LbmControlData","mCpUpdateInterval", false);
	// tracer output file
	mCpOutfile = attr->readString("controlparticle_outfile",mCpOutfile,"LbmControlData","mCpOutfile", false);
	if(getenv("ELBEEM_CPOUTFILE")) {
		string outfile(getenv("ELBEEM_CPOUTFILE"));
		mCpOutfile = outfile;
		debMsgStd("LbmControlData::parseAttrList",DM_NOTIFY,"Using envvar ELBEEM_CPOUTFILE to set mCpOutfile to "<<outfile<<","<<mCpOutfile,7);
	}

	for(int cpii=0; cpii<10; cpii++) {
		string suffix("");
		//if(cpii>0)
		{  suffix = string("0"); suffix[0]+=cpii; }
		LbmControlSet *cset;
		cset = new LbmControlSet();
		cset->initCparts();

		cset->mContrPartFile = attr->readString("controlparticle"+suffix+"_file",cset->mContrPartFile,"LbmControlData","cset->mContrPartFile", false);
		if((cpii==0) && (getenv("ELBEEM_CPINFILE")) ) {
			string infile(getenv("ELBEEM_CPINFILE"));
			cset->mContrPartFile = infile;
			debMsgStd("LbmControlData::parseAttrList",DM_NOTIFY,"Using envvar ELBEEM_CPINFILE to set mContrPartFile to "<<infile<<","<<cset->mContrPartFile,7);
		}

		LbmFloat cpvort=0.;
		cset->mcRadiusAtt =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_radiusatt", 0., "LbmControlData","mcRadiusAtt" );
		cset->mcRadiusVel =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_radiusvel", 0., "LbmControlData","mcRadiusVel" );
		cset->mcRadiusVel =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_radiusvel", 0., "LbmControlData","mcRadiusVel" );
		cset->mCparts->setRadiusAtt(cset->mcRadiusAtt.get(0.));
		cset->mCparts->setRadiusVel(cset->mcRadiusVel.get(0.));

		// WARNING currently only for first set
		//if(cpii==0) {
		cset->mcForceAtt  =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_attraction", 0. , "LbmControlData","cset->mcForceAtt", false);
		cset->mcForceVel  =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_velocity",   0. , "LbmControlData","mcForceVel", false);
		cset->mcForceMaxd =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_maxdist",    0. , "LbmControlData","mcForceMaxd", false);
		cset->mCparts->setInfluenceAttraction(cset->mcForceAtt.get(0.) );
		// warning - stores temprorarily, value converted to dt dep. factor
		cset->mCparts->setInfluenceVelocity(cset->mcForceVel.get(0.) , 0.01 ); // dummy dt
		cset->mCparts->setInfluenceMaxdist(cset->mcForceMaxd.get(0.) );
		cpvort =  attr->readFloat("controlparticle"+suffix+"_vorticity",   cpvort, "LbmControlData","cpvort", false);
		cset->mCparts->setInfluenceTangential(cpvort);
			
		cset->mcRadiusMind =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_radiusmin", cset->mcRadiusMind.get(0.), "LbmControlData","mcRadiusMind", false);
		cset->mcRadiusMaxd =  attr->readChannelSinglePrecFloat("controlparticle"+suffix+"_radiusmax", cset->mcRadiusMind.get(0.), "LbmControlData","mcRadiusMaxd", false);
		cset->mCparts->setRadiusMinMaxd(cset->mcRadiusMind.get(0.));
		cset->mCparts->setRadiusMaxd(cset->mcRadiusMaxd.get(0.));
		//}

		// now local...
		//LbmVec cpOffset(0.), cpScale(1.);
		LbmFloat cpTimescale = 1.;
		string cpMirroring("");

		//cset->mcCpOffset = attr->readChannelVec3f("controlparticle"+suffix+"_offset", ntlVec3f(0.),"LbmControlData","mcCpOffset", false);
		//cset->mcCpScale =  attr->readChannelVec3f("controlparticle"+suffix+"_scale",  ntlVec3f(1.), "LbmControlData","mcCpScale", false);
		cset->mcCpOffset = attr->readChannelVec3f("controlparticle"+suffix+"_offset", ntlVec3f(0.),"LbmControlData","mcCpOffset", false);
		cset->mcCpScale =  attr->readChannelVec3f("controlparticle"+suffix+"_scale",  ntlVec3f(1.), "LbmControlData","mcCpScale", false);
		cpTimescale =  attr->readFloat("controlparticle"+suffix+"_timescale",  cpTimescale, "LbmControlData","cpTimescale", false);
		cpMirroring =  attr->readString("controlparticle"+suffix+"_mirror",  cpMirroring, "LbmControlData","cpMirroring", false);

		LbmFloat cpsWidth = cset->mCparts->getCPSWith();
		cpsWidth =  attr->readFloat("controlparticle"+suffix+"_cpswidth",  cpsWidth, "LbmControlData","cpsWidth", false);
		LbmFloat cpsDt = cset->mCparts->getCPSTimestep();
		cpsDt =  attr->readFloat("controlparticle"+suffix+"_cpstimestep",  cpsDt, "LbmControlData","cpsDt", false);
		LbmFloat cpsTstart = cset->mCparts->getCPSTimeStart();
		cpsTstart =  attr->readFloat("controlparticle"+suffix+"_cpststart",  cpsTstart, "LbmControlData","cpsTstart", false);
		LbmFloat cpsTend = cset->mCparts->getCPSTimeEnd();
		cpsTend =  attr->readFloat("controlparticle"+suffix+"_cpstend",  cpsTend, "LbmControlData","cpsTend", false);
		LbmFloat cpsMvmfac = cset->mCparts->getCPSMvmWeightFac();
		cpsMvmfac =  attr->readFloat("controlparticle"+suffix+"_cpsmvmfac",  cpsMvmfac, "LbmControlData","cpsMvmfac", false);
		cset->mCparts->setCPSWith(cpsWidth);
		cset->mCparts->setCPSTimestep(cpsDt);
		cset->mCparts->setCPSTimeStart(cpsTstart);
		cset->mCparts->setCPSTimeEnd(cpsTend);
		cset->mCparts->setCPSMvmWeightFac(cpsMvmfac);

		cset->mCparts->setOffset( vec2L(cset->mcCpOffset.get(0.)) );
		cset->mCparts->setScale( vec2L(cset->mcCpScale.get(0.)) );
		cset->mCparts->setInitTimeScale( cpTimescale );
		cset->mCparts->setInitMirror( cpMirroring );

		int mDebugInit = 0;
		mDebugInit = attr->readInt("controlparticle"+suffix+"_debuginit", mDebugInit,"LbmControlData","mDebugInit", false);
		cset->mCparts->setDebugInit(mDebugInit);

		// motion particle settings
		LbmVec mcpOffset(0.), mcpScale(1.);
		LbmFloat mcpTimescale = 1.;
		string mcpMirroring("");

		cset->mCpmotionFile = attr->readString("cpmotion"+suffix+"_file",cset->mCpmotionFile,"LbmControlData","mCpmotionFile", false);
		mcpTimescale =  attr->readFloat("cpmotion"+suffix+"_timescale",  mcpTimescale, "LbmControlData","mcpTimescale", false);
		mcpMirroring =  attr->readString("cpmotion"+suffix+"_mirror",  mcpMirroring, "LbmControlData","mcpMirroring", false);
		mcpOffset = vec2L( attr->readVec3d("cpmotion"+suffix+"_offset", vec2P(mcpOffset),"LbmControlData","cpOffset", false) );
		mcpScale =  vec2L( attr->readVec3d("cpmotion"+suffix+"_scale",  vec2P(mcpScale), "LbmControlData","cpScale", false) );

		cset->mCpmotion->setOffset( vec2L(mcpOffset) );
		cset->mCpmotion->setScale( vec2L(mcpScale) );
		cset->mCpmotion->setInitTimeScale( mcpTimescale );
		cset->mCpmotion->setInitMirror( mcpMirroring );

		if(cset->mContrPartFile.length()>1) {
			errMsg("LbmControlData","Using control particle set "<<cpii<<" file:"<<cset->mContrPartFile<<" cpmfile:"<<cset->mCpmotionFile<<" mirr:'"<<cset->mCpmotion->getInitMirror()<<"' " );
			mCons.push_back( cset );
		} else {
			delete cset;
		}
	}

	// debug, testing - make sure theres at least an empty set
	if(mCons.size()<1) {
		mCons.push_back( new LbmControlSet() );
		mCons[0]->initCparts();
	}

	// take from first set
	for(int cpii=1; cpii<(int)mCons.size(); cpii++) {
		mCons[cpii]->mCparts->setRadiusMinMaxd( mCons[0]->mCparts->getRadiusMinMaxd() );
		mCons[cpii]->mCparts->setRadiusMaxd(    mCons[0]->mCparts->getRadiusMaxd() );
		mCons[cpii]->mCparts->setInfluenceAttraction( mCons[0]->mCparts->getInfluenceAttraction() );
		mCons[cpii]->mCparts->setInfluenceTangential( mCons[0]->mCparts->getInfluenceTangential() );
		mCons[cpii]->mCparts->setInfluenceVelocity( mCons[0]->mCparts->getInfluenceVelocity() , 0.01 ); // dummy dt
		mCons[cpii]->mCparts->setInfluenceMaxdist( mCons[0]->mCparts->getInfluenceMaxdist() );
	}
	
	// invert for usage in relax macro
	mDiffVelCon =  1.-attr->readFloat("cpdiffvelcon",  mDiffVelCon, "LbmControlData","mDiffVelCon", false);

	mDebugCpscale     =  attr->readFloat("cpdebug_cpscale",  mDebugCpscale, "LbmControlData","mDebugCpscale", false);
	mDebugMaxdScale   =  attr->readFloat("cpdebug_maxdscale",  mDebugMaxdScale, "LbmControlData","mDebugMaxdScale", false);
	mDebugAttScale    =  attr->readFloat("cpdebug_attscale",  mDebugAttScale, "LbmControlData","mDebugAttScale", false);
	mDebugVelScale    =  attr->readFloat("cpdebug_velscale",  mDebugVelScale, "LbmControlData","mDebugVelScale", false);
	mDebugCompavScale =  attr->readFloat("cpdebug_compavscale",  mDebugCompavScale, "LbmControlData","mDebugCompavScale", false);
	mDebugAvgVelScale =  attr->readFloat("cpdebug_avgvelsc",  mDebugAvgVelScale, "LbmControlData","mDebugAvgVelScale", false);
}


void 
LbmFsgrSolver::initCpdata()
{
	// enable for cps via env. vars
	//if( (getenv("ELBEEM_CPINFILE")) || (getenv("ELBEEM_CPOUTFILE")) ){ mUseTestdata=1; }

	
	// manually switch on! if this is zero, nothing is done...
	mpControl->mSetForceStrength = this->mTForceStrength = 1.;
	while (!mpControl->mCons.empty()) {
		delete mpControl->mCons.back();  mpControl->mCons.pop_back();
	}

	
	// init all control fluid objects
	int numobjs = (int)(mpGiObjects->size());
	for(int o=0; o<numobjs; o++) {
		ntlGeometryObjModel *obj = (ntlGeometryObjModel *)(*mpGiObjects)[o];
		if(obj->getGeoInitType() & FGI_CONTROL) {
			// add new control set per object
			LbmControlSet *cset;

			cset = new LbmControlSet();
			cset->initCparts();
	
			// dont load any file
			cset->mContrPartFile = string("");

			cset->mcForceAtt = obj->getCpsAttrFStr();
			cset->mcRadiusAtt = obj->getCpsAttrFRad();
			cset->mcForceVel = obj->getCpsVelFStr();
			cset->mcRadiusVel = obj->getCpsVelFRad();

			cset->mCparts->setCPSTimeStart(obj->getCpsTimeStart());
			cset->mCparts->setCPSTimeEnd(obj->getCpsTimeEnd());
			
			if(obj->getCpsQuality() > LBM_EPSILON)
				cset->mCparts->setCPSWith(1.0 / obj->getCpsQuality());
			
			// this value can be left at 0.5:
			cset->mCparts->setCPSMvmWeightFac(0.5);

			mpControl->mCons.push_back( cset );
			mpControl->mCons[mpControl->mCons.size()-1]->mCparts->initFromObject(obj);
		}
	}
	
	// NT blender integration manual test setup
	if(0) {
		// manually switch on! if this is zero, nothing is done...
		mpControl->mSetForceStrength = this->mTForceStrength = 1.;
		while (!mpControl->mCons.empty()) {
			delete mpControl->mCons.back();  mpControl->mCons.pop_back();
		}

		// add new set
		LbmControlSet *cset;

		cset = new LbmControlSet();
		cset->initCparts();
		// dont load any file
		cset->mContrPartFile = string("");

		// set radii for attraction & velocity forces
		// set strength of the forces
		// don't set directly! but use channels: 
		// mcForceAtt, mcForceVel, mcForceMaxd, mcRadiusAtt, mcRadiusVel, mcRadiusMind, mcRadiusMaxd etc.

		// wrong: cset->mCparts->setInfluenceAttraction(1.15); cset->mCparts->setRadiusAtt(1.5);
		// right, e.g., to init some constant values:
		cset->mcForceAtt = AnimChannel<float>(0.2);
		cset->mcRadiusAtt = AnimChannel<float>(0.75);
		cset->mcForceVel = AnimChannel<float>(0.2);
		cset->mcRadiusVel = AnimChannel<float>(0.75);

		// this value can be left at 0.5:
		cset->mCparts->setCPSMvmWeightFac(0.5);

		mpControl->mCons.push_back( cset );

		// instead of reading from file (cset->mContrPartFile), manually init some particles
		mpControl->mCons[0]->mCparts->initBlenderTest();

		// other values that might be interesting to change:
		//cset->mCparts->setCPSTimestep(0.02);
		//cset->mCparts->setCPSTimeStart(0.);
		//cset->mCparts->setCPSTimeEnd(1.); 

		//mpControl->mDiffVelCon = 1.; // more rigid velocity control, 0 (default) allows more turbulence
	}

	// control particle -------------------------------------------------------------------------------------

	// init cppf stage, use set 0!
	if(mCppfStage>0) {
		if(mpControl->mCpOutfile.length()<1) mpControl->mCpOutfile = string("cpout"); // use getOutFilename !?
		char strbuf[100];
		const char *cpFormat = "_d%dcppf%d";

		// initial coarse stage, no input
		if(mCppfStage==1) {
		 	mpControl->mCons[0]->mContrPartFile = "";
		} else {
			// read from prev stage
			snprintf(strbuf,100, cpFormat  ,LBMDIM,mCppfStage-1);
			mpControl->mCons[0]->mContrPartFile = mpControl->mCpOutfile;
			mpControl->mCons[0]->mContrPartFile += strbuf;
			mpControl->mCons[0]->mContrPartFile += ".cpart2";
		}

		snprintf(strbuf,100, cpFormat  ,LBMDIM,mCppfStage);
		mpControl->mCpOutfile += strbuf;
	} // */
	
	for(int cpssi=0; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		ControlParticles *cparts = mpControl->mCons[cpssi]->mCparts;
		ControlParticles *cpmotion = mpControl->mCons[cpssi]->mCpmotion;

		// now set with real dt
		cparts->setInfluenceVelocity( mpControl->mCons[cpssi]->mcForceVel.get(0.), mLevel[mMaxRefine].timestep);
		cparts->setCharLength( mLevel[mMaxRefine].nodeSize );
		cparts->setCharLength( mLevel[mMaxRefine].nodeSize );
		errMsg("LbmControlData","CppfStage "<<mCppfStage<<" in:"<<mpControl->mCons[cpssi]->mContrPartFile<<
				" out:"<<mpControl->mCpOutfile<<" cl:"<< cparts->getCharLength() );

		// control particle test init
		if(mpControl->mCons[cpssi]->mCpmotionFile.length()>=1) cpmotion->initFromTextFile(mpControl->mCons[cpssi]->mCpmotionFile);
		// not really necessary...
		//? cparts->setFluidSpacing( mLevel[mMaxRefine].nodeSize	); // use grid coords!?
		//? cparts->calculateKernelWeight();
		//? debMsgStd("LbmFsgrSolver::initCpdata",DM_MSG,"ControlParticles - motion inited: "<<cparts->getSize() ,10);

		// ensure both are on for env. var settings
		// when no particles, but outfile enabled, initialize
		const int lev = mMaxRefine;
		if((mpParticles) && (mpControl->mCpOutfile.length()>=1) && (cpssi==0)) {
			// check if auto num
			if( (mpParticles->getNumInitialParticles()<=1) && 
					(mpParticles->getNumParticles()<=1) ) { // initParticles done afterwards anyway
				int tracers = 0;
				const int workSet = mLevel[lev].setCurr;
				FSGR_FORIJK_BOUNDS(lev) { 
					if(RFLAG(lev,i,j,k, workSet)&(CFFluid)) tracers++;
				}
				if(LBMDIM==3) tracers /= 8;
				else          tracers /= 4;
				mpParticles->setNumInitialParticles(tracers);
				mpParticles->setDumpTextFile(mpControl->mCpOutfile);
				debMsgStd("LbmFsgrSolver::initCpdata",DM_MSG,"ControlParticles - set tracers #"<<tracers<<", actual #"<<mpParticles->getNumParticles() ,10);
			}
			if(mpParticles->getDumpTextInterval()<=0.) {
				mpParticles->setDumpTextInterval(mLevel[lev].timestep * mLevel[lev].lSizex);
				debMsgStd("LbmFsgrSolver::initCpdata",DM_MSG,"ControlParticles - dump delta t not set, using dti="<< mpParticles->getDumpTextInterval()<<", sim dt="<<mLevel[lev].timestep, 5 );
			}
			mpParticles->setDumpParts(true); // DEBUG? also dump as particle system
		}

		if(mpControl->mCons[cpssi]->mContrPartFile.length()>=1) cparts->initFromTextFile(mpControl->mCons[cpssi]->mContrPartFile);
		cparts->setFluidSpacing( mLevel[lev].nodeSize	); // use grid coords!?
		cparts->calculateKernelWeight();
		debMsgStd("LbmFsgrSolver::initCpdata",DM_MSG,"ControlParticles mCons"<<cpssi<<" - inited, parts:"<<cparts->getTotalSize()<<","<<cparts->getSize()<<" dt:"<<mpParam->getTimestep()<<" control time:"<<cparts->getControlTimStart()<<" to "<<cparts->getControlTimEnd() ,10);
	} // cpssi

	if(getenv("ELBEEM_CPINFILE")) {
		this->mTForceStrength = 1.0;
	}
	this->mTForceStrength = mpControl->mSetForceStrength;
	if(mpControl->mCpOutfile.length()>=1) mpParticles->setDumpTextFile(mpControl->mCpOutfile);

	// control particle  init end -------------------------------------------------------------------------------------

	// make sure equiv to solver init
	if(this->mTForceStrength>0.) { \
		mpControl->mCpForces.resize( mMaxRefine+1 );
		for(int lev = 0; lev<=mMaxRefine; lev++) {
			LONGINT rcellSize = (mLevel[lev].lSizex*mLevel[lev].lSizey*mLevel[lev].lSizez);
			debMsgStd("LbmFsgrSolver::initControl",DM_MSG,"mCpForces init, lev="<<lev<<" rcs:"<<(int)(rcellSize+4)<<","<<(rcellSize*sizeof(ControlForces)/(1024*1024)), 9 );
			mpControl->mCpForces[lev].resize( (int)(rcellSize+4) );
			//for(int i=0 ;i<rcellSize; i++) mpControl->mCpForces.push_back( ControlForces() );
			for(int i=0 ;i<rcellSize; i++) mpControl->mCpForces[lev][i].resetForces();
		}
	} // on?

	debMsgStd("LbmFsgrSolver::initCpdata",DM_MSG,"ControlParticles #mCons "<<mpControl->mCons.size()<<" done", 6);
}


#define CPODEBUG 0
//define CPINTER ((int)(mpControl->mCpUpdateInterval))

#define KERN(x,y,z)    mpControl->mCpKernel[ (((z)*cpkarWidth + (y))*cpkarWidth + (x)) ]
#define MDKERN(x,y,z)  mpControl->mMdKernel[ (((z)*mdkarWidth + (y))*mdkarWidth + (x)) ]

#define BOUNDCHECK(x,low,high)  ( ((x)<low) ? low : (((x)>high) ? high : (x) )  )
#define BOUNDSKIP(x,low,high)  ( ((x)<low) || ((x)>high) )

void 
LbmFsgrSolver::handleCpdata()
{
	myTime_t cpstart = getTime();
	int cpChecks=0;
	int cpInfs=0;
	//debMsgStd("ControlData::handleCpdata",DM_MSG,"called... "<<this->mTForceStrength,1);

	// add cp influence
  if((true) && (this->mTForceStrength>0.)) {
		// ok continue...
	} // on off
	else {
		return;
	}
	
	// check if we have control objects
	if(mpControl->mCons.size()==0)
		return;
	
	if((mpControl->mCpUpdateInterval<1) || (this->mStepCnt%mpControl->mCpUpdateInterval==0)) {
		// do full reinit later on... 
	}
	else if(this->mStepCnt>mpControl->mCpUpdateInterval) {
		// only reinit new cells
		// TODO !? remove loop dependance!?
#define NOFORCEENTRY(lev, i,j,k) (LBMGET_FORCE(lev, i,j,k).maxDistance==CPF_MAXDINIT) 
		// interpolate missing
		for(int lev=0; lev<=mMaxRefine; lev++) {
		FSGR_FORIJK_BOUNDS(lev) { 
			if( (RFLAG(lev,i,j,k, mLevel[lev].setCurr)) & (CFFluid|CFInter) )  
			//if( (RFLAG(lev,i,j,k, mLevel[lev].setCurr)) & (CFInter) )  
			//if(0)
			{ // only check new inter? RFLAG?check
				if(NOFORCEENTRY(lev, i,j,k)) {
					//errMsg("CP","FE_MISSING at "<<PRINT_IJK<<" f"<<LBMGET_FORCE(lev, i,j,k).weightAtt<<" md"<<LBMGET_FORCE(lev, i,j,k).maxDistance );

					LbmFloat nbs=0.;
					ControlForces vals;
					vals.resetForces(); vals.maxDistance = 0.;
					for(int l=1; l<this->cDirNum; l++) { 
						int ni=i+this->dfVecX[l], nj=j+this->dfVecY[l], nk=k+this->dfVecZ[l];
						//errMsg("CP","FE_MISSING check "<<PRINT_VEC(ni,nj,nk)<<" f"<<LBMGET_FORCE(lev, ni,nj,nk).weightAtt<<" md"<<LBMGET_FORCE(lev, ni,nj,nk).maxDistance );
						if(!NOFORCEENTRY(lev, ni,nj,nk)) {
							//? vals.weightAtt   += LBMGET_FORCE(lev, ni,nj,nk).weightAtt;
							//? vals.forceAtt    += LBMGET_FORCE(lev, ni,nj,nk).forceAtt;
							vals.maxDistance += LBMGET_FORCE(lev, ni,nj,nk).maxDistance;
							vals.forceMaxd   += LBMGET_FORCE(lev, ni,nj,nk).forceMaxd;
							vals.weightVel   += LBMGET_FORCE(lev, ni,nj,nk).weightVel;
							vals.forceVel    += LBMGET_FORCE(lev, ni,nj,nk).forceVel; 
							// ignore att/compAv/avgVel here for now
							nbs += 1.;
						}
					}
					if(nbs>0.) {
						nbs = 1./nbs;
						//? LBMGET_FORCE(lev, i,j,k).weightAtt   =	vals.weightAtt*nbs;
						//? LBMGET_FORCE(lev, i,j,k).forceAtt    = vals.forceAtt*nbs;
						LBMGET_FORCE(lev, i,j,k).maxDistance = vals.maxDistance*nbs;
						LBMGET_FORCE(lev, i,j,k).forceMaxd   =	vals.forceMaxd*nbs;
						LBMGET_FORCE(lev, i,j,k).weightVel   =	vals.weightVel*nbs;
						LBMGET_FORCE(lev, i,j,k).forceVel    = vals.forceVel*nbs;
					}
						/*ControlForces *ff = &LBMGET_FORCE(lev, i,j,k);  // DEBUG
						errMsg("CP","FE_MISSING rec at "<<PRINT_IJK // DEBUG
								<<" w:"<<ff->weightAtt<<" wa:" <<PRINT_VEC( ff->forceAtt[0],ff->forceAtt[1],ff->forceAtt[2] )  
								<<" v:"<<ff->weightVel<<" wv:" <<PRINT_VEC( ff->forceVel[0],ff->forceVel[1],ff->forceVel[2] )  
								<<" v:"<<ff->maxDistance<<" wv:" <<PRINT_VEC( ff->forceMaxd[0],ff->forceMaxd[1],ff->forceMaxd[2] )  ); // DEBUG */
					// else errMsg("CP","FE_MISSING rec at "<<PRINT_IJK<<" failed!"); // DEBUG
					
				}
			}
		}} // ijk, lev

		// mStepCnt > mpControl->mCpUpdateInterval
		return;
	} else {
		// nothing to do ...
		return;
	}

	// reset
	for(int lev=0; lev<=mMaxRefine; lev++) {
		FSGR_FORIJK_BOUNDS(lev) { LBMGET_FORCE(lev,i,j,k).resetForces(); }
	}
	// do setup for coarsest level
	const int coarseLev = 0;
	const int fineLev = mMaxRefine;

	// init for current time
	for(int cpssi=0; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		ControlParticles *cparts = mpControl->mCons[cpssi]->mCparts;
		LbmControlSet *cset = mpControl->mCons[cpssi];
		
		cparts->setRadiusAtt(cset->mcRadiusAtt.get(mSimulationTime));
		cparts->setRadiusVel(cset->mcRadiusVel.get(mSimulationTime));
		cparts->setInfluenceAttraction(cset->mcForceAtt.get(mSimulationTime) );
		cparts->setInfluenceMaxdist(cset->mcForceMaxd.get(mSimulationTime) );
		cparts->setRadiusMinMaxd(cset->mcRadiusMind.get(mSimulationTime));
		cparts->setRadiusMaxd(cset->mcRadiusMaxd.get(mSimulationTime));
		cparts->calculateKernelWeight(); // always necessary!?
		cparts->setOffset( vec2L(cset->mcCpOffset.get(mSimulationTime)) );
		cparts->setScale(  vec2L(cset->mcCpScale.get(mSimulationTime)) );

		cparts->setInfluenceVelocity( cset->mcForceVel.get(mSimulationTime), mLevel[fineLev].timestep );
		cparts->setLastOffset( vec2L(cset->mcCpOffset.get(mSimulationTime-mLevel[fineLev].timestep)) );
		cparts->setLastScale(  vec2L(cset->mcCpScale.get(mSimulationTime-mLevel[fineLev].timestep)) );
		
	}

	// check actual values
	LbmFloat iatt  = ABS(mpControl->mCons[0]->mCparts->getInfluenceAttraction());
	LbmFloat ivel  = mpControl->mCons[0]->mCparts->getInfluenceVelocity();
	LbmFloat imaxd = mpControl->mCons[0]->mCparts->getInfluenceMaxdist();
	//errMsg("FINCIT","iatt="<<iatt<<" ivel="<<ivel<<" imaxd="<<imaxd);
	for(int cpssi=1; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		LbmFloat iatt2  = ABS(mpControl->mCons[cpssi]->mCparts->getInfluenceAttraction());
		LbmFloat ivel2  = mpControl->mCons[cpssi]->mCparts->getInfluenceVelocity();
		LbmFloat imaxd2 = mpControl->mCons[cpssi]->mCparts->getInfluenceMaxdist();
		
		// we allow negative attraction force here!
		if(iatt2 > iatt)   iatt = iatt2;
		
		if(ivel2 >ivel)   ivel = ivel2;
		if(imaxd2>imaxd)  imaxd= imaxd2;
		//errMsg("FINCIT"," "<<cpssi<<" iatt2="<<iatt2<<" ivel2="<<ivel2<<" imaxd2="<<imaxd<<" NEW "<<" iatt="<<iatt<<" ivel="<<ivel<<" imaxd="<<imaxd);
	}

	if(iatt==0. && ivel==0. && imaxd==0.) {
		debMsgStd("ControlData::initControl",DM_MSG,"Skipped, all zero...",4);
		return;
	}
	//iatt  = mpControl->mCons[1]->mCparts->getInfluenceAttraction(); //ivel  = mpControl->mCons[1]->mCparts->getInfluenceVelocity(); //imaxd = mpControl->mCons[1]->mCparts->getInfluenceMaxdist(); // TTTTTT

	// do control setup
	for(int cpssi=0; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		ControlParticles *cparts = mpControl->mCons[cpssi]->mCparts;
		ControlParticles *cpmotion = mpControl->mCons[cpssi]->mCpmotion;

		// TEST!?
		bool radmod = false;
		const LbmFloat minRadSize = mLevel[coarseLev].nodeSize * 1.5;
		if((cparts->getRadiusAtt()>0.) && (cparts->getRadiusAtt()<minRadSize) && (!radmod) ) {
			LbmFloat radfac = minRadSize / cparts->getRadiusAtt(); radmod=true;
			debMsgStd("ControlData::initControl",DM_MSG,"Modified radii att, fac="<<radfac, 7);
			cparts->setRadiusAtt(cparts->getRadiusAtt()*radfac);
			cparts->setRadiusVel(cparts->getRadiusVel()*radfac);
			cparts->setRadiusMaxd(cparts->getRadiusMaxd()*radfac);
			cparts->setRadiusMinMaxd(cparts->getRadiusMinMaxd()*radfac);
		} else if((cparts->getRadiusVel()>0.) && (cparts->getRadiusVel()<minRadSize) && (!radmod) ) {
			LbmFloat radfac = minRadSize / cparts->getRadiusVel();
			debMsgStd("ControlData::initControl",DM_MSG,"Modified radii vel, fac="<<radfac, 7);
			cparts->setRadiusVel(cparts->getRadiusVel()*radfac);
			cparts->setRadiusMaxd(cparts->getRadiusMaxd()*radfac);
			cparts->setRadiusMinMaxd(cparts->getRadiusMinMaxd()*radfac);
		} else if((cparts->getRadiusMaxd()>0.) && (cparts->getRadiusMaxd()<minRadSize) && (!radmod) ) {
			LbmFloat radfac = minRadSize / cparts->getRadiusMaxd();
			debMsgStd("ControlData::initControl",DM_MSG,"Modified radii maxd, fac="<<radfac, 7);
			cparts->setRadiusMaxd(cparts->getRadiusMaxd()*radfac);
			cparts->setRadiusMinMaxd(cparts->getRadiusMinMaxd()*radfac);
		}
		if(radmod) {
			debMsgStd("ControlData::initControl",DM_MSG,"Modified radii: att="<< 
				cparts->getRadiusAtt()<<", vel=" << cparts->getRadiusVel()<<", maxd=" <<
				cparts->getRadiusMaxd()<<", mind=" << cparts->getRadiusMinMaxd() ,5);
		}

		cpmotion->prepareControl( mSimulationTime+((LbmFloat)mpControl->mCpUpdateInterval)*(mpParam->getTimestep()), mpParam->getTimestep(), NULL );
		cparts->prepareControl( mSimulationTime+((LbmFloat)mpControl->mCpUpdateInterval)*(mpParam->getTimestep()), mpParam->getTimestep(), cpmotion );
	}

	// do control...
	for(int lev=0; lev<=mMaxRefine; lev++) {
		LbmFloat levVolume = 1.;
		LbmFloat levForceScale = 1.;
		for(int ll=lev; ll<mMaxRefine; ll++) {
			if(LBMDIM==3) levVolume *= 8.;
			else levVolume *= 4.;
			levForceScale *= 2.;
		}
		errMsg("LbmFsgrSolver::handleCpdata","levVolume="<<levVolume<<" levForceScale="<<levForceScale );
	//todo: scale velocity, att by level timestep!?
		
	for(int cpssi=0; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		ControlParticles *cparts = mpControl->mCons[cpssi]->mCparts;
		// ControlParticles *cpmotion = mpControl->mCons[cpssi]->mCpmotion;
		
		// if control set is not active skip it
		if((cparts->getControlTimStart() > mSimulationTime) || (cparts->getControlTimEnd() < mLastSimTime))
		{
			continue;
		}

		const LbmFloat velLatticeScale = mLevel[lev].timestep/mLevel[lev].nodeSize;
		LbmFloat gsx = ((mvGeoEnd[0]-mvGeoStart[0])/(LbmFloat)mLevel[lev].lSizex);
		LbmFloat gsy = ((mvGeoEnd[1]-mvGeoStart[1])/(LbmFloat)mLevel[lev].lSizey);
		LbmFloat gsz = ((mvGeoEnd[2]-mvGeoStart[2])/(LbmFloat)mLevel[lev].lSizez);
#if LBMDIM==2
		gsz = gsx;
#endif
		LbmFloat goffx = mvGeoStart[0];
		LbmFloat goffy = mvGeoStart[1];
		LbmFloat goffz = mvGeoStart[2];

		//const LbmFloat cpwIncFac = 2.0;
		// max to two thirds of domain size
		const int cpw = MIN( mLevel[lev].lSizex/3, MAX( (int)( cparts->getRadiusAtt()  /gsx) +1  , 2) ); // normal kernel, att,vel
		const int cpkarWidth = 2*cpw+1;
		mpControl->mCpKernel.resize(cpkarWidth* cpkarWidth* cpkarWidth);
		ControlParticle cpt; cpt.reset();
		cpt.pos = LbmVec( (gsx*(LbmFloat)cpw)+goffx, (gsy*(LbmFloat)cpw)+goffy, (gsz*(LbmFloat)cpw)+goffz );  // optimize?
		cpt.density = 0.5; cpt.densityWeight = 0.5;
#if LBMDIM==3
		for(int k= 0; k<cpkarWidth; ++k) {
#else // LBMDIM==3
		{ int k = cpw;
#endif 
			for(int j= 0; j<cpkarWidth; ++j) 
				for(int i= 0; i<cpkarWidth; ++i) {
					KERN(i,j,k).resetForces();
					//LbmFloat dx = i-cpw; LbmFloat dy = j-cpw; LbmFloat dz = k-cpw;
					//LbmVec dv = ( LbmVec(dx,dy,dz) );
					//LbmFloat dl = norm( dv ); //LbmVec dir = dv / dl;
					LbmVec pos = LbmVec( (gsx*(LbmFloat)i)+goffx, (gsy*(LbmFloat)j)+goffy, (gsz*(LbmFloat)k)+goffz );  // optimize?
					cparts->calculateCpInfluenceOpt( &cpt, pos, LbmVec(0,0,0), &KERN(i,j,k)  ,1. );
					/*if((CPODEBUG)&&(k==cpw)) errMsg("kern"," at "<<PRINT_IJK<<" pos"<<pos<<" cpp"<<cpt.pos 
						<<" wf:"<<KERN(i,j,k).weightAtt<<" wa:"<< PRINT_VEC( KERN(i,j,k).forceAtt[0],KERN(i,j,k).forceAtt[1],KERN(i,j,k).forceAtt[2] )  
						<<" wf:"<<KERN(i,j,k).weightVel<<" wa:"<< PRINT_VEC( KERN(i,j,k).forceVel[0],KERN(i,j,k).forceVel[1],KERN(i,j,k).forceVel[2] )  
						<<" wf:"<<KERN(i,j,k).maxDistance<<" wa:"<< PRINT_VEC( KERN(i,j,k).forceMaxd[0],KERN(i,j,k).forceMaxd[1],KERN(i,j,k).forceMaxd[2] )  ); // */
					KERN(i,j,k).weightAtt *= 2.;
					KERN(i,j,k).forceAtt *= 2.;
					//KERN(i,j,k).forceAtt[1] *= 2.; KERN(i,j,k).forceAtt[2] *= 2.;
					KERN(i,j,k).weightVel *= 2.;
					KERN(i,j,k).forceVel *= 2.;
					//KERN(i,j,k).forceVel[1] *= 2.; KERN(i,j,k).forceVel[2] *= 2.;
				}
		}

		if(CPODEBUG) errMsg("cpw"," = "<<cpw<<" f"<< cparts->getRadiusAtt()<<" gsx"<<gsx<<" kpw"<<cpkarWidth); // DEBUG
		// first cp loop - add att and vel forces
		for(int cppi=0; cppi<cparts->getSize(); cppi++) {
			ControlParticle *cp = cparts->getParticle(cppi);
			if(cp->influence<=0.) continue;
			const int cpi = (int)( (cp->pos[0]-goffx)/gsx );
			const int cpj = (int)( (cp->pos[1]-goffy)/gsy );
			int       cpk = (int)( (cp->pos[2]-goffz)/gsz );
			/*if( ((LBMDIM==3)&&(BOUNDSKIP(cpk - cpwsm, getForZMinBnd(), getForZMaxBnd(lev) ))) ||
				((LBMDIM==3)&&(BOUNDSKIP(cpk + cpwsm, getForZMinBnd(), getForZMaxBnd(lev) ))) ||
				BOUNDSKIP(cpj - cpwsm, 0, mLevel[lev].lSizey ) ||
				BOUNDSKIP(cpj + cpwsm, 0, mLevel[lev].lSizey ) ||
				BOUNDSKIP(cpi - cpwsm, 0, mLevel[lev].lSizex ) ||
				BOUNDSKIP(cpi + cpwsm, 0, mLevel[lev].lSizex ) ) {
				continue;
				} // */
			int is,ie,js,je,ks,ke;
			ks = BOUNDCHECK(cpk - cpw, getForZMinBnd(), getForZMaxBnd(lev) );
			ke = BOUNDCHECK(cpk + cpw, getForZMinBnd(), getForZMaxBnd(lev) );
			js = BOUNDCHECK(cpj - cpw, 0, mLevel[lev].lSizey );
			je = BOUNDCHECK(cpj + cpw, 0, mLevel[lev].lSizey );
			is = BOUNDCHECK(cpi - cpw, 0, mLevel[lev].lSizex );
			ie = BOUNDCHECK(cpi + cpw, 0, mLevel[lev].lSizex );
			if(LBMDIM==2) { cpk = 0; ks = 0; ke = 1; }
			if(CPODEBUG) errMsg("cppft","i"<<cppi<<" cpw"<<cpw<<" gpos"<<PRINT_VEC(cpi,cpj,cpk)<<"   i:"<<is<<","<<ie<<"   j:"<<js<<","<<je<<"   k:"<<ks<<","<<ke<<" "); // DEBUG
			cpInfs++;

			for(int k= ks; k<ke; ++k) {
				for(int j= js; j<je; ++j) {

					CellFlagType *pflag = &RFLAG(lev,is,j,k, mLevel[lev].setCurr);
					ControlForces *kk = &KERN( is-cpi+cpw, j-cpj+cpw, k-cpk+cpw);
					ControlForces *ff = &LBMGET_FORCE(lev,is,j,k); 
					pflag--; kk--; ff--;

					for(int i= is; i<ie; ++i) {
						// first cp loop (att,vel)
						pflag++; kk++; ff++;

						//add weight for bnd cells
						const LbmFloat pwforce = kk->weightAtt;
						// control particle mod,
						// dont add multiple CFFluid fsgr boundaries
						if(lev==mMaxRefine) {
							//if( ( ((*pflag)&(CFFluid )) && (lev==mMaxRefine) ) ||
									//( ((*pflag)&(CFGrNorm)) && (lev <mMaxRefine) ) ) {
							if((*pflag)&(CFFluid|CFUnused)) {
								// check not fromcoarse?
								cp->density += levVolume* kk->weightAtt; // old CFFluid
							} else if( (*pflag) & (CFEmpty) ) {  
								cp->density -= levVolume* 0.5; 
							} else { //if( ((*pflag) & (CFBnd)) ) {  
								cp->density -= levVolume* 0.2;  // penalty
							}
						} else {
							//if((*pflag)&(CFGrNorm)) {
								//cp->density += levVolume* kk->weightAtt; // old CFFluid
							//} 
						}
						//else if(!((*pflag) & (CFUnused)) ) {  cp->density -= levVolume* 0.2; } // penalty

						if( (*pflag) & (CFFluid|CFInter) )  // RFLAG_check
						{

							cpChecks++;
							//const LbmFloat pwforce = kk->weightAtt;
							LbmFloat pwvel = kk->weightVel;
							if((pwforce==0.)&&(pwvel==0.)) { continue; }
							ff->weightAtt += 1e-6; // for distance

							if(pwforce>0.) {
								ff->weightAtt += pwforce *cp->densityWeight *cp->influence;
								ff->forceAtt += kk->forceAtt *levForceScale *cp->densityWeight *cp->influence;

								// old fill handling here
								const int workSet =mLevel[lev].setCurr;
								LbmFloat ux=0., uy=0., uz=0.;
								FORDF1{  
									const LbmFloat dfn = QCELL(lev, i,j,k, workSet, l);
									ux  += (this->dfDvecX[l]*dfn);
									uy  += (this->dfDvecY[l]*dfn); 
									uz  += (this->dfDvecZ[l]*dfn); 
								}
								// control particle mod
								cp->avgVelWeight += levVolume*pwforce;
								cp->avgVelAcc += LbmVec(ux,uy,uz) * levVolume*pwforce;
							}

							if(pwvel>0.) {
								// TODO make switch? vel.influence depends on density weight... 
								// (reduced lowering with 0.75 factor)
								pwvel *=  cp->influence *(1.-0.75*cp->densityWeight);
								// control particle mod
								// todo use Omega instead!?
								ff->forceVel += cp->vel*levVolume*pwvel * velLatticeScale; // levVolume?
								ff->weightVel += levVolume*pwvel; // levVolume?
								ff->compAv += cp->avgVel*levVolume*pwvel; // levVolume?
								ff->compAvWeight += levVolume*pwvel; // levVolume?
							}

							if(CPODEBUG) errMsg("cppft","i"<<cppi<<" at "<<PRINT_IJK<<" kern:"<<
									PRINT_VEC(i-cpi+cpw, j-cpj+cpw, k-cpk+cpw )
									//<<" w:"<<ff->weightAtt<<" wa:"
									//<<PRINT_VEC( ff->forceAtt[0],ff->forceAtt[1],ff->forceAtt[2] )  
									//<<" v:"<<ff->weightVel<<" wv:"
									//<<PRINT_VEC( ff->forceVel[0],ff->forceVel[1],ff->forceVel[2] )  
									//<<" v:"<<ff->maxDistance<<" wv:"
									//<<PRINT_VEC( ff->forceMaxd[0],ff->forceMaxd[1],ff->forceMaxd[2] )  
									);
						} // celltype

					} // ijk
				} // ijk
			} // ijk
		} // cpi, end first cp loop (att,vel)
		debMsgStd("LbmFsgrSolver::handleCpdata",DM_MSG,"Force cpgrid "<<cpssi<<" generated checks:"<<cpChecks<<" infs:"<<cpInfs ,9);
	} //cpssi
	} // lev

	// second loop
	for(int lev=0; lev<=mMaxRefine; lev++) {
		LbmFloat levVolume = 1.;
		LbmFloat levForceScale = 1.;
		for(int ll=lev; ll<mMaxRefine; ll++) {
			if(LBMDIM==3) levVolume *= 8.;
			else levVolume *= 4.;
			levForceScale *= 2.;
		}
	// prepare maxd forces
	for(int cpssi=0; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		ControlParticles *cparts = mpControl->mCons[cpssi]->mCparts;

		// WARNING copied from above!
		const LbmFloat velLatticeScale = mLevel[lev].timestep/mLevel[lev].nodeSize;
		LbmFloat gsx = ((mvGeoEnd[0]-mvGeoStart[0])/(LbmFloat)mLevel[lev].lSizex);
		LbmFloat gsy = ((mvGeoEnd[1]-mvGeoStart[1])/(LbmFloat)mLevel[lev].lSizey);
		LbmFloat gsz = ((mvGeoEnd[2]-mvGeoStart[2])/(LbmFloat)mLevel[lev].lSizez);
#if LBMDIM==2
		gsz = gsx;
#endif
		LbmFloat goffx = mvGeoStart[0];
		LbmFloat goffy = mvGeoStart[1];
		LbmFloat goffz = mvGeoStart[2];

		//const LbmFloat cpwIncFac = 2.0;
		const int mdw = MIN( mLevel[lev].lSizex/2, MAX( (int)( cparts->getRadiusMaxd() /gsx) +1  , 2) ); // wide kernel, md
		const int mdkarWidth = 2*mdw+1;
		mpControl->mMdKernel.resize(mdkarWidth* mdkarWidth* mdkarWidth);
		ControlParticle cpt; cpt.reset();
		cpt.density = 0.5; cpt.densityWeight = 0.5;
		cpt.pos = LbmVec( (gsx*(LbmFloat)mdw)+goffx, (gsy*(LbmFloat)mdw)+goffy, (gsz*(LbmFloat)mdw)+goffz );  // optimize?
#if LBMDIM==3
		for(int k= 0; k<mdkarWidth; ++k) {
#else // LBMDIM==3
		{ int k = mdw;
#endif 
			for(int j= 0; j<mdkarWidth; ++j) 
				for(int i= 0; i<mdkarWidth; ++i) {
					MDKERN(i,j,k).resetForces();
					LbmVec pos = LbmVec( (gsx*(LbmFloat)i)+goffx, (gsy*(LbmFloat)j)+goffy, (gsz*(LbmFloat)k)+goffz );  // optimize?
					cparts->calculateMaxdForce( &cpt, pos, &MDKERN(i,j,k)  );
				}
		}

		// second cpi loop, maxd forces
		if(cparts->getInfluenceMaxdist()>0.) {
			for(int cppi=0; cppi<cparts->getSize(); cppi++) {
				ControlParticle *cp = cparts->getParticle(cppi);
				if(cp->influence<=0.) continue;
				const int cpi = (int)( (cp->pos[0]-goffx)/gsx );
				const int cpj = (int)( (cp->pos[1]-goffy)/gsy );
				int       cpk = (int)( (cp->pos[2]-goffz)/gsz );

				int is,ie,js,je,ks,ke;
				ks = BOUNDCHECK(cpk - mdw, getForZMinBnd(), getForZMaxBnd(lev) );
				ke = BOUNDCHECK(cpk + mdw, getForZMinBnd(), getForZMaxBnd(lev) );
				js = BOUNDCHECK(cpj - mdw, 0, mLevel[lev].lSizey );
				je = BOUNDCHECK(cpj + mdw, 0, mLevel[lev].lSizey );
				is = BOUNDCHECK(cpi - mdw, 0, mLevel[lev].lSizex );
				ie = BOUNDCHECK(cpi + mdw, 0, mLevel[lev].lSizex );
				if(LBMDIM==2) { cpk = 0; ks = 0; ke = 1; }
				if(CPODEBUG) errMsg("cppft","i"<<cppi<<" mdw"<<mdw<<" gpos"<<PRINT_VEC(cpi,cpj,cpk)<<"   i:"<<is<<","<<ie<<"   j:"<<js<<","<<je<<"   k:"<<ks<<","<<ke<<" "); // DEBUG
				cpInfs++;

				for(int k= ks; k<ke; ++k)
				 for(int j= js; j<je; ++j) {
					CellFlagType *pflag = &RFLAG(lev,is-1,j,k, mLevel[lev].setCurr);
					for(int i= is; i<ie; ++i) {
						// second cpi loop, maxd forces
						pflag++;
						if( (*pflag) & (CFFluid|CFInter) )  // RFLAG_check
						{
							cpChecks++;
							ControlForces *ff = &LBMGET_FORCE(lev,i,j,k); 
							if(ff->weightAtt == 0.) {
								ControlForces *kk = &MDKERN( i-cpi+mdw, j-cpj+mdw, k-cpk+mdw);
								const LbmFloat pmdf = kk->maxDistance;
								if((ff->maxDistance > pmdf) || (ff->maxDistance<0.))
									ff->maxDistance = pmdf;
								ff->forceMaxd = kk->forceMaxd;
								// todo use Omega instead!?
								ff->forceVel = cp->vel* velLatticeScale;
							}
						} // celltype
				} } // ijk
			} // cpi, md loop 
		} // maxd inf>0 */


		debMsgStd("ControlData::initControl",DM_MSG,"Maxd cpgrid "<<cpssi<<" generated checks:"<<cpChecks<<" infs:"<<cpInfs ,9);
	} //cpssi

	// normalize, only done once for the whole array
	mpControl->mCons[0]->mCparts->finishControl( mpControl->mCpForces[lev], iatt,ivel,imaxd );

	} // lev loop

	myTime_t cpend = getTime(); 
	debMsgStd("ControlData::handleCpdata",DM_MSG,"Time for cpgrid generation:"<< getTimeString(cpend-cpstart)<<", checks:"<<cpChecks<<" infs:"<<cpInfs<<" " ,8);

	// warning, may return  before
}

#if LBM_USE_GUI==1

#define USE_GLUTILITIES
#include "../gui/gui_utilities.h"

void LbmFsgrSolver::cpDebugDisplay(int dispset)
{
	for(int cpssi=0; cpssi<(int)mpControl->mCons.size(); cpssi++) {
		ControlParticles *cparts = mpControl->mCons[cpssi]->mCparts;
		//ControlParticles *cpmotion = mpControl->mCons[cpssi]->mCpmotion;
		// display cp parts
		const bool cpCubes = false;
		const bool cpDots = true;
		const bool cpCpdist = true;
		const bool cpHideIna = true;
		glShadeModel(GL_FLAT);
		glDisable( GL_LIGHTING ); // dont light lines

		// dot influence
		if((mpControl->mDebugCpscale>0.) && cpDots) {
			glPointSize(mpControl->mDebugCpscale * 8.);
			glBegin(GL_POINTS);
			for(int i=0; i<cparts->getSize(); i++) {
				if((cpHideIna)&&( (cparts->getParticle(i)->influence<=0.) || (cparts->getParticle(i)->size<=0.) )) continue;
				ntlVec3Gfx org( vec2G(cparts->getParticle(i)->pos ) );
				//LbmFloat halfsize = 0.5; 
				LbmFloat scale = cparts->getParticle(i)->densityWeight;
				//glColor4f( scale,scale,scale,scale );
				glColor4f( 0.,scale,0.,scale );
				glVertex3f( org[0],org[1],org[2] );
				//errMsg("lbmDebugDisplay","CP "<<i<<" at "<<org); // DEBUG
			}
			glEnd();
		}

		// cp positions
		if((mpControl->mDebugCpscale>0.) && cpDots) {
			glPointSize(mpControl->mDebugCpscale * 3.);
			glBegin(GL_POINTS); 
			glColor3f( 0,1,0 );
		}
		for(int i=0; i<cparts->getSize(); i++) {
			if((cpHideIna)&&( (cparts->getParticle(i)->influence<=0.) || (cparts->getParticle(i)->size<=0.) )) continue;
			ntlVec3Gfx  org( vec2G(cparts->getParticle(i)->pos ) );
			LbmFloat halfsize = 0.5; 
			LbmFloat scale = cparts->getRadiusAtt() * cparts->getParticle(i)->densityWeight;
			if(cpCubes){	glLineWidth( 1 ); 
				glColor3f( 1,1,1 );
				ntlVec3Gfx s = org-(halfsize * (scale)); 
				ntlVec3Gfx e = org+(halfsize * (scale)); 
				drawCubeWire( s,e ); }
			if((mpControl->mDebugCpscale>0.) && cpDots) {
				glVertex3f( org[0],org[1],org[2] );
			}
		}
		if(cpDots) glEnd();

		if(mpControl->mDebugAvgVelScale>0.) {
			const float scale = mpControl->mDebugAvgVelScale;

			glColor3f( 1.0,1.0,1 );
			glBegin(GL_LINES); 
			for(int i=0; i<cparts->getSize(); i++) {
				if((cpHideIna)&&( (cparts->getParticle(i)->influence<=0.) || (cparts->getParticle(i)->size<=0.) )) continue;
				ntlVec3Gfx  org( vec2G(cparts->getParticle(i)->pos ) );

				//errMsg("CPAVGVEL","i"<<i<<" pos"<<org<<" av"<<cparts->getParticle(i)->avgVel);// DEBUG
				float dx = cparts->getParticle(i)->avgVel[0];
				float dy = cparts->getParticle(i)->avgVel[1];
				float dz = cparts->getParticle(i)->avgVel[2];
				dx *= scale; dy *= scale; dz *= scale;
				glVertex3f( org[0],org[1],org[2] );
				glVertex3f( org[0]+dx,org[1]+dy,org[2]+dz );
			}
			glEnd();
		} // */

		if( (LBMDIM==2) && (cpCpdist) ) {
			
			// debug, for use of e.g. LBMGET_FORCE LbmControlData *mpControl = this;
#			define TESTGET_FORCE(lev,i,j,k)   mpControl->mCpForces[lev][ ((k*mLevel[lev].lSizey)+j)*mLevel[lev].lSizex+i ]
	
			glBegin(GL_LINES);
			//const int lev=0; 
			for(int lev=0; lev<=mMaxRefine; lev++) {
			FSGR_FORIJK_BOUNDS(lev) {
					LbmVec pos = LbmVec( 
						((mvGeoEnd[0]-mvGeoStart[0])/(LbmFloat)mLevel[lev].lSizex) * ((LbmFloat)i+0.5) + mvGeoStart[0], 
						((mvGeoEnd[1]-mvGeoStart[1])/(LbmFloat)mLevel[lev].lSizey) * ((LbmFloat)j+0.5) + mvGeoStart[1], 
						((mvGeoEnd[2]-mvGeoStart[2])/(LbmFloat)mLevel[lev].lSizez) * ((LbmFloat)k+0.5) + mvGeoStart[2]  );
					if(LBMDIM==2) pos[2] = ((mvGeoEnd[2]-mvGeoStart[2])*0.5 + mvGeoStart[2]);

					if((mpControl->mDebugMaxdScale>0.) && (TESTGET_FORCE(lev,i,j,k).weightAtt<=0.) )
					if(TESTGET_FORCE(lev,i,j,k).maxDistance>=0.) 
					if(TESTGET_FORCE(lev,i,j,k).maxDistance<CPF_MAXDINIT ) {
						const float scale = mpControl->mDebugMaxdScale*10001.;
						float dx = TESTGET_FORCE(lev,i,j,k).forceMaxd[0];
						float dy = TESTGET_FORCE(lev,i,j,k).forceMaxd[1];
						float dz = TESTGET_FORCE(lev,i,j,k).forceMaxd[2];
						dx *= scale; dy *= scale; dz *= scale;
						glColor3f( 0,1,0 );
						glVertex3f( pos[0],pos[1],pos[2] );
						glVertex3f( pos[0]+dx,pos[1]+dy,pos[2]+dz );
					} // */
					if((mpControl->mDebugAttScale>0.) && (TESTGET_FORCE(lev,i,j,k).weightAtt>0.)) {
						const float scale = mpControl->mDebugAttScale*100011.;
						float dx = TESTGET_FORCE(lev,i,j,k).forceAtt[0];
						float dy = TESTGET_FORCE(lev,i,j,k).forceAtt[1];
						float dz = TESTGET_FORCE(lev,i,j,k).forceAtt[2];
						dx *= scale; dy *= scale; dz *= scale;
						glColor3f( 1,0,0 );
						glVertex3f( pos[0],pos[1],pos[2] );
						glVertex3f( pos[0]+dx,pos[1]+dy,pos[2]+dz );
					} // */
					// why check maxDistance?
					if((mpControl->mDebugVelScale>0.) && (TESTGET_FORCE(lev,i,j,k).maxDistance+TESTGET_FORCE(lev,i,j,k).weightVel>0.)) {
						float scale = mpControl->mDebugVelScale*1.;
						float wvscale = TESTGET_FORCE(lev,i,j,k).weightVel;
						float dx = TESTGET_FORCE(lev,i,j,k).forceVel[0];
						float dy = TESTGET_FORCE(lev,i,j,k).forceVel[1];
						float dz = TESTGET_FORCE(lev,i,j,k).forceVel[2];
						scale *= wvscale;
						dx *= scale; dy *= scale; dz *= scale;
						glColor3f( 0.2,0.2,1 );
						glVertex3f( pos[0],pos[1],pos[2] );
						glVertex3f( pos[0]+dx,pos[1]+dy,pos[2]+dz );
					} // */
					if((mpControl->mDebugCompavScale>0.) && (TESTGET_FORCE(lev,i,j,k).compAvWeight>0.)) {
						const float scale = mpControl->mDebugCompavScale*1.;
						float dx = TESTGET_FORCE(lev,i,j,k).compAv[0];
						float dy = TESTGET_FORCE(lev,i,j,k).compAv[1];
						float dz = TESTGET_FORCE(lev,i,j,k).compAv[2];
						dx *= scale; dy *= scale; dz *= scale;
						glColor3f( 0.2,0.2,1 );
						glVertex3f( pos[0],pos[1],pos[2] );
						glVertex3f( pos[0]+dx,pos[1]+dy,pos[2]+dz );
					} // */
			} // att,maxd
			}
			glEnd();
		}
	} // cpssi

	//fprintf(stderr,"BLA\n");
	glEnable( GL_LIGHTING ); // dont light lines
	glShadeModel(GL_SMOOTH);
}

#else // LBM_USE_GUI==1
void LbmFsgrSolver::cpDebugDisplay(int dispset) { }
#endif // LBM_USE_GUI==1



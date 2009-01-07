/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann Interface Class
 * contains stuff to be statically compiled
 * 
 *****************************************************************************/

/* LBM Files */ 
#include "ntl_matrices.h"
#include "solver_interface.h" 
#include "ntl_ray.h"
#include "ntl_world.h"
#include "elbeem.h"




/******************************************************************************
 * Interface Constructor
 *****************************************************************************/
LbmSolverInterface::LbmSolverInterface() :
	mPanic( false ),
  mSizex(10), mSizey(10), mSizez(10), 
  mAllfluid(false), mStepCnt( 0 ),
	mFixMass( 0.0 ),
  mOmega( 1.0 ),
  mGravity(0.0),
	mSurfaceTension( 0.0 ), 
	mBoundaryEast(  (CellFlagType)(CFBnd) ),mBoundaryWest( (CellFlagType)(CFBnd) ),mBoundaryNorth(  (CellFlagType)(CFBnd) ),
	mBoundarySouth( (CellFlagType)(CFBnd) ),mBoundaryTop(  (CellFlagType)(CFBnd) ),mBoundaryBottom( (CellFlagType)(CFBnd) ),
  mInitDone( false ),
  mInitDensityGradient( false ),
	mpSifAttrs( NULL ), mpSifSwsAttrs(NULL), mpParam( NULL ), mpParticles(NULL),
	mNumParticlesLost(0), 
	mNumInvalidDfs(0), mNumFilledCells(0), mNumEmptiedCells(0), mNumUsedCells(0), mMLSUPS(0),
	mDebugVelScale( 0.01 ), mNodeInfoString("+"),
	mvGeoStart(-1.0), mvGeoEnd(1.0), mpSimTrafo(NULL),
	mAccurateGeoinit(0),
	mName("lbm_default") ,
	mpIso( NULL ), mIsoValue(0.499),
	mSilent(false) , 
	mLbmInitId(1) ,
	mpGiTree( NULL ),
	mpGiObjects( NULL ), mGiObjInside(), mpGlob( NULL ),
	mRefinementDesired(0),
	mOutputSurfacePreview(0), mPreviewFactor(0.25),
	mSmoothSurface(1.0), mSmoothNormals(1.0),
	mIsoSubdivs(1), mPartGenProb(0.),
	mDumpVelocities(false),
	mMarkedCells(), mMarkedCellIndex(0),
	mDomainBound("noslip"), mDomainPartSlipValue(0.1),
	mFarFieldSize(0.), 
	mPartDropMassSub(0.1),  // good default
	mPartUsePhysModel(false),
	mTForceStrength(0.0), 
	mCppfStage(0),
	mDumpRawText(false),
	mDumpRawBinary(false),
	mDumpRawBinaryZip(true)
{
#if ELBEEM_PLUGIN==1
	if(gDebugLevel<=1) setSilent(true);
#endif
	mpSimTrafo = new ntlMat4Gfx(0.0); 
	mpSimTrafo->initId();

	if(getenv("ELBEEM_RAWDEBUGDUMP")) { 
		debMsgStd("LbmSolverInterface",DM_MSG,"Using env var ELBEEM_RAWDEBUGDUMP, mDumpRawText on",2);
		mDumpRawText = true;
	}

	if(getenv("ELBEEM_BINDEBUGDUMP")) { 
		debMsgStd("LbmSolverInterface",DM_MSG,"Using env var ELBEEM_RAWDEBUGDUMP, mDumpRawText on",2);
		mDumpRawBinary = true;
	}
}

LbmSolverInterface::~LbmSolverInterface() 
{ 
	if(mpSimTrafo) delete mpSimTrafo;
}


/******************************************************************************
 * initialize correct grid sizes given a geometric bounding box
 * and desired grid resolutions, all params except maxrefine
 * will be modified
 *****************************************************************************/
void initGridSizes(int &sizex, int &sizey, int &sizez,
		ntlVec3Gfx &geoStart, ntlVec3Gfx &geoEnd, 
		int mMaxRefine, bool parallel) 
{
	// fix size inits to force cubic cells and mult4 level dimensions
	const int debugGridsizeInit = 1;
  if(debugGridsizeInit) debMsgStd("initGridSizes",DM_MSG,"Called - size X:"<<sizex<<" Y:"<<sizey<<" Z:"<<sizez<<" "<<geoStart<<","<<geoEnd ,10);

	int maxGridSize = sizex; // get max size
	if(sizey>maxGridSize) maxGridSize = sizey;
	if(sizez>maxGridSize) maxGridSize = sizez;
	LbmFloat maxGeoSize = (geoEnd[0]-geoStart[0]); // get max size
	if((geoEnd[1]-geoStart[1])>maxGeoSize) maxGeoSize = (geoEnd[1]-geoStart[1]);
	if((geoEnd[2]-geoStart[2])>maxGeoSize) maxGeoSize = (geoEnd[2]-geoStart[2]);
	// FIXME better divide max geo size by corresponding resolution rather than max? no prob for rx==ry==rz though
	LbmFloat cellSize = (maxGeoSize / (LbmFloat)maxGridSize);
  if(debugGridsizeInit) debMsgStd("initGridSizes",DM_MSG,"Start:"<<geoStart<<" End:"<<geoEnd<<" maxS:"<<maxGeoSize<<" maxG:"<<maxGridSize<<" cs:"<<cellSize, 10);
	// force grid sizes according to geom. size, rounded
	sizex = (int) ((geoEnd[0]-geoStart[0]) / cellSize +0.5);
	sizey = (int) ((geoEnd[1]-geoStart[1]) / cellSize +0.5);
	sizez = (int) ((geoEnd[2]-geoStart[2]) / cellSize +0.5);
	// match refinement sizes, round downwards to multiple of 4
	int sizeMask = 0;
	int maskBits = mMaxRefine;
	if(parallel==1) maskBits+=2;
	for(int i=0; i<maskBits; i++) { sizeMask |= (1<<i); }

	// at least size 4 on coarsest level
	int minSize = 4<<mMaxRefine; //(maskBits+2);
	if(sizex<minSize) sizex = minSize;
	if(sizey<minSize) sizey = minSize;
	if(sizez<minSize) sizez = minSize;
	
	sizeMask = ~sizeMask;
  if(debugGridsizeInit) debMsgStd("initGridSizes",DM_MSG,"Size X:"<<sizex<<" Y:"<<sizey<<" Z:"<<sizez<<" m"<<convertFlags2String(sizeMask)<<", maskBits:"<<maskBits<<",minSize:"<<minSize ,10);
	sizex &= sizeMask;
	sizey &= sizeMask;
	sizez &= sizeMask;
	// debug
	int nextinc = (~sizeMask)+1;
  debMsgStd("initGridSizes",DM_MSG,"MPTeffMLtest, curr size:"<<PRINT_VEC(sizex,sizey,sizez)<<", next size:"<<PRINT_VEC(sizex+nextinc,sizey+nextinc,sizez+nextinc) ,10);

	// force geom size to match rounded/modified grid sizes
	geoEnd[0] = geoStart[0] + cellSize*(LbmFloat)sizex;
	geoEnd[1] = geoStart[1] + cellSize*(LbmFloat)sizey;
	geoEnd[2] = geoStart[2] + cellSize*(LbmFloat)sizez;
}

void calculateMemreqEstimate( int resx,int resy,int resz, 
		int refine, float farfield,
		double *reqret, double *reqretFine, string *reqstr) {
	// debug estimation?
	const bool debugMemEst = true;
	// COMPRESSGRIDS define is not available here, make sure it matches
	const bool useGridComp = true;
	// make sure we can handle bid numbers here... all double
	double memCnt = 0.0;
	double ddTotalNum = (double)dTotalNum;
	if(reqretFine) *reqretFine = -1.;

	double currResx = (double)resx;
	double currResy = (double)resy;
	double currResz = (double)resz;
	double rcellSize = ((currResx*currResy*currResz) *ddTotalNum);
	memCnt += (double)(sizeof(CellFlagType) * (rcellSize/ddTotalNum +4.0) *2.0);
	if(debugMemEst) debMsgStd("calculateMemreqEstimate",DM_MSG,"res:"<<PRINT_VEC(currResx,currResy,currResz)<<" rcellSize:"<<rcellSize<<" mc:"<<memCnt, 10);
  if(!useGridComp) {
		memCnt += (double)(sizeof(LbmFloat) * (rcellSize +4.0) *2.0);
		if(reqretFine) *reqretFine = (double)(sizeof(LbmFloat) * (rcellSize +4.0) *2.0);
		if(debugMemEst) debMsgStd("calculateMemreqEstimate",DM_MSG," no-comp, mc:"<<memCnt, 10);
	} else {
		double compressOffset = (double)(currResx*currResy*ddTotalNum*2.0);
		memCnt += (double)(sizeof(LbmFloat) * (rcellSize+compressOffset +4.0));
		if(reqretFine) *reqretFine = (double)(sizeof(LbmFloat) * (rcellSize+compressOffset +4.0));
		if(debugMemEst) debMsgStd("calculateMemreqEstimate",DM_MSG," w-comp, mc:"<<memCnt, 10);
	}
	for(int i=refine-1; i>=0; i--) {
		currResx /= 2.0;
		currResy /= 2.0;
		currResz /= 2.0;
		rcellSize = ((currResz*currResy*currResx) *ddTotalNum);
		memCnt += (double)(sizeof(CellFlagType) * (rcellSize/ddTotalNum +4.0) *2.0);
		memCnt += (double)(sizeof(LbmFloat) * (rcellSize +4.0) *2.0);
		if(debugMemEst) debMsgStd("calculateMemreqEstimate",DM_MSG,"refine "<<i<<", mc:"<<memCnt, 10);
	}

	// isosurface memory, use orig res values
	if(farfield>0.) {
		memCnt += (double)( (3*sizeof(int)+sizeof(float)) * ((resx+2)*(resy+2)*(resz+2)) );
	} else {
		// ignore 3 int slices...
		memCnt += (double)( (              sizeof(float)) * ((resx+2)*(resy+2)*(resz+2)) );
	}
	if(debugMemEst) debMsgStd("calculateMemreqEstimate",DM_MSG,"iso, mc:"<<memCnt, 10);

	// cpdata init check missing...

	double memd = memCnt;
	const char *sizeStr = "";
	const double sfac = 1024.0;
	if(memd>sfac){ memd /= sfac; sizeStr="KB"; }
	if(memd>sfac){ memd /= sfac; sizeStr="MB"; }
	if(memd>sfac){ memd /= sfac; sizeStr="GB"; }
	if(memd>sfac){ memd /= sfac; sizeStr="TB"; }

	// return values
	std::ostringstream ret;
	if(memCnt< 1024.0*1024.0) {
		// show full MBs
		ret << (ceil(memd));
	} else {
		// two digits for anything larger than MB
		ret << (ceil(memd*100.0)/100.0);
	}
	ret	<< " "<< sizeStr;
	*reqret = memCnt;
	*reqstr = ret.str();
	//debMsgStd("LbmFsgrSolver::initialize",DM_MSG,"Required Grid memory: "<< memd <<" "<< sizeStr<<" ",4);
}

void LbmSolverInterface::initDomainTrafo(float *mat) { 
	mpSimTrafo->initArrayCheck(mat); 
}

/*******************************************************************************/
/*! parse a boundary flag string */
CellFlagType LbmSolverInterface::readBoundaryFlagInt(string name, int defaultValue, string source,string target, bool needed) {
	string val  = mpSifAttrs->readString(name, "", source, target, needed);
	if(!strcmp(val.c_str(),"")) {
		// no value given...
		return CFEmpty;
	}
	if(!strcmp(val.c_str(),"bnd_no")) {
		return (CellFlagType)( CFBnd );
	}
	if(!strcmp(val.c_str(),"bnd_free")) {
		return (CellFlagType)( CFBnd );
	}
	if(!strcmp(val.c_str(),"fluid")) {
		/* might be used for some in/out flow cases */
		return (CellFlagType)( CFFluid );
	}
	errMsg("LbmSolverInterface::readBoundaryFlagInt","Invalid value '"<<val<<"' " );
# if FSGR_STRICT_DEBUG==1
	errFatal("readBoundaryFlagInt","Strict abort..."<<val, SIMWORLD_INITERROR);
# endif
	return defaultValue;
}

/*******************************************************************************/
/*! parse standard attributes */
void LbmSolverInterface::parseStdAttrList() {
	if(!mpSifAttrs) {
		errFatal("LbmSolverInterface::parseAttrList","mpSifAttrs pointer not initialized!",SIMWORLD_INITERROR);
		return; }

	// st currently unused
	mBoundaryEast  = readBoundaryFlagInt("boundary_east",  mBoundaryEast, "LbmSolverInterface", "mBoundaryEast", false);
	mBoundaryWest  = readBoundaryFlagInt("boundary_west",  mBoundaryWest, "LbmSolverInterface", "mBoundaryWest", false);
	mBoundaryNorth = readBoundaryFlagInt("boundary_north", mBoundaryNorth,"LbmSolverInterface", "mBoundaryNorth", false);
	mBoundarySouth = readBoundaryFlagInt("boundary_south", mBoundarySouth,"LbmSolverInterface", "mBoundarySouth", false);
	mBoundaryTop   = readBoundaryFlagInt("boundary_top",   mBoundaryTop,"LbmSolverInterface", "mBoundaryTop", false);
	mBoundaryBottom= readBoundaryFlagInt("boundary_bottom", mBoundaryBottom,"LbmSolverInterface", "mBoundaryBottom", false);

	mpSifAttrs->readMat4Gfx("domain_trafo" , (*mpSimTrafo), "ntlBlenderDumper","mpSimTrafo", false, mpSimTrafo ); 

	LbmVec sizeVec(mSizex,mSizey,mSizez);
	sizeVec = vec2L( mpSifAttrs->readVec3d("size",  vec2P(sizeVec), "LbmSolverInterface", "sizeVec", false) );
	mSizex = (int)sizeVec[0]; 
	mSizey = (int)sizeVec[1]; 
	mSizez = (int)sizeVec[2];
	// param needs size in any case
	// test solvers might not have mpPara, though
	if(mpParam) mpParam->setSize(mSizex, mSizey, mSizez ); 

	mInitDensityGradient = mpSifAttrs->readBool("initdensitygradient", mInitDensityGradient,"LbmSolverInterface", "mInitDensityGradient", false);
	mIsoValue = mpSifAttrs->readFloat("isovalue", mIsoValue, "LbmOptSolver","mIsoValue", false );

	mDebugVelScale = mpSifAttrs->readFloat("debugvelscale", mDebugVelScale,"LbmSolverInterface", "mDebugVelScale", false);
	mNodeInfoString = mpSifAttrs->readString("nodeinfo", mNodeInfoString, "SimulationLbm","mNodeInfoString", false );

	mDumpVelocities = mpSifAttrs->readBool("dump_velocities", mDumpVelocities, "SimulationLbm","mDumpVelocities", false );
	if(getenv("ELBEEM_DUMPVELOCITIES")) {
		int get = atoi(getenv("ELBEEM_DUMPVELOCITIES"));
		if((get==0)||(get==1)) {
			mDumpVelocities = get;
			debMsgStd("LbmSolverInterface::parseAttrList",DM_NOTIFY,"Using envvar ELBEEM_DUMPVELOCITIES to set mDumpVelocities to "<<get<<","<<mDumpVelocities,8);
		}
	}
	
	// new test vars
	// mTForceStrength set from test solver 
	mFarFieldSize = mpSifAttrs->readFloat("farfieldsize", mFarFieldSize,"LbmSolverInterface", "mFarFieldSize", false);
	// old compat
	float sizeScale = mpSifAttrs->readFloat("test_scale", 0.,"LbmTestdata", "mSizeScale", false);
	if((mFarFieldSize<=0.)&&(sizeScale>0.)) { mFarFieldSize=sizeScale; errMsg("LbmTestdata","Warning - using mSizeScale..."); }

	mPartDropMassSub = mpSifAttrs->readFloat("part_dropmass", mPartDropMassSub,"LbmSolverInterface", "mPartDropMassSub", false);

	mCppfStage = mpSifAttrs->readInt("cppfstage", mCppfStage,"LbmSolverInterface", "mCppfStage", false);
	mPartGenProb = mpSifAttrs->readFloat("partgenprob", mPartGenProb,"LbmFsgrSolver", "mPartGenProb", false);
	mPartUsePhysModel = mpSifAttrs->readBool("partusephysmodel", mPartUsePhysModel,"LbmFsgrSolver", "mPartUsePhysModel", false);
	mIsoSubdivs = mpSifAttrs->readInt("isosubdivs", mIsoSubdivs,"LbmFsgrSolver", "mIsoSubdivs", false);
}


/*******************************************************************************/
/*! geometry initialization */
/*******************************************************************************/

/*****************************************************************************/
/*! init tree for certain geometry init */
void LbmSolverInterface::initGeoTree() {
	if(mpGlob == NULL) { errFatal("LbmSolverInterface::initGeoTree","Requires globals!",SIMWORLD_INITERROR); return; }
	ntlScene *scene = mpGlob->getSimScene();
	mpGiObjects = scene->getObjects();
	mGiObjInside.resize( mpGiObjects->size() );
	mGiObjDistance.resize( mpGiObjects->size() );
	mGiObjSecondDist.resize( mpGiObjects->size() );
	for(size_t i=0; i<mpGiObjects->size(); i++) { 
		if((*mpGiObjects)[i]->getGeoInitIntersect()) mAccurateGeoinit=true;
		//debMsgStd("LbmSolverInterface::initGeoTree",DM_MSG,"id:"<<mLbmInitId<<" obj:"<< (*mpGiObjects)[i]->getName() <<" gid:"<<(*mpGiObjects)[i]->getGeoInitId(), 9 );
	}
	debMsgStd("LbmSolverInterface::initGeoTree",DM_MSG,"Accurate geo init: "<<mAccurateGeoinit, 9)
	debMsgStd("LbmSolverInterface::initGeoTree",DM_MSG,"Objects: "<<mpGiObjects->size() ,10);

	if(mpGiTree != NULL) delete mpGiTree;
	char treeFlag = (1<<(this->mLbmInitId+4));
	mpGiTree = new ntlTree( 
# if FSGR_STRICT_DEBUG!=1
			15, 8,  // TREEwarning - fixed values for depth & maxtriangles here...
# else // FSGR_STRICT_DEBUG==1
			10, 20,  // reduced/slower debugging values
# endif // FSGR_STRICT_DEBUG==1
			scene, treeFlag );
}

/*****************************************************************************/
/*! destroy tree etc. when geometry init done */
void LbmSolverInterface::freeGeoTree() {
	if(mpGiTree != NULL) {
		delete mpGiTree;
		mpGiTree = NULL;
	}
}


int globGeoInitDebug = 0;
int globGICPIProblems = 0;
/*****************************************************************************/
/*! check for a certain flag type at position org */
bool LbmSolverInterface::geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId, gfxReal &distance, int shootDir) {
	// shift ve ctors to avoid rounding errors
	org += ntlVec3Gfx(0.0001);
	OId = -1;

	// select shooting direction
	ntlVec3Gfx dir = ntlVec3Gfx(1., 0., 0.);
	if(shootDir==1) dir = ntlVec3Gfx(0., 1., 0.);
	else if(shootDir==2) dir = ntlVec3Gfx(0., 0., 1.);
	else if(shootDir==-2) dir = ntlVec3Gfx(0., 0., -1.);
	else if(shootDir==-1) dir = ntlVec3Gfx(0., -1., 0.);
	
	ntlRay ray(org, dir, 0, 1.0, mpGlob);
	bool done = false;
	bool inside = false;

	vector<int> giObjFirstHistSide;
	giObjFirstHistSide.resize( mpGiObjects->size() );
	for(size_t i=0; i<mGiObjInside.size(); i++) { 
		mGiObjInside[i] = 0; 
		mGiObjDistance[i] = -1.0; 
		mGiObjSecondDist[i] = -1.0; 
		giObjFirstHistSide[i] = 0; 
	}
	// if not inside, return distance to first hit
	gfxReal firstHit=-1.0;
	int     firstOId = -1;
	if(globGeoInitDebug) errMsg("IIIstart"," isect "<<org<<" f"<<flags<<" acc"<<mAccurateGeoinit);
	
	if(mAccurateGeoinit) {
		while(!done) {
			// find first inside intersection
			ntlTriangle *triIns = NULL;
			distance = -1.0;
			ntlVec3Gfx normal(0.0);
			if(shootDir==0) mpGiTree->intersectX(ray,distance,normal, triIns, flags, true);
			else            mpGiTree->intersect(ray,distance,normal, triIns, flags, true);
			if(triIns) {
				ntlVec3Gfx norg = ray.getOrigin() + ray.getDirection()*distance;
				LbmFloat orientation = dot(normal, dir);
				OId = triIns->getObjectId();
				if(orientation<=0.0) {
					// outside hit
					normal *= -1.0;
					mGiObjInside[OId]++;
					if(giObjFirstHistSide[OId]==0) giObjFirstHistSide[OId] = 1;
					if(globGeoInitDebug) errMsg("IIO"," oid:"<<OId<<" org"<<org<<" norg"<<norg<<" orient:"<<orientation);
				} else {
					// inside hit
					mGiObjInside[OId]++;
					if(mGiObjDistance[OId]<0.0) mGiObjDistance[OId] = distance;
					if(globGeoInitDebug) errMsg("III"," oid:"<<OId<<" org"<<org<<" norg"<<norg<<" orient:"<<orientation);
					if(giObjFirstHistSide[OId]==0) giObjFirstHistSide[OId] = -1;
				}
				norg += normal * getVecEpsilon();
				ray = ntlRay(norg, dir, 0, 1.0, mpGlob);
				// remember first hit distance, in case we're not 
				// inside anything
				if(firstHit<0.0) {
					firstHit = distance;
					firstOId = OId;
				}
			} else {
				// no more intersections... return false
				done = true;
			}
		}

		distance = -1.0;
		for(size_t i=0; i<mGiObjInside.size(); i++) {
			if(mGiObjInside[i]>0) {
				bool mess = false;
				if((mGiObjInside[i]%2)==1) {
					if(giObjFirstHistSide[i] != -1) mess=true;
				} else {
					if(giObjFirstHistSide[i] !=  1) mess=true;
				}
				if(mess) {
					//errMsg("IIIproblem","At "<<org<<" obj "<<i<<" inside:"<<mGiObjInside[i]<<" firstside:"<<giObjFirstHistSide[i] );
					globGICPIProblems++;
					mGiObjInside[i]++; // believe first hit side...
				}
			}
		}
		for(size_t i=0; i<mGiObjInside.size(); i++) {
			if(globGeoInitDebug) errMsg("CHIII","i"<<i<<" ins="<<mGiObjInside[i]<<" t"<<mGiObjDistance[i]<<" d"<<distance);
			if(((mGiObjInside[i]%2)==1)&&(mGiObjDistance[i]>0.0)) {
				if(  (distance<0.0)                             || // first intersection -> good
					  ((distance>0.0)&&(distance>mGiObjDistance[i])) // more than one intersection -> use closest one
						) {						
					distance = mGiObjDistance[i];
					OId = i;
					inside = true;
				} 
			}
		}
		if(!inside) {
			distance = firstHit;
			OId = firstOId;
		}
		if(globGeoInitDebug) errMsg("CHIII","i"<<inside<<"  fh"<<firstHit<<" fo"<<firstOId<<" - h"<<distance<<" o"<<OId);

		return inside;
	} else {

		// find first inside intersection
		ntlTriangle *triIns = NULL;
		distance = -1.0;
		ntlVec3Gfx normal(0.0);
		if(shootDir==0) mpGiTree->intersectX(ray,distance,normal, triIns, flags, true);
		else            mpGiTree->intersect(ray,distance,normal, triIns, flags, true);
		if(triIns) {
			// check outside intersect
			LbmFloat orientation = dot(normal, dir);
			if(orientation<=0.0) return false;

			OId = triIns->getObjectId();
			return true;
		}
		return false;
	}
}
bool LbmSolverInterface::geoInitCheckPointInside(ntlVec3Gfx org, ntlVec3Gfx dir, int flags, int &OId, gfxReal &distance, const gfxReal halfCellsize, bool &thinHit, bool recurse) {
	// shift ve ctors to avoid rounding errors
	org += ntlVec3Gfx(0.0001); //?
	OId = -1;
	ntlRay ray(org, dir, 0, 1.0, mpGlob);
	//int insCnt = 0;
	bool done = false;
	bool inside = false;
	for(size_t i=0; i<mGiObjInside.size(); i++) { 
		mGiObjInside[i] = 0; 
		mGiObjDistance[i] = -1.0; 
		mGiObjSecondDist[i] = -1.0; 
	}
	// if not inside, return distance to first hit
	gfxReal firstHit=-1.0;
	int     firstOId = -1;
	thinHit = false;
	
	if(mAccurateGeoinit) {
		while(!done) {
			// find first inside intersection
			ntlTriangle *triIns = NULL;
			distance = -1.0;
			ntlVec3Gfx normal(0.0);
			mpGiTree->intersect(ray,distance,normal, triIns, flags, true);
			if(triIns) {
				ntlVec3Gfx norg = ray.getOrigin() + ray.getDirection()*distance;
				LbmFloat orientation = dot(normal, dir);
				OId = triIns->getObjectId();
				if(orientation<=0.0) {
					// outside hit
					normal *= -1.0;
					//mGiObjDistance[OId] = -1.0;
					//errMsg("IIO"," oid:"<<OId<<" org"<<org<<" norg"<<norg);
				} else {
					// inside hit
					//if(mGiObjDistance[OId]<0.0) mGiObjDistance[OId] = distance;
					//errMsg("III"," oid:"<<OId<<" org"<<org<<" norg"<<norg);
					if(mGiObjInside[OId]==1) {
						// second inside hit
						if(mGiObjSecondDist[OId]<0.0) mGiObjSecondDist[OId] = distance;
					}
				}
				mGiObjInside[OId]++;
				// always store first hit for thin obj init
				if(mGiObjDistance[OId]<0.0) mGiObjDistance[OId] = distance;

				norg += normal * getVecEpsilon();
				ray = ntlRay(norg, dir, 0, 1.0, mpGlob);
				// remember first hit distance, in case we're not 
				// inside anything
				if(firstHit<0.0) {
					firstHit = distance;
					firstOId = OId;
				}
			} else {
				// no more intersections... return false
				done = true;
				//if(insCnt%2) inside=true;
			}
		}

		distance = -1.0;
		// standard inside check
		for(size_t i=0; i<mGiObjInside.size(); i++) {
			if(((mGiObjInside[i]%2)==1)&&(mGiObjDistance[i]>0.0)) {
				if(  (distance<0.0)                             || // first intersection -> good
					  ((distance>0.0)&&(distance>mGiObjDistance[i])) // more than one intersection -> use closest one
						) {						
					distance = mGiObjDistance[i];
					OId = i;
					inside = true;
				} 
			}
		}
		// now check for thin hits
		if(!inside) {
			distance = -1.0;
			for(size_t i=0; i<mGiObjInside.size(); i++) {
				if((mGiObjInside[i]>=2)&&(mGiObjDistance[i]>0.0)&&(mGiObjSecondDist[i]>0.0)&&
					 (mGiObjDistance[i]<1.0*halfCellsize)&&(mGiObjSecondDist[i]<2.0*halfCellsize) ) {
					if(  (distance<0.0)                             || // first intersection -> good
							((distance>0.0)&&(distance>mGiObjDistance[i])) // more than one intersection -> use closest one
							) {						
						distance = mGiObjDistance[i];
						OId = i;
						inside = true;
						thinHit = true;
					} 
				}
			}
		}
		if(!inside) {
			// check for hit in this cell, opposite to current dir (only recurse once)
			if(recurse) {
				gfxReal r_distance;
				int r_OId;
				bool ret = geoInitCheckPointInside(org, dir*-1.0, flags, r_OId, r_distance, halfCellsize, thinHit, false);
				if((ret)&&(thinHit)) {
					OId = r_OId;
					distance = 0.0; 
					return true;
				}
			}
		}
		// really no hit...
		if(!inside) {
			distance = firstHit;
			OId = firstOId;
			/*if((mGiObjDistance[OId]>0.0)&&(mGiObjSecondDist[OId]>0.0)) {
				const gfxReal thisdist = mGiObjSecondDist[OId]-mGiObjDistance[OId];
				// dont walk over this cell...
				if(thisdist<halfCellsize) distance-=2.0*halfCellsize;
			} // ? */
		}
		//errMsg("CHIII","i"<<inside<<"  fh"<<firstHit<<" fo"<<firstOId<<" - h"<<distance<<" o"<<OId);

		return inside;
	} else {

		// find first inside intersection
		ntlTriangle *triIns = NULL;
		distance = -1.0;
		ntlVec3Gfx normal(0.0);
		mpGiTree->intersect(ray,distance,normal, triIns, flags, true);
		if(triIns) {
			// check outside intersect
			LbmFloat orientation = dot(normal, dir);
			if(orientation<=0.0) return false;

			OId = triIns->getObjectId();
			return true;
		}
		return false;
	}
}


/*****************************************************************************/
ntlVec3Gfx LbmSolverInterface::getGeoMaxMovementVelocity(LbmFloat simtime, LbmFloat stepsize) {
	ntlVec3Gfx max(0.0);
	if(mpGlob == NULL) return max;
	// mpGiObjects has to be inited here...
	
	for(int i=0; i< (int)mpGiObjects->size(); i++) {
		//errMsg("LbmSolverInterface::getGeoMaxMovementVelocity","i="<<i<<" "<< (*mpGiObjects)[i]->getName() ); // DEBUG
		if( (*mpGiObjects)[i]->getGeoInitType() & (FGI_FLUID|FGI_MBNDINFLOW) ){
			//ntlVec3Gfx objMaxVel = obj->calculateMaxVel(sourceTime,targetTime);
			ntlVec3Gfx orgvel = (*mpGiObjects)[i]->calculateMaxVel( simtime, simtime+stepsize );
			if( normNoSqrt(orgvel) > normNoSqrt(max) ) { max = orgvel; } 
			//errMsg("MVT","i"<<i<<" a"<< (*mpGiObjects)[i]->getInitialVelocity(simtime)<<" o"<<orgvel ); // DEBUG
			// TODO check if inflow and simtime 
			ntlVec3Gfx inivel = (*mpGiObjects)[i]->getInitialVelocity(simtime);
			if( normNoSqrt(inivel) > normNoSqrt(max) ) { max = inivel; } 
		}
	}
	errMsg("LbmSolverInterface::getGeoMaxMovementVelocity", "max="<< max ); // DEBUG
	return max;
}




/*******************************************************************************/
/*! cell iteration functions */
/*******************************************************************************/




/*****************************************************************************/
//! add cell to mMarkedCells list
void 
LbmSolverInterface::addCellToMarkedList( CellIdentifierInterface *cid ) {
	for(size_t i=0; i<mMarkedCells.size(); i++) {
		// check if cids alreay in
		if( mMarkedCells[i]->equal(cid) ) return;
		//mMarkedCells[i]->setEnd(false);
	}
	mMarkedCells.push_back( cid );
	//cid->setEnd(true);
}

/*****************************************************************************/
//! marked cell iteration methods
CellIdentifierInterface* 
LbmSolverInterface::markedGetFirstCell( ) {
	if(mMarkedCells.size() > 0){ return mMarkedCells[0]; }
	return NULL;
}

CellIdentifierInterface* 
LbmSolverInterface::markedAdvanceCell() {
	mMarkedCellIndex++;
	if(mMarkedCellIndex>=(int)mMarkedCells.size()) return NULL;
	return mMarkedCells[mMarkedCellIndex];
}

void LbmSolverInterface::markedClearList() {
	// FIXME free cids?
	mMarkedCells.clear();
}

/*******************************************************************************/
/*! string helper functions */
/*******************************************************************************/



// 32k
string convertSingleFlag2String(CellFlagType cflag) {
	CellFlagType flag = cflag;
	if(flag == CFUnused         ) return string("cCFUnused");
	if(flag == CFEmpty          ) return string("cCFEmpty");      
	if(flag == CFBnd            ) return string("cCFBnd");        
	if(flag == CFBndNoslip      ) return string("cCFBndNoSlip");        
	if(flag == CFBndFreeslip    ) return string("cCFBndFreeSlip");        
	if(flag == CFBndPartslip    ) return string("cCFBndPartSlip");        
	if(flag == CFNoInterpolSrc  ) return string("cCFNoInterpolSrc");
	if(flag == CFFluid          ) return string("cCFFluid");      
	if(flag == CFInter          ) return string("cCFInter");      
	if(flag == CFNoNbFluid      ) return string("cCFNoNbFluid");  
	if(flag == CFNoNbEmpty      ) return string("cCFNoNbEmpty");  
	if(flag == CFNoDelete       ) return string("cCFNoDelete");   
	if(flag == CFNoBndFluid     ) return string("cCFNoBndFluid"); 
	if(flag == CFGrNorm         ) return string("cCFGrNorm");     
	if(flag == CFGrFromFine     ) return string("cCFGrFromFine"); 
	if(flag == CFGrFromCoarse   ) return string("cCFGrFromCoarse");
	if(flag == CFGrCoarseInited ) return string("cCFGrCoarseInited");
	if(flag == CFMbndInflow )     return string("cCFMbndInflow");
	if(flag == CFMbndOutflow )    return string("cCFMbndOutflow");
	if(flag == CFInvalid        ) return string("cfINVALID");

	std::ostringstream mult;
	int val = 0;
	if(flag != 0) {
		while(! (flag&1) ) {
			flag = flag>>1;
			val++;
		}
	} else {
		val = -1;
	}
	if(val>=24) {
		mult << "cfOID_" << (flag>>24) <<"_TYPE";
	} else {
		mult << "cfUNKNOWN_" << val <<"_TYPE";
	}
	return mult.str();
}
	
//! helper function to convert flag to string (for debuggin)
string convertCellFlagType2String( CellFlagType cflag ) {
	int flag = cflag;

	const int jmax = sizeof(CellFlagType)*8;
	bool somefound = false;
	std::ostringstream mult;
	mult << "[";
	for(int j=0; j<jmax ; j++) {
		if(flag& (1<<j)) {
			if(somefound) mult << "|";
			mult << j<<"<"<< convertSingleFlag2String( (CellFlagType)(1<<j) ); // this call should always be _non_-recursive
			somefound = true;
		}
	};
	mult << "]";

	// return concatenated string
	if(somefound) return mult.str();

	// empty?
	return string("[emptyCFT]");
}


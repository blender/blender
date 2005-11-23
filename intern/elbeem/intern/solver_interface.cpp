/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann Interface Class
 * contains stuff to be statically compiled
 * 
 *****************************************************************************/

/* LBM Files */ 
#include "solver_interface.h" 
#include "ntl_scene.h"
#include "ntl_ray.h"
#include "elbeem.h"


/*****************************************************************************/
//! common variables 

/*****************************************************************************/
/*! class for solver templating - 3D implementation D3Q19 */

	//! how many dimensions?
	const int LbmD3Q19::cDimension = 3;

	// Wi factors for collide step 
	const LbmFloat LbmD3Q19::cCollenZero    = (1.0/3.0);
	const LbmFloat LbmD3Q19::cCollenOne     = (1.0/18.0);
	const LbmFloat LbmD3Q19::cCollenSqrtTwo = (1.0/36.0);

	//! threshold value for filled/emptied cells 
	const LbmFloat LbmD3Q19::cMagicNr2    = 1.0005;
	const LbmFloat LbmD3Q19::cMagicNr2Neg = -0.0005;
	const LbmFloat LbmD3Q19::cMagicNr     = 1.010001;
	const LbmFloat LbmD3Q19::cMagicNrNeg  = -0.010001;

	//! size of a single set of distribution functions 
	const int    LbmD3Q19::cDfNum      = 19;
	//! direction vector contain vecs for all spatial dirs, even if not used for LBM model
	const int    LbmD3Q19::cDirNum     = 27;

	//const string LbmD3Q19::dfString[ cDfNum ] = { 
	const char* LbmD3Q19::dfString[ cDfNum ] = { 
		" C", " N"," S"," E"," W"," T"," B",
		"NE","NW","SE","SW",
		"NT","NB","ST","SB",
		"ET","EB","WT","WB"
	};

	const int LbmD3Q19::dfNorm[ cDfNum ] = { 
		cDirC, cDirN, cDirS, cDirE, cDirW, cDirT, cDirB, 
		cDirNE, cDirNW, cDirSE, cDirSW, 
		cDirNT, cDirNB, cDirST, cDirSB, 
		cDirET, cDirEB, cDirWT, cDirWB
	};

	const int LbmD3Q19::dfInv[ cDfNum ] = { 
		cDirC,  cDirS, cDirN, cDirW, cDirE, cDirB, cDirT,
		cDirSW, cDirSE, cDirNW, cDirNE,
		cDirSB, cDirST, cDirNB, cDirNT, 
		cDirWB, cDirWT, cDirEB, cDirET
	};

	const int LbmD3Q19::dfRefX[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		cDirSE, cDirSW, cDirNE, cDirNW,
		0, 0, 0, 0, 
		cDirEB, cDirET, cDirWB, cDirWT
	};

	const int LbmD3Q19::dfRefY[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		cDirNW, cDirNE, cDirSW, cDirSE,
		cDirNB, cDirNT, cDirSB, cDirST,
		0, 0, 0, 0
	};

	const int LbmD3Q19::dfRefZ[ cDfNum ] = { 
		0,  0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 
		cDirST, cDirSB, cDirNT, cDirNB,
		cDirWT, cDirWB, cDirET, cDirEB
	};

	// Vector Order 3D:
	//  0   1  2   3  4   5  6       7  8  9 10  11 12 13 14  15 16 17 18     19 20 21 22  23 24 25 26
	//  0,  0, 0,  1,-1,  0, 0,      1,-1, 1,-1,  0, 0, 0, 0,  1, 1,-1,-1,     1,-1, 1,-1,  1,-1, 1,-1
	//  0,  1,-1,  0, 0,  0, 0,      1, 1,-1,-1,  1, 1,-1,-1,  0, 0, 0, 0,     1, 1,-1,-1,  1, 1,-1,-1
	//  0,  0, 0,  0, 0,  1,-1,      0, 0, 0, 0,  1,-1, 1,-1,  1,-1, 1,-1,     1, 1, 1, 1, -1,-1,-1,-1

	const int LbmD3Q19::dfVecX[ cDirNum ] = { 
		0, 0,0, 1,-1, 0,0,
		1,-1,1,-1,
		0,0,0,0,
		1,1,-1,-1,
		 1,-1, 1,-1,
		 1,-1, 1,-1,
	};
	const int LbmD3Q19::dfVecY[ cDirNum ] = { 
		0, 1,-1, 0,0,0,0,
		1,1,-1,-1,
		1,1,-1,-1,
		0,0,0,0,
		 1, 1,-1,-1,
		 1, 1,-1,-1
	};
	const int LbmD3Q19::dfVecZ[ cDirNum ] = { 
		0, 0,0,0,0,1,-1,
		0,0,0,0,
		1,-1,1,-1,
		1,-1,1,-1,
		 1, 1, 1, 1,
		-1,-1,-1,-1
	};

	const LbmFloat LbmD3Q19::dfDvecX[ cDirNum ] = {
		0, 0,0, 1,-1, 0,0,
		1,-1,1,-1,
		0,0,0,0,
		1,1,-1,-1,
		 1,-1, 1,-1,
		 1,-1, 1,-1
	};
	const LbmFloat LbmD3Q19::dfDvecY[ cDirNum ] = {
		0, 1,-1, 0,0,0,0,
		1,1,-1,-1,
		1,1,-1,-1,
		0,0,0,0,
		 1, 1,-1,-1,
		 1, 1,-1,-1
	};
	const LbmFloat LbmD3Q19::dfDvecZ[ cDirNum ] = {
		0, 0,0,0,0,1,-1,
		0,0,0,0,
		1,-1,1,-1,
		1,-1,1,-1,
		 1, 1, 1, 1,
		-1,-1,-1,-1
	};

	/* principal directions */
	const int LbmD3Q19::princDirX[ 2*LbmD3Q19::cDimension ] = { 
		1,-1, 0,0, 0,0
	};
	const int LbmD3Q19::princDirY[ 2*LbmD3Q19::cDimension ] = { 
		0,0, 1,-1, 0,0
	};
	const int LbmD3Q19::princDirZ[ 2*LbmD3Q19::cDimension ] = { 
		0,0, 0,0, 1,-1
	};

	/*! arrays for les model coefficients, inited in lbmsolver constructor */
	LbmFloat LbmD3Q19::lesCoeffDiag[ (cDimension-1)*(cDimension-1) ][ cDirNum ];
	LbmFloat LbmD3Q19::lesCoeffOffdiag[ cDimension ][ cDirNum ];


	const LbmFloat LbmD3Q19::dfLength[ cDfNum ]= { 
		cCollenZero,
		cCollenOne, cCollenOne, cCollenOne, 
		cCollenOne, cCollenOne, cCollenOne,
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo
	};

	/* precalculated equilibrium dfs, inited in lbmsolver constructor */
	LbmFloat LbmD3Q19::dfEquil[ cDfNum ];

// D3Q19 end



/*****************************************************************************/
/*! class for solver templating - 2D implementation D2Q9 */

		//! how many dimensions?
		const int LbmD2Q9::cDimension = 2;

		//! Wi factors for collide step 
		const LbmFloat LbmD2Q9::cCollenZero    = (4.0/9.0);
		const LbmFloat LbmD2Q9::cCollenOne     = (1.0/9.0);
		const LbmFloat LbmD2Q9::cCollenSqrtTwo = (1.0/36.0);

		//! threshold value for filled/emptied cells 
		const LbmFloat LbmD2Q9::cMagicNr2    = 1.0005;
		const LbmFloat LbmD2Q9::cMagicNr2Neg = -0.0005;
		const LbmFloat LbmD2Q9::cMagicNr     = 1.010001;
		const LbmFloat LbmD2Q9::cMagicNrNeg  = -0.010001;

		//! size of a single set of distribution functions 
		const int LbmD2Q9::cDfNum  = 9;
		const int LbmD2Q9::cDirNum = 9;

	//const string LbmD2Q9::dfString[ cDfNum ] = { 
	const char* LbmD2Q9::dfString[ cDfNum ] = { 
		" C", 
		" N",	" S", " E", " W",
		"NE", "NW", "SE","SW" 
	};

	const int LbmD2Q9::dfNorm[ cDfNum ] = { 
		cDirC, 
		cDirN,  cDirS,  cDirE,  cDirW,
		cDirNE, cDirNW, cDirSE, cDirSW 
	};

	const int LbmD2Q9::dfInv[ cDfNum ] = { 
		cDirC,  
		cDirS,  cDirN,  cDirW,  cDirE,
		cDirSW, cDirSE, cDirNW, cDirNE 
	};

	const int LbmD2Q9::dfRefX[ cDfNum ] = { 
		0,  
		0,  0,  0,  0,
		cDirSE, cDirSW, cDirNE, cDirNW 
	};

	const int LbmD2Q9::dfRefY[ cDfNum ] = { 
		0,  
		0,  0,  0,  0,
		cDirNW, cDirNE, cDirSW, cDirSE 
	};

	const int LbmD2Q9::dfRefZ[ cDfNum ] = { 
		0,  0, 0, 0, 0,
		0, 0, 0, 0
	};

	// Vector Order 2D:
	// 0  1 2  3  4  5  6 7  8
	// 0, 0,0, 1,-1, 1,-1,1,-1 
	// 0, 1,-1, 0,0, 1,1,-1,-1 

	const int LbmD2Q9::dfVecX[ cDirNum ] = { 
		0, 
		0,0, 1,-1,
		1,-1,1,-1 
	};
	const int LbmD2Q9::dfVecY[ cDirNum ] = { 
		0, 
		1,-1, 0,0,
		1,1,-1,-1 
	};
	const int LbmD2Q9::dfVecZ[ cDirNum ] = { 
		0, 0,0,0,0, 0,0,0,0 
	};

	const LbmFloat LbmD2Q9::dfDvecX[ cDirNum ] = {
		0, 
		0,0, 1,-1,
		1,-1,1,-1 
	};
	const LbmFloat LbmD2Q9::dfDvecY[ cDirNum ] = {
		0, 
		1,-1, 0,0,
		1,1,-1,-1 
	};
	const LbmFloat LbmD2Q9::dfDvecZ[ cDirNum ] = {
		0, 0,0,0,0, 0,0,0,0 
	};

	const int LbmD2Q9::princDirX[ 2*LbmD2Q9::cDimension ] = { 
		1,-1, 0,0
	};
	const int LbmD2Q9::princDirY[ 2*LbmD2Q9::cDimension ] = { 
		0,0, 1,-1
	};
	const int LbmD2Q9::princDirZ[ 2*LbmD2Q9::cDimension ] = { 
		0,0, 0,0
	};


	/*! arrays for les model coefficients, inited in lbmsolver constructor */
	LbmFloat LbmD2Q9::lesCoeffDiag[ (cDimension-1)*(cDimension-1) ][ cDirNum ];
	LbmFloat LbmD2Q9::lesCoeffOffdiag[ cDimension ][ cDirNum ];


	const LbmFloat LbmD2Q9::dfLength[ cDfNum ]= { 
		cCollenZero,
		cCollenOne, cCollenOne, cCollenOne, cCollenOne, 
		cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo, cCollenSqrtTwo
	};

	/* precalculated equilibrium dfs, inited in lbmsolver constructor */
	LbmFloat LbmD2Q9::dfEquil[ cDfNum ];

// D2Q9 end



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
	mpAttrs( NULL ), mpParam( NULL ),
	mNumParticlesLost(0), mNumInvalidDfs(0), mNumFilledCells(0), mNumEmptiedCells(0), mNumUsedCells(0), mMLSUPS(0),
	mDebugVelScale( 0.01 ), mNodeInfoString("+"),
	mRandom( 5123 ),
	mvGeoStart(-1.0), mvGeoEnd(1.0),
	mAccurateGeoinit(0),
	mName("lbm_default") ,
	mpIso( NULL ), mIsoValue(0.499),
	mSilent(false) , 
	mGeoInitId( 1 ),
	mpGiTree( NULL ),
	mpGiObjects( NULL ), mGiObjInside(), mpGlob( NULL ),
	mRefinementDesired(0),
	mOutputSurfacePreview(0), mPreviewFactor(0.25),
	mSmoothSurface(0.0), mSmoothNormals(0.0),
	mMarkedCells(), mMarkedCellIndex(0)
{
#if ELBEEM_BLENDER==1
	if(gDebugLevel<=1) mSilent = true;
#endif
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
  if(debugGridsizeInit) debMsgStd("initGridSizes",DM_MSG,"Called - size X:"<<sizex<<" Y:"<<sizey<<" Z:"<<sizez<<" " ,10);

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
	int minSize = 2<<(maskBits+2);
	if(sizex<minSize) sizex = minSize;
	if(sizey<minSize) sizey = minSize;
	if(sizez<minSize) sizez = minSize;
	
	sizeMask = ~sizeMask;
  if(debugGridsizeInit) debMsgStd("initGridSizes",DM_MSG,"Size X:"<<sizex<<" Y:"<<sizey<<" Z:"<<sizez<<" m"<<convertFlags2String(sizeMask) ,10);
	sizex &= sizeMask;
	sizey &= sizeMask;
	sizez &= sizeMask;

	// force geom size to match rounded/modified grid sizes
	geoEnd[0] = geoStart[0] + cellSize*(LbmFloat)sizex;
	geoEnd[1] = geoStart[1] + cellSize*(LbmFloat)sizey;
	geoEnd[2] = geoStart[2] + cellSize*(LbmFloat)sizez;
}

void calculateMemreqEstimate( int resx,int resy,int resz, int refine,
		double *reqret, string *reqstr) {
	// make sure we can handle bid numbers here... all double
	double memCnt = 0.0;
	double ddTotalNum = (double)dTotalNum;

	double currResx = (double)resx;
	double currResy = (double)resy;
	double currResz = (double)resz;
	double rcellSize = ((currResx*currResy*currResz) *ddTotalNum);
	memCnt += (double)(sizeof(CellFlagType) * (rcellSize/ddTotalNum +4.0) *2.0);
#if COMPRESSGRIDS==0
	memCnt += (double)(sizeof(LbmFloat) * (rcellSize +4.0) *2.0);
#else // COMPRESSGRIDS==0
	double compressOffset = (double)(currResx*currResy*ddTotalNum*2.0);
	memCnt += (double)(sizeof(LbmFloat) * (rcellSize+compressOffset +4.0));
#endif // COMPRESSGRIDS==0
	for(int i=refine-1; i>=0; i--) {
		currResx /= 2.0;
		currResy /= 2.0;
		currResz /= 2.0;
		rcellSize = ((currResz*currResy*currResx) *ddTotalNum);
		memCnt += (double)(sizeof(CellFlagType) * (rcellSize/ddTotalNum +4.0) *2.0);
		memCnt += (double)(sizeof(LbmFloat) * (rcellSize +4.0) *2.0);
	}

	// isosurface memory
	memCnt += (double)( (3*sizeof(int)+sizeof(float)) * ((resx+2)*(resy+2)*(resz+2)) );

	double memd = memCnt;
	char *sizeStr = "";
	const double sfac = 1000.0;
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


/*******************************************************************************/
/*! parse a boundary flag string */
CellFlagType LbmSolverInterface::readBoundaryFlagInt(string name, int defaultValue, string source,string target, bool needed) {
	string val  = mpAttrs->readString(name, "", source, target, needed);
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
# if LBM_STRICT_DEBUG==1
	errFatal("readBoundaryFlagInt","Strict abort..."<<val, SIMWORLD_INITERROR);
# endif
	return defaultValue;
}

/*******************************************************************************/
/*! parse standard attributes */
void LbmSolverInterface::parseStdAttrList() {
	if(!mpAttrs) {
		errFatal("LbmSolverInterface::parseAttrList","mpAttrs pointer not initialized!",SIMWORLD_INITERROR);
		return; }

	// st currently unused
	//mSurfaceTension  = mpAttrs->readFloat("d_surfacetension",  mSurfaceTension, "LbmSolverInterface", "mSurfaceTension", false);
	mBoundaryEast  = readBoundaryFlagInt("boundary_east",  mBoundaryEast, "LbmSolverInterface", "mBoundaryEast", false);
	mBoundaryWest  = readBoundaryFlagInt("boundary_west",  mBoundaryWest, "LbmSolverInterface", "mBoundaryWest", false);
	mBoundaryNorth = readBoundaryFlagInt("boundary_north", mBoundaryNorth,"LbmSolverInterface", "mBoundaryNorth", false);
	mBoundarySouth = readBoundaryFlagInt("boundary_south", mBoundarySouth,"LbmSolverInterface", "mBoundarySouth", false);
	mBoundaryTop   = readBoundaryFlagInt("boundary_top",   mBoundaryTop,"LbmSolverInterface", "mBoundaryTop", false);
	mBoundaryBottom= readBoundaryFlagInt("boundary_bottom", mBoundaryBottom,"LbmSolverInterface", "mBoundaryBottom", false);

	LbmVec sizeVec(mSizex,mSizey,mSizez);
	sizeVec = vec2L( mpAttrs->readVec3d("size",  vec2P(sizeVec), "LbmSolverInterface", "sizeVec", false) );
	mSizex = (int)sizeVec[0]; 
	mSizey = (int)sizeVec[1]; 
	mSizez = (int)sizeVec[2];
	mpParam->setSize(mSizex, mSizey, mSizez ); // param needs size in any case

	mInitDensityGradient = mpAttrs->readBool("initdensitygradient", mInitDensityGradient,"LbmSolverInterface", "mInitDensityGradient", false);
	mGeoInitId = mpAttrs->readInt("geoinitid", mGeoInitId,"LbmSolverInterface", "mGeoInitId", false);
	mIsoValue = mpAttrs->readFloat("isovalue", mIsoValue, "LbmOptSolver","mIsoValue", false );

	mDebugVelScale = mpAttrs->readFloat("debugvelscale", mDebugVelScale,"LbmSolverInterface", "mDebugVelScale", false);
	mNodeInfoString = mpAttrs->readString("nodeinfo", mNodeInfoString, "SimulationLbm","mNodeInfoString", false );
}


/*******************************************************************************/
/*! geometry initialization */
/*******************************************************************************/

/*****************************************************************************/
/*! init tree for certain geometry init */
void LbmSolverInterface::initGeoTree(int id) {
	if(mpGlob == NULL) { errFatal("LbmSolverInterface::initGeoTree","Requires globals!",SIMWORLD_INITERROR); return; }
	mGeoInitId = id;
	ntlScene *scene = mpGlob->getScene();
	mpGiObjects = scene->getObjects();
	mGiObjInside.resize( mpGiObjects->size() );
	mGiObjDistance.resize( mpGiObjects->size() );
	mGiObjSecondDist.resize( mpGiObjects->size() );
	for(size_t i=0; i<mpGiObjects->size(); i++) { 
		if((*mpGiObjects)[i]->getGeoInitIntersect()) mAccurateGeoinit=true;
	}
	debMsgStd("LbmSolverInterface::initGeoTree",DM_MSG,"Accurate geo init: "<<mAccurateGeoinit, 9)

	if(mpGiTree != NULL) delete mpGiTree;
	char treeFlag = (1<<(mGeoInitId+4));
	mpGiTree = new ntlTree( 
			15, 8,  // warning - fixed values for depth & maxtriangles here...
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
/*****************************************************************************/
/*! check for a certain flag type at position org */
bool LbmSolverInterface::geoInitCheckPointInside(ntlVec3Gfx org, int flags, int &OId, gfxReal &distance) {
	// shift ve ctors to avoid rounding errors
	org += ntlVec3Gfx(0.0001);
	ntlVec3Gfx dir = ntlVec3Gfx(1.0, 0.0, 0.0);
	OId = -1;
	ntlRay ray(org, dir, 0, 1.0, mpGlob);
	//int insCnt = 0;
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
			mpGiTree->intersectX(ray,distance,normal, triIns, flags, true);
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
					errMsg("IIIproblem","At "<<org<<" obj "<<i<<" inside:"<<mGiObjInside[i]<<" firstside:"<<giObjFirstHistSide[i] );
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
		mpGiTree->intersectX(ray,distance,normal, triIns, flags, true);
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
/*! get max. velocity of all objects to initialize as fluid regions or inflow */
ntlVec3Gfx LbmSolverInterface::getGeoMaxInitialVelocity() {
	ntlVec3Gfx max(0.0);
	if(mpGlob == NULL) return max;

	ntlScene *scene = mpGlob->getScene();
	mpGiObjects = scene->getObjects();
	
	for(int i=0; i< (int)mpGiObjects->size(); i++) {
		if( (*mpGiObjects)[i]->getGeoInitType() & (FGI_FLUID|FGI_MBNDINFLOW) ){
			ntlVec3Gfx ovel = (*mpGiObjects)[i]->getInitialVelocity();
			if( normNoSqrt(ovel) > normNoSqrt(max) ) { max = ovel; } 
			//errMsg("IVT","i"<<i<<" "<< (*mpGiObjects)[i]->getInitialVelocity() ); // DEBUG
		}
	}
	//errMsg("IVT","max "<<" "<< max ); // DEBUG
	// unused !? mGiInsideCnt.resize( mpGiObjects->size() );

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


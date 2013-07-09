/** \file elbeem/intern/loop_tools.h
 *  \ingroup elbeem
 */

// advance pointer in main loop
#define ADVANCE_POINTERS(p)	\
	ccel += (QCELLSTEP*(p));	\
	tcel += (QCELLSTEP*(p));	\
	pFlagSrc+= (p); \
	pFlagDst+= (p); \
	i+= (p);

#define MAX_CALC_ARR 4

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// init region vars
#define  GRID_REGION_INIT()   \
	const int istart = -1+gridLoopBound; \
	const int iend   = mLevel[mMaxRefine].lSizex-1-gridLoopBound; \
	LbmFloat calcCurrentMass=0; \
	LbmFloat calcCurrentVolume=0; \
	int      calcCellsFilled=0; \
	int      calcCellsEmptied=0; \
	int      calcNumUsedCells=0; \




//  -----------------------------------------------------------------------------------
// serial stuff
#if PARALLEL!=1

#define PERFORM_USQRMAXCHECK USQRMAXCHECK(usqr,ux,uy,uz, mMaxVlen, mMxvx,mMxvy,mMxvz);
#define LIST_EMPTY(x) mListEmpty.push_back( x );
#define LIST_FULL(x)  mListFull.push_back( x );
#define FSGR_ADDPART(x)  mpParticles->addFullParticle( x );

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define  GRID_REGION_START()  \
	{ /* main_region */ \
	int kstart=getForZMinBnd(), kend=getForZMaxBnd(mMaxRefine); \
	if(gridLoopBound>0){ kstart=getForZMin1(), kend=getForZMax1(mMaxRefine); } \
	int kdir = 1; \
	int jstart = gridLoopBound; \
	int jend   = mLevel[mMaxRefine].lSizey-gridLoopBound; \
	const int id=0; \
	LbmFloat *ccel = NULL, *tcel = NULL; \
	CellFlagType *pFlagSrc=NULL, *pFlagDst=NULL; \
	if(mLevel[mMaxRefine].setCurr==1) { \
	kdir = -1; \
	int temp = kend; \
	kend = kstart-1; \
	kstart = temp-1; \
	temp = id; /* dummy remove warning */ \
	} \



	

//  -----------------------------------------------------------------------------------
#else // PARALLEL==1

//#include "paraloop.h"
#define PERFORM_USQRMAXCHECK USQRMAXCHECK(usqr,ux,uy,uz, calcMaxVlen, calcMxvx,calcMxvy,calcMxvz);
#define LIST_EMPTY(x)    calcListEmpty.push_back( x );
#define LIST_FULL(x)     calcListFull.push_back( x );
#define FSGR_ADDPART(x)  calcListParts.push_back( x );


// parallel region
//was: # pragma omp parallel default(shared) 
#if COMPRESSGRIDS!=1
	// requires compressed grids...!
	ERROR!
#endif

// loop start
#define  GRID_REGION_START()  \
	{ \
	 \
	 \
	if(mSizez<2) { \
	mPanic = 1; \
	errFatal("ParaLoop::2D","Not valid...!", SIMWORLD_GENERICERROR); \
	} \
	 \
	 \
	vector<LbmPoint> calcListFull; \
	vector<LbmPoint> calcListEmpty; \
	vector<ParticleObject> calcListParts; \
	LbmFloat calcMxvx, calcMxvy, calcMxvz, calcMaxVlen; \
	calcMxvx = calcMxvy = calcMxvz = calcMaxVlen = 0.0; \
	calcListEmpty.reserve(mListEmpty.capacity() / omp_get_num_threads() ); \
	calcListFull.reserve( mListFull.capacity()  / omp_get_num_threads() ); \
	calcListParts.reserve(mSizex); \
	 \
	 \
	const int id = omp_get_thread_num(); \
	const int Nthrds = omp_get_num_threads(); \
	 \
	 \
	 \
	 \
	 \
	int kdir = 1; \
	 \
	int kstart=getForZMinBnd(), kend=getForZMaxBnd(mMaxRefine); \
	if(gridLoopBound>0){ kstart=getForZMin1(); kend=getForZMax1(mMaxRefine); } \
	LbmFloat *ccel = NULL, *tcel = NULL; \
	CellFlagType *pFlagSrc=NULL, *pFlagDst=NULL; \
	 \
	 \
	if(mLevel[mMaxRefine].setCurr==1) { \
	kdir = -1; \
	int temp = kend; \
	kend = kstart-1; \
	kstart = temp-1; \
	} \
	 \
	const int Nj = mLevel[mMaxRefine].lSizey; \
	int jstart = 0+( (id * Nj ) / Nthrds ); \
	int jend   = 0+(((id+1) * Nj ) / Nthrds ); \
	if( ((Nj/Nthrds) *Nthrds) != Nj) { \
	errMsg("LbmFsgrSolver","Invalid domain size Nj="<<Nj<<" Nthrds="<<Nthrds); \
	} \
	 \
	if(jstart<gridLoopBound) jstart = gridLoopBound; \
	if(jend>mLevel[mMaxRefine].lSizey-gridLoopBound) jend = mLevel[mMaxRefine].lSizey-gridLoopBound; \
	 \
	debMsgStd("ParaLoop::OMP",DM_MSG,"Thread:"<<id<<" i:"<<istart<<"-"<<iend<<" j:"<<jstart<<"-"<<jend<<", k:"<<kstart<<"-"<<kend<<"  ", 1); \
	 \




// para GRID LOOP END is parainc3 

#endif // PARALLEL==1


//  -----------------------------------------------------------------------------------

// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define  GRID_LOOP_START()   \
	for(int k=kstart;k!=kend;k+=kdir) { \
	pFlagSrc = &RFLAG(lev, istart, jstart, k, SRCS(lev)); \
	pFlagDst = &RFLAG(lev, istart, jstart, k, TSET(lev)); \
	ccel = RACPNT(lev,     istart, jstart, k, SRCS(lev)); \
	tcel = RACPNT(lev,     istart, jstart, k, TSET(lev)); \
	for(int j=jstart;j!=jend;++j) { \
	/* for(int i=0;i<mLevel[lev].lSizex-2;   ) { */ \
	for(int i=istart;i!=iend;   ) { \
	ADVANCE_POINTERS(1); \




// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
#define  GRID_LOOPREG_END()  \
	 \
	} /* i */ \
	int i=0; \
	ADVANCE_POINTERS(2*gridLoopBound); \
	} /* j */ \
	/* COMPRESSGRIDS!=1 */ \
	/* int i=0;  */ \
	/* ADVANCE_POINTERS(mLevel[lev].lSizex*2);  */ \
	} /* all cell loop k,j,i */ \
	if(doReduce) { } /* dummy remove warning */ \
	} /* main_region */ \
	 \




// old loop for COMPRESSGRIDS==0
#define old__GRID_LOOP_START() \
  for(int k=kstart;k<kend;++k) { \
	  for(int j=1;j<mLevel[lev].lSizey-1;++j) { \
  		for(int i=0;i<mLevel[lev].lSizex-2;   ) {



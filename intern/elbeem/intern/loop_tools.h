
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



	
#define unused_GRID_REGION_END() \
	} /* main_region */  \
	// end unusedGRID_REGION_END


//  -----------------------------------------------------------------------------------
#else // PARALLEL==1

#include "paraloop.h"

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



#define PERFORM_USQRMAXCHECK \
_Pragma("omp critical") {\
USQRMAXCHECK(usqr,ux,uy,uz, mMaxVlen, mMxvx,mMxvy,mMxvz); \
} \


#define LIST_EMPTY(x) \
_Pragma("omp critical") {\
mListEmpty.push_back( x ); }

#define LIST_FULL(x) \
_Pragma("omp critical") {\
mListFull.push_back( x ); }

#define FSGR_ADDPART(x)  \
_Pragma("omp critical") { \
mpParticles->addFullParticle( x ); } \


#define MAX_THREADS 2

#define  GRID_REGION_START()  \
{ /* main_region */ \
	int kstart=getForZMinBnd(), kend=getForZMaxBnd(mMaxRefine); \
	if(gridLoopBound>0){ kstart=getForZMin1(), kend=getForZMax1(mMaxRefine); } \
	int kdir = 1; \
	const int id=omp_get_thread_num(); \
	int jstart = (id*((mLevel[mMaxRefine].lSizey-gridLoopBound) / MAX_THREADS))+gridLoopBound; \
	int jend   = (id+1)*((mLevel[mMaxRefine].lSizey-gridLoopBound)/ MAX_THREADS); \
	if(id+1 == MAX_THREADS) \
	{ \
	jend = mLevel[mMaxRefine].lSizey-gridLoopBound; \
	} \
	if(jstart<1) jstart = 1; \
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


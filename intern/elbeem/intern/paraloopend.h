
// same as grid loop_end + barrier

			} // i
		int i=0; //dummy
		ADVANCE_POINTERS(2*gridLoopBound);
	} // j

#	if COMPRESSGRIDS==1
#	if PARALLEL==1
	//frintf(stderr," (id=%d k=%d) ",id,k);
#pragma omp barrier
#	endif // PARALLEL==1
#	else // COMPRESSGRIDS==1
	int i=0; //dummy
	ADVANCE_POINTERS(mLevel[lev].lSizex*2);
#	endif // COMPRESSGRIDS==1

} // all cell loop k,j,i

#pragma omp critical
{
	if(doReduce) {
		// synchronize global vars
		for(size_t j=0; j<calcListFull.size() ; j++) mListFull.push_back( calcListFull[j] ); 
		for(size_t j=0; j<calcListEmpty.size(); j++) mListEmpty.push_back( calcListEmpty[j] ); 
		for(size_t j=0; j<calcListParts.size(); j++) mpParticles->addFullParticle( calcListParts[j] );
		if(calcMaxVlen>mMaxVlen) {  
			mMxvx = calcMxvx;  
			mMxvy = calcMxvy;  
			mMxvz = calcMxvz;  
			mMaxVlen = calcMaxVlen;  
		}  
		if(0) {debMsgStd("OMP_CRIT",DM_MSG,	"reduce id"<<id<<" curr: "<<mMaxVlen<<"|"<<mMxvx<<","<<mMxvy<<","<<mMxvz<< 
																				"      calc[ "<<calcMaxVlen<<"|"<<calcMxvx<<","<<calcMxvy<<","<<calcMxvz<<"]  " ,4 ); }
	}
} // critical


} /* main_region */  
	//?lobOutstrForce = true;


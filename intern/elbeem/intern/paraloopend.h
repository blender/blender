// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
} /* i */ 
	int i=0; 
	ADVANCE_POINTERS(2*gridLoopBound); 
} /* j */ 
#pragma omp barrier 
	/* COMPRESSGRIDS!=1 */ 
	/* int i=0;  */ 
	/* ADVANCE_POINTERS(mLevel[lev].lSizex*2);  */ 
} /* all cell loop k,j,i */ 
	if(doReduce) { } /* dummy remove warning */ 
} /* main_region */ 


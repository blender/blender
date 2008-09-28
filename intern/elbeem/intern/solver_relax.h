/******************************************************************************
 *
 * El'Beem - the visual lattice boltzmann freesurface simulator
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann relaxation macros
 *
 *****************************************************************************/

#if FSGR_STRICT_DEBUG==1
#define CAUSE_PANIC { this->mPanic=1; /* *((int*)(0x0)) = 1; crash*/ }
#else // FSGR_STRICT_DEBUG==1
#define CAUSE_PANIC { this->mPanic=1; } /*set flag*/
#endif // FSGR_STRICT_DEBUG==1
	
/******************************************************************************
 * normal relaxation
 *****************************************************************************/

// standard arrays
#define CSRC_C    RAC(ccel                                , dC )
#define CSRC_E    RAC(ccel + (-1)             *(dTotalNum), dE )
#define CSRC_W    RAC(ccel + (+1)             *(dTotalNum), dW )
#define CSRC_N    RAC(ccel + (-mLevel[lev].lOffsx)        *(dTotalNum), dN )
#define CSRC_S    RAC(ccel + (+mLevel[lev].lOffsx)        *(dTotalNum), dS )
#define CSRC_NE   RAC(ccel + (-mLevel[lev].lOffsx-1)      *(dTotalNum), dNE)
#define CSRC_NW   RAC(ccel + (-mLevel[lev].lOffsx+1)      *(dTotalNum), dNW)
#define CSRC_SE   RAC(ccel + (+mLevel[lev].lOffsx-1)      *(dTotalNum), dSE)
#define CSRC_SW   RAC(ccel + (+mLevel[lev].lOffsx+1)      *(dTotalNum), dSW)
#define CSRC_T    RAC(ccel + (-mLevel[lev].lOffsy)        *(dTotalNum), dT )
#define CSRC_B    RAC(ccel + (+mLevel[lev].lOffsy)        *(dTotalNum), dB )
#define CSRC_ET   RAC(ccel + (-mLevel[lev].lOffsy-1)      *(dTotalNum), dET)
#define CSRC_EB   RAC(ccel + (+mLevel[lev].lOffsy-1)      *(dTotalNum), dEB)
#define CSRC_WT   RAC(ccel + (-mLevel[lev].lOffsy+1)      *(dTotalNum), dWT)
#define CSRC_WB   RAC(ccel + (+mLevel[lev].lOffsy+1)      *(dTotalNum), dWB)
#define CSRC_NT   RAC(ccel + (-mLevel[lev].lOffsy-mLevel[lev].lOffsx) *(dTotalNum), dNT)
#define CSRC_NB   RAC(ccel + (+mLevel[lev].lOffsy-mLevel[lev].lOffsx) *(dTotalNum), dNB)
#define CSRC_ST   RAC(ccel + (-mLevel[lev].lOffsy+mLevel[lev].lOffsx) *(dTotalNum), dST)
#define CSRC_SB   RAC(ccel + (+mLevel[lev].lOffsy+mLevel[lev].lOffsx) *(dTotalNum), dSB)

#define XSRC_C(x)    RAC(ccel + (x)                 *dTotalNum, dC )
#define XSRC_E(x)    RAC(ccel + ((x)-1)             *dTotalNum, dE )
#define XSRC_W(x)    RAC(ccel + ((x)+1)             *dTotalNum, dW )
#define XSRC_N(x)    RAC(ccel + ((x)-mLevel[lev].lOffsx)        *dTotalNum, dN )
#define XSRC_S(x)    RAC(ccel + ((x)+mLevel[lev].lOffsx)        *dTotalNum, dS )
#define XSRC_NE(x)   RAC(ccel + ((x)-mLevel[lev].lOffsx-1)      *dTotalNum, dNE)
#define XSRC_NW(x)   RAC(ccel + ((x)-mLevel[lev].lOffsx+1)      *dTotalNum, dNW)
#define XSRC_SE(x)   RAC(ccel + ((x)+mLevel[lev].lOffsx-1)      *dTotalNum, dSE)
#define XSRC_SW(x)   RAC(ccel + ((x)+mLevel[lev].lOffsx+1)      *dTotalNum, dSW)
#define XSRC_T(x)    RAC(ccel + ((x)-mLevel[lev].lOffsy)        *dTotalNum, dT )
#define XSRC_B(x)    RAC(ccel + ((x)+mLevel[lev].lOffsy)        *dTotalNum, dB )
#define XSRC_ET(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy-1)      *dTotalNum, dET)
#define XSRC_EB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy-1)      *dTotalNum, dEB)
#define XSRC_WT(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy+1)      *dTotalNum, dWT)
#define XSRC_WB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy+1)      *dTotalNum, dWB)
#define XSRC_NT(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy-mLevel[lev].lOffsx) *dTotalNum, dNT)
#define XSRC_NB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy-mLevel[lev].lOffsx) *dTotalNum, dNB)
#define XSRC_ST(x)   RAC(ccel + ((x)-mLevel[lev].lOffsy+mLevel[lev].lOffsx) *dTotalNum, dST)
#define XSRC_SB(x)   RAC(ccel + ((x)+mLevel[lev].lOffsy+mLevel[lev].lOffsx) *dTotalNum, dSB)



#define OMEGA(l) mLevel[(l)].omega

#define EQC (  DFL1*(rho - usqr))
#define EQN (  DFL2*(rho + uy*(4.5*uy + 3.0) - usqr))
#define EQS (  DFL2*(rho + uy*(4.5*uy - 3.0) - usqr))
#define EQE (  DFL2*(rho + ux*(4.5*ux + 3.0) - usqr))
#define EQW (  DFL2*(rho + ux*(4.5*ux - 3.0) - usqr))
#define EQT (  DFL2*(rho + uz*(4.5*uz + 3.0) - usqr))
#define EQB (  DFL2*(rho + uz*(4.5*uz - 3.0) - usqr))
                    
#define EQNE ( DFL3*(rho + (+ux+uy)*(4.5*(+ux+uy) + 3.0) - usqr))
#define EQNW ( DFL3*(rho + (-ux+uy)*(4.5*(-ux+uy) + 3.0) - usqr))
#define EQSE ( DFL3*(rho + (+ux-uy)*(4.5*(+ux-uy) + 3.0) - usqr))
#define EQSW ( DFL3*(rho + (-ux-uy)*(4.5*(-ux-uy) + 3.0) - usqr))
#define EQNT ( DFL3*(rho + (+uy+uz)*(4.5*(+uy+uz) + 3.0) - usqr))
#define EQNB ( DFL3*(rho + (+uy-uz)*(4.5*(+uy-uz) + 3.0) - usqr))
#define EQST ( DFL3*(rho + (-uy+uz)*(4.5*(-uy+uz) + 3.0) - usqr))
#define EQSB ( DFL3*(rho + (-uy-uz)*(4.5*(-uy-uz) + 3.0) - usqr))
#define EQET ( DFL3*(rho + (+ux+uz)*(4.5*(+ux+uz) + 3.0) - usqr))
#define EQEB ( DFL3*(rho + (+ux-uz)*(4.5*(+ux-uz) + 3.0) - usqr))
#define EQWT ( DFL3*(rho + (-ux+uz)*(4.5*(-ux+uz) + 3.0) - usqr))
#define EQWB ( DFL3*(rho + (-ux-uz)*(4.5*(-ux-uz) + 3.0) - usqr))


// this is a bit ugly, but necessary for the CSRC_ access...
#define MSRC_C    m[dC ]
#define MSRC_N    m[dN ]
#define MSRC_S    m[dS ]
#define MSRC_E    m[dE ]
#define MSRC_W    m[dW ]
#define MSRC_T    m[dT ]
#define MSRC_B    m[dB ]
#define MSRC_NE   m[dNE]
#define MSRC_NW   m[dNW]
#define MSRC_SE   m[dSE]
#define MSRC_SW   m[dSW]
#define MSRC_NT   m[dNT]
#define MSRC_NB   m[dNB]
#define MSRC_ST   m[dST]
#define MSRC_SB   m[dSB]
#define MSRC_ET   m[dET]
#define MSRC_EB   m[dEB]
#define MSRC_WT   m[dWT]
#define MSRC_WB   m[dWB]

// this is a bit ugly, but necessary for the ccel local access...
#define CCEL_C    RAC(ccel, dC )
#define CCEL_N    RAC(ccel, dN )
#define CCEL_S    RAC(ccel, dS )
#define CCEL_E    RAC(ccel, dE )
#define CCEL_W    RAC(ccel, dW )
#define CCEL_T    RAC(ccel, dT )
#define CCEL_B    RAC(ccel, dB )
#define CCEL_NE   RAC(ccel, dNE)
#define CCEL_NW   RAC(ccel, dNW)
#define CCEL_SE   RAC(ccel, dSE)
#define CCEL_SW   RAC(ccel, dSW)
#define CCEL_NT   RAC(ccel, dNT)
#define CCEL_NB   RAC(ccel, dNB)
#define CCEL_ST   RAC(ccel, dST)
#define CCEL_SB   RAC(ccel, dSB)
#define CCEL_ET   RAC(ccel, dET)
#define CCEL_EB   RAC(ccel, dEB)
#define CCEL_WT   RAC(ccel, dWT)
#define CCEL_WB   RAC(ccel, dWB)
// for coarse to fine interpol access
#define CCELG_C(f)    (RAC(ccel, dC )*mGaussw[(f)])
#define CCELG_N(f)    (RAC(ccel, dN )*mGaussw[(f)])
#define CCELG_S(f)    (RAC(ccel, dS )*mGaussw[(f)])
#define CCELG_E(f)    (RAC(ccel, dE )*mGaussw[(f)])
#define CCELG_W(f)    (RAC(ccel, dW )*mGaussw[(f)])
#define CCELG_T(f)    (RAC(ccel, dT )*mGaussw[(f)])
#define CCELG_B(f)    (RAC(ccel, dB )*mGaussw[(f)])
#define CCELG_NE(f)   (RAC(ccel, dNE)*mGaussw[(f)])
#define CCELG_NW(f)   (RAC(ccel, dNW)*mGaussw[(f)])
#define CCELG_SE(f)   (RAC(ccel, dSE)*mGaussw[(f)])
#define CCELG_SW(f)   (RAC(ccel, dSW)*mGaussw[(f)])
#define CCELG_NT(f)   (RAC(ccel, dNT)*mGaussw[(f)])
#define CCELG_NB(f)   (RAC(ccel, dNB)*mGaussw[(f)])
#define CCELG_ST(f)   (RAC(ccel, dST)*mGaussw[(f)])
#define CCELG_SB(f)   (RAC(ccel, dSB)*mGaussw[(f)])
#define CCELG_ET(f)   (RAC(ccel, dET)*mGaussw[(f)])
#define CCELG_EB(f)   (RAC(ccel, dEB)*mGaussw[(f)])
#define CCELG_WT(f)   (RAC(ccel, dWT)*mGaussw[(f)])
#define CCELG_WB(f)   (RAC(ccel, dWB)*mGaussw[(f)])


#if PARALLEL==1
#define CSMOMEGA_STATS(dlev, domega) 
#else // PARALLEL==1
#if FSGR_OMEGA_DEBUG==1
#define CSMOMEGA_STATS(dlev, domega) \
	mLevel[dlev].avgOmega += domega; mLevel[dlev].avgOmegaCnt+=1.0; 
#else // FSGR_OMEGA_DEBUG==1
#define CSMOMEGA_STATS(dlev, domega) 
#endif // FSGR_OMEGA_DEBUG==1
#endif // PARALLEL==1


// used for main loops and grav init
// source set
#define SRCS(l) mLevel[(l)].setCurr
// target set
#define TSET(l) mLevel[(l)].setOther

// handle mov. obj 
#if FSGR_STRICT_DEBUG==1

#define  LBMDS_ADDMOV(linv,l)  \
	 \
	if((nbflag[linv]&CFBndMoving)&&(!(nbflag[l]&CFBnd))){ \
	 \
	LbmFloat dte=QCELL_NBINV(lev, i, j, k, SRCS(lev), l,dFlux)-(mSimulationTime+this->mpParam->getTimestep()); \
	if( ABS(dte)< 1e-15 ) { \
	m[l]+=QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l); \
	} else { \
	const int sdx = i+this->dfVecX[linv], sdy = j+this->dfVecY[linv], sdz = k+this->dfVecZ[linv]; \
	 \
	errMsg("INVALID_MOV_OBJ_TIME"," at "<<PRINT_IJK<<" from l"<<l<<" "<<PRINT_VEC(sdx,sdy,sdz)<<" t="<<(mSimulationTime+this->mpParam->getTimestep())<<" ct="<<QCELL_NBINV(lev, i, j, k, SRCS(lev), l,dFlux)<<" dte="<<dte); \
	debugMarkCell(lev,sdx,sdy,sdz); \
	} \
	} \



#else // FSGR_STRICT_DEBUG==1

#define  LBMDS_ADDMOV(linv,l)  \
	 \
	 \
	if((nbflag[linv]&CFBndMoving)&&(!(nbflag[l]&CFBnd))){ \
	 \
	m[l]+=QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l); \
	} \



#endif // !FSGR_STRICT_DEBUG==1

// treatment of freeslip reflection
// used both for OPT and nonOPT
#define  DEFAULT_STREAM_FREESLIP(l,invl,mnbf)  \
	 \
	int nb1 = 0, nb2 = 0; \
	LbmFloat newval = 0.0; \
	const int dx = this->dfVecX[invl], dy = this->dfVecY[invl], dz = this->dfVecZ[invl]; \
	 \
	 \
	 \
	const LbmFloat movadd = ( \
	((nbflag[invl]&CFBndMoving)&&(!(nbflag[l]&CFBnd))) ? \
	(QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l)) : 0.); \
	 \
	if(dz==0) { \
	nb1 = !(RFLAG(lev, i,   j+dy,k, SRCS(lev))&(CFFluid|CFInter)); \
	nb2 = !(RFLAG(lev, i+dx,j,   k, SRCS(lev))&(CFFluid|CFInter)); \
	if((nb1)&&(!nb2)) { \
	 \
	newval = QCELL(lev, i+dx,j,k,SRCS(lev), this->dfRefX[l]); \
	} else \
	if((!nb1)&&(nb2)) { \
	 \
	newval = QCELL(lev, i,j+dy,k,SRCS(lev), this->dfRefY[l]); \
	} else { \
	 \
	newval = RAC(ccel, this->dfInv[l] ) +movadd /* */; \
	} \
	} else \
	if(dy==0) { \
	nb1 = !(RFLAG(lev, i,j,k+dz, SRCS(lev))&(CFFluid|CFInter)); \
	nb2 = !(RFLAG(lev, i+dx,j,k, SRCS(lev))&(CFFluid|CFInter)); \
	if((nb1)&&(!nb2)) { \
	 \
	newval = QCELL(lev, i+dx,j,k,SRCS(lev), this->dfRefX[l]); \
	} else \
	if((!nb1)&&(nb2)) { \
	 \
	newval = QCELL(lev, i,j,k+dz,SRCS(lev), this->dfRefZ[l]); \
	} else { \
	 \
	newval = RAC(ccel, this->dfInv[l] )  +movadd /* */; \
	} \
	 \
	} else \
	 \
	{ \
	 \
	nb1 = !(RFLAG(lev, i,j,k+dz, SRCS(lev))&(CFFluid|CFInter)); \
	nb2 = !(RFLAG(lev, i,j+dy,k, SRCS(lev))&(CFFluid|CFInter)); \
	if((nb1)&&(!nb2)) { \
	 \
	newval = QCELL(lev, i,j+dy,k,SRCS(lev), this->dfRefY[l]); \
	} else \
	if((!nb1)&&(nb2)) { \
	 \
	newval = QCELL(lev, i,j,k+dz,SRCS(lev), this->dfRefZ[l]); \
	} else { \
	 \
	newval = RAC(ccel, this->dfInv[l] )  +movadd /* */; \
	} \
	} \
	 \
	if(mnbf & CFBndPartslip) { \
	const LbmFloat partv = mObjectPartslips[(int)(mnbf>>24)]; \
	 \
	m[l] = (RAC(ccel, this->dfInv[l] )  +movadd /* d *(1./1.) */ ) * partv + newval * (1.0-partv); \
	} else { \
	m[l] = newval; \
	} \
	 \




// complete default stream&collide, 2d/3d
/* read distribution funtions of adjacent cells = sweep step */ 
#if OPT3D==0 

#if FSGR_STRICT_DEBUG==1
#define MARKCELLCHECK \
	debugMarkCell(lev,i,j,k); CAUSE_PANIC;
#define STREAMCHECK(id,ni,nj,nk,nl) \
	if((!(m[nl] > -1.0) && (m[nl]<1.0)) ) {\
		errMsg("STREAMCHECK","ID"<<id<<" Invalid streamed DF nl"<<nl<<" value:"<<m[nl]<<" at "<<PRINT_IJK<<" from "<<PRINT_VEC(ni,nj,nk)<<" nl"<<(nl)<<\
				" nfc"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr)<<" nfo"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setOther)  ); \
		/*FORDF0{ errMsg("STREAMCHECK"," at "<<PRINT_IJK<<" df "<<l<<"="<<m[l] ); } */ \
		MARKCELLCHECK; \
		m[nl] = dfEquil[nl]; /* REPAIR */ \
	}
#define COLLCHECK \
	if( (rho>2.0) || (rho<-1.0) || (ABS(ux)>1.0) || (ABS(uy)>1.0) |(ABS(uz)>1.0) ) {\
		errMsg("COLLCHECK","Invalid collision values r:"<<rho<<" u:"PRINT_VEC(ux,uy,uz)<<" at? "<<PRINT_IJK ); \
		/*FORDF0{ errMsg("COLLCHECK"," at? "<<PRINT_IJK<<" df "<<l<<"="<<m[l] ); }*/ \
		rho=ux=uy=uz= 0.; /* REPAIR */ \
		MARKCELLCHECK; \
	}
#else
#define STREAMCHECK(id, ni,nj,nk,nl) 
#define COLLCHECK
#endif

// careful ux,uy,uz need to be inited before!
#define  DEFAULT_STREAM  \
	m[dC] = RAC(ccel,dC); \
	STREAMCHECK(1, i,j,k, dC); \
	FORDF1 { \
	CellFlagType nbf = nbflag[ this->dfInv[l] ]; \
	if(nbf & CFBnd) { \
	if(nbf & CFBndNoslip) { \
	 \
	m[l] = RAC(ccel, this->dfInv[l] ); \
	LBMDS_ADDMOV(this->dfInv[l],l); \
	STREAMCHECK(2, i,j,k, l); \
	} else if(nbf & (CFBndFreeslip|CFBndPartslip)) { \
	 \
	if(l<=LBMDIM*2) { \
	m[l] = RAC(ccel, this->dfInv[l] ); STREAMCHECK(3, i,j,k, l); \
	LBMDS_ADDMOV(this->dfInv[l],l); \
	} else { \
	const int inv_l = this->dfInv[l]; \
	DEFAULT_STREAM_FREESLIP(l,inv_l,nbf); \
	} \
	 \
	} \
	else { \
	errMsg("LbmFsgrSolver","Invalid Bnd type at "<<PRINT_IJK<<" f"<<convertCellFlagType2String(nbf)<<",nbdir"<<this->dfInv[l] ); \
	} \
	} else { \
	m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l); \
	if(RFLAG(lev, i,j,k, mLevel[lev].setCurr)&CFFluid) { \
	if(!(nbf&(CFFluid|CFInter)) ) { \
	int ni=i+this->dfVecX[this->dfInv[l]], nj=j+this->dfVecY[this->dfInv[l]], nk=k+this->dfVecZ[this->dfInv[l]]; \
	errMsg("STREAMCHECK"," Invalid nbflag, streamed DF l"<<l<<" value:"<<m[l]<<" at "<<PRINT_IJK<<" from "<< \
	PRINT_VEC(ni,nj,nk) <<" l"<<(l)<< \
	" nfc"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr)<<" nfo"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setOther)  ); \
	 \
	 \
	} } \
	STREAMCHECK(4, i+this->dfVecX[this->dfInv[l]], j+this->dfVecY[this->dfInv[l]],k+this->dfVecZ[this->dfInv[l]], l); \
	} \
	} \




// careful ux,uy,uz need to be inited before!
#define  DEFAULT_COLLIDEG(grav)  \
	this->collideArrays(lev, i,j,k, m, rho,ux,uy,uz, OMEGA(lev), grav, mLevel[lev].lcsmago, &mDebugOmegaRet, &lcsmqo ); \
	CSMOMEGA_STATS(lev,mDebugOmegaRet); \
	FORDF0 { RAC(tcel,l) = m[l]; } \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	COLLCHECK; \



#define  OPTIMIZED_STREAMCOLLIDE  \
	m[0] = RAC(ccel,0); \
	FORDF1 { \
	 \
	if(RFLAG_NBINV(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); CAUSE_PANIC; \
	} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
	STREAMCHECK(8, i+this->dfVecX[this->dfInv[l]], j+this->dfVecY[this->dfInv[l]],k+this->dfVecZ[this->dfInv[l]], l); \
	} \
	rho=m[0]; \
	DEFAULT_COLLIDEG(mLevel[lev].gravity) \



#define  OPTIMIZED_STREAMCOLLIDE___UNUSED  \
	 \
	this->collideArrays(lev, i,j,k, m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].gravity, mLevel[lev].lcsmago , &mDebugOmegaRet, &lcsmqo   ); \
	CSMOMEGA_STATS(lev,mDebugOmegaRet); \
	FORDF0 { RAC(tcel,l) = m[l]; } \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	COLLCHECK; \



#else  // 3D, opt OPT3D==true


// default stream opt3d add moving bc val
#define  DEFAULT_STREAM  \
	m[dC] = RAC(ccel,dC); \
	 \
	if((!nbored & CFBnd)) { \
	 \
	m[dN ] = CSRC_N ; m[dS ] = CSRC_S ; \
	m[dE ] = CSRC_E ; m[dW ] = CSRC_W ; \
	m[dT ] = CSRC_T ; m[dB ] = CSRC_B ; \
	m[dNE] = CSRC_NE; m[dNW] = CSRC_NW; m[dSE] = CSRC_SE; m[dSW] = CSRC_SW; \
	m[dNT] = CSRC_NT; m[dNB] = CSRC_NB; m[dST] = CSRC_ST; m[dSB] = CSRC_SB; \
	m[dET] = CSRC_ET; m[dEB] = CSRC_EB; m[dWT] = CSRC_WT; m[dWB] = CSRC_WB; \
	} else { \
	 \
	if(nbflag[dS ]&CFBnd) { m[dN ] = RAC(ccel,dS ); LBMDS_ADDMOV(dS ,dN ); } else { m[dN ] = CSRC_N ; } \
	if(nbflag[dN ]&CFBnd) { m[dS ] = RAC(ccel,dN ); LBMDS_ADDMOV(dN ,dS ); } else { m[dS ] = CSRC_S ; } \
	if(nbflag[dW ]&CFBnd) { m[dE ] = RAC(ccel,dW ); LBMDS_ADDMOV(dW ,dE ); } else { m[dE ] = CSRC_E ; } \
	if(nbflag[dE ]&CFBnd) { m[dW ] = RAC(ccel,dE ); LBMDS_ADDMOV(dE ,dW ); } else { m[dW ] = CSRC_W ; } \
	if(nbflag[dB ]&CFBnd) { m[dT ] = RAC(ccel,dB ); LBMDS_ADDMOV(dB ,dT ); } else { m[dT ] = CSRC_T ; } \
	if(nbflag[dT ]&CFBnd) { m[dB ] = RAC(ccel,dT ); LBMDS_ADDMOV(dT ,dB ); } else { m[dB ] = CSRC_B ; } \
	 \
	 \
	if(nbflag[dSW]&CFBnd) { if(nbflag[dSW]&CFBndNoslip){ m[dNE] = RAC(ccel,dSW); LBMDS_ADDMOV(dSW,dNE); }else{ DEFAULT_STREAM_FREESLIP(dNE,dSW,nbflag[dSW]);} } else { m[dNE] = CSRC_NE; } \
	if(nbflag[dSE]&CFBnd) { if(nbflag[dSE]&CFBndNoslip){ m[dNW] = RAC(ccel,dSE); LBMDS_ADDMOV(dSE,dNW); }else{ DEFAULT_STREAM_FREESLIP(dNW,dSE,nbflag[dSE]);} } else { m[dNW] = CSRC_NW; } \
	if(nbflag[dNW]&CFBnd) { if(nbflag[dNW]&CFBndNoslip){ m[dSE] = RAC(ccel,dNW); LBMDS_ADDMOV(dNW,dSE); }else{ DEFAULT_STREAM_FREESLIP(dSE,dNW,nbflag[dNW]);} } else { m[dSE] = CSRC_SE; } \
	if(nbflag[dNE]&CFBnd) { if(nbflag[dNE]&CFBndNoslip){ m[dSW] = RAC(ccel,dNE); LBMDS_ADDMOV(dNE,dSW); }else{ DEFAULT_STREAM_FREESLIP(dSW,dNE,nbflag[dNE]);} } else { m[dSW] = CSRC_SW; } \
	if(nbflag[dSB]&CFBnd) { if(nbflag[dSB]&CFBndNoslip){ m[dNT] = RAC(ccel,dSB); LBMDS_ADDMOV(dSB,dNT); }else{ DEFAULT_STREAM_FREESLIP(dNT,dSB,nbflag[dSB]);} } else { m[dNT] = CSRC_NT; } \
	if(nbflag[dST]&CFBnd) { if(nbflag[dST]&CFBndNoslip){ m[dNB] = RAC(ccel,dST); LBMDS_ADDMOV(dST,dNB); }else{ DEFAULT_STREAM_FREESLIP(dNB,dST,nbflag[dST]);} } else { m[dNB] = CSRC_NB; } \
	if(nbflag[dNB]&CFBnd) { if(nbflag[dNB]&CFBndNoslip){ m[dST] = RAC(ccel,dNB); LBMDS_ADDMOV(dNB,dST); }else{ DEFAULT_STREAM_FREESLIP(dST,dNB,nbflag[dNB]);} } else { m[dST] = CSRC_ST; } \
	if(nbflag[dNT]&CFBnd) { if(nbflag[dNT]&CFBndNoslip){ m[dSB] = RAC(ccel,dNT); LBMDS_ADDMOV(dNT,dSB); }else{ DEFAULT_STREAM_FREESLIP(dSB,dNT,nbflag[dNT]);} } else { m[dSB] = CSRC_SB; } \
	if(nbflag[dWB]&CFBnd) { if(nbflag[dWB]&CFBndNoslip){ m[dET] = RAC(ccel,dWB); LBMDS_ADDMOV(dWB,dET); }else{ DEFAULT_STREAM_FREESLIP(dET,dWB,nbflag[dWB]);} } else { m[dET] = CSRC_ET; } \
	if(nbflag[dWT]&CFBnd) { if(nbflag[dWT]&CFBndNoslip){ m[dEB] = RAC(ccel,dWT); LBMDS_ADDMOV(dWT,dEB); }else{ DEFAULT_STREAM_FREESLIP(dEB,dWT,nbflag[dWT]);} } else { m[dEB] = CSRC_EB; } \
	if(nbflag[dEB]&CFBnd) { if(nbflag[dEB]&CFBndNoslip){ m[dWT] = RAC(ccel,dEB); LBMDS_ADDMOV(dEB,dWT); }else{ DEFAULT_STREAM_FREESLIP(dWT,dEB,nbflag[dEB]);} } else { m[dWT] = CSRC_WT; } \
	if(nbflag[dET]&CFBnd) { if(nbflag[dET]&CFBndNoslip){ m[dWB] = RAC(ccel,dET); LBMDS_ADDMOV(dET,dWB); }else{ DEFAULT_STREAM_FREESLIP(dWB,dET,nbflag[dET]);} } else { m[dWB] = CSRC_WB; } \
	} \





#define  COLL_CALCULATE_DFEQ(dstarray)  \
	dstarray[dN ] = EQN ; dstarray[dS ] = EQS ; \
	dstarray[dE ] = EQE ; dstarray[dW ] = EQW ; \
	dstarray[dT ] = EQT ; dstarray[dB ] = EQB ; \
	dstarray[dNE] = EQNE; dstarray[dNW] = EQNW; dstarray[dSE] = EQSE; dstarray[dSW] = EQSW; \
	dstarray[dNT] = EQNT; dstarray[dNB] = EQNB; dstarray[dST] = EQST; dstarray[dSB] = EQSB; \
	dstarray[dET] = EQET; dstarray[dEB] = EQEB; dstarray[dWT] = EQWT; dstarray[dWB] = EQWB; \



#define  COLL_CALCULATE_NONEQTENSOR(csolev, srcArray )  \
	lcsmqadd  = (srcArray##NE - lcsmeq[ dNE ]); \
	lcsmqadd -= (srcArray##NW - lcsmeq[ dNW ]); \
	lcsmqadd -= (srcArray##SE - lcsmeq[ dSE ]); \
	lcsmqadd += (srcArray##SW - lcsmeq[ dSW ]); \
	lcsmqo = (lcsmqadd*    lcsmqadd); \
	lcsmqadd  = (srcArray##ET - lcsmeq[  dET ]); \
	lcsmqadd -= (srcArray##EB - lcsmeq[  dEB ]); \
	lcsmqadd -= (srcArray##WT - lcsmeq[  dWT ]); \
	lcsmqadd += (srcArray##WB - lcsmeq[  dWB ]); \
	lcsmqo += (lcsmqadd*    lcsmqadd); \
	lcsmqadd  = (srcArray##NT - lcsmeq[  dNT ]); \
	lcsmqadd -= (srcArray##NB - lcsmeq[  dNB ]); \
	lcsmqadd -= (srcArray##ST - lcsmeq[  dST ]); \
	lcsmqadd += (srcArray##SB - lcsmeq[  dSB ]); \
	lcsmqo += (lcsmqadd*    lcsmqadd); \
	lcsmqo *= 2.0; \
	lcsmqadd  = (srcArray##E  -  lcsmeq[ dE  ]); \
	lcsmqadd += (srcArray##W  -  lcsmeq[ dW  ]); \
	lcsmqadd += (srcArray##NE -  lcsmeq[ dNE ]); \
	lcsmqadd += (srcArray##NW -  lcsmeq[ dNW ]); \
	lcsmqadd += (srcArray##SE -  lcsmeq[ dSE ]); \
	lcsmqadd += (srcArray##SW -  lcsmeq[ dSW ]); \
	lcsmqadd += (srcArray##ET  - lcsmeq[ dET ]); \
	lcsmqadd += (srcArray##EB  - lcsmeq[ dEB ]); \
	lcsmqadd += (srcArray##WT  - lcsmeq[ dWT ]); \
	lcsmqadd += (srcArray##WB  - lcsmeq[ dWB ]); \
	lcsmqo += (lcsmqadd*    lcsmqadd); \
	lcsmqadd  = (srcArray##N  -  lcsmeq[ dN  ]); \
	lcsmqadd += (srcArray##S  -  lcsmeq[ dS  ]); \
	lcsmqadd += (srcArray##NE -  lcsmeq[ dNE ]); \
	lcsmqadd += (srcArray##NW -  lcsmeq[ dNW ]); \
	lcsmqadd += (srcArray##SE -  lcsmeq[ dSE ]); \
	lcsmqadd += (srcArray##SW -  lcsmeq[ dSW ]); \
	lcsmqadd += (srcArray##NT  - lcsmeq[ dNT ]); \
	lcsmqadd += (srcArray##NB  - lcsmeq[ dNB ]); \
	lcsmqadd += (srcArray##ST  - lcsmeq[ dST ]); \
	lcsmqadd += (srcArray##SB  - lcsmeq[ dSB ]); \
	lcsmqo += (lcsmqadd*    lcsmqadd); \
	lcsmqadd  = (srcArray##T  -  lcsmeq[ dT  ]); \
	lcsmqadd += (srcArray##B  -  lcsmeq[ dB  ]); \
	lcsmqadd += (srcArray##NT -  lcsmeq[ dNT ]); \
	lcsmqadd += (srcArray##NB -  lcsmeq[ dNB ]); \
	lcsmqadd += (srcArray##ST -  lcsmeq[ dST ]); \
	lcsmqadd += (srcArray##SB -  lcsmeq[ dSB ]); \
	lcsmqadd += (srcArray##ET  - lcsmeq[ dET ]); \
	lcsmqadd += (srcArray##EB  - lcsmeq[ dEB ]); \
	lcsmqadd += (srcArray##WT  - lcsmeq[ dWT ]); \
	lcsmqadd += (srcArray##WB  - lcsmeq[ dWB ]); \
	lcsmqo += (lcsmqadd*    lcsmqadd); \
	lcsmqo = sqrt(lcsmqo); \



//			COLL_CALCULATE_CSMOMEGAVAL(csolev, lcsmomega); 

// careful - need lcsmqo 
#define  COLL_CALCULATE_CSMOMEGAVAL(csolev, dstomega )  \
	dstomega =  1.0/ \
	( 3.0*( mLevel[(csolev)].lcnu+mLevel[(csolev)].lcsmago_sqr*( \
	-mLevel[(csolev)].lcnu + sqrt( mLevel[(csolev)].lcnu*mLevel[(csolev)].lcnu + 18.0*mLevel[(csolev)].lcsmago_sqr* lcsmqo ) \
	/ (6.0*mLevel[(csolev)].lcsmago_sqr)) \
	) +0.5 ); \



#define  DEFAULT_COLLIDE_LES(grav)  \
	rho = + MSRC_C  + MSRC_N \
	+ MSRC_S  + MSRC_E \
	+ MSRC_W  + MSRC_T \
	+ MSRC_B  + MSRC_NE \
	+ MSRC_NW + MSRC_SE \
	+ MSRC_SW + MSRC_NT \
	+ MSRC_NB + MSRC_ST \
	+ MSRC_SB + MSRC_ET \
	+ MSRC_EB + MSRC_WT \
	+ MSRC_WB; \
	 \
	ux = MSRC_E - MSRC_W \
	+ MSRC_NE - MSRC_NW \
	+ MSRC_SE - MSRC_SW \
	+ MSRC_ET + MSRC_EB \
	- MSRC_WT - MSRC_WB ; \
	 \
	uy = MSRC_N - MSRC_S \
	+ MSRC_NE + MSRC_NW \
	- MSRC_SE - MSRC_SW \
	+ MSRC_NT + MSRC_NB \
	- MSRC_ST - MSRC_SB ; \
	 \
	uz = MSRC_T - MSRC_B \
	+ MSRC_NT - MSRC_NB \
	+ MSRC_ST - MSRC_SB \
	+ MSRC_ET - MSRC_EB \
	+ MSRC_WT - MSRC_WB ; \
	PRECOLLIDE_MODS(rho,ux,uy,uz, grav); \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	COLL_CALCULATE_DFEQ(lcsmeq); \
	COLL_CALCULATE_NONEQTENSOR(lev, MSRC_); \
	COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
	CSMOMEGA_STATS(lev,lcsmomega); \
	 \
	RAC(tcel,dC ) = (1.0-lcsmomega)*MSRC_C  + lcsmomega*EQC ; \
	 \
	RAC(tcel,dN ) = (1.0-lcsmomega)*MSRC_N  + lcsmomega*lcsmeq[ dN ]; \
	RAC(tcel,dS ) = (1.0-lcsmomega)*MSRC_S  + lcsmomega*lcsmeq[ dS ]; \
	RAC(tcel,dE ) = (1.0-lcsmomega)*MSRC_E  + lcsmomega*lcsmeq[ dE ]; \
	RAC(tcel,dW ) = (1.0-lcsmomega)*MSRC_W  + lcsmomega*lcsmeq[ dW ]; \
	RAC(tcel,dT ) = (1.0-lcsmomega)*MSRC_T  + lcsmomega*lcsmeq[ dT ]; \
	RAC(tcel,dB ) = (1.0-lcsmomega)*MSRC_B  + lcsmomega*lcsmeq[ dB ]; \
	 \
	RAC(tcel,dNE) = (1.0-lcsmomega)*MSRC_NE + lcsmomega*lcsmeq[ dNE]; \
	RAC(tcel,dNW) = (1.0-lcsmomega)*MSRC_NW + lcsmomega*lcsmeq[ dNW]; \
	RAC(tcel,dSE) = (1.0-lcsmomega)*MSRC_SE + lcsmomega*lcsmeq[ dSE]; \
	RAC(tcel,dSW) = (1.0-lcsmomega)*MSRC_SW + lcsmomega*lcsmeq[ dSW]; \
	RAC(tcel,dNT) = (1.0-lcsmomega)*MSRC_NT + lcsmomega*lcsmeq[ dNT]; \
	RAC(tcel,dNB) = (1.0-lcsmomega)*MSRC_NB + lcsmomega*lcsmeq[ dNB]; \
	RAC(tcel,dST) = (1.0-lcsmomega)*MSRC_ST + lcsmomega*lcsmeq[ dST]; \
	RAC(tcel,dSB) = (1.0-lcsmomega)*MSRC_SB + lcsmomega*lcsmeq[ dSB]; \
	RAC(tcel,dET) = (1.0-lcsmomega)*MSRC_ET + lcsmomega*lcsmeq[ dET]; \
	RAC(tcel,dEB) = (1.0-lcsmomega)*MSRC_EB + lcsmomega*lcsmeq[ dEB]; \
	RAC(tcel,dWT) = (1.0-lcsmomega)*MSRC_WT + lcsmomega*lcsmeq[ dWT]; \
	RAC(tcel,dWB) = (1.0-lcsmomega)*MSRC_WB + lcsmomega*lcsmeq[ dWB]; \



#define  DEFAULT_COLLIDE_NOLES(grav)  \
	rho = + MSRC_C  + MSRC_N \
	+ MSRC_S  + MSRC_E \
	+ MSRC_W  + MSRC_T \
	+ MSRC_B  + MSRC_NE \
	+ MSRC_NW + MSRC_SE \
	+ MSRC_SW + MSRC_NT \
	+ MSRC_NB + MSRC_ST \
	+ MSRC_SB + MSRC_ET \
	+ MSRC_EB + MSRC_WT \
	+ MSRC_WB; \
	 \
	ux = MSRC_E - MSRC_W \
	+ MSRC_NE - MSRC_NW \
	+ MSRC_SE - MSRC_SW \
	+ MSRC_ET + MSRC_EB \
	- MSRC_WT - MSRC_WB ; \
	 \
	uy = MSRC_N - MSRC_S \
	+ MSRC_NE + MSRC_NW \
	- MSRC_SE - MSRC_SW \
	+ MSRC_NT + MSRC_NB \
	- MSRC_ST - MSRC_SB ; \
	 \
	uz = MSRC_T - MSRC_B \
	+ MSRC_NT - MSRC_NB \
	+ MSRC_ST - MSRC_SB \
	+ MSRC_ET - MSRC_EB \
	+ MSRC_WT - MSRC_WB ; \
	PRECOLLIDE_MODS(rho, ux,uy,uz, grav); \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	 \
	RAC(tcel,dC ) = (1.0-OMEGA(lev))*MSRC_C  + OMEGA(lev)*EQC ; \
	 \
	RAC(tcel,dN ) = (1.0-OMEGA(lev))*MSRC_N  + OMEGA(lev)*EQN ; \
	RAC(tcel,dS ) = (1.0-OMEGA(lev))*MSRC_S  + OMEGA(lev)*EQS ; \
	RAC(tcel,dE ) = (1.0-OMEGA(lev))*MSRC_E  + OMEGA(lev)*EQE ; \
	RAC(tcel,dW ) = (1.0-OMEGA(lev))*MSRC_W  + OMEGA(lev)*EQW ; \
	RAC(tcel,dT ) = (1.0-OMEGA(lev))*MSRC_T  + OMEGA(lev)*EQT ; \
	RAC(tcel,dB ) = (1.0-OMEGA(lev))*MSRC_B  + OMEGA(lev)*EQB ; \
	 \
	RAC(tcel,dNE) = (1.0-OMEGA(lev))*MSRC_NE + OMEGA(lev)*EQNE; \
	RAC(tcel,dNW) = (1.0-OMEGA(lev))*MSRC_NW + OMEGA(lev)*EQNW; \
	RAC(tcel,dSE) = (1.0-OMEGA(lev))*MSRC_SE + OMEGA(lev)*EQSE; \
	RAC(tcel,dSW) = (1.0-OMEGA(lev))*MSRC_SW + OMEGA(lev)*EQSW; \
	RAC(tcel,dNT) = (1.0-OMEGA(lev))*MSRC_NT + OMEGA(lev)*EQNT; \
	RAC(tcel,dNB) = (1.0-OMEGA(lev))*MSRC_NB + OMEGA(lev)*EQNB; \
	RAC(tcel,dST) = (1.0-OMEGA(lev))*MSRC_ST + OMEGA(lev)*EQST; \
	RAC(tcel,dSB) = (1.0-OMEGA(lev))*MSRC_SB + OMEGA(lev)*EQSB; \
	RAC(tcel,dET) = (1.0-OMEGA(lev))*MSRC_ET + OMEGA(lev)*EQET; \
	RAC(tcel,dEB) = (1.0-OMEGA(lev))*MSRC_EB + OMEGA(lev)*EQEB; \
	RAC(tcel,dWT) = (1.0-OMEGA(lev))*MSRC_WT + OMEGA(lev)*EQWT; \
	RAC(tcel,dWB) = (1.0-OMEGA(lev))*MSRC_WB + OMEGA(lev)*EQWB; \





#define  OPTIMIZED_STREAMCOLLIDE_LES  \
	 \
	m[dC ] = CSRC_C ; \
	m[dN ] = CSRC_N ; m[dS ] = CSRC_S ; \
	m[dE ] = CSRC_E ; m[dW ] = CSRC_W ; \
	m[dT ] = CSRC_T ; m[dB ] = CSRC_B ; \
	m[dNE] = CSRC_NE; m[dNW] = CSRC_NW; m[dSE] = CSRC_SE; m[dSW] = CSRC_SW; \
	m[dNT] = CSRC_NT; m[dNB] = CSRC_NB; m[dST] = CSRC_ST; m[dSB] = CSRC_SB; \
	m[dET] = CSRC_ET; m[dEB] = CSRC_EB; m[dWT] = CSRC_WT; m[dWB] = CSRC_WB; \
	 \
	rho = MSRC_C  + MSRC_N + MSRC_S  + MSRC_E + MSRC_W  + MSRC_T \
	+ MSRC_B  + MSRC_NE + MSRC_NW + MSRC_SE + MSRC_SW + MSRC_NT \
	+ MSRC_NB + MSRC_ST + MSRC_SB + MSRC_ET + MSRC_EB + MSRC_WT + MSRC_WB; \
	ux = MSRC_E - MSRC_W + MSRC_NE - MSRC_NW + MSRC_SE - MSRC_SW \
	+ MSRC_ET + MSRC_EB - MSRC_WT - MSRC_WB; \
	uy = MSRC_N - MSRC_S + MSRC_NE + MSRC_NW - MSRC_SE - MSRC_SW \
	+ MSRC_NT + MSRC_NB - MSRC_ST - MSRC_SB; \
	uz = MSRC_T - MSRC_B + MSRC_NT - MSRC_NB + MSRC_ST - MSRC_SB \
	+ MSRC_ET - MSRC_EB + MSRC_WT - MSRC_WB; \
	PRECOLLIDE_MODS(rho, ux,uy,uz, mLevel[lev].gravity); \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	COLL_CALCULATE_DFEQ(lcsmeq); \
	COLL_CALCULATE_NONEQTENSOR(lev, MSRC_) \
	COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
	CSMOMEGA_STATS(lev,lcsmomega); \
	 \
	RAC(tcel,dC ) = (1.0-lcsmomega)*MSRC_C  + lcsmomega*EQC ; \
	RAC(tcel,dN ) = (1.0-lcsmomega)*MSRC_N  + lcsmomega*lcsmeq[ dN ]; \
	RAC(tcel,dS ) = (1.0-lcsmomega)*MSRC_S  + lcsmomega*lcsmeq[ dS ]; \
	RAC(tcel,dE ) = (1.0-lcsmomega)*MSRC_E  + lcsmomega*lcsmeq[ dE ]; \
	RAC(tcel,dW ) = (1.0-lcsmomega)*MSRC_W  + lcsmomega*lcsmeq[ dW ]; \
	RAC(tcel,dT ) = (1.0-lcsmomega)*MSRC_T  + lcsmomega*lcsmeq[ dT ]; \
	RAC(tcel,dB ) = (1.0-lcsmomega)*MSRC_B  + lcsmomega*lcsmeq[ dB ]; \
	 \
	RAC(tcel,dNE) = (1.0-lcsmomega)*MSRC_NE + lcsmomega*lcsmeq[ dNE]; \
	RAC(tcel,dNW) = (1.0-lcsmomega)*MSRC_NW + lcsmomega*lcsmeq[ dNW]; \
	RAC(tcel,dSE) = (1.0-lcsmomega)*MSRC_SE + lcsmomega*lcsmeq[ dSE]; \
	RAC(tcel,dSW) = (1.0-lcsmomega)*MSRC_SW + lcsmomega*lcsmeq[ dSW]; \
	 \
	RAC(tcel,dNT) = (1.0-lcsmomega)*MSRC_NT + lcsmomega*lcsmeq[ dNT]; \
	RAC(tcel,dNB) = (1.0-lcsmomega)*MSRC_NB + lcsmomega*lcsmeq[ dNB]; \
	RAC(tcel,dST) = (1.0-lcsmomega)*MSRC_ST + lcsmomega*lcsmeq[ dST]; \
	RAC(tcel,dSB) = (1.0-lcsmomega)*MSRC_SB + lcsmomega*lcsmeq[ dSB]; \
	 \
	RAC(tcel,dET) = (1.0-lcsmomega)*MSRC_ET + lcsmomega*lcsmeq[ dET]; \
	RAC(tcel,dEB) = (1.0-lcsmomega)*MSRC_EB + lcsmomega*lcsmeq[ dEB]; \
	RAC(tcel,dWT) = (1.0-lcsmomega)*MSRC_WT + lcsmomega*lcsmeq[ dWT]; \
	RAC(tcel,dWB) = (1.0-lcsmomega)*MSRC_WB + lcsmomega*lcsmeq[ dWB]; \



#define  OPTIMIZED_STREAMCOLLIDE_UNUSED  \
	 \
	rho = CSRC_C  + CSRC_N + CSRC_S  + CSRC_E + CSRC_W  + CSRC_T \
	+ CSRC_B  + CSRC_NE + CSRC_NW + CSRC_SE + CSRC_SW + CSRC_NT \
	+ CSRC_NB + CSRC_ST + CSRC_SB + CSRC_ET + CSRC_EB + CSRC_WT + CSRC_WB; \
	ux = CSRC_E - CSRC_W + CSRC_NE - CSRC_NW + CSRC_SE - CSRC_SW \
	+ CSRC_ET + CSRC_EB - CSRC_WT - CSRC_WB; \
	uy = CSRC_N - CSRC_S + CSRC_NE + CSRC_NW - CSRC_SE - CSRC_SW \
	+ CSRC_NT + CSRC_NB - CSRC_ST - CSRC_SB; \
	uz = CSRC_T - CSRC_B + CSRC_NT - CSRC_NB + CSRC_ST - CSRC_SB \
	+ CSRC_ET - CSRC_EB + CSRC_WT - CSRC_WB; \
	PRECOLLIDE_MODS(rho, ux,uy,uz, mLevel[lev].gravity); \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	COLL_CALCULATE_DFEQ(lcsmeq); \
	COLL_CALCULATE_NONEQTENSOR(lev, CSRC_) \
	COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
	 \
	RAC(tcel,dC ) = (1.0-lcsmomega)*CSRC_C  + lcsmomega*EQC ; \
	RAC(tcel,dN ) = (1.0-lcsmomega)*CSRC_N  + lcsmomega*lcsmeq[ dN ]; \
	RAC(tcel,dS ) = (1.0-lcsmomega)*CSRC_S  + lcsmomega*lcsmeq[ dS ]; \
	RAC(tcel,dE ) = (1.0-lcsmomega)*CSRC_E  + lcsmomega*lcsmeq[ dE ]; \
	RAC(tcel,dW ) = (1.0-lcsmomega)*CSRC_W  + lcsmomega*lcsmeq[ dW ]; \
	RAC(tcel,dT ) = (1.0-lcsmomega)*CSRC_T  + lcsmomega*lcsmeq[ dT ]; \
	RAC(tcel,dB ) = (1.0-lcsmomega)*CSRC_B  + lcsmomega*lcsmeq[ dB ]; \
	 \
	RAC(tcel,dNE) = (1.0-lcsmomega)*CSRC_NE + lcsmomega*lcsmeq[ dNE]; \
	RAC(tcel,dNW) = (1.0-lcsmomega)*CSRC_NW + lcsmomega*lcsmeq[ dNW]; \
	RAC(tcel,dSE) = (1.0-lcsmomega)*CSRC_SE + lcsmomega*lcsmeq[ dSE]; \
	RAC(tcel,dSW) = (1.0-lcsmomega)*CSRC_SW + lcsmomega*lcsmeq[ dSW]; \
	 \
	RAC(tcel,dNT) = (1.0-lcsmomega)*CSRC_NT + lcsmomega*lcsmeq[ dNT]; \
	RAC(tcel,dNB) = (1.0-lcsmomega)*CSRC_NB + lcsmomega*lcsmeq[ dNB]; \
	RAC(tcel,dST) = (1.0-lcsmomega)*CSRC_ST + lcsmomega*lcsmeq[ dST]; \
	RAC(tcel,dSB) = (1.0-lcsmomega)*CSRC_SB + lcsmomega*lcsmeq[ dSB]; \
	 \
	RAC(tcel,dET) = (1.0-lcsmomega)*CSRC_ET + lcsmomega*lcsmeq[ dET]; \
	RAC(tcel,dEB) = (1.0-lcsmomega)*CSRC_EB + lcsmomega*lcsmeq[ dEB]; \
	RAC(tcel,dWT) = (1.0-lcsmomega)*CSRC_WT + lcsmomega*lcsmeq[ dWT]; \
	RAC(tcel,dWB) = (1.0-lcsmomega)*CSRC_WB + lcsmomega*lcsmeq[ dWB]; \



#define  OPTIMIZED_STREAMCOLLIDE_NOLES  \
	 \
	rho = CSRC_C  + CSRC_N + CSRC_S  + CSRC_E + CSRC_W  + CSRC_T \
	+ CSRC_B  + CSRC_NE + CSRC_NW + CSRC_SE + CSRC_SW + CSRC_NT \
	+ CSRC_NB + CSRC_ST + CSRC_SB + CSRC_ET + CSRC_EB + CSRC_WT + CSRC_WB; \
	ux = CSRC_E - CSRC_W + CSRC_NE - CSRC_NW + CSRC_SE - CSRC_SW \
	+ CSRC_ET + CSRC_EB - CSRC_WT - CSRC_WB; \
	uy = CSRC_N - CSRC_S + CSRC_NE + CSRC_NW - CSRC_SE - CSRC_SW \
	+ CSRC_NT + CSRC_NB - CSRC_ST - CSRC_SB; \
	uz = CSRC_T - CSRC_B + CSRC_NT - CSRC_NB + CSRC_ST - CSRC_SB \
	+ CSRC_ET - CSRC_EB + CSRC_WT - CSRC_WB; \
	PRECOLLIDE_MODS(rho, ux,uy,uz, mLevel[lev].gravity); \
	usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
	RAC(tcel,dC ) = (1.0-OMEGA(lev))*CSRC_C  + OMEGA(lev)*EQC ; \
	RAC(tcel,dN ) = (1.0-OMEGA(lev))*CSRC_N  + OMEGA(lev)*EQN ; \
	RAC(tcel,dS ) = (1.0-OMEGA(lev))*CSRC_S  + OMEGA(lev)*EQS ; \
	RAC(tcel,dE ) = (1.0-OMEGA(lev))*CSRC_E  + OMEGA(lev)*EQE ; \
	RAC(tcel,dW ) = (1.0-OMEGA(lev))*CSRC_W  + OMEGA(lev)*EQW ; \
	RAC(tcel,dT ) = (1.0-OMEGA(lev))*CSRC_T  + OMEGA(lev)*EQT ; \
	RAC(tcel,dB ) = (1.0-OMEGA(lev))*CSRC_B  + OMEGA(lev)*EQB ; \
	 \
	RAC(tcel,dNE) = (1.0-OMEGA(lev))*CSRC_NE + OMEGA(lev)*EQNE; \
	RAC(tcel,dNW) = (1.0-OMEGA(lev))*CSRC_NW + OMEGA(lev)*EQNW; \
	RAC(tcel,dSE) = (1.0-OMEGA(lev))*CSRC_SE + OMEGA(lev)*EQSE; \
	RAC(tcel,dSW) = (1.0-OMEGA(lev))*CSRC_SW + OMEGA(lev)*EQSW; \
	 \
	RAC(tcel,dNT) = (1.0-OMEGA(lev))*CSRC_NT + OMEGA(lev)*EQNT; \
	RAC(tcel,dNB) = (1.0-OMEGA(lev))*CSRC_NB + OMEGA(lev)*EQNB; \
	RAC(tcel,dST) = (1.0-OMEGA(lev))*CSRC_ST + OMEGA(lev)*EQST; \
	RAC(tcel,dSB) = (1.0-OMEGA(lev))*CSRC_SB + OMEGA(lev)*EQSB; \
	 \
	RAC(tcel,dET) = (1.0-OMEGA(lev))*CSRC_ET + OMEGA(lev)*EQET; \
	RAC(tcel,dEB) = (1.0-OMEGA(lev))*CSRC_EB + OMEGA(lev)*EQEB; \
	RAC(tcel,dWT) = (1.0-OMEGA(lev))*CSRC_WT + OMEGA(lev)*EQWT; \
	RAC(tcel,dWB) = (1.0-OMEGA(lev))*CSRC_WB + OMEGA(lev)*EQWB; \





// LES switching for OPT3D
#if USE_LES==1
#define DEFAULT_COLLIDEG(grav) DEFAULT_COLLIDE_LES(grav)
#define OPTIMIZED_STREAMCOLLIDE OPTIMIZED_STREAMCOLLIDE_LES
#else 
#define DEFAULT_COLLIDEG(grav) DEFAULT_COLLIDE_NOLES(grav)
#define OPTIMIZED_STREAMCOLLIDE OPTIMIZED_STREAMCOLLIDE_NOLES
#endif

#endif  // 3D, opt OPT3D==true

#define USQRMAXCHECK(Cusqr,Cux,Cuy,Cuz,  CmMaxVlen,CmMxvx,CmMxvy,CmMxvz) \
			if(Cusqr>CmMaxVlen) { \
				CmMxvx = Cux; CmMxvy = Cuy; CmMxvz = Cuz; CmMaxVlen = Cusqr; \
			} /* stats */ 



/******************************************************************************
 * interpolateCellFromCoarse macros
 *****************************************************************************/


// WOXDY_N = Weight Order X Dimension Y _ number N
#define WO1D1   ( 1.0/ 2.0)
#define WO1D2   ( 1.0/ 4.0)
#define WO1D3   ( 1.0/ 8.0)

#define WO2D1_1 (-1.0/16.0)
#define WO2D1_9 ( 9.0/16.0)

#define WO2D2_11 (WO2D1_1 * WO2D1_1)
#define WO2D2_19 (WO2D1_9 * WO2D1_1)
#define WO2D2_91 (WO2D1_9 * WO2D1_1)
#define WO2D2_99 (WO2D1_9 * WO2D1_9)

#define WO2D3_111 (WO2D1_1 * WO2D1_1 * WO2D1_1)
#define WO2D3_191 (WO2D1_9 * WO2D1_1 * WO2D1_1)
#define WO2D3_911 (WO2D1_9 * WO2D1_1 * WO2D1_1)
#define WO2D3_991 (WO2D1_9 * WO2D1_9 * WO2D1_1)
#define WO2D3_119 (WO2D1_1 * WO2D1_1 * WO2D1_9)
#define WO2D3_199 (WO2D1_9 * WO2D1_1 * WO2D1_9)
#define WO2D3_919 (WO2D1_9 * WO2D1_1 * WO2D1_9)
#define WO2D3_999 (WO2D1_9 * WO2D1_9 * WO2D1_9)

#if FSGR_STRICT_DEBUG==1
#define ADD_INT_DFSCHECK(alev, ai,aj,ak, at, afac, l) \
				if(	(((1.0-(at))>0.0) && (!(QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr , l) > -1.0 ))) || \
						(((    (at))>0.0) && (!(QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setOther, l) > -1.0 ))) ){ \
					errMsg("INVDFSCHECK", " l"<<(alev)<<" "<<PRINT_VEC((ai),(aj),(ak))<<" fc:"<<RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr )<<" fo:"<<RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther )<<" dfl"<<l ); \
					debugMarkCell((alev), (ai),(aj),(ak));\
					CAUSE_PANIC; \
				}
				// end ADD_INT_DFSCHECK
#define ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac) \
				if(	(((1.0-(at))>0.0) && (!(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr )&(CFInter|CFFluid|CFGrCoarseInited) ))) || \
						(((    (at))>0.0) && (!(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther)&(CFInter|CFFluid|CFGrCoarseInited) ))) ){ \
					errMsg("INVFLAGCINTCHECK", " l"<<(alev)<<" at:"<<(at)<<" "<<PRINT_VEC((ai),(aj),(ak))<<\
							" fc:"<<   convertCellFlagType2String(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr  )) <<\
							" fold:"<< convertCellFlagType2String(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther )) ); \
					debugMarkCell((alev), (ai),(aj),(ak));\
					CAUSE_PANIC; \
				}
				// end ADD_INT_DFSCHECK
				
#define INTUNUTCHECK(ix,iy,iz) \
				if(	(RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) != (CFFluid|CFGrFromCoarse)) ){\
					errMsg("INTFLAGUNU_CHECK", PRINT_VEC(i,j,k)<<" child not unused at l"<<(lev+1)<<" "<<PRINT_VEC((ix),(iy),(iz))<<" flag: "<<  RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) ); \
					debugMarkCell((lev+1), (ix),(iy),(iz));\
					CAUSE_PANIC; \
				}\
				RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) |= CFGrCoarseInited; \
				// INTUNUTCHECK 
#define INTSTRICTCHECK(ix,iy,iz,caseId) \
				if(	QCELL(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr, l) <= 0.0 ){\
					errMsg("INVDFCCELLCHECK", "caseId:"<<caseId<<" "<<PRINT_VEC(i,j,k)<<" child inter at "<<PRINT_VEC((ix),(iy),(iz))<<" invalid df "<<l<<" = "<< QCELL(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr, l) ); \
					debugMarkCell((lev+1), (ix),(iy),(iz));\
					CAUSE_PANIC; \
				}\
				// INTSTRICTCHECK

#else// FSGR_STRICT_DEBUG==1
#define ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac) 
#define ADD_INT_DFSCHECK(alev, ai,aj,ak, at, afac, l) 
#define INTSTRICTCHECK(x,y,z,caseId) 
#define INTUNUTCHECK(ix,iy,iz) 
#endif// FSGR_STRICT_DEBUG==1


#if FSGR_STRICT_DEBUG==1
#define INTDEBOUT \
		{ /*LbmFloat rho,ux,uy,uz;*/ \
			rho = ux=uy=uz=0.0; \
			FORDF0{ LbmFloat m = QCELL(lev,i,j,k, dstSet, l); \
				rho += m; ux  += (this->dfDvecX[l]*m); uy  += (this->dfDvecY[l]*m); uz  += (this->dfDvecZ[l]*m);  \
				if(ABS(m)>1.0) { errMsg("interpolateCellFromCoarse", "ICFC_DFCHECK cell  "<<PRINT_IJK<<" m"<<l<<":"<< m );CAUSE_PANIC;}\
				/*errMsg("interpolateCellFromCoarse", " cell "<<PRINT_IJK<<" df"<<l<<":"<<m );*/ \
			}  \
			/*if(this->mPanic) { errMsg("interpolateCellFromCoarse", "ICFC_DFOUT cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) ); }*/ \
			if(markNbs) errMsg("interpolateCellFromCoarse", " cell "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) );  \
			/*errMsg("interpolateCellFromCoarse", "ICFC_DFDEBUG cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) ); */\
		} \
		/* both cases are ok to interpolate */	\
		if( (!(RFLAG(lev,i,j,k, dstSet) & CFGrFromCoarse)) &&	\
				(!(RFLAG(lev,i,j,k, dstSet) & CFUnused)) ) {	\
			/* might also have CFGrCoarseInited (shouldnt be a problem here)*/	\
			errMsg("interpolateCellFromCoarse", "CHECK cell not CFGrFromCoarse? "<<PRINT_IJK<<" flag:"<< RFLAG(lev,i,j,k, dstSet)<<" fstr:"<<convertCellFlagType2String(  RFLAG(lev,i,j,k, dstSet) ));	\
			/* FIXME check this warning...? return; this can happen !? */	\
			/*CAUSE_PANIC;*/	\
		}	\
		// end INTDEBOUT
#else // FSGR_STRICT_DEBUG==1
#define INTDEBOUT 
#endif // FSGR_STRICT_DEBUG==1

	
// t=0.0 -> only current
// t=0.5 -> mix
// t=1.0 -> only other
#if OPT3D==0 
#define ADD_INT_DFS(alev, ai,aj,ak, at, afac) \
						ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac); \
						FORDF0{ \
							LbmFloat df = ( \
									QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr , l)*(1.0-(at)) + \
									QCELL((alev), (ai),(aj),(ak),mLevel[(alev)].setOther, l)*(    (at)) \
									) ; \
							ADD_INT_DFSCHECK(alev, ai,aj,ak, at, afac, l); \
							df *= (afac); \
							rho += df;  \
							ux  += (this->dfDvecX[l]*df);  \
							uy  += (this->dfDvecY[l]*df);   \
							uz  += (this->dfDvecZ[l]*df);   \
							intDf[l] += df; \
						} 
// write interpolated dfs back to cell (correct non-eq. parts)
#define IDF_WRITEBACK_ \
		FORDF0{ \
			LbmFloat eq = getCollideEq(l, rho,ux,uy,uz);\
			QCELL(lev,i,j,k, dstSet, l) = (eq+ (intDf[l]-eq)*mDfScaleDown);\
		} \
		/* check that all values are ok */ \
		INTDEBOUT
#define IDF_WRITEBACK \
		LbmFloat omegaDst, omegaSrc;\
		/* smago new */ \
		LbmFloat feq[LBM_DFNUM]; \
		LbmFloat dfScale = mDfScaleDown; \
		FORDF0{ \
			feq[l] = getCollideEq(l, rho,ux,uy,uz); \
		} \
		if(mLevel[lev  ].lcsmago>0.0) {\
			LbmFloat Qo = this->getLesNoneqTensorCoeff(intDf,feq); \
			omegaDst  = this->getLesOmega(mLevel[lev+0].omega,mLevel[lev+0].lcsmago,Qo); \
			omegaSrc = this->getLesOmega(mLevel[lev-1].omega,mLevel[lev-1].lcsmago,Qo); \
		} else {\
			omegaDst = mLevel[lev+0].omega; \
			omegaSrc = mLevel[lev-1].omega;\
		} \
		 \
		dfScale   = (mLevel[lev+0].timestep/mLevel[lev-1].timestep)* (1.0/omegaDst-1.0)/ (1.0/omegaSrc-1.0);  \
		FORDF0{ \
			/*errMsg("SMAGO"," org"<<mDfScaleDown<<" n"<<dfScale<<" qc"<< QCELL(lev,i,j,k, dstSet, l)<<" idf"<<intDf[l]<<" eq"<<feq[l] ); */ \
			QCELL(lev,i,j,k, dstSet, l) = (feq[l]+ (intDf[l]-feq[l])*dfScale);\
		} \
		/* check that all values are ok */ \
		INTDEBOUT

#else //OPT3D==0 

#define ADDALLVALS \
	addVal = addDfFacT * RAC(addfcel , dC ); \
	                                        intDf[dC ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dN ); \
	             uy+=addVal;               intDf[dN ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dS ); \
	             uy-=addVal;               intDf[dS ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dE ); \
	ux+=addVal;                            intDf[dE ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dW ); \
	ux-=addVal;                            intDf[dW ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dT ); \
	                          uz+=addVal;  intDf[dT ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dB ); \
	                          uz-=addVal;  intDf[dB ] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNE); \
	ux+=addVal; uy+=addVal;               intDf[dNE] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNW); \
	ux-=addVal; uy+=addVal;               intDf[dNW] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dSE); \
	ux+=addVal; uy-=addVal;               intDf[dSE] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dSW); \
	ux-=addVal; uy-=addVal;               intDf[dSW] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNT); \
	             uy+=addVal; uz+=addVal;  intDf[dNT] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dNB); \
	             uy+=addVal; uz-=addVal;  intDf[dNB] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dST); \
	             uy-=addVal; uz+=addVal;  intDf[dST] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dSB); \
	             uy-=addVal; uz-=addVal;  intDf[dSB] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dET); \
	ux+=addVal;              uz+=addVal;  intDf[dET] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dEB); \
	ux+=addVal;              uz-=addVal;  intDf[dEB] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dWT); \
	ux-=addVal;              uz+=addVal;  intDf[dWT] += addVal; rho += addVal; \
	addVal  = addDfFacT * RAC(addfcel , dWB); \
	ux-=addVal;              uz-=addVal;  intDf[dWB] += addVal; rho += addVal; 

#define ADD_INT_DFS(alev, ai,aj,ak, at, afac) \
	addDfFacT = at*afac; \
	addfcel = RACPNT((alev), (ai),(aj),(ak),mLevel[(alev)].setOther); \
	ADDALLVALS\
	addDfFacT = (1.0-at)*afac; \
	addfcel = RACPNT((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr); \
	ADDALLVALS

// also ugly...
#define INTDF_C    intDf[dC ]
#define INTDF_N    intDf[dN ]
#define INTDF_S    intDf[dS ]
#define INTDF_E    intDf[dE ]
#define INTDF_W    intDf[dW ]
#define INTDF_T    intDf[dT ]
#define INTDF_B    intDf[dB ]
#define INTDF_NE   intDf[dNE]
#define INTDF_NW   intDf[dNW]
#define INTDF_SE   intDf[dSE]
#define INTDF_SW   intDf[dSW]
#define INTDF_NT   intDf[dNT]
#define INTDF_NB   intDf[dNB]
#define INTDF_ST   intDf[dST]
#define INTDF_SB   intDf[dSB]
#define INTDF_ET   intDf[dET]
#define INTDF_EB   intDf[dEB]
#define INTDF_WT   intDf[dWT]
#define INTDF_WB   intDf[dWB]


// write interpolated dfs back to cell (correct non-eq. parts)
#define IDF_WRITEBACK_LES \
		dstcell = RACPNT(lev, i,j,k,dstSet); \
		usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
		\
		lcsmeq[dC] = EQC ; \
		COLL_CALCULATE_DFEQ(lcsmeq); \
		COLL_CALCULATE_NONEQTENSOR(lev, INTDF_ )\
		COLL_CALCULATE_CSMOMEGAVAL(lev+0, lcsmDstOmega); \
		COLL_CALCULATE_CSMOMEGAVAL(lev-1, lcsmSrcOmega); \
		\
		lcsmdfscale   = (mLevel[lev+0].timestep/mLevel[lev-1].timestep)* (1.0/lcsmDstOmega-1.0)/ (1.0/lcsmSrcOmega-1.0);  \
		RAC(dstcell, dC ) = (lcsmeq[dC ] + (intDf[dC ]-lcsmeq[dC ] )*lcsmdfscale);\
		RAC(dstcell, dN ) = (lcsmeq[dN ] + (intDf[dN ]-lcsmeq[dN ] )*lcsmdfscale);\
		RAC(dstcell, dS ) = (lcsmeq[dS ] + (intDf[dS ]-lcsmeq[dS ] )*lcsmdfscale);\
		RAC(dstcell, dE ) = (lcsmeq[dE ] + (intDf[dE ]-lcsmeq[dE ] )*lcsmdfscale);\
		RAC(dstcell, dW ) = (lcsmeq[dW ] + (intDf[dW ]-lcsmeq[dW ] )*lcsmdfscale);\
		RAC(dstcell, dT ) = (lcsmeq[dT ] + (intDf[dT ]-lcsmeq[dT ] )*lcsmdfscale);\
		RAC(dstcell, dB ) = (lcsmeq[dB ] + (intDf[dB ]-lcsmeq[dB ] )*lcsmdfscale);\
		RAC(dstcell, dNE) = (lcsmeq[dNE] + (intDf[dNE]-lcsmeq[dNE] )*lcsmdfscale);\
		RAC(dstcell, dNW) = (lcsmeq[dNW] + (intDf[dNW]-lcsmeq[dNW] )*lcsmdfscale);\
		RAC(dstcell, dSE) = (lcsmeq[dSE] + (intDf[dSE]-lcsmeq[dSE] )*lcsmdfscale);\
		RAC(dstcell, dSW) = (lcsmeq[dSW] + (intDf[dSW]-lcsmeq[dSW] )*lcsmdfscale);\
		RAC(dstcell, dNT) = (lcsmeq[dNT] + (intDf[dNT]-lcsmeq[dNT] )*lcsmdfscale);\
		RAC(dstcell, dNB) = (lcsmeq[dNB] + (intDf[dNB]-lcsmeq[dNB] )*lcsmdfscale);\
		RAC(dstcell, dST) = (lcsmeq[dST] + (intDf[dST]-lcsmeq[dST] )*lcsmdfscale);\
		RAC(dstcell, dSB) = (lcsmeq[dSB] + (intDf[dSB]-lcsmeq[dSB] )*lcsmdfscale);\
		RAC(dstcell, dET) = (lcsmeq[dET] + (intDf[dET]-lcsmeq[dET] )*lcsmdfscale);\
		RAC(dstcell, dEB) = (lcsmeq[dEB] + (intDf[dEB]-lcsmeq[dEB] )*lcsmdfscale);\
		RAC(dstcell, dWT) = (lcsmeq[dWT] + (intDf[dWT]-lcsmeq[dWT] )*lcsmdfscale);\
		RAC(dstcell, dWB) = (lcsmeq[dWB] + (intDf[dWB]-lcsmeq[dWB] )*lcsmdfscale);\
		/* IDF_WRITEBACK optimized */

#define IDF_WRITEBACK_NOLES \
		dstcell = RACPNT(lev, i,j,k,dstSet); \
		usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
		\
		RAC(dstcell, dC ) = (EQC  + (intDf[dC ]-EQC  )*mDfScaleDown);\
		RAC(dstcell, dN ) = (EQN  + (intDf[dN ]-EQN  )*mDfScaleDown);\
		RAC(dstcell, dS ) = (EQS  + (intDf[dS ]-EQS  )*mDfScaleDown);\
		/*old*/ RAC(dstcell, dE ) = (EQE  + (intDf[dE ]-EQE  )*mDfScaleDown);\
		RAC(dstcell, dW ) = (EQW  + (intDf[dW ]-EQW  )*mDfScaleDown);\
		RAC(dstcell, dT ) = (EQT  + (intDf[dT ]-EQT  )*mDfScaleDown);\
		RAC(dstcell, dB ) = (EQB  + (intDf[dB ]-EQB  )*mDfScaleDown);\
		/*old*/ RAC(dstcell, dNE) = (EQNE + (intDf[dNE]-EQNE )*mDfScaleDown);\
		RAC(dstcell, dNW) = (EQNW + (intDf[dNW]-EQNW )*mDfScaleDown);\
		RAC(dstcell, dSE) = (EQSE + (intDf[dSE]-EQSE )*mDfScaleDown);\
		RAC(dstcell, dSW) = (EQSW + (intDf[dSW]-EQSW )*mDfScaleDown);\
		RAC(dstcell, dNT) = (EQNT + (intDf[dNT]-EQNT )*mDfScaleDown);\
		RAC(dstcell, dNB) = (EQNB + (intDf[dNB]-EQNB )*mDfScaleDown);\
		RAC(dstcell, dST) = (EQST + (intDf[dST]-EQST )*mDfScaleDown);\
		RAC(dstcell, dSB) = (EQSB + (intDf[dSB]-EQSB )*mDfScaleDown);\
		RAC(dstcell, dET) = (EQET + (intDf[dET]-EQET )*mDfScaleDown);\
		/*old*/ RAC(dstcell, dEB) = (EQEB + (intDf[dEB]-EQEB )*mDfScaleDown);\
		RAC(dstcell, dWT) = (EQWT + (intDf[dWT]-EQWT )*mDfScaleDown);\
		RAC(dstcell, dWB) = (EQWB + (intDf[dWB]-EQWB )*mDfScaleDown);\
		/* IDF_WRITEBACK optimized */

#if USE_LES==1
#define IDF_WRITEBACK IDF_WRITEBACK_LES
#else 
#define IDF_WRITEBACK IDF_WRITEBACK_NOLES
#endif

#endif// OPT3D==0 



/******************************************************************************/
/*! relaxation LES functions */
/******************************************************************************/


inline LbmFloat LbmFsgrSolver::getLesNoneqTensorCoeff(
		LbmFloat df[], 				
		LbmFloat feq[] ) {
	LbmFloat Qo = 0.0;
	for(int m=0; m< ((LBMDIM*LBMDIM)-LBMDIM)/2 ; m++) { 
		LbmFloat qadd = 0.0;
		for(int l=1; l<this->cDfNum; l++) { 
			if(this->lesCoeffOffdiag[m][l]==0.0) continue;
			qadd += this->lesCoeffOffdiag[m][l]*(df[l]-feq[l]);
		}
		Qo += (qadd*qadd);
	}
	Qo *= 2.0; // off diag twice
	for(int m=0; m<LBMDIM; m++) { 
		LbmFloat qadd = 0.0;
		for(int l=1; l<this->cDfNum; l++) { 
			if(this->lesCoeffDiag[m][l]==0.0) continue;
			qadd += this->lesCoeffDiag[m][l]*(df[l]-feq[l]);
		}
		Qo += (qadd*qadd);
	}
	Qo = sqrt(Qo);
	return Qo;
};

inline LbmFloat LbmFsgrSolver::getLesOmega(LbmFloat omega, LbmFloat csmago, LbmFloat Qo) {
	const LbmFloat tau = 1.0/omega;
	const LbmFloat nu = (2.0*tau-1.0) * (1.0/6.0);
	const LbmFloat C = csmago;
	const LbmFloat Csqr = C*C;
	LbmFloat S = -nu + sqrt( nu*nu + 18.0*Csqr*Qo ) / (6.0*Csqr);
	return( 1.0/( 3.0*( nu+Csqr*S ) +0.5 ) );
}

#define DEBUG_CALCPRINTCELL(str,df) {\
		LbmFloat prho=df[0], pux=0., puy=0., puz=0.; \
		for(int dfl=1; dfl<this->cDfNum; dfl++) { \
			prho += df[dfl];  \
			pux  += (this->dfDvecX[dfl]*df[dfl]);  \
			puy  += (this->dfDvecY[dfl]*df[dfl]);  \
			puz  += (this->dfDvecZ[dfl]*df[dfl]);  \
		} \
		errMsg("DEBUG_CALCPRINTCELL",">"<<str<<" rho="<<prho<<" vel="<<ntlVec3Gfx(pux,puy,puz) ); \
	} /* END DEBUG_CALCPRINTCELL */ 

// "normal" collision
inline void LbmFsgrSolver::collideArrays(
		int lev, int i, int j, int k, // position - more for debugging
		LbmFloat df[], 				
		LbmFloat &outrho, // out only!
		// velocity modifiers (returns actual velocity!)
		LbmFloat &mux, LbmFloat &muy, LbmFloat &muz, 
		LbmFloat omega, 
		LbmVec gravity,
		LbmFloat csmago, 
		LbmFloat *newOmegaRet, LbmFloat *newQoRet
	) {
	int l;
	LbmFloat rho=df[0]; 
	LbmFloat ux = 0; //mux;
	LbmFloat uy = 0; //muy;
	LbmFloat uz = 0; //muz; 
	LbmFloat feq[19];
	LbmFloat omegaNew;
	LbmFloat Qo = 0.0;

	for(l=1; l<this->cDfNum; l++) { 
		rho += df[l]; 
		ux  += (this->dfDvecX[l]*df[l]); 
		uy  += (this->dfDvecY[l]*df[l]);  
		uz  += (this->dfDvecZ[l]*df[l]);  
	}  


	PRECOLLIDE_MODS(rho,ux,uy,uz, gravity);
	for(l=0; l<this->cDfNum; l++) { 
		feq[l] = getCollideEq(l,rho,ux,uy,uz); 
	}

	if(csmago>0.0) {
		Qo = getLesNoneqTensorCoeff(df,feq);
		omegaNew = getLesOmega(omega,csmago,Qo);
	} else {
		omegaNew = omega; // smago off...
	}
	if(newOmegaRet) *newOmegaRet = omegaNew; // return value for stats
	if(newQoRet)    *newQoRet = Qo; // return value of non-eq. stress tensor

	for(l=0; l<this->cDfNum; l++) { 
		df[l] = (1.0-omegaNew ) * df[l] + omegaNew * feq[l]; 
	}  
	//if((i==16)&&(j==10)) DEBUG_CALCPRINTCELL( "2dcoll "<<PRINT_IJK, df);

	mux = ux;
	muy = uy;
	muz = uz;
	outrho = rho;

	lev=i=j=k; // debug, remove warnings
};


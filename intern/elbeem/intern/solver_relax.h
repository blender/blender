/******************************************************************************
 *
 * El'Beem - the visual lattice boltzmann freesurface simulator
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann relaxation macros
 *
 *****************************************************************************/


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

// treatment of freeslip reflection
// used both for OPT and nonOPT
#define DEFAULT_STREAM_FREESLIP(l,invl,mnbf) \
		/*const int inv_l = D::dfInv[l];*/ \
		int nb1 = 0, nb2 = 0;   /* is neighbor in this direction an obstacle? */\
		LbmFloat newval = 0.0; /* new value for m[l], differs for free/part slip */\
		const int dx = D::dfVecX[invl], dy = D::dfVecY[invl], dz = D::dfVecZ[invl]; \
		if(dz==0) { \
			nb1 = !(RFLAG(lev, i,   j+dy,k, SRCS(lev))&(CFFluid|CFInter)); \
			nb2 = !(RFLAG(lev, i+dx,j,   k, SRCS(lev))&(CFFluid|CFInter)); \
			if((nb1)&&(!nb2)) { \
				/* x reflection */\
				newval = QCELL(lev, i+dx,j,k,SRCS(lev), D::dfRefX[l]); \
			} else  \
			if((!nb1)&&(nb2)) { \
				/* y reflection */\
				newval = QCELL(lev, i,j+dy,k,SRCS(lev), D::dfRefY[l]); \
			} else { \
				/* normal no slip in all other cases */\
				newval = QCELL(lev, i,j,k,SRCS(lev), invl); \
			} \
		} else /* z=0 */\
		if(dy==0) { \
			nb1 = !(RFLAG(lev, i,j,k+dz, SRCS(lev))&(CFFluid|CFInter)); \
			nb2 = !(RFLAG(lev, i+dx,j,k, SRCS(lev))&(CFFluid|CFInter)); \
			if((nb1)&&(!nb2)) { \
				/* x reflection */\
				newval = QCELL(lev, i+dx,j,k,SRCS(lev), D::dfRefX[l]); \
			} else  \
			if((!nb1)&&(nb2)) { \
				/* z reflection */\
				newval = QCELL(lev, i,j,k+dz,SRCS(lev), D::dfRefZ[l]); \
			} else { \
				/* normal no slip in all other cases */\
				newval = ( QCELL(lev, i,j,k,SRCS(lev), invl) ); \
			} \
			/* end y=0 */ \
		} else { \
			/* x=0 */\
			nb1 = !(RFLAG(lev, i,j,k+dz, SRCS(lev))&(CFFluid|CFInter)); \
			nb2 = !(RFLAG(lev, i,j+dy,k, SRCS(lev))&(CFFluid|CFInter)); \
			if((nb1)&&(!nb2)) { \
				/* y reflection */\
				newval = QCELL(lev, i,j+dy,k,SRCS(lev), D::dfRefY[l]); \
			} else  \
			if((!nb1)&&(nb2)) { \
				/* z reflection */\
				newval = QCELL(lev, i,j,k+dz,SRCS(lev), D::dfRefZ[l]); \
			} else { \
				/* normal no slip in all other cases */\
				newval = ( QCELL(lev, i,j,k,SRCS(lev), invl) ); \
			} \
		} \
		if(mnbf & CFBndPartslip) { /* part slip interpolation */ \
			const LbmFloat partv = mObjectPartslips[(int)(mnbf>>24)]; \
			m[l] = RAC(ccel, D::dfInv[l] ) * partv + newval * (1.0-partv); /* part slip */ \
		} else {\
			m[l] = newval; /* normal free slip*/\
		}\

// complete default stream&collide, 2d/3d
/* read distribution funtions of adjacent cells = sweep step */ 
#if OPT3D==0 

#if FSGR_STRICT_DEBUG==1
#define MARKCELLCHECK \
	debugMarkCell(lev,i,j,k); D::mPanic=1;
#define STREAMCHECK(ni,nj,nk,nl) \
	if((m[l] < -1.0) || (m[l]>1.0)) {\
		errMsg("STREAMCHECK","Invalid streamed DF l"<<l<<" value:"<<m[l]<<" at "<<PRINT_IJK<<" from "<<PRINT_VEC(ni,nj,nk)<<" nl"<<(nl)<<\
				" nfc"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setCurr)<<" nfo"<< RFLAG(lev, ni,nj,nk, mLevel[lev].setOther)  ); \
		MARKCELLCHECK; \
	}
#define COLLCHECK \
	if( (rho>2.0) || (rho<-1.0) || (ABS(ux)>1.0) || (ABS(uy)>1.0) |(ABS(uz)>1.0) ) {\
		errMsg("COLLCHECK","Invalid collision values r:"<<rho<<" u:"PRINT_VEC(ux,uy,uz)<<" at? "<<PRINT_IJK ); \
		MARKCELLCHECK; \
	}
#else
#define STREAMCHECK(ni,nj,nk,nl) 
#define COLLCHECK
#endif

// careful ux,uy,uz need to be inited before!

#define DEFAULT_STREAM \
		m[dC] = RAC(ccel,dC); \
		FORDF1 { \
			CellFlagType nbf = nbflag[ D::dfInv[l] ];\
			if(nbf & CFBnd) { \
				if(nbf & CFBndNoslip) { \
					/* no slip, default */ \
					m[l] = RAC(ccel, D::dfInv[l] ); STREAMCHECK(i,j,k, D::dfInv[l]); /* noslip */ \
				} else if(nbf & (CFBndFreeslip|CFBndPartslip)) { \
					/* free slip */ \
					if(l<=LBMDIM*2) { \
						m[l] = RAC(ccel, D::dfInv[l] ); STREAMCHECK(i,j,k, D::dfInv[l]); /* noslip for <dim*2 */ \
					} else { \
					const int inv_l = D::dfInv[l]; \
					DEFAULT_STREAM_FREESLIP(l,inv_l,nbf); \
				} /* l>2*dim free slip */ \
				\
				} /* type reflect */\
				else {\
					errMsg("LbmFsgrSolver","Invalid Bnd type at "<<PRINT_IJK<<" f"<<convertCellFlagType2String(nbf)<<",nbdir"<<D::dfInv[l] ); \
				} \
			} else { \
				m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l); \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			} \
		}   

#define _________________DEFAULT_STREAM \
		m[dC] = RAC(ccel,dC); \
		FORDF1 { \
			CellFlagType nbf = nbflag[ D::dfInv[l] ];\
			if(nbf & CFBnd) { \
				if(nbf & CFBndNoslip) { \
					/* no slip, default */ \
					m[l] = RAC(ccel, D::dfInv[l] ); STREAMCHECK(i,j,k, D::dfInv[l]); /* noslip */ \
				} else if(nbf & (CFBndFreeslip|CFBndPartslip)) { \
					/* free slip */ \
					if(l<=LBMDIM*2) { \
						m[l] = RAC(ccel, D::dfInv[l] ); STREAMCHECK(i,j,k, D::dfInv[l]); /* noslip for <dim*2 */ \
					} else { \
					const int inv_l = D::dfInv[l]; \
					int debug_srcl = -1; \
					int nb1 = 0, nb2 = 0;   /* is neighbor in this direction an obstacle? */\
					LbmFloat newval = 0.0; /* new value for m[l], differs for free/part slip */\
					const int dx = D::dfVecX[inv_l], dy = D::dfVecY[inv_l], dz = D::dfVecZ[inv_l]; \
					\
					if(dz==0) { \
						nb1 = !(RFLAG(lev, i,   j+dy,k, SRCS(lev))&(CFFluid|CFInter)); /* FIXME add noslip|free|part here */ \
						nb2 = !(RFLAG(lev, i+dx,j,   k, SRCS(lev))&(CFFluid|CFInter)); \
						if((nb1)&&(!nb2)) { \
							/* x reflection */\
							newval = QCELL(lev, i+dx,j,k,SRCS(lev), D::dfRefX[l]); \
							debug_srcl = D::dfRefX[l]; \
						} else  \
						if((!nb1)&&(nb2)) { \
							/* y reflection */\
							newval = QCELL(lev, i,j+dy,k,SRCS(lev), D::dfRefY[l]); \
							debug_srcl = D::dfRefY[l]; \
						} else { \
							/* normal no slip in all other cases */\
							newval = QCELL(lev, i,j,k,SRCS(lev), inv_l); \
							debug_srcl = inv_l; \
						} \
					} else /* z=0 */\
					if(dy==0) { \
						nb1 = !(RFLAG(lev, i,j,k+dz, SRCS(lev))&(CFFluid|CFInter)); \
						nb2 = !(RFLAG(lev, i+dx,j,k, SRCS(lev))&(CFFluid|CFInter)); \
						if((nb1)&&(!nb2)) { \
							/* x reflection */\
							newval = QCELL(lev, i+dx,j,k,SRCS(lev), D::dfRefX[l]); \
						} else  \
						if((!nb1)&&(nb2)) { \
							/* z reflection */\
							newval = QCELL(lev, i,j,k+dz,SRCS(lev), D::dfRefZ[l]); \
						} else { \
							/* normal no slip in all other cases */\
							newval = ( QCELL(lev, i,j,k,SRCS(lev), inv_l) ); \
						} \
						/* end y=0 */ \
					} else { \
						/* x=0 */\
						nb1 = !(RFLAG(lev, i,j,k+dz, SRCS(lev))&(CFFluid|CFInter)); \
						nb2 = !(RFLAG(lev, i,j+dy,k, SRCS(lev))&(CFFluid|CFInter)); \
						if((nb1)&&(!nb2)) { \
							/* y reflection */\
							newval = QCELL(lev, i,j+dy,k,SRCS(lev), D::dfRefY[l]); \
						} else  \
						if((!nb1)&&(nb2)) { \
							/* z reflection */\
							newval = QCELL(lev, i,j,k+dz,SRCS(lev), D::dfRefZ[l]); \
						} else { \
							/* normal no slip in all other cases */\
							newval = ( QCELL(lev, i,j,k,SRCS(lev), inv_l) ); \
						} \
					} \
					if(nbf & CFBndPartslip) { /* part slip interpolation */ \
						const LbmFloat partv = mObjectPartslips[(int)(nbf>>24)]; \
						m[l] = RAC(ccel, D::dfInv[l] ) * partv + newval * (1.0-partv); /* part slip */ \
					} else {\
						m[l] = newval; /* normal free slip*/\
					}\
					/*if(RFLAG(lev, i,j,k, SRCS(lev))&CFInter) errMsg("FS","at "<<PRINT_IJK<<",l"<<l<<" nb1"<<nb1<<" nb2"<<nb2<<" dx"<<PRINT_VEC(dx,dy,dz)<<",srcl"<<debug_srcl<<" -> "<<newval );*/ \
				} /* l>2*dim free slip */ \
				\
				} /* type reflect */\
				else {\
					errMsg("LbmFsgrSolver","Invalid Bnd type at "<<PRINT_IJK<<" f"<<convertCellFlagType2String(nbf)<<",nbdir"<<D::dfInv[l] ); \
				} \
			} else { \
				m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l,l); \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			} \
		}   

// careful ux,uy,uz need to be inited before!
#define DEFAULT_COLLIDE \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago, &mDebugOmegaRet, &lcsmqo ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			FORDF0 { RAC(tcel,l) = m[l]; }   \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
			COLLCHECK;
#define OPTIMIZED_STREAMCOLLIDE \
			m[0] = RAC(ccel,0); \
			FORDF1 { /* df0 is set later on... */ \
				/* FIXME CHECK INV ? */\
				if(RFLAG_NBINV(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); D::mPanic=1;  \
				} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			}   \
			rho=m[0]; ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago , &mDebugOmegaRet, &lcsmqo   ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			FORDF0 { RAC(tcel,l) = m[l]; } \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
			COLLCHECK;

#else  // 3D, opt OPT3D==true


#define DEFAULT_STREAM \
		m[dC] = RAC(ccel,dC); \
		/* explicit streaming */ \
		if((!nbored & CFBnd)) { \
			/* no boundary near?, no real speed diff.? */ \
			m[dN ] = CSRC_N ; m[dS ] = CSRC_S ; \
			m[dE ] = CSRC_E ; m[dW ] = CSRC_W ; \
			m[dT ] = CSRC_T ; m[dB ] = CSRC_B ; \
			m[dNE] = CSRC_NE; m[dNW] = CSRC_NW; m[dSE] = CSRC_SE; m[dSW] = CSRC_SW; \
			m[dNT] = CSRC_NT; m[dNB] = CSRC_NB; m[dST] = CSRC_ST; m[dSB] = CSRC_SB; \
			m[dET] = CSRC_ET; m[dEB] = CSRC_EB; m[dWT] = CSRC_WT; m[dWB] = CSRC_WB; \
		} else { \
			/* explicit streaming, normal velocity always zero for obstacles */ \
			if(nbflag[dS ]&CFBnd) { m[dN ] = RAC(ccel,dS ); } else { m[dN ] = CSRC_N ; } \
			if(nbflag[dN ]&CFBnd) { m[dS ] = RAC(ccel,dN ); } else { m[dS ] = CSRC_S ; } \
			if(nbflag[dW ]&CFBnd) { m[dE ] = RAC(ccel,dW ); } else { m[dE ] = CSRC_E ; } \
			if(nbflag[dE ]&CFBnd) { m[dW ] = RAC(ccel,dE ); } else { m[dW ] = CSRC_W ; } \
			if(nbflag[dB ]&CFBnd) { m[dT ] = RAC(ccel,dB ); } else { m[dT ] = CSRC_T ; } \
			if(nbflag[dT ]&CFBnd) { m[dB ] = RAC(ccel,dT ); } else { m[dB ] = CSRC_B ; } \
 			\
			/* also treat free slip here */ \
			if(nbflag[dSW]&CFBnd) { if(nbflag[dSW]&CFBndNoslip){ m[dNE] = RAC(ccel,dSW); }else{ DEFAULT_STREAM_FREESLIP(dNE,dSW,nbflag[dSW]);} } else { m[dNE] = CSRC_NE; } \
			if(nbflag[dSE]&CFBnd) { if(nbflag[dSE]&CFBndNoslip){ m[dNW] = RAC(ccel,dSE); }else{ DEFAULT_STREAM_FREESLIP(dNW,dSE,nbflag[dSE]);} } else { m[dNW] = CSRC_NW; } \
			if(nbflag[dNW]&CFBnd) { if(nbflag[dNW]&CFBndNoslip){ m[dSE] = RAC(ccel,dNW); }else{ DEFAULT_STREAM_FREESLIP(dSE,dNW,nbflag[dNW]);} } else { m[dSE] = CSRC_SE; } \
			if(nbflag[dNE]&CFBnd) { if(nbflag[dNE]&CFBndNoslip){ m[dSW] = RAC(ccel,dNE); }else{ DEFAULT_STREAM_FREESLIP(dSW,dNE,nbflag[dNE]);} } else { m[dSW] = CSRC_SW; } \
			if(nbflag[dSB]&CFBnd) { if(nbflag[dSB]&CFBndNoslip){ m[dNT] = RAC(ccel,dSB); }else{ DEFAULT_STREAM_FREESLIP(dNT,dSB,nbflag[dSB]);} } else { m[dNT] = CSRC_NT; } \
			if(nbflag[dST]&CFBnd) { if(nbflag[dST]&CFBndNoslip){ m[dNB] = RAC(ccel,dST); }else{ DEFAULT_STREAM_FREESLIP(dNB,dST,nbflag[dST]);} } else { m[dNB] = CSRC_NB; } \
			if(nbflag[dNB]&CFBnd) { if(nbflag[dNB]&CFBndNoslip){ m[dST] = RAC(ccel,dNB); }else{ DEFAULT_STREAM_FREESLIP(dST,dNB,nbflag[dNB]);} } else { m[dST] = CSRC_ST; } \
			if(nbflag[dNT]&CFBnd) { if(nbflag[dNT]&CFBndNoslip){ m[dSB] = RAC(ccel,dNT); }else{ DEFAULT_STREAM_FREESLIP(dSB,dNT,nbflag[dNT]);} } else { m[dSB] = CSRC_SB; } \
			if(nbflag[dWB]&CFBnd) { if(nbflag[dWB]&CFBndNoslip){ m[dET] = RAC(ccel,dWB); }else{ DEFAULT_STREAM_FREESLIP(dET,dWB,nbflag[dWB]);} } else { m[dET] = CSRC_ET; } \
			if(nbflag[dWT]&CFBnd) { if(nbflag[dWT]&CFBndNoslip){ m[dEB] = RAC(ccel,dWT); }else{ DEFAULT_STREAM_FREESLIP(dEB,dWT,nbflag[dWT]);} } else { m[dEB] = CSRC_EB; } \
			if(nbflag[dEB]&CFBnd) { if(nbflag[dEB]&CFBndNoslip){ m[dWT] = RAC(ccel,dEB); }else{ DEFAULT_STREAM_FREESLIP(dWT,dEB,nbflag[dEB]);} } else { m[dWT] = CSRC_WT; } \
			if(nbflag[dET]&CFBnd) { if(nbflag[dET]&CFBndNoslip){ m[dWB] = RAC(ccel,dET); }else{ DEFAULT_STREAM_FREESLIP(dWB,dET,nbflag[dET]);} } else { m[dWB] = CSRC_WB; } \
		} 



#define COLL_CALCULATE_DFEQ(dstarray) \
			dstarray[dN ] = EQN ; dstarray[dS ] = EQS ; \
			dstarray[dE ] = EQE ; dstarray[dW ] = EQW ; \
			dstarray[dT ] = EQT ; dstarray[dB ] = EQB ; \
			dstarray[dNE] = EQNE; dstarray[dNW] = EQNW; dstarray[dSE] = EQSE; dstarray[dSW] = EQSW; \
			dstarray[dNT] = EQNT; dstarray[dNB] = EQNB; dstarray[dST] = EQST; dstarray[dSB] = EQSB; \
			dstarray[dET] = EQET; dstarray[dEB] = EQEB; dstarray[dWT] = EQWT; dstarray[dWB] = EQWB; 
#define COLL_CALCULATE_NONEQTENSOR(csolev, srcArray ) \
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
			lcsmqo = sqrt(lcsmqo); /* FIXME check effect of sqrt*/ \

//			COLL_CALCULATE_CSMOMEGAVAL(csolev, lcsmomega); 

// careful - need lcsmqo 
#define COLL_CALCULATE_CSMOMEGAVAL(csolev, dstomega ) \
			dstomega =  1.0/\
					( 3.0*( mLevel[(csolev)].lcnu+mLevel[(csolev)].lcsmago_sqr*(\
							-mLevel[(csolev)].lcnu + sqrt( mLevel[(csolev)].lcnu*mLevel[(csolev)].lcnu + 18.0*mLevel[(csolev)].lcsmago_sqr* lcsmqo ) \
							/ (6.0*mLevel[(csolev)].lcsmago_sqr)) \
						) +0.5 ); 

#define DEFAULT_COLLIDE_LES \
			rho = + MSRC_C  + MSRC_N  \
				+ MSRC_S  + MSRC_E  \
				+ MSRC_W  + MSRC_T  \
				+ MSRC_B  + MSRC_NE \
				+ MSRC_NW + MSRC_SE \
				+ MSRC_SW + MSRC_NT \
				+ MSRC_NB + MSRC_ST \
				+ MSRC_SB + MSRC_ET \
				+ MSRC_EB + MSRC_WT \
				+ MSRC_WB; \
 			\
			ux += MSRC_E - MSRC_W \
				+ MSRC_NE - MSRC_NW \
				+ MSRC_SE - MSRC_SW \
				+ MSRC_ET + MSRC_EB \
				- MSRC_WT - MSRC_WB ;  \
 			\
			uy += MSRC_N - MSRC_S \
				+ MSRC_NE + MSRC_NW \
				- MSRC_SE - MSRC_SW \
				+ MSRC_NT + MSRC_NB \
				- MSRC_ST - MSRC_SB ;  \
 			\
			uz += MSRC_T - MSRC_B \
				+ MSRC_NT - MSRC_NB \
				+ MSRC_ST - MSRC_SB \
				+ MSRC_ET - MSRC_EB \
				+ MSRC_WT - MSRC_WB ;  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			COLL_CALCULATE_DFEQ(lcsmeq); \
			COLL_CALCULATE_NONEQTENSOR(lev, MSRC_)\
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
			RAC(tcel,dWB) = (1.0-lcsmomega)*MSRC_WB + lcsmomega*lcsmeq[ dWB]; 

#define DEFAULT_COLLIDE_NOLES \
			rho = + MSRC_C  + MSRC_N  \
				+ MSRC_S  + MSRC_E  \
				+ MSRC_W  + MSRC_T  \
				+ MSRC_B  + MSRC_NE \
				+ MSRC_NW + MSRC_SE \
				+ MSRC_SW + MSRC_NT \
				+ MSRC_NB + MSRC_ST \
				+ MSRC_SB + MSRC_ET \
				+ MSRC_EB + MSRC_WT \
				+ MSRC_WB; \
 			\
			ux += MSRC_E - MSRC_W \
				+ MSRC_NE - MSRC_NW \
				+ MSRC_SE - MSRC_SW \
				+ MSRC_ET + MSRC_EB \
				- MSRC_WT - MSRC_WB ;  \
 			\
			uy += MSRC_N - MSRC_S \
				+ MSRC_NE + MSRC_NW \
				- MSRC_SE - MSRC_SW \
				+ MSRC_NT + MSRC_NB \
				- MSRC_ST - MSRC_SB ;  \
 			\
			uz += MSRC_T - MSRC_B \
				+ MSRC_NT - MSRC_NB \
				+ MSRC_ST - MSRC_SB \
				+ MSRC_ET - MSRC_EB \
				+ MSRC_WT - MSRC_WB ;  \
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
			RAC(tcel,dWB) = (1.0-OMEGA(lev))*MSRC_WB + OMEGA(lev)*EQWB; 



#define OPTIMIZED_STREAMCOLLIDE_LES \
			/* only surrounded by fluid cells...!, so safe streaming here... */ \
			m[dC ] = CSRC_C ; \
			m[dN ] = CSRC_N ; m[dS ] = CSRC_S ; \
			m[dE ] = CSRC_E ; m[dW ] = CSRC_W ; \
			m[dT ] = CSRC_T ; m[dB ] = CSRC_B ; \
			m[dNE] = CSRC_NE; m[dNW] = CSRC_NW; m[dSE] = CSRC_SE; m[dSW] = CSRC_SW; \
			m[dNT] = CSRC_NT; m[dNB] = CSRC_NB; m[dST] = CSRC_ST; m[dSB] = CSRC_SB; \
			m[dET] = CSRC_ET; m[dEB] = CSRC_EB; m[dWT] = CSRC_WT; m[dWB] = CSRC_WB; \
			\
			rho = MSRC_C  + MSRC_N + MSRC_S  + MSRC_E + MSRC_W  + MSRC_T  \
				+ MSRC_B  + MSRC_NE + MSRC_NW + MSRC_SE + MSRC_SW + MSRC_NT \
				+ MSRC_NB + MSRC_ST + MSRC_SB + MSRC_ET + MSRC_EB + MSRC_WT + MSRC_WB; \
			ux = MSRC_E - MSRC_W + MSRC_NE - MSRC_NW + MSRC_SE - MSRC_SW \
				+ MSRC_ET + MSRC_EB - MSRC_WT - MSRC_WB + mLevel[lev].gravity[0];  \
			uy = MSRC_N - MSRC_S + MSRC_NE + MSRC_NW - MSRC_SE - MSRC_SW \
				+ MSRC_NT + MSRC_NB - MSRC_ST - MSRC_SB + mLevel[lev].gravity[1];  \
			uz = MSRC_T - MSRC_B + MSRC_NT - MSRC_NB + MSRC_ST - MSRC_SB \
				+ MSRC_ET - MSRC_EB + MSRC_WT - MSRC_WB + mLevel[lev].gravity[2];  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			COLL_CALCULATE_DFEQ(lcsmeq); \
			COLL_CALCULATE_NONEQTENSOR(lev, MSRC_) \
			COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
			CSMOMEGA_STATS(lev,lcsmomega); \
			\
			RAC(tcel,dC ) = (1.0-lcsmomega)*MSRC_C  + lcsmomega*EQC ; \
			RAC(tcel,dN ) = (1.0-lcsmomega)*MSRC_N  + lcsmomega*lcsmeq[ dN ];  \
			RAC(tcel,dS ) = (1.0-lcsmomega)*MSRC_S  + lcsmomega*lcsmeq[ dS ];  \
			RAC(tcel,dE ) = (1.0-lcsmomega)*MSRC_E  + lcsmomega*lcsmeq[ dE ]; \
			RAC(tcel,dW ) = (1.0-lcsmomega)*MSRC_W  + lcsmomega*lcsmeq[ dW ];  \
			RAC(tcel,dT ) = (1.0-lcsmomega)*MSRC_T  + lcsmomega*lcsmeq[ dT ];  \
			RAC(tcel,dB ) = (1.0-lcsmomega)*MSRC_B  + lcsmomega*lcsmeq[ dB ]; \
			\
			RAC(tcel,dNE) = (1.0-lcsmomega)*MSRC_NE + lcsmomega*lcsmeq[ dNE];  \
			RAC(tcel,dNW) = (1.0-lcsmomega)*MSRC_NW + lcsmomega*lcsmeq[ dNW];  \
			RAC(tcel,dSE) = (1.0-lcsmomega)*MSRC_SE + lcsmomega*lcsmeq[ dSE];  \
			RAC(tcel,dSW) = (1.0-lcsmomega)*MSRC_SW + lcsmomega*lcsmeq[ dSW]; \
			\
			RAC(tcel,dNT) = (1.0-lcsmomega)*MSRC_NT + lcsmomega*lcsmeq[ dNT];  \
			RAC(tcel,dNB) = (1.0-lcsmomega)*MSRC_NB + lcsmomega*lcsmeq[ dNB];  \
			RAC(tcel,dST) = (1.0-lcsmomega)*MSRC_ST + lcsmomega*lcsmeq[ dST];  \
			RAC(tcel,dSB) = (1.0-lcsmomega)*MSRC_SB + lcsmomega*lcsmeq[ dSB]; \
			\
			RAC(tcel,dET) = (1.0-lcsmomega)*MSRC_ET + lcsmomega*lcsmeq[ dET];  \
			RAC(tcel,dEB) = (1.0-lcsmomega)*MSRC_EB + lcsmomega*lcsmeq[ dEB]; \
			RAC(tcel,dWT) = (1.0-lcsmomega)*MSRC_WT + lcsmomega*lcsmeq[ dWT];  \
			RAC(tcel,dWB) = (1.0-lcsmomega)*MSRC_WB + lcsmomega*lcsmeq[ dWB];  \

#define OPTIMIZED_STREAMCOLLIDE_UNUSED \
			/* only surrounded by fluid cells...!, so safe streaming here... */ \
			rho = CSRC_C  + CSRC_N + CSRC_S  + CSRC_E + CSRC_W  + CSRC_T  \
				+ CSRC_B  + CSRC_NE + CSRC_NW + CSRC_SE + CSRC_SW + CSRC_NT \
				+ CSRC_NB + CSRC_ST + CSRC_SB + CSRC_ET + CSRC_EB + CSRC_WT + CSRC_WB; \
			ux = CSRC_E - CSRC_W + CSRC_NE - CSRC_NW + CSRC_SE - CSRC_SW \
				+ CSRC_ET + CSRC_EB - CSRC_WT - CSRC_WB + mLevel[lev].gravity[0];  \
			uy = CSRC_N - CSRC_S + CSRC_NE + CSRC_NW - CSRC_SE - CSRC_SW \
				+ CSRC_NT + CSRC_NB - CSRC_ST - CSRC_SB + mLevel[lev].gravity[1];  \
			uz = CSRC_T - CSRC_B + CSRC_NT - CSRC_NB + CSRC_ST - CSRC_SB \
				+ CSRC_ET - CSRC_EB + CSRC_WT - CSRC_WB + mLevel[lev].gravity[2];  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			COLL_CALCULATE_DFEQ(lcsmeq); \
			COLL_CALCULATE_NONEQTENSOR(lev, CSRC_) \
			COLL_CALCULATE_CSMOMEGAVAL(lev, lcsmomega); \
			\
			RAC(tcel,dC ) = (1.0-lcsmomega)*CSRC_C  + lcsmomega*EQC ; \
			RAC(tcel,dN ) = (1.0-lcsmomega)*CSRC_N  + lcsmomega*lcsmeq[ dN ];  \
			RAC(tcel,dS ) = (1.0-lcsmomega)*CSRC_S  + lcsmomega*lcsmeq[ dS ];  \
			RAC(tcel,dE ) = (1.0-lcsmomega)*CSRC_E  + lcsmomega*lcsmeq[ dE ]; \
			RAC(tcel,dW ) = (1.0-lcsmomega)*CSRC_W  + lcsmomega*lcsmeq[ dW ];  \
			RAC(tcel,dT ) = (1.0-lcsmomega)*CSRC_T  + lcsmomega*lcsmeq[ dT ];  \
			RAC(tcel,dB ) = (1.0-lcsmomega)*CSRC_B  + lcsmomega*lcsmeq[ dB ]; \
			\
			RAC(tcel,dNE) = (1.0-lcsmomega)*CSRC_NE + lcsmomega*lcsmeq[ dNE];  \
			RAC(tcel,dNW) = (1.0-lcsmomega)*CSRC_NW + lcsmomega*lcsmeq[ dNW];  \
			RAC(tcel,dSE) = (1.0-lcsmomega)*CSRC_SE + lcsmomega*lcsmeq[ dSE];  \
			RAC(tcel,dSW) = (1.0-lcsmomega)*CSRC_SW + lcsmomega*lcsmeq[ dSW]; \
			\
			RAC(tcel,dNT) = (1.0-lcsmomega)*CSRC_NT + lcsmomega*lcsmeq[ dNT];  \
			RAC(tcel,dNB) = (1.0-lcsmomega)*CSRC_NB + lcsmomega*lcsmeq[ dNB];  \
			RAC(tcel,dST) = (1.0-lcsmomega)*CSRC_ST + lcsmomega*lcsmeq[ dST];  \
			RAC(tcel,dSB) = (1.0-lcsmomega)*CSRC_SB + lcsmomega*lcsmeq[ dSB]; \
			\
			RAC(tcel,dET) = (1.0-lcsmomega)*CSRC_ET + lcsmomega*lcsmeq[ dET];  \
			RAC(tcel,dEB) = (1.0-lcsmomega)*CSRC_EB + lcsmomega*lcsmeq[ dEB]; \
			RAC(tcel,dWT) = (1.0-lcsmomega)*CSRC_WT + lcsmomega*lcsmeq[ dWT];  \
			RAC(tcel,dWB) = (1.0-lcsmomega)*CSRC_WB + lcsmomega*lcsmeq[ dWB];  \

#define OPTIMIZED_STREAMCOLLIDE_NOLES \
			/* only surrounded by fluid cells...!, so safe streaming here... */ \
			rho = CSRC_C  + CSRC_N + CSRC_S  + CSRC_E + CSRC_W  + CSRC_T  \
				+ CSRC_B  + CSRC_NE + CSRC_NW + CSRC_SE + CSRC_SW + CSRC_NT \
				+ CSRC_NB + CSRC_ST + CSRC_SB + CSRC_ET + CSRC_EB + CSRC_WT + CSRC_WB; \
			ux = CSRC_E - CSRC_W + CSRC_NE - CSRC_NW + CSRC_SE - CSRC_SW \
				+ CSRC_ET + CSRC_EB - CSRC_WT - CSRC_WB + mLevel[lev].gravity[0];  \
			uy = CSRC_N - CSRC_S + CSRC_NE + CSRC_NW - CSRC_SE - CSRC_SW \
				+ CSRC_NT + CSRC_NB - CSRC_ST - CSRC_SB + mLevel[lev].gravity[1];  \
			uz = CSRC_T - CSRC_B + CSRC_NT - CSRC_NB + CSRC_ST - CSRC_SB \
				+ CSRC_ET - CSRC_EB + CSRC_WT - CSRC_WB + mLevel[lev].gravity[2];  \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz); \
			RAC(tcel,dC ) = (1.0-OMEGA(lev))*CSRC_C  + OMEGA(lev)*EQC ; \
			RAC(tcel,dN ) = (1.0-OMEGA(lev))*CSRC_N  + OMEGA(lev)*EQN ;  \
			RAC(tcel,dS ) = (1.0-OMEGA(lev))*CSRC_S  + OMEGA(lev)*EQS ;  \
			RAC(tcel,dE ) = (1.0-OMEGA(lev))*CSRC_E  + OMEGA(lev)*EQE ; \
			RAC(tcel,dW ) = (1.0-OMEGA(lev))*CSRC_W  + OMEGA(lev)*EQW ;  \
			RAC(tcel,dT ) = (1.0-OMEGA(lev))*CSRC_T  + OMEGA(lev)*EQT ;  \
			RAC(tcel,dB ) = (1.0-OMEGA(lev))*CSRC_B  + OMEGA(lev)*EQB ; \
			 \
			RAC(tcel,dNE) = (1.0-OMEGA(lev))*CSRC_NE + OMEGA(lev)*EQNE;  \
			RAC(tcel,dNW) = (1.0-OMEGA(lev))*CSRC_NW + OMEGA(lev)*EQNW;  \
			RAC(tcel,dSE) = (1.0-OMEGA(lev))*CSRC_SE + OMEGA(lev)*EQSE;  \
			RAC(tcel,dSW) = (1.0-OMEGA(lev))*CSRC_SW + OMEGA(lev)*EQSW; \
			 \
			RAC(tcel,dNT) = (1.0-OMEGA(lev))*CSRC_NT + OMEGA(lev)*EQNT;  \
			RAC(tcel,dNB) = (1.0-OMEGA(lev))*CSRC_NB + OMEGA(lev)*EQNB;  \
			RAC(tcel,dST) = (1.0-OMEGA(lev))*CSRC_ST + OMEGA(lev)*EQST;  \
			RAC(tcel,dSB) = (1.0-OMEGA(lev))*CSRC_SB + OMEGA(lev)*EQSB; \
			 \
			RAC(tcel,dET) = (1.0-OMEGA(lev))*CSRC_ET + OMEGA(lev)*EQET;  \
			RAC(tcel,dEB) = (1.0-OMEGA(lev))*CSRC_EB + OMEGA(lev)*EQEB; \
			RAC(tcel,dWT) = (1.0-OMEGA(lev))*CSRC_WT + OMEGA(lev)*EQWT;  \
			RAC(tcel,dWB) = (1.0-OMEGA(lev))*CSRC_WB + OMEGA(lev)*EQWB;  \


// debug version1
#define STREAMCHECK(ni,nj,nk,nl) 
#define COLLCHECK
#define OPTIMIZED_STREAMCOLLIDE_DEBUG \
			m[0] = RAC(ccel,0); \
			FORDF1 { /* df0 is set later on... */ \
				if(RFLAG_NB(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); D::mPanic=1;  \
				} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
				STREAMCHECK(i+D::dfVecX[D::dfInv[l]], j+D::dfVecY[D::dfInv[l]],k+D::dfVecZ[D::dfInv[l]], l); \
			}   \
			rho=m[0]; ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago , &mDebugOmegaRet, &lcsmqo   ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			FORDF0 { RAC(tcel,l) = m[l]; } \
			usqr = 1.5 * (ux*ux + uy*uy + uz*uz);  \
			COLLCHECK;



// more debugging
/*DEBUG \
			m[0] = RAC(ccel,0); \
			FORDF1 { \
				if(RFLAG_NB(lev, i,j,k,SRCS(lev),l)&CFBnd) { errMsg("???", "bnd-err-nobndfl"); D::mPanic=1;  \
				} else { m[l] = QCELL_NBINV(lev, i, j, k, SRCS(lev), l, l); } \
			}   \
errMsg("T","QSDM at %d,%d,%d  lcsmqo=%25.15f, lcsmomega=%f \n", i,j,k, lcsmqo,lcsmomega ); \
			rho=m[0]; ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			ux = mLevel[lev].gravity[0]; uy = mLevel[lev].gravity[1]; uz = mLevel[lev].gravity[2]; \
			D::collideArrays( m, rho,ux,uy,uz, OMEGA(lev), mLevel[lev].lcsmago  , &mDebugOmegaRet, &lcsmqo  ); \
			CSMOMEGA_STATS(lev,mDebugOmegaRet); \
			*/
#if USE_LES==1
#define DEFAULT_COLLIDE DEFAULT_COLLIDE_LES
#define OPTIMIZED_STREAMCOLLIDE OPTIMIZED_STREAMCOLLIDE_LES
#else 
#define DEFAULT_COLLIDE DEFAULT_COLLIDE_NOLES
#define OPTIMIZED_STREAMCOLLIDE OPTIMIZED_STREAMCOLLIDE_NOLES
#endif

#endif

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
					D::mPanic = 1; \
				}
				// end ADD_INT_DFSCHECK
#define ADD_INT_FLAGCHECK(alev, ai,aj,ak, at, afac) \
				if(	(((1.0-(at))>0.0) && (!(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr )&(CFInter|CFFluid|CFGrCoarseInited) ))) || \
						(((    (at))>0.0) && (!(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther)&(CFInter|CFFluid|CFGrCoarseInited) ))) ){ \
					errMsg("INVFLAGCINTCHECK", " l"<<(alev)<<" at:"<<(at)<<" "<<PRINT_VEC((ai),(aj),(ak))<<\
							" fc:"<<   convertCellFlagType2String(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setCurr  )) <<\
							" fold:"<< convertCellFlagType2String(RFLAG((alev), (ai),(aj),(ak),mLevel[(alev)].setOther )) ); \
					debugMarkCell((alev), (ai),(aj),(ak));\
					D::mPanic = 1; \
				}
				// end ADD_INT_DFSCHECK
				
				//if(	!(RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) & CFUnused) ){
				//errMsg("INTFLAGUNU", PRINT_VEC(i,j,k)<<" child at "<<PRINT_VEC((ix),(iy),(iz)) );
				//if(iy==15) errMsg("IFFC", PRINT_VEC(i,j,k)<<" child interpolated at "<<PRINT_VEC((ix),(iy),(iz)) );
				//if(((ix)>10)&&(iy>5)&&(iz>5)) { debugMarkCell(lev+1, (ix),(iy),(iz) ); }
#define INTUNUTCHECK(ix,iy,iz) \
				if(	(RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) != (CFFluid|CFGrFromCoarse)) ){\
					errMsg("INTFLAGUNU_CHECK", PRINT_VEC(i,j,k)<<" child not unused at l"<<(lev+1)<<" "<<PRINT_VEC((ix),(iy),(iz))<<" flag: "<<  RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) ); \
					debugMarkCell((lev+1), (ix),(iy),(iz));\
					D::mPanic = 1; \
				}\
				RFLAG(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr) |= CFGrCoarseInited; \
				// INTUNUTCHECK 
#define INTSTRICTCHECK(ix,iy,iz,caseId) \
				if(	QCELL(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr, l) <= 0.0 ){\
					errMsg("INVDFCCELLCHECK", "caseId:"<<caseId<<" "<<PRINT_VEC(i,j,k)<<" child inter at "<<PRINT_VEC((ix),(iy),(iz))<<" invalid df "<<l<<" = "<< QCELL(lev+1, (ix),(iy),(iz), mLevel[lev+1].setCurr, l) ); \
					debugMarkCell((lev+1), (ix),(iy),(iz));\
					D::mPanic = 1; \
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
				rho += m; ux  += (D::dfDvecX[l]*m); uy  += (D::dfDvecY[l]*m); uz  += (D::dfDvecZ[l]*m);  \
				if(ABS(m)>1.0) { errMsg("interpolateCellFromCoarse", "ICFC_DFCHECK cell  "<<PRINT_IJK<<" m"<<l<<":"<< m ); D::mPanic=1; }\
				/*errMsg("interpolateCellFromCoarse", " cell "<<PRINT_IJK<<" df"<<l<<":"<<m );*/ \
			}  \
			/*if(D::mPanic) { errMsg("interpolateCellFromCoarse", "ICFC_DFOUT cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) ); }*/ \
			if(markNbs) errMsg("interpolateCellFromCoarse", " cell "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) );  \
			/*errMsg("interpolateCellFromCoarse", "ICFC_DFDEBUG cell  "<<PRINT_IJK<<" rho:"<<rho<<" u:"<<PRINT_VEC(ux,uy,uz)<<" b"<<PRINT_VEC(betx,bety,betz) ); */\
		} \
		/* both cases are ok to interpolate */	\
		if( (!(RFLAG(lev,i,j,k, dstSet) & CFGrFromCoarse)) &&	\
				(!(RFLAG(lev,i,j,k, dstSet) & CFUnused)) ) {	\
			/* might also have CFGrCoarseInited (shouldnt be a problem here)*/	\
			errMsg("interpolateCellFromCoarse", "CHECK cell not CFGrFromCoarse? "<<PRINT_IJK<<" flag:"<< RFLAG(lev,i,j,k, dstSet)<<" fstr:"<<convertCellFlagType2String(  RFLAG(lev,i,j,k, dstSet) ));	\
			/* FIXME check this warning...? return; this can happen !? */	\
			/*D::mPanic = 1;*/	\
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
							ux  += (D::dfDvecX[l]*df);  \
							uy  += (D::dfDvecY[l]*df);   \
							uz  += (D::dfDvecZ[l]*df);   \
							intDf[l] += df; \
						} 
// write interpolated dfs back to cell (correct non-eq. parts)
#define IDF_WRITEBACK_ \
		FORDF0{ \
			LbmFloat eq = D::getCollideEq(l, rho,ux,uy,uz);\
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
			feq[l] = D::getCollideEq(l, rho,ux,uy,uz); \
		} \
		if(mLevel[lev  ].lcsmago>0.0) {\
			LbmFloat Qo = D::getLesNoneqTensorCoeff(intDf,feq); \
			omegaDst  = D::getLesOmega(mLevel[lev+0].omega,mLevel[lev+0].lcsmago,Qo); \
			omegaSrc = D::getLesOmega(mLevel[lev-1].omega,mLevel[lev-1].lcsmago,Qo); \
		} else {\
			omegaDst = mLevel[lev+0].omega; \
			omegaSrc = mLevel[lev-1].omega;\
		} \
		 \
		dfScale   = (mLevel[lev+0].stepsize/mLevel[lev-1].stepsize)* (1.0/omegaDst-1.0)/ (1.0/omegaSrc-1.0);  \
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
		lcsmdfscale   = (mLevel[lev+0].stepsize/mLevel[lev-1].stepsize)* (1.0/lcsmDstOmega-1.0)/ (1.0/lcsmSrcOmega-1.0);  \
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


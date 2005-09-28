/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Combined 2D/3D Lattice Boltzmann Solver auxiliary classes
 * 
 *****************************************************************************/
#ifndef LBMHEADER_H

/* LBM Files */
#include "lbminterface.h"
#include <sstream>


//! shorten static const definitions
#define STCON static const


/*****************************************************************************/
/*! class for solver templating - 3D implementation */
//class LbmD3Q19 : public LbmSolverInterface {
class LbmD3Q19 {

	public:

		// constructor, init interface
		LbmD3Q19() {};
		// virtual destructor 
		virtual ~LbmD3Q19() {};
		//! id string of solver
		string getIdString() { return string("3D"); }

		//! how many dimensions?
		STCON int cDimension;

		// Wi factors for collide step 
		STCON LbmFloat cCollenZero;
		STCON LbmFloat cCollenOne;
		STCON LbmFloat cCollenSqrtTwo;

		//! threshold value for filled/emptied cells 
		STCON LbmFloat cMagicNr2;
		STCON LbmFloat cMagicNr2Neg;
		STCON LbmFloat cMagicNr;
		STCON LbmFloat cMagicNrNeg;

		//! size of a single set of distribution functions 
		STCON int    cDfNum;
		//! direction vector contain vecs for all spatial dirs, even if not used for LBM model
		STCON int    cDirNum;

		//! distribution functions directions 
		typedef enum {
			 cDirInv=  -1,
			 cDirC  =  0,
			 cDirN  =  1,
			 cDirS  =  2,
			 cDirE  =  3,
			 cDirW  =  4,
			 cDirT  =  5,
			 cDirB  =  6,
			 cDirNE =  7,
			 cDirNW =  8,
			 cDirSE =  9,
			 cDirSW = 10,
			 cDirNT = 11,
			 cDirNB = 12,
			 cDirST = 13,
			 cDirSB = 14,
			 cDirET = 15,
			 cDirEB = 16,
			 cDirWT = 17,
			 cDirWB = 18
		} dfDir;

		/* Vector Order 3D:
		 *  0   1  2   3  4   5  6       7  8  9 10  11 12 13 14  15 16 17 18     19 20 21 22  23 24 25 26
		 *  0,  0, 0,  1,-1,  0, 0,      1,-1, 1,-1,  0, 0, 0, 0,  1, 1,-1,-1,     1,-1, 1,-1,  1,-1, 1,-1
		 *  0,  1,-1,  0, 0,  0, 0,      1, 1,-1,-1,  1, 1,-1,-1,  0, 0, 0, 0,     1, 1,-1,-1,  1, 1,-1,-1
		 *  0,  0, 0,  0, 0,  1,-1,      0, 0, 0, 0,  1,-1, 1,-1,  1,-1, 1,-1,     1, 1, 1, 1, -1,-1,-1,-1
		 */

		/*! name of the dist. function 
			 only for nicer output */
		STCON char* dfString[ 19 ];

		/*! index of normal dist func, not used so far?... */
		STCON int dfNorm[ 19 ];

		/*! index of inverse dist func, not fast, but useful... */
		STCON int dfInv[ 19 ];

		/*! index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefX[ 19 ];
		/*! index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefY[ 19 ];
		/*! index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefZ[ 19 ];

		/*! dist func vectors */
		STCON int dfVecX[ 27 ];
		STCON int dfVecY[ 27 ];
		STCON int dfVecZ[ 27 ];

		/*! arrays as before with doubles */
		STCON LbmFloat dfDvecX[ 27 ];
		STCON LbmFloat dfDvecY[ 27 ];
		STCON LbmFloat dfDvecZ[ 27 ];

		/*! principal directions */
		STCON int princDirX[ 2*3 ];
		STCON int princDirY[ 2*3 ];
		STCON int princDirZ[ 2*3 ];

		/*! vector lengths */
		STCON LbmFloat dfLength[ 19 ];

		/*! equilibrium distribution functions, precalculated = getCollideEq(i, 0,0,0,0) */
		static LbmFloat dfEquil[ 19 ];

		/*! arrays for les model coefficients */
		static LbmFloat lesCoeffDiag[ (3-1)*(3-1) ][ 27 ];
		static LbmFloat lesCoeffOffdiag[ 3 ][ 27 ];

}; // LbmData3D



/*****************************************************************************/
//! class for solver templating - 2D implementation 
//class LbmD2Q9 : public LbmSolverInterface {
class LbmD2Q9 {
	
	public:

		// constructor, init interface
		LbmD2Q9() {};
		// virtual destructor 
		virtual ~LbmD2Q9() {};
		//! id string of solver
		string getIdString() { return string("2D"); }

		//! how many dimensions?
		STCON int cDimension;

		//! Wi factors for collide step 
		STCON LbmFloat cCollenZero;
		STCON LbmFloat cCollenOne;
		STCON LbmFloat cCollenSqrtTwo;

		//! threshold value for filled/emptied cells 
		STCON LbmFloat cMagicNr2;
		STCON LbmFloat cMagicNr2Neg;
		STCON LbmFloat cMagicNr;
		STCON LbmFloat cMagicNrNeg;

		//! size of a single set of distribution functions 
		STCON int    cDfNum;
		STCON int    cDirNum;

		//! distribution functions directions 
		typedef enum {
			 cDirInv=  -1,
			 cDirC  =  0,
			 cDirN  =  1,
			 cDirS  =  2,
			 cDirE  =  3,
			 cDirW  =  4,
			 cDirNE =  5,
			 cDirNW =  6,
			 cDirSE =  7,
			 cDirSW =  8
		} dfDir;

		/* Vector Order 2D:
		 * 0  1 2  3  4  5  6 7  8
		 * 0, 0,0, 1,-1, 1,-1,1,-1 
		 * 0, 1,-1, 0,0, 1,1,-1,-1  */

		/* name of the dist. function 
			 only for nicer output */
		STCON char* dfString[ 9 ];

		/* index of normal dist func, not used so far?... */
		STCON int dfNorm[ 9 ];

		/* index of inverse dist func, not fast, but useful... */
		STCON int dfInv[ 9 ];

		/* index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefX[ 9 ];
		/* index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefY[ 9 ];
		/* index of x reflected dist func for free slip, not valid for all DFs... */
		STCON int dfRefZ[ 9 ];

		/* dist func vectors */
		STCON int dfVecX[ 9 ];
		STCON int dfVecY[ 9 ];
		/* Z, 2D values are all 0! */
		STCON int dfVecZ[ 9 ];

		/* arrays as before with doubles */
		STCON LbmFloat dfDvecX[ 9 ];
		STCON LbmFloat dfDvecY[ 9 ];
		/* Z, 2D values are all 0! */
		STCON LbmFloat dfDvecZ[ 9 ];

		/*! principal directions */
		STCON int princDirX[ 2*2 ];
		STCON int princDirY[ 2*2 ];
		STCON int princDirZ[ 2*2 ];

		/* vector lengths */
		STCON LbmFloat dfLength[ 9 ];

		/* equilibrium distribution functions, precalculated = getCollideEq(i, 0,0,0,0) */
		static LbmFloat dfEquil[ 9 ];

		/*! arrays for les model coefficients */
		static LbmFloat lesCoeffDiag[ (2-1)*(2-1) ][ 9 ];
		static LbmFloat lesCoeffOffdiag[ 2 ][ 9 ];

}; // LbmData3D



// not needed hereafter
#undef STCON



/*****************************************************************************/
//! class for solver templating - lbgk (srt) model implementation 
template<class DQ>
class LbmModelLBGK : public DQ , public LbmSolverInterface {
	public:

		/*! type for cells contents, needed for cell id interface */
		typedef DQ LbmCellContents;
		/*! type for cells */
		typedef LbmCellTemplate< LbmCellContents > LbmCell;

		// constructor
		LbmModelLBGK() : DQ(), LbmSolverInterface() {};
		// virtual destructor 
		virtual ~LbmModelLBGK() {};
		//! id string of solver
		string getIdString() { return DQ::getIdString() + string("lbgk]"); }

		/*! calculate length of velocity vector */
		static inline LbmFloat getVelVecLen(int l, LbmFloat ux,LbmFloat uy,LbmFloat uz) {
			return ((ux)*DQ::dfDvecX[l]+(uy)*DQ::dfDvecY[l]+(uz)*DQ::dfDvecZ[l]);
		};

		/*! calculate equilibrium DF for given values */
		static inline LbmFloat getCollideEq(int l, LbmFloat rho,  LbmFloat ux, LbmFloat uy, LbmFloat uz) {
			LbmFloat tmp = getVelVecLen(l,ux,uy,uz); 
			return( DQ::dfLength[l] *( 
						+ rho - (3.0/2.0*(ux*ux + uy*uy + uz*uz)) 
						+ 3.0 *tmp 
						+ 9.0/2.0 *(tmp*tmp) ) 
					);
		};


		// input mux etc. as acceleration
		// outputs rho,ux,uy,uz
		/*inline void collideArrays_org(LbmFloat df[19], 				
				LbmFloat &outrho, // out only!
				// velocity modifiers (returns actual velocity!)
				LbmFloat &mux, LbmFloat &muy, LbmFloat &muz, 
				LbmFloat omega
			) {
			LbmFloat rho=df[0]; 
			LbmFloat ux = mux;
			LbmFloat uy = muy;
			LbmFloat uz = muz;
			for(int l=1; l<DQ::cDfNum; l++) { 
				rho += df[l]; 
				ux  += (DQ::dfDvecX[l]*df[l]); 
				uy  += (DQ::dfDvecY[l]*df[l]);  
				uz  += (DQ::dfDvecZ[l]*df[l]);  
			}  
			for(int l=0; l<DQ::cDfNum; l++) { 
				//LbmFloat tmp = (ux*DQ::dfDvecX[l]+uy*DQ::dfDvecY[l]+uz*DQ::dfDvecZ[l]); 
				df[l] = (1.0-omega ) * df[l] + omega * ( getCollideEq(l,rho,ux,uy,uz) ); 
			}  

			mux = ux;
			muy = uy;
			muz = uz;
			outrho = rho;
		};*/
		
		// LES functions
		inline LbmFloat getLesNoneqTensorCoeff(
				LbmFloat df[], 				
				LbmFloat feq[] ) {
			LbmFloat Qo = 0.0;
			for(int m=0; m< ((DQ::cDimension*DQ::cDimension)-DQ::cDimension)/2 ; m++) { 
				LbmFloat qadd = 0.0;
				for(int l=1; l<DQ::cDfNum; l++) { 
					if(DQ::lesCoeffOffdiag[m][l]==0.0) continue;
					qadd += DQ::lesCoeffOffdiag[m][l]*(df[l]-feq[l]);
				}
				Qo += (qadd*qadd);
			}
			Qo *= 2.0; // off diag twice
			for(int m=0; m<DQ::cDimension; m++) { 
				LbmFloat qadd = 0.0;
				for(int l=1; l<DQ::cDfNum; l++) { 
					if(DQ::lesCoeffDiag[m][l]==0.0) continue;
					qadd += DQ::lesCoeffDiag[m][l]*(df[l]-feq[l]);
				}
				Qo += (qadd*qadd);
			}
			Qo = sqrt(Qo);
			return Qo;
		}
		inline LbmFloat getLesOmega(LbmFloat omega, LbmFloat csmago, LbmFloat Qo) {
			const LbmFloat tau = 1.0/omega;
			const LbmFloat nu = (2.0*tau-1.0) * (1.0/6.0);
			const LbmFloat C = csmago;
			const LbmFloat Csqr = C*C;
			LbmFloat S = -nu + sqrt( nu*nu + 18.0*Csqr*Qo ) / (6.0*Csqr);
			return( 1.0/( 3.0*( nu+Csqr*S ) +0.5 ) );
		}

		// "normal" collision
		inline void collideArrays(LbmFloat df[], 				
				LbmFloat &outrho, // out only!
				// velocity modifiers (returns actual velocity!)
				LbmFloat &mux, LbmFloat &muy, LbmFloat &muz, 
				LbmFloat omega, LbmFloat csmago, LbmFloat *newOmegaRet = NULL
			) {
			LbmFloat rho=df[0]; 
			LbmFloat ux = mux;
			LbmFloat uy = muy;
			LbmFloat uz = muz; 
			for(int l=1; l<DQ::cDfNum; l++) { 
				rho += df[l]; 
				ux  += (DQ::dfDvecX[l]*df[l]); 
				uy  += (DQ::dfDvecY[l]*df[l]);  
				uz  += (DQ::dfDvecZ[l]*df[l]);  
			}  
			LbmFloat feq[19];
			for(int l=0; l<DQ::cDfNum; l++) { 
				feq[l] = getCollideEq(l,rho,ux,uy,uz); 
			}

			LbmFloat omegaNew;
			if(csmago>0.0) {
				LbmFloat Qo = getLesNoneqTensorCoeff(df,feq);
				omegaNew = getLesOmega(omega,csmago,Qo);
			} else {
				omegaNew = omega; // smago off...
			}
			if(newOmegaRet) *newOmegaRet=omegaNew; // return value for stats

			for(int l=0; l<DQ::cDfNum; l++) { 
				df[l] = (1.0-omegaNew ) * df[l] + omegaNew * feq[l]; 
			}  

			mux = ux;
			muy = uy;
			muz = uz;
			outrho = rho;
		};

}; // LBGK

#ifdef LBMMODEL_DEFINED
// force compiler error!
ERROR - Dont include several LBM models at once...
#endif
#define LBMMODEL_DEFINED 1


typedef LbmModelLBGK<  LbmD2Q9 > LbmBGK2D;
typedef LbmModelLBGK< LbmD3Q19 > LbmBGK3D;


#define LBMHEADER_H
#endif




/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Lattice-Boltzmann defines...
 * 
 *****************************************************************************/
#ifndef TYPES_LBM_H

/* standard precision for LBM solver */
typedef double LBM_Float;
//typedef float LBM_Float;

typedef double LBM2D_Float;
//typedef float LBM2D_Float; // FLAGS might not work!!!!


/******************************************************************************
 * 2D 
 *****************************************************************************/


//! use incompressible LGBK model?
#define LBM2D_INCOMPBGK 1


/*! size of a single set of distribution functions */
#define LBM2D_DISTFUNCSIZE  9
/*! size of a single set for a cell (+cell flags, mass, bubble id) */
#define LBM2D_SETSIZE  12
/*! floats per LBM cell */
#define LBM2D_FLOATSPERCELL (LBM2D_SETSIZE +LBM2D_SETSIZE )


/*! sphere init full or empty */
#define LBM2D_FILLED true
#define LBM2D_EMPTY  false

/*! distribution functions directions */
#define WC   0
#define WN   1
#define WS   2
#define WE   3
#define WW   4
#define WNE  5
#define WNW  6
#define WSE  7
#define WSW  8
#define FLAG2D_BND    (9)
#define FLAG2D_MASS   (10)
#define FLAG2D_BUBBLE (11)

/* Wi factors for collide step */
#define LBM2D_COLLEN_ZERO   (4.0/9.0)
#define LBM2D_COLLEN_ONE    (1.0/9.0)
#define LBM2D_COLLEN_SQRTWO (1.0/36.0)


/* calculate equlibrium function for a single direction at cell i,j
 * pass 0 for the u_ terms that are not needed
 */
#define LBM2D_VELVEC(l, ux,uy) ((ux)*DF2DdvecX[l]+(uy)*DF2DdvecY[l])
#if LBM2D_INCOMPBGK!=1
#define LBM2D_COLLIDE_EQ(target, l,Rho, ux,uy)    \
  {\
		LBM2D_Float tmp = LBM2D_VELVEC(l,ux,uy); \
    target = ( (DF2Dlength[l]*Rho) *( \
		  					+ 1.0 - (3.0/2.0*(ux*ux + uy*uy)) \
							  + 3.0 *tmp \
							  + 9.0/2.0 *(tmp*tmp) ) \
	  );\
  }
#endif

/* incompressible LBGK model?? */
#if LBM2D_INCOMPBGK==1
#define LBM2D_COLLIDE_EQ(target, l,Rho, ux,uy)    \
  {\
		LBM2D_Float tmp = LBM2D_VELVEC(l,ux,uy); \
    target = ( (DF2Dlength[l]) *( \
		  					+ Rho - (3.0/2.0*(ux*ux + uy*uy )) \
							  + 3.0 *tmp \
							  + 9.0/2.0 *(tmp*tmp) ) \
	  );\
  }
#endif

/* calculate new distribution function for cell i,j
 * Now also includes gravity
 */
#define LBM2D_COLLIDE(l,omega, Rho, ux,uy ) \
  {\
		LBM2D_Float collideTempVar; \
		LBM2D_COLLIDE_EQ(collideTempVar, l,Rho, (ux), (uy) ); \
		m[l] = (1.0-omega) * m[l] + \
 					 omega* collideTempVar \
           ; \
  }\


#ifdef LBM2D_IMPORT
extern char *DF2Dstring[LBM2D_DISTFUNCSIZE];
extern int DF2Dnorm[LBM2D_DISTFUNCSIZE];
extern int DF2Dinv[LBM2D_DISTFUNCSIZE];
extern int DF2DrefX[LBM2D_DISTFUNCSIZE];
extern int DF2DrefY[LBM2D_DISTFUNCSIZE];
extern LBM2D_Float DF2Dequil[ LBM2D_DISTFUNCSIZE ];
extern int DF2DvecX[LBM2D_DISTFUNCSIZE];
extern int DF2DvecY[LBM2D_DISTFUNCSIZE];
extern LBM2D_Float DF2DdvecX[LBM2D_DISTFUNCSIZE];
extern LBM2D_Float DF2DdvecY[LBM2D_DISTFUNCSIZE];
extern LBM2D_Float DF2Dlength[LBM2D_DISTFUNCSIZE];
#endif



/******************************************************************************
 * 3D 
 *****************************************************************************/

// use incompressible LGBK model?
#define LBM_INCOMPBGK 1



/*! size of a single set of distribution functions */
#define LBM_DISTFUNCSIZE  19
/*! size of a single set for a cell (+cell flags, mass, bubble id) */
#define LBM_SETSIZE  22
/*! floats per LBM cell */
#define LBM_FLOATSPERCELL (LBM_SETSIZE +LBM_SETSIZE )


/*! distribution functions directions */
#define MC   0
#define MN   1
#define MS   2
#define ME   3
#define MW   4
#define MT   5
#define MB   6
#define MNE  7
#define MNW  8
#define MSE  9
#define MSW 10
#define MNT 11
#define MNB 12 
#define MST 13
#define MSB 14
#define MET 15 
#define MEB 16
#define MWT 17
#define MWB 18
#define FLAG_BND    (19)
#define FLAG_MASS   (20)
#define FLAG_BUBBLE (21)

/* Wi factors for collide step */
#define LBM_COLLEN_ZERO   (1.0/3.0)
#define LBM_COLLEN_ONE    (1.0/18.0)
#define LBM_COLLEN_SQRTWO (1.0/36.0)


/* calculate equlibrium function for a single direction at cell i,j,k
 * pass 0 for the u_ terms that are not needed
 */
#define LBM_VELVEC(l, ux,uy,uz) ((ux)*DFdvecX[l]+(uy)*DFdvecY[l]+(uz)*DFdvecZ[l])
#ifndef LBM_INCOMPBGK
#define LBM_COLLIDE_EQ(target, l,Rho, ux,uy,uz)    \
  {\
		LBM_Float tmp = LBM_VELVEC(l,ux,uy,uz); \
    target = ( (DFlength[l]*Rho) *( \
		  					+ 1.0 - (3.0/2.0*(ux*ux + uy*uy + uz*uz)) \
							  + 3.0 *tmp \
							  + 9.0/2.0 *(tmp*tmp) ) \
	  );\
  }
#endif

/* incompressible LBGK model?? */
#ifdef LBM_INCOMPBGK
#define LBM_COLLIDE_EQ(target, l,Rho, ux,uy,uz)    \
  {\
		LBM_Float tmp = LBM_VELVEC(l,ux,uy,uz); \
    target = ( (DFlength[l]) *( \
		  					+ Rho - (3.0/2.0*(ux*ux + uy*uy + uz*uz)) \
							  + 3.0 *tmp \
							  + 9.0/2.0 *(tmp*tmp) ) \
	  );\
  }
#endif

/* calculate new distribution function for cell i,j,k
 * Now also includes gravity
 */
#define LBM_COLLIDE(l,omega,Rho, ux,uy,uz ) \
  {\
		LBM_Float collideTempVar; \
		LBM_COLLIDE_EQ(collideTempVar, l,Rho, (ux), (uy), (uz) ); \
		m[l] = (1.0-omega) * m[l] + \
 					 omega* collideTempVar \
           ; \
  }\


#ifdef LBM3D_IMPORT
char *DFstring[LBM_DISTFUNCSIZE];
int DFnorm[LBM_DISTFUNCSIZE];
int DFinv[LBM_DISTFUNCSIZE];
int DFrefX[LBM_DISTFUNCSIZE];
int DFrefY[LBM_DISTFUNCSIZE];
int DFrefZ[LBM_DISTFUNCSIZE];
LBM_Float DFequil[ LBM_DISTFUNCSIZE ];
int DFvecX[LBM_DISTFUNCSIZE];
int DFvecY[LBM_DISTFUNCSIZE];
int DFvecZ[LBM_DISTFUNCSIZE];
LBM_Float DFdvecX[LBM_DISTFUNCSIZE];
LBM_Float DFdvecY[LBM_DISTFUNCSIZE];
LBM_Float DFdvecZ[LBM_DISTFUNCSIZE];
LBM_Float DFlength[LBM_DISTFUNCSIZE];
#endif


/******************************************************************************
 * BOTH
 *****************************************************************************/

/*! boundary flags 
 * only 1 should be active for a cell */
#define BND  (1<< 0)
#define ACCX (1<< 1)
#define ACCY (1<< 2)
#define ACCZ (1<< 3)
#define FREESLIP (1<< 4)
#define NOSLIP   (1<< 5)
#define PERIODIC (1<< 6)
#define PARTSLIP (1<< 7)
/*! surface type, also only 1 should be active (2. flag byte) */
#define EMPTY   (0)
#define FLUID   (1<< 8)
#define INTER   (1<< 9)
/*! neighbor flags (3. flag byte) */
#define I_NONBFLUID (1<<16)
#define I_NONBINTER (1<<17)
#define I_NONBEMPTY (1<<18)
#define I_NODELETE  (1<<19)
#define I_NEWCELL   (1<<20)
#define I_NEWINTERFACE (1<<21)
/*! marker only for debugging, this bit is reset each step */
#define I_CELLMARKER     ((int) (1<<30) )
#define I_NOTCELLMARKER  ((int) (~(1<<30)) )





#define TYPES_LBM_H
#endif


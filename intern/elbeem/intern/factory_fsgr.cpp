/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004,2005 Nils Thuerey
 *
 * Standard LBM Factory implementation
 *
 *****************************************************************************/

#include "factory_lbm.h"

// compiler sanity check
#ifndef LBMDIM
#if LBMDIM!=2
#if LBMDIM!=3
print("Error - LBMDIM has to be defined (2/3)!");
#endif
#endif
#endif

// disable sometimes to speed up compiling/2d tests
#define DISABLE 0

#include "lbmdimensions.h"
#include "lbmfsgrsolver.h"

//! lbm factory functions
LbmSolverInterface* createSolverLbmFsgr() {
#if DISABLE!=1
#if LBMDIM==2
	return new LbmFsgrSolver< LbmBGK2D >();
#endif // LBMDIM==2
#if LBMDIM==3
	return new LbmFsgrSolver< LbmBGK3D >();
#endif // LBMDIM==3
#endif // DISABLE
	return NULL;
}



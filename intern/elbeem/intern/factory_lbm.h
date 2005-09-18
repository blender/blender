/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * 2D/3D LBM Factory header
 *
 *****************************************************************************/

#include "lbminterface.h"

//! lbm factory functions
LbmSolverInterface* createSolverLbmFsgr();
#ifdef LBM_INCLUDE_TESTSOLVERS
LbmSolverInterface* createSolverOld();
#endif // LBM_INCLUDE_OLD



/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Global variables (unavoidable at times...)
 * all defines in main.cpp
 *
 *****************************************************************************/
 

/*****************************************************************************/
//! user interface variables

// global raytracer pointer (=world)
class ntlRaytracer;
extern ntlRaytracer *gpWorld;

// debug output switch
extern bool myDebugOut;

// global leave program variable
extern bool gQuit;

//! start simulation?
extern bool gThreadRunning;

//! short manual
extern char* usageString;


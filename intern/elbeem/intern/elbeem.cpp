/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * Main program functions
 *
 */

//#include "globals.h"


/*****************************************************************************/
// region of interest global vars
// currently used by e.g. fsgr solver
double guiRoiSX = 0.0;
double guiRoiSY = 0.0;
double guiRoiSZ = 0.0;
double guiRoiEX = 1.0;
double guiRoiEY = 1.0;
double guiRoiEZ = 1.0;
int guiRoiMaxLev=6, guiRoiMinLev=0;

//! global raytracer pointer (=world)
class ntlRaytracer;
ntlRaytracer *gpWorld = (ntlRaytracer*)0;

//! debug output switch
bool myDebugOut = false;

//! global leave program variable
bool gQuit = false;

//! start simulation?
bool gThreadRunning = false;

/* usage message */
char* usageString = 
	"El'Beem - Lattice Boltzmann Free Surface Simulator\n\
  Command-line Options: \n\
  -b : .obj file dump mode for Blender\n\
  -c : Force command line mode for rendering \n\
  -d : Dump mode for ECR\n\
  -f <filename> : Specify fluid description file to use as <filename>\n\
  -h : Display this message \n\
 	-o : single frame output to given file\n\
  \n ";



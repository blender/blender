/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2005 Nils Thuerey
 *
 * API header
 */
#ifndef ELBEEM_API_H
#define ELBEEM_API_H



/*! fluid geometry init types */
#define FGI_FLAGSTART   16
#define FGI_FLUID			  (1<<(FGI_FLAGSTART+ 0))
#define FGI_NO_FLUID	  (1<<(FGI_FLAGSTART+ 1))
#define FGI_BNDNO			  (1<<(FGI_FLAGSTART+ 2))
#define FGI_BNDFREE		  (1<<(FGI_FLAGSTART+ 3))
#define FGI_BNDPART		  (1<<(FGI_FLAGSTART+ 4))
#define FGI_NO_BND		  (1<<(FGI_FLAGSTART+ 5))
#define FGI_MBNDINFLOW	(1<<(FGI_FLAGSTART+ 6))
#define FGI_MBNDOUTFLOW	(1<<(FGI_FLAGSTART+ 7))

#define FGI_ALLBOUNDS ( FGI_BNDNO | FGI_BNDFREE | FGI_BNDPART | FGI_MBNDINFLOW | FGI_MBNDOUTFLOW )


/*! blender types for mesh->type */
//#define OB_FLUIDSIM_DOMAIN      2
#define OB_FLUIDSIM_FLUID       4
#define OB_FLUIDSIM_OBSTACLE    8
#define OB_FLUIDSIM_INFLOW      16
#define OB_FLUIDSIM_OUTFLOW     32


// global settings for the simulation
typedef struct elbeemSimulationSettings {
  /* version number */
  short version;

	/* geometrical extent */
	float geoStart[3], geoSize[3];

  /* resolutions */
  short resolutionxyz;
  short previewresxyz;
  /* size of the domain in real units (meters along largest resolution x,y,z extent) */
  float realsize;

  /* fluid properties */
  double viscosity;
  /* gravity strength */
  float gravx,gravy,gravz;
  /* anim start end time */
  float animStart, aniFrameTime;
  /* g star param (LBM compressibility) */
  float gstar;
  /* activate refinement? */
  short maxRefine;

  /* store output path, and file prefix for baked fluid surface */
  char surfdataPath[160+80];
} elbeemSimulationSettings;


// a single mesh object
typedef struct elbeemMesh {
  /* obstacle,fluid or inflow... */
  short type;

  /* initial velocity (for fluid/inflow) */
  float iniVelx,iniVely,iniVelz;

	/* vertices */
  int numVertices;
	float *vertices; // = float[][3];

	/* triangles */
	int   numTriangles;
  int   *triangles; // = int[][3];
} elbeemMesh;

// API functions

// start fluidsim init
int elbeemInit(struct elbeemSimulationSettings*);

// add mesh as fluidsim object
int elbeemAddMesh(struct elbeemMesh*);

// do the actual simulation
int elbeemSimulate(void);



#endif // ELBEEM_API_H

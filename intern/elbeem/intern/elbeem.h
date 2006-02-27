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


/*! blender types for mesh->type */
//#define OB_FLUIDSIM_DOMAIN      2
#define OB_FLUIDSIM_FLUID       4
#define OB_FLUIDSIM_OBSTACLE    8
#define OB_FLUIDSIM_INFLOW      16
#define OB_FLUIDSIM_OUTFLOW     32

#define FLUIDSIM_OBSTACLE_NOSLIP     1
#define FLUIDSIM_OBSTACLE_PARTSLIP   2
#define FLUIDSIM_OBSTACLE_FREESLIP   3


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
  float gravity[3];
  /* anim start end time */
  float animStart, aniFrameTime;
	/* no. of frames to simulate & output */
	short noOfFrames;
  /* g star param (LBM compressibility) */
  float gstar;
  /* activate refinement? */
  short maxRefine;
  /* amount of particles to generate (0=off) */
  float generateParticles;

  /* store output path, and file prefix for baked fluid surface */
  char outputPath[160+80];

	/* channel for frame time, visc & gravity animations */
	int channelSizeFrameTime;
	float *channelFrameTime;
	int channelSizeViscosity;
	float *channelViscosity;
	int channelSizeGravity;
	float *channelGravity;  // vector

	/* boundary types and settings for domain walls */
	short obstacleType;
	float obstaclePartslip;
	/* generate speed vectors for vertices (e.g. for image based motion blur)*/
	short generateVertexVectors;
	/* strength of surface smoothing */
	float surfaceSmoothing;

	/* global transformation to apply to fluidsim mesh */
	float surfaceTrafo[4*4];
} elbeemSimulationSettings;


// a single mesh object
typedef struct elbeemMesh {
  /* obstacle,fluid or inflow... */
  short type;

	/* vertices */
  int numVertices;
	float *vertices; // = float[][3];

	/* triangles */
	int   numTriangles;
  int   *triangles; // = int[][3];

	/* animation channels */
	int channelSizeTranslation;
	float *channelTranslation;
	int channelSizeRotation;
	float *channelRotation;
	int channelSizeScale;
	float *channelScale;
	
	/* active channel */
	int channelSizeActive;
	float *channelActive;
	/* initial velocity channel (e.g. for inflow) */
	int channelSizeInitialVel;
	float *channelInitialVel; // vector
	/* use initial velocity in object coordinates? (e.g. for rotation) */
	short localInivelCoords;
	/* boundary types and settings */
	short obstacleType;
	float obstaclePartslip;

	/* name of the mesh, mostly for debugging */
	char *name;
} elbeemMesh;

// API functions

#ifdef __cplusplus
extern "C"  {
#endif // __cplusplus
 
// reset elbeemSimulationSettings struct with defaults
void elbeemResetSettings(struct elbeemSimulationSettings*);
 
// start fluidsim init
int elbeemInit(struct elbeemSimulationSettings*);

// reset elbeemMesh struct with zeroes
void elbeemResetMesh(struct elbeemMesh*);

// add mesh as fluidsim object
int elbeemAddMesh(struct elbeemMesh*);

// do the actual simulation
int elbeemSimulate(void);


// helper function - simplify animation channels
// returns if the channel and its size changed
int elbeemSimplifyChannelFloat(float *channel, int *size);
int elbeemSimplifyChannelVec3(float *channel, int *size);

#ifdef __cplusplus
}
#endif // __cplusplus

/******************************************************************************/
// internal defines, do not use for setting up simulation

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


#endif // ELBEEM_API_H

/** \file elbeem/extern/elbeem.h
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * All code distributed as part of El'Beem is covered by the version 2 of the 
 * GNU General Public License. See the file COPYING for details.
 * Copyright 2003-2006 Nils Thuerey
 *
 * API header
 */
#ifndef ELBEEM_API_H
#define ELBEEM_API_H


// simulation run callback function type (elbeemSimulationSettings->runsimCallback)
// best use with FLUIDSIM_CBxxx defines below.
// >parameters
// return values: 0=continue, 1=stop, 2=abort
// data pointer: user data pointer from elbeemSimulationSettings->runsimUserData
// status integer: 1=running simulation, 2=new frame saved
// frame integer: if status is 1, contains current frame number
typedef int (*elbeemRunSimulationCallback)(void *data, int status, int frame);
#define FLUIDSIM_CBRET_CONTINUE    0
#define FLUIDSIM_CBRET_STOP        1
#define FLUIDSIM_CBRET_ABORT       2 
#define FLUIDSIM_CBSTATUS_STEP     1 
#define FLUIDSIM_CBSTATUS_NEWFRAME 2 


// global settings for the simulation
typedef struct elbeemSimulationSettings {
  /* version number */
  short version;
	/* id number of simulation domain, needed if more than a
	 * single domain should be simulated */
	short domainId; // unused within blender

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
  /* probability for surface particle generation (0.0=off) */
  float generateParticles;
  /* amount of tracer particles to generate (0=off) */
  int numTracerParticles;

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
	short domainobsType;
	float domainobsPartslip;

	/* what surfaces to generate */
	int mFsSurfGenSetting;

	/* generate speed vectors for vertices (e.g. for image based motion blur)*/
	short generateVertexVectors;
	/* strength of surface smoothing */
	float surfaceSmoothing;
	/* no. of surface subdivisions */
	int   surfaceSubdivs;

	/* global transformation to apply to fluidsim mesh */
	float surfaceTrafo[4*4];

	/* development variables, testing for upcoming releases...*/
	float farFieldSize;

	/* callback function to notify calling program of performed simulation steps
	 * or newly available frame data, if NULL it is ignored */
	elbeemRunSimulationCallback runsimCallback;
	/* pointer passed to runsimCallback for user data storage */
	void* runsimUserData;
	/* simulation threads used by omp */
	int threads;

} elbeemSimulationSettings;


// defines for elbeemMesh->type below
/* please keep in sync with DNA_object_fluidsim.h */
#define OB_FLUIDSIM_FLUID       4
#define OB_FLUIDSIM_OBSTACLE    8
#define OB_FLUIDSIM_INFLOW      16
#define OB_FLUIDSIM_OUTFLOW     32
#define OB_FLUIDSIM_PARTICLE    64
#define OB_FLUIDSIM_CONTROL 	128

// defines for elbeemMesh->obstacleType below (low bits) high bits (>=64) are reserved for mFsSurfGenSetting flags which are defined in solver_class.h
#define FLUIDSIM_OBSTACLE_NOSLIP     1
#define FLUIDSIM_OBSTACLE_PARTSLIP   2
#define FLUIDSIM_OBSTACLE_FREESLIP   3
#define FLUIDSIM_FSSG_NOOBS			 64


#define OB_VOLUMEINIT_VOLUME 1
#define OB_VOLUMEINIT_SHELL  2
#define OB_VOLUMEINIT_BOTH   (OB_VOLUMEINIT_SHELL|OB_VOLUMEINIT_VOLUME)

// a single mesh object
typedef struct elbeemMesh {
  /* obstacle,fluid or inflow or control ... */
  short type;
	/* id of simulation domain it belongs to */
	short parentDomainId;

	/* vertices */
  int numVertices;
	float *vertices; // = float[n][3];
	/* animated vertices */
  int channelSizeVertices;
	float *channelVertices; // = float[channelSizeVertices* (n*3+1) ];

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
	/* amount of force transfer from fluid to obj, 0=off, 1=normal */
	float obstacleImpactFactor;
	/* init volume, shell or both? use OB_VOLUMEINIT_xxx defines above */
	short volumeInitType;

	/* name of the mesh, mostly for debugging */
	const char *name;
	
	/* fluid control settings */
	float cpsTimeStart;
	float cpsTimeEnd;
	float cpsQuality;
	
	int channelSizeAttractforceStrength;
	float *channelAttractforceStrength;
	int channelSizeAttractforceRadius;
	float *channelAttractforceRadius;
	int channelSizeVelocityforceStrength;
	float *channelVelocityforceStrength;
	int channelSizeVelocityforceRadius;
	float *channelVelocityforceRadius;
} elbeemMesh;

// API functions

#ifdef __cplusplus
extern "C"  {
#endif // __cplusplus
 

// reset elbeemSimulationSettings struct with defaults
void elbeemResetSettings(struct elbeemSimulationSettings*);
 
// start fluidsim init (returns !=0 upon failure)
int elbeemInit(void);

// frees fluidsim
int elbeemFree(void);

// start fluidsim init (returns !=0 upon failure)
int elbeemAddDomain(struct elbeemSimulationSettings*);

// get failure message during simulation or init
// if an error occured (the string is copied into buffer,
// max. length = 256 chars )
void elbeemGetErrorString(char *buffer);

// reset elbeemMesh struct with zeroes
void elbeemResetMesh(struct elbeemMesh*);

// add mesh as fluidsim object
int elbeemAddMesh(struct elbeemMesh*);

// do the actual simulation
int elbeemSimulate(void);

// continue a previously stopped simulation
int elbeemContinueSimulation(void);


// helper functions 

// simplify animation channels
// returns if the channel and its size changed
int elbeemSimplifyChannelFloat(float *channel, int *size);
int elbeemSimplifyChannelVec3(float *channel, int *size);

// helper functions implemented in utilities.cpp

/* set elbeem debug output level (0=off to 10=full on) */
void elbeemSetDebugLevel(int level);
/* elbeem debug output function, prints if debug level >0 */
void elbeemDebugOut(char *msg);

/* estimate how much memory a given setup will require */
double elbeemEstimateMemreq(int res,
    float sx, float sy, float sz,
    int refine, char *retstr);



#ifdef __cplusplus
}
#endif // __cplusplus



/******************************************************************************/
// internal defines, do not use for initializing elbeemMesh
// structs, for these use OB_xxx defines above

/*! fluid geometry init types */
// type "int" used, so max is 8
#define FGI_FLAGSTART   16
#define FGI_FLUID			  (1<<(FGI_FLAGSTART+ 0))
#define FGI_NO_FLUID	  (1<<(FGI_FLAGSTART+ 1))
#define FGI_BNDNO			  (1<<(FGI_FLAGSTART+ 2))
#define FGI_BNDFREE		  (1<<(FGI_FLAGSTART+ 3))
#define FGI_BNDPART		  (1<<(FGI_FLAGSTART+ 4))
#define FGI_NO_BND		  (1<<(FGI_FLAGSTART+ 5))
#define FGI_MBNDINFLOW	(1<<(FGI_FLAGSTART+ 6))
#define FGI_MBNDOUTFLOW	(1<<(FGI_FLAGSTART+ 7))
#define FGI_CONTROL	(1<<(FGI_FLAGSTART+ 8))

// all boundary types at once
#define FGI_ALLBOUNDS ( FGI_BNDNO | FGI_BNDFREE | FGI_BNDPART | FGI_MBNDINFLOW | FGI_MBNDOUTFLOW )


#endif // ELBEEM_API_H

// --------------------------------------------------------------------------
//
// El'Beem - the visual lattice boltzmann freesurface simulator
// All code distributed as part of El'Beem is covered by the version 2 of the 
// GNU General Public License. See the file COPYING for details.  
//
// Copyright 2008 Nils Thuerey , Richard Keiser, Mark Pauly, Ulrich Ruede
//
// control particle classes
//
// --------------------------------------------------------------------------

#ifndef CONTROLPARTICLES_H
#define CONTROLPARTICLES_H

#include "ntl_geometrymodel.h"

// indicator for LBM inclusion
//#ifndef LBMDIM

//#include <NxFoundation.h>
//#include <vector>
//class MultisphGUI;
//#define NORMALIZE(a) a.normalize()
//#define MAGNITUDE(a) a.magnitude()
//#define CROSS(a,b,c) a.cross(b,c)
//#define ABS(a) (a>0. ? (a) : -(a))
//#include "cpdefines.h"

//#else // LBMDIM

// use compatibility defines
//#define NORMALIZE(a) normalize(a)
//#define MAGNITUDE(a) norm(a)
//#define CROSS(a,b,c) a=cross(b,c)

//#endif // LBMDIM

#define MAGNITUDE(a) norm(a)

// math.h compatibility
#define CP_PI ((LbmFloat)3.14159265358979323846)

// project 2d test cases onto plane?
// if not, 3d distance is used for 2d sim as well
#define CP_PROJECT2D 1


// default init for mincpdist, ControlForces::maxDistance
#define CPF_MAXDINIT 10000.

// storage of influence for a fluid cell/particle in lbm/sph
class ControlForces
{
public:
	ControlForces() { };
	~ControlForces() {};

	// attraction force
	LbmFloat weightAtt;
	LbmVec forceAtt;
	// velocity influence
	LbmFloat weightVel;
	LbmVec forceVel;
	// maximal distance influence, 
	// first is max. distance to first control particle
	// second attraction strength
	LbmFloat maxDistance;
	LbmVec forceMaxd;

	LbmFloat compAvWeight;
	LbmVec compAv;

	void resetForces() {
		weightAtt = weightVel = 0.;
		maxDistance = CPF_MAXDINIT;
		forceAtt = forceVel = forceMaxd = LbmVec(0.,0.,0.);
		compAvWeight=0.; compAv=LbmVec(0.);
	};
};


// single control particle
class ControlParticle
{
public:
	ControlParticle() { reset(); };
	~ControlParticle() {};

	// control parameters
	
	// position
	LbmVec pos;
	// size (influences influence radius)
	LbmFloat size;
	// overall strength of influence
	LbmFloat influence;
	// rotation axis
	LbmVec rotaxis;

	// computed values

	// velocity
	LbmVec vel;
	// computed density
	LbmFloat density;
	LbmFloat densityWeight;

	LbmVec avgVel;
	LbmVec avgVelAcc;
	LbmFloat avgVelWeight;

	// init all zero / defaults
	void reset();
};


// container for a particle configuration at time t
class ControlParticleSet
{
public:

	// time of particle set
	LbmFloat time;
	// particle positions
	std::vector<ControlParticle> particles;

};


// container & management of control particles
class ControlParticles
{
public:
	ControlParticles();
	~ControlParticles();

	// reset datastructures for next influence step
	// if motion object is given, particle 1 of second system is used for overall 
	// position and speed offset
	void prepareControl(LbmFloat simtime, LbmFloat dt, ControlParticles *motion);
	// post control operations
	void finishControl(std::vector<ControlForces> &forces, LbmFloat iatt, LbmFloat ivel, LbmFloat imaxd);
	// recalculate 
	void calculateKernelWeight();

	// calculate forces at given position, and modify velocity
	// according to timestep (from initControl)
	void calculateCpInfluenceOpt (ControlParticle *cp, LbmVec fluidpos, LbmVec fluidvel, ControlForces *force, LbmFloat fillFactor);
	void calculateMaxdForce      (ControlParticle *cp, LbmVec fluidpos, ControlForces *force);

	// no. of particles
	inline int getSize() { return (int)_particles.size(); }
	int getTotalSize();
	// get particle [i]
	inline ControlParticle* getParticle(int i){ return &_particles[i]; }

	// set influence parameters
	void setInfluenceTangential(LbmFloat set) { _influenceTangential=set; }
	void setInfluenceAttraction(LbmFloat set) { _influenceAttraction=set; }
	void setInfluenceMaxdist(LbmFloat set)    { _influenceMaxdist=set; }
	// calculate for delta t
	void setInfluenceVelocity(LbmFloat set, LbmFloat dt);
	// get influence parameters
	inline LbmFloat getInfluenceAttraction()    { return _influenceAttraction; }
	inline LbmFloat getInfluenceTangential()    { return _influenceTangential; }
	inline LbmFloat getInfluenceVelocity()      { return _influenceVelocity; }
	inline LbmFloat getInfluenceMaxdist()       { return _influenceMaxdist; }
	inline LbmFloat getCurrTimestep()           { return _currTimestep; }

	void setRadiusAtt(LbmFloat set)       { _radiusAtt=set; }
	inline LbmFloat getRadiusAtt()        { return _radiusAtt; }
	void setRadiusVel(LbmFloat set)       { _radiusVel=set; }
	inline LbmFloat getRadiusVel()        { return _radiusVel; }
	void setRadiusMaxd(LbmFloat set)      { _radiusMaxd=set; }
	inline LbmFloat getRadiusMaxd()       { return _radiusMaxd; }
	void setRadiusMinMaxd(LbmFloat set)   { _radiusMinMaxd=set; }
	inline LbmFloat getRadiusMinMaxd()    { return _radiusMinMaxd; }

	LbmFloat getControlTimStart();
	LbmFloat getControlTimEnd();

	// set/get characteristic length (and inverse)
	void setCharLength(LbmFloat set)      { _charLength=set; _charLengthInv=1./_charLength; }
	inline LbmFloat getCharLength()       { return _charLength;}
	inline LbmFloat getCharLengthInv()    { return _charLengthInv;}

	// set init parameters
	void setInitTimeScale(LbmFloat set)  { _initTimeScale = set; };
	void setInitMirror(string set)  { _initMirror = set; };
	string getInitMirror()          { return _initMirror; };

	void setLastOffset(LbmVec set) { _initLastPartOffset = set; };
	void setLastScale(LbmVec set)  { _initLastPartScale = set; };
	void setOffset(LbmVec set) { _initPartOffset = set; };
	void setScale(LbmVec set)  { _initPartScale = set; };

	// set/get cps params
	void setCPSWith(LbmFloat set)       { mCPSWidth = set; };
	void setCPSTimestep(LbmFloat set)   { mCPSTimestep = set; };
	void setCPSTimeStart(LbmFloat set)  { mCPSTimeStart = set; };
	void setCPSTimeEnd(LbmFloat set)    { mCPSTimeEnd = set; };
	void setCPSMvmWeightFac(LbmFloat set) { mCPSWeightFac = set; };

	LbmFloat getCPSWith()       { return mCPSWidth; };
	LbmFloat getCPSTimestep()   { return mCPSTimestep; };
	LbmFloat getCPSTimeStart()  { return mCPSTimeStart; };
	LbmFloat getCPSTimeEnd()    { return mCPSTimeEnd; };
	LbmFloat getCPSMvmWeightFac() { return mCPSWeightFac; };

	void setDebugInit(int set)       { mDebugInit = set; };

	// set init parameters
	void setFluidSpacing(LbmFloat set)  { _fluidSpacing = set; };

	// load positions & timing from text file
	int initFromTextFile(string filename);
	int initFromTextFileOld(string filename);
	// load positions & timing from gzipped binary file
	int initFromBinaryFile(string filename);
	int initFromMVCMesh(string filename);
	// init an example test case
	int initExampleSet();

	// init for a given time
	void initTime(LbmFloat t, LbmFloat dt);

	// blender test init
	void initBlenderTest();
	
	int initFromObject(ntlGeometryObjModel *model);

protected:
	// sets influence params
	friend class MultisphGUI;

	// tangential and attraction influence
	LbmFloat _influenceTangential, _influenceAttraction;
	// direct velocity influence
	LbmFloat _influenceVelocity;
	// maximal distance influence
	LbmFloat _influenceMaxdist;

	// influence radii
	LbmFloat _radiusAtt, _radiusVel, _radiusMinMaxd, _radiusMaxd;

	// currently valid time & timestep
	LbmFloat _currTime, _currTimestep;
	// all particles
	std::vector<ControlParticle> _particles;

	// particle sets
	std::vector<ControlParticleSet> mPartSets;

	// additional parameters for initing particles
	LbmFloat _initTimeScale;
	LbmVec _initPartOffset;
	LbmVec _initPartScale;
	LbmVec _initLastPartOffset;
	LbmVec _initLastPartScale;
	// mirror particles for loading?
	string _initMirror;

	// row spacing paramter, e.g. use for approximation of kernel area/volume
	LbmFloat _fluidSpacing;
	// save current kernel weight
	LbmFloat _kernelWeight;
	// charateristic length in world coordinates for normalizatioon of forces
	LbmFloat _charLength, _charLengthInv;


	/*! do ani mesh CPS */
	void calculateCPS(string filename);
	//! ani mesh cps params 
	ntlVec3Gfx mvCPSStart, mvCPSEnd;
	gfxReal mCPSWidth, mCPSTimestep;
	gfxReal mCPSTimeStart, mCPSTimeEnd;
	gfxReal mCPSWeightFac;

	int mDebugInit;

	
protected:
	// apply init transformations
	void applyTrafos();

	// helper function for init -> swap components everywhere
	void swapCoords(int a,int b);
	// helper function for init -> mirror time
	void mirrorTime();

	// helper, init given array
	void initTimeArray(LbmFloat t, std::vector<ControlParticle> &parts);

	bool checkPointInside(ntlTree *tree, ntlVec3Gfx org, gfxReal &distance);
};



#endif


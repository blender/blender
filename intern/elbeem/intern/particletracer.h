/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Particle Viewer/Tracer
 *
 *****************************************************************************/
#ifndef NTL_PARTICLETRACER_H

#include "ntl_geometryobject.h"
template<class Scalar> class ntlMatrix4x4;

// particle types
#define PART_BUBBLE (1<< 1)
#define PART_DROP   (1<< 2)
#define PART_INTER  (1<< 3)
#define PART_FLOAT  (1<< 4)

// particle state
#define PART_IN     (1<< 8)
#define PART_OUT    (1<< 9)
#define PART_INACTIVE (1<<10)

//! A single particle
class ParticleObject
{
	public:
  	//! Standard constructor
  	inline ParticleObject(ntlVec3Gfx mp) :
			mPos(mp),mVel(0.0), mSize(1.0), mStatus(0),mLifeTime(0) { };
  	//! Copy constructor
  	inline ParticleObject(const ParticleObject &a) :
			mPos(a.mPos), mVel(a.mVel), mSize(a.mSize), 
			mStatus(a.mStatus),
			mLifeTime(a.mLifeTime) { };
  	//! Destructor
  	inline ~ParticleObject() { /* empty */ };

		//! add vector to position
		inline void advance(float vx, float vy, float vz) {
			mPos[0] += vx; mPos[1] += vy; mPos[2] += vz; }
		//! advance with own velocity
		inline void advanceVel() { mPos += mVel; }
		//! add acceleration to velocity
		inline void addToVel(ntlVec3Gfx acc) { mVel += acc; }

		//! get/set vector to position
		inline ntlVec3Gfx getPos() { return mPos; }
		inline void setPos(ntlVec3Gfx set) { mPos=set; }
		//! set velocity
		inline void setVel(ntlVec3Gfx set) { mVel = set; }
		//! set velocity
		inline void setVel(gfxReal x, gfxReal y, gfxReal z) { mVel = ntlVec3Gfx(x,y,z); }
		//! get velocity
		inline ntlVec3Gfx getVel() { return mVel; }

		//! get/set size value
		inline gfxReal getSize() { return mSize; }
		inline void setSize(gfxReal set) { mSize=set; }

		//! get whole flags
		inline int getFlags() const { return mStatus; }
		//! get status (higher byte)
		inline int getStatus() const { return (mStatus&0xFF00); }
		//! set status  (higher byte)
		inline void setStatus(int set) { mStatus = set|(mStatus&0x00FF); }
		//! get type (lower byte)
		inline int getType() const { return (mStatus&0x00FF); }
		//! set type (lower byte)
		inline void setType(int set) { mStatus = set|(mStatus&0xFF00); }
		//! get active flag
		inline bool getActive() const { return ((mStatus&PART_INACTIVE)==0); }
		//! set active flag
		inline void setActive(bool set) { 
			if(set) mStatus &= (~PART_INACTIVE);	
			else mStatus |= PART_INACTIVE;
		}
		//! get/set lifetime
		inline int getLifeTime() const { return mLifeTime; }
		//! set type (lower byte)
		inline void setLifeTime(int set) { mLifeTime = set; }
		
	protected:

		/*! the particle position */
		ntlVec3Gfx mPos;
		/*! the particle velocity */
		ntlVec3Gfx mVel;
		/*! size / mass of particle */
		gfxReal mSize;
		/*! particle status */
		int mStatus;
		/*! count survived time steps */
		int mLifeTime;
};


//! A whole particle array
class ParticleTracer :
	public ntlGeometryObject
{
	public:
  	//! Standard constructor
  	ParticleTracer();
  	//! Destructor
  	~ParticleTracer();

		//! add a particle at this position
		void addParticle(float x, float y, float z);

		//! save particle positions before adding a new timestep
		void savePreviousPositions();

		//! draw the particle array
		void draw();
		
		//! parse settings from attributes (dont use own list!)
		void parseAttrList( AttributeList *att );

		//! adapt time step by rescaling velocities
		void adaptPartTimestep(float factor);

		// access funcs
		
		//! get the number of particles
		inline int  getNumParticles() 				{ return mParts.size(); }

		//! iterate over all newest particles (for advancing positions)
		inline vector<ParticleObject>::iterator getParticlesBegin() { return mParts.begin(); }
		//! end iterator for newest particles
		inline vector<ParticleObject>::iterator getParticlesEnd() { return mParts.end(); }
		//! end iterator for newest particles
		inline ParticleObject* getLast() { return &(mParts[ mParts.size()-1 ]); }
		
		/*! set geometry start (for renderer) */
		inline void setStart(ntlVec3Gfx set) { mStart = set; initTrafoMatrix(); }
		/*! set geometry end (for renderer) */
		inline void setEnd(ntlVec3Gfx set) { mEnd = set; initTrafoMatrix(); }
		/*! get values */
		inline ntlVec3Gfx getStart() { return mStart; }
		/*! set geometry end (for renderer) */
		inline ntlVec3Gfx getEnd() { return mEnd; }
		
		/*! set simulation domain start */
		inline void setSimStart(ntlVec3Gfx set) { mSimStart = set; initTrafoMatrix(); }
		/*! set simulation domain end */
		inline void setSimEnd(ntlVec3Gfx set) { mSimEnd = set; initTrafoMatrix(); }
		
		/*! set/get dump flag */
		inline void setDumpParts(bool set) { mDumpParts = set; }
		inline bool getDumpParts() { return mDumpParts; }
		
		//! set the particle scaling factor
		inline void setPartScale(float set) { mPartScale = set; }


		// NTL geometry implementation
		/*! Get the triangles from this object */
		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId );

		virtual void notifyOfDump(int frameNr,char *frameNrStr,string outfilename);
		// free deleted particles
		void cleanup();

	protected:

		/*! the particle array (for multiple timesteps) */
		vector<ParticleObject> mParts;

		/*! size of the particles to display */
		float mPartSize;

		/*! start and end vectors for the triangulation region to create particles in */
		ntlVec3Gfx mStart, mEnd;

		/*! start and end vectors of the simulation domain */
		ntlVec3Gfx mSimStart, mSimEnd;

		/*! scaling param for particles */
		float mPartScale;
		/*! head and tail distance for particle shapes */
		float mPartHeadDist, mPartTailDist;
		/*! no of segments for particle cone */
		int mPartSegments;
		/*! use length/absval of values to scale particles? */
		int mValueScale;
		/*! value length maximal cutoff value, for mValueScale==2 */
		float mValueCutoffTop;
		/*! value length minimal cutoff value, for mValueScale==2 */
		float mValueCutoffBottom;

		/*! dump particles (or certain types of) to disk? */
		int mDumpParts;
		/*! show only a certain type (debugging) */
		int mShowOnly;

		//! transform matrix
		ntlMatrix4x4<gfxReal> *mpTrafo;

		/*! init sim/pos transformation */
		void initTrafoMatrix();
};

#define NTL_PARTICLETRACER_H
#endif


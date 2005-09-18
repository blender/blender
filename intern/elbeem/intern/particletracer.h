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


//! A single particle
class ParticleObject
{
	public:
  	//! Standard constructor
  	inline ParticleObject(ntlVec3Gfx mp) :
			mPos(mp), mActive( true ) { };
  	//! Copy constructor
  	inline ParticleObject(const ParticleObject &a) :
			mPos(a.mPos), mActive(a.mActive) { };
  	//! Destructor
  	inline ~ParticleObject() { /* empty */ };

		//! add vector to position
		inline void advance(double vx, double vy, double vz) {
			mPos[0] += vx; mPos[1] += vy; mPos[2] += vz; }

		//! add vector to position
		inline ntlVec3Gfx getPos() { return mPos; }

		//! get active flag
		inline bool getActive() { return mActive; }
		//! set active flag
		inline void setActive(bool set) { mActive = set; }
		
	protected:

		/*! the particle position */
		ntlVec3Gfx mPos;

		/*! particle active? */
		bool mActive;
};


//! A whole particle array
class ParticleTracer :
	public ntlGeometryObject
{
	public:
  	//! Standard constructor
  	ParticleTracer();
  	//! Destructor
  	~ParticleTracer() { /* empty */ };

		//! add a particle at this position
		void addParticle(double x, double y, double z);

		//! save particle positions before adding a new timestep
		void savePreviousPositions();

		//! draw the particle array
		void draw();
		
		//! parse settings from attributes (dont use own list!)
		void parseAttrList( AttributeList *att );


		// access funcs
		
		//! set the number of timesteps to trace
		void setTimesteps(int steps);

		//! set the number of particles
		inline void setNumParticles(int set) { mNumParticles = set; }
		//! get the number of particles
		inline int  getNumParticles() 				{ return mNumParticles; }

		//! set the number of timesteps to trace
		inline void setTrailLength(int set) { mTrailLength = set; mParts.resize(mTrailLength*mTrailInterval); }
		//! get the number of timesteps to trace
		inline int  getTrailLength()				{ return mTrailLength; }
		//! set the number of timesteps between each anim step saving
		inline void setTrailInterval(int set) { mTrailInterval = set; mParts.resize(mTrailLength*mTrailInterval); }

		//! get the no. of particles in the current array
		inline int getPartArraySize() { return mParts[0].size(); }

		//! iterate over all newest particles (for advancing positions)
		inline vector<ParticleObject>::iterator getParticlesBegin() { return mParts[0].begin(); }
		//! end iterator for newest particles
		inline vector<ParticleObject>::iterator getParticlesEnd() { return mParts[0].end(); }
		
		/*! set geometry start (for renderer) */
		inline void setStart(ntlVec3Gfx set) { mStart = set; }
		/*! set geometry end (for renderer) */
		inline void setEnd(ntlVec3Gfx set) { mEnd = set; }
		
		/*! set simulation domain start */
		inline void setSimStart(ntlVec3Gfx set) { mSimStart = set; }
		/*! set simulation domain end */
		inline void setSimEnd(ntlVec3Gfx set) { mSimEnd = set; }
		
		//! set the particle scaling factor
		inline void setPartScale(double set) { mPartScale = set; }
		//! set the trail scaling factor
		inline void setTrailScale(double set) { mTrailScale = set; }


		// NTL geometry implementation

		/*! Get the triangles from this object */
		virtual void getTriangles( vector<ntlTriangle> *triangles, 
				vector<ntlVec3Gfx> *vertices, 
				vector<ntlVec3Gfx> *normals, int objectId );


	protected:

		/*! the particle array (for multiple timesteps) */
		vector< vector<ParticleObject> > mParts;

		/*! desired number of particles */
		int mNumParticles;

		/*! number of particle positions to trace */
		int mTrailLength;

		/*! number of timesteps to between saving particle positions */
		int mTrailInterval;
		int mTrailIntervalCounter;

		/*! size of the particles to display */
		double mPartSize;

		/*! size of the particle trail */
		double mTrailScale;

		/*! start and end vectors for the triangulation region to create particles in */
		ntlVec3Gfx mStart, mEnd;

		/*! start and end vectors of the simulation domain */
		ntlVec3Gfx mSimStart, mSimEnd;

		/*! scaling param for particles */
		double mPartScale;
		/*! head and tail distance for particle shapes */
		double mPartHeadDist, mPartTailDist;
		/*! no of segments for particle cone */
		int mPartSegments;
		/*! use length/absval of values to scale particles? */
		int mValueScale;
		/*! value length maximal cutoff value, for mValueScale==2 */
		double mValueCutoffTop;
		/*! value length minimal cutoff value, for mValueScale==2 */
		double mValueCutoffBottom;

};

#define NTL_PARTICLETRACER_H
#endif


/** \file smoke/intern/FLUID_3D.h
 *  \ingroup smoke
 */
//////////////////////////////////////////////////////////////////////
// This file is part of Wavelet Turbulence.
// 
// Wavelet Turbulence is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Wavelet Turbulence is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Wavelet Turbulence.  If not, see <http://www.gnu.org/licenses/>.
// 
// Copyright 2008 Theodore Kim and Nils Thuerey
// 
// FLUID_3D.h: interface for the FLUID_3D class.
//
//////////////////////////////////////////////////////////////////////
// Heavy parallel optimization done. Many of the old functions now
// take begin and end parameters and process only specified part of the data.
// Some functions were divided into multiple ones.
//		- MiikaH
//////////////////////////////////////////////////////////////////////

#ifndef FLUID_3D_H
#define FLUID_3D_H

#include <cstdlib>
#include <cmath>
#include <cstring>
#include <iostream>
#include "OBSTACLE.h"
// #include "WTURBULENCE.h"
#include "VEC3.h"

using namespace std;
using namespace BasicVector;
class WTURBULENCE;

class FLUID_3D  
{
	public:
		FLUID_3D(int *res, /* int amplify, */ float *p0, float dtdef);
		FLUID_3D() {};
		virtual ~FLUID_3D();

		void initBlenderRNA(float *alpha, float *beta, float *dt_factor, float *vorticity, int *border_colli);
		
		// create & allocate vector noise advection 
		void initVectorNoise(int amplify);

		void addSmokeColumn();
		static void addSmokeTestCase(float* field, Vec3Int res);

		void step(float dt);
		void addObstacle(OBSTACLE* obstacle);

		const float* xVelocity() { return _xVelocity; }; 
		const float* yVelocity() { return _yVelocity; }; 
		const float* zVelocity() { return _zVelocity; }; 

		int xRes() const { return _xRes; };
		int yRes() const { return _yRes; };
		int zRes() const { return _zRes; };

	public:
		// dimensions
		int _xRes, _yRes, _zRes, _maxRes;
		Vec3Int _res;
		size_t _totalCells;
		int _slabSize;
		float _dx;
		float _p0[3];
		float _p1[3];
		float _totalTime;
		int _totalSteps;
		int _totalImgDumps;
		int _totalVelDumps;

		void artificialDampingSL(int zBegin, int zEnd);
		void artificialDampingExactSL(int pos);

		void setBorderObstacles();

		// fields
		float* _density;
		float* _densityOld;
		float* _heat;
		float* _heatOld;
		float* _xVelocity;
		float* _yVelocity;
		float* _zVelocity;
		float* _xVelocityOb;
		float* _yVelocityOb;
		float* _zVelocityOb;
		float* _xVelocityOld;
		float* _yVelocityOld;
		float* _zVelocityOld;
		float* _xForce;
		float* _yForce;
		float* _zForce;
		unsigned char*  _obstacles; /* only used (usefull) for static obstacles like domain boundaries */
		unsigned char*  _obstaclesAnim;

		// Required for proper threading:
		float* _xVelocityTemp;
		float* _yVelocityTemp;
		float* _zVelocityTemp;
		float* _heatTemp;
		float* _densityTemp;

		// CG fields
		int _iterations;

		// simulation constants
		float _dt;
		float *_dtFactor;
		float _vorticityEps;
		float _heatDiffusion;
		float *_vorticityRNA;	// RNA-pointer.
		float *_alpha; // for the buoyancy density term <-- as pointer to get blender RNA in here
		float *_beta; // was _buoyancy <-- as pointer to get blender RNA in here
		float _tempAmb; /* ambient temperature */
		float _constantScaling;

		bool _domainBcFront;  // z
		bool _domainBcTop;    // y
		bool _domainBcLeft;   // x
		bool _domainBcBack;   // DOMAIN_BC_FRONT
		bool _domainBcBottom; // DOMAIN_BC_TOP
		bool _domainBcRight;  // DOMAIN_BC_LEFT
		int *_borderColli; // border collision rules <-- as pointer to get blender RNA in here
		int _colloPrev;		// To track whether value has been changed (to not
							// have to recalibrate borders if nothing has changed
		void setBorderCollisions();

		void setObstacleVelocity(int zBegin, int zEnd);

		// WTURBULENCE object, if active
		// WTURBULENCE* _wTurbulence;

		// boundary setting functions
		void copyBorderAll(float* field, int zBegin, int zEnd);

		// timestepping functions
		void wipeBoundaries(int zBegin, int zEnd);
		void wipeBoundariesSL(int zBegin, int zEnd);
		void addForce(int zBegin, int zEnd);
		void addVorticity(int zBegin, int zEnd);
		void addBuoyancy(float *heat, float *density, int zBegin, int zEnd);

		// solver stuff
		void project();
		void diffuseHeat();
		void solvePressure(float* field, float* b, unsigned char* skip);
		void solvePressurePre(float* field, float* b, unsigned char* skip);
		void solveHeat(float* field, float* b, unsigned char* skip);


		// handle obstacle boundaries
		void setObstacleBoundaries(float *_pressure, int zBegin, int zEnd);
		void setObstaclePressure(float *_pressure, int zBegin, int zEnd);

	public:
		// advection, accessed e.g. by WTURBULENCE class
		//void advectMacCormack();
		void advectMacCormackBegin(int zBegin, int zEnd);
		void advectMacCormackEnd1(int zBegin, int zEnd);
		void advectMacCormackEnd2(int zBegin, int zEnd);

		// boundary setting functions
		static void copyBorderX(float* field, Vec3Int res, int zBegin, int zEnd);
		static void copyBorderY(float* field, Vec3Int res, int zBegin, int zEnd);
		static void copyBorderZ(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setNeumannX(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setNeumannY(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setNeumannZ(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setZeroX(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setZeroY(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setZeroZ(float* field, Vec3Int res, int zBegin, int zEnd);
		static void setZeroBorder(float* field, Vec3Int res, int zBegin, int zEnd) {
			setZeroX(field, res, zBegin, zEnd);
			setZeroY(field, res, zBegin, zEnd);
			setZeroZ(field, res, zBegin, zEnd);
		};

		

		// static advection functions, also used by WTURBULENCE
		static void advectFieldSemiLagrange(const float dt, const float* velx, const float* vely,  const float* velz,
				float* oldField, float* newField, Vec3Int res, int zBegin, int zEnd);
		static void advectFieldMacCormack1(const float dt, const float* xVelocity, const float* yVelocity, const float* zVelocity, 
				float* oldField, float* tempResult, Vec3Int res, int zBegin, int zEnd);
		static void advectFieldMacCormack2(const float dt, const float* xVelocity, const float* yVelocity, const float* zVelocity, 
				float* oldField, float* newField, float* tempResult, float* temp1,Vec3Int res, const unsigned char* obstacles, int zBegin, int zEnd);


		// temp ones for testing
		/*static void advectFieldMacCormack(const float dt, const float* xVelocity, const float* yVelocity, const float* zVelocity, 
				float* oldField, float* newField, float* temp1, float* temp2, Vec3Int res, const unsigned char* obstacles);*/
		/*static void advectFieldSemiLagrange2(const float dt, const float* velx, const float* vely,  const float* velz,
				float* oldField, float* newField, Vec3Int res);*/

		// maccormack helper functions
		static void clampExtrema(const float dt, const float* xVelocity, const float* yVelocity,  const float* zVelocity,
				float* oldField, float* newField, Vec3Int res, int zBegin, int zEnd);
		static void clampOutsideRays(const float dt, const float* xVelocity, const float* yVelocity,  const float* zVelocity,
				float* oldField, float* newField, Vec3Int res, const unsigned char* obstacles, const float *oldAdvection, int zBegin, int zEnd);



		// output helper functions
		// static void writeImageSliceXY(const float *field, Vec3Int res, int slice, string prefix, int picCnt, float scale=1.);
		// static void writeImageSliceYZ(const float *field, Vec3Int res, int slice, string prefix, int picCnt, float scale=1.);
		// static void writeImageSliceXZ(const float *field, Vec3Int res, int slice, string prefix, int picCnt, float scale=1.);
		// static void writeProjectedIntern(const float *field, Vec3Int res, int dir1, int dir2, string prefix, int picCnt, float scale=1.); 
};

#endif

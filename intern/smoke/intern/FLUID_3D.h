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

#ifndef FLUID_3D_H
#define FLUID_3D_H

#include <cstdlib>
#include <cmath>
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
		FLUID_3D(int *res, /* int amplify, */ float *p0, float dt);
		FLUID_3D() {};
		virtual ~FLUID_3D();

		void initBlenderRNA(float *alpha, float *beta);
		
		// create & allocate vector noise advection 
		void initVectorNoise(int amplify);

		void addSmokeColumn();
		static void addSmokeTestCase(float* field, Vec3Int res, float value);

		void step();
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

    void artificialDamping(float* field);

		// fields
		float* _density;
		float* _densityOld;
		float* _heat;
		float* _heatOld;
		float* _xVelocity;
		float* _yVelocity;
		float* _zVelocity;
		float* _xVelocityOld;
		float* _yVelocityOld;
		float* _zVelocityOld;
		float* _xForce;
		float* _yForce;
		float* _zForce;
		unsigned char*  _obstacles;

		// CG fields
		int _iterations;

		// simulation constants
		float _dt;
		float _vorticityEps;
		float _heatDiffusion;
		float *_alpha; // for the buoyancy density term <-- as pointer to get blender RNA in here
		float *_beta; // was _buoyancy <-- as pointer to get blender RNA in here
		float _tempAmb; /* ambient temperature */

		// WTURBULENCE object, if active
		// WTURBULENCE* _wTurbulence;

		// boundary setting functions
		void copyBorderAll(float* field);

		// timestepping functions
		void wipeBoundaries();
		void addForce();
		void addVorticity();
		void addBuoyancy(float *heat, float *density);

		// solver stuff
		void project();
		void diffuseHeat();
		void solvePressure(float* field, float* b, unsigned char* skip);
		void solvePressurePre(float* field, float* b, unsigned char* skip);
		void solveHeat(float* field, float* b, unsigned char* skip);

		// handle obstacle boundaries
		void setObstacleBoundaries(float *_pressure);
		void setObstaclePressure(float *_pressure);

	public:
		// advection, accessed e.g. by WTURBULENCE class
		void advectMacCormack();

		// boundary setting functions
		static void copyBorderX(float* field, Vec3Int res);
		static void copyBorderY(float* field, Vec3Int res);
		static void copyBorderZ(float* field, Vec3Int res);
		static void setNeumannX(float* field, Vec3Int res);
		static void setNeumannY(float* field, Vec3Int res);
		static void setNeumannZ(float* field, Vec3Int res);
		static void setZeroX(float* field, Vec3Int res);
		static void setZeroY(float* field, Vec3Int res);
		static void setZeroZ(float* field, Vec3Int res);
		static void setZeroBorder(float* field, Vec3Int res) {
			setZeroX(field, res);
			setZeroY(field, res);
			setZeroZ(field, res);
		};

		// static advection functions, also used by WTURBULENCE
		static void advectFieldSemiLagrange(const float dt, const float* velx, const float* vely,  const float* velz,
				float* oldField, float* newField, Vec3Int res);
		static void advectFieldMacCormack(const float dt, const float* xVelocity, const float* yVelocity, const float* zVelocity, 
				float* oldField, float* newField, float* temp1, float* temp2, Vec3Int res, const unsigned char* obstacles);

		// maccormack helper functions
		static void clampExtrema(const float dt, const float* xVelocity, const float* yVelocity,  const float* zVelocity,
				float* oldField, float* newField, Vec3Int res);
		static void clampOutsideRays(const float dt, const float* xVelocity, const float* yVelocity,  const float* zVelocity,
				float* oldField, float* newField, Vec3Int res, const unsigned char* obstacles, const float *oldAdvection);

		// output helper functions
		// static void writeImageSliceXY(const float *field, Vec3Int res, int slice, string prefix, int picCnt, float scale=1.);
		// static void writeImageSliceYZ(const float *field, Vec3Int res, int slice, string prefix, int picCnt, float scale=1.);
		// static void writeImageSliceXZ(const float *field, Vec3Int res, int slice, string prefix, int picCnt, float scale=1.);
		// static void writeProjectedIntern(const float *field, Vec3Int res, int dir1, int dir2, string prefix, int picCnt, float scale=1.); 
};

#endif

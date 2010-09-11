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
// WTURBULENCE handling
///////////////////////////////////////////////////////////////////////////////////

#ifndef WTURBULENCE_H
#define WTURBULENCE_H

#include "VEC3.h"
using namespace BasicVector;
class SIMPLE_PARSER;

///////////////////////////////////////////////////////////////////////////////
/// Main WTURBULENCE class, stores large density array etc.
///////////////////////////////////////////////////////////////////////////////
class WTURBULENCE  
{
	public:
		// both config files can be NULL, altCfg might override values from noiseCfg
		WTURBULENCE(int xResSm, int yResSm, int zResSm, int amplify, int noisetype);

		/// destructor
		virtual ~WTURBULENCE();
		
		void setNoise(int type);
		void initBlenderRNA(float *strength);

		// step more readable version -- no rotation correction
		void stepTurbulenceReadable(float dt, float* xvel, float* yvel, float* zvel, unsigned char *obstacles);

		// step more complete version -- include rotation correction
		// and use OpenMP if available
		void stepTurbulenceFull(float dt, float* xvel, float* yvel, float* zvel, unsigned char *obstacles);
	
		// texcoord functions
		void advectTextureCoordinates(float dtOrg, float* xvel, float* yvel, float* zvel, float *tempBig1, float *tempBig2);
		void resetTextureCoordinates(float *_eigMin, float *_eigMax);

		void computeEnergy(float *energy, float* xvel, float* yvel, float* zvel, unsigned char *obstacles);

		// evaluate wavelet noise function
		Vec3 WVelocity(Vec3 p);
		Vec3 WVelocityWithJacobian(Vec3 p, float* xUnwarped, float* yUnwarped, float* zUnwarped);

		// access functions
		inline float* getDensityBig() { return _densityBig; }
		inline float* getArrayTcU() { return _tcU; }
		inline float* getArrayTcV() { return _tcV; }
		inline float* getArrayTcW() { return _tcW; }

		inline Vec3Int getResSm() { return _resSm; } // small resolution
		inline Vec3Int getResBig() { return _resBig; }
		inline int getOctaves() { return _octaves; }

		// is accessed on through rna gui
		float *_strength;

	// protected:
		// enlargement factor from original velocity field / simulation
		// _Big = _amplify * _Sm
		int _amplify;
		int _octaves;
		
		// noise settings
		float _cullingThreshold;
		// float _noiseStrength;
		// float _noiseSizeScale;
		// bool _uvwAdvection;
		// bool _uvwReset;
		// float _noiseTimeanimSpeed;
		// int _dumpInterval;
		// nt _noiseControlType;
		// debug, scale density for projections output images
		// float _outputScale;

		// noise resolution
		int _xResBig;
		int _yResBig;
		int _zResBig;
		Vec3Int _resBig;
		Vec3 _invResBig;
		int _totalCellsBig;
		int _slabSizeBig;
		// original / small resolution
		int _xResSm;
		int _yResSm;
		int _zResSm;
		Vec3Int _resSm;
		Vec3 _invResSm;
		int _totalCellsSm;
		int _slabSizeSm;

		float* _densityBig;
		float* _densityBigOld;

		// texture coordinates for noise
		float* _tcU;
		float* _tcV;
		float* _tcW;
		float* _tcTemp;

		// noise data
		float* _noiseTile;
		//float* _noiseTileExt;

		// step counter
		int _totalStepsBig;
		
		void computeEigenvalues(float *_eigMin, float *_eigMax);
		void decomposeEnergy(float *energy, float *_highFreqEnergy);
};

#endif // WTURBULENCE_H


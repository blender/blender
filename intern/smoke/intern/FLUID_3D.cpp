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
// FLUID_3D.cpp: implementation of the FLUID_3D class.
//
//////////////////////////////////////////////////////////////////////

#include "FLUID_3D.h"
#include "IMAGE.h"
#include <INTERPOLATE.h>
#include "SPHERE.h"
#include <zlib.h>

// boundary conditions of the fluid domain
#define DOMAIN_BC_FRONT  0 // z
#define DOMAIN_BC_TOP    1 // y
#define DOMAIN_BC_LEFT   1 // x
#define DOMAIN_BC_BACK   DOMAIN_BC_FRONT
#define DOMAIN_BC_BOTTOM DOMAIN_BC_TOP
#define DOMAIN_BC_RIGHT  DOMAIN_BC_LEFT

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

FLUID_3D::FLUID_3D(int *res, float *p0, float dt) :
	_xRes(res[0]), _yRes(res[1]), _zRes(res[2]), _res(0.0f), _dt(dt)
{
	// set simulation consts
	// _dt = dt; // 0.10
	
	// start point of array
	_p0[0] = p0[0];
	_p0[1] = p0[1];
	_p0[2] = p0[2];

	_iterations = 100;
	_tempAmb = 0; 
	_heatDiffusion = 1e-3;
	_vorticityEps = 2.0;
	_totalTime = 0.0f;
	_totalSteps = 0;
	_res = Vec3Int(_xRes,_yRes,_zRes);
	_maxRes = MAX3(_xRes, _yRes, _zRes);
	
	// initialize wavelet turbulence
	/*
	if(amplify)
		_wTurbulence = new WTURBULENCE(_res[0],_res[1],_res[2], amplify, noisetype);
	else
		_wTurbulence = NULL;
	*/
	
	// scale the constants according to the refinement of the grid
	_dx = 1.0f / (float)_maxRes;
	float scaling = 64.0f / _maxRes;
	scaling = (scaling < 1.0f) ? 1.0f : scaling;
	_vorticityEps /= scaling;

	// allocate arrays
	_totalCells   = _xRes * _yRes * _zRes;
	_slabSize = _xRes * _yRes;
	_xVelocity    = new float[_totalCells];
	_yVelocity    = new float[_totalCells];
	_zVelocity    = new float[_totalCells];
	_xVelocityOld = new float[_totalCells];
	_yVelocityOld = new float[_totalCells];
	_zVelocityOld = new float[_totalCells];
	_xForce       = new float[_totalCells];
	_yForce       = new float[_totalCells];
	_zForce       = new float[_totalCells];
	_density      = new float[_totalCells];
	_densityOld   = new float[_totalCells];
	_heat         = new float[_totalCells];
	_heatOld      = new float[_totalCells];
	_obstacles    = new unsigned char[_totalCells]; // set 0 at end of step

	// DG TODO: check if alloc went fine

	for (int x = 0; x < _totalCells; x++)
	{
		_density[x]      = 0.0f;
		_densityOld[x]   = 0.0f;
		_heat[x]         = 0.0f;
		_heatOld[x]      = 0.0f;
		_xVelocity[x]    = 0.0f;
		_yVelocity[x]    = 0.0f;
		_zVelocity[x]    = 0.0f;
		_xVelocityOld[x] = 0.0f;
		_yVelocityOld[x] = 0.0f;
		_zVelocityOld[x] = 0.0f;
		_xForce[x]       = 0.0f;
		_yForce[x]       = 0.0f;
		_zForce[x]       = 0.0f;
		_obstacles[x]    = false;
	}

	// set side obstacles
	int index;
	for (int y = 0; y < _yRes; y++)
	for (int x = 0; x < _xRes; x++)
	{
		// front slab
		index = x + y * _xRes;
		if(DOMAIN_BC_FRONT==1) _obstacles[index] = 1;

		// back slab
		index += _totalCells - _slabSize;
		if(DOMAIN_BC_BACK==1) _obstacles[index] = 1;
	}

	for (int z = 0; z < _zRes; z++)
	for (int x = 0; x < _xRes; x++)
	{
		// bottom slab
		index = x + z * _slabSize;
		if(DOMAIN_BC_BOTTOM==1) _obstacles[index] = 1;

		// top slab
		index += _slabSize - _xRes;
		if(DOMAIN_BC_TOP==1) _obstacles[index] = 1;
	}

	for (int z = 0; z < _zRes; z++)
	for (int y = 0; y < _yRes; y++)
	{
		// left slab
		index = y * _xRes + z * _slabSize;
		if(DOMAIN_BC_LEFT==1) _obstacles[index] = 1;

		// right slab
		index += _xRes - 1;
		if(DOMAIN_BC_RIGHT==1) _obstacles[index] = 1;
	}
}

FLUID_3D::~FLUID_3D()
{
	if (_xVelocity) delete[] _xVelocity;
	if (_yVelocity) delete[] _yVelocity;
	if (_zVelocity) delete[] _zVelocity;
	if (_xVelocityOld) delete[] _xVelocityOld;
	if (_yVelocityOld) delete[] _yVelocityOld;
	if (_zVelocityOld) delete[] _zVelocityOld;
	if (_xForce) delete[] _xForce;
	if (_yForce) delete[] _yForce;
	if (_zForce) delete[] _zForce;
	if (_density) delete[] _density;
	if (_densityOld) delete[] _densityOld;
	if (_heat) delete[] _heat;
	if (_heatOld) delete[] _heatOld;
	if (_obstacles) delete[] _obstacles;
    // if (_wTurbulence) delete _wTurbulence;

    // printf("deleted fluid\n");
}

// init direct access functions from blender
void FLUID_3D::initBlenderRNA(float *alpha, float *beta)
{
	_alpha = alpha;
	_beta = beta;
}

//////////////////////////////////////////////////////////////////////
// step simulation once
//////////////////////////////////////////////////////////////////////
void FLUID_3D::step()
{
	// addSmokeTestCase(_density, _res);
	// addSmokeTestCase(_heat, _res);
	
	// wipe forces
	for (int i = 0; i < _totalCells; i++)
	{
		_xForce[i] = _yForce[i] = _zForce[i] = 0.0f;
		// _obstacles[i] &= ~2;
	}

	wipeBoundaries();

	// run the solvers
	addVorticity();
	addBuoyancy(_heat, _density);
	addForce();
	project();
	diffuseHeat();

	// advect everything
	advectMacCormack();

	// if(_wTurbulence) {
	// 	_wTurbulence->stepTurbulenceFull(_dt/_dx,
	//			_xVelocity, _yVelocity, _zVelocity, _obstacles);
		// _wTurbulence->stepTurbulenceReadable(_dt/_dx,
		//  _xVelocity, _yVelocity, _zVelocity, _obstacles);
	// }
/*
 // no file output
  float *src = _density;
	string prefix = string("./original.preview/density_fullxy_");
	writeImageSliceXY(src,_res, _res[2]/2, prefix, _totalSteps);
*/
	// artificial damping -- this is necessary because we use a
  // collated grid, and at very coarse grid resolutions, banding
  // artifacts can occur
	artificialDamping(_xVelocity);
	artificialDamping(_yVelocity);
	artificialDamping(_zVelocity);
/*
// no file output
  string pbrtPrefix = string("./pbrt/density_small_");
  IMAGE::dumpPBRT(_totalSteps, pbrtPrefix, _density, _res[0],_res[1],_res[2]);
  */
	_totalTime += _dt;
	_totalSteps++;	

	// todo xxx dg: only clear obstacles, not boundaries
	// memset(_obstacles, 0, sizeof(unsigned char)*_xRes*_yRes*_zRes);
}

//////////////////////////////////////////////////////////////////////
// helper function to dampen co-located grid artifacts of given arrays in intervals
// (only needed for velocity, strength (w) depends on testcase...
//////////////////////////////////////////////////////////////////////
void FLUID_3D::artificialDamping(float* field) {
	const float w = 0.9;
	if(_totalSteps % 4 == 1) {
		for (int z = 1; z < _res[2]-1; z++)
			for (int y = 1; y < _res[1]-1; y++)
				for (int x = 1+(y+z)%2; x < _res[0]-1; x+=2) {
					const int index = x + y*_res[0] + z * _slabSize;
					field[index] = (1-w)*field[index] + 1./6. * w*(
							field[index+1] + field[index-1] +
							field[index+_res[0]] + field[index-_res[0]] +
							field[index+_slabSize] + field[index-_slabSize] );
				}
	}
	if(_totalSteps % 4 == 3) {
		for (int z = 1; z < _res[2]-1; z++)
			for (int y = 1; y < _res[1]-1; y++)
				for (int x = 1+(y+z+1)%2; x < _res[0]-1; x+=2) {
					const int index = x + y*_res[0] + z * _slabSize;
					field[index] = (1-w)*field[index] + 1./6. * w*(
							field[index+1] + field[index-1] +
							field[index+_res[0]] + field[index-_res[0]] +
							field[index+_slabSize] + field[index-_slabSize] );
				}
	}
}

//////////////////////////////////////////////////////////////////////
// copy out the boundary in all directions
//////////////////////////////////////////////////////////////////////
void FLUID_3D::copyBorderAll(float* field)
{
	int index;
	for (int y = 0; y < _yRes; y++)
		for (int x = 0; x < _xRes; x++)
		{
			// front slab
			index = x + y * _xRes;
			field[index] = field[index + _slabSize];

			// back slab
			index += _totalCells - _slabSize;
			field[index] = field[index - _slabSize];
    }

	for (int z = 0; z < _zRes; z++)
		for (int x = 0; x < _xRes; x++)
    {
			// bottom slab
			index = x + z * _slabSize;
			field[index] = field[index + _xRes];

			// top slab
			index += _slabSize - _xRes;
			field[index] = field[index - _xRes];
    }

	for (int z = 0; z < _zRes; z++)
		for (int y = 0; y < _yRes; y++)
    {
			// left slab
			index = y * _xRes + z * _slabSize;
			field[index] = field[index + 1];

			// right slab
			index += _xRes - 1;
			field[index] = field[index - 1];
		}
}

//////////////////////////////////////////////////////////////////////
// wipe boundaries of velocity and density
//////////////////////////////////////////////////////////////////////
void FLUID_3D::wipeBoundaries()
{
	setZeroBorder(_xVelocity, _res);
	setZeroBorder(_yVelocity, _res);
	setZeroBorder(_zVelocity, _res);
	setZeroBorder(_density, _res);
}

//////////////////////////////////////////////////////////////////////
// add forces to velocity field
//////////////////////////////////////////////////////////////////////
void FLUID_3D::addForce()
{
	for (int i = 0; i < _totalCells; i++)
	{
		_xVelocity[i] += _dt * _xForce[i];
		_yVelocity[i] += _dt * _yForce[i];
		_zVelocity[i] += _dt * _zForce[i];
	}
}

//////////////////////////////////////////////////////////////////////
// project into divergence free field
//////////////////////////////////////////////////////////////////////
void FLUID_3D::project()
{
	int x, y, z;
	size_t index;

	float *_pressure = new float[_totalCells];
	float *_divergence   = new float[_totalCells];

	memset(_pressure, 0, sizeof(float)*_totalCells);
	memset(_divergence, 0, sizeof(float)*_totalCells);

	setObstacleBoundaries(_pressure);

	// copy out the boundaries
	if(DOMAIN_BC_LEFT == 0)  setNeumannX(_xVelocity, _res);
	else setZeroX(_xVelocity, _res); 

	if(DOMAIN_BC_TOP == 0)   setNeumannY(_yVelocity, _res);
	else setZeroY(_yVelocity, _res); 

	if(DOMAIN_BC_FRONT == 0) setNeumannZ(_zVelocity, _res);
	else setZeroZ(_zVelocity, _res);

	// calculate divergence
	index = _slabSize + _xRes + 1;
	for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
		for (y = 1; y < _yRes - 1; y++, index += 2)
			for (x = 1; x < _xRes - 1; x++, index++)
			{
				float xright = _xVelocity[index + 1];
				float xleft  = _xVelocity[index - 1];
				float yup    = _yVelocity[index + _xRes];
				float ydown  = _yVelocity[index - _xRes];
				float ztop   = _zVelocity[index + _slabSize];
				float zbottom = _zVelocity[index - _slabSize];

				if(_obstacles[index+1]) xright = - _xVelocity[index];
				if(_obstacles[index-1]) xleft  = - _xVelocity[index];
				if(_obstacles[index+_xRes]) yup    = - _yVelocity[index];
				if(_obstacles[index-_xRes]) ydown  = - _yVelocity[index];
				if(_obstacles[index+_slabSize]) ztop    = - _zVelocity[index];
				if(_obstacles[index-_slabSize]) zbottom = - _zVelocity[index];

				_divergence[index] = -_dx * 0.5f * (
						xright - xleft +
						yup - ydown +
						ztop - zbottom );

				// DG: commenting this helps CG to get a better start, 10-20% speed improvement
				// _pressure[index] = 0.0f;
			}
	copyBorderAll(_pressure);

	// solve Poisson equation
	solvePressurePre(_pressure, _divergence, _obstacles);

	setObstaclePressure(_pressure);

	// project out solution
	float invDx = 1.0f / _dx;
	index = _slabSize + _xRes + 1;
	for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
		for (y = 1; y < _yRes - 1; y++, index += 2)
			for (x = 1; x < _xRes - 1; x++, index++)
			{
				// if(!_obstacles[index])
				{
					_xVelocity[index] -= 0.5f * (_pressure[index + 1]     - _pressure[index - 1]) * invDx;
					_yVelocity[index] -= 0.5f * (_pressure[index + _xRes]  - _pressure[index - _xRes]) * invDx;
					_zVelocity[index] -= 0.5f * (_pressure[index + _slabSize] - _pressure[index - _slabSize]) * invDx;
				}/*
				else
				{
					_xVelocity[index] = _yVelocity[index] = _zVelocity[index] = 0.0f;
				}*/
			}

	if (_pressure) delete[] _pressure;
	if (_divergence) delete[] _divergence;
}

//////////////////////////////////////////////////////////////////////
// diffuse heat
//////////////////////////////////////////////////////////////////////
void FLUID_3D::diffuseHeat()
{
	SWAP_POINTERS(_heat, _heatOld);

	copyBorderAll(_heatOld);
	solveHeat(_heat, _heatOld, _obstacles);

	// zero out inside obstacles
	for (int x = 0; x < _totalCells; x++)
		if (_obstacles[x])
			_heat[x] = 0.0f;
}

//////////////////////////////////////////////////////////////////////
// stamp an obstacle in the _obstacles field
//////////////////////////////////////////////////////////////////////
void FLUID_3D::addObstacle(OBSTACLE* obstacle)
{
	int index = 0;
	for (int z = 0; z < _zRes; z++)
		for (int y = 0; y < _yRes; y++)
			for (int x = 0; x < _xRes; x++, index++)
				if (obstacle->inside(x * _dx, y * _dx, z * _dx)) {
					_obstacles[index] = true;
        }
}

//////////////////////////////////////////////////////////////////////
// calculate the obstacle directional types
//////////////////////////////////////////////////////////////////////
void FLUID_3D::setObstaclePressure(float *_pressure)
{
	// tag remaining obstacle blocks
	for (int z = 1, index = _slabSize + _xRes + 1;
			z < _zRes - 1; z++, index += 2 * _xRes)
		for (int y = 1; y < _yRes - 1; y++, index += 2)
			for (int x = 1; x < _xRes - 1; x++, index++)
		{
			// could do cascade of ifs, but they are a pain
			if (_obstacles[index])
			{
				const int top   = _obstacles[index + _slabSize];
				const int bottom= _obstacles[index - _slabSize];
				const int up    = _obstacles[index + _xRes];
				const int down  = _obstacles[index - _xRes];
				const int left  = _obstacles[index - 1];
				const int right = _obstacles[index + 1];

				// unused
				// const bool fullz = (top && bottom);
				// const bool fully = (up && down);
				//const bool fullx = (left && right);

				_xVelocity[index] =
				_yVelocity[index] =
				_zVelocity[index] = 0.0f;
				_pressure[index] = 0.0f;

				// average pressure neighbors
				float pcnt = 0.;
				if (left && !right) {
					_pressure[index] += _pressure[index + 1];
					pcnt += 1.;
				}
				if (!left && right) {
					_pressure[index] += _pressure[index - 1];
					pcnt += 1.;
				}
				if (up && !down) {
					_pressure[index] += _pressure[index - _xRes];
					pcnt += 1.;
				}
				if (!up && down) {
					_pressure[index] += _pressure[index + _xRes];
					pcnt += 1.;
				}
				if (top && !bottom) {
					_pressure[index] += _pressure[index - _slabSize];
					pcnt += 1.;
					// _zVelocity[index] +=  - _zVelocity[index - _slabSize];
					// vp += 1.0;
				}
				if (!top && bottom) {
					_pressure[index] += _pressure[index + _slabSize];
					pcnt += 1.;
					// _zVelocity[index] +=  - _zVelocity[index + _slabSize];
					// vp += 1.0;
				}
				
				if(pcnt > 0.000001f)
				 	_pressure[index] /= pcnt;

				// test - dg
				// if(vp > 0.000001f)
				//  	_zVelocity[index] /= vp;

				// TODO? set correct velocity bc's
				// velocities are only set to zero right now
				// this means it's not a full no-slip boundary condition
				// but a "half-slip" - still looks ok right now
			}
		}
}

void FLUID_3D::setObstacleBoundaries(float *_pressure)
{
	// cull degenerate obstacles , move to addObstacle?
	for (int z = 1, index = _slabSize + _xRes + 1;
			z < _zRes - 1; z++, index += 2 * _xRes)
		for (int y = 1; y < _yRes - 1; y++, index += 2)
			for (int x = 1; x < _xRes - 1; x++, index++)
			{
				if (_obstacles[index] != EMPTY)
				{
					const int top   = _obstacles[index + _slabSize];
					const int bottom= _obstacles[index - _slabSize];
					const int up    = _obstacles[index + _xRes];
					const int down  = _obstacles[index - _xRes];
					const int left  = _obstacles[index - 1];
					const int right = _obstacles[index + 1];

					int counter = 0;
					if (up)    counter++;
					if (down)  counter++;
					if (left)  counter++;
					if (right) counter++;
					if (top)  counter++;
					if (bottom) counter++;

					if (counter < 3)
						_obstacles[index] = EMPTY;
				}
				if (_obstacles[index])
				{
					_xVelocity[index] =
					_yVelocity[index] =
					_zVelocity[index] = 0.0f;
					_pressure[index] = 0.0f;
				}
			}
}

//////////////////////////////////////////////////////////////////////
// add buoyancy forces
//////////////////////////////////////////////////////////////////////
void FLUID_3D::addBuoyancy(float *heat, float *density)
{
	int index = 0;

	for (int z = 0; z < _zRes; z++)
		for (int y = 0; y < _yRes; y++)
			for (int x = 0; x < _xRes; x++, index++)
			{
				_zForce[index] += *_alpha * density[index] + (*_beta * (heat[index] - _tempAmb)); // DG: was _yForce, changed for Blender
			}
}

//////////////////////////////////////////////////////////////////////
// add vorticity to the force field
//////////////////////////////////////////////////////////////////////
void FLUID_3D::addVorticity()
{
	int x,y,z,index;
	if(_vorticityEps<=0.) return;

	float *_xVorticity, *_yVorticity, *_zVorticity, *_vorticity;

	_xVorticity = new float[_totalCells];
	_yVorticity = new float[_totalCells];
	_zVorticity = new float[_totalCells];
	_vorticity = new float[_totalCells];

	memset(_xVorticity, 0, sizeof(float)*_totalCells);
	memset(_yVorticity, 0, sizeof(float)*_totalCells);
	memset(_zVorticity, 0, sizeof(float)*_totalCells);
	memset(_vorticity, 0, sizeof(float)*_totalCells);

	// calculate vorticity
	float gridSize = 0.5f / _dx;
	index = _slabSize + _xRes + 1;
	for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
		for (y = 1; y < _yRes - 1; y++, index += 2)
			for (x = 1; x < _xRes - 1; x++, index++)
			{
				int up    = _obstacles[index + _xRes] ? index : index + _xRes;
				int down  = _obstacles[index - _xRes] ? index : index - _xRes;
				float dy  = (up == index || down == index) ? 1.0f / _dx : gridSize;
				int out   = _obstacles[index + _slabSize] ? index : index + _slabSize;
				int in    = _obstacles[index - _slabSize] ? index : index - _slabSize;
				float dz  = (out == index || in == index) ? 1.0f / _dx : gridSize;
				int right = _obstacles[index + 1] ? index : index + 1;
				int left  = _obstacles[index - 1] ? index : index - 1;
				float dx  = (right == index || right == index) ? 1.0f / _dx : gridSize;

				_xVorticity[index] = (_zVelocity[up] - _zVelocity[down]) * dy + (-_yVelocity[out] + _yVelocity[in]) * dz;
				_yVorticity[index] = (_xVelocity[out] - _xVelocity[in]) * dz + (-_zVelocity[right] + _zVelocity[left]) * dx;
				_zVorticity[index] = (_yVelocity[right] - _yVelocity[left]) * dx + (-_xVelocity[up] + _xVelocity[down])* dy;

				_vorticity[index] = sqrtf(_xVorticity[index] * _xVorticity[index] +
						_yVorticity[index] * _yVorticity[index] +
						_zVorticity[index] * _zVorticity[index]);
			}

	// calculate normalized vorticity vectors
	float eps = _vorticityEps;
	index = _slabSize + _xRes + 1;
	for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
		for (y = 1; y < _yRes - 1; y++, index += 2)
			for (x = 1; x < _xRes - 1; x++, index++)
				if (!_obstacles[index])
				{
					float N[3];

					int up    = _obstacles[index + _xRes] ? index : index + _xRes;
					int down  = _obstacles[index - _xRes] ? index : index - _xRes;
					float dy  = (up == index || down == index) ? 1.0f / _dx : gridSize;
					int out   = _obstacles[index + _slabSize] ? index : index + _slabSize;
					int in    = _obstacles[index - _slabSize] ? index : index - _slabSize;
					float dz  = (out == index || in == index) ? 1.0f / _dx : gridSize;
					int right = _obstacles[index + 1] ? index : index + 1;
					int left  = _obstacles[index - 1] ? index : index - 1;
					float dx  = (right == index || right == index) ? 1.0f / _dx : gridSize;
					N[0] = (_vorticity[right] - _vorticity[left]) * dx;
					N[1] = (_vorticity[up] - _vorticity[down]) * dy;
					N[2] = (_vorticity[out] - _vorticity[in]) * dz;

					float magnitude = sqrtf(N[0] * N[0] + N[1] * N[1] + N[2] * N[2]);
					if (magnitude > 0.0f)
					{
						magnitude = 1.0f / magnitude;
						N[0] *= magnitude;
						N[1] *= magnitude;
						N[2] *= magnitude;

						_xForce[index] += (N[1] * _zVorticity[index] - N[2] * _yVorticity[index]) * _dx * eps;
						_yForce[index] -= (N[0] * _zVorticity[index] - N[2] * _xVorticity[index]) * _dx * eps;
						_zForce[index] += (N[0] * _yVorticity[index] - N[1] * _xVorticity[index]) * _dx * eps;
					}
				}
				
	if (_xVorticity) delete[] _xVorticity;
	if (_yVorticity) delete[] _yVorticity;
	if (_zVorticity) delete[] _zVorticity;
	if (_vorticity) delete[] _vorticity;
}

//////////////////////////////////////////////////////////////////////
// Advect using the MacCormack method from the Selle paper
//////////////////////////////////////////////////////////////////////
void FLUID_3D::advectMacCormack()
{
	Vec3Int res = Vec3Int(_xRes,_yRes,_zRes);

	if(DOMAIN_BC_LEFT == 0) copyBorderX(_xVelocity, res);
	else setZeroX(_xVelocity, res); 

	if(DOMAIN_BC_TOP == 0) copyBorderY(_yVelocity, res);
	else setZeroY(_yVelocity, res); 

	if(DOMAIN_BC_FRONT == 0) copyBorderZ(_zVelocity, res);
	else setZeroZ(_zVelocity, res);

	SWAP_POINTERS(_xVelocity, _xVelocityOld);
	SWAP_POINTERS(_yVelocity, _yVelocityOld);
	SWAP_POINTERS(_zVelocity, _zVelocityOld);
	SWAP_POINTERS(_density, _densityOld);
	SWAP_POINTERS(_heat, _heatOld);

	const float dt0 = _dt / _dx;
	// use force arrays as temp arrays
	for (int x = 0; x < _totalCells; x++)
		_xForce[x] = _yForce[x] = 0.0;

	float* t1 = _xForce;
	float* t2 = _yForce;

	advectFieldMacCormack(dt0, _xVelocityOld, _yVelocityOld, _zVelocityOld, _densityOld, _density, t1,t2, res, _obstacles);
	advectFieldMacCormack(dt0, _xVelocityOld, _yVelocityOld, _zVelocityOld, _heatOld, _heat, t1,t2, res, _obstacles);
	advectFieldMacCormack(dt0, _xVelocityOld, _yVelocityOld, _zVelocityOld, _xVelocityOld, _xVelocity, t1,t2, res, _obstacles);
	advectFieldMacCormack(dt0, _xVelocityOld, _yVelocityOld, _zVelocityOld, _yVelocityOld, _yVelocity, t1,t2, res, _obstacles);
	advectFieldMacCormack(dt0, _xVelocityOld, _yVelocityOld, _zVelocityOld, _zVelocityOld, _zVelocity, t1,t2, res, _obstacles);

	if(DOMAIN_BC_LEFT == 0) copyBorderX(_xVelocity, res);
	else setZeroX(_xVelocity, res); 

	if(DOMAIN_BC_TOP == 0) copyBorderY(_yVelocity, res);
	else setZeroY(_yVelocity, res); 

	if(DOMAIN_BC_FRONT == 0) copyBorderZ(_zVelocity, res);
	else setZeroZ(_zVelocity, res);

	setZeroBorder(_density, res);
	setZeroBorder(_heat, res);

  for (int x = 0; x < _totalCells; x++)
    t1[x] = t2[x] = 0.0;
}

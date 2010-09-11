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
// Both solvers optimized by merging loops and precalculating
// stuff used in iteration loop.
//		- MiikaH
//////////////////////////////////////////////////////////////////////

#include "FLUID_3D.h"
#include <cstring>
#define SOLVER_ACCURACY 1e-06

//////////////////////////////////////////////////////////////////////
// solve the heat equation with CG
//////////////////////////////////////////////////////////////////////
void FLUID_3D::solveHeat(float* field, float* b, unsigned char* skip)
{
	int x, y, z;
	const int twoxr = 2 * _xRes;
	size_t index;
	const float heatConst = _dt * _heatDiffusion / (_dx * _dx);
	float *_q, *_residual, *_direction, *_Acenter;

	// i = 0
	int i = 0;

	_residual     = new float[_totalCells]; // set 0
	_direction    = new float[_totalCells]; // set 0
	_q            = new float[_totalCells]; // set 0
	_Acenter       = new float[_totalCells]; // set 0

  	memset(_residual, 0, sizeof(float)*_totalCells);
	memset(_q, 0, sizeof(float)*_totalCells);
	memset(_direction, 0, sizeof(float)*_totalCells);
	memset(_Acenter, 0, sizeof(float)*_totalCells);

	float deltaNew = 0.0f;

  // r = b - Ax
  index = _slabSize + _xRes + 1;
  for (z = 1; z < _zRes - 1; z++, index += twoxr)
    for (y = 1; y < _yRes - 1; y++, index += 2)
      for (x = 1; x < _xRes - 1; x++, index++)
      {
        // if the cell is a variable
        _Acenter[index] = 1.0f;
        if (!skip[index])
        {
          // set the matrix to the Poisson stencil in order
          if (!skip[index + 1]) _Acenter[index] += heatConst;
          if (!skip[index - 1]) _Acenter[index] += heatConst;
          if (!skip[index + _xRes]) _Acenter[index] += heatConst;
          if (!skip[index - _xRes]) _Acenter[index] += heatConst;
          if (!skip[index + _slabSize]) _Acenter[index] += heatConst;
          if (!skip[index - _slabSize]) _Acenter[index] += heatConst;

		  _residual[index] = b[index] - (_Acenter[index] * field[index] + 
          field[index - 1] * (skip[index - 1] ? 0.0 : -heatConst) + 
          field[index + 1] * (skip[index + 1] ? 0.0 : -heatConst) +
          field[index - _xRes] * (skip[index - _xRes] ? 0.0 : -heatConst) + 
          field[index + _xRes] * (skip[index + _xRes] ? 0.0 : -heatConst) +
          field[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -heatConst) + 
          field[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -heatConst));
        }
		else
		{
        _residual[index] = 0.0f;
		}

		_direction[index] = _residual[index];
		deltaNew += _residual[index] * _residual[index];
      }


  // While deltaNew > (eps^2) * delta0
  const float eps  = SOLVER_ACCURACY;
  float maxR = 2.0f * eps;
  while ((i < _iterations) && (maxR > eps))
  {
    // q = Ad
	float alpha = 0.0f;

    index = _slabSize + _xRes + 1;
    for (z = 1; z < _zRes - 1; z++, index += twoxr)
      for (y = 1; y < _yRes - 1; y++, index += 2)
        for (x = 1; x < _xRes - 1; x++, index++)
        {
          // if the cell is a variable
          if (!skip[index])
          {

			_q[index] = (_Acenter[index] * _direction[index] + 
            _direction[index - 1] * (skip[index - 1] ? 0.0 : -heatConst) + 
            _direction[index + 1] * (skip[index + 1] ? 0.0 : -heatConst) +
            _direction[index - _xRes] * (skip[index - _xRes] ? 0.0 : -heatConst) + 
            _direction[index + _xRes] * (skip[index + _xRes] ? 0.0 : -heatConst) +
            _direction[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -heatConst) + 
            _direction[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -heatConst));
          }
		  else
		  {
          _q[index] = 0.0f;
		  }
		  alpha += _direction[index] * _q[index];
        }

    if (fabs(alpha) > 0.0f)
      alpha = deltaNew / alpha;
	
	float deltaOld = deltaNew;
	deltaNew = 0.0f;

	maxR = 0.0f;

    index = _slabSize + _xRes + 1;
    for (z = 1; z < _zRes - 1; z++, index += twoxr)
      for (y = 1; y < _yRes - 1; y++, index += 2)
        for (x = 1; x < _xRes - 1; x++, index++)
		{
          field[index] += alpha * _direction[index];

		  _residual[index] -= alpha * _q[index];
          maxR = (_residual[index] > maxR) ? _residual[index] : maxR;

		  deltaNew += _residual[index] * _residual[index];
		}

    float beta = deltaNew / deltaOld;

    index = _slabSize + _xRes + 1;
    for (z = 1; z < _zRes - 1; z++, index += twoxr)
      for (y = 1; y < _yRes - 1; y++, index += 2)
        for (x = 1; x < _xRes - 1; x++, index++)
         _direction[index] = _residual[index] + beta * _direction[index];

	
    i++;
  }
  // cout << i << " iterations converged to " << maxR << endl;

	if (_residual) delete[] _residual;
	if (_direction) delete[] _direction;
	if (_q)       delete[] _q;
	if (_Acenter)  delete[] _Acenter;
}


void FLUID_3D::solvePressurePre(float* field, float* b, unsigned char* skip)
{
	int x, y, z;
	size_t index;
	float *_q, *_Precond, *_h, *_residual, *_direction;

	// i = 0
	int i = 0;

	_residual     = new float[_totalCells]; // set 0
	_direction    = new float[_totalCells]; // set 0
	_q            = new float[_totalCells]; // set 0
	_h			  = new float[_totalCells]; // set 0
	_Precond	  = new float[_totalCells]; // set 0

	memset(_residual, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_q, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_direction, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_h, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_Precond, 0, sizeof(float)*_xRes*_yRes*_zRes);

	float deltaNew = 0.0f;

	// r = b - Ax
	index = _slabSize + _xRes + 1;
	for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
		for (y = 1; y < _yRes - 1; y++, index += 2)
		  for (x = 1; x < _xRes - 1; x++, index++)
		  {
			// if the cell is a variable
			float Acenter = 0.0f;
			if (!skip[index])
			{
			  // set the matrix to the Poisson stencil in order
			  if (!skip[index + 1]) Acenter += 1.;
			  if (!skip[index - 1]) Acenter += 1.;
			  if (!skip[index + _xRes]) Acenter += 1.;
			  if (!skip[index - _xRes]) Acenter += 1.;
			  if (!skip[index + _slabSize]) Acenter += 1.;
			  if (!skip[index - _slabSize]) Acenter += 1.;

			  _residual[index] = b[index] - (Acenter * field[index] +  
			  field[index - 1] * (skip[index - 1] ? 0.0 : -1.0f)+ 
			  field[index + 1] * (skip[index + 1] ? 0.0 : -1.0f)+
			  field[index - _xRes] * (skip[index - _xRes] ? 0.0 : -1.0f)+ 
			  field[index + _xRes] * (skip[index + _xRes] ? 0.0 : -1.0f)+
			  field[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -1.0f)+ 
			  field[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -1.0f) );
			}
			else
			{
			_residual[index] = 0.0f;
			}

			// P^-1
			if(Acenter < 1.0)
				_Precond[index] = 0.0;
			else
				_Precond[index] = 1.0 / Acenter;

			// p = P^-1 * r
			_direction[index] = _residual[index] * _Precond[index];

			deltaNew += _residual[index] * _direction[index];
		  }


  // While deltaNew > (eps^2) * delta0
  const float eps  = SOLVER_ACCURACY;
  //while ((i < _iterations) && (deltaNew > eps*delta0))
  float maxR = 2.0f * eps;
  // while (i < _iterations)
  while ((i < _iterations) && (maxR > 0.001*eps))
  {

	float alpha = 0.0f;

    index = _slabSize + _xRes + 1;
    for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
      for (y = 1; y < _yRes - 1; y++, index += 2)
        for (x = 1; x < _xRes - 1; x++, index++)
        {
          // if the cell is a variable
          float Acenter = 0.0f;
          if (!skip[index])
          {
            // set the matrix to the Poisson stencil in order
            if (!skip[index + 1]) Acenter += 1.;
            if (!skip[index - 1]) Acenter += 1.;
            if (!skip[index + _xRes]) Acenter += 1.;
            if (!skip[index - _xRes]) Acenter += 1.;
            if (!skip[index + _slabSize]) Acenter += 1.;
            if (!skip[index - _slabSize]) Acenter += 1.;

			_q[index] = Acenter * _direction[index] +  
            _direction[index - 1] * (skip[index - 1] ? 0.0 : -1.0f) + 
            _direction[index + 1] * (skip[index + 1] ? 0.0 : -1.0f) +
            _direction[index - _xRes] * (skip[index - _xRes] ? 0.0 : -1.0f) + 
            _direction[index + _xRes] * (skip[index + _xRes] ? 0.0 : -1.0f)+
            _direction[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -1.0f) + 
            _direction[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -1.0f);
          }
		  else
		  {
          _q[index] = 0.0f;
		  }

		  alpha += _direction[index] * _q[index];
        }


    if (fabs(alpha) > 0.0f)
      alpha = deltaNew / alpha;

	float deltaOld = deltaNew;
	deltaNew = 0.0f;

	maxR = 0.0;

	float tmp;

    // x = x + alpha * d
    index = _slabSize + _xRes + 1;
    for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
      for (y = 1; y < _yRes - 1; y++, index += 2)
        for (x = 1; x < _xRes - 1; x++, index++)
		{
          field[index] += alpha * _direction[index];

		  _residual[index] -= alpha * _q[index];

		  _h[index] = _Precond[index] * _residual[index];

		  tmp = _residual[index] * _h[index];
		  deltaNew += tmp;
		  maxR = (tmp > maxR) ? tmp : maxR;

		}


    // beta = deltaNew / deltaOld
    float beta = deltaNew / deltaOld;

    // d = h + beta * d
    index = _slabSize + _xRes + 1;
    for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes)
      for (y = 1; y < _yRes - 1; y++, index += 2)
        for (x = 1; x < _xRes - 1; x++, index++)
          _direction[index] = _h[index] + beta * _direction[index];

    // i = i + 1
    i++;
  }
  // cout << i << " iterations converged to " << sqrt(maxR) << endl;

	if (_h) delete[] _h;
	if (_Precond) delete[] _Precond;
	if (_residual) delete[] _residual;
	if (_direction) delete[] _direction;
	if (_q)       delete[] _q;
}

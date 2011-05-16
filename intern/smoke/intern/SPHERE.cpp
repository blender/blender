/** \file smoke/intern/SPHERE.cpp
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
// SPHERE.cpp: implementation of the SPHERE class.
//
//////////////////////////////////////////////////////////////////////

#include "SPHERE.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SPHERE::SPHERE(float x, float y, float z, float radius) :
  _radius(radius)
{
  _center[0] = x;
  _center[1] = y;
  _center[2] = z;
}

SPHERE::~SPHERE()
{

}

bool SPHERE::inside(float x, float y, float z)
{
  float translate[] = {x - _center[0], y - _center[1], z - _center[2]};
  float magnitude = translate[0] * translate[0] + 
                    translate[1] * translate[1] + 
                    translate[2] * translate[2];

  return (magnitude < _radius * _radius) ? true : false;
}

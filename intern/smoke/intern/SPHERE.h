/** \file smoke/intern/SPHERE.h
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
// SPHERE.h: interface for the SPHERE class.
//
//////////////////////////////////////////////////////////////////////

#ifndef SPHERE_H
#define SPHERE_H

#include "OBSTACLE.h"

class SPHERE : public OBSTACLE  
{
public:
	SPHERE(float x, float y, float z, float radius);
	virtual ~SPHERE();

  bool inside(float x, float y, float z);

private:
  float _center[3];
  float _radius;
};

#endif

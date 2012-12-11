
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include <math.h>
#include "RandGen.h"
#include "PseudoNoise.h"

static const unsigned NB_VALUE_NOISE = 512;

real *PseudoNoise::_values;

PseudoNoise::PseudoNoise ()
{
}

void
PseudoNoise::init (long seed)
{
  _values = new real[NB_VALUE_NOISE];
  RandGen::srand48(seed);
  for (unsigned int i=0; i<NB_VALUE_NOISE; i++)
    _values[i] = -1.0 + 2.0 * RandGen::drand48();
}

real 
PseudoNoise::linearNoise (real x)
{
  real tmp;
  int i = modf(x, &tmp) * NB_VALUE_NOISE;
  real x1=_values[i], x2=_values[(i+1)%NB_VALUE_NOISE];
  real t = modf(x * NB_VALUE_NOISE, &tmp);
  return x1*(1-t)+x2*t;
}

static real 
LanczosWindowed(real t)
{
  if (fabs(t)>2) return 0;
  if (fabs(t)<M_EPSILON) return 1.0;
  return sin(M_PI*t)/(M_PI*t) * sin(M_PI*t/2.0)/(M_PI*t/2.0);
}

real 
PseudoNoise::smoothNoise (real x)
{
  real tmp;
  int i = modf(x, &tmp) * NB_VALUE_NOISE;
  int h = i - 1;
  if (h < 0)
    {
	  h = NB_VALUE_NOISE + h;
    }

  real x1=_values[i], x2=_values[(i+1)%NB_VALUE_NOISE];
  real x0=_values[h], x3=_values[(i+2)%NB_VALUE_NOISE];

  real t = modf(x * NB_VALUE_NOISE, &tmp);
  real y0=LanczosWindowed(-1-t);
  real y1=LanczosWindowed(-t);
  real y2=LanczosWindowed(1-t);
  real y3=LanczosWindowed(2-t);
  //	cerr<<"x0="<<x0<<"  x1="<<x1<<"  x2="<<x2<<"  x3="<<x3<<endl;
  //	cerr<<"y0="<<y0<<"  y1="<<y1<<"  y2="<<y2<<"  y3="<<y3<<"  :";
  return (x0*y0+x1*y1+x2*y2+x3*y3)/(y0+y1+y2+y3);
}

real 
PseudoNoise::turbulenceSmooth (real x, unsigned nbOctave)
{
  real y=0;
  real k=1.0;
  for (unsigned i=0; i<nbOctave; i++)
    {
      y=y+k*smoothNoise(x*k);
      k=k/2.0;
    }
  return y;
}

real 
PseudoNoise::turbulenceLinear (real x, unsigned nbOctave)
{
  real y=0;
  real k=1.0;
  for (unsigned i=0; i<nbOctave; i++)
    {
      y=y+k*linearNoise(x*k);
      k=k/2.0;
    }
  return y;
}

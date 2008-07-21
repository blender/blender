
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

#include "GaussianFilter.h"
#include <stdlib.h>

GaussianFilter::GaussianFilter(float iSigma )
{
  _sigma = iSigma;
  _mask = 0;
  computeMask();
}

GaussianFilter::GaussianFilter(const GaussianFilter& iBrother) 
{
  _sigma = iBrother._sigma;
  _maskSize = iBrother._maskSize;
  _bound = iBrother._bound;
  _storedMaskSize = iBrother._storedMaskSize;
  _mask = new float[_maskSize*_maskSize];
  memcpy(_mask, iBrother._mask, _maskSize*_maskSize*sizeof(float));
}


GaussianFilter& GaussianFilter::operator= (const GaussianFilter& iBrother) 
{
  _sigma = iBrother._sigma;
  _maskSize = iBrother._maskSize;
  _bound = iBrother._bound;
  _storedMaskSize = iBrother._storedMaskSize;
  _mask = new float[_storedMaskSize*_storedMaskSize];
  memcpy(_mask, iBrother._mask, _storedMaskSize*_storedMaskSize*sizeof(float));
  return *this;
}


GaussianFilter::~GaussianFilter()
{
  if(0!=_mask)
  {
    delete [] _mask;
  }
}

int GaussianFilter::computeMaskSize(float sigma)
{
  int maskSize = (int)floor(4*sigma)+1;
  if(0 == maskSize%2)
    ++maskSize;

  return maskSize;
}

void GaussianFilter::setSigma(float sigma)
{
  _sigma = sigma;
  computeMask();
}

void GaussianFilter::computeMask()
{
  if(0 != _mask){
    delete [] _mask;
  }

  _maskSize = computeMaskSize(_sigma);
  _storedMaskSize = (_maskSize+1)>>1;
  _bound = _storedMaskSize-1;

  float norm = _sigma*_sigma*2.f*M_PI; 
  float invNorm = 1.0/norm;
  _mask = new float[_storedMaskSize*_storedMaskSize*sizeof(float)];
  for(int i=0; i<_storedMaskSize; ++i)
    for(int j=0; j<_storedMaskSize; ++j)
      _mask[i*_storedMaskSize+j] = invNorm*exp(-(i*i + j*j)/(2.0*_sigma*_sigma));
      //_mask[i*_storedMaskSize+j] = exp(-(i*i + j*j)/(2.0*_sigma*_sigma));
}


/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to perform gaussian filtering operations on an image
 */

#include <cstdlib>

#include "GaussianFilter.h"

#include "BLI_math_base.h"

namespace Freestyle {

GaussianFilter::GaussianFilter(float iSigma)
{
  _sigma = iSigma;
  _mask = nullptr;
  computeMask();
}

GaussianFilter::GaussianFilter(const GaussianFilter &iBrother)
{
  _sigma = iBrother._sigma;
  _maskSize = iBrother._maskSize;
  _bound = iBrother._bound;
  _storedMaskSize = iBrother._storedMaskSize;
  _mask = new float[_maskSize * _maskSize];
  memcpy(_mask, iBrother._mask, _maskSize * _maskSize * sizeof(float));
}

GaussianFilter &GaussianFilter::operator=(const GaussianFilter &iBrother)
{
  _sigma = iBrother._sigma;
  _maskSize = iBrother._maskSize;
  _bound = iBrother._bound;
  _storedMaskSize = iBrother._storedMaskSize;
  _mask = new float[_storedMaskSize * _storedMaskSize];
  memcpy(_mask, iBrother._mask, _storedMaskSize * _storedMaskSize * sizeof(float));
  return *this;
}

GaussianFilter::~GaussianFilter()
{
  delete[] _mask;
}

int GaussianFilter::computeMaskSize(float sigma)
{
  int maskSize = int(floor(4 * sigma)) + 1;
  if (0 == (maskSize % 2)) {
    ++maskSize;
  }

  return maskSize;
}

void GaussianFilter::setSigma(float sigma)
{
  _sigma = sigma;
  computeMask();
}

void GaussianFilter::computeMask()
{
  delete[] _mask;

  _maskSize = computeMaskSize(_sigma);
  _storedMaskSize = (_maskSize + 1) >> 1;
  _bound = _storedMaskSize - 1;

  float norm = _sigma * _sigma * 2.0f * M_PI;
  float invNorm = 1.0f / norm;
  _mask = new float[_storedMaskSize * _storedMaskSize * sizeof(float)];
  for (int i = 0; i < _storedMaskSize; ++i) {
    for (int j = 0; j < _storedMaskSize; ++j) {
#if 0
      _mask[i * _storedMaskSize + j] = exp(-(i * i + j * j) / (2.0 * _sigma * _sigma));
#else
      _mask[i * _storedMaskSize + j] = invNorm * exp(-(i * i + j * j) / (2.0 * _sigma * _sigma));
#endif
    }
  }
}

} /* namespace Freestyle */

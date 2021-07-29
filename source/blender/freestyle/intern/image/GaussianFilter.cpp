/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/image/GaussianFilter.cpp
 *  \ingroup freestyle
 *  \brief Class to perform gaussian filtering operations on an image
 *  \author Stephane Grabli
 *  \date 20/05/2003
 */

#include <stdlib.h>

#include "GaussianFilter.h"

namespace Freestyle {

GaussianFilter::GaussianFilter(float iSigma)
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
	_mask = new float[_maskSize * _maskSize];
	memcpy(_mask, iBrother._mask, _maskSize * _maskSize * sizeof(float));
}

GaussianFilter& GaussianFilter::operator=(const GaussianFilter& iBrother)
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
	if (0 != _mask) {
		delete[] _mask;
	}
}

int GaussianFilter::computeMaskSize(float sigma)
{
	int maskSize = (int)floor(4 * sigma) + 1;
	if (0 == (maskSize % 2))
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
	if (0 != _mask) {
		delete[] _mask;
	}

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

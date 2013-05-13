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

#ifndef __IMAGEPYRAMID_H__
#define __IMAGEPYRAMID_H__

/** \file blender/freestyle/intern/image/ImagePyramid.h
 *  \ingroup freestyle
 *  \brief Class to represent a pyramid of images
 *  \author Stephane Grabli
 *  \date 25/12/2003
 */

#include <vector>

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class GrayImage;

class LIB_IMAGE_EXPORT ImagePyramid
{
protected:
	std::vector<GrayImage*> _levels;

public:
	ImagePyramid() {}
	ImagePyramid(const ImagePyramid& iBrother);
	//ImagePyramid(const GrayImage& level0, unsigned nbLevels);
	virtual ~ImagePyramid();

	/*! Builds the pyramid.
	 * must be overloaded by inherited classes.
	 * if nbLevels==0, the complete pyramid is built
	 */
	virtual void BuildPyramid(const GrayImage& level0, unsigned nbLevels) = 0;

	/*! Builds a pyramid without copying the base level */
	virtual void BuildPyramid(GrayImage *level0, unsigned nbLevels) = 0;

	virtual GrayImage *getLevel(int l);
	/*! Returns the pixel x,y using bilinear interpolation.
	 *  \param x
	 *    the abscissa specified in the finest level coordinate system
	 *  \param y
	 *    the ordinate specified in the finest level coordinate system
	 *  \param level
	 *    the level from which we want the pixel to be evaluated
	 */
	virtual float pixel(int x, int y, int level=0);

	/*! Returns the width of the level-th level image */
	virtual int width(int level=0);

	/*! Returns the height of the level-th level image */
	virtual int height(int level=0);

	/*! Returns the number of levels in the pyramid */
	inline int getNumberOfLevels() const
	{
		return _levels.size();
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ImagePyramid")
#endif
};

class LIB_IMAGE_EXPORT GaussianPyramid : public ImagePyramid
{
protected:
	float _sigma;

public:
	GaussianPyramid(float iSigma=1.f) : ImagePyramid()
	{
		_sigma = iSigma;
	}

	GaussianPyramid(const GrayImage& level0, unsigned nbLevels, float iSigma=1.0f);
	GaussianPyramid(GrayImage *level0, unsigned nbLevels, float iSigma=1.0f);
	GaussianPyramid(const GaussianPyramid& iBrother);
	virtual ~GaussianPyramid() {}

	virtual void BuildPyramid(const GrayImage& level0, unsigned nbLevels);
	virtual void BuildPyramid(GrayImage *level0, unsigned nbLevels);

	/* accessors */
	inline float getSigma() const
	{
		return _sigma;
	}

	/* modifiers */
};

} /* namespace Freestyle */

#endif // __IMAGEPYRAMID_H__

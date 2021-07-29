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

#ifndef __FREESTYLE_ADVANCED_FUNCTIONS_0D_H__
#define __FREESTYLE_ADVANCED_FUNCTIONS_0D_H__

/** \file blender/freestyle/intern/stroke/AdvancedFunctions0D.h
 *  \ingroup freestyle
 *  \brief Functions taking 0D input
 *  \author Stephane Grabli
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include "../image/GaussianFilter.h"
#include "../image/Image.h"

#include "../view_map/Functions0D.h"

//
// Functions definitions
//
///////////////////////////////////////////////////////////

namespace Freestyle {

namespace Functions0D {

// DensityF0D
/*! Returns the density of the (result) image evaluated at an Interface0D.
 *  This density is evaluated using a pixels square window around the evaluation point and integrating
 *  these values using a gaussian.
 */
class DensityF0D : public UnaryFunction0D<double>
{
public:
	/*! Builds the functor from the gaussian sigma value.
	 *  \param sigma
	 *    sigma indicates the x value for which the gaussian function is 0.5. It leads to the window size value.
	 *    (the larger, the smoother)
	 */
	DensityF0D(double sigma = 2) : UnaryFunction0D<double>()
	{
		_filter.setSigma((float)sigma);
	}

	/*! Returns the string "DensityF0D" */
	string getName() const
	{
		return "DensityF0D";
	}

	/*! The () operator. */
	int operator()(Interface0DIterator& iter);

private:
	GaussianFilter _filter;
};

// LocalAverageDepthF0D
/*! Returns the average depth around a point.
 *  The result is obtained by querying the depth buffer on a window around that point.
 */
class LocalAverageDepthF0D : public UnaryFunction0D<double>
{
private:
	GaussianFilter _filter;

public:
	/*! Builds the functor from the size of the mask that will be used. */
	LocalAverageDepthF0D(real maskSize = 5.0f) : UnaryFunction0D<double>()
	{
		_filter.setSigma((float)maskSize / 2.0f);
	}

	/*! Returns the string "LocalAverageDepthF0D" */
	string getName() const
	{
		return "LocalAverageDepthF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// ReadMapPixel
/*! Reads a pixel in a map. */
class ReadMapPixelF0D : public UnaryFunction0D<float>
{
private:
	const char * _mapName;
	int _level;

public:
	/*! Builds the functor from name of the 
	 *  Map that must be read.
	 *  \param iMapName
	 *    The name of the map.
	 *  \param level
	 *    The level of the pyramid from which the pixel must be read.
	 */
	ReadMapPixelF0D(const char *iMapName, int level) : UnaryFunction0D<float>()
	{
		_mapName = iMapName;
		_level = level;
	}

	/*! Returns the string "ReadMapPixelF0D" */
	string getName() const
	{
		return "ReadMapPixelF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// ReadSteerableViewMapPixel
/*! Reads a pixel in one of the level of one of the steerable viewmaps. */
class ReadSteerableViewMapPixelF0D : public UnaryFunction0D<float>
{
private:
	unsigned _orientation;
	int _level;

public:
	/*! Builds the functor
	 *  \param nOrientation
	 *    The integer belonging to [0,4] indicating the orientation (E,NE,N,NW) we are interested in.
	 *  \param level
	 *    The level of the pyramid from which the pixel must be read.
	 */
	ReadSteerableViewMapPixelF0D(unsigned nOrientation, int level) : UnaryFunction0D<float>()
	{
		_orientation = nOrientation;
		_level = level;
	}

	/*! Returns the string "ReadSteerableViewMapPixelF0D" */
	string getName() const
	{
		return "ReadSteerableViewMapPixelF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// ReadCompleteViewMapPixel
/*! Reads a pixel in one of the level of the complete viewmap. */
class ReadCompleteViewMapPixelF0D : public UnaryFunction0D<float>
{
private:
	int _level;

public:
	/*! Builds the functor 
	 *  \param level
	 *    The level of the pyramid from which the pixel must be read.
	 */
	ReadCompleteViewMapPixelF0D(int level) : UnaryFunction0D<float>()
	{
		_level = level;
	}

	/*! Returns the string "ReadCompleteViewMapPixelF0D" */
	string getName() const
	{
		return "ReadCompleteViewMapPixelF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

// GetViewMapGradientNormF0D
/*! Returns the norm of the gradient of the global viewmap density image. */
class GetViewMapGradientNormF0D: public UnaryFunction0D< float>
{
private:
	int _level;
	float _step;

public:
	/*! Builds the functor 
	 *  \param level
	 *    The level of the pyramid from which the pixel must be read.
	 */
	GetViewMapGradientNormF0D(int level) : UnaryFunction0D<float>()
	{
		_level = level;
		_step = (float)pow(2.0, _level);
	}

	/*! Returns the string "GetOccludeeF0D" */
	string getName() const
	{
		return "GetViewMapGradientNormF0D";
	}

	/*! the () operator. */
	int operator()(Interface0DIterator& iter);
};

} // end of namespace Functions0D

} /* namespace Freestyle */

#endif // __FREESTYLE_ADVANCED_FUNCTIONS_0D_H__

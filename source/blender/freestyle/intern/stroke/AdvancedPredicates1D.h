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

#ifndef __FREESTYLE_ADVANCED_PREDICATES_1D_H__
#define __FREESTYLE_ADVANCED_PREDICATES_1D_H__

/** \file blender/freestyle/intern/stroke/AdvancedPredicates1D.h
 *  \ingroup freestyle
 *  \brief Class gathering stroke creation algorithms
 *  \author Stephane Grabli
 *  \date 01/07/2003
 */

#include <string>

#include "AdvancedFunctions1D.h"
#include "Predicates1D.h"

#include "../view_map/Interface1D.h"

//
// Predicates definitions
//
///////////////////////////////////////////////////////////

namespace Freestyle {

namespace Predicates1D {

// DensityLowerThanUP1D
/*! Returns true if the density evaluated for the
*  Interface1D is less than a user-defined density value.
*/
class DensityLowerThanUP1D : public UnaryPredicate1D
{
public:
	/*! Builds the functor.
	 *  \param threshold
	 *    The value of the threshold density.
	 *    Any Interface1D having a density lower than this threshold will match.
	 *  \param sigma
	 *    The sigma value defining the density evaluation window size used in the DensityF0D functor.
	 */
	DensityLowerThanUP1D(double threshold, double sigma = 2)
	{
		_threshold = threshold;
		_sigma = sigma;
	}

	/*! Returns the string "DensityLowerThanUP1D" */
	string getName() const
	{
		return "DensityLowerThanUP1D";
	}

	/*! The () operator. */
	int operator()(Interface1D& inter)
	{
		Functions1D::DensityF1D fun(_sigma);
		if (fun(inter) < 0)
			return -1;
		result = (fun.result < _threshold);
		return 0;
	}

private:
	double _sigma;
	double _threshold;
};

} // end of namespace Predicates1D

} /* namespace Freestyle */

#endif // __FREESTYLE_ADVANCED_PREDICATES_1D_H__

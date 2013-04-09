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
 * The Original Code is:
 *   OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *   Copyright (C) 2000 Bruno Levy
 *   Contact: Bruno Levy
 *      levy@loria.fr
 *      ISA Project
 *      LORIA, INRIA Lorraine, 
 *      Campus Scientifique, BP 239
 *      54506 VANDOEUVRE LES NANCY CEDEX 
 *      FRANCE
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/geometry/normal_cycle.cpp
 *  \ingroup freestyle
 *  \author Bruno Levy
 */

#include "matrix_util.h"
#include "normal_cycle.h"

namespace Freestyle {

namespace OGF {

//_________________________________________________________


NormalCycle::NormalCycle()
{
}

void NormalCycle::begin()
{
	M_[0] = M_[1] = M_[2] = M_[3] = M_[4] = M_[5] = 0;
}

void NormalCycle::end()
{
	double eigen_vectors[9];
	MatrixUtil::semi_definite_symmetric_eigen(M_, 3, eigen_vectors, eigen_value_);

	axis_[0] = Vec3r(eigen_vectors[0], eigen_vectors[1], eigen_vectors[2]);

	axis_[1] = Vec3r(eigen_vectors[3], eigen_vectors[4], eigen_vectors[5]);

	axis_[2] = Vec3r(eigen_vectors[6], eigen_vectors[7], eigen_vectors[8]);

	// Normalize the eigen vectors
	for (int i = 0; i < 3; i++) {
		axis_[i].normalize();
	}

	// Sort the eigen vectors
	i_[0] = 0;
	i_[1] = 1;
	i_[2] = 2;

	double l0 = ::fabs(eigen_value_[0]);
	double l1 = ::fabs(eigen_value_[1]);
	double l2 = ::fabs(eigen_value_[2]);

	if (l1 > l0) {
		ogf_swap(l0,    l1   );
		ogf_swap(i_[0], i_[1]);
	}
	if (l2 > l1) {
		ogf_swap(l1,    l2   );
		ogf_swap(i_[1], i_[2]);
	}
	if (l1 > l0) {
		ogf_swap(l0,    l1   );
		ogf_swap(i_[0], i_[1]);
	}
}

//_________________________________________________________

}  // OGF namespace

} /* namespace Freestyle */

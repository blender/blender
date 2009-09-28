// Copyright  (C)  2007  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>

// Version: 1.0
// Author: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Maintainer: Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// URL: http://www.orocos.org/kdl

// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.

// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

#ifndef KDLINERTIA_HPP
#define KDLINERTIA_HPP

#include <Eigen/Array>
#include "frames.hpp"

namespace KDL {

using namespace Eigen;

/**
 *	This class offers the inertia-structure of a body
 *	An inertia is defined in a certain reference point and a certain reference base.
 *	The reference point does not have to coincide with the origin of the reference frame.
 */
class Inertia{
public:

	/**
	 * 	This constructor creates a cartesian space inertia matrix,
	 * 	the arguments are the mass and the inertia moments in the cog.
	 */
	Inertia(double m=0,double Ixx=0,double Iyy=0,double Izz=0,double Ixy=0,double Ixz=0,double Iyz=0);

    static inline Inertia Zero(){
        return Inertia(0,0,0,0,0,0,0);
    };

    friend class Rotation;
    friend class Frame;

    /**
     * F = m*a
     */
   // Wrench operator* (const AccelerationTwist& acc);


    ~Inertia();
private:
    Matrix<double,6,6,RowMajor> data;

};




}

#endif

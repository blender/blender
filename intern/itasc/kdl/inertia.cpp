/** \file itasc/kdl/inertia.cpp
 *  \ingroup itasc
 */
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

#include "inertia.hpp"

#include <Eigen/Core>

namespace KDL {
using namespace Eigen;

Inertia::Inertia(double m,double Ixx,double Iyy,double Izz,double Ixy,double Ixz,double Iyz):
data(Matrix<double,6,6>::Zero())
{
    data(0,0)=Ixx;
    data(1,1)=Iyy;
    data(2,2)=Izz;
    data(2,1)=data(1,2)=Ixy;
    data(3,1)=data(1,3)=Ixz;
    data(3,2)=data(2,3)=Iyz;

	data.block(3,3,3,3)=m*Matrix<double,3,3>::Identity();
}

Inertia::~Inertia()
{
}



}

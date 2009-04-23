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

#ifndef KDL_JACOBIAN_HPP
#define KDL_JACOBIAN_HPP

#include "frames.hpp"

namespace KDL
{
    //Forward declaration
    class ChainJntToJacSolver;

    class Jacobian
    {
        friend class ChainJntToJacSolver;
    private:
        unsigned int size;
        unsigned int nr_blocks;
    public:
        Twist* twists;
        Jacobian(unsigned int size,unsigned int nr=1);
        Jacobian(const Jacobian& arg);

        Jacobian& operator=(const Jacobian& arg);

        bool operator ==(const Jacobian& arg);
        bool operator !=(const Jacobian& arg);
        
        friend bool Equal(const Jacobian& a,const Jacobian& b,double eps=epsilon);
        

        ~Jacobian();

        double operator()(int i,int j)const;
        double& operator()(int i,int j);
        unsigned int rows()const;
        unsigned int columns()const;

        friend void SetToZero(Jacobian& jac);

        friend void changeRefPoint(const Jacobian& src1, const Vector& base_AB, Jacobian& dest);
        friend void changeBase(const Jacobian& src1, const Rotation& rot, Jacobian& dest);
        friend void changeRefFrame(const Jacobian& src1,const Frame& frame, Jacobian& dest);


    };
}

#endif

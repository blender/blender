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

#include "chainjnttojacsolver.hpp"

namespace KDL
{
    ChainJntToJacSolver::ChainJntToJacSolver(const Chain& _chain):
        chain(_chain)
    {
    }

    ChainJntToJacSolver::~ChainJntToJacSolver()
    {
    }

    int ChainJntToJacSolver::JntToJac(const JntArray& q_in,Jacobian& jac)
    {
        assert(q_in.rows()==chain.getNrOfJoints()&&
               q_in.rows()==jac.columns());
        T_tmp = Frame::Identity();
        SetToZero(t_tmp);
        int j=0;
        Frame total;
        for (unsigned int i=0;i<chain.getNrOfSegments();i++) {
            //Calculate new Frame_base_ee
            if(chain.getSegment(i).getJoint().getType()!=Joint::None){
            	//pose of the new end-point expressed in the base
                total = T_tmp*chain.getSegment(i).pose(q_in(j));
                //changing base of new segment's twist to base frame
                //t_tmp = T_tmp.M*chain.getSegment(i).twist(1.0);
                t_tmp = T_tmp.M*chain.getSegment(i).twist(q_in(j),1.0);
            }else{
                total = T_tmp*chain.getSegment(i).pose(0.0);

            }

            //Changing Refpoint of all columns to new ee
            changeRefPoint(jac,total.p-T_tmp.p,jac);

            //Only increase jointnr if the segment has a joint
            if(chain.getSegment(i).getJoint().getType()!=Joint::None){
                jac.twists[j] = t_tmp;
                j++;
            }

            T_tmp = total;
        }
        return 0;
    }
}


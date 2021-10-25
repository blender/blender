/** \file itasc/kdl/chainjnttojacsolver.cpp
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


		Frame T_local, T_joint;
        T_total = Frame::Identity();
        SetToZero(t_local);

		int i=chain.getNrOfSegments()-1;
		unsigned int q_nr = chain.getNrOfJoints();

		//Lets recursively iterate until we are in the root segment
		while (i >= 0) {
			const Segment& segment = chain.getSegment(i);
			int ndof = segment.getJoint().getNDof();
			q_nr -= ndof;

	        //get the pose of the joint.
			T_joint = segment.getJoint().pose(((JntArray&)q_in)(q_nr));
			// combine with the tip to have the tip pose
			T_local = T_joint*segment.getFrameToTip();
			//calculate new T_end:
			T_total = T_local * T_total;

			for (int dof=0; dof<ndof; dof++) {
				// combine joint rotation with tip position to get a reference frame for the joint
				T_joint.p = T_local.p;
				// in which the twist can be computed (needed for NDof joint)
				t_local = segment.twist(T_joint, 1.0, dof);
				//transform the endpoint of the local twist to the global endpoint:
				t_local = t_local.RefPoint(T_total.p - T_local.p);
				//transform the base of the twist to the endpoint
				t_local = T_total.M.Inverse(t_local);
				//store the twist in the jacobian:
				jac.twists[q_nr+dof] = t_local;
			}
			i--;
		}//endwhile
		//Change the base of the complete jacobian from the endpoint to the base
		changeBase(jac, T_total.M, jac);
        return 0;
    }
}


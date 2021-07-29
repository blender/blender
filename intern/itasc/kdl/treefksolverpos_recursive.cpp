/** \file itasc/kdl/treefksolverpos_recursive.cpp
 *  \ingroup itasc
 */
// Copyright  (C)  2007  Ruben Smits <ruben dot smits at mech dot kuleuven dot be>
// Copyright  (C)  2008 Julia Jesse

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

#include "treefksolverpos_recursive.hpp"
#include <iostream>

namespace KDL {

    TreeFkSolverPos_recursive::TreeFkSolverPos_recursive(const Tree& _tree):
        tree(_tree)
    {
    }

    int TreeFkSolverPos_recursive::JntToCart(const JntArray& q_in, Frame& p_out, const std::string& segmentName, const std::string& baseName)
    {      
		SegmentMap::value_type const* it = tree.getSegmentPtr(segmentName); 
		SegmentMap::value_type const* baseit = tree.getSegmentPtr(baseName); 
        
        if(q_in.rows() != tree.getNrOfJoints())
    	    	return -1;
        else if(!it) //if the segment name is not found
         	return -2;
        else if(!baseit) //if the base segment name is not found
         	return -3;
        else{
			p_out = recursiveFk(q_in, it, baseit);
        	return 0;
        }
    }

	Frame TreeFkSolverPos_recursive::recursiveFk(const JntArray& q_in, SegmentMap::value_type const* it, SegmentMap::value_type const* baseit)
	{
		//gets the frame for the current element (segment)
		const TreeElement& currentElement = it->second;
		
		if(it == baseit){
			return KDL::Frame::Identity();
		}
		else{
			Frame currentFrame = currentElement.segment.pose(((JntArray&)q_in)(currentElement.q_nr));
			return recursiveFk(q_in, currentElement.parent, baseit) * currentFrame;
		}
	}

    TreeFkSolverPos_recursive::~TreeFkSolverPos_recursive()
    {
    }


}

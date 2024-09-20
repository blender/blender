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


#ifndef KDL_SEGMENT_HPP
#define KDL_SEGMENT_HPP

#include "frames.hpp"
#include "inertia.hpp"
#include "joint.hpp"
#include <vector>

namespace KDL {

    /**
	  * \brief This class encapsulates a simple segment, that is a "rigid
	  * body" (i.e., a frame and an inertia) with a joint and with
	  * "handles", root and tip to connect to other segments.
     *
     * A simple segment is described by the following properties :
     *      - Joint
     *      - inertia: of the rigid body part of the Segment
     *      - Offset from the end of the joint to the tip of the segment:
     *        the joint is located at the root of the segment.
     *
     * @ingroup KinematicFamily
     */
    class Segment {
        friend class Chain;
    private:
        Inertia M;
        Joint joint;
        Frame f_tip;

    public:
        /**
         * Constructor of the segment
         *
         * @param joint joint of the segment, default:
         * Joint(Joint::None)
         * @param f_tip frame from the end of the joint to the tip of
         * the segment, default: Frame::Identity()
         * @param M rigid body inertia of the segment, default: Inertia::Zero()
         */
        Segment(const Joint& joint=Joint(), const Frame& f_tip=Frame::Identity(),const Inertia& M = Inertia::Zero());
        Segment(const Segment& in);
        Segment& operator=(const Segment& arg);

        virtual ~Segment();

        /**
         * Request the pose of the segment, given the joint position q.
         *
         * @param q 1D position of the joint
         *
         * @return pose from the root to the tip of the segment
         */
        Frame pose(const double* q)const;
        /**
         * Request the 6D-velocity of the tip of the segment, given
         * the joint position q and the joint velocity qdot.
         *
         * @param q ND position of the joint
         * @param qdot ND velocity of the joint
         *
         * @return 6D-velocity of the tip of the segment, expressed
         *in the base-frame of the segment(root) and with the tip of
         *the segment as reference point.
         */
        Twist twist(const double* q,const double& qdot, int dof=0)const;

        /**
         * Request the 6D-velocity at a given point p, relative to base frame of the segment
         * givven the joint velocity qdot.
         *
         * @param p reference point
         * @param qdot ND velocity of the joint
         *
         * @return 6D-velocity at a given point p, expressed
         * in the base-frame of the segment(root) 
         */
	    Twist twist(const Vector& p, const double& qdot, int dof=0)const;

        /**
         * Request the 6D-velocity at a given frame origin, relative to base frame of the segment
         * assuming the frame rotation is the rotation of the joint.
         *
         * @param f joint pose frame + reference point
         * @param qdot ND velocity of the joint
         *
         * @return 6D-velocity at frame reference point, expressed
         * in the base-frame of the segment(root) 
         */
		Twist twist(const Frame& f, const double& qdot, int dof)const;

        /**
         * Request the joint of the segment
         *
         *
         * @return const reference to the joint of the segment
         */
        const Joint& getJoint()const
        {
            return joint;
        }
        /**
         * Request the inertia of the segment
         *
         *
         * @return const reference to the inertia of the segment
         */
        const Inertia& getInertia()const
        {
            return M;
        }

        /**
         * Request the pose from the joint end to the tip of the
         *segment.
         *
         * @return const reference to the joint end - segment tip pose.
         */
        const Frame& getFrameToTip()const
        {
            return f_tip;
        }

    };
}//end of namespace KDL

#endif

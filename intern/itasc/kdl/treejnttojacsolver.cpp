/** \file itasc/kdl/treejnttojacsolver.cpp
 *  \ingroup itasc
 */
/*
 * TreeJntToJacSolver.cpp
 *
 *  Created on: Nov 27, 2008
 *      Author: rubensmits
 */

#include "treejnttojacsolver.hpp"
#include <iostream>

namespace KDL {

TreeJntToJacSolver::TreeJntToJacSolver(const Tree& tree_in) :
    tree(tree_in) {
}

TreeJntToJacSolver::~TreeJntToJacSolver() {
}

int TreeJntToJacSolver::JntToJac(const JntArray& q_in, Jacobian& jac,
        const std::string& segmentname) {
    //First we check all the sizes:
    if (q_in.rows() != tree.getNrOfJoints() || jac.columns()
            != tree.getNrOfJoints())
        return -1;

    //Lets search the tree-element
    SegmentMap::value_type const* it = tree.getSegmentPtr(segmentname);

    //If segmentname is not inside the tree, back out:
    if (!it)
        return -2;

    //Let's make the jacobian zero:
    SetToZero(jac);

    SegmentMap::value_type const* root = tree.getSegmentPtr("root");

    Frame T_total = Frame::Identity();
	Frame T_local, T_joint;
	Twist t_local;
    //Lets recursively iterate until we are in the root segment
    while (it != root) {
        //get the corresponding q_nr for this TreeElement:
        unsigned int q_nr = it->second.q_nr;

        //get the pose of the joint.
		T_joint = it->second.segment.getJoint().pose(((JntArray&)q_in)(q_nr));
		// combine with the tip to have the tip pose
		T_local = T_joint*it->second.segment.getFrameToTip();
        //calculate new T_end:
        T_total = T_local * T_total;

        //get the twist of the segment:
		int ndof = it->second.segment.getJoint().getNDof();
		for (int dof=0; dof<ndof; dof++) {
			// combine joint rotation with tip position to get a reference frame for the joint
			T_joint.p = T_local.p;
			// in which the twist can be computed (needed for NDof joint)
            t_local = it->second.segment.twist(T_joint, 1.0, dof);
            //transform the endpoint of the local twist to the global endpoint:
            t_local = t_local.RefPoint(T_total.p - T_local.p);
            //transform the base of the twist to the endpoint
            t_local = T_total.M.Inverse(t_local);
            //store the twist in the jacobian:
            jac.twists[q_nr+dof] = t_local;
        }
        //goto the parent
        it = it->second.parent;
    }//endwhile
    //Change the base of the complete jacobian from the endpoint to the base
    changeBase(jac, T_total.M, jac);

    return 0;

}//end JntToJac
}//end namespace


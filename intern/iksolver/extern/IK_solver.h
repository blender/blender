/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 *
 * @author Laurence
 * @mainpage IK - Blender inverse kinematics module.
 *
 * @section about About the IK module
 *
 * This module allows you to create segments and form them into 
 * chains. You can then define a goal point that the end of the 
 * chain should attempt to reach - an inverse kinematic problem.
 * This module will then modify the segments in the chain in
 * order to get the end of the chain as near as possible to the 
 * goal. This solver uses an inverse jacobian method to find
 * a solution.
 * 
 * @section issues Known issues with this IK solver.
 *
 * - The current solver works with only one type of segment. These
 * segments always have 3 degress of freedom (DOF). i.e. the solver
 * uses all these degrees to solve the IK problem. It would be 
 * nice to allow the user to specify different segment types such
 * as 1 DOF joints in a given plane. 2 DOF joints about given axis.
 * - There is currently no support for joint constraints in the
 * solver. This is within the realms of possibility - please ask
 * if this functionality is required.
 * - The solver is slow, inverse jacobian methods in general give
 * 'smooth' solutions and the method is also very flexible, it 
 * does not rely on specific angle parameterization and can be 
 * extended to deal with different joint types and joint 
 * constraints. However it is not suitable for real time use. 
 * Other algorithms exist which are more suitable for real-time
 * applications, please ask if this functionality is required.     
 * 
 * @section dependencies Dependencies
 * 
 * This module only depends on Moto.
 */

#ifndef NAN_INCLUDED_IK_solver_h
#define NAN_INCLUDED_IK_solver_h

#ifdef __cplusplus
extern "C" {
#endif

/** 
 * External segment structure
 */


/**
 * This structure defines a single segment of an IK chain. 
 * - Individual segments are always defined in local coordinates.
 * - The segment is assumed to be oriented in  the local 
 * y-direction.
 * - seg_start is the start of the segment relative to the end 
 * of the parent segment.
 * - basis is a column major matrix defining the rest position 
 * of the bone.
 * - length is the simply the length of the bone.  
 * - basis_change is a 3x3 matrix representing the change 
 * from the rest position of the segment to the solved position.
 * In fact it is the transpose of this matrix because blender 
 * does something weird with quaternion conversion. This is 
 * strictly an ouput variable for returning the results of an
 * an ik solve back to you.
 * The local transformation specified as a column major matrix 
 * of a segment is then defined as.
 * translate(seg_start)*basis*transpose(basis_change)*translate(0,length,0) 
 */

typedef struct IK_Segment_Extern {
	float seg_start[3];
	float basis[9];
	float length;
	float basis_change[9];
} IK_Segment_Extern;

typedef IK_Segment_Extern* IK_Segment_ExternPtr; 

/** 
 * External chain structure.
 * This structure is filled when you call IK_LoadChain.
 * The first segment in the chain is the root segment.
 * The end of the last segment is the end-effector of the chain 
 * this is the point that tries to move to the goal in the ik 
 * solver. 
 * - num_segments is the number of segments in the array pointed
 * to by the member segments.
 * - chain_dof is the number of degrees of freedom of the chain
 * that is the number of independent ways the chain can be changed
 * to reach the goal. 
 * - segments points to an array of IK_Segment_Extern structs
 * containing the segments of this chain. 
 * - intern is pointer used by the module to store information
 * about the chain. Please do not touch the member in any way.   
 */

typedef struct IK_Chain_Extern {
	int num_segments;
	int chain_dof;
	IK_Segment_ExternPtr segments;
	void * intern;
} IK_Chain_Extern; 

typedef IK_Chain_Extern* IK_Chain_ExternPtr;


/**
 * Create a clean chain structure. 
 * @return A IK_Chain_Extern structure allocated on the heap.
 * Do not attempt to delete or free this memory yourself please 
 * use the FreeChain(...) function for this.
 */

extern IK_Chain_ExternPtr IK_CreateChain(void);

/**
 * Copy segment information into the chain structure.
 * @param chain A chain to load the segments into.
 * @param segments a ptr to an array of IK_Input_Segment_Extern structures
 * @param num_segs the number of segments to load into the chain
 * @return 1 if the chain was correctly loaded into the structure. 
 * @return 0 if an error occured loading the chain. This will normally
 * occur when there is not enough memory to allocate internal chain data.
 * In this case you should not use the chain structure and should call
 * IK_FreeChain to free the memory associated with the chain.
 */

extern int IK_LoadChain(IK_Chain_ExternPtr chain,IK_Segment_ExternPtr segments, int num_segs);

/**
 * Compute the solution of an inverse kinematic problem.
 * @param chain a ptr to an IK_Segment_Extern loaded with the segments
 * to solve for.
 * @param goal the goal of the IK problem
 * @param tolerance .The distance to the solution within which the chain is deemed 
 * to be solved.
 * @param max_iterations. The maximum number of iterations to use in solving the 
 * problem. 
 * @param max_angle_change. The maximum allowed angular change. 0.1 is a good value here.
 * @param output. Results of the solution are written to the segments pointed to by output.
 * Only the basis and basis_change fields are written. You must make sure that you have
 * allocated enough room for the output segments.
 * @return 0 if the solved chain did not reach the goal. This occurs when the
 * goal was unreachable by the chain end effector.
 * @return 1 if the chain reached the goal.
 */ 

extern int IK_SolveChain(
	IK_Chain_ExternPtr chain,
	float goal[3],
	float tolerance,
	int max_iterations,
	float max_angle_change, 
	IK_Segment_ExternPtr output
);

/** 
 * Free a chain and all it's internal memory.
 */

extern void IK_FreeChain(IK_Chain_ExternPtr);


#ifdef __cplusplus
}
#endif

#endif // NAN_INCLUDED_IK_solver_h


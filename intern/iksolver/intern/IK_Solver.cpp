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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define IK_USE_EXPMAP



#ifdef IK_USE_EXPMAP 
#	include "IK_QSolver_Class.h"
#else 
#	include "IK_Solver_Class.h"
#endif
#include "../extern/IK_solver.h"

#include <iostream>

	IK_Chain_ExternPtr  
IK_CreateChain(
	void
) {

	MEM_SmartPtr<IK_Chain_Extern> output (new IK_Chain_Extern);
	MEM_SmartPtr<IK_QSolver_Class> output_void (IK_QSolver_Class::New()); 	
	

	if (output == NULL || output_void == NULL) {
		return NULL;
	}

	output->chain_dof = 0;
	output->num_segments = 0;
	output->segments = NULL;
	output->intern = output_void.Release();
	return output.Release();
};


	int 
IK_LoadChain(
	IK_Chain_ExternPtr chain,
	IK_Segment_ExternPtr segments,
	int num_segs
) {

	if (chain == NULL || segments == NULL) return 0;

	IK_QSolver_Class *intern_cpp = (IK_QSolver_Class *)chain->intern;
	if (intern_cpp == NULL) return 0;

	std::vector<IK_QSegment> & segs = intern_cpp->Chain().Segments();	
	if (segs.size() != num_segs) {
		segs = std::vector<IK_QSegment>(num_segs);
	}

	std::vector<IK_QSegment>::const_iterator seg_end= segs.end();
	std::vector<IK_QSegment>::iterator seg_begin= segs.begin();

	IK_Segment_ExternPtr extern_seg_it = segments;
	

	for (;seg_begin != seg_end; ++seg_begin,++extern_seg_it) {
	
		MT_Point3 tr1(extern_seg_it->seg_start);

		MT_Matrix3x3 A(
			extern_seg_it->basis[0],extern_seg_it->basis[1],extern_seg_it->basis[2],
			extern_seg_it->basis[3],extern_seg_it->basis[4],extern_seg_it->basis[5],
			extern_seg_it->basis[6],extern_seg_it->basis[7],extern_seg_it->basis[8]
		);

		MT_Scalar length(extern_seg_it->length);


		*seg_begin = IK_QSegment( 
			tr1,A,length,MT_Vector3(0,0,0)
		);

	}


	intern_cpp->Chain().UpdateGlobalTransformations();
	intern_cpp->Chain().ComputeJacobian();
	
	chain->num_segments = num_segs;
	chain->chain_dof = intern_cpp->Chain().DoF();
	chain->segments = segments;

	return 1;
};		
		
	int 
IK_SolveChain(
	IK_Chain_ExternPtr chain,
	float goal[3],
	float tolerance,
	int max_iterations,
	float max_angle_change, 
	IK_Segment_ExternPtr output
){
	if (chain == NULL || output == NULL) return 0;
	if (chain->intern == NULL) return 0;

	IK_QSolver_Class *intern_cpp = (IK_QSolver_Class *)chain->intern;

	IK_QJacobianSolver & solver = intern_cpp->Solver();

	bool solve_result = solver.Solve(
		intern_cpp->Chain(),
		MT_Vector3(goal),
		MT_Vector3(0,0,0),
		MT_Scalar(tolerance),
		max_iterations,
		MT_Scalar(max_angle_change)	
	);
	
	// turn the computed role->pitch->yaw into a quaternion and 
	// return the result in output
	
	std::vector<IK_QSegment> & segs = intern_cpp->Chain().Segments();	
	std::vector<IK_QSegment>::const_iterator seg_end= segs.end();
	std::vector<IK_QSegment>::iterator seg_begin= segs.begin();

	for (;seg_begin != seg_end; ++seg_begin, ++output) {
		MT_Matrix3x3 qrot = seg_begin->ExpMap().getMatrix();

		// don't forget to transpose this qrot for use by blender!

		qrot.transpose(); // blender uses transpose here ????

		output->basis_change[0] = float(qrot[0][0]);
		output->basis_change[1] = float(qrot[0][1]);
		output->basis_change[2] = float(qrot[0][2]);
		output->basis_change[3] = float(qrot[1][0]);
		output->basis_change[4] = float(qrot[1][1]);
		output->basis_change[5] = float(qrot[1][2]);
		output->basis_change[6] = float(qrot[2][0]);
		output->basis_change[7] = float(qrot[2][1]);
		output->basis_change[8] = float(qrot[2][2]);

	}


	return solve_result ? 1 : 0;
}

	void 
IK_FreeChain(
	IK_Chain_ExternPtr chain
){
	IK_QSolver_Class *intern_cpp = (IK_QSolver_Class *)chain->intern;

	delete(intern_cpp);
	delete(chain);

}	





	
	


	




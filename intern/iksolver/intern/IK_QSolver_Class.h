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
 */

#ifndef NAN_INCLUDED_IK_Solver_Class
#define NAN_INCLUDED_IK_Solver_Class 

#include "IK_QChain.h"
#include "IK_QJacobianSolver.h"
#include "IK_QSegment.h"
#include "MEM_SmartPtr.h"

/**
 * This class just contains all instances of internal data 
 * associated with the external chain structure needed for 
 * an ik solve. A pointer to this class gets hidden in the 
 * external structure as a void pointer.
 */

class IK_QSolver_Class {

public :

	static 
		IK_QSolver_Class *
	New(
	){
		MEM_SmartPtr<IK_QSolver_Class> output (new IK_QSolver_Class);
	
		MEM_SmartPtr<IK_QJacobianSolver> solver (IK_QJacobianSolver::New());

		if (output == NULL ||
			solver == NULL
		) {
			return NULL;
		}

		output->m_solver = solver.Release();
	
		return output.Release();
	};
	
		IK_QChain &	
	Chain(
	) {
		return m_chain;
	};
		
		IK_QJacobianSolver &
	Solver(
	) {
		return m_solver.Ref();
	}

	~IK_QSolver_Class(
	) {
		// nothing to do
	}


private :

	IK_QSolver_Class(
	) {
	};

	IK_QChain m_chain;
	MEM_SmartPtr<IK_QJacobianSolver> m_solver;

};	

#endif


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

#ifndef NAN_INCLUDED_IK_Solver_Class

#define NAN_INCLUDED_IK_Solver_Class 

#include "IK_Chain.h"
#include "IK_JacobianSolver.h"
#include "IK_Segment.h"
#include "MEM_SmartPtr.h"

class IK_Solver_Class {

public :

	static 
		IK_Solver_Class *
	New(
	){
		MEM_SmartPtr<IK_Solver_Class> output (new IK_Solver_Class);
	
		MEM_SmartPtr<IK_JacobianSolver> solver (IK_JacobianSolver::New());

		if (output == NULL ||
			solver == NULL
		) {
			return NULL;
		}

		output->m_solver = solver.Release();
	
		return output.Release();
	};
	
		IK_Chain &	
	Chain(
	) {
		return m_chain;
	};
		
		IK_JacobianSolver &
	Solver(
	) {
		return m_solver.Ref();
	}

	~IK_Solver_Class(
	) {
		// nothing to do
	}


private :

	IK_Solver_Class(
	) {
	};

	IK_Chain m_chain;
	MEM_SmartPtr<IK_JacobianSolver> m_solver;

};	

#endif

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

#ifndef CGChainSolver_h

#define CGChainSolver_h

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */

#include "IK_ConjugateGradientSolver.h"
#include "IK_Chain.h"
#include "MT_Scalar.h"
#include "TNT/vec.h"
#include "MEM_SmartPtr.h"


/**
 * This class is a concrete differentiable potenial function for
 * an IK_Chain representing the distance to the goal. 
 * @warning This method of solving IK problems is not as good
 * as IK_JacobianSolver. I advise you to use that class instead.
 */

class ChainPotential : 
public DifferentiablePotenialFunctionNd 
{
public :

	static 
		ChainPotential *
	New(
		IK_Chain &chain
	);
	
	// End effector goal

		void
	SetGoal(
		const MT_Vector3 goal
	);

	// Inherited from DifferentiablePotenialFunctionNd
	//////////////////////////////////////////////////

		MT_Scalar 
	Evaluate(
		const MT_Scalar x
	);

		MT_Scalar
	Derivative(
		const MT_Scalar x
	);

		MT_Scalar
	Evaluate(
		const TNT::Vector<MT_Scalar> &x
	);

		void
	Derivative(
		const TNT::Vector<MT_Scalar> &x,
		TNT::Vector<MT_Scalar> &dy
	);

	// return the dimension of the domain of the potenial
	// function

		int
	Dimension(
	) const {
		return m_dimension;
	}
	
	~ChainPotential(
	){
	};
	
private :

		MT_Scalar
	DistancePotential(
		MT_Vector3 pos,
		MT_Vector3 goal
	) const;

		void
	DistanceGradient(
		MT_Vector3 pos,
		MT_Vector3 goal
	);

	ChainPotential(
		IK_Chain & chain
	) :
		DifferentiablePotenialFunctionNd(),
		m_chain(chain),
		m_t_chain(chain),
		m_dimension (chain.Segments().size())
	{
	};

	MT_Vector3 m_goal;
	TNT::Vector<MT_Scalar> m_distance_grad;
	TNT::Vector<MT_Scalar> m_angle_grad;
	TNT::Vector<MT_Scalar> m_temp_pos;
	TNT::Vector<MT_Scalar> m_temp_grad;

	TNT::Vector<MT_Scalar> m_original_pos;
	int m_dimension;

	IK_Chain &m_chain;
	IK_Chain m_t_chain; // deep copy 

};
		

class IK_CGChainSolver : public MEM_NonCopyable
{
public :
	
	
	static
		IK_CGChainSolver *
	New(
	);

		bool
	Solve(
		IK_Chain & chain,
		MT_Vector3 new_position,
		MT_Scalar tolerance
	);

	~IK_CGChainSolver();	

private :

	IK_CGChainSolver(
	);

	MEM_SmartPtr<ChainPotential>  m_potential;
	MEM_SmartPtr<IK_ConjugateGradientSolver> m_grad_solver;
};

#endif


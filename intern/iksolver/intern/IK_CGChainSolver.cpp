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

#include "IK_CGChainSolver.h"
#include "IK_Segment.h"

using namespace std;

	ChainPotential *
ChainPotential::
New(
	IK_Chain &chain
){
	return new ChainPotential(chain);
};



// End effector goal

	void
ChainPotential::
SetGoal(
	const MT_Vector3 goal
){
	m_goal = goal;
};

// Inherited from DifferentiablePotenialFunctionNd
//////////////////////////////////////////////////

	MT_Scalar 
ChainPotential::
Evaluate(
	MT_Scalar x
){

	// evaluate the function at postiion x along the line specified 
	// by the vector pair (m_line_pos,m_line_dir)

	m_temp_pos.newsize(m_line_dir.size());

	TNT::vectorscale(m_temp_pos,m_line_dir,x);
	TNT::vectoradd(m_temp_pos,m_line_pos);

	return Evaluate(m_temp_pos);
}
	
	MT_Scalar
ChainPotential::
Derivative(
	MT_Scalar x
){
	m_temp_pos.newsize(m_line_dir.size());
	m_temp_grad.newsize(m_line_dir.size());

	TNT::vectorscale(m_temp_pos,m_line_dir,x);
	TNT::vectoradd(m_temp_pos,m_line_pos);
	
	Derivative(m_temp_pos,m_temp_grad);

	return TNT::dot_prod(m_temp_grad,m_line_dir);
}
	

	MT_Scalar
ChainPotential::
Evaluate(
	const TNT::Vector<MT_Scalar> &x
){


	// set the vector of angles in the backup chain

	vector<IK_Segment> &segs = m_t_chain.Segments();
	vector<IK_Segment>::iterator seg_it = segs.begin();
	TNT::Vector<MT_Scalar>::const_iterator a_it = x.begin();
#if 0
	TNT::Vector<MT_Scalar>::const_iterator a_end = x.end();
#endif	
	// while we are iterating through the angles and segments
	// we compute the current angle potenial
	MT_Scalar angle_potential = 0;
#if 0
	
	for (; a_it != a_end; ++a_it, ++seg_it) {
		
		MT_Scalar dif = (*a_it - seg_it->CentralAngle());	
		dif *= dif * seg_it->Weight();
		angle_potential += dif;
		seg_it->SetTheta(*a_it);
	}
#endif

	int chain_dof = m_t_chain.DoF();
	int n;

	for (n=0; n < chain_dof;seg_it ++) {
		n += seg_it->SetAngles(a_it + n);
	}

	
	m_t_chain.UpdateGlobalTransformations();
	
	MT_Scalar output = (DistancePotential(m_t_chain.EndEffector(),m_goal) + angle_potential);
	
	return output;
};

	void
ChainPotential::
Derivative(
	const TNT::Vector<MT_Scalar> &x,
	TNT::Vector<MT_Scalar> &dy
){

	m_distance_grad.newsize(3);
	// set the vector of angles in the backup chain

	vector<IK_Segment> & segs = m_t_chain.Segments();
	vector<IK_Segment>::iterator seg_it = segs.begin();
	TNT::Vector<MT_Scalar>::const_iterator a_it = x.begin();
	TNT::Vector<MT_Scalar>::const_iterator a_end = x.end();

	m_angle_grad.newsize(segs.size());
	m_angle_grad = 0;
#if 0
	// FIXME compute angle gradients
	TNT::Vector<MT_Scalar>::iterator ag_it = m_angle_grad.begin();
#endif
	
	const int chain_dof = m_t_chain.DoF();
	for (int n=0; n < chain_dof;seg_it ++) {
		n += seg_it->SetAngles(a_it + n);
	}
	
	m_t_chain.UpdateGlobalTransformations();
	m_t_chain.ComputeJacobian();

	DistanceGradient(m_t_chain.EndEffector(),m_goal);

	// use chain rule for calculating derivative
	// of potenial function
	TNT::matmult(dy,m_t_chain.TransposedJacobian(),m_distance_grad);
#if 0
	TNT::vectoradd(dy,m_angle_grad);
#endif

};


	MT_Scalar
ChainPotential::
DistancePotential(
	MT_Vector3 pos,
	MT_Vector3 goal
) const {
	return (pos - goal).length2();
}

// update m_distance_gradient 

	void
ChainPotential::
DistanceGradient(
	MT_Vector3 pos,
	MT_Vector3 goal
){

	MT_Vector3 output = 2*(pos - goal);
	m_distance_grad.newsize(3);

	m_distance_grad[0] = output.x();
	m_distance_grad[1] = output.y();
	m_distance_grad[2] = output.z();
}


	IK_CGChainSolver *
IK_CGChainSolver::
New(
){
	
	MEM_SmartPtr<IK_CGChainSolver> output (new IK_CGChainSolver());
	MEM_SmartPtr<IK_ConjugateGradientSolver>solver (IK_ConjugateGradientSolver::New());	

	if (output == NULL || solver == NULL) return NULL;

	output->m_grad_solver = solver.Release();
	return output.Release();
};	


	bool
IK_CGChainSolver::
Solve(
	IK_Chain & chain,
	MT_Vector3 new_position,
	MT_Scalar tolerance
){

	// first build a potenial function for the chain

	m_potential = ChainPotential::New(chain);
	if (m_potential == NULL) return false;

	m_potential->SetGoal(new_position);

	// make a TNT::vector to describe the current
	// chain state

	TNT::Vector<MT_Scalar> p;
	p.newsize(chain.DoF());

	TNT::Vector<MT_Scalar>::iterator p_it = p.begin();
	vector<IK_Segment>::const_iterator seg_it = chain.Segments().begin();
	vector<IK_Segment>::const_iterator seg_end = chain.Segments().end();

	for (; seg_it != seg_end; seg_it++) {

		int i;
		int seg_dof = seg_it->DoF();
		for (i=0; i < seg_dof; ++i,++p_it) {
			*p_it = seg_it->ActiveAngle(i);
		}
	}

	// solve the thing
	int iterations(0);
	MT_Scalar return_value(0);

	m_grad_solver->Solve(
		p,
		tolerance,
		iterations,
		return_value,
		m_potential.Ref(),
		100
	);

	// update this chain

	vector<IK_Segment>::iterator out_seg_it = chain.Segments().begin();
	TNT::Vector<MT_Scalar>::const_iterator p_cit = p.begin();

	const int chain_dof = chain.DoF();
	int n;

	for (n=0; n < chain_dof;out_seg_it ++) {
		n += out_seg_it->SetAngles(p_cit + n);
	}
	
	chain.UpdateGlobalTransformations();
	chain.ComputeJacobian();

	return true;
}
	
IK_CGChainSolver::
~IK_CGChainSolver(
){
	//nothing to do
};	


IK_CGChainSolver::
IK_CGChainSolver(
){ 
	//nothing to do;
};


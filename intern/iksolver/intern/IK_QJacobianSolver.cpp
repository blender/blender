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
 * Original Author: Laurence
 * Contributor(s): Brecht
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h>
#include "IK_QJacobianSolver.h"

void IK_QJacobianSolver::AddSegmentList(IK_QSegment *seg)
{
	m_segments.push_back(seg);

	IK_QSegment *child;
	for (child = seg->Child(); child; child = child->Sibling())
		AddSegmentList(child);
}

bool IK_QJacobianSolver::Setup(IK_QSegment *root, std::list<IK_QTask*>& tasks)
{
	m_segments.clear();
	AddSegmentList(root);

	// assing each segment a unique id for the jacobian
	std::vector<IK_QSegment*>::iterator seg;
	int num_dof = 0;

	for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
		(*seg)->SetDoFId(num_dof);
		num_dof += (*seg)->NumberOfDoF();
	}

	if (num_dof == 0)
		return false;

	// compute task id's and assing weights to task
	int primary_size = 0, primary = 0;
	int secondary_size = 0, secondary = 0;
	MT_Scalar primary_weight = 0.0, secondary_weight = 0.0;
	std::list<IK_QTask*>::iterator task;

	for (task = tasks.begin(); task != tasks.end(); task++) {
		IK_QTask *qtask = *task;

		if (qtask->Primary()) {
			qtask->SetId(primary_size);
			primary_size += qtask->Size();
			primary_weight += qtask->Weight();
			primary++;
		}
		else {
			qtask->SetId(secondary_size);
			secondary_size += qtask->Size();
			secondary_weight += qtask->Weight();
			secondary++;
		}
	}

	if (primary_size == 0 || MT_fuzzyZero(primary_weight))
		return false;

	m_secondary_enabled = (secondary > 0);
	
	// rescale weights of tasks to sum up to 1
	MT_Scalar primary_rescale = 1.0/primary_weight;
	MT_Scalar secondary_rescale;
	if (MT_fuzzyZero(secondary_weight))
		secondary_rescale = 0.0;
	else
		secondary_rescale = 1.0/secondary_weight;
	
	for (task = tasks.begin(); task != tasks.end(); task++) {
		IK_QTask *qtask = *task;

		if (qtask->Primary())
			qtask->SetWeight(qtask->Weight()*primary_rescale);
		else
			qtask->SetWeight(qtask->Weight()*secondary_rescale);
	}

	// set matrix sizes
	m_jacobian.ArmMatrices(num_dof, primary_size, primary);
	if (secondary > 0)
		m_jacobian_sub.ArmMatrices(num_dof, secondary_size, secondary);

	// set dof weights
	int i;

	for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
		for (i = 0; i < (*seg)->NumberOfDoF(); i++)
			m_jacobian.SetDoFWeight((*seg)->DoFId()+i, (*seg)->Weight(i));

	return true;
}

bool IK_QJacobianSolver::UpdateAngles(MT_Scalar& norm)
{
	// assing each segment a unique id for the jacobian
	std::vector<IK_QSegment*>::iterator seg;
	IK_QSegment *qseg, *minseg = NULL;
	MT_Scalar minabsdelta = 1e10, absdelta;
	MT_Vector3 delta, mindelta;
	bool locked = false, clamp[3];
	int i, mindof = 0;

	// here we check if any angle limits were violated. angles whose clamped
	// position is the same as it was before, are locked immediate. of the
	// other violation angles the most violating angle is rememberd
	for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
		qseg = *seg;
		if (qseg->UpdateAngle(m_jacobian, delta, clamp)) {
			for (i = 0; i < qseg->NumberOfDoF(); i++) {
				if (clamp[i] && !qseg->Locked(i)) {
					absdelta = MT_abs(delta[i]);

					if (absdelta < MT_EPSILON) {
						qseg->Lock(i, m_jacobian, delta);
						locked = true;
					}
					else if (absdelta < minabsdelta) {
						minabsdelta = absdelta;
						mindelta = delta;
						minseg = qseg;
						mindof = i;
					}
				}
			}
		}
	}

	// lock most violating angle
	if (minseg) {
		minseg->Lock(mindof, m_jacobian, mindelta);
		locked = true;

		if (minabsdelta > norm)
			norm = minabsdelta;
	}

	if (locked == false)
		// no locking done, last inner iteration, apply the angles
		for (seg = m_segments.begin(); seg != m_segments.end(); seg++) {
			(*seg)->UnLock();
			(*seg)->UpdateAngleApply();
		}
	
	// signal if another inner iteration is needed
	return locked;
}

bool IK_QJacobianSolver::Solve(
	IK_QSegment *root,
	std::list<IK_QTask*> tasks,
	const MT_Scalar,
	const int max_iterations
)
{
	//double dt = analyze_time();

	if (!Setup(root, tasks))
		return false;

	// iterate
	for (int iterations = 0; iterations < max_iterations; iterations++) {
		// update transform
		root->UpdateTransform(MT_Transform::Identity());

		std::list<IK_QTask*>::iterator task;

		// compute jacobian
		for (task = tasks.begin(); task != tasks.end(); task++) {
			if ((*task)->Primary())
				(*task)->ComputeJacobian(m_jacobian);
			else
				(*task)->ComputeJacobian(m_jacobian_sub);
		}

		MT_Scalar norm = 0.0;

		do {
			// invert jacobian
			try {
				m_jacobian.Invert();
				if (m_secondary_enabled)
					m_jacobian.SubTask(m_jacobian_sub);
			}
			catch (...) {
				printf("IK Exception\n");
				return false;
			}

			// update angles and check limits
		} while (UpdateAngles(norm));

		// unlock segments again after locking in clamping loop
		std::vector<IK_QSegment*>::iterator seg;
		for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
			(*seg)->UnLock();

		// compute angle update norm
		MT_Scalar maxnorm = m_jacobian.AngleUpdateNorm();
		if (maxnorm > norm)
			norm = maxnorm;

		// check for convergence
		if (norm < 1e-3) {
			//analyze_add_run(iterations, analyze_time()-dt);
			return true;
		}
	}

	//analyze_add_run(max_iterations, analyze_time()-dt);
	return false;
}


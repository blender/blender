/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include "IK_QJacobianSolver.h"
#include "MT_Quaternion.h"

//#include "analyze.h"
IK_QJacobianSolver::IK_QJacobianSolver()
{
	m_poleconstraint = false;
	m_getpoleangle = false;
	m_rootmatrix.setIdentity();
}

MT_Scalar IK_QJacobianSolver::ComputeScale()
{
	std::vector<IK_QSegment*>::iterator seg;
	float length = 0.0f;

	for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
		length += (*seg)->MaxExtension();
	
	if(length == 0.0f)
		return 1.0f;
	else
		return 1.0f/length;
}

void IK_QJacobianSolver::Scale(float scale, std::list<IK_QTask*>& tasks)
{
	std::list<IK_QTask*>::iterator task;
	std::vector<IK_QSegment*>::iterator seg;

	for (task = tasks.begin(); task != tasks.end(); task++)
		(*task)->Scale(scale);

	for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
		(*seg)->Scale(scale);
	
	m_rootmatrix.getOrigin() *= scale;
	m_goal *= scale;
	m_polegoal *= scale;
}

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

	// assign each segment a unique id for the jacobian
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
	m_jacobian.ArmMatrices(num_dof, primary_size);
	if (secondary > 0)
		m_jacobian_sub.ArmMatrices(num_dof, secondary_size);

	// set dof weights
	int i;

	for (seg = m_segments.begin(); seg != m_segments.end(); seg++)
		for (i = 0; i < (*seg)->NumberOfDoF(); i++)
			m_jacobian.SetDoFWeight((*seg)->DoFId()+i, (*seg)->Weight(i));

	return true;
}

void IK_QJacobianSolver::SetPoleVectorConstraint(IK_QSegment *tip, MT_Vector3& goal, MT_Vector3& polegoal, float poleangle, bool getangle)
{
	m_poleconstraint = true;
	m_poletip = tip;
	m_goal = goal;
	m_polegoal = polegoal;
	m_poleangle = (getangle)? 0.0f: poleangle;
	m_getpoleangle = getangle;
}

static MT_Scalar safe_acos(MT_Scalar f)
{
	// acos that does not return NaN with rounding errors
	if (f <= -1.0f) return MT_PI;
	else if (f >= 1.0f) return 0.0;
	else return acos(f);
}

static MT_Vector3 normalize(const MT_Vector3& v)
{
	// a sane normalize function that doesn't give (1, 0, 0) in case
	// of a zero length vector, like MT_Vector3.normalize
	MT_Scalar len = v.length();
	return MT_fuzzyZero(len)?  MT_Vector3(0, 0, 0): v/len;
}

static float angle(const MT_Vector3& v1, const MT_Vector3& v2)
{
	return safe_acos(v1.dot(v2));
}

void IK_QJacobianSolver::ConstrainPoleVector(IK_QSegment *root, std::list<IK_QTask*>& tasks)
{
	// this function will be called before and after solving. calling it before
	// solving gives predictable solutions by rotating towards the solution,
	// and calling it afterwards ensures the solution is exact.

	if(!m_poleconstraint)
		return;
	
	// disable pole vector constraint in case of multiple position tasks
	std::list<IK_QTask*>::iterator task;
	int positiontasks = 0;

	for (task = tasks.begin(); task != tasks.end(); task++)
		if((*task)->PositionTask())
			positiontasks++;
	
	if (positiontasks >= 2) {
		m_poleconstraint = false;
		return;
	}

	// get positions and rotations
	root->UpdateTransform(m_rootmatrix);

	const MT_Vector3 rootpos = root->GlobalStart();
	const MT_Vector3 endpos = m_poletip->GlobalEnd();
	const MT_Matrix3x3& rootbasis = root->GlobalTransform().getBasis();

	// construct "lookat" matrices (like gluLookAt), based on a direction and
	// an up vector, with the direction going from the root to the end effector
	// and the up vector going from the root to the pole constraint position.
	MT_Vector3 dir = normalize(endpos - rootpos);
	MT_Vector3 rootx= rootbasis.getColumn(0);
	MT_Vector3 rootz= rootbasis.getColumn(2);
	MT_Vector3 up = rootx*cos(m_poleangle) + rootz*sin(m_poleangle);

	// in post, don't rotate towards the goal but only correct the pole up
	MT_Vector3 poledir = (m_getpoleangle)? dir: normalize(m_goal - rootpos);
	MT_Vector3 poleup = normalize(m_polegoal - rootpos);

	MT_Matrix3x3 mat, polemat;

	mat[0] = normalize(MT_cross(dir, up));
	mat[1] = MT_cross(mat[0], dir);
	mat[2] = -dir;

	polemat[0] = normalize(MT_cross(poledir, poleup));
	polemat[1] = MT_cross(polemat[0], poledir);
	polemat[2] = -poledir;

	if(m_getpoleangle) {
		// we compute the pole angle that to rotate towards the target
		m_poleangle = angle(mat[1], polemat[1]);

		if(rootz.dot(mat[1]*cos(m_poleangle) + mat[0]*sin(m_poleangle)) > 0.0f)
			m_poleangle = -m_poleangle;

		// solve again, with the pole angle we just computed
		m_getpoleangle = false;
		ConstrainPoleVector(root, tasks);
	}
	else {
		// now we set as root matrix the difference between the current and
		// desired rotation based on the pole vector constraint. we use
		// transpose instead of inverse because we have orthogonal matrices
		// anyway, and in case of a singular matrix we don't get NaN's.
		MT_Transform trans(MT_Point3(0, 0, 0), polemat.transposed()*mat);
		m_rootmatrix = trans*m_rootmatrix;
	}
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
	float scale = ComputeScale();
	bool solved = false;
	//double dt = analyze_time();

	Scale(scale, tasks);

	ConstrainPoleVector(root, tasks);

	root->UpdateTransform(m_rootmatrix);

	// iterate
	for (int iterations = 0; iterations < max_iterations; iterations++) {
		// update transform
		root->UpdateTransform(m_rootmatrix);

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
				fprintf(stderr, "IK Exception\n");
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
			solved = true;
			break;
		}
	}

	if(m_poleconstraint)
		root->PrependBasis(m_rootmatrix.getBasis());

	Scale(1.0f/scale, tasks);

	//analyze_add_run(max_iterations, analyze_time()-dt);

	return solved;
}


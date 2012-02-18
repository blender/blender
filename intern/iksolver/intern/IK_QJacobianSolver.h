/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file iksolver/intern/IK_QJacobianSolver.h
 *  \ingroup iksolver
 */


#ifndef __IK_QJACOBIANSOLVER_H__

#define __IK_QJACOBIANSOLVER_H__

/**
 * @author Laurence Bourn
 * @date 28/6/2001
 */

#include <vector>
#include <list>

#include "MT_Vector3.h"
#include "MT_Transform.h"
#include "IK_QJacobian.h"
#include "IK_QSegment.h"
#include "IK_QTask.h"

class IK_QJacobianSolver
{
public:
	IK_QJacobianSolver();
	~IK_QJacobianSolver() {};

	// setup pole vector constraint
	void SetPoleVectorConstraint(IK_QSegment *tip, MT_Vector3& goal,
		MT_Vector3& polegoal, float poleangle, bool getangle);
	float GetPoleAngle() { return m_poleangle; };

	// call setup once before solving, if it fails don't solve
	bool Setup(IK_QSegment *root, std::list<IK_QTask*>& tasks);

	// returns true if converged, false if max number of iterations was used
	bool Solve(
		IK_QSegment *root,
		std::list<IK_QTask*> tasks,
		const MT_Scalar tolerance,
		const int max_iterations
	);

private:
	void AddSegmentList(IK_QSegment *seg);
	bool UpdateAngles(MT_Scalar& norm);
	void ConstrainPoleVector(IK_QSegment *root, std::list<IK_QTask*>& tasks);

	MT_Scalar ComputeScale();
	void Scale(float scale, std::list<IK_QTask*>& tasks);

private:

	IK_QJacobian m_jacobian;
	IK_QJacobian m_jacobian_sub;

	bool m_secondary_enabled;

	std::vector<IK_QSegment*> m_segments;

	MT_Transform m_rootmatrix;

	bool m_poleconstraint;
	bool m_getpoleangle;
	MT_Vector3 m_goal;
	MT_Vector3 m_polegoal;
	float m_poleangle;
	IK_QSegment *m_poletip;
};

#endif


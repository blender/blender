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

/** \file iksolver/intern/IK_Solver.cpp
 *  \ingroup iksolver
 */


#include "../extern/IK_solver.h"

#include "IK_QJacobianSolver.h"
#include "IK_QSegment.h"
#include "IK_QTask.h"

#include <list>
using namespace std;

class IK_QSolver {
public:
	IK_QSolver() : root(NULL) {};

	IK_QJacobianSolver solver;
	IK_QSegment *root;
	std::list<IK_QTask*> tasks;
};

// FIXME: locks still result in small "residual" changes to the locked axes...
IK_QSegment *CreateSegment(int flag, bool translate)
{
	int ndof = 0;
	ndof += (flag & IK_XDOF)? 1: 0;
	ndof += (flag & IK_YDOF)? 1: 0;
	ndof += (flag & IK_ZDOF)? 1: 0;

	IK_QSegment *seg;

	if (ndof == 0)
		return NULL;
	else if (ndof == 1) {
		int axis;

		if (flag & IK_XDOF) axis = 0;
		else if (flag & IK_YDOF) axis = 1;
		else axis = 2;

		if (translate)
			seg = new IK_QTranslateSegment(axis);
		else
			seg = new IK_QRevoluteSegment(axis);
	}
	else if (ndof == 2) {
		int axis1, axis2;

		if (flag & IK_XDOF) {
			axis1 = 0;
			axis2 = (flag & IK_YDOF)? 1: 2;
		}
		else {
			axis1 = 1;
			axis2 = 2;
		}

		if (translate)
			seg = new IK_QTranslateSegment(axis1, axis2);
		else {
			if (axis1 + axis2 == 2)
				seg = new IK_QSwingSegment();
			else
				seg = new IK_QElbowSegment((axis1 == 0)? 0: 2);
		}
	}
	else {
		if (translate)
			seg = new IK_QTranslateSegment();
		else
			seg = new IK_QSphericalSegment();
	}

	return seg;
}

IK_Segment *IK_CreateSegment(int flag)
{
	IK_QSegment *rot = CreateSegment(flag, false);
	IK_QSegment *trans = CreateSegment(flag >> 3, true);

	IK_QSegment *seg;

	if (rot == NULL && trans == NULL)
		seg = new IK_QNullSegment();
	else if (rot == NULL)
		seg = trans;
	else {
		seg = rot;

		// make it seem from the interface as if the rotation and translation
		// segment are one
		if (trans) {
			seg->SetComposite(trans);
			trans->SetParent(seg);
		}
	}

	return seg;
}

void IK_FreeSegment(IK_Segment *seg)
{
	IK_QSegment *qseg = (IK_QSegment*)seg;

	if (qseg->Composite())
		delete qseg->Composite();
	delete qseg;
}

void IK_SetParent(IK_Segment *seg, IK_Segment *parent)
{
	IK_QSegment *qseg = (IK_QSegment*)seg;
	IK_QSegment *qparent = (IK_QSegment*)parent;

	if (qparent && qparent->Composite())
		qseg->SetParent(qparent->Composite());
	else
		qseg->SetParent(qparent);
}

void IK_SetTransform(IK_Segment *seg, float start[3], float rest[][3], float basis[][3], float length)
{
	IK_QSegment *qseg = (IK_QSegment*)seg;

	MT_Vector3 mstart(start);
	// convert from blender column major to moto row major
	MT_Matrix3x3 mbasis(basis[0][0], basis[1][0], basis[2][0],
	                    basis[0][1], basis[1][1], basis[2][1],
	                    basis[0][2], basis[1][2], basis[2][2]);
	MT_Matrix3x3 mrest(rest[0][0], rest[1][0], rest[2][0],
	                   rest[0][1], rest[1][1], rest[2][1],
	                   rest[0][2], rest[1][2], rest[2][2]);
	MT_Scalar mlength(length);

	if (qseg->Composite()) {
		MT_Vector3 cstart(0, 0, 0);
		MT_Matrix3x3 cbasis;
		cbasis.setIdentity();
		
		qseg->SetTransform(mstart, mrest, mbasis, 0.0);
		qseg->Composite()->SetTransform(cstart, cbasis, cbasis, mlength);
	}
	else
		qseg->SetTransform(mstart, mrest, mbasis, mlength);
}

void IK_SetLimit(IK_Segment *seg, IK_SegmentAxis axis, float lmin, float lmax)
{
	IK_QSegment *qseg = (IK_QSegment*)seg;

	if (axis >= IK_TRANS_X) {
		if(!qseg->Translational()) {
			if(qseg->Composite() && qseg->Composite()->Translational())
				qseg = qseg->Composite();
			else
				return;
		}

		if(axis == IK_TRANS_X) axis = IK_X;
		else if(axis == IK_TRANS_Y) axis = IK_Y;
		else axis = IK_Z;
	}

	qseg->SetLimit(axis, lmin, lmax);
}

void IK_SetStiffness(IK_Segment *seg, IK_SegmentAxis axis, float stiffness)
{
	if (stiffness < 0.0)
		return;
	
	if (stiffness > 0.999)
		stiffness = 0.999;

	IK_QSegment *qseg = (IK_QSegment*)seg;
	MT_Scalar weight = 1.0-stiffness;

	if (axis >= IK_TRANS_X) {
		if(!qseg->Translational()) {
			if(qseg->Composite() && qseg->Composite()->Translational())
				qseg = qseg->Composite();
			else
				return;
		}

		if(axis == IK_TRANS_X) axis = IK_X;
		else if(axis == IK_TRANS_Y) axis = IK_Y;
		else axis = IK_Z;
	}

	qseg->SetWeight(axis, weight);
}

void IK_GetBasisChange(IK_Segment *seg, float basis_change[][3])
{
	IK_QSegment *qseg = (IK_QSegment*)seg;

	if (qseg->Translational() && qseg->Composite())
		qseg = qseg->Composite();

	const MT_Matrix3x3& change = qseg->BasisChange();

	// convert from moto row major to blender column major
	basis_change[0][0] = (float)change[0][0];
	basis_change[1][0] = (float)change[0][1];
	basis_change[2][0] = (float)change[0][2];
	basis_change[0][1] = (float)change[1][0];
	basis_change[1][1] = (float)change[1][1];
	basis_change[2][1] = (float)change[1][2];
	basis_change[0][2] = (float)change[2][0];
	basis_change[1][2] = (float)change[2][1];
	basis_change[2][2] = (float)change[2][2];
}

void IK_GetTranslationChange(IK_Segment *seg, float *translation_change)
{
	IK_QSegment *qseg = (IK_QSegment*)seg;

	if (!qseg->Translational() && qseg->Composite())
		qseg = qseg->Composite();
	
	const MT_Vector3& change = qseg->TranslationChange();

	translation_change[0] = (float)change[0];
	translation_change[1] = (float)change[1];
	translation_change[2] = (float)change[2];
}

IK_Solver *IK_CreateSolver(IK_Segment *root)
{
	if (root == NULL)
		return NULL;
	
	IK_QSolver *solver = new IK_QSolver();
	solver->root = (IK_QSegment*)root;

	return (IK_Solver*)solver;
}

void IK_FreeSolver(IK_Solver *solver)
{
	if (solver == NULL)
		return;

	IK_QSolver *qsolver = (IK_QSolver*)solver;
	std::list<IK_QTask*>& tasks = qsolver->tasks;
	std::list<IK_QTask*>::iterator task;

	for (task = tasks.begin(); task != tasks.end(); task++)
		delete (*task);
	
	delete qsolver;
}

void IK_SolverAddGoal(IK_Solver *solver, IK_Segment *tip, float goal[3], float weight)
{
	if (solver == NULL || tip == NULL)
		return;

	IK_QSolver *qsolver = (IK_QSolver*)solver;
	IK_QSegment *qtip = (IK_QSegment*)tip;

	if (qtip->Composite())
		qtip = qtip->Composite();

	MT_Vector3 pos(goal);

	IK_QTask *ee = new IK_QPositionTask(true, qtip, pos);
	ee->SetWeight(weight);
	qsolver->tasks.push_back(ee);
}

void IK_SolverAddGoalOrientation(IK_Solver *solver, IK_Segment *tip, float goal[][3], float weight)
{
	if (solver == NULL || tip == NULL)
		return;

	IK_QSolver *qsolver = (IK_QSolver*)solver;
	IK_QSegment *qtip = (IK_QSegment*)tip;

	if (qtip->Composite())
		qtip = qtip->Composite();

	// convert from blender column major to moto row major
	MT_Matrix3x3 rot(goal[0][0], goal[1][0], goal[2][0],
	                 goal[0][1], goal[1][1], goal[2][1],
	                 goal[0][2], goal[1][2], goal[2][2]);

	IK_QTask *orient = new IK_QOrientationTask(true, qtip, rot);
	orient->SetWeight(weight);
	qsolver->tasks.push_back(orient);
}

void IK_SolverSetPoleVectorConstraint(IK_Solver *solver, IK_Segment *tip, float goal[3], float polegoal[3], float poleangle, int getangle)
{
	if (solver == NULL || tip == NULL)
		return;

	IK_QSolver *qsolver = (IK_QSolver*)solver;
	IK_QSegment *qtip = (IK_QSegment*)tip;

	MT_Vector3 qgoal(goal);
	MT_Vector3 qpolegoal(polegoal);

	qsolver->solver.SetPoleVectorConstraint(
		qtip, qgoal, qpolegoal, poleangle, getangle);
}

float IK_SolverGetPoleAngle(IK_Solver *solver)
{
	if (solver == NULL)
		return 0.0f;

	IK_QSolver *qsolver = (IK_QSolver*)solver;

	return qsolver->solver.GetPoleAngle();
}

void IK_SolverAddCenterOfMass(IK_Solver *solver, IK_Segment *root, float goal[3], float weight)
{
	if (solver == NULL || root == NULL)
		return;

	IK_QSolver *qsolver = (IK_QSolver*)solver;
	IK_QSegment *qroot = (IK_QSegment*)root;

	// convert from blender column major to moto row major
	MT_Vector3 center(goal);

	IK_QTask *com = new IK_QCenterOfMassTask(true, qroot, center);
	com->SetWeight(weight);
	qsolver->tasks.push_back(com);
}

int IK_Solve(IK_Solver *solver, float tolerance, int max_iterations)
{
	if (solver == NULL)
		return 0;

	IK_QSolver *qsolver = (IK_QSolver*)solver;

	IK_QSegment *root = qsolver->root;
	IK_QJacobianSolver& jacobian = qsolver->solver;
	std::list<IK_QTask*>& tasks = qsolver->tasks;
	MT_Scalar tol = tolerance;

	if(!jacobian.Setup(root, tasks))
		return 0;

	bool result = jacobian.Solve(root, tasks, tol, max_iterations);

	return ((result)? 1: 0);
}


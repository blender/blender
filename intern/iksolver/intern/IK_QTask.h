/*
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
 * Original author: Laurence
 */

/** \file \ingroup iksolver
 */

#pragma once

#include "IK_Math.h"
#include "IK_QJacobian.h"
#include "IK_QSegment.h"

class IK_QTask
{
public:
	IK_QTask(
		int size,
		bool primary,
		bool active,
		const IK_QSegment *segment
	);
	virtual ~IK_QTask() {}

	int Id() const
	{ return m_size; }

	void SetId(int id)
	{ m_id = id; }

	int Size() const
	{ return m_size; }

	bool Primary() const
	{ return m_primary; }

	bool Active() const
	{ return m_active; }

	double Weight() const
	{ return m_weight*m_weight; }

	void SetWeight(double weight)
	{ m_weight = sqrt(weight); }

	virtual void ComputeJacobian(IK_QJacobian& jacobian)=0;

	virtual double Distance() const=0;

	virtual bool PositionTask() const { return false; }

	virtual void Scale(double) {}

protected:
	int m_id;
	int m_size;
	bool m_primary;
	bool m_active;
	const IK_QSegment *m_segment;
	double m_weight;
};

class IK_QPositionTask : public IK_QTask
{
public:
	IK_QPositionTask(
		bool primary,
		const IK_QSegment *segment,
		const Vector3d& goal
	);

	void ComputeJacobian(IK_QJacobian& jacobian);

	double Distance() const;

	bool PositionTask() const { return true; }
	void Scale(double scale) { m_goal *= scale; m_clamp_length *= scale; }

private:
	Vector3d m_goal;
	double m_clamp_length;
};

class IK_QOrientationTask : public IK_QTask
{
public:
	IK_QOrientationTask(
		bool primary,
		const IK_QSegment *segment,
		const Matrix3d& goal
	);

	double Distance() const { return m_distance; }
	void ComputeJacobian(IK_QJacobian& jacobian);

private:
	Matrix3d m_goal;
	double m_distance;
};


class IK_QCenterOfMassTask : public IK_QTask
{
public:
	IK_QCenterOfMassTask(
		bool primary,
		const IK_QSegment *segment,
		const Vector3d& center
	);

	void ComputeJacobian(IK_QJacobian& jacobian);

	double Distance() const;

	void Scale(double scale) { m_goal_center *= scale; m_distance *= scale; }

private:
	double ComputeTotalMass(const IK_QSegment *segment);
	Vector3d ComputeCenter(const IK_QSegment *segment);
	void JacobianSegment(IK_QJacobian& jacobian, Vector3d& center, const IK_QSegment *segment);

	Vector3d m_goal_center;
	double m_total_mass_inv;
	double m_distance;
};


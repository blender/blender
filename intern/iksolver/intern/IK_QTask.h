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
 * Original author: Laurence
 * Contributor(s): Brecht
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef NAN_INCLUDED_IK_QTask_h
#define NAN_INCLUDED_IK_QTask_h

#include "MT_Vector3.h"
#include "MT_Transform.h"
#include "MT_Matrix4x4.h"
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
	virtual ~IK_QTask() {};

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

	MT_Scalar Weight() const
	{ return m_weight*m_weight; }

	void SetWeight(MT_Scalar weight)
	{ m_weight = sqrt(weight); }

	virtual void ComputeJacobian(IK_QJacobian& jacobian)=0;

	virtual MT_Scalar Distance() const=0;

	virtual bool PositionTask() const { return false; }

	virtual void Scale(float) {}

protected:
	int m_id;
	int m_size;
	bool m_primary;
	bool m_active;
	const IK_QSegment *m_segment;
	MT_Scalar m_weight;
};

class IK_QPositionTask : public IK_QTask
{
public:
	IK_QPositionTask(
		bool primary,
		const IK_QSegment *segment,
		const MT_Vector3& goal
	);

	void ComputeJacobian(IK_QJacobian& jacobian);

	MT_Scalar Distance() const;

	bool PositionTask() const { return true; }
	void Scale(float scale) { m_goal *= scale; m_clamp_length *= scale; }

private:
	MT_Vector3 m_goal;
	MT_Scalar m_clamp_length;
};

class IK_QOrientationTask : public IK_QTask
{
public:
	IK_QOrientationTask(
		bool primary,
		const IK_QSegment *segment,
		const MT_Matrix3x3& goal
	);

	MT_Scalar Distance() const { return m_distance; };
	void ComputeJacobian(IK_QJacobian& jacobian);

private:
	MT_Matrix3x3 m_goal;
	MT_Scalar m_distance;
};


class IK_QCenterOfMassTask : public IK_QTask
{
public:
	IK_QCenterOfMassTask(
		bool primary,
		const IK_QSegment *segment,
		const MT_Vector3& center
	);

	void ComputeJacobian(IK_QJacobian& jacobian);

	MT_Scalar Distance() const;

	void Scale(float scale) { m_goal_center *= scale; m_distance *= scale; }

private:
	MT_Scalar ComputeTotalMass(const IK_QSegment *segment);
	MT_Vector3 ComputeCenter(const IK_QSegment *segment);
	void JacobianSegment(IK_QJacobian& jacobian, MT_Vector3& center, const IK_QSegment *segment);

	MT_Vector3 m_goal_center;
	MT_Scalar m_total_mass_inv;
	MT_Scalar m_distance;
};

#endif


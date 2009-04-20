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

#include "IK_QTask.h"

// IK_QTask

IK_QTask::IK_QTask(
	int size,
	bool primary,
	bool active,
	const IK_QSegment *segment
) :
	m_size(size), m_primary(primary), m_active(active), m_segment(segment),
	m_weight(1.0)
{
}

// IK_QPositionTask

IK_QPositionTask::IK_QPositionTask(
	bool primary,
	const IK_QSegment *segment,
	const MT_Vector3& goal
) :
	IK_QTask(3, primary, true, segment), m_goal(goal)
{
	// computing clamping length
	int num;
	const IK_QSegment *seg;

	m_clamp_length = 0.0;
	num = 0;

	for (seg = m_segment; seg; seg = seg->Parent()) {
		m_clamp_length += seg->MaxExtension();
		num++;
	}

	m_clamp_length /= 2*num;
}

void IK_QPositionTask::ComputeJacobian(IK_QJacobian& jacobian)
{
	// compute beta
	const MT_Vector3& pos = m_segment->GlobalEnd();

	MT_Vector3 d_pos = m_goal - pos;
	MT_Scalar length = d_pos.length();

	if (length > m_clamp_length)
		d_pos = (m_clamp_length/length)*d_pos;
	
	jacobian.SetBetas(m_id, m_size, m_weight*d_pos);

	// compute derivatives
	int i;
	const IK_QSegment *seg;

	for (seg = m_segment; seg; seg = seg->Parent()) {
		MT_Vector3 p = seg->GlobalStart() - pos;

		for (i = 0; i < seg->NumberOfDoF(); i++) {
			MT_Vector3 axis = seg->Axis(i)*m_weight;

			if (seg->Translational())
				jacobian.SetDerivatives(m_id, seg->DoFId()+i, axis);
			else {
				MT_Vector3 pa = p.cross(axis);
				jacobian.SetDerivatives(m_id, seg->DoFId()+i, pa);
			}
		}
	}
}

MT_Scalar IK_QPositionTask::Distance() const
{
	const MT_Vector3& pos = m_segment->GlobalEnd();
	MT_Vector3 d_pos = m_goal - pos;
	return d_pos.length();
}

// IK_QOrientationTask

IK_QOrientationTask::IK_QOrientationTask(
	bool primary,
	const IK_QSegment *segment,
	const MT_Matrix3x3& goal
) :
	IK_QTask(3, primary, true, segment), m_goal(goal), m_distance(0.0)
{
}

void IK_QOrientationTask::ComputeJacobian(IK_QJacobian& jacobian)
{
	// compute betas
	const MT_Matrix3x3& rot = m_segment->GlobalTransform().getBasis();

	MT_Matrix3x3 d_rotm = m_goal*rot.transposed();
	d_rotm.transpose();

	MT_Vector3 d_rot;
	d_rot = -0.5*MT_Vector3(d_rotm[2][1] - d_rotm[1][2],
	                        d_rotm[0][2] - d_rotm[2][0],
	                        d_rotm[1][0] - d_rotm[0][1]);

	m_distance = d_rot.length();

	jacobian.SetBetas(m_id, m_size, m_weight*d_rot);

	// compute derivatives
	int i;
	const IK_QSegment *seg;

	for (seg = m_segment; seg; seg = seg->Parent())
		for (i = 0; i < seg->NumberOfDoF(); i++) {

			if (seg->Translational())
				jacobian.SetDerivatives(m_id, seg->DoFId()+i, MT_Vector3(0, 0, 0));
			else {
				MT_Vector3 axis = seg->Axis(i)*m_weight;
				jacobian.SetDerivatives(m_id, seg->DoFId()+i, axis);
			}
		}
}

// IK_QCenterOfMassTask
// Note: implementation not finished!

IK_QCenterOfMassTask::IK_QCenterOfMassTask(
	bool primary,
	const IK_QSegment *segment,
	const MT_Vector3& goal_center
) :
	IK_QTask(3, primary, true, segment), m_goal_center(goal_center)
{
	m_total_mass_inv = ComputeTotalMass(m_segment);
	if (!MT_fuzzyZero(m_total_mass_inv))
		m_total_mass_inv = 1.0/m_total_mass_inv;
}

MT_Scalar IK_QCenterOfMassTask::ComputeTotalMass(const IK_QSegment *segment)
{
	MT_Scalar mass = /*seg->Mass()*/ 1.0;

	const IK_QSegment *seg;
	for (seg = segment->Child(); seg; seg = seg->Sibling())
		mass += ComputeTotalMass(seg);
	
	return mass;
}

MT_Vector3 IK_QCenterOfMassTask::ComputeCenter(const IK_QSegment *segment)
{
	MT_Vector3 center = /*seg->Mass()**/segment->GlobalStart();

	const IK_QSegment *seg;
	for (seg = segment->Child(); seg; seg = seg->Sibling())
		center += ComputeCenter(seg);
	
	return center;
}

void IK_QCenterOfMassTask::JacobianSegment(IK_QJacobian& jacobian, MT_Vector3& center, const IK_QSegment *segment)
{
	int i;
	MT_Vector3 p = center - segment->GlobalStart();

	for (i = 0; i < segment->NumberOfDoF(); i++) {
		MT_Vector3 axis = segment->Axis(i)*m_weight;
		axis *= /*segment->Mass()**/m_total_mass_inv;
		
		if (segment->Translational())
			jacobian.SetDerivatives(m_id, segment->DoFId()+i, axis);
		else {
			MT_Vector3 pa = axis.cross(p);
			jacobian.SetDerivatives(m_id, segment->DoFId()+i, pa);
		}
	}
	
	const IK_QSegment *seg;
	for (seg = segment->Child(); seg; seg = seg->Sibling())
		JacobianSegment(jacobian, center, seg);
}

void IK_QCenterOfMassTask::ComputeJacobian(IK_QJacobian& jacobian)
{
	MT_Vector3 center = ComputeCenter(m_segment)*m_total_mass_inv;

	// compute beta
	MT_Vector3 d_pos = m_goal_center - center;

	m_distance = d_pos.length();

#if 0
	if (m_distance > m_clamp_length)
		d_pos = (m_clamp_length/m_distance)*d_pos;
#endif
	
	jacobian.SetBetas(m_id, m_size, m_weight*d_pos);

	// compute derivatives
	JacobianSegment(jacobian, center, m_segment);
}

MT_Scalar IK_QCenterOfMassTask::Distance() const
{
	return m_distance;
}


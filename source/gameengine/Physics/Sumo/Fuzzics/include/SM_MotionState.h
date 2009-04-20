/**
 * $Id$
 *
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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef SM_MOTIONSTATE_H
#define SM_MOTIONSTATE_H

#include "MT_Transform.h"

class SM_MotionState {
public:
	SM_MotionState() :
		m_time(0.0),
		m_pos(0.0, 0.0, 0.0),
		m_orn(0.0, 0.0, 0.0, 1.0),
		m_lin_vel(0.0, 0.0, 0.0),
		m_ang_vel(0.0, 0.0, 0.0)
		{}

	void setPosition(const MT_Point3& pos)             { m_pos = pos; }
	void setOrientation(const MT_Quaternion& orn)      { m_orn = orn; }
	void setLinearVelocity(const MT_Vector3& lin_vel)  { m_lin_vel = lin_vel; }
	void setAngularVelocity(const MT_Vector3& ang_vel) { m_ang_vel = ang_vel; }
	void setTime(MT_Scalar time)                       { m_time = time; }
	
	const MT_Point3&     getPosition()        const { return m_pos; }
	const MT_Quaternion& getOrientation()     const { return m_orn; }
	const MT_Vector3&    getLinearVelocity()  const { return m_lin_vel; }
	const MT_Vector3&    getAngularVelocity() const { return m_ang_vel;	}
	
	MT_Scalar            getTime()            const { return m_time; }
	
	void integrateMidpoint(MT_Scalar timeStep, const SM_MotionState &prev_state, const MT_Vector3 &velocity, const MT_Quaternion& ang_vel);
	void integrateBackward(MT_Scalar timeStep, const MT_Vector3 &velocity, const MT_Quaternion& ang_vel);
	void integrateForward(MT_Scalar timeStep, const SM_MotionState &prev_state);
	
	void lerp(const SM_MotionState &prev, const SM_MotionState &next);
	void lerp(MT_Scalar t, const SM_MotionState &other);
	
	virtual MT_Transform getTransform() const {
		return MT_Transform(m_pos, m_orn);
	}

protected:
	MT_Scalar         m_time;
	MT_Point3         m_pos;
	MT_Quaternion     m_orn;
	MT_Vector3        m_lin_vel;
	MT_Vector3        m_ang_vel;
};
	
#endif


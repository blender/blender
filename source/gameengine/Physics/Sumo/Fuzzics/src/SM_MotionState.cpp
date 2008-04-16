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
#include <MT_Scalar.h>
#include <MT_Vector3.h>
#include <MT_Quaternion.h>
 
#include "SM_MotionState.h"

void SM_MotionState::integrateMidpoint(MT_Scalar timeStep, const SM_MotionState &prev_state, const MT_Vector3 &velocity, const MT_Quaternion& ang_vel)
{
	m_pos += (prev_state.getLinearVelocity() + velocity) * (timeStep * 0.5);
	m_orn += (prev_state.getAngularVelocity() * prev_state.getOrientation() + ang_vel * m_orn) * (timeStep * 0.25);
	m_orn.normalize();
}

void SM_MotionState::integrateBackward(MT_Scalar timeStep, const MT_Vector3 &velocity, const MT_Quaternion& ang_vel)
{
	m_pos += velocity * timeStep;
	m_orn += ang_vel * m_orn * (timeStep * 0.5);
	m_orn.normalize();
}

void SM_MotionState::integrateForward(MT_Scalar timeStep, const SM_MotionState &prev_state)
{
	m_pos += prev_state.getLinearVelocity() * timeStep;
	m_orn += prev_state.getAngularVelocity() * m_orn * (timeStep * 0.5);
	m_orn.normalize();
}

/*
// Newtonian lerp: interpolate based on Newtonian motion
void SM_MotionState::nlerp(const SM_MotionState &prev, const SM_MotionState &next)
{
	MT_Scalar dt = next.getTime() - prev.getTime();
	MT_Scalar t = getTime() - prev.getTime();
	MT_Vector3 dx = next.getPosition() - prev.getPosition();
	MT_Vector3 a = dx/(dt*dt) - prev.getLinearVelocity()/dt;
	
	m_pos = prev.getPosition() + prev.getLinearVelocity()*t + a*t*t;
}
*/

void SM_MotionState::lerp(const SM_MotionState &prev, const SM_MotionState &next)
{
	MT_Scalar dt = next.getTime() - prev.getTime();
	if (MT_fuzzyZero(dt))
	{
		*this = next;
		return;
	}
	
	MT_Scalar x = (getTime() - prev.getTime())/dt;
	
	m_pos = x*next.getPosition() + (1-x)*prev.getPosition();
	
	m_orn = prev.getOrientation().slerp(next.getOrientation(), 1-x);
	
	m_lin_vel = x*next.getLinearVelocity() + (1-x)*prev.getLinearVelocity();
	m_ang_vel = x*next.getAngularVelocity() + (1-x)*prev.getAngularVelocity();
}

void SM_MotionState::lerp(MT_Scalar t, const SM_MotionState &other)
{
	MT_Scalar x = (t - getTime())/(other.getTime() - getTime());
	m_pos = (1-x)*m_pos + x*other.getPosition();
	
	m_orn =  other.getOrientation().slerp(m_orn, x);
	
	m_lin_vel = (1-x)*m_lin_vel + x*other.getLinearVelocity();
	m_ang_vel = (1-x)*m_ang_vel + x*other.getAngularVelocity();
	
	m_time = t;
}


/**
 * $Id$
 *
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

void SM_MotionState::lerp(MT_Scalar t, const SM_MotionState &other)
{
	MT_Scalar x = (t - getTime())/(other.getTime() - getTime());
	m_pos = x*m_pos + (1-x)*other.getPosition();
	
	m_orn = m_orn.slerp(other.getOrientation(), x);
	
	m_lin_vel = x*m_lin_vel + (1-x)*other.getLinearVelocity();
	m_ang_vel = x*m_ang_vel + (1-x)*other.getAngularVelocity();
	
	m_time = t;
}


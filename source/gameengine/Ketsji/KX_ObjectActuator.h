/**
 * Do translation/rotation actions
 *
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

#ifndef __KX_OBJECTACTUATOR
#define __KX_OBJECTACTUATOR

#include "SCA_IActuator.h"
#include "MT_Vector3.h"

//
// Bitfield that stores the flags for each CValue derived class
//
struct KX_LocalFlags {
	KX_LocalFlags() :
		Force(false),
		Torque(false),
		DRot(false),
		DLoc(false),
		LinearVelocity(false),
		AngularVelocity(false),
		AddOrSetLinV(false),
		ClampVelocity(false),
		ZeroForce(false),
		ZeroDRot(false),
		ZeroDLoc(false),
		ZeroLinearVelocity(false),
		ZeroAngularVelocity(false)
	{
	}

	unsigned short Force : 1;
	unsigned short Torque : 1;
	unsigned short DRot : 1;
	unsigned short DLoc : 1;
	unsigned short LinearVelocity : 1;
	unsigned short AngularVelocity : 1;
	unsigned short AddOrSetLinV : 1;
	unsigned short ClampVelocity : 1;
	unsigned short ZeroForce : 1;
	unsigned short ZeroTorque : 1;
	unsigned short ZeroDRot : 1;
	unsigned short ZeroDLoc : 1;
	unsigned short ZeroLinearVelocity : 1;
	unsigned short ZeroAngularVelocity : 1;
};

class KX_ObjectActuator : public SCA_IActuator
{
	Py_Header;

	MT_Vector3		m_force;
	MT_Vector3		m_torque;
	MT_Vector3		m_dloc;
	MT_Vector3		m_drot;
	MT_Vector3		m_linear_velocity;
	MT_Vector3		m_angular_velocity;	
	MT_Scalar		m_linear_length2;
	MT_Scalar		m_angular_length2;
	MT_Scalar		m_current_linear_factor;
	MT_Scalar		m_current_angular_factor;
	short			m_damping;
  	KX_LocalFlags	m_bitLocalFlag;

	// A hack bool -- oh no sorry everyone
	// This bool is used to check if we have informed 
	// the physics object that we are no longer 
	// setting linear velocity.

	bool m_active_combined_velocity;
	bool m_linear_damping_active;
	bool m_angular_damping_active;
	
public:
	enum KX_OBJECT_ACT_VEC_TYPE {
		KX_OBJECT_ACT_NODEF = 0,
		KX_OBJECT_ACT_FORCE,
		KX_OBJECT_ACT_TORQUE,
		KX_OBJECT_ACT_DLOC,
		KX_OBJECT_ACT_DROT,
		KX_OBJECT_ACT_LINEAR_VELOCITY,
		KX_OBJECT_ACT_ANGULAR_VELOCITY,
		KX_OBJECT_ACT_MAX
	};
		
	/**
	 * Check whether this is a valid vector mode 
	 */
	bool isValid(KX_OBJECT_ACT_VEC_TYPE type);

	KX_ObjectActuator(
		SCA_IObject* gameobj,
		const MT_Vector3& force,
		const MT_Vector3& torque,
		const MT_Vector3& dloc,
		const MT_Vector3& drot,
		const MT_Vector3& linV,
		const MT_Vector3& angV,
		const short damping,
		const KX_LocalFlags& flag,
		PyTypeObject* T=&Type
	);

	CValue* GetReplica();

	void SetForceLoc(const double force[3])	{ /*m_force=force;*/ }
	void UpdateFuzzyFlags()
		{ 
			m_bitLocalFlag.ZeroForce = MT_fuzzyZero(m_force);
			m_bitLocalFlag.ZeroTorque = MT_fuzzyZero(m_torque);
			m_bitLocalFlag.ZeroDLoc = MT_fuzzyZero(m_dloc);
			m_bitLocalFlag.ZeroDRot = MT_fuzzyZero(m_drot);
			m_bitLocalFlag.ZeroLinearVelocity = MT_fuzzyZero(m_linear_velocity);
			m_linear_length2 = (m_bitLocalFlag.ZeroLinearVelocity) ? 0.0 : m_linear_velocity.length2();
			m_bitLocalFlag.ZeroAngularVelocity = MT_fuzzyZero(m_angular_velocity);
			m_angular_length2 = (m_bitLocalFlag.ZeroAngularVelocity) ? 0.0 : m_angular_velocity.length2();
		}
	virtual bool Update();



	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	
	virtual PyObject* _getattr(const STR_String& attr);

	KX_PYMETHOD(KX_ObjectActuator,GetForce);
	KX_PYMETHOD(KX_ObjectActuator,SetForce);
	KX_PYMETHOD(KX_ObjectActuator,GetTorque);
	KX_PYMETHOD(KX_ObjectActuator,SetTorque);
	KX_PYMETHOD(KX_ObjectActuator,GetDLoc);
	KX_PYMETHOD(KX_ObjectActuator,SetDLoc);
	KX_PYMETHOD(KX_ObjectActuator,GetDRot);
	KX_PYMETHOD(KX_ObjectActuator,SetDRot);
	KX_PYMETHOD(KX_ObjectActuator,GetLinearVelocity);
	KX_PYMETHOD(KX_ObjectActuator,SetLinearVelocity);
	KX_PYMETHOD(KX_ObjectActuator,GetAngularVelocity);
	KX_PYMETHOD(KX_ObjectActuator,SetAngularVelocity);
	KX_PYMETHOD(KX_ObjectActuator,SetVelocityDamping);
	KX_PYMETHOD(KX_ObjectActuator,GetVelocityDamping);
};

#endif //__KX_OBJECTACTUATOR


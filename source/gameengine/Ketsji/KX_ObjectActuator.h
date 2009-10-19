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

#ifdef USE_MATHUTILS
void KX_ObjectActuator_Mathutils_Callback_Init(void);
#endif

class KX_GameObject;

//
// Stores the flags for each CValue derived class
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
		ZeroForce(false),
		ZeroDRot(false),
		ZeroDLoc(false),
		ZeroLinearVelocity(false),
		ZeroAngularVelocity(false)
	{
	}

	bool Force;
	bool Torque;
	bool DRot;
	bool DLoc;
	bool LinearVelocity;
	bool AngularVelocity;
	bool AddOrSetLinV;
	bool ServoControl;
	bool ZeroForce;
	bool ZeroTorque;
	bool ZeroDRot;
	bool ZeroDLoc;
	bool ZeroLinearVelocity;
	bool ZeroAngularVelocity;
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
	MT_Vector3		m_pid;
	MT_Scalar		m_linear_length2;
	MT_Scalar		m_angular_length2;
	// used in damping
	MT_Scalar		m_current_linear_factor;
	MT_Scalar		m_current_angular_factor;
	short			m_damping;
	// used in servo control
	MT_Vector3		m_previous_error;
	MT_Vector3		m_error_accumulator;
  	KX_LocalFlags	m_bitLocalFlag;
	KX_GameObject*  m_reference;
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
		KX_GameObject* refobj,
		const MT_Vector3& force,
		const MT_Vector3& torque,
		const MT_Vector3& dloc,
		const MT_Vector3& drot,
		const MT_Vector3& linV,
		const MT_Vector3& angV,
		const short damping,
		const KX_LocalFlags& flag
	);
	~KX_ObjectActuator();
	CValue* GetReplica();
	void ProcessReplica();
	bool UnlinkObject(SCA_IObject* clientobj);
	void Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map);

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

	/* Attributes */
	static PyObject*	pyattr_get_forceLimitX(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_forceLimitX(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_forceLimitY(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_forceLimitY(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_forceLimitZ(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_forceLimitZ(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_reference(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_reference(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

#ifdef USE_MATHUTILS
	static PyObject*	pyattr_get_linV(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_linV(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_angV(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int			pyattr_set_angV(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
#endif

	// This lets the attribute macros use UpdateFuzzyFlags()
	static int PyUpdateFuzzyFlags(void *self, const PyAttributeDef *attrdef)
	{
		KX_ObjectActuator* act = reinterpret_cast<KX_ObjectActuator*>(self);
		act->UpdateFuzzyFlags();
		return 0;
	}

	// This is the keep the PID values in check after they are assigned with Python
	static int PyCheckPid(void *self, const PyAttributeDef *attrdef)
	{
		KX_ObjectActuator* act = reinterpret_cast<KX_ObjectActuator*>(self);

		//P 0 to 200
		if (act->m_pid[0] < 0) {
			act->m_pid[0] = 0;
		} else if (act->m_pid[0] > 200) {
			act->m_pid[0] = 200;
		}

		//I 0 to 3
		if (act->m_pid[1] < 0) {
			act->m_pid[1] = 0;
		} else if (act->m_pid[1] > 3) {
			act->m_pid[1] = 3;
		}

		//D -100 to 100
		if (act->m_pid[2] < -100) {
			act->m_pid[2] = -100;
		} else if (act->m_pid[2] > 100) {
			act->m_pid[2] = 100;
		}

		return 0;
	}
};

#endif //__KX_OBJECTACTUATOR


/**
 * KX_ConstraintActuator.h
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

#ifndef __KX_CONSTRAINTACTUATOR
#define __KX_CONSTRAINTACTUATOR

#include "SCA_IActuator.h"
#include "MT_Scalar.h"
#include "MT_Vector3.h"
#include "KX_ClientObjectInfo.h"

class KX_ConstraintActuator : public SCA_IActuator
{
	Py_Header;
protected:	
	// Damp time (int),
	int m_posDampTime;
	int m_rotDampTime;
	// min (float) 
	float m_minimumBound;
	// max (float)
	float m_maximumBound;
	// sinus of minimum angle
	float m_minimumSine;
	// sinus of maximum angle
	float m_maximumSine;
	// reference direction
	MT_Vector3 m_refDirection;
	// locrotxyz choice (pick one): only one choice allowed at a time!
	int m_locrot;
	// active time of actuator
	int m_activeTime;
	int m_currentTime;
	// option
	int m_option;
	// property to check
	char m_property[32];

	/**
	 * Clamp <var> to <min>, <max>. Borders are included (in as far as
	 * float comparisons are good for equality...).
	 */
	void Clamp(MT_Scalar &var, float min, float max);

	
 public:
	 //  m_locrot
	enum KX_CONSTRAINTTYPE {
		KX_ACT_CONSTRAINT_NODEF = 0,
		KX_ACT_CONSTRAINT_LOCX,
		KX_ACT_CONSTRAINT_LOCY,
		KX_ACT_CONSTRAINT_LOCZ,
		KX_ACT_CONSTRAINT_ROTX,
		KX_ACT_CONSTRAINT_ROTY,
		KX_ACT_CONSTRAINT_ROTZ,
		KX_ACT_CONSTRAINT_DIRPX,
		KX_ACT_CONSTRAINT_DIRPY,
		KX_ACT_CONSTRAINT_DIRPZ,
		KX_ACT_CONSTRAINT_DIRNX,
		KX_ACT_CONSTRAINT_DIRNY,
		KX_ACT_CONSTRAINT_DIRNZ,
		KX_ACT_CONSTRAINT_ORIX,
		KX_ACT_CONSTRAINT_ORIY,
		KX_ACT_CONSTRAINT_ORIZ,
		KX_ACT_CONSTRAINT_MAX
	};
	// match ACT_CONST_... values from BIF_interface.h
	enum KX_CONSTRAINTOPT {
		KX_ACT_CONSTRAINT_NORMAL = 64,
		KX_ACT_CONSTRAINT_MATERIAL = 128,
		KX_ACT_CONSTRAINT_PERMANENT = 256,
		KX_ACT_CONSTRAINT_DISTANCE = 512
	};
	bool IsValidMode(KX_CONSTRAINTTYPE m); 
	bool RayHit(KX_ClientObjectInfo* client, MT_Point3& hit_point, MT_Vector3& hit_normal, void * const data);

	KX_ConstraintActuator(SCA_IObject* gameobj,
						  int posDamptime,
						  int rotDampTime,
						  float min,
						  float max,
						  float refDir[3],
						  int locrot,
						  int time,
						  int option,
						  char *property,
						  PyTypeObject* T=&Type);
	virtual ~KX_ConstraintActuator();
	virtual CValue* GetReplica() {
		KX_ConstraintActuator* replica = new KX_ConstraintActuator(*this);
		replica->ProcessReplica();
		// this will copy properties and so on...
		CValue::AddDataToReplica(replica);
		return replica;
	};

	virtual bool Update(double curtime, bool frame);

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	virtual PyObject* _getattr(const STR_String& attr);

	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetDamp);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetDamp);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetRotDamp);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetRotDamp);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetDirection);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetDirection);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetOption);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetOption);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetTime);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetTime);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetProperty);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetProperty);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetMin);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetMin);
	static char SetDistance_doc[];
	static char GetDistance_doc[];
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetMax);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetMax);
	static char SetRayLength_doc[];
	static char GetRayLength_doc[];
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetLimit);
	KX_PYMETHOD_DOC_NOARGS(KX_ConstraintActuator,GetLimit);
};

#endif //__KX_CONSTRAINTACTUATOR


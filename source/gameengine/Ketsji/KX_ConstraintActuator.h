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

class KX_ConstraintActuator : public SCA_IActuator
{
	Py_Header;
protected:	
	// Damp time (int),
	int m_dampTime;
	// min (float),
	float m_minimumBound;
	// max (float),
	float m_maximumBound;
	// locrotxyz choice (pick one): only one choice allowed at a time!
	int m_locrot;

	/**
	 * Clamp <var> to <min>, <max>. Borders are included (in as far as
	 * float comparisons are good for equality...).
	 */
	void Clamp(MT_Scalar &var, float min, float max);

	
 public:
	enum KX_CONSTRAINTTYPE {
		KX_ACT_CONSTRAINT_NODEF = 0,
		KX_ACT_CONSTRAINT_LOCX,
		KX_ACT_CONSTRAINT_LOCY,
		KX_ACT_CONSTRAINT_LOCZ,
		KX_ACT_CONSTRAINT_ROTX,
		KX_ACT_CONSTRAINT_ROTY,
		KX_ACT_CONSTRAINT_ROTZ,
		KX_ACT_CONSTRAINT_MAX
	};

	bool IsValidMode(KX_CONSTRAINTTYPE m); 

	KX_ConstraintActuator(SCA_IObject* gameobj,
						  int damptime,
						  float min,
						  float max,
						  int locrot,
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
	KX_PYMETHOD_DOC(KX_ConstraintActuator,GetDamp);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetMin);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,GetMin);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetMax);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,GetMax);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,SetLimit);
	KX_PYMETHOD_DOC(KX_ConstraintActuator,GetLimit);

};

#endif //__KX_CONSTRAINTACTUATOR


//
// Add object to the game world on action of this actuator
//
// $Id$
//
// ***** BEGIN GPL LICENSE BLOCK *****
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
// All rights reserved.
//
// The Original Code is: all of this file.
//
// Contributor(s): Campbell Barton
//
// ***** END GPL LICENSE BLOCK *****
//

#ifndef __KX_SCA_DYNAMICACTUATOR
#define __KX_SCA_DYNAMICACTUATOR

#include "SCA_IActuator.h"
#include "SCA_PropertyActuator.h"
#include "SCA_LogicManager.h"

#include "KX_GameObject.h"
#include "KX_IPhysicsController.h"

class KX_SCA_DynamicActuator : public SCA_IActuator
{
	Py_Header;

	// dynamics operation to apply to the game object
	short m_dyn_operation;
	float m_setmass;
 public:
	KX_SCA_DynamicActuator(
		SCA_IObject* gameobj, 
		short dyn_operation,
 		float setmass
	);

	~KX_SCA_DynamicActuator(
	);

		CValue* 
	GetReplica(
	);

	virtual bool 
	Update();

	//Python Interface
	enum DynamicOperation {
		KX_DYN_RESTORE_DYNAMICS = 0,
		KX_DYN_DISABLE_DYNAMICS,
		KX_DYN_ENABLE_RIGID_BODY,
		KX_DYN_DISABLE_RIGID_BODY,
		KX_DYN_SET_MASS,
	};
}; 

#endif

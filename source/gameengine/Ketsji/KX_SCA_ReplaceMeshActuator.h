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
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
// All rights reserved.
//
// The Original Code is: all of this file.
//
// Contributor(s): none yet.
//
// ***** END GPL LICENSE BLOCK *****
//
// Previously existed as:
// \source\gameengine\GameLogic\SCA_ReplaceMeshActuator.h
// Please look here for revision history.
//

#ifndef __KX_SCA_REPLACEMESHACTUATOR
#define __KX_SCA_REPLACEMESHACTUATOR

#include "SCA_IActuator.h"
#include "SCA_PropertyActuator.h"
#include "SCA_LogicManager.h"
#include "SCA_IScene.h"

#include "RAS_MeshObject.h"

class KX_SCA_ReplaceMeshActuator : public SCA_IActuator
{
	Py_Header;

	// mesh reference (mesh to replace)
	RAS_MeshObject* m_mesh;
	SCA_IScene*	 m_scene;
	bool m_use_gfx; 
	bool m_use_phys;

 public:
	KX_SCA_ReplaceMeshActuator(
		SCA_IObject* gameobj, 
		RAS_MeshObject *mesh, 
		SCA_IScene* scene,
		bool use_gfx,
		bool use_phys
	);

	~KX_SCA_ReplaceMeshActuator(
	);

		CValue* 
	GetReplica(
	);

	virtual bool 
	Update();

	void	InstantReplaceMesh();

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */

	static PyObject* pyattr_get_mesh(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_mesh(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	KX_PYMETHOD_DOC(KX_SCA_ReplaceMeshActuator,instantReplaceMesh);

}; 

#endif


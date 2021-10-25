/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file KX_SCA_EndObjectActuator.h
 *  \ingroup ketsji
 *  \brief Add object to the game world on action of this actuator
 *  \attention Previously existed as: source/gameengine/GameLogic/SCA_EndObjectActuator.h
 * Please look here for revision history.
 */

#ifndef __KX_SCA_ENDOBJECTACTUATOR_H__
#define __KX_SCA_ENDOBJECTACTUATOR_H__

#include "SCA_IActuator.h"

class SCA_IScene;

class KX_SCA_EndObjectActuator : public SCA_IActuator
{
	Py_Header
	SCA_IScene*		m_scene;

 public:
	KX_SCA_EndObjectActuator(
		SCA_IObject* gameobj,
		SCA_IScene* scene
	);

	~KX_SCA_EndObjectActuator();

		CValue* 
	GetReplica(
	);

	virtual bool 
	Update();

	virtual void Replace_IScene(SCA_IScene *val)
	{
		m_scene= val;
	};

	/* --------------------------------------------------------------------- */
	/* Python interface ---------------------------------------------------- */
	/* --------------------------------------------------------------------- */
	
}; /* end of class KX_EditObjectActuator : public SCA_PropertyActuator */

#endif


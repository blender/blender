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

/** \file KX_TrackToActuator.h
 *  \ingroup ketsji
 */

#ifndef __KX_TRACKTOACTUATOR_H__
#define __KX_TRACKTOACTUATOR_H__

#include "SCA_IActuator.h"
#include "SCA_IObject.h"
#include "MT_Matrix3x3.h"
#include "KX_GameObject.h"


class KX_TrackToActuator : public SCA_IActuator
{
	Py_Header
	// Object reference. Actually, we use the object's 'life'
	SCA_IObject*	m_object;
	// 3d toggle
	bool m_allow3D;
	// time field
	int m_time;
	int	m_trackTime;
	int	m_trackflag;
	int m_upflag;
	
	MT_Matrix3x3 m_parentlocalmat;
	KX_GameObject* m_parentobj;

  public:
	KX_TrackToActuator(SCA_IObject* gameobj, SCA_IObject *ob, int time,
	                   bool threedee,int trackflag,int upflag);
	virtual ~KX_TrackToActuator();
	virtual CValue* GetReplica() {
		KX_TrackToActuator* replica = new KX_TrackToActuator(*this);
		replica->ProcessReplica();
		return replica;
	};

	virtual void ProcessReplica();
	virtual bool UnlinkObject(SCA_IObject* clientobj);
	virtual void Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map);
	virtual bool Update(double curtime, bool frame);

#ifdef WITH_PYTHON

	/* Python part */

	/* These are used to get and set m_ob */
	static PyObject* pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	
#endif // WITH_PYTHON

}; /* end of class KX_TrackToActuator : public KX_EditObjectActuator */

#endif


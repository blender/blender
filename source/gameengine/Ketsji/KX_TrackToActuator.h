//
// Add object to the game world on action of this actuator
//
// $Id$
//
// ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version. The Blender
// Foundation also sells licenses for use in proprietary software under
// the Blender License.  See http://www.blender.org/BL/ for information
// about this.
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
// ***** END GPL/BL DUAL LICENSE BLOCK *****
//

#ifndef __KX_TrackToActuator
#define __KX_TrackToActuator

#include "SCA_IActuator.h"
#include "SCA_IObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class KX_TrackToActuator : public SCA_IActuator
{
	Py_Header;
	// Object reference. Actually, we use the object's 'life'
	SCA_IObject*	m_object;
	// 3d toggle
	bool m_allow3D;
	// time field
	int m_time;
	int	m_trackTime;
	int	m_trackflag;
	int m_upflag;
 public:
	KX_TrackToActuator(SCA_IObject* gameobj, SCA_IObject *ob, int time,
				       bool threedee,int trackflag,int upflag, PyTypeObject* T=&Type);
	virtual ~KX_TrackToActuator();
	virtual CValue* GetReplica() {
		KX_TrackToActuator* replica = new KX_TrackToActuator(*this);
		replica->ProcessReplica();
		// this will copy properties and so on...
		CValue::AddDataToReplica(replica);
		return replica;
	};

	virtual bool Update(double curtime,double deltatime);

	/* Python part */
	virtual PyObject*  _getattr(char *attr);
	
	/* 1. setObject */
	KX_PYMETHOD_DOC(KX_TrackToActuator,SetObject);
	/* 2. getObject */
	KX_PYMETHOD_DOC(KX_TrackToActuator,GetObject);
	/* 3. setTime */
	KX_PYMETHOD_DOC(KX_TrackToActuator,SetTime);
	/* 4. getTime */
	KX_PYMETHOD_DOC(KX_TrackToActuator,GetTime);
	/* 5. getUse3D */
	KX_PYMETHOD_DOC(KX_TrackToActuator,GetUse3D);
	/* 6. setUse3D */
	KX_PYMETHOD_DOC(KX_TrackToActuator,SetUse3D);
	
}; /* end of class KX_TrackToActuator : public KX_EditObjectActuator */

#endif


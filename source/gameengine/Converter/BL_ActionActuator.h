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

#ifndef BL_ACTIONACTUATOR
#define BL_ACTIONACTUATOR

#include "SCA_IActuator.h"
#include "MT_Point3.h"

class BL_ActionActuator : public SCA_IActuator  
{
public:
	Py_Header;
	BL_ActionActuator(SCA_IObject* gameobj,
						const STR_String& propname,
						float starttime,
						float endtime,
						struct bAction *action,
						short	playtype,
						short	blendin,
						short	priority,
						float	stride,
						PyTypeObject* T=&Type) 
		: SCA_IActuator(gameobj,T),
		m_starttime (starttime),
		m_endtime(endtime) ,
		m_localtime(starttime),
		m_lastUpdate(-1),
		m_propname(propname), 
		m_action(action),
		m_playtype(playtype),
		m_flag(0),
		m_blendin(blendin),
		m_blendframe(0),
		m_pose(NULL),
		m_userpose(NULL),
		m_blendpose(NULL),
		m_priority(priority),
		m_stridelength(stride),
		m_lastpos(0, 0, 0)
	{
	};
	virtual ~BL_ActionActuator();
	virtual	bool Update(double curtime,double deltatime);
	CValue* GetReplica();
	void ProcessReplica();

	KX_PYMETHOD_DOC(BL_ActionActuator,SetAction);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetBlendin);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetPriority);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetStart);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetEnd);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetFrame);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetProperty);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetBlendtime);
	KX_PYMETHOD_DOC(BL_ActionActuator,SetChannel);

	KX_PYMETHOD_DOC(BL_ActionActuator,GetAction);
	KX_PYMETHOD_DOC(BL_ActionActuator,GetBlendin);
	KX_PYMETHOD_DOC(BL_ActionActuator,GetPriority);
	KX_PYMETHOD_DOC(BL_ActionActuator,GetStart);
	KX_PYMETHOD_DOC(BL_ActionActuator,GetEnd);
	KX_PYMETHOD_DOC(BL_ActionActuator,GetFrame);
	KX_PYMETHOD_DOC(BL_ActionActuator,GetProperty);
//	KX_PYMETHOD(BL_ActionActuator,GetChannel);


	virtual PyObject* _getattr(char* attr);
	void SetBlendTime (float newtime);

protected:
	float	m_blendframe;
	MT_Point3	m_lastpos;
	int		m_flag;
	float	m_starttime;
	float	m_endtime;
	float	m_localtime;
	float	m_lastUpdate;
	short	m_playtype;
	float	m_blendin;
	short	m_priority;
	float	m_stridelength;
	struct bPose* m_pose;
	struct bPose* m_blendpose;
	struct bPose* m_userpose;
	STR_String	m_propname;
	struct bAction *m_action;
	
};

enum {
	ACT_FLAG_REVERSE	= 0x00000001,
	ACT_FLAG_LOCKINPUT	= 0x00000002,
	ACT_FLAG_KEYUP		= 0x00000004
};
#endif


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

#ifndef BL_ARMATUREOBJECT
#define BL_ARMATUREOBJECT

#include "KX_GameObject.h"

#include "SG_IObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class BL_ActionActuator;

class BL_ArmatureObject : public KX_GameObject  
{
public:
	double GetLastFrame ();
	short GetActivePriority();
	virtual void ProcessReplica(BL_ArmatureObject *replica);
	class BL_ActionActuator * GetActiveAction();
	BL_ArmatureObject(void* sgReplicationInfo, SG_Callbacks callbacks,
		struct bArmature *arm,
		struct bPose *pose) :
	KX_GameObject(sgReplicationInfo,callbacks),
		m_pose(pose),
		m_mrdPose(NULL),
		m_armature(arm),
		m_activeAct(NULL),
		m_activePriority(999)
	{}

	virtual CValue*		GetReplica();
	virtual				~BL_ArmatureObject();
	void GetMRDPose(bPose **pose);
	void	GetPose(struct bPose **pose);
	void SetPose (struct bPose *pose);
	void ApplyPose();
	bool SetActiveAction(class BL_ActionActuator *act, short priority, double curtime);
	struct bArmature * GetArmature(){return m_armature;};

protected:
	struct bArmature	*m_armature;
	struct bPose		*m_pose;
	struct bPose		*m_mrdPose;
	double	m_lastframe;
	class BL_ActionActuator *m_activeAct;
	short	m_activePriority;
};

#endif


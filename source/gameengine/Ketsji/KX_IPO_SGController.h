/**
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
#ifndef __IPO_SGCONTROLLER_H
#define __IPO_SGCONTROLLER_H

#include "SG_Controller.h"
#include "SG_Spatial.h"

#include "KX_IPOTransform.h"
#include "KX_IInterpolator.h"

class KX_IpoSGController : public SG_Controller
{
	KX_IPOTransform     m_ipo_xform;
	T_InterpolatorList  m_interpolators;
	/* Why not bools? */
	short               m_modify_position	 : 1;
	short               m_modify_orientation : 1;
	short               m_modify_scaling     : 1;

	/** Interpret the ipo as a force rather than a displacement? */
	bool                m_ipo_as_force;

	/** Ipo-as-force acts in local rather than in global coordinates? */
	bool                m_force_ipo_acts_local;

	/** Were settings altered since the last update? */
	bool				m_modified;

	/** Local time of this ipo.*/
	double		        m_ipotime;

	/** A reference to the original game object. */
	class KX_GameObject* m_game_object;

public:
	KX_IpoSGController();

	virtual ~KX_IpoSGController();

	virtual	SG_Controller*	GetReplica(class SG_Node* destnode);

		void
	SetOption(
		int option,
		int value
	);

	/** Set sumo data. */
	void UpdateSumoReference();
	/** Set reference to the corresponding game object. */
	void SetGameObject(class KX_GameObject*);

	void	SetModifyPosition(bool modifypos) {	
		m_modify_position=modifypos;
	}
	void	SetModifyOrientation(bool modifyorient) {	
		m_modify_orientation=modifyorient;
	}
	void	SetModifyScaling(bool modifyscale) {	
		m_modify_scaling=modifyscale;
	}
	
	KX_IPOTransform& GetIPOTransform()
	{
		return m_ipo_xform;
	}
	void	AddInterpolator(KX_IInterpolator* interp);
	virtual bool Update(double time);
	virtual void	SetSimulatedTime(double time)
	{
		m_ipotime = time;
		m_modified = true;
	}
};

#endif //__IPO_SGCONTROLLER_H


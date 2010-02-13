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
#ifndef __IPO_SGCONTROLLER_H
#define __IPO_SGCONTROLLER_H

#include "SG_Controller.h"
#include "SG_Spatial.h"

#include "KX_IPOTransform.h"
#include "KX_IInterpolator.h"

#define KX_MAX_IPO_CHANNELS 19	//note- [0] is not used

class KX_IpoSGController : public SG_Controller
{
	KX_IPOTransform     m_ipo_xform;
	T_InterpolatorList  m_interpolators;

	/** Flag for each IPO channel that can be applied to a game object */
	bool				m_ipo_channels_active[KX_MAX_IPO_CHANNELS];

	/** Interpret the ipo as a force rather than a displacement? */
	bool                m_ipo_as_force;

	/** Add Ipo curve to current loc/rot/scale */
	bool                m_ipo_add;

	/** Ipo must be applied in local coordinate rather than in global coordinates (used for force and Add mode)*/
	bool                m_ipo_local;
	
	/** Were settings altered since the last update? */
	bool				m_modified;

	/** Local time of this ipo.*/
	double		        m_ipotime;

	/** Location of the object when the IPO is first fired (for local transformations) */
	class MT_Point3		m_ipo_start_point;

	/** Orientation of the object when the IPO is first fired (for local transformations) */
	class MT_Matrix3x3	m_ipo_start_orient;

	/** Scale of the object when the IPO is first fired (for local transformations) */
	class MT_Vector3	m_ipo_start_scale;

	/** if IPO initial position has been set for local normal IPO */
	bool				m_ipo_start_initialized;

	/** Euler angles at the start of the game, needed for incomplete ROT Ipo curves */
	class MT_Vector3	m_ipo_start_euler;

	/** true is m_ipo_start_euler has been initialized */
	bool				m_ipo_euler_initialized;

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

	void SetIPOChannelActive(int index, bool value) {
		//indexes found in makesdna\DNA_ipo_types.h
		m_ipo_channels_active[index] = value;
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
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_IpoSGController"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__IPO_SGCONTROLLER_H



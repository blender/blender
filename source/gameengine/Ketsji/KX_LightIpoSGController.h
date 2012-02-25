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

/** \file KX_LightIpoSGController.h
 *  \ingroup ketsji
 */

#ifndef __KX_LIGHTIPOSGCONTROLLER_H__
#define __KX_LIGHTIPOSGCONTROLLER_H__

#include "SG_Controller.h"
#include "SG_Spatial.h"

#include "KX_IInterpolator.h"

struct RAS_LightObject;

class KX_LightIpoSGController : public SG_Controller
{
public:
	MT_Scalar           m_energy;
	MT_Scalar           m_col_rgb[3];
	MT_Scalar           m_dist;

private:
	T_InterpolatorList	m_interpolators;
	unsigned short  	m_modify_energy 	 : 1;
	unsigned short	    m_modify_color       : 1;
	unsigned short		m_modify_dist    	 : 1;
	bool				m_modified;

	double		        m_ipotime;
public:
	KX_LightIpoSGController() : 
				m_modify_energy(false),
				m_modify_color(false),
				m_modify_dist(false),
				m_modified(true),
				m_ipotime(0.0)
		{}

	virtual ~KX_LightIpoSGController();

	virtual	SG_Controller*	GetReplica(class SG_Node* destnode);

	virtual bool Update(double time);
	
	virtual void SetSimulatedTime(double time) {
		m_ipotime = time;
		m_modified = true;
	}

	void	SetModifyEnergy(bool modify) {	
		m_modify_energy = modify;
	}

	void	SetModifyColor(bool modify) {	
		m_modify_color = modify;
	}

	void	SetModifyDist(bool modify) {	
		m_modify_dist = modify;
	}

			void
	SetOption(
		int option,
		int value
	){
		// intentionally empty
	};

	void	AddInterpolator(KX_IInterpolator* interp);
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_LightIpoSGController"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // __KX_LIGHTIPOSGCONTROLLER_H__


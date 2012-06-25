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

/** \file KX_CameraIpoSGController.h
 *  \ingroup ketsji
 */

#ifndef __KX_CAMERAIPOSGCONTROLLER_H__
#define __KX_CAMERAIPOSGCONTROLLER_H__

#include "SG_Controller.h"
#include "SG_Spatial.h"

#include "KX_IInterpolator.h"

struct RAS_CameraData;

class KX_CameraIpoSGController : public SG_Controller
{
public:
	MT_Scalar           m_lens;
	MT_Scalar           m_clipstart;
	MT_Scalar           m_clipend;

private:
	T_InterpolatorList	m_interpolators;
	unsigned short  	m_modify_lens 	 : 1;
	unsigned short	    m_modify_clipstart       : 1;
	unsigned short		m_modify_clipend    	 : 1;
	bool				m_modified;

	double		        m_ipotime;
public:
	KX_CameraIpoSGController() : 
				m_modify_lens(false),
				m_modify_clipstart(false),
				m_modify_clipend(false),
				m_modified(true),
				m_ipotime(0.0)
		{}

	~KX_CameraIpoSGController();
	SG_Controller*	GetReplica(class SG_Node* destnode);
	bool Update(double time);

		void
	SetOption(
		int option,
		int value
	);

	void SetSimulatedTime(double time) {
		m_ipotime = time;
		m_modified = true;
	}
	void	SetModifyLens(bool modify) {	
		m_modify_lens = modify;
	}
	void	SetModifyClipEnd(bool modify) {	
		m_modify_clipend = modify;
	}
	void	SetModifyClipStart(bool modify) {	
		m_modify_clipstart = modify;
	}
	void	AddInterpolator(KX_IInterpolator* interp);
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:KX_CameraIpoSGController")
#endif
};

#endif // __KX_CAMERAIPOSGCONTROLLER_H__


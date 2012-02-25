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

/** \file KX_ScalarInterpolator.h
 *  \ingroup ketsji
 */

#ifndef __KX_SCALARINTERPOLATOR_H__
#define __KX_SCALARINTERPOLATOR_H__

#include "MT_Scalar.h"
#include "KX_IInterpolator.h"

class KX_IScalarInterpolator;

class KX_ScalarInterpolator : public KX_IInterpolator {
public:
	KX_ScalarInterpolator(MT_Scalar* target, 
						  KX_IScalarInterpolator *ipo) :
		m_target(target),
		m_ipo(ipo)
		{}
	
	virtual ~KX_ScalarInterpolator() {}
	virtual void Execute(float currentTime) const;
	void		SetNewTarget(MT_Scalar* newtarget)
	{
		m_target=newtarget;
	}
	MT_Scalar*	GetTarget()
	{
		return m_target;
	}
private:
	MT_Scalar*               m_target;
	KX_IScalarInterpolator *m_ipo;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:KX_ScalarInterpolator"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif


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

/** \file KX_BlenderScalarInterpolator.h
 *  \ingroup bgeconv
 */

#ifndef __KX_BLENDERSCALARINTERPOLATOR_H__
#define __KX_BLENDERSCALARINTERPOLATOR_H__

#include <vector>

#include "KX_IScalarInterpolator.h"

typedef unsigned short BL_IpoChannel;

class BL_ScalarInterpolator : public KX_IScalarInterpolator {
public:
	BL_ScalarInterpolator() {} // required for use in STL list
	BL_ScalarInterpolator(struct FCurve* fcu) :
		m_fcu(fcu)
		{}

	virtual ~BL_ScalarInterpolator() {}
	
	virtual float GetValue(float currentTime) const;
	struct FCurve *GetFCurve() { return m_fcu; }

private:
	struct FCurve *m_fcu;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_ScalarInterpolator"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};


class BL_InterpolatorList : public std::vector<KX_IScalarInterpolator *> {
public:
	BL_InterpolatorList(struct bAction *action);
	~BL_InterpolatorList();

	KX_IScalarInterpolator *GetScalarInterpolator(const char *rna_path, int array_index);	


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_InterpolatorList"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_BLENDERSCALARINTERPOLATOR_H__


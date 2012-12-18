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

#ifndef __KX_STORAGE
#define __KX_STORAGE

#include "RAS_MaterialBucket.h"

enum RAS_STORAGE_TYPE	{
	RAS_AUTO_STORAGE,
	RAS_IMMEDIATE,
	RAS_VA,
	RAS_VBO
};

class RAS_IStorage
{

public:
	virtual ~RAS_IStorage() {};

	virtual bool	Init()=0;
	virtual void	Exit()=0;

	virtual void	IndexPrimitives(RAS_MeshSlot& ms)=0;
	virtual void	IndexPrimitivesMulti(class RAS_MeshSlot& ms)=0;

	virtual void	SetDrawingMode(int drawingmode)=0;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_IStorage"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif //__KX_STORAGE

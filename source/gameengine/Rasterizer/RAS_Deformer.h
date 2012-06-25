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

/** \file RAS_Deformer.h
 *  \ingroup bgerast
 */

#ifndef __RAS_DEFORMER_H__
#define __RAS_DEFORMER_H__

#if defined(WIN32) && !defined(FREE_WINDOWS)
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include <stdlib.h>
#include "CTR_Map.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

struct DerivedMesh;
class RAS_MeshObject;

class RAS_Deformer
{
public:
	RAS_Deformer() : m_pMesh(NULL), m_bDynamic(false) {};
	virtual ~RAS_Deformer(){};
	virtual void Relink(CTR_Map<class CTR_HashedPtr, void*>*map)=0;
	virtual bool Apply(class RAS_IPolyMaterial *polymat)=0;
	virtual bool Update(void)=0;
	virtual bool UpdateBuckets(void)=0;
	virtual RAS_Deformer *GetReplica()=0;
	virtual void ProcessReplica()=0;
	virtual bool SkipVertexTransform()
	{
		return false;
	}
	virtual bool ShareVertexArray()
	{
		return true;
	}
	virtual bool UseVertexArray()
	{
		return true;
	}
	// true when deformer produces varying vertex (shape or armature)
	bool IsDynamic()
	{
		return m_bDynamic;
	}
	virtual struct DerivedMesh* GetFinalMesh()
	{
		return NULL;
	}
	virtual struct DerivedMesh* GetPhysicsMesh()
	{
		return NULL;
	}
	virtual class RAS_MeshObject* GetRasMesh()
	{
		/* m_pMesh does not seem to be being used?? */
		return NULL;
	}
	virtual float (* GetTransVerts(int *tot))[3]	{	*tot= 0; return NULL; }

protected:
	class RAS_MeshObject	*m_pMesh;
	bool  m_bDynamic;	


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_Deformer")
#endif
};

#endif


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

/** \file BL_DeformableGameObject.h
 *  \ingroup bgeconv
 */

#ifndef __BL_DEFORMABLEGAMEOBJECT_H__
#define __BL_DEFORMABLEGAMEOBJECT_H__

#if defined(WIN32) && !defined(FREE_WINDOWS)
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "DNA_mesh_types.h"
#include "KX_GameObject.h"
#include "BL_MeshDeformer.h"
#include "KX_SoftBodyDeformer.h"
#include <vector>

class BL_ShapeActionActuator;
struct Key;

class BL_DeformableGameObject : public KX_GameObject  
{
public:
	CValue*		GetReplica();

	double GetLastFrame ()
	{
		return m_lastframe;
	}
	Object* GetBlendObject()
	{
		return m_blendobj;
	}
	virtual void Relink(CTR_Map<CTR_HashedPtr, void*>*map)
	{
		if (m_pDeformer)
			m_pDeformer->Relink (map);
		KX_GameObject::Relink(map);
	};
	void ProcessReplica();

	BL_DeformableGameObject(Object* blendobj, void* sgReplicationInfo, SG_Callbacks callbacks) :
		KX_GameObject(sgReplicationInfo,callbacks),
		m_pDeformer(NULL),
		m_activeAct(NULL),
		m_lastframe(0.),
		m_blendobj(blendobj),
		m_activePriority(9999)
	{
		m_isDeformable = true;
	};
	virtual ~BL_DeformableGameObject();
	bool SetActiveAction(class BL_ShapeActionActuator *act, short priority, double curtime);

	bool GetShape(vector<float> &shape);
	
	virtual void	SetDeformer(class RAS_Deformer* deformer);
	virtual class RAS_Deformer* GetDeformer()
	{
		return m_pDeformer;
	}

public:
	
protected:	
	
	RAS_Deformer		*m_pDeformer;

	class BL_ShapeActionActuator *m_activeAct;
	double		m_lastframe;
	Object*		m_blendobj;
	short		m_activePriority;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_DeformableGameObject"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif


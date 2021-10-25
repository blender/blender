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

/** \file BL_MeshDeformer.h
 *  \ingroup bgeconv
 */

#ifndef __BL_MESHDEFORMER_H__
#define __BL_MESHDEFORMER_H__

#include "RAS_Deformer.h"
#include "DNA_object_types.h"
#include "DNA_key_types.h"
#include "MT_Point3.h"
#include <stdlib.h>

#ifdef _MSC_VER
#  pragma warning (disable:4786)  /* get rid of stupid stl-visual compiler debug warning */
#endif

class BL_DeformableGameObject;

class BL_MeshDeformer : public RAS_Deformer
{
public:
	void VerifyStorage();
	void RecalcNormals();
	virtual void Relink(CTR_Map<class CTR_HashedPtr, void*>*map);
	BL_MeshDeformer(BL_DeformableGameObject *gameobj,
					struct Object* obj,
					class RAS_MeshObject *meshobj ):
		m_pMeshObject(meshobj),
		m_bmesh((struct Mesh*)(obj->data)),
		m_transverts(0),
		m_transnors(0),
		m_objMesh(obj),
		m_tvtot(0),
		m_gameobj(gameobj),
		m_lastDeformUpdate(-1)
	{};
	virtual ~BL_MeshDeformer();
	virtual void SetSimulatedTime(double time) {}
	virtual bool Apply(class RAS_IPolyMaterial *mat);
	virtual bool Update(void) { return false; }
	virtual bool UpdateBuckets(void) { return false; }
	virtual	RAS_Deformer*	GetReplica() {return NULL;}
	virtual void ProcessReplica();
	struct Mesh* GetMesh() { return m_bmesh; }
	virtual class RAS_MeshObject* GetRasMesh() { return m_pMeshObject; }
	virtual float (* GetTransVerts(int *tot))[3]	{	*tot= m_tvtot; return m_transverts; }
	//	virtual void InitDeform(double time) {}

protected:
	class RAS_MeshObject*		m_pMeshObject;
	struct Mesh*				m_bmesh;
	
	// this is so m_transverts doesn't need to be converted
	// before deformation
	float						(*m_transverts)[3];
	float 						(*m_transnors)[3];
	struct Object*				m_objMesh; 
	// --
	int							m_tvtot;
	BL_DeformableGameObject*	m_gameobj;
	double						m_lastDeformUpdate;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_MeshDeformer")
#endif
};

#endif


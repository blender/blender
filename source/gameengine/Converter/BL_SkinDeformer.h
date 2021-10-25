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

/** \file BL_SkinDeformer.h
 *  \ingroup bgeconv
 */

#ifndef __BL_SKINDEFORMER_H__
#define __BL_SKINDEFORMER_H__

#ifdef _MSC_VER
#  pragma warning (disable:4786)  /* get rid of stupid stl-visual compiler debug warning */
#endif  /* WIN32 */

#include "CTR_HashedPtr.h"
#include "BL_MeshDeformer.h"
#include "BL_ArmatureObject.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "BKE_armature.h"

#include "RAS_Deformer.h"


class BL_SkinDeformer : public BL_MeshDeformer  
{
public:
//	void SetArmatureController (BL_ArmatureController *cont);
	virtual void Relink(CTR_Map<class CTR_HashedPtr, void*>*map);
	void SetArmature (class BL_ArmatureObject *armobj);

	BL_SkinDeformer(BL_DeformableGameObject *gameobj,
	                struct Object *bmeshobj,
	                class RAS_MeshObject *mesh,
	                BL_ArmatureObject* arma = NULL);

	/* this second constructor is needed for making a mesh deformable on the fly. */
	BL_SkinDeformer(BL_DeformableGameObject *gameobj,
	                struct Object *bmeshobj_old,
	                struct Object *bmeshobj_new,
	                class RAS_MeshObject *mesh,
	                bool release_object,
	                bool recalc_normal,
	                BL_ArmatureObject* arma = NULL);

	virtual RAS_Deformer *GetReplica();
	virtual void ProcessReplica();

	virtual ~BL_SkinDeformer();
	bool Update (void);
	bool UpdateInternal (bool shape_applied);
	bool Apply (class RAS_IPolyMaterial *polymat);
	bool UpdateBuckets(void) 
	{
		// update the deformer and all the mesh slots; Apply() does it well, so just call it.
		return Apply(NULL);
	}
	bool PoseUpdated(void)
		{ 
			if (m_armobj && m_lastArmaUpdate!=m_armobj->GetLastFrame()) {
				return true;
			}
			return false;
		}

	void ForceUpdate()
	{
		m_lastArmaUpdate = -1.0;
	};
	virtual bool ShareVertexArray()
	{
		return false;
	}

protected:
	BL_ArmatureObject*		m_armobj;	//	Our parent object
	float					m_time;
	double					m_lastArmaUpdate;
	//ListBase*				m_defbase;
	float					m_obmat[4][4];	// the reference matrix for skeleton deform
	bool					m_releaseobject;
	bool					m_poseApplied;
	bool					m_recalcNormal;
	bool					m_copyNormals; // dirty flag so we know if Apply() needs to copy normal information (used for BGEDeformVerts())
	struct bPoseChannel**	m_dfnrToPC;
	short					m_deformflags;

	void BlenderDeformVerts();
	void BGEDeformVerts();

	void UpdateTransverts();


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_SkinDeformer")
#endif
};

#endif  /* __BL_SKINDEFORMER_H__ */

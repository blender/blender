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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifdef WIN32
#pragma warning (disable : 4786)
#endif //WIN32

#include "BL_SkinDeformer.h"
#include "GEN_Map.h"
#include "STR_HashedString.h"
#include "RAS_IPolygonMaterial.h"
#include "RAS_MeshObject.h"

//#include "BL_ArmatureController.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "MT_Point3.h"

extern "C"{
	#include "BKE_lattice.h"
}
 #include "BKE_utildefines.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#define __NLA_DEFNORMALS
//#undef __NLA_DEFNORMALS

BL_SkinDeformer::BL_SkinDeformer(BL_DeformableGameObject *gameobj,
								struct Object *bmeshobj, 
								class RAS_MeshObject *mesh,
								BL_ArmatureObject* arma)
							:	//
							BL_MeshDeformer(gameobj, bmeshobj, mesh),
							m_armobj(arma),
							m_lastArmaUpdate(-1),
							//m_defbase(&bmeshobj->defbase),
							m_releaseobject(false),
							m_poseApplied(false),
							m_recalcNormal(true)
{
	copy_m4_m4(m_obmat, bmeshobj->obmat);
};

BL_SkinDeformer::BL_SkinDeformer(
	BL_DeformableGameObject *gameobj,
	struct Object *bmeshobj_old,	// Blender object that owns the new mesh
	struct Object *bmeshobj_new,	// Blender object that owns the original mesh
	class RAS_MeshObject *mesh,
	bool release_object,
	bool recalc_normal,
	BL_ArmatureObject* arma)	:	
		BL_MeshDeformer(gameobj, bmeshobj_old, mesh),
		m_armobj(arma),
		m_lastArmaUpdate(-1),
		//m_defbase(&bmeshobj_old->defbase),
		m_releaseobject(release_object),
		m_recalcNormal(recalc_normal)
	{
		// this is needed to ensure correct deformation of mesh:
		// the deformation is done with Blender's armature_deform_verts() function
		// that takes an object as parameter and not a mesh. The object matrice is used
		// in the calculation, so we must use the matrix of the original object to
		// simulate a pure replacement of the mesh.
		copy_m4_m4(m_obmat, bmeshobj_new->obmat);
	}

BL_SkinDeformer::~BL_SkinDeformer()
{
	if(m_releaseobject && m_armobj)
		m_armobj->Release();
}

void BL_SkinDeformer::Relink(GEN_Map<class GEN_HashedPtr, void*>*map)
{
	if (m_armobj) {
		void **h_obj = (*map)[m_armobj];

		if (h_obj)
			m_armobj = (BL_ArmatureObject*)(*h_obj);
		else
			m_armobj=NULL;
	}

	BL_MeshDeformer::Relink(map);
}

bool BL_SkinDeformer::Apply(RAS_IPolyMaterial *mat)
{
	RAS_MeshSlot::iterator it;
	RAS_MeshMaterial *mmat;
	RAS_MeshSlot *slot;
	size_t i, nmat, imat;

	// update the vertex in m_transverts
	if (!Update())
		return false;

	if (m_transverts) {
		// the vertex cache is unique to this deformer, no need to update it
		// if it wasn't updated! We must update all the materials at once
		// because we will not get here again for the other material
		nmat = m_pMeshObject->NumMaterials();
		for (imat=0; imat<nmat; imat++) {
			mmat = m_pMeshObject->GetMeshMaterial(imat);
			if(!mmat->m_slots[(void*)m_gameobj])
				continue;

			slot = *mmat->m_slots[(void*)m_gameobj];

			// for each array
			for(slot->begin(it); !slot->end(it); slot->next(it)) {
				// for each vertex
				// copy the untransformed data from the original mvert
				for(i=it.startvertex; i<it.endvertex; i++) {
					RAS_TexVert& v = it.vertex[i];
					v.SetXYZ(m_transverts[v.getOrigIndex()]);
				}
			}
		}
	}
	return true;
}

RAS_Deformer *BL_SkinDeformer::GetReplica()
{
	BL_SkinDeformer *result;

	result = new BL_SkinDeformer(*this);
	/* there is m_armobj that must be fixed but we cannot do it now, it will be done in Relink */
	result->ProcessReplica();
	return result;
}

void BL_SkinDeformer::ProcessReplica()
{
	BL_MeshDeformer::ProcessReplica();
	m_lastArmaUpdate = -1;
	m_releaseobject = false;
}

//void where_is_pose (Object *ob);
//void armature_deform_verts(Object *armOb, Object *target, float (*vertexCos)[3], int numVerts, int deformflag); 
bool BL_SkinDeformer::UpdateInternal(bool shape_applied)
{
	/* See if the armature has been updated for this frame */
	if (PoseUpdated()){	
		float obmat[4][4];	// the original object matrice 

		/* XXX note: where_is_pose() (from BKE_armature.h) calculates all matrices needed to start deforming */
		/* but it requires the blender object pointer... */
		Object* par_arma = m_armobj->GetArmatureObject();

		if(!shape_applied) {
			/* store verts locally */
			VerifyStorage();
		
			/* duplicate */
			for (int v =0; v<m_bmesh->totvert; v++)
				VECCOPY(m_transverts[v], m_bmesh->mvert[v].co);
		}

		m_armobj->ApplyPose();

		// save matrix first
		copy_m4_m4(obmat, m_objMesh->obmat);
		// set reference matrix
		copy_m4_m4(m_objMesh->obmat, m_obmat);

		armature_deform_verts( par_arma, m_objMesh, NULL, m_transverts, NULL, m_bmesh->totvert, ARM_DEF_VGROUP, NULL, NULL );
		
		// restore matrix 
		copy_m4_m4(m_objMesh->obmat, obmat);

#ifdef __NLA_DEFNORMALS
		if (m_recalcNormal)
			RecalcNormals();
#endif

		/* Update the current frame */
		m_lastArmaUpdate=m_armobj->GetLastFrame();

		m_armobj->RestorePose();
		/* dynamic vertex, cannot use display list */
		m_bDynamic = true;
		/* indicate that the m_transverts and normals are up to date */
		return true;
	}

	return false;
}

bool BL_SkinDeformer::Update(void)
{
	return UpdateInternal(false);
}

/* XXX note: I propose to drop this function */
void BL_SkinDeformer::SetArmature(BL_ArmatureObject *armobj)
{
	// only used to set the object now
	m_armobj = armobj;
}

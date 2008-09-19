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
#include "BL_SkinMeshObject.h"

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
#include "BLI_arithb.h"

#define __NLA_DEFNORMALS
//#undef __NLA_DEFNORMALS

BL_SkinDeformer::BL_SkinDeformer(BL_DeformableGameObject *gameobj,
								struct Object *bmeshobj, 
								class BL_SkinMeshObject *mesh,
								BL_ArmatureObject* arma)
							:	//
							BL_MeshDeformer(gameobj, bmeshobj, mesh),
							m_armobj(arma),
							m_lastArmaUpdate(-1),
							m_defbase(&bmeshobj->defbase),
							m_releaseobject(false),
							m_poseApplied(false)
{
	Mat4CpyMat4(m_obmat, bmeshobj->obmat);
};

BL_SkinDeformer::BL_SkinDeformer(
	BL_DeformableGameObject *gameobj,
	struct Object *bmeshobj_old,	// Blender object that owns the new mesh
	struct Object *bmeshobj_new,	// Blender object that owns the original mesh
	class BL_SkinMeshObject *mesh,
	bool release_object,
	BL_ArmatureObject* arma)	:	
		BL_MeshDeformer(gameobj, bmeshobj_old, mesh),
		m_armobj(arma),
		m_lastArmaUpdate(-1),
		m_defbase(&bmeshobj_old->defbase),
		m_releaseobject(release_object)
	{
		// this is needed to ensure correct deformation of mesh:
		// the deformation is done with Blender's armature_deform_verts() function
		// that takes an object as parameter and not a mesh. The object matrice is used
		// in the calculation, so we must use the matrix of the original object to
		// simulate a pure replacement of the mesh.
		Mat4CpyMat4(m_obmat, bmeshobj_new->obmat);
	}

BL_SkinDeformer::~BL_SkinDeformer()
{
	if(m_releaseobject && m_armobj)
		m_armobj->Release();
}

bool BL_SkinDeformer::Apply(RAS_IPolyMaterial *mat)
{
	size_t i, j;

	// update the vertex in m_transverts
	Update();

	// The vertex cache can only be updated for this deformer:
	// Duplicated objects with more than one ploymaterial (=multiple mesh slot per object)
	// share the same mesh (=the same cache). As the rendering is done per polymaterial
	// cycling through the objects, the entire mesh cache cannot be updated in one shot.
	vecVertexArray& vertexarrays = m_pMeshObject->GetVertexCache(mat);

	// For each array
	for (i=0; i<vertexarrays.size(); i++) {
		KX_VertexArray& vertexarray = (*vertexarrays[i]);

		// For each vertex
		// copy the untransformed data from the original mvert
		for (j=0; j<vertexarray.size(); j++) {
			RAS_TexVert& v = vertexarray[j];
			v.SetXYZ(m_transverts[v.getOrigIndex()]);
		}
	}

	return true;
}

RAS_Deformer *BL_SkinDeformer::GetReplica()
{
	BL_SkinDeformer *result;

	result = new BL_SkinDeformer(*this);
	result->ProcessReplica();
	return result;
}

void BL_SkinDeformer::ProcessReplica()
{
}

//void where_is_pose (Object *ob);
//void armature_deform_verts(Object *armOb, Object *target, float (*vertexCos)[3], int numVerts, int deformflag); 
bool BL_SkinDeformer::Update(void)
{
	/* See if the armature has been updated for this frame */
	if (PoseUpdated()){	
		float obmat[4][4];	// the original object matrice 
		
		/* XXX note: where_is_pose() (from BKE_armature.h) calculates all matrices needed to start deforming */
		/* but it requires the blender object pointer... */
		Object* par_arma = m_armobj->GetArmatureObject();
		if (!PoseApplied()){
			m_armobj->ApplyPose();
			where_is_pose( par_arma ); 
		}

		/* store verts locally */
		VerifyStorage();
	
		/* duplicate */
		for (int v =0; v<m_bmesh->totvert; v++)
			VECCOPY(m_transverts[v], m_bmesh->mvert[v].co);

		// save matrix first
		Mat4CpyMat4(obmat, m_objMesh->obmat);
		// set reference matrix
		Mat4CpyMat4(m_objMesh->obmat, m_obmat);

		armature_deform_verts( par_arma, m_objMesh, NULL, m_transverts, NULL, m_bmesh->totvert, ARM_DEF_VGROUP, NULL, NULL );
		
		// restore matrix 
		Mat4CpyMat4(m_objMesh->obmat, obmat);

#ifdef __NLA_DEFNORMALS
		RecalcNormals();
#endif

		/* Update the current frame */
		m_lastArmaUpdate=m_armobj->GetLastFrame();
		/* reset for next frame */
		PoseApplied(false);
		/* indicate that the m_transverts and normals are up to date */
		return true;
	}
	return false;
}

/* XXX note: I propose to drop this function */
void BL_SkinDeformer::SetArmature(BL_ArmatureObject *armobj)
{
	// only used to set the object now
	m_armobj = armobj;
}

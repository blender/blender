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

/** \file gameengine/Converter/BL_ModifierDeformer.cpp
 *  \ingroup bgeconv
 */


#if defined(WIN32) && !defined(FREE_WINDOWS)
#pragma warning (disable : 4786)
#endif //WIN32

#include "MEM_guardedalloc.h"
#include "BL_ModifierDeformer.h"
#include "CTR_Map.h"
#include "STR_HashedString.h"
#include "RAS_IPolygonMaterial.h"
#include "RAS_MeshObject.h"
#include "PHY_IGraphicController.h"

//#include "BL_ArmatureController.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "BLI_utildefines.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_key.h"
#include "BKE_ipo.h"
#include "MT_Point3.h"

extern "C"{
	#include "BKE_customdata.h"
	#include "BKE_DerivedMesh.h"
	#include "BKE_lattice.h"
	#include "BKE_modifier.h"
}
 

#include "BLI_blenlib.h"
#include "BLI_math.h"

#define __NLA_DEFNORMALS
//#undef __NLA_DEFNORMALS


BL_ModifierDeformer::~BL_ModifierDeformer()
{
	if (m_dm) {
		// deformedOnly is used as a user counter
		if (--m_dm->deformedOnly == 0) {
			m_dm->needsFree = 1;
			m_dm->release(m_dm);
		}
	}
};

RAS_Deformer *BL_ModifierDeformer::GetReplica()
{
	BL_ModifierDeformer *result;

	result = new BL_ModifierDeformer(*this);
	result->ProcessReplica();
	return result;
}

void BL_ModifierDeformer::ProcessReplica()
{
	/* Note! - This is not inherited from PyObjectPlus */
	BL_ShapeDeformer::ProcessReplica();
	if (m_dm)
		// by default try to reuse mesh, deformedOnly is used as a user count
		m_dm->deformedOnly++;
	// this will force an update and if the mesh cannot be reused, a new one will be created
	m_lastModifierUpdate = -1;
}

bool BL_ModifierDeformer::HasCompatibleDeformer(Object *ob)
{
	if (!ob->modifiers.first)
		return false;
	// soft body cannot use mesh modifiers
	if ((ob->gameflag & OB_SOFT_BODY) != 0)
		return false;
	ModifierData* md;
	for (md = (ModifierData*)ob->modifiers.first; md; md = (ModifierData*)md->next) {
		if (modifier_dependsOnTime(md))
			continue;
		if (!(md->mode & eModifierMode_Realtime))
			continue;
		/* armature modifier are handled by SkinDeformer, not ModifierDeformer */
		if (md->type == eModifierType_Armature )
			continue;
		return true;
	}
	return false;
}

bool BL_ModifierDeformer::HasArmatureDeformer(Object *ob)
{
	if (!ob->modifiers.first)
		return false;

	ModifierData* md = (ModifierData*)ob->modifiers.first;
	if (md->type == eModifierType_Armature )
		return true;

	return false;
}

// return a deformed mesh that supports mapping (with a valid CD_ORIGINDEX layer)
struct DerivedMesh* BL_ModifierDeformer::GetPhysicsMesh()
{
	/* we need to compute the deformed mesh taking into account the current
	 * shape and skin deformers, we cannot just call mesh_create_derived_physics()
	 * because that would use the m_transvers already deformed previously by BL_ModifierDeformer::Update(),
	 * so restart from scratch by forcing a full update the shape/skin deformers 
	 * (will do nothing if there is no such deformer) */
	BL_ShapeDeformer::ForceUpdate();
	BL_ShapeDeformer::Update();
	// now apply the modifiers but without those that don't support mapping
	Object* blendobj = m_gameobj->GetBlendObject();
	/* hack: the modifiers require that the mesh is attached to the object
	 * It may not be the case here because of replace mesh actuator */
	Mesh *oldmesh = (Mesh*)blendobj->data;
	blendobj->data = m_bmesh;
	DerivedMesh *dm = mesh_create_derived_physics(m_scene, blendobj, m_transverts, CD_MASK_MESH);
	/* restore object data */
	blendobj->data = oldmesh;
	/* m_transverts is correct here (takes into account deform only modifiers) */
	/* the derived mesh returned by this function must be released by the caller !!! */
	return dm;
}

bool BL_ModifierDeformer::Update(void)
{
	bool bShapeUpdate = BL_ShapeDeformer::Update();

	if (bShapeUpdate || m_lastModifierUpdate != m_gameobj->GetLastFrame()) {
		// static derived mesh are not updated
		if (m_dm == NULL || m_bDynamic) {
			/* execute the modifiers */
			Object* blendobj = m_gameobj->GetBlendObject();
			/* hack: the modifiers require that the mesh is attached to the object
			 * It may not be the case here because of replace mesh actuator */
			Mesh *oldmesh = (Mesh*)blendobj->data;
			blendobj->data = m_bmesh;
			/* execute the modifiers */		
			DerivedMesh *dm = mesh_create_derived_no_virtual(m_scene, blendobj, m_transverts, CD_MASK_MESH);
			/* restore object data */
			blendobj->data = oldmesh;
			/* free the current derived mesh and replace, (dm should never be NULL) */
			if (m_dm != NULL) {
				// HACK! use deformedOnly as a user counter
				if (--m_dm->deformedOnly == 0) {
					m_dm->needsFree = 1;
					m_dm->release(m_dm);
				}
			}
			m_dm = dm;
			// get rid of temporary data
			m_dm->needsFree = 0;
			m_dm->release(m_dm);
			// HACK! use deformedOnly as a user counter
			m_dm->deformedOnly = 1;
			/* update the graphic controller */
			PHY_IGraphicController *ctrl = m_gameobj->GetGraphicController();
			if (ctrl) {
				float min_r[3], max_r[3];
				INIT_MINMAX(min_r, max_r);
				m_dm->getMinMax(m_dm, min_r, max_r);
				ctrl->setLocalAabb(min_r, max_r);
			}
		}
		m_lastModifierUpdate=m_gameobj->GetLastFrame();
		bShapeUpdate = true;
	}
	return bShapeUpdate;
}

bool BL_ModifierDeformer::Apply(RAS_IPolyMaterial *mat)
{
	if (!Update())
		return false;

	// drawing is based on derived mesh, must set it in the mesh slots
	int nmat = m_pMeshObject->NumMaterials();
	for (int imat=0; imat<nmat; imat++) {
		RAS_MeshMaterial *mmat = m_pMeshObject->GetMeshMaterial(imat);
		RAS_MeshSlot **slot = mmat->m_slots[(void*)m_gameobj];
		if (!slot || !*slot)
			continue;
		(*slot)->m_pDerivedMesh = m_dm;
	}
	return true;
}

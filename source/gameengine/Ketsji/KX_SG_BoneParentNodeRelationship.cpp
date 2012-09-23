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

/** \file gameengine/Ketsji/KX_SG_BoneParentNodeRelationship.cpp
 *  \ingroup ketsji
 */


#include <iostream>
 
#include "KX_SG_BoneParentNodeRelationship.h"

#include "MT_Matrix4x4.h"
#include "BL_ArmatureObject.h"


/**
 * Implementation of classes defined in KX_SG_BoneParentNodeRelationship.h
 */

/** 
 * first of all KX_SG_BoneParentRelation
 */

	KX_BoneParentRelation *
KX_BoneParentRelation::
New(Bone* bone
) {
	return new KX_BoneParentRelation(bone);
}

	bool
KX_BoneParentRelation::
UpdateChildCoordinates(
	SG_Spatial * child,
	const SG_Spatial * parent,
	bool& parentUpdated
) {
	MT_assert(child != NULL);
	
	// This way of accessing child coordinates is a bit cumbersome
	// be nice to have non constant reference access to these values.

	const MT_Vector3 & child_scale = child->GetLocalScale();
	const MT_Point3 & child_pos = child->GetLocalPosition();
	const MT_Matrix3x3 & child_rotation = child->GetLocalOrientation();
	// we don't know if the armature has been updated or not, assume yes
	parentUpdated = true;

	// the childs world locations which we will update.
	
	MT_Vector3 child_w_scale;
	MT_Point3 child_w_pos;
	MT_Matrix3x3 child_w_rotation;
	
	bool valid_parent_transform = false;
	
	if (parent)
	{
		BL_ArmatureObject *armature = (BL_ArmatureObject*)(parent->GetSGClientObject());
		if (armature)
		{
			MT_Matrix4x4 parent_matrix;
			if (armature->GetBoneMatrix(m_bone, parent_matrix))
			{
				// Get the child's transform, and the bone matrix.
				MT_Matrix4x4 child_transform ( 
					MT_Transform(child_pos + MT_Vector3(0.0, armature->GetBoneLength(m_bone), 0.0), 
						child_rotation.scaled(
							child_scale[0], 
							child_scale[1], 
							child_scale[2])));
				
				// The child's world transform is parent * child
				parent_matrix = parent->GetWorldTransform() * parent_matrix;
				child_transform = parent_matrix * child_transform;
				
				// Recompute the child transform components from the transform.
				child_w_scale.setValue( 
					MT_Vector3(child_transform[0][0], child_transform[0][1], child_transform[0][2]).length(),
					MT_Vector3(child_transform[1][0], child_transform[1][1], child_transform[1][2]).length(),
					MT_Vector3(child_transform[2][0], child_transform[2][1], child_transform[2][2]).length());
				child_w_rotation.setValue(child_transform[0][0], child_transform[0][1], child_transform[0][2], 
					child_transform[1][0], child_transform[1][1], child_transform[1][2], 
					child_transform[2][0], child_transform[2][1], child_transform[2][2]);
				child_w_rotation.scale(1.0/child_w_scale[0], 1.0/child_w_scale[1], 1.0/child_w_scale[2]);
					
				child_w_pos = MT_Point3(child_transform[0][3], child_transform[1][3], child_transform[2][3]);
					
				valid_parent_transform = true;
			}
		}
	} 
	
	if (valid_parent_transform)
	{
		child->SetWorldScale(child_w_scale);
		child->SetWorldPosition(child_w_pos);
		child->SetWorldOrientation(child_w_rotation);
	}
	else {
		child->SetWorldFromLocalTransform();
	}
	child->ClearModified();
	// this node must always be updated, so reschedule it for next time
	child->ActivateRecheduleUpdateCallback();
	return valid_parent_transform;
}

	SG_ParentRelation *
KX_BoneParentRelation::
NewCopy(
) {
	KX_BoneParentRelation* bone_parent = new KX_BoneParentRelation(m_bone);
	return bone_parent;
}

KX_BoneParentRelation::
~KX_BoneParentRelation(
) {
	//nothing to do
}


KX_BoneParentRelation::
KX_BoneParentRelation(Bone* bone
)
: m_bone(bone)
{
	// nothing to do
}

/*
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
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup depsgraph
 */

#include "intern/builder/deg_builder_rna.h"

#include <cstring>

#include "BLI_utildefines.h"
#include "BLI_listbase.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_sequence_types.h"
}

#include "BKE_constraint.h"

#include "RNA_access.h"

#include "intern/depsgraph.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace DEG {

namespace {

bool pointer_to_component_node_criteria(
        const PointerRNA *ptr,
        const PropertyRNA *prop,
        RNAPointerSource /*source*/,
        ID **id,
        NodeType *type,
        const char **component_name,
        OperationCode *operation_code,
        const char **operation_name,
        int *operation_name_tag)
{
	if (ptr->type == NULL) {
		return false;
	}
	/* Set default values for returns. */
	*id = (ID *)ptr->id.data;
	*component_name = "";
	*operation_code = OperationCode::OPERATION;
	*operation_name = "";
	*operation_name_tag = -1;
	/* Handling of commonly known scenarios. */
	if (ptr->type == &RNA_PoseBone) {
		bPoseChannel *pchan = (bPoseChannel *)ptr->data;
		if (prop != NULL && RNA_property_is_idprop(prop)) {
			*type = NodeType::PARAMETERS;
			*operation_code = OperationCode::ID_PROPERTY;
			*operation_name = RNA_property_identifier((PropertyRNA *)prop);
			*operation_name_tag = -1;
		}
		else {
			/* Bone - generally, we just want the bone component. */
			*type = NodeType::BONE;
			*component_name = pchan->name;
			/* But B-Bone properties should connect to the actual operation. */
			if (!ELEM(NULL, pchan->bone, prop) && pchan->bone->segments > 1 &&
			    STRPREFIX(RNA_property_identifier(prop), "bbone_"))
			{
				*operation_code = OperationCode::BONE_SEGMENTS;
			}
		}
		return true;
	}
	else if (ptr->type == &RNA_Bone) {
		Bone *bone = (Bone *)ptr->data;
		/* armature-level bone, but it ends up going to bone component anyway */
		// NOTE: the ID in this case will end up being bArmature.
		*type = NodeType::BONE;
		*component_name = bone->name;
		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
		Object *object = (Object *)ptr->id.data;
		bConstraint *con = (bConstraint *)ptr->data;
		/* Check whether is object or bone constraint. */
		/* NOTE: Currently none of the area can address transform of an object
		 * at a given constraint, but for rigging one might use constraint
		 * influence to be used to drive some corrective shape keys or so. */
		if (BLI_findindex(&object->constraints, con) != -1) {
			*type = NodeType::TRANSFORM;
			*operation_code = OperationCode::TRANSFORM_LOCAL;
			return true;
		}
		else if (object->pose != NULL) {
			LISTBASE_FOREACH (bPoseChannel *, pchan, &object->pose->chanbase) {
				if (BLI_findindex(&pchan->constraints, con) != -1) {
					*type = NodeType::BONE;
					*operation_code = OperationCode::BONE_LOCAL;
					*component_name = pchan->name;
					return true;
				}
			}
		}
	}
	else if (ELEM(ptr->type, &RNA_ConstraintTarget, &RNA_ConstraintTargetBone)) {
		Object *object = (Object *)ptr->id.data;
		bConstraintTarget *tgt = (bConstraintTarget *)ptr->data;
		/* Check whether is object or bone constraint. */
		bPoseChannel *pchan = NULL;
		bConstraint *con = BKE_constraint_find_from_target(object, tgt, &pchan);
		if (con != NULL) {
			if (pchan != NULL) {
				*type = NodeType::BONE;
				*operation_code = OperationCode::BONE_LOCAL;
				*component_name = pchan->name;
			}
			else {
				*type = NodeType::TRANSFORM;
				*operation_code = OperationCode::TRANSFORM_LOCAL;
			}
			return true;
		}
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
		*type = NodeType::GEOMETRY;
		return true;
	}
	else if (ptr->type == &RNA_Object) {
		/* Transforms props? */
		if (prop != NULL) {
			const char *prop_identifier = RNA_property_identifier((PropertyRNA *)prop);
			/* TODO(sergey): How to optimize this? */
			if (strstr(prop_identifier, "location") ||
			    strstr(prop_identifier, "rotation") ||
			    strstr(prop_identifier, "scale") ||
			    strstr(prop_identifier, "matrix_"))
			{
				*type = NodeType::TRANSFORM;
				return true;
			}
			else if (strstr(prop_identifier, "data")) {
				/* We access object.data, most likely a geometry.
				 * Might be a bone tho. */
				*type = NodeType::GEOMETRY;
				return true;
			}
		}
	}
	else if (ptr->type == &RNA_ShapeKey) {
		KeyBlock *key_block = (KeyBlock *)ptr->data;
		*id = (ID *)ptr->id.data;
		*type = NodeType::PARAMETERS;
		*operation_code = OperationCode::PARAMETERS_EVAL;
		*operation_name = key_block->name;
		return true;
	}
	else if (ptr->type == &RNA_Key) {
		*id = (ID *)ptr->id.data;
		*type = NodeType::GEOMETRY;
		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
		Sequence *seq = (Sequence *)ptr->data;
		/* Sequencer strip */
		*type = NodeType::SEQUENCER;
		*component_name = seq->name;
		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_NodeSocket)) {
		*type = NodeType::SHADING;
		return true;
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_ShaderNode)) {
		*type = NodeType::SHADING;
		return true;
	}
	else if (ELEM(ptr->type, &RNA_Curve, &RNA_TextCurve)) {
		*id = (ID *)ptr->id.data;
		*type = NodeType::GEOMETRY;
		return true;
	}
	if (prop != NULL) {
		/* All unknown data effectively falls under "parameter evaluation". */
		if (RNA_property_is_idprop(prop)) {
			*type = NodeType::PARAMETERS;
			*operation_code = OperationCode::ID_PROPERTY;
			*operation_name = RNA_property_identifier((PropertyRNA *)prop);
			*operation_name_tag = -1;
		}
		else {
			*type = NodeType::PARAMETERS;
			*operation_code = OperationCode::PARAMETERS_EVAL;
			*operation_name = "";
			*operation_name_tag = -1;
		}
		return true;
	}
	return false;
}

}  // namespace

RNANodeQuery::RNANodeQuery(Depsgraph *depsgraph)
        : depsgraph_(depsgraph)
{
}

Node *RNANodeQuery::find_node(const PointerRNA *ptr,
                              const PropertyRNA *prop,
                              RNAPointerSource source)
{
	ID *id;
	NodeType node_type;
	const char *component_name, *operation_name;
	OperationCode operation_code;
	int operation_name_tag;
	if (pointer_to_component_node_criteria(
	                 ptr, prop, source,
	                 &id, &node_type, &component_name,
	                 &operation_code, &operation_name, &operation_name_tag))
	{
		IDNode *id_node = depsgraph_->find_id_node(id);
		if (id_node == NULL) {
			return NULL;
		}
		ComponentNode *comp_node =
		        id_node->find_component(node_type, component_name);
		if (comp_node == NULL) {
			return NULL;
		}
		if (operation_code == OperationCode::OPERATION) {
			return comp_node;
		}
		return comp_node->find_operation(operation_code,
		                                 operation_name,
		                                 operation_name_tag);
	}
	return NULL;
}

}  // namespace DEG

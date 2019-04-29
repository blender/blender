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

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_rna.h"

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
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
#include "intern/builder/deg_builder.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace DEG {

/* ********************************* ID Data ******************************** */

class RNANodeQueryIDData {
 public:
  explicit RNANodeQueryIDData(const ID *id) : id_(id), contraint_to_pchan_map_(NULL)
  {
  }

  ~RNANodeQueryIDData()
  {
    if (contraint_to_pchan_map_ != NULL) {
      BLI_ghash_free(contraint_to_pchan_map_, NULL, NULL);
    }
  }

  const bPoseChannel *get_pchan_for_constraint(const bConstraint *constraint)
  {
    ensure_constraint_to_pchan_map();
    return static_cast<bPoseChannel *>(BLI_ghash_lookup(contraint_to_pchan_map_, constraint));
  }

  void ensure_constraint_to_pchan_map()
  {
    if (contraint_to_pchan_map_ != NULL) {
      return;
    }
    BLI_assert(GS(id_->name) == ID_OB);
    const Object *object = reinterpret_cast<const Object *>(id_);
    contraint_to_pchan_map_ = BLI_ghash_ptr_new("id data pchan constraint map");
    if (object->pose != NULL) {
      LISTBASE_FOREACH (const bPoseChannel *, pchan, &object->pose->chanbase) {
        LISTBASE_FOREACH (const bConstraint *, constraint, &pchan->constraints) {
          BLI_ghash_insert(contraint_to_pchan_map_,
                           const_cast<bConstraint *>(constraint),
                           const_cast<bPoseChannel *>(pchan));
        }
      }
    }
  }

 protected:
  /* ID this data corresponds to. */
  const ID *id_;

  /* indexed by bConstraint*, returns pose channel which contains that
   * constraint. */
  GHash *contraint_to_pchan_map_;
};

/* ***************************** Node Identifier **************************** */

RNANodeIdentifier::RNANodeIdentifier()
    : id(NULL),
      type(NodeType::UNDEFINED),
      component_name(""),
      operation_code(OperationCode::OPERATION),
      operation_name(),
      operation_name_tag(-1)
{
}

bool RNANodeIdentifier::is_valid() const
{
  return id != NULL && type != NodeType::UNDEFINED;
}

/* ********************************** Query ********************************* */

namespace {

void ghash_id_data_free_func(void *value)
{
  RNANodeQueryIDData *id_data = static_cast<RNANodeQueryIDData *>(value);
  OBJECT_GUARDED_DELETE(id_data, RNANodeQueryIDData);
}

}  // namespace

RNANodeQuery::RNANodeQuery(Depsgraph *depsgraph, DepsgraphBuilder *builder)
    : depsgraph_(depsgraph),
      builder_(builder),
      id_data_map_(BLI_ghash_ptr_new("rna node query id data hash"))
{
}

RNANodeQuery::~RNANodeQuery()
{
  BLI_ghash_free(id_data_map_, NULL, ghash_id_data_free_func);
}

Node *RNANodeQuery::find_node(const PointerRNA *ptr,
                              const PropertyRNA *prop,
                              RNAPointerSource source)
{
  const RNANodeIdentifier node_identifier = construct_node_identifier(ptr, prop, source);
  if (!node_identifier.is_valid()) {
    return NULL;
  }
  IDNode *id_node = depsgraph_->find_id_node(node_identifier.id);
  if (id_node == NULL) {
    return NULL;
  }
  ComponentNode *comp_node = id_node->find_component(node_identifier.type,
                                                     node_identifier.component_name);
  if (comp_node == NULL) {
    return NULL;
  }
  if (node_identifier.operation_code == OperationCode::OPERATION) {
    return comp_node;
  }
  return comp_node->find_operation(node_identifier.operation_code,
                                   node_identifier.operation_name,
                                   node_identifier.operation_name_tag);
}

RNANodeIdentifier RNANodeQuery::construct_node_identifier(const PointerRNA *ptr,
                                                          const PropertyRNA *prop,
                                                          RNAPointerSource source)
{
  RNANodeIdentifier node_identifier;
  if (ptr->type == NULL) {
    return node_identifier;
  }
  /* Set default values for returns. */
  node_identifier.id = static_cast<ID *>(ptr->id.data);
  node_identifier.component_name = "";
  node_identifier.operation_code = OperationCode::OPERATION;
  node_identifier.operation_name = "";
  node_identifier.operation_name_tag = -1;
  /* Handling of commonly known scenarios. */
  if (ptr->type == &RNA_PoseBone) {
    const bPoseChannel *pchan = static_cast<const bPoseChannel *>(ptr->data);
    if (prop != NULL && RNA_property_is_idprop(prop)) {
      node_identifier.type = NodeType::PARAMETERS;
      node_identifier.operation_code = OperationCode::ID_PROPERTY;
      node_identifier.operation_name = RNA_property_identifier(
          reinterpret_cast<const PropertyRNA *>(prop));
      node_identifier.operation_name_tag = -1;
    }
    else {
      /* Bone - generally, we just want the bone component. */
      node_identifier.type = NodeType::BONE;
      node_identifier.component_name = pchan->name;
      /* But B-Bone properties should connect to the actual operation. */
      if (!ELEM(NULL, pchan->bone, prop) && pchan->bone->segments > 1 &&
          STRPREFIX(RNA_property_identifier(prop), "bbone_")) {
        node_identifier.operation_code = OperationCode::BONE_SEGMENTS;
      }
    }
    return node_identifier;
  }
  else if (ptr->type == &RNA_Bone) {
    /* Armature-level bone mapped to Armature Eval, and thus Pose Init.
     * Drivers have special code elsewhere that links them to the pose
     * bone components, instead of using this generic code. */
    node_identifier.type = NodeType::PARAMETERS;
    node_identifier.operation_code = OperationCode::ARMATURE_EVAL;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
    const Object *object = static_cast<const Object *>(ptr->id.data);
    const bConstraint *constraint = static_cast<const bConstraint *>(ptr->data);
    RNANodeQueryIDData *id_data = ensure_id_data(&object->id);
    /* Check whether is object or bone constraint. */
    /* NOTE: Currently none of the area can address transform of an object
     * at a given constraint, but for rigging one might use constraint
     * influence to be used to drive some corrective shape keys or so. */
    const bPoseChannel *pchan = id_data->get_pchan_for_constraint(constraint);
    if (pchan == NULL) {
      node_identifier.type = NodeType::TRANSFORM;
      node_identifier.operation_code = OperationCode::TRANSFORM_LOCAL;
    }
    else {
      node_identifier.type = NodeType::BONE;
      node_identifier.operation_code = OperationCode::BONE_LOCAL;
      node_identifier.component_name = pchan->name;
    }
    return node_identifier;
  }
  else if (ELEM(ptr->type, &RNA_ConstraintTarget, &RNA_ConstraintTargetBone)) {
    Object *object = (Object *)ptr->id.data;
    bConstraintTarget *tgt = (bConstraintTarget *)ptr->data;
    /* Check whether is object or bone constraint. */
    bPoseChannel *pchan = NULL;
    bConstraint *con = BKE_constraint_find_from_target(object, tgt, &pchan);
    if (con != NULL) {
      if (pchan != NULL) {
        node_identifier.type = NodeType::BONE;
        node_identifier.operation_code = OperationCode::BONE_LOCAL;
        node_identifier.component_name = pchan->name;
      }
      else {
        node_identifier.type = NodeType::TRANSFORM;
        node_identifier.operation_code = OperationCode::TRANSFORM_LOCAL;
      }
      return node_identifier;
    }
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Modifier)) {
    /* When modifier is used as FROM operation this is likely referencing to
     * the property (for example, modifier's influence).
     * But when it's used as TO operation, this is geometry component. */
    switch (source) {
      case RNAPointerSource::ENTRY:
        node_identifier.type = NodeType::GEOMETRY;
        break;
      case RNAPointerSource::EXIT:
        node_identifier.type = NodeType::PARAMETERS;
        node_identifier.operation_code = OperationCode::PARAMETERS_EVAL;
        break;
    }
    return node_identifier;
  }
  else if (ptr->type == &RNA_Object) {
    /* Transforms props? */
    if (prop != NULL) {
      const char *prop_identifier = RNA_property_identifier((PropertyRNA *)prop);
      /* TODO(sergey): How to optimize this? */
      if (strstr(prop_identifier, "location") || strstr(prop_identifier, "rotation") ||
          strstr(prop_identifier, "scale") || strstr(prop_identifier, "matrix_")) {
        node_identifier.type = NodeType::TRANSFORM;
        return node_identifier;
      }
      else if (strstr(prop_identifier, "data")) {
        /* We access object.data, most likely a geometry.
         * Might be a bone tho. */
        node_identifier.type = NodeType::GEOMETRY;
        return node_identifier;
      }
      else if (STREQ(prop_identifier, "hide_viewport") || STREQ(prop_identifier, "hide_render")) {
        node_identifier.type = NodeType::OBJECT_FROM_LAYER;
        return node_identifier;
      }
    }
  }
  else if (ptr->type == &RNA_ShapeKey) {
    KeyBlock *key_block = static_cast<KeyBlock *>(ptr->data);
    node_identifier.id = static_cast<ID *>(ptr->id.data);
    node_identifier.type = NodeType::PARAMETERS;
    node_identifier.operation_code = OperationCode::PARAMETERS_EVAL;
    node_identifier.operation_name = key_block->name;
    return node_identifier;
  }
  else if (ptr->type == &RNA_Key) {
    node_identifier.id = static_cast<ID *>(ptr->id.data);
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
    const Sequence *seq = static_cast<Sequence *>(ptr->data);
    /* Sequencer strip */
    node_identifier.type = NodeType::SEQUENCER;
    node_identifier.component_name = seq->name;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_NodeSocket)) {
    node_identifier.type = NodeType::SHADING;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_ShaderNode)) {
    node_identifier.type = NodeType::SHADING;
    return node_identifier;
  }
  else if (ELEM(ptr->type, &RNA_Curve, &RNA_TextCurve)) {
    node_identifier.id = (ID *)ptr->id.data;
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  else if (ELEM(ptr->type, &RNA_BezierSplinePoint, &RNA_SplinePoint)) {
    node_identifier.id = (ID *)ptr->id.data;
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_ImageUser)) {
    if (GS(node_identifier.id->name) == ID_NT) {
      node_identifier.type = NodeType::ANIMATION;
      node_identifier.operation_code = OperationCode::IMAGE_ANIMATION;
      return node_identifier;
    }
  }
  else if (ELEM(ptr->type, &RNA_MeshVertex, &RNA_MeshEdge, &RNA_MeshLoop, &RNA_MeshPolygon)) {
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  if (prop != NULL) {
    /* All unknown data effectively falls under "parameter evaluation". */
    if (RNA_property_is_idprop(prop)) {
      node_identifier.type = NodeType::PARAMETERS;
      node_identifier.operation_code = OperationCode::ID_PROPERTY;
      node_identifier.operation_name = RNA_property_identifier((PropertyRNA *)prop);
      node_identifier.operation_name_tag = -1;
    }
    else {
      node_identifier.type = NodeType::PARAMETERS;
      node_identifier.operation_code = OperationCode::PARAMETERS_EVAL;
      node_identifier.operation_name = "";
      node_identifier.operation_name_tag = -1;
    }
    return node_identifier;
  }
  return node_identifier;
}

RNANodeQueryIDData *RNANodeQuery::ensure_id_data(const ID *id)
{
  RNANodeQueryIDData **id_data_ptr;
  if (!BLI_ghash_ensure_p(
          id_data_map_, const_cast<ID *>(id), reinterpret_cast<void ***>(&id_data_ptr))) {
    *id_data_ptr = OBJECT_GUARDED_NEW(RNANodeQueryIDData, id);
  }
  return *id_data_ptr;
}

}  // namespace DEG

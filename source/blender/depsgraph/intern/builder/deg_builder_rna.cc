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

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_sequence_types.h"

#include "BKE_constraint.h"

#include "RNA_access.h"

#include "intern/builder/deg_builder.h"
#include "intern/depsgraph.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

namespace DEG {

/* ********************************* ID Data ******************************** */

class RNANodeQueryIDData {
 public:
  explicit RNANodeQueryIDData(const ID *id) : id_(id), constraint_to_pchan_map_(nullptr)
  {
  }

  ~RNANodeQueryIDData()
  {
    delete constraint_to_pchan_map_;
  }

  const bPoseChannel *get_pchan_for_constraint(const bConstraint *constraint)
  {
    ensure_constraint_to_pchan_map();
    return constraint_to_pchan_map_->lookup_default(constraint, nullptr);
  }

  void ensure_constraint_to_pchan_map()
  {
    if (constraint_to_pchan_map_ != nullptr) {
      return;
    }
    BLI_assert(GS(id_->name) == ID_OB);
    const Object *object = reinterpret_cast<const Object *>(id_);
    constraint_to_pchan_map_ = new Map<const bConstraint *, const bPoseChannel *>();
    if (object->pose != nullptr) {
      LISTBASE_FOREACH (const bPoseChannel *, pchan, &object->pose->chanbase) {
        LISTBASE_FOREACH (const bConstraint *, constraint, &pchan->constraints) {
          constraint_to_pchan_map_->add_new(constraint, pchan);
        }
      }
    }
  }

 protected:
  /* ID this data corresponds to. */
  const ID *id_;

  /* indexed by bConstraint*, returns pose channel which contains that
   * constraint. */
  Map<const bConstraint *, const bPoseChannel *> *constraint_to_pchan_map_;
};

/* ***************************** Node Identifier **************************** */

RNANodeIdentifier::RNANodeIdentifier()
    : id(nullptr),
      type(NodeType::UNDEFINED),
      component_name(""),
      operation_code(OperationCode::OPERATION),
      operation_name(),
      operation_name_tag(-1)
{
}

bool RNANodeIdentifier::is_valid() const
{
  return id != nullptr && type != NodeType::UNDEFINED;
}

/* ********************************** Query ********************************* */

RNANodeQuery::RNANodeQuery(Depsgraph *depsgraph, DepsgraphBuilder *builder)
    : depsgraph_(depsgraph), builder_(builder)
{
}

RNANodeQuery::~RNANodeQuery()
{
}

Node *RNANodeQuery::find_node(const PointerRNA *ptr,
                              const PropertyRNA *prop,
                              RNAPointerSource source)
{
  const RNANodeIdentifier node_identifier = construct_node_identifier(ptr, prop, source);
  if (!node_identifier.is_valid()) {
    return nullptr;
  }
  IDNode *id_node = depsgraph_->find_id_node(node_identifier.id);
  if (id_node == nullptr) {
    return nullptr;
  }
  ComponentNode *comp_node = id_node->find_component(node_identifier.type,
                                                     node_identifier.component_name);
  if (comp_node == nullptr) {
    return nullptr;
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
  if (ptr->type == nullptr) {
    return node_identifier;
  }
  /* Set default values for returns. */
  node_identifier.id = ptr->owner_id;
  node_identifier.component_name = "";
  node_identifier.operation_code = OperationCode::OPERATION;
  node_identifier.operation_name = "";
  node_identifier.operation_name_tag = -1;
  /* Handling of commonly known scenarios. */
  if (prop != nullptr && RNA_property_is_idprop(prop)) {
    node_identifier.type = NodeType::PARAMETERS;
    node_identifier.operation_code = OperationCode::ID_PROPERTY;
    node_identifier.operation_name = RNA_property_identifier(
        reinterpret_cast<const PropertyRNA *>(prop));
    return node_identifier;
  }
  else if (ptr->type == &RNA_PoseBone) {
    const bPoseChannel *pchan = static_cast<const bPoseChannel *>(ptr->data);
    /* Bone - generally, we just want the bone component. */
    node_identifier.type = NodeType::BONE;
    node_identifier.component_name = pchan->name;
    /* However check property name for special handling. */
    if (prop != nullptr) {
      Object *object = reinterpret_cast<Object *>(node_identifier.id);
      const char *prop_name = RNA_property_identifier(prop);
      /* B-Bone properties should connect to the final operation. */
      if (STRPREFIX(prop_name, "bbone_")) {
        if (builder_->check_pchan_has_bbone_segments(object, pchan)) {
          node_identifier.operation_code = OperationCode::BONE_SEGMENTS;
        }
        else {
          node_identifier.operation_code = OperationCode::BONE_DONE;
        }
      }
      /* Final transform properties go to the Done node for the exit. */
      else if (STREQ(prop_name, "head") || STREQ(prop_name, "tail") ||
               STREQ(prop_name, "length") || STRPREFIX(prop_name, "matrix")) {
        if (source == RNAPointerSource::EXIT) {
          node_identifier.operation_code = OperationCode::BONE_DONE;
        }
      }
      /* And other properties can always go to the entry operation. */
      else {
        node_identifier.operation_code = OperationCode::BONE_LOCAL;
      }
    }
    return node_identifier;
  }
  else if (ptr->type == &RNA_Bone) {
    /* Armature-level bone mapped to Armature Eval, and thus Pose Init.
     * Drivers have special code elsewhere that links them to the pose
     * bone components, instead of using this generic code. */
    node_identifier.type = NodeType::ARMATURE;
    node_identifier.operation_code = OperationCode::ARMATURE_EVAL;
    /* If trying to look up via an Object, e.g. due to lookup via
     * obj.pose.bones[].bone in a driver attached to the Object,
     * redirect to its data. */
    if (GS(node_identifier.id->name) == ID_OB) {
      node_identifier.id = (ID *)((Object *)node_identifier.id)->data;
    }
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Constraint)) {
    const Object *object = reinterpret_cast<const Object *>(ptr->owner_id);
    const bConstraint *constraint = static_cast<const bConstraint *>(ptr->data);
    RNANodeQueryIDData *id_data = ensure_id_data(&object->id);
    /* Check whether is object or bone constraint. */
    /* NOTE: Currently none of the area can address transform of an object
     * at a given constraint, but for rigging one might use constraint
     * influence to be used to drive some corrective shape keys or so. */
    const bPoseChannel *pchan = id_data->get_pchan_for_constraint(constraint);
    if (pchan == nullptr) {
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
    Object *object = reinterpret_cast<Object *>(ptr->owner_id);
    bConstraintTarget *tgt = (bConstraintTarget *)ptr->data;
    /* Check whether is object or bone constraint. */
    bPoseChannel *pchan = nullptr;
    bConstraint *con = BKE_constraint_find_from_target(object, tgt, &pchan);
    if (con != nullptr) {
      if (pchan != nullptr) {
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
  else if (RNA_struct_is_a(ptr->type, &RNA_Mesh) || RNA_struct_is_a(ptr->type, &RNA_Modifier) ||
           RNA_struct_is_a(ptr->type, &RNA_GpencilModifier) ||
           RNA_struct_is_a(ptr->type, &RNA_Spline) || RNA_struct_is_a(ptr->type, &RNA_TextBox) ||
           RNA_struct_is_a(ptr->type, &RNA_GPencilLayer) ||
           RNA_struct_is_a(ptr->type, &RNA_LatticePoint) ||
           RNA_struct_is_a(ptr->type, &RNA_MeshUVLoop) ||
           RNA_struct_is_a(ptr->type, &RNA_MeshLoopColor) ||
           RNA_struct_is_a(ptr->type, &RNA_VertexGroupElement)) {
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
    if (prop != nullptr) {
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
      else if (STREQ(prop_identifier, "dimensions")) {
        node_identifier.type = NodeType::PARAMETERS;
        node_identifier.operation_code = OperationCode::DIMENSIONS;
        return node_identifier;
      }
    }
  }
  else if (ptr->type == &RNA_ShapeKey) {
    KeyBlock *key_block = static_cast<KeyBlock *>(ptr->data);
    node_identifier.id = ptr->owner_id;
    node_identifier.type = NodeType::PARAMETERS;
    node_identifier.operation_code = OperationCode::PARAMETERS_EVAL;
    node_identifier.operation_name = key_block->name;
    return node_identifier;
  }
  else if (ptr->type == &RNA_Key) {
    node_identifier.id = ptr->owner_id;
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
    /* Sequencer strip */
    node_identifier.type = NodeType::SEQUENCER;
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
    node_identifier.id = ptr->owner_id;
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  else if (ELEM(ptr->type, &RNA_BezierSplinePoint, &RNA_SplinePoint)) {
    node_identifier.id = ptr->owner_id;
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  else if (RNA_struct_is_a(ptr->type, &RNA_ImageUser)) {
    if (GS(node_identifier.id->name) == ID_NT) {
      node_identifier.type = NodeType::IMAGE_ANIMATION;
      node_identifier.operation_code = OperationCode::IMAGE_ANIMATION;
      return node_identifier;
    }
  }
  else if (ELEM(ptr->type, &RNA_MeshVertex, &RNA_MeshEdge, &RNA_MeshLoop, &RNA_MeshPolygon)) {
    node_identifier.type = NodeType::GEOMETRY;
    return node_identifier;
  }
  if (prop != nullptr) {
    /* All unknown data effectively falls under "parameter evaluation". */
    node_identifier.type = NodeType::PARAMETERS;
    node_identifier.operation_code = OperationCode::PARAMETERS_EVAL;
    node_identifier.operation_name = "";
    node_identifier.operation_name_tag = -1;
    return node_identifier;
  }
  return node_identifier;
}

RNANodeQueryIDData *RNANodeQuery::ensure_id_data(const ID *id)
{
  unique_ptr<RNANodeQueryIDData> &id_data = id_data_map_.lookup_or_add(
      id, [&]() { return blender::make_unique<RNANodeQueryIDData>(id); });
  return id_data.get();
}

}  // namespace DEG

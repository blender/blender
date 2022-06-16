/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node_operation.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "intern/depsgraph.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"

namespace blender::deg {

const char *operationCodeAsString(OperationCode opcode)
{
  switch (opcode) {
    /* Generic Operations. */
    case OperationCode::OPERATION:
      return "OPERATION";
    case OperationCode::ID_PROPERTY:
      return "ID_PROPERTY";
    case OperationCode::PARAMETERS_ENTRY:
      return "PARAMETERS_ENTRY";
    case OperationCode::PARAMETERS_EVAL:
      return "PARAMETERS_EVAL";
    case OperationCode::PARAMETERS_EXIT:
      return "PARAMETERS_EXIT";
    /* Animation, Drivers, etc. */
    case OperationCode::ANIMATION_ENTRY:
      return "ANIMATION_ENTRY";
    case OperationCode::ANIMATION_EVAL:
      return "ANIMATION_EVAL";
    case OperationCode::ANIMATION_EXIT:
      return "ANIMATION_EXIT";
    case OperationCode::DRIVER:
      return "DRIVER";
    /* Scene related. */
    case OperationCode::SCENE_EVAL:
      return "SCENE_EVAL";
    case OperationCode::AUDIO_ENTRY:
      return "AUDIO_ENTRY";
    case OperationCode::AUDIO_VOLUME:
      return "AUDIO_VOLUME";
    /* Object related. */
    case OperationCode::OBJECT_FROM_LAYER_ENTRY:
      return "OBJECT_FROM_LAYER_ENTRY";
    case OperationCode::OBJECT_BASE_FLAGS:
      return "OBJECT_BASE_FLAGS";
    case OperationCode::OBJECT_FROM_LAYER_EXIT:
      return "OBJECT_FROM_LAYER_EXIT";
    case OperationCode::DIMENSIONS:
      return "DIMENSIONS";
    /* Transform. */
    case OperationCode::TRANSFORM_INIT:
      return "TRANSFORM_INIT";
    case OperationCode::TRANSFORM_LOCAL:
      return "TRANSFORM_LOCAL";
    case OperationCode::TRANSFORM_PARENT:
      return "TRANSFORM_PARENT";
    case OperationCode::TRANSFORM_CONSTRAINTS:
      return "TRANSFORM_CONSTRAINTS";
    case OperationCode::TRANSFORM_FINAL:
      return "TRANSFORM_FINAL";
    case OperationCode::TRANSFORM_EVAL:
      return "TRANSFORM_EVAL";
    case OperationCode::TRANSFORM_SIMULATION_INIT:
      return "TRANSFORM_SIMULATION_INIT";
    /* Rigid body. */
    case OperationCode::RIGIDBODY_REBUILD:
      return "RIGIDBODY_REBUILD";
    case OperationCode::RIGIDBODY_SIM:
      return "RIGIDBODY_SIM";
    case OperationCode::RIGIDBODY_TRANSFORM_COPY:
      return "RIGIDBODY_TRANSFORM_COPY";
    /* Geometry. */
    case OperationCode::GEOMETRY_EVAL_INIT:
      return "GEOMETRY_EVAL_INIT";
    case OperationCode::GEOMETRY_EVAL:
      return "GEOMETRY_EVAL";
    case OperationCode::GEOMETRY_EVAL_DONE:
      return "GEOMETRY_EVAL_DONE";
    case OperationCode::GEOMETRY_SHAPEKEY:
      return "GEOMETRY_SHAPEKEY";
    case OperationCode::GEOMETRY_WRITE_CACHE:
      return "GEOMETRY_WRITE_CACHE";
    /* Object data. */
    case OperationCode::LIGHT_PROBE_EVAL:
      return "LIGHT_PROBE_EVAL";
    case OperationCode::SPEAKER_EVAL:
      return "SPEAKER_EVAL";
    case OperationCode::SOUND_EVAL:
      return "SOUND_EVAL";
    case OperationCode::ARMATURE_EVAL:
      return "ARMATURE_EVAL";
    /* Pose. */
    case OperationCode::POSE_INIT:
      return "POSE_INIT";
    case OperationCode::POSE_INIT_IK:
      return "POSE_INIT_IK";
    case OperationCode::POSE_CLEANUP:
      return "POSE_CLEANUP";
    case OperationCode::POSE_DONE:
      return "POSE_DONE";
    case OperationCode::POSE_IK_SOLVER:
      return "POSE_IK_SOLVER";
    case OperationCode::POSE_SPLINE_IK_SOLVER:
      return "POSE_SPLINE_IK_SOLVER";
    /* Bone. */
    case OperationCode::BONE_LOCAL:
      return "BONE_LOCAL";
    case OperationCode::BONE_POSE_PARENT:
      return "BONE_POSE_PARENT";
    case OperationCode::BONE_CONSTRAINTS:
      return "BONE_CONSTRAINTS";
    case OperationCode::BONE_READY:
      return "BONE_READY";
    case OperationCode::BONE_DONE:
      return "BONE_DONE";
    case OperationCode::BONE_SEGMENTS:
      return "BONE_SEGMENTS";
    /* Particle System. */
    case OperationCode::PARTICLE_SYSTEM_INIT:
      return "PARTICLE_SYSTEM_INIT";
    case OperationCode::PARTICLE_SYSTEM_EVAL:
      return "PARTICLE_SYSTEM_EVAL";
    case OperationCode::PARTICLE_SYSTEM_DONE:
      return "PARTICLE_SYSTEM_DONE";
    /* Particles Settings. */
    case OperationCode::PARTICLE_SETTINGS_INIT:
      return "PARTICLE_SETTINGS_INIT";
    case OperationCode::PARTICLE_SETTINGS_EVAL:
      return "PARTICLE_SETTINGS_EVAL";
    case OperationCode::PARTICLE_SETTINGS_RESET:
      return "PARTICLE_SETTINGS_RESET";
    /* Point Cache. */
    case OperationCode::POINT_CACHE_RESET:
      return "POINT_CACHE_RESET";
    /* File cache. */
    case OperationCode::FILE_CACHE_UPDATE:
      return "FILE_CACHE_UPDATE";
    /* Batch cache. */
    case OperationCode::GEOMETRY_SELECT_UPDATE:
      return "GEOMETRY_SELECT_UPDATE";
    /* Masks. */
    case OperationCode::MASK_ANIMATION:
      return "MASK_ANIMATION";
    case OperationCode::MASK_EVAL:
      return "MASK_EVAL";
    /* Collections. */
    case OperationCode::VIEW_LAYER_EVAL:
      return "VIEW_LAYER_EVAL";
    /* Copy on write. */
    case OperationCode::COPY_ON_WRITE:
      return "COPY_ON_WRITE";
    /* Shading. */
    case OperationCode::SHADING:
      return "SHADING";
    case OperationCode::MATERIAL_UPDATE:
      return "MATERIAL_UPDATE";
    case OperationCode::LIGHT_UPDATE:
      return "LIGHT_UPDATE";
    case OperationCode::WORLD_UPDATE:
      return "WORLD_UPDATE";
    /* Node Tree. */
    case OperationCode::NTREE_OUTPUT:
      return "NTREE_OUTPUT";
    /* Movie clip. */
    case OperationCode::MOVIECLIP_EVAL:
      return "MOVIECLIP_EVAL";
    case OperationCode::MOVIECLIP_SELECT_UPDATE:
      return "MOVIECLIP_SELECT_UPDATE";
    /* Image. */
    case OperationCode::IMAGE_ANIMATION:
      return "IMAGE_ANIMATION";
    /* Synchronization. */
    case OperationCode::SYNCHRONIZE_TO_ORIGINAL:
      return "SYNCHRONIZE_TO_ORIGINAL";
    /* Generic datablock. */
    case OperationCode::GENERIC_DATABLOCK_UPDATE:
      return "GENERIC_DATABLOCK_UPDATE";
    /* Sequencer. */
    case OperationCode::SEQUENCES_EVAL:
      return "SEQUENCES_EVAL";
    /* instancing/duplication. */
    case OperationCode::DUPLI:
      return "DUPLI";
    case OperationCode::SIMULATION_EVAL:
      return "SIMULATION_EVAL";
  }
  BLI_assert_msg(0, "Unhandled operation code, should never happen.");
  return "UNKNOWN";
}

OperationNode::OperationNode() : name_tag(-1), flag(0)
{
}

string OperationNode::identifier() const
{
  return string(operationCodeAsString(opcode)) + "(" + name + ")";
}

string OperationNode::full_identifier() const
{
  string owner_str = owner->owner->name;
  if (owner->type == NodeType::BONE || !owner->name.empty()) {
    owner_str += "/" + owner->name;
  }
  return owner_str + "/" + identifier();
}

void OperationNode::tag_update(Depsgraph *graph, eUpdateSource source)
{
  if ((flag & DEPSOP_FLAG_NEEDS_UPDATE) == 0) {
    graph->add_entry_tag(this);
  }
  /* Tag for update, but also note that this was the source of an update. */
  flag |= (DEPSOP_FLAG_NEEDS_UPDATE | DEPSOP_FLAG_DIRECTLY_MODIFIED);
  switch (source) {
    case DEG_UPDATE_SOURCE_TIME:
    case DEG_UPDATE_SOURCE_RELATIONS:
    case DEG_UPDATE_SOURCE_VISIBILITY:
      /* Currently nothing. */
      break;
    case DEG_UPDATE_SOURCE_USER_EDIT:
      flag |= DEPSOP_FLAG_USER_MODIFIED;
      break;
  }
}

void OperationNode::set_as_entry()
{
  BLI_assert(owner != nullptr);
  owner->set_entry_operation(this);
}

void OperationNode::set_as_exit()
{
  BLI_assert(owner != nullptr);
  owner->set_exit_operation(this);
}

DEG_DEPSNODE_DEFINE(OperationNode, NodeType::OPERATION, "Operation");
static DepsNodeFactoryImpl<OperationNode> DNTI_OPERATION;

void deg_register_operation_depsnodes()
{
  register_node_typeinfo(&DNTI_OPERATION);
}

}  // namespace blender::deg

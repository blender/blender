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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/node/deg_node.h"

#include <stdio.h>

#include "BLI_utildefines.h"

#include "intern/depsgraph.h"
#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node_component.h"
#include "intern/node/deg_node_factory.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"
#include "intern/node/deg_node_time.h"

namespace DEG {

const char *nodeClassAsString(NodeClass node_class)
{
  switch (node_class) {
    case NodeClass::GENERIC:
      return "GENERIC";
    case NodeClass::COMPONENT:
      return "COMPONENT";
    case NodeClass::OPERATION:
      return "OPERATION";
  }
  BLI_assert(!"Unhandled node class, should never happen.");
  return "UNKNOWN";
}

const char *nodeTypeAsString(NodeType type)
{
  switch (type) {
    case NodeType::UNDEFINED:
      return "UNDEFINED";
    case NodeType::OPERATION:
      return "OPERATION";
    /* **** Generic Types **** */
    case NodeType::TIMESOURCE:
      return "TIMESOURCE";
    case NodeType::ID_REF:
      return "ID_REF";
    /* **** Outer Types **** */
    case NodeType::PARAMETERS:
      return "PARAMETERS";
    case NodeType::PROXY:
      return "PROXY";
    case NodeType::ANIMATION:
      return "ANIMATION";
    case NodeType::TRANSFORM:
      return "TRANSFORM";
    case NodeType::GEOMETRY:
      return "GEOMETRY";
    case NodeType::SEQUENCER:
      return "SEQUENCER";
    case NodeType::LAYER_COLLECTIONS:
      return "LAYER_COLLECTIONS";
    case NodeType::COPY_ON_WRITE:
      return "COPY_ON_WRITE";
    case NodeType::OBJECT_FROM_LAYER:
      return "OBJECT_FROM_LAYER";
    /* **** Evaluation-Related Outer Types (with Subdata) **** */
    case NodeType::EVAL_POSE:
      return "EVAL_POSE";
    case NodeType::BONE:
      return "BONE";
    case NodeType::PARTICLE_SYSTEM:
      return "PARTICLE_SYSTEM";
    case NodeType::PARTICLE_SETTINGS:
      return "PARTICLE_SETTINGS";
    case NodeType::SHADING:
      return "SHADING";
    case NodeType::SHADING_PARAMETERS:
      return "SHADING_PARAMETERS";
    case NodeType::CACHE:
      return "CACHE";
    case NodeType::POINT_CACHE:
      return "POINT_CACHE";
    case NodeType::BATCH_CACHE:
      return "BATCH_CACHE";
    case NodeType::DUPLI:
      return "DUPLI";
    case NodeType::SYNCHRONIZATION:
      return "SYNCHRONIZATION";
    case NodeType::AUDIO:
      return "AUDIO";
    case NodeType::ARMATURE:
      return "ARMATURE";
    case NodeType::GENERIC_DATABLOCK:
      return "GENERIC_DATABLOCK";

    /* Total number of meaningful node types. */
    case NodeType::NUM_TYPES:
      return "SpecialCase";
  }
  BLI_assert(!"Unhandled node type, should never happen.");
  return "UNKNOWN";
}

NodeType nodeTypeFromSceneComponent(eDepsSceneComponentType component)
{
  switch (component) {
    case DEG_SCENE_COMP_PARAMETERS:
      return NodeType::PARAMETERS;
    case DEG_SCENE_COMP_ANIMATION:
      return NodeType::ANIMATION;
    case DEG_SCENE_COMP_SEQUENCER:
      return NodeType::SEQUENCER;
  }
  return NodeType::UNDEFINED;
}

NodeType nodeTypeFromObjectComponent(eDepsObjectComponentType component)
{
  switch (component) {
    case DEG_OB_COMP_PARAMETERS:
      return NodeType::PARAMETERS;
    case DEG_OB_COMP_PROXY:
      return NodeType::PROXY;
    case DEG_OB_COMP_ANIMATION:
      return NodeType::ANIMATION;
    case DEG_OB_COMP_TRANSFORM:
      return NodeType::TRANSFORM;
    case DEG_OB_COMP_GEOMETRY:
      return NodeType::GEOMETRY;
    case DEG_OB_COMP_EVAL_POSE:
      return NodeType::EVAL_POSE;
    case DEG_OB_COMP_BONE:
      return NodeType::BONE;
    case DEG_OB_COMP_SHADING:
      return NodeType::SHADING;
    case DEG_OB_COMP_CACHE:
      return NodeType::CACHE;
  }
  return NodeType::UNDEFINED;
}

/*******************************************************************************
 * Type information.
 */

Node::TypeInfo::TypeInfo(NodeType type, const char *type_name, int id_recalc_tag)
    : type(type), type_name(type_name), id_recalc_tag(id_recalc_tag)
{
}

/*******************************************************************************
 * Evaluation statistics.
 */

Node::Stats::Stats()
{
  reset();
}

void Node::Stats::reset()
{
  current_time = 0.0;
}

void Node::Stats::reset_current()
{
  current_time = 0.0;
}

/*******************************************************************************
 * Node itself.
 */

Node::Node()
{
  name = "";
}

Node::~Node()
{
  /* Free links. */
  /* NOTE: We only free incoming links. This is to avoid double-free of links
   * when we're trying to free same link from both it's sides. We don't have
   * dangling links so this is not a problem from memory leaks point of view. */
  for (Relation *rel : inlinks) {
    OBJECT_GUARDED_DELETE(rel, Relation);
  }
}

/* Generic identifier for Depsgraph Nodes. */
string Node::identifier() const
{
  return string(nodeTypeAsString(type)) + " : " + name;
}

NodeClass Node::get_class() const
{
  if (type == NodeType::OPERATION) {
    return NodeClass::OPERATION;
  }
  else if (type < NodeType::PARAMETERS) {
    return NodeClass::GENERIC;
  }
  else {
    return NodeClass::COMPONENT;
  }
}

/*******************************************************************************
 * Generic nodes definition.
 */

DEG_DEPSNODE_DEFINE(TimeSourceNode, NodeType::TIMESOURCE, "Time Source");
static DepsNodeFactoryImpl<TimeSourceNode> DNTI_TIMESOURCE;

DEG_DEPSNODE_DEFINE(IDNode, NodeType::ID_REF, "ID Node");
static DepsNodeFactoryImpl<IDNode> DNTI_ID_REF;

void deg_register_base_depsnodes()
{
  register_node_typeinfo(&DNTI_TIMESOURCE);
  register_node_typeinfo(&DNTI_ID_REF);
}

}  // namespace DEG

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

#pragma once

#include "intern/depsgraph_type.h"

#include "BLI_utildefines.h"

struct GHash;
struct ID;
struct Scene;

namespace DEG {

struct Depsgraph;
struct OperationNode;
struct Relation;

/* Metatype of Nodes - The general "level" in the graph structure
 * the node serves. */
enum class NodeClass {
  /* Types generally unassociated with user-visible entities,
   * but needed for graph functioning. */
  GENERIC = 0,
  /* [Outer Node] An "aspect" of evaluating/updating an ID-Block, requiring
   * certain types of evaluation behavior. */
  COMPONENT = 1,
  /* [Inner Node] A glorified function-pointer/callback for scheduling up
   * evaluation operations for components, subject to relationship
   * requirements. */
  OPERATION = 2,
};
const char *nodeClassAsString(NodeClass node_class);

/* Types of Nodes */
enum class NodeType {
  /* Fallback type for invalid return value */
  UNDEFINED = 0,
  /* Inner Node (Operation) */
  OPERATION,

  /* **** Generic Types **** */

  /* Time-Source */
  TIMESOURCE,
  /* ID-Block reference - used as landmarks/collection point for components,
   * but not usually part of main graph. */
  ID_REF,

  /* **** Outer Types **** */

  /* Parameters Component - Default when nothing else fits
   * (i.e. just SDNA property setting). */
  PARAMETERS,
  /* Generic "Proxy-Inherit" Component. */
  PROXY,
  /* Animation Component */
  ANIMATION,
  /* Transform Component (Parenting/Constraints) */
  TRANSFORM,
  /* Geometry Component (Mesh/Displist) */
  GEOMETRY,
  /* Sequencer Component (Scene Only) */
  SEQUENCER,
  /* Component which contains all operations needed for layer collections
   * evaluation. */
  LAYER_COLLECTIONS,
  /* Entry component of majority of ID nodes: prepares CoW pointers for
   * execution. */
  COPY_ON_WRITE,
  /* Used by all operations which are updating object when something is
   * changed in view layer. */
  OBJECT_FROM_LAYER,
  /* Audio-related evaluation. */
  AUDIO,
  ARMATURE,
  /* Un-interestying datablock, which is a part of dependency graph, but does
   * not have very distinctive update procedure.  */
  GENERIC_DATABLOCK,

  /* **** Evaluation-Related Outer Types (with Subdata) **** */

  /* Pose Component - Owner/Container of Bones Eval */
  EVAL_POSE,
  /* Bone Component - Child/Subcomponent of Pose */
  BONE,
  /* Particle Systems Component */
  PARTICLE_SYSTEM,
  PARTICLE_SETTINGS,
  /* Material Shading Component */
  SHADING,
  SHADING_PARAMETERS,
  /* Point cache Component */
  POINT_CACHE,
  /* Cache Component */
  /* TODO(sergey); Verify that we really need this. */
  CACHE,
  /* Batch Cache Component.
   * TODO (dfelinto/sergey): rename to make it more generic. */
  BATCH_CACHE,
  /* Duplication system. Used to force duplicated objects visible when
   * when duplicator is visible. */
  DUPLI,
  /* Synchronization back to original datablock. */
  SYNCHRONIZATION,

  /* Total number of meaningful node types. */
  NUM_TYPES,
};
const char *nodeTypeAsString(NodeType type);

/* All nodes in Depsgraph are descended from this. */
struct Node {
  /* Helper class for static typeinfo in subclasses. */
  struct TypeInfo {
    TypeInfo(NodeType type, const char *type_name, int id_recalc_tag = 0);
    NodeType type;
    const char *type_name;
    int id_recalc_tag;
  };
  struct Stats {
    Stats();
    /* Reset all the counters. Including all stats needed for average
     * evaluation time calculation. */
    void reset();
    /* Reset counters needed for the current graph evaluation, does not
     * touch averaging accumulators. */
    void reset_current();
    /* Time spend on this node during current graph evaluation. */
    double current_time;
  };
  /* Relationships between nodes
   * The reason why all depsgraph nodes are descended from this type (apart
   * from basic serialization benefits - from the typeinfo) is that we can
   * have relationships between these nodes. */
  typedef vector<Relation *> Relations;

  string name;        /* Identifier - mainly for debugging purposes. */
  NodeType type;      /* Structural type of node. */
  Relations inlinks;  /* Nodes which this one depends on. */
  Relations outlinks; /* Nodes which depend on this one. */
  Stats stats;        /* Evaluation statistics. */

  /* Generic tags for traversal algorithms and such.
   *
   * Actual meaning of values depends on a specific area. Every area is to
   * clean this before use. */
  int custom_flags;

  /* Methods. */
  Node();
  virtual ~Node();

  virtual string identifier() const;

  virtual void init(const ID * /*id*/, const char * /*subdata*/)
  {
  }

  virtual void tag_update(Depsgraph * /*graph*/, eUpdateSource /*source*/)
  {
  }

  virtual OperationNode *get_entry_operation()
  {
    return NULL;
  }
  virtual OperationNode *get_exit_operation()
  {
    return NULL;
  }

  virtual NodeClass get_class() const;
};

/* Macros for common static typeinfo. */
#define DEG_DEPSNODE_DECLARE static const Node::TypeInfo typeinfo
#define DEG_DEPSNODE_DEFINE(NodeType, type_, tname_) \
  const Node::TypeInfo NodeType::typeinfo = Node::TypeInfo(type_, tname_)

void deg_register_base_depsnodes();

}  // namespace DEG

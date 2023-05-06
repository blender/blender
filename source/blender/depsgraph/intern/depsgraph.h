/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation */

/** \file
 * \ingroup depsgraph
 *
 * Datatypes for internal use in the Depsgraph
 *
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#pragma once

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h" /* for ID_Type and INDEX_ID_MAX */

#include "BLI_threads.h" /* for SpinLock */

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_physics.h"

#include "intern/debug/deg_debug.h"
#include "intern/depsgraph_type.h"

struct ID;
struct Scene;
struct ViewLayer;

namespace blender::deg {

struct IDNode;
struct Node;
struct OperationNode;
struct Relation;
struct TimeSourceNode;

/* Dependency Graph object */
struct Depsgraph {
  typedef Vector<OperationNode *> OperationNodes;
  typedef Vector<IDNode *> IDDepsNodes;

  Depsgraph(Main *bmain, Scene *scene, ViewLayer *view_layer, eEvaluationMode mode);
  ~Depsgraph();

  TimeSourceNode *add_time_source();
  TimeSourceNode *find_time_source() const;
  void tag_time_source();

  IDNode *find_id_node(const ID *id) const;
  IDNode *add_id_node(ID *id, ID *id_cow_hint = nullptr);
  void clear_id_nodes();

  /** Add new relationship between two nodes. */
  Relation *add_new_relation(Node *from, Node *to, const char *description, int flags = 0);

  /* Check whether two nodes are connected by relation with given
   * description. Description might be nullptr to check ANY relation between
   * given nodes. */
  Relation *check_nodes_connected(const Node *from, const Node *to, const char *description);

  /* Tag a specific node as needing updates. */
  void add_entry_tag(OperationNode *node);

  /* Clear storage used by all nodes. */
  void clear_all_nodes();

  /* Copy-on-Write Functionality ........ */

  /* For given original ID get ID which is created by CoW system. */
  ID *get_cow_id(const ID *id_orig) const;

  /* Core Graph Functionality ........... */

  /* <ID : IDNode> mapping from ID blocks to nodes representing these
   * blocks, used for quick lookups. */
  Map<const ID *, IDNode *> id_hash;

  /* Ordered list of ID nodes, order matches ID allocation order.
   * Used for faster iteration, especially for areas which are critical to
   * keep exact order of iteration. */
  IDDepsNodes id_nodes;

  /* Top-level time source node. */
  TimeSourceNode *time_source;

  /* The graph contains data-blocks whose visibility depends on evaluation (driven or animated). */
  bool has_animated_visibility;

  /* Indicates whether relations needs to be updated. */
  bool need_update_relations;

  /* Indicates whether indirect effect of nodes on a directly visible ones needs to be updated. */
  bool need_update_nodes_visibility;

  /* Indicated whether IDs in this graph are to be tagged as if they first appear visible, with
   * an optional tag for their animation (time) update. */
  bool need_tag_id_on_graph_visibility_update;
  bool need_tag_id_on_graph_visibility_time_update;

  /* Indicates which ID types were updated. */
  char id_type_updated[INDEX_ID_MAX];

  /* Indicates type of IDs present in the depsgraph. */
  char id_type_exist[INDEX_ID_MAX];

  /* Quick-Access Temp Data ............. */

  /* Nodes which have been tagged as "directly modified". */
  Set<OperationNode *> entry_tags;

  /* Convenience Data ................... */

  /* XXX: should be collected after building (if actually needed?) */
  /* All operation nodes, sorted in order of single-thread traversal order. */
  OperationNodes operations;

  /* Spin lock for threading-critical operations.
   * Mainly used by graph evaluation. */
  SpinLock lock;

  /* Main, scene, layer, mode this dependency graph is built for. */
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;
  eEvaluationMode mode;

  /* Time at which dependency graph is being or was last evaluated.
   * frame is the value before, and ctime the value after time remapping. */
  float frame;
  float ctime;

  /* Evaluated version of datablocks we access a lot.
   * Stored here to save us form doing hash lookup. */
  Scene *scene_cow;

  /* Active dependency graph is a dependency graph which is used by the
   * currently active window. When dependency graph is active, it is allowed
   * for evaluation functions to write animation f-curve result, drivers
   * result and other selective things (object matrix?) to original object.
   *
   * This way we simplify operators, which don't need to worry about where
   * to read stuff from. */
  bool is_active;

  /* Optimize out evaluation of operations which affect hidden objects or disabled modifiers. */
  bool use_visibility_optimization;

  DepsgraphDebug debug;

  bool is_evaluating;

  /* Is set to truth for dependency graph which are used for post-processing (compositor and
   * sequencer).
   * Such dependency graph needs all view layers (so render pipeline can access names), but it
   * does not need any bases. */
  bool is_render_pipeline_depsgraph;

  /* Notify editors about changes to IDs in this depsgraph. */
  bool use_editors_update;

  /* Cached list of colliders/effectors for collections and the scene
   * created along with relations, for fast lookup during evaluation. */
  Map<const ID *, ListBase *> *physics_relations[DEG_PHYSICS_RELATIONS_NUM];

  MEM_CXX_CLASS_ALLOC_FUNCS("Depsgraph");
};

}  // namespace blender::deg

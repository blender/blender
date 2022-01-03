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

#include "intern/eval/deg_eval_copy_on_write.h"
#include "intern/node/deg_node.h"
#include "intern/node/deg_node_id.h"
#include "intern/node/deg_node_operation.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_object.h"

#include "DNA_object_types.h"

struct ID;
struct bPoseChannel;

namespace blender {
namespace deg {

struct BoneComponentNode;
struct Depsgraph;
struct IDNode;
struct OperationNode;

/* ID Component - Base type for all components */
struct ComponentNode : public Node {
  /* Key used to look up operations within a component */
  struct OperationIDKey {
    OperationCode opcode;
    const char *name;
    int name_tag;

    OperationIDKey();
    OperationIDKey(OperationCode opcode);
    OperationIDKey(OperationCode opcode, const char *name, int name_tag);

    string identifier() const;
    bool operator==(const OperationIDKey &other) const;
    uint64_t hash() const;
  };

  /* Typedef for container of operations */
  ComponentNode();
  ~ComponentNode();

  /** Initialize 'component' node - from pointer data given. */
  void init(const ID *id, const char *subdata) override;

  virtual string identifier() const override;

  /* Find an existing operation, if requested operation does not exist
   * nullptr will be returned. */
  OperationNode *find_operation(OperationIDKey key) const;
  OperationNode *find_operation(OperationCode opcode, const char *name, int name_tag) const;

  /* Find an existing operation, will throw an assert() if it does not exist. */
  OperationNode *get_operation(OperationIDKey key) const;
  OperationNode *get_operation(OperationCode opcode, const char *name, int name_tag) const;

  /* Check operation exists and return it. */
  bool has_operation(OperationIDKey key) const;
  bool has_operation(OperationCode opcode, const char *name, int name_tag) const;

  /**
   * Create a new node for representing an operation and add this to graph
   * \warning If an existing node is found, it will be modified. This helps
   * when node may have been partially created earlier (e.g. parent ref before
   * parent item is added)
   *
   * \param opcode: The operation to perform.
   * \param name: Identifier for operation - used to find/locate it again.
   */
  OperationNode *add_operation(const DepsEvalOperationCb &op,
                               OperationCode opcode,
                               const char *name,
                               int name_tag);

  /* Entry/exit operations management.
   *
   * Use those instead of direct set since this will perform sanity checks. */
  void set_entry_operation(OperationNode *op_node);
  void set_exit_operation(OperationNode *op_node);

  void clear_operations();

  virtual void tag_update(Depsgraph *graph, eUpdateSource source) override;

  virtual OperationNode *get_entry_operation() override;
  virtual OperationNode *get_exit_operation() override;

  void finalize_build(Depsgraph *graph);

  IDNode *owner;

  /* ** Inner nodes for this component ** */

  /* Operations stored as a hash map, for faster build.
   * This hash map will be freed when graph is fully built. */
  Map<ComponentNode::OperationIDKey, OperationNode *> *operations_map;

  /* This is a "normal" list of operations, used by evaluation
   * and other routines after construction. */
  Vector<OperationNode *> operations;

  OperationNode *entry_operation;
  OperationNode *exit_operation;

  virtual bool depends_on_cow()
  {
    return true;
  }

  /* Denotes whether COW component is to be tagged when this component
   * is tagged for update. */
  virtual bool need_tag_cow_before_update()
  {
    return true;
  }

  /* Denotes whether this component affects (possibly indirectly) on a
   * directly visible object. */
  bool affects_directly_visible;
};

/* ---------------------------------------- */

#define DEG_COMPONENT_NODE_DEFINE_TYPEINFO(NodeType, type_, type_name_, id_recalc_tag) \
  const Node::TypeInfo NodeType::typeinfo = Node::TypeInfo(type_, type_name_, id_recalc_tag)

#define DEG_COMPONENT_NODE_DECLARE DEG_DEPSNODE_DECLARE

#define DEG_COMPONENT_NODE_DEFINE(name, NAME, id_recalc_tag) \
  DEG_COMPONENT_NODE_DEFINE_TYPEINFO( \
      name##ComponentNode, NodeType::NAME, #name " Component", id_recalc_tag); \
  static DepsNodeFactoryImpl<name##ComponentNode> DNTI_##NAME

#define DEG_COMPONENT_NODE_DECLARE_GENERIC(name) \
  struct name##ComponentNode : public ComponentNode { \
    DEG_COMPONENT_NODE_DECLARE; \
  }

/* When updating object data in edit-mode, don't request COW update since this will duplicate
 * all object data which is unnecessary when the edit-mode data is used for calculating modifiers.
 *
 * TODO: Investigate modes besides edit-mode. */
#define DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_OBDATA_IN_EDIT_MODE(name) \
  struct name##ComponentNode : public ComponentNode { \
    DEG_COMPONENT_NODE_DECLARE; \
    virtual bool need_tag_cow_before_update() override \
    { \
      if (OB_DATA_SUPPORT_EDITMODE(owner->id_type) && \
          BKE_object_data_is_in_editmode(owner->id_orig)) { \
        return false; \
      } \
      return true; \
    } \
  }

#define DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(name) \
  struct name##ComponentNode : public ComponentNode { \
    DEG_COMPONENT_NODE_DECLARE; \
    virtual bool need_tag_cow_before_update() \
    { \
      return false; \
    } \
  }

#define DEG_COMPONENT_NODE_DECLARE_NO_COW(name) \
  struct name##ComponentNode : public ComponentNode { \
    DEG_COMPONENT_NODE_DECLARE; \
    virtual bool depends_on_cow() \
    { \
      return false; \
    } \
  }

DEG_COMPONENT_NODE_DECLARE_GENERIC(Animation);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(BatchCache);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Cache);
DEG_COMPONENT_NODE_DECLARE_GENERIC(CopyOnWrite);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_OBDATA_IN_EDIT_MODE(Geometry);
DEG_COMPONENT_NODE_DECLARE_GENERIC(ImageAnimation);
DEG_COMPONENT_NODE_DECLARE_GENERIC(LayerCollections);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Particles);
DEG_COMPONENT_NODE_DECLARE_GENERIC(ParticleSettings);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Pose);
DEG_COMPONENT_NODE_DECLARE_GENERIC(PointCache);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Proxy);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Sequencer);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(Shading);
DEG_COMPONENT_NODE_DECLARE_GENERIC(ShadingParameters);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Transform);
DEG_COMPONENT_NODE_DECLARE_NO_COW_TAG_ON_UPDATE(ObjectFromLayer);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Dupli);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Synchronization);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Audio);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Armature);
DEG_COMPONENT_NODE_DECLARE_GENERIC(GenericDatablock);
DEG_COMPONENT_NODE_DECLARE_NO_COW(Visibility);
DEG_COMPONENT_NODE_DECLARE_GENERIC(Simulation);
DEG_COMPONENT_NODE_DECLARE_GENERIC(NTreeOutput);

/* Bone Component */
struct BoneComponentNode : public ComponentNode {
  /** Initialize 'bone component' node - from pointer data given. */
  void init(const ID *id, const char *subdata);

  struct bPoseChannel *pchan; /* the bone that this component represents */

  DEG_COMPONENT_NODE_DECLARE;
};

/* Eventually we would not tag parameters in all cases.
 * Support for this each ID needs to be added on an individual basis. */
struct ParametersComponentNode : public ComponentNode {
  virtual bool need_tag_cow_before_update() override
  {
    if (ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW(owner->id_type)) {
      /* Disabled as this is not true for newly added objects, needs investigation. */
      // BLI_assert(deg_copy_on_write_is_expanded(owner->id_cow));
      return false;
    }
    return true;
  }

  DEG_COMPONENT_NODE_DECLARE;
};

void deg_register_component_depsnodes();

}  // namespace deg
}  // namespace blender

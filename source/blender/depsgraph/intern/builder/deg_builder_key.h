/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/builder/deg_builder_rna.h"
#include "intern/node/deg_node_component.hh"
#include "intern/node/deg_node_id.hh"
#include "intern/node/deg_node_operation.hh"

#include "DNA_ID.h"

#include "RNA_types.hh"

struct ID;
struct PropertyRNA;

namespace blender::deg {

struct TimeSourceKey {
  TimeSourceKey() = default;

  std::string identifier() const;
};

struct ComponentKey {
  ComponentKey() = default;

  ComponentKey(const ID *id, NodeType type, const char *name = "") : id(id), type(type), name(name)
  {
  }

  std::string identifier() const;

  const ID *id = nullptr;
  NodeType type = NodeType::UNDEFINED;
  const char *name = "";
};

struct OperationKey {
  OperationKey() = default;

  OperationKey(const ID *id, NodeType component_type, const char *name, int name_tag = -1)
      : id(id), component_type(component_type), name(name), name_tag(name_tag)
  {
  }

  OperationKey(const ID *id,
               NodeType component_type,
               const char *component_name,
               const char *name,
               int name_tag)
      : id(id),
        component_type(component_type),
        component_name(component_name),
        name(name),
        name_tag(name_tag)
  {
  }

  OperationKey(const ID *id, NodeType component_type, OperationCode opcode)
      : id(id), component_type(component_type), opcode(opcode)
  {
  }

  OperationKey(const ID *id,
               NodeType component_type,
               const char *component_name,
               OperationCode opcode)
      : id(id), component_type(component_type), component_name(component_name), opcode(opcode)
  {
  }

  OperationKey(const ID *id,
               NodeType component_type,
               OperationCode opcode,
               const char *name,
               int name_tag = -1)
      : id(id), component_type(component_type), opcode(opcode), name(name), name_tag(name_tag)
  {
  }
  OperationKey(const ID *id,
               NodeType component_type,
               const char *component_name,
               OperationCode opcode,
               const char *name,
               int name_tag = -1)
      : id(id),
        component_type(component_type),
        component_name(component_name),
        opcode(opcode),
        name(name),
        name_tag(name_tag)
  {
  }

  OperationKey(OperationKey &&other) noexcept = default;
  OperationKey &operator=(OperationKey &&other) = default;

  OperationKey(const OperationKey &other) = default;
  OperationKey &operator=(const OperationKey &other) = default;

  std::string identifier() const;

  const ID *id = nullptr;
  NodeType component_type = NodeType::UNDEFINED;
  const char *component_name = "";
  OperationCode opcode = OperationCode::OPERATION;
  const char *name = "";
  int name_tag = -1;
};

/* Similar to the #OperationKey but does not contain external references, which makes it
 * suitable to identify operations even after the original database or graph was destroyed.
 * The downside of this key over the #OperationKey is that it performs string allocation upon
 * the key construction. */
struct PersistentOperationKey : public OperationKey {
  /* Create the key which identifies the given operation node. */
  PersistentOperationKey(const OperationNode *operation_node)
  {
    const ComponentNode *component_node = operation_node->owner;
    const IDNode *id_node = component_node->owner;

    /* Copy names over to our object, so that the key stays valid even after the `operation_node`
     * is destroyed. */
    component_name_storage_ = component_node->name;
    name_storage_ = operation_node->name;

    /* Assign fields used by the #OperationKey API. */
    id = id_node->id_orig;
    component_type = component_node->type;
    component_name = component_name_storage_.c_str();
    opcode = operation_node->opcode;
    name = name_storage_.c_str();
    name_tag = operation_node->name_tag;
  }

  PersistentOperationKey(PersistentOperationKey &&other) noexcept : OperationKey(other)
  {
    component_name_storage_ = std::move(other.component_name_storage_);
    name_storage_ = std::move(other.name_storage_);

    /* Re-assign pointers to the strings.
     * This is needed because string content can actually change address if the string uses the
     * small string optimization. */
    component_name = component_name_storage_.c_str();
    name = name_storage_.c_str();
  }

  PersistentOperationKey &operator=(PersistentOperationKey &&other) = delete;

  PersistentOperationKey(const PersistentOperationKey &other) = delete;
  PersistentOperationKey &operator=(const PersistentOperationKey &other) = delete;

 private:
  std::string component_name_storage_;
  std::string name_storage_;
};

struct RNAPathKey {
  RNAPathKey(ID *id, const char *path, RNAPointerSource source);
  RNAPathKey(const PointerRNA &target_prop,
             const char *rna_path_from_target_prop,
             RNAPointerSource source);
  RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop, RNAPointerSource source);

  std::string identifier() const;

  ID *id;
  PointerRNA ptr;
  PropertyRNA *prop;
  RNAPointerSource source;
};

}  // namespace blender::deg

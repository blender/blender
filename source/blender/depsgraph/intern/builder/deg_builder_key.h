/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include "intern/builder/deg_builder_rna.h"
#include "intern/depsgraph_type.h"

#include "DNA_ID.h"

#include "RNA_access.h"
#include "RNA_types.h"

struct ID;
struct PropertyRNA;

namespace blender::deg {

struct TimeSourceKey {
  TimeSourceKey() = default;

  string identifier() const;
};

struct ComponentKey {
  ComponentKey() = default;

  inline ComponentKey(const ID *id, NodeType type, const char *name = "")
      : id(id), type(type), name(name)
  {
  }

  string identifier() const;

  const ID *id = nullptr;
  NodeType type = NodeType::UNDEFINED;
  const char *name = "";
};

struct OperationKey {
  OperationKey() = default;

  inline OperationKey(const ID *id, NodeType component_type, const char *name, int name_tag = -1)
      : id(id),
        component_type(component_type),
        component_name(""),
        opcode(OperationCode::OPERATION),
        name(name),
        name_tag(name_tag)
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
        opcode(OperationCode::OPERATION),
        name(name),
        name_tag(name_tag)
  {
  }

  OperationKey(const ID *id, NodeType component_type, OperationCode opcode)
      : id(id),
        component_type(component_type),
        component_name(""),
        opcode(opcode),
        name(""),
        name_tag(-1)
  {
  }

  OperationKey(const ID *id,
               NodeType component_type,
               const char *component_name,
               OperationCode opcode)
      : id(id),
        component_type(component_type),
        component_name(component_name),
        opcode(opcode),
        name(""),
        name_tag(-1)
  {
  }

  OperationKey(const ID *id,
               NodeType component_type,
               OperationCode opcode,
               const char *name,
               int name_tag = -1)
      : id(id),
        component_type(component_type),
        component_name(""),
        opcode(opcode),
        name(name),
        name_tag(name_tag)
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

  string identifier() const;

  const ID *id = nullptr;
  NodeType component_type = NodeType::UNDEFINED;
  const char *component_name = "";
  OperationCode opcode = OperationCode::OPERATION;
  const char *name = "";
  int name_tag = -1;
};

struct RNAPathKey {
  RNAPathKey(ID *id, const char *path, RNAPointerSource source);
  RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop, RNAPointerSource source);

  string identifier() const;

  ID *id;
  PointerRNA ptr;
  PropertyRNA *prop;
  RNAPointerSource source;
};

}  // namespace blender::deg

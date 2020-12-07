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
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

namespace blender::deg {

////////////////////////////////////////////////////////////////////////////////
// Time source.

TimeSourceKey::TimeSourceKey() : id(nullptr)
{
}

TimeSourceKey::TimeSourceKey(ID *id) : id(id)
{
}

string TimeSourceKey::identifier() const
{
  return string("TimeSourceKey");
}

////////////////////////////////////////////////////////////////////////////////
// Component.

ComponentKey::ComponentKey() : id(nullptr), type(NodeType::UNDEFINED), name("")
{
}

ComponentKey::ComponentKey(ID *id, NodeType type, const char *name)
    : id(id), type(type), name(name)
{
}

string ComponentKey::identifier() const
{
  const char *idname = (id) ? id->name : "<None>";
  string result = string("ComponentKey(");
  result += idname;
  result += ", " + string(nodeTypeAsString(type));
  if (name[0] != '\0') {
    result += ", '" + string(name) + "'";
  }
  result += ')';
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Operation.

OperationKey::OperationKey()
    : id(nullptr),
      component_type(NodeType::UNDEFINED),
      component_name(""),
      opcode(OperationCode::OPERATION),
      name(""),
      name_tag(-1)
{
}

OperationKey::OperationKey(ID *id, NodeType component_type, const char *name, int name_tag)
    : id(id),
      component_type(component_type),
      component_name(""),
      opcode(OperationCode::OPERATION),
      name(name),
      name_tag(name_tag)
{
}

OperationKey::OperationKey(
    ID *id, NodeType component_type, const char *component_name, const char *name, int name_tag)
    : id(id),
      component_type(component_type),
      component_name(component_name),
      opcode(OperationCode::OPERATION),
      name(name),
      name_tag(name_tag)
{
}

OperationKey::OperationKey(ID *id, NodeType component_type, OperationCode opcode)
    : id(id),
      component_type(component_type),
      component_name(""),
      opcode(opcode),
      name(""),
      name_tag(-1)
{
}

OperationKey::OperationKey(ID *id,
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

OperationKey::OperationKey(
    ID *id, NodeType component_type, OperationCode opcode, const char *name, int name_tag)
    : id(id),
      component_type(component_type),
      component_name(""),
      opcode(opcode),
      name(name),
      name_tag(name_tag)
{
}

OperationKey::OperationKey(ID *id,
                           NodeType component_type,
                           const char *component_name,
                           OperationCode opcode,
                           const char *name,
                           int name_tag)
    : id(id),
      component_type(component_type),
      component_name(component_name),
      opcode(opcode),
      name(name),
      name_tag(name_tag)
{
}

string OperationKey::identifier() const
{
  string result = string("OperationKey(");
  result += "type: " + string(nodeTypeAsString(component_type));
  result += ", component name: '" + string(component_name) + "'";
  result += ", operation code: " + string(operationCodeAsString(opcode));
  if (name[0] != '\0') {
    result += ", '" + string(name) + "'";
  }
  result += ")";
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// RNA path.

RNAPathKey::RNAPathKey(ID *id, const char *path, RNAPointerSource source) : id(id), source(source)
{
  /* Create ID pointer for root of path lookup. */
  PointerRNA id_ptr;
  RNA_id_pointer_create(id, &id_ptr);
  /* Try to resolve path. */
  int index;
  if (!RNA_path_resolve_full(&id_ptr, path, &ptr, &prop, &index)) {
    ptr = PointerRNA_NULL;
    prop = nullptr;
  }
}

RNAPathKey::RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop, RNAPointerSource source)
    : id(id), ptr(ptr), prop(prop), source(source)
{
}

string RNAPathKey::identifier() const
{
  const char *id_name = (id) ? id->name : "<No ID>";
  const char *prop_name = (prop) ? RNA_property_identifier(prop) : "<No Prop>";
  return string("RnaPathKey(") + "id: " + id_name + ", prop: '" + prop_name + "')";
}

}  // namespace blender::deg

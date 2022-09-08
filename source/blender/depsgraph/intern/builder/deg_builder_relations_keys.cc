/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2013 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

namespace blender::deg {

////////////////////////////////////////////////////////////////////////////////
/* Time source. */

string TimeSourceKey::identifier() const
{
  return string("TimeSourceKey");
}

////////////////////////////////////////////////////////////////////////////////
// Component.

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

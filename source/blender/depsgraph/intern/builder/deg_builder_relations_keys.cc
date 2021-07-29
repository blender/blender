/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * Original Author: Joshua Leung
 * Contributor(s): Based on original depsgraph.c code - Blender Foundation (2005-2013)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_relations_keys.cc
 *  \ingroup depsgraph
 *
 * Methods for constructing depsgraph
 */

#include "intern/builder/deg_builder_relations.h"

namespace DEG {

/////////////////////////////////////////
// Time source.

TimeSourceKey::TimeSourceKey()
        : id(NULL)
{
}

TimeSourceKey::TimeSourceKey(ID *id)
        : id(id)
{
}

string TimeSourceKey::identifier() const
{
	return string("TimeSourceKey");
}

/////////////////////////////////////////
// Component.

ComponentKey::ComponentKey()
        : id(NULL),
          type(DEG_NODE_TYPE_UNDEFINED),
          name("")
{
}

ComponentKey::ComponentKey(ID *id, eDepsNode_Type type, const char *name)
        : id(id),
          type(type),
          name(name)
{
}

string ComponentKey::identifier() const
{
	const char *idname = (id) ? id->name : "<None>";
	char typebuf[5];
	BLI_snprintf(typebuf, sizeof(typebuf), "%d", type);
	return string("ComponentKey(") +
	       idname + ", " + typebuf + ", '" + name + "')";
}

/////////////////////////////////////////
// Operation.

OperationKey::OperationKey()
        : id(NULL),
          component_type(DEG_NODE_TYPE_UNDEFINED),
          component_name(""),
          opcode(DEG_OPCODE_OPERATION),
          name(""),
          name_tag(-1)
{
}

OperationKey::OperationKey(ID *id,
                           eDepsNode_Type component_type,
                           const char *name,
                           int name_tag)
        : id(id),
          component_type(component_type),
          component_name(""),
          opcode(DEG_OPCODE_OPERATION),
          name(name),
          name_tag(name_tag)
{
}

OperationKey::OperationKey(ID *id,
                           eDepsNode_Type component_type,
                           const char *component_name,
                           const char *name,
                           int name_tag)
        : id(id),
          component_type(component_type),
          component_name(component_name),
          opcode(DEG_OPCODE_OPERATION),
          name(name),
          name_tag(name_tag)
{
}

OperationKey::OperationKey(ID *id,
                           eDepsNode_Type component_type,
                           eDepsOperation_Code opcode)
        : id(id),
          component_type(component_type),
          component_name(""),
          opcode(opcode),
          name(""),
          name_tag(-1)
{
}

OperationKey::OperationKey(ID *id,
                           eDepsNode_Type component_type,
                           const char *component_name,
                           eDepsOperation_Code opcode)
        : id(id),
          component_type(component_type),
          component_name(component_name),
          opcode(opcode),
          name(""),
          name_tag(-1)
{
}

OperationKey::OperationKey(ID *id,
                           eDepsNode_Type component_type,
                           eDepsOperation_Code opcode,
                           const char *name,
                           int name_tag)
        : id(id),
          component_type(component_type),
          component_name(""),
          opcode(opcode),
          name(name),
          name_tag(name_tag)
{
}

OperationKey::OperationKey(ID *id,
                           eDepsNode_Type component_type,
                           const char *component_name,
                           eDepsOperation_Code opcode,
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
	char typebuf[5];
	BLI_snprintf(typebuf, sizeof(typebuf), "%d", component_type);
	return string("OperationKey(") +
	       "t: " + typebuf +
	       ", cn: '" + component_name +
	       "', c: " + DEG_OPNAMES[opcode] +
	       ", n: '" + name + "')";
}

/////////////////////////////////////////
// RNA path.

RNAPathKey::RNAPathKey(ID *id, const PointerRNA &ptr, PropertyRNA *prop)
        : id(id),
          ptr(ptr),
          prop(prop)
{
}

string RNAPathKey::identifier() const
{
	const char *id_name   = (id) ?  id->name : "<No ID>";
	const char *prop_name = (prop) ? RNA_property_identifier(prop) : "<No Prop>";
	return string("RnaPathKey(") + "id: " + id_name +
	                               ", prop: " + prop_name +  "')";
}

}  // namespace DEG

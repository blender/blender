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
 * Original Author: Lukas Toenne
 * Contributor(s): Sergey SHarybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/builder/deg_builder_relations_impl.h
 *  \ingroup depsgraph
 */

#pragma once

#include "intern/nodes/deg_node_id.h"

extern "C" {
#include "DNA_ID.h"
}

namespace DEG {

template <typename KeyType>
OperationDepsNode *DepsgraphRelationBuilder::find_operation_node(const KeyType& key)
{
	DepsNode *node = get_node(key);
	return node != NULL ? node->get_exit_operation() : NULL;
}

template <typename KeyFrom, typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const KeyFrom &key_from,
                                            const KeyTo &key_to,
                                            const char *description,
                                            bool check_unique)
{
	DepsNode *node_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	OperationDepsNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationDepsNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (op_from && op_to) {
		add_operation_relation(op_from, op_to, description, check_unique);
	}
	else {
		if (!op_from) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%s) - Could not find op_from (%s)\n",
			        description, key_from.identifier().c_str());
		}
		else {
			fprintf(stderr, "add_relation(%s) - Failed, but op_from (%s) was ok\n",
			        description, key_from.identifier().c_str());
		}
		if (!op_to) {
			/* XXX TODO handle as error or report if needed */
			fprintf(stderr, "add_relation(%s) - Could not find op_to (%s)\n",
			        description, key_to.identifier().c_str());
		}
		else {
			fprintf(stderr, "add_relation(%s) - Failed, but op_to (%s) was ok\n",
			        description, key_to.identifier().c_str());
		}
	}
}

template <typename KeyTo>
void DepsgraphRelationBuilder::add_relation(const TimeSourceKey &key_from,
                                            const KeyTo &key_to,
                                            const char *description,
                                            bool check_unique)
{
	TimeSourceDepsNode *time_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	OperationDepsNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (time_from != NULL && op_to != NULL) {
		add_time_relation(time_from, op_to, description, check_unique);
	}
}

template <typename KeyType>
void DepsgraphRelationBuilder::add_node_handle_relation(
        const KeyType &key_from,
        const DepsNodeHandle *handle,
        const char *description,
        bool check_unique)
{
	DepsNode *node_from = get_node(key_from);
	OperationDepsNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationDepsNode *op_to = handle->node->get_entry_operation();
	if (op_from != NULL && op_to != NULL) {
		add_operation_relation(op_from, op_to, description, check_unique);
	}
	else {
		if (!op_from) {
			fprintf(stderr, "add_node_handle_relation(%s) - Could not find op_from (%s)\n",
			        description, key_from.identifier().c_str());
		}
		if (!op_to) {
			fprintf(stderr, "add_node_handle_relation(%s) - Could not find op_to (%s)\n",
			        description, key_from.identifier().c_str());
		}
	}
}

template <typename KeyType>
DepsNodeHandle DepsgraphRelationBuilder::create_node_handle(
        const KeyType &key,
        const char *default_name)
{
	return DepsNodeHandle(this, get_node(key), default_name);
}

/* Rig compatibility: we check if bone is using local transform as a variable
 * for driver on itself and ignore those relations to avoid "false-positive"
 * dependency cycles.
 */
template <typename KeyFrom, typename KeyTo>
bool DepsgraphRelationBuilder::is_same_bone_dependency(const KeyFrom& key_from,
                                                       const KeyTo& key_to)
{
	/* Get operations for requested keys. */
	DepsNode *node_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	if (node_from == NULL || node_to == NULL) {
		return false;
	}
	OperationDepsNode *op_from = node_from->get_exit_operation();
	OperationDepsNode *op_to = node_to->get_entry_operation();
	if (op_from == NULL || op_to == NULL) {
		return false;
	}
	/* Different armatures, bone can't be the same. */
	if (op_from->owner->owner != op_to->owner->owner) {
		return false;
	}
	/* We are only interested in relations like BONE_DONE -> BONE_LOCAL... */
	if (!(op_from->opcode == DEG_OPCODE_BONE_DONE &&
	      op_to->opcode == DEG_OPCODE_BONE_LOCAL))
	{
		return false;
	}
	/* ... BUT, we also need to check if it's same bone.  */
	if (!STREQ(op_from->owner->name, op_to->owner->name)) {
		return false;
	}
	return true;
}

template <typename KeyFrom, typename KeyTo>
bool DepsgraphRelationBuilder::is_same_nodetree_node_dependency(
        const KeyFrom& key_from,
        const KeyTo& key_to)
{
	/* Get operations for requested keys. */
	DepsNode *node_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	if (node_from == NULL || node_to == NULL) {
		return false;
	}
	OperationDepsNode *op_from = node_from->get_exit_operation();
	OperationDepsNode *op_to = node_to->get_entry_operation();
	if (op_from == NULL || op_to == NULL) {
		return false;
	}
	/* Check if this is actually a node tree. */
	if (GS(op_from->owner->owner->id->name) != ID_NT) {
		return false;
	}
	/* Different node trees. */
	if (op_from->owner->owner != op_to->owner->owner) {
		return false;
	}
	/* We are only interested in relations like BONE_DONE -> BONE_LOCAL... */
	if (!(op_from->opcode == DEG_OPCODE_PARAMETERS_EVAL &&
	      op_to->opcode == DEG_OPCODE_PARAMETERS_EVAL))
	{
		return false;
	}
	return true;
}

template <typename KeyFrom, typename KeyTo>
bool DepsgraphRelationBuilder::is_same_shapekey_dependency(
        const KeyFrom& key_from,
        const KeyTo& key_to)
{
	/* Get operations for requested keys. */
	DepsNode *node_from = get_node(key_from);
	DepsNode *node_to = get_node(key_to);
	if (node_from == NULL || node_to == NULL) {
		return false;
	}
	OperationDepsNode *op_from = node_from->get_exit_operation();
	OperationDepsNode *op_to = node_to->get_entry_operation();
	if (op_from == NULL || op_to == NULL) {
		return false;
	}
	/* Check if this is actually a shape key datablock. */
	if (GS(op_from->owner->owner->id->name) != ID_KE) {
		return false;
	}
	/* Different key data blocks. */
	if (op_from->owner->owner != op_to->owner->owner) {
		return false;
	}
	return true;
}

}  // namespace DEG

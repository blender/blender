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

/** \file \ingroup depsgraph
 */

#pragma once

#include "intern/node/deg_node_id.h"

extern "C" {
#include "DNA_ID.h"
}

namespace DEG {

template <typename KeyType>
OperationNode *DepsgraphRelationBuilder::find_operation_node(const KeyType& key)
{
	Node *node = get_node(key);
	return node != NULL ? node->get_exit_operation() : NULL;
}

template <typename KeyFrom, typename KeyTo>
Relation *DepsgraphRelationBuilder::add_relation(const KeyFrom &key_from,
                                                 const KeyTo &key_to,
                                                 const char *description,
                                                 int flags)
{
	Node *node_from = get_node(key_from);
	Node *node_to = get_node(key_to);
	OperationNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (op_from && op_to) {
		return add_operation_relation(op_from, op_to, description, flags);
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
	return NULL;
}

template <typename KeyTo>
Relation *DepsgraphRelationBuilder::add_relation(
        const TimeSourceKey &key_from,
        const KeyTo &key_to,
        const char *description,
        int flags)
{
	TimeSourceNode *time_from = get_node(key_from);
	Node *node_to = get_node(key_to);
	OperationNode *op_to = node_to ? node_to->get_entry_operation() : NULL;
	if (time_from != NULL && op_to != NULL) {
		return add_time_relation(
		        time_from, op_to, description, flags);
	}
	return NULL;
}

template <typename KeyType>
Relation *DepsgraphRelationBuilder::add_node_handle_relation(
        const KeyType &key_from,
        const DepsNodeHandle *handle,
        const char *description,
        int flags)
{
	Node *node_from = get_node(key_from);
	OperationNode *op_from = node_from ? node_from->get_exit_operation() : NULL;
	OperationNode *op_to = handle->node->get_entry_operation();
	if (op_from != NULL && op_to != NULL) {
		return add_operation_relation(op_from, op_to, description, flags);
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
	return NULL;
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
	Node *node_from = get_node(key_from);
	Node *node_to = get_node(key_to);
	if (node_from == NULL || node_to == NULL) {
		return false;
	}
	OperationNode *op_from = node_from->get_exit_operation();
	OperationNode *op_to = node_to->get_entry_operation();
	if (op_from == NULL || op_to == NULL) {
		return false;
	}
	/* Different armatures, bone can't be the same. */
	if (op_from->owner->owner != op_to->owner->owner) {
		return false;
	}
	/* We are only interested in relations like BONE_DONE -> BONE_LOCAL... */
	if (!(op_from->opcode == OperationCode::BONE_DONE &&
	      op_to->opcode == OperationCode::BONE_LOCAL))
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
	Node *node_from = get_node(key_from);
	Node *node_to = get_node(key_to);
	if (node_from == NULL || node_to == NULL) {
		return false;
	}
	OperationNode *op_from = node_from->get_exit_operation();
	OperationNode *op_to = node_to->get_entry_operation();
	if (op_from == NULL || op_to == NULL) {
		return false;
	}
	/* Check if this is actually a node tree. */
	if (GS(op_from->owner->owner->id_orig->name) != ID_NT) {
		return false;
	}
	/* Different node trees. */
	if (op_from->owner->owner != op_to->owner->owner) {
		return false;
	}
	/* We are only interested in relations like BONE_DONE -> BONE_LOCAL... */
	if (!(op_from->opcode == OperationCode::PARAMETERS_EVAL &&
	      op_to->opcode == OperationCode::PARAMETERS_EVAL))
	{
		return false;
	}
	return true;
}

}  // namespace DEG

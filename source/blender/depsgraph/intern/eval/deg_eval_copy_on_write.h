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
 * The Original Code is Copyright (C) 20137Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include <stddef.h>

struct ID;

/* Uncomment this to have verbose log about original and CoW pointers
 * logged, with detailed information when they are allocated, expanded
 * and remapped.
 */
// #define DEG_DEBUG_COW_POINTERS

#ifdef DEG_DEBUG_COW_POINTERS
#  define DEG_COW_PRINT(format, ...) printf(format, __VA_ARGS__);
#else
#  define DEG_COW_PRINT(format, ...)
#endif

struct Depsgraph;

namespace DEG {

struct Depsgraph;
class DepsgraphNodeBuilder;
struct IDNode;

/* Get fully expanded (ready for use) copy-on-write data-block for the given
 * original data-block.
 */
ID *deg_expand_copy_on_write_datablock(const struct Depsgraph *depsgraph,
                                       const IDNode *id_node,
                                       DepsgraphNodeBuilder *node_builder = NULL,
                                       bool create_placeholders = false);
ID *deg_expand_copy_on_write_datablock(const struct Depsgraph *depsgraph,
                                       struct ID *id_orig,
                                       DepsgraphNodeBuilder *node_builder = NULL,
                                       bool create_placeholders = false);

/* Makes sure given CoW data-block is brought back to state of the original
 * data-block.
 */
ID *deg_update_copy_on_write_datablock(const struct Depsgraph *depsgraph, const IDNode *id_node);
ID *deg_update_copy_on_write_datablock(const struct Depsgraph *depsgraph, struct ID *id_orig);

/* Helper function which frees memory used by copy-on-written databnlock. */
void deg_free_copy_on_write_datablock(struct ID *id_cow);

/* Callback function for depsgraph operation node which ensures copy-on-write
 * datablock is ready for use by further evaluation routines.
 */
void deg_evaluate_copy_on_write(struct ::Depsgraph *depsgraph, const struct IDNode *id_node);

/* Check that given ID is properly expanded and does not have any shallow
 * copies inside. */
bool deg_validate_copy_on_write_datablock(ID *id_cow);

/* Tag given ID block as being copy-on-wtritten. */
void deg_tag_copy_on_write_id(struct ID *id_cow, const struct ID *id_orig);

/* Check whether ID datablock is expanded.
 *
 * TODO(sergey): Make it an inline function or a macro.
 */
bool deg_copy_on_write_is_expanded(const struct ID *id_cow);

/* Check whether copy-on-write datablock is needed for given ID.
 *
 * There are some exceptions on data-blocks which are covered by dependency graph
 * but which we don't want to start duplicating.
 *
 * This includes images.
 */
bool deg_copy_on_write_is_needed(const ID *id_orig);

}  // namespace DEG

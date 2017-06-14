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
 * The Original Code is Copyright (C) 20137Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Sergey Sharybin
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/depsgraph/intern/eval/deg_eval_copy_on_write.h
 *  \ingroup depsgraph
 */

#pragma once

struct EvaluationContext;
struct ID;

/* Unkomment this to have verbose log about original and CoW pointers
 * logged, with detailed information when they are allocated, expanded
 * and remapped.
 */
// #define DEG_DEBUG_COW_POINTERS

#ifdef DEG_DEBUG_COW_POINTERS
#  define DEG_COW_PRINT(format, ...) printf(format, __VA_ARGS__);
#else
#  define DEG_COW_PRINT(format, ...)
#endif

namespace DEG {

struct Depsgraph;
struct IDDepsNode;

/* Get fully expanded (ready for use) copy-on-write datablock for the given
 * original datablock.
 */
ID *deg_expand_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       const IDDepsNode *id_node);
ID *deg_expand_copy_on_write_datablock(const struct Depsgraph *depsgraph,
                                       struct ID *id_orig);

/* Makes sure given CoW datablock is brought back to state of the original
 * datablock.
 */
ID *deg_update_copy_on_write_datablock(const Depsgraph *depsgraph,
                                       const IDDepsNode *id_node);
ID *deg_update_copy_on_write_datablock(const struct Depsgraph *depsgraph,
                                       struct ID *id_orig);

/* Helper function which frees memory used by copy-on-written databnlock. */
void deg_free_copy_on_write_datablock(struct ID *id_cow);

/* Callback function for depsgraph operation node which ensures copy-on-write
 * datablock is ready for use by further evaluation routines.
 */
void deg_evaluate_copy_on_write(struct EvaluationContext *eval_ctx,
                                const struct Depsgraph *depsgraph,
                                const struct IDDepsNode *id_node);

/* Check that given ID is propely expanded and does not have any shallow
 * copies inside.
  */
bool deg_validate_copy_on_write_datablock(ID *id_cow);

}  // namespace DEG

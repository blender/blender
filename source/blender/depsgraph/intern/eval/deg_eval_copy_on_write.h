/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup depsgraph
 */

#pragma once

#include <cstddef>

#include "DNA_ID.h"

struct ID;
struct Depsgraph;

/* Uncomment this to have verbose log about original and evaluated pointers
 * logged, with detailed information when they are allocated, expanded
 * and remapped.
 */
// #define DEG_DEBUG_COW_POINTERS

#ifdef DEG_DEBUG_COW_POINTERS
#  define DEG_COW_PRINT(format, ...) printf(format, __VA_ARGS__);
#else
#  define DEG_COW_PRINT(format, ...)
#endif

namespace blender::deg {

struct Depsgraph;
class DepsgraphNodeBuilder;
struct IDNode;

/**
 * Makes sure given evaluated data-block is brought back to state of the original
 * data-block.
 */
ID *deg_update_eval_copy_datablock(const struct Depsgraph *depsgraph, const IDNode *id_node);
ID *deg_update_eval_copy_datablock(const struct Depsgraph *depsgraph, struct ID *id_orig);

/** Helper function which frees memory used by copy-on-written data-block. */
void deg_free_eval_copy_datablock(struct ID *id_cow);

/**
 * Callback function for depsgraph operation node which ensures evaluated
 * data-block is ready for use by further evaluation routines.
 */
void deg_create_eval_copy(struct ::Depsgraph *depsgraph, const struct IDNode *id_node);

/**
 * Check that given ID is properly expanded and does not have any shallow
 * copies inside.
 */
bool deg_validate_eval_copy_datablock(ID *id_cow);

/** Tag given ID block as being copy-on-eval. */
void deg_tag_eval_copy_id(Depsgraph &depsgraph, struct ID *id_cow, const struct ID *id_orig);

/**
 * Check whether ID data-block is expanded.
 *
 * TODO(sergey): Make it an inline function or a macro.
 */
bool deg_eval_copy_is_expanded(const struct ID *id_cow);

/**
 * Check whether an evaluated data-block copy is needed for given ID.
 *
 * There are some exceptions on data-blocks which are covered by dependency graph
 * but which we don't want to start duplicating.
 *
 * This includes images.
 */
bool deg_eval_copy_is_needed(const ID *id_orig);
bool deg_eval_copy_is_needed(const ID_Type id_type);

}  // namespace blender::deg

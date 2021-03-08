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
 */

/** \file
 * \ingroup spoutliner
 *
 * C-API for the Tree-Element types.
 * This API shouldn't stay for long. All tree building should eventually be done through C++ types,
 * with more type safety and an easier to reason about design.
 */

#pragma once

#include "DNA_space_types.h"

struct TreeElement;

#ifdef __cplusplus
extern "C" {
#endif

/** C alias for an #AbstractTreeElement handle. */
typedef struct TreeElementType TreeElementType;

TreeElementType *outliner_tree_element_type_create(int type, TreeElement *legacy_te, void *idv);
void outliner_tree_element_type_free(TreeElementType **type);

void outliner_tree_element_type_expand(TreeElementType *type, SpaceOutliner *space_outliner);
bool outliner_tree_element_type_is_expand_valid(TreeElementType *type);
bool outliner_tree_element_type_expand_poll(TreeElementType *type, SpaceOutliner *space_outliner);
void outliner_tree_element_type_post_expand(TreeElementType *type, SpaceOutliner *space_outliner);

#ifdef __cplusplus
}
#endif

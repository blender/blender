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
 * C-API for the Tree-Display types.
 */

#pragma once

#include "DNA_space_types.h"

struct ListBase;

#ifdef __cplusplus
extern "C" {
#endif

/** C alias for an #AbstractTreeDisplay handle. */
typedef struct TreeDisplay TreeDisplay;

/**
 * \brief The data to build the tree from.
 */
typedef struct TreeSourceData {
  struct Main *bmain;
  struct Scene *scene;
  struct ViewLayer *view_layer;
} TreeSourceData;

TreeDisplay *outliner_tree_display_create(eSpaceOutliner_Mode mode, SpaceOutliner *space_outliner);
void outliner_tree_display_destroy(TreeDisplay **tree_display);

ListBase outliner_tree_display_build_tree(TreeDisplay *tree_display, TreeSourceData *source_data);

/* The following functions are needed to build the tree. They are calls back into C; the way
 * elements are created should be refactored and ported to C++ with a new design/API too. */
/**
 * TODO: this function needs to be split up! It's getting a bit too large...
 *
 * \note "ID" is not always a real ID.
 * \note If child items are only added to the tree if the item is open,
 * the `TSE_` type _must_ be added to #outliner_element_needs_rebuild_on_open_change().
 */
struct TreeElement *outliner_add_element(SpaceOutliner *space_outliner,
                                         ListBase *lb,
                                         void *idv,
                                         struct TreeElement *parent,
                                         short type,
                                         short index);
/* make sure elements are correctly nested */
void outliner_make_object_parent_hierarchy(ListBase *lb);
bool outliner_animdata_test(const struct AnimData *adt);
TreeElement *outliner_add_collection_recursive(SpaceOutliner *space_outliner,
                                               struct Collection *collection,
                                               TreeElement *ten);

const char *outliner_idcode_to_plural(short idcode);

#ifdef __cplusplus
}
#endif

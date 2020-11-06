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
 */

#pragma once

#include "DNA_space_types.h"

struct bContext;
struct ListBase;
struct SpaceOutliner;
struct TreeSourceData;

#ifdef __cplusplus

namespace blender {
namespace outliner {

using Tree = ListBase;

class AbstractTreeView {
 public:
  virtual ~AbstractTreeView() = default;

  /** Build a tree for this view and the current context. */
  virtual Tree buildTree(const TreeSourceData &source_data, SpaceOutliner &space_outliner) = 0;
};

class TreeViewViewLayer : public AbstractTreeView {
 public:
  Tree buildTree(const TreeSourceData &source_data, SpaceOutliner &space_outliner) override final;
};

}  // namespace outliner
}  // namespace blender

extern "C" {
#endif

/* -------------------------------------------------------------------- */
/* C-API */

typedef struct TreeView TreeView;

/**
 * \brief The data to build the tree from.
 */
typedef struct TreeSourceData {
  struct Main *bmain;
  struct Scene *scene;
  struct ViewLayer *view_layer;
} TreeSourceData;

TreeView *outliner_tree_view_create(eSpaceOutliner_Mode mode);
void outliner_tree_view_destroy(TreeView **tree_view);

ListBase outliner_tree_view_build_tree(TreeView *tree_view,
                                       TreeSourceData *source_data,
                                       struct SpaceOutliner *space_outliner);

/* The following functions are needed to build the actual tree. Could be moved to a helper class
 * (e.g. TreeBuilder). */
struct TreeElement *outliner_add_element(struct SpaceOutliner *space_outliner,
                                         ListBase *lb,
                                         void *idv,
                                         struct TreeElement *parent,
                                         short type,
                                         short index);
void outliner_make_object_parent_hierarchy(ListBase *lb);

#ifdef __cplusplus
}
#endif

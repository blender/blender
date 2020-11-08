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
 * For now all sub-class declarations of #AbstractTreeView are in this file. They could be moved
 * into own headers of course.
 */

#pragma once

#include "DNA_space_types.h"

struct ListBase;
struct Main;
struct SpaceOutliner;
struct TreeElement;
struct TreeSourceData;

#ifdef __cplusplus

namespace blender {
namespace ed {
namespace outliner {

/* -------------------------------------------------------------------- */
/* Tree-View Interface */

/**
 * \brief Base Class For Tree-Views
 *
 * Abstract base class defining the interface for tree-view variants. For each Outliner display
 * type (e.g View Layer, Scenes, Blender File), a derived class implements a #buildTree() function,
 * that based on Blender data (#TreeSourceData), builds a custom tree of whatever data it wants to
 * visualize.
 */
class AbstractTreeView {
 public:
  AbstractTreeView(SpaceOutliner &space_outliner) : _space_outliner(space_outliner)
  {
  }
  virtual ~AbstractTreeView() = default;

  /**
   * Build a tree for this view with the Blender context data given in \a source_data and the view
   * settings in \a space_outliner.
   */
  virtual ListBase buildTree(const TreeSourceData &source_data) = 0;

 protected:
  /** All derived classes will need a handle to this, so storing it in the base for convenience. */
  SpaceOutliner &_space_outliner;
};

/* -------------------------------------------------------------------- */
/* View Layer Tree-View */

/**
 * \brief Tree-View for the View Layer display mode.
 */
class TreeViewViewLayer final : public AbstractTreeView {
  ViewLayer *_view_layer = nullptr;
  bool _show_objects = true;

 public:
  TreeViewViewLayer(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  void add_view_layer(ListBase &, TreeElement &);
  void add_layer_collections_recursive(ListBase &, ListBase &, TreeElement &);
  void add_layer_collection_objects(ListBase &, LayerCollection &, TreeElement &);
  void add_layer_collection_objects_children(TreeElement &);
};

/* -------------------------------------------------------------------- */
/* Library Tree-View */

/**
 * \brief Tree-View for the Libraries display mode.
 */
class TreeViewLibraries final : public AbstractTreeView {
 public:
  TreeViewLibraries(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  TreeElement *add_library_contents(Main &, ListBase &, Library *) const;
  bool library_id_filter_poll(Library *lib, ID *id) const;
  short id_filter_get() const;
};

}  // namespace outliner
}  // namespace ed
}  // namespace blender

extern "C" {
#endif

/* -------------------------------------------------------------------- */
/* C-API */

/** There is no actual implementation of this, it's the C name for an #AbstractTreeView handle. */
typedef struct TreeView TreeView;

/**
 * \brief The data to build the tree from.
 */
typedef struct TreeSourceData {
  struct Main *bmain;
  struct Scene *scene;
  struct ViewLayer *view_layer;
} TreeSourceData;

TreeView *outliner_tree_view_create(eSpaceOutliner_Mode mode, SpaceOutliner *space_outliner);
void outliner_tree_view_destroy(TreeView **tree_view);

ListBase outliner_tree_view_build_tree(TreeView *tree_view, TreeSourceData *source_data);

/* The following functions are needed to build the tree. These are calls back into C; the way
 * elements are created should be refactored and ported to C++ with a new design/API too. */
struct TreeElement *outliner_add_element(struct SpaceOutliner *space_outliner,
                                         ListBase *lb,
                                         void *idv,
                                         struct TreeElement *parent,
                                         short type,
                                         short index);
void outliner_make_object_parent_hierarchy(ListBase *lb);

const char *outliner_idcode_to_plural(short idcode);

#ifdef __cplusplus
}
#endif

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
 * \brief Establish and manage Outliner trees for different display modes.
 *
 * Each Outliner display mode (e.g View Layer, Scenes, Blender File) is implemented as a
 * tree-display class with the #AbstractTreeDisplay interface.
 *
 * Their main responsibility is building the Outliner tree for a display mode. For that, they
 * implement the #buildTree() function, which based on Blender data (#TreeSourceData), builds a
 * custom tree of whatever data it wants to visualize.
 * Further, they can implement display mode dependent queries and general operations using the
 * #AbstractTreeDisplay abstraction as common interface.
 *
 * Outliners keep the current tree-display object alive until the next full tree rebuild to keep
 * access to it.
 */

#pragma once

#include "tree_display.h"

struct ListBase;
struct Main;
struct SpaceOutliner;
struct TreeElement;
struct TreeSourceData;

namespace blender::ed::outliner {

/* -------------------------------------------------------------------- */
/* Tree-Display Interface */

/**
 * \brief Base Class For Tree-Displays
 *
 * Abstract base class defining the interface for tree-display variants.
 */
class AbstractTreeDisplay {
 public:
  AbstractTreeDisplay(SpaceOutliner &space_outliner) : space_outliner_(space_outliner)
  {
  }
  virtual ~AbstractTreeDisplay() = default;

  /**
   * Build a tree for this display mode with the Blender context data given in \a source_data and
   * the view settings in \a space_outliner.
   */
  virtual ListBase buildTree(const TreeSourceData &source_data) = 0;

 protected:
  /** All derived classes will need a handle to this, so storing it in the base for convenience. */
  SpaceOutliner &space_outliner_;
};

/* -------------------------------------------------------------------- */
/* View Layer Tree-Display */

/**
 * \brief Tree-Display for the View Layer display mode.
 */
class TreeDisplayViewLayer final : public AbstractTreeDisplay {
  ViewLayer *view_layer_ = nullptr;
  bool show_objects_ = true;

 public:
  TreeDisplayViewLayer(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  void add_view_layer(ListBase &, TreeElement &);
  void add_layer_collections_recursive(ListBase &, ListBase &, TreeElement &);
  void add_layer_collection_objects(ListBase &, LayerCollection &, TreeElement &);
  void add_layer_collection_objects_children(TreeElement &);
};

/* -------------------------------------------------------------------- */
/* Library Tree-Display */

/**
 * \brief Tree-Display for the Libraries display mode.
 */
class TreeDisplayLibraries final : public AbstractTreeDisplay {
 public:
  TreeDisplayLibraries(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  TreeElement *add_library_contents(Main &, ListBase &, Library *) const;
  bool library_id_filter_poll(Library *lib, ID *id) const;
  short id_filter_get() const;
};

/* -------------------------------------------------------------------- */
/* Library Overrides Tree-Display. */

/**
 * \brief Tree-Display for the Library Overrides display mode.
 */
class TreeDisplayOverrideLibrary final : public AbstractTreeDisplay {
 public:
  TreeDisplayOverrideLibrary(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  TreeElement *add_library_contents(Main &, ListBase &, Library *) const;
  bool override_library_id_filter_poll(Library *lib, ID *id) const;
  short id_filter_get() const;
};

/* -------------------------------------------------------------------- */
/* Video Sequencer Tree-Display */

enum SequenceAddOp {
  SEQUENCE_DUPLICATE_NOOP = 0,
  SEQUENCE_DUPLICATE_ADD,
  SEQUENCE_DUPLICATE_NONE
};

/**
 * \brief Tree-Display for the Video Sequencer display mode
 */
class TreeDisplaySequencer final : public AbstractTreeDisplay {
 public:
  TreeDisplaySequencer(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  TreeElement *add_sequencer_contents() const;
  SequenceAddOp need_add_seq_dup(Sequence *seq) const;
  void add_seq_dup(Sequence *seq, TreeElement *te, short index) const;
};

/* -------------------------------------------------------------------- */
/* Orphaned Data Tree-Display */

/**
 * \brief Tree-Display for the Orphaned Data display mode
 */
class TreeDisplayIDOrphans final : public AbstractTreeDisplay {
 public:
  TreeDisplayIDOrphans(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  bool datablock_has_orphans(ListBase &) const;
};

/* -------------------------------------------------------------------- */
/* Scenes Tree-Display */

/**
 * \brief Tree-Display for the Scenes display mode
 */
class TreeDisplayScenes final : public AbstractTreeDisplay {
 public:
  TreeDisplayScenes(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;
};

/* -------------------------------------------------------------------- */
/* Data API Tree-Display */

/**
 * \brief Tree-Display for the Scenes display mode
 */
class TreeDisplayDataAPI final : public AbstractTreeDisplay {
 public:
  TreeDisplayDataAPI(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;
};

}  // namespace blender::ed::outliner

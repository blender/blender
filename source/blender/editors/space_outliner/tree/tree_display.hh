/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include <memory>

struct ID;
struct LayerCollection;
struct Library;
struct ListBase;
struct Main;
struct Scene;
struct Sequence;
struct SpaceOutliner;
struct ViewLayer;

namespace blender::ed::outliner {

struct TreeElement;
class TreeElementID;

/**
 * \brief The data to build the tree from.
 */
struct TreeSourceData {
  Main *bmain;
  Scene *scene;
  ViewLayer *view_layer;

  TreeSourceData(Main &bmain, Scene &scene, ViewLayer &view_layer)
      : bmain(&bmain), scene(&scene), view_layer(&view_layer)
  {
  }
};

/* -------------------------------------------------------------------- */
/* Tree-Display Interface */

/**
 * \brief Base Class For Tree-Displays
 *
 * Abstract base class defining the interface for tree-display variants.
 */
class AbstractTreeDisplay {
 public:
  AbstractTreeDisplay(SpaceOutliner &space_outliner) : space_outliner_(space_outliner) {}
  virtual ~AbstractTreeDisplay() = default;

  static std::unique_ptr<AbstractTreeDisplay> createFromDisplayMode(
      int /*eSpaceOutliner_Mode*/ mode, SpaceOutliner &space_outliner);

  /**
   * Build a tree for this display mode with the Blender context data given in \a source_data and
   * the view settings in \a space_outliner.
   */
  virtual ListBase buildTree(const TreeSourceData &source_data) = 0;

  /**
   * Define if the display mode should be allowed to show a mode column on the left. This column
   * adds an icon to indicate which objects are in the current mode (edit mode, pose mode, etc.)
   * and allows adding other objects to the mode by clicking the icon.
   *
   * Returns false by default.
   */
  virtual bool supportsModeColumn() const;

  /**
   * Some trees may want to skip building children of collapsed parents. This should be done if the
   * tree type may become very complex, which could cause noticeable slowdowns.
   * Problem: This doesn't address performance issues while searching, since all elements are
   * constructed for that. Trees of this type have to be rebuilt for any change to the collapsed
   * state of any element.
   */
  virtual bool is_lazy_built() const;

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
  Scene *scene_ = nullptr;
  ViewLayer *view_layer_ = nullptr;
  bool show_objects_ = true;

 public:
  TreeDisplayViewLayer(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

  bool supportsModeColumn() const override;

 private:
  void add_view_layer(Scene &, ListBase &, TreeElement *);
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
  TreeElement *add_library_contents(Main &, ListBase &, Library *);
  bool library_id_filter_poll(const Library *lib, ID *id) const;
  short id_filter_get() const;
};

/* -------------------------------------------------------------------- */
/* Library Overrides Tree-Display. */

/**
 * \brief Tree-Display for the Library Overrides display mode, Properties view mode.
 */
class TreeDisplayOverrideLibraryProperties final : public AbstractTreeDisplay {
 public:
  TreeDisplayOverrideLibraryProperties(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

 private:
  ListBase add_library_contents(Main &);
  short id_filter_get() const;
};

/**
 * \brief Tree-Display for the Library Overrides display mode, Hierarchies view mode.
 */
class TreeDisplayOverrideLibraryHierarchies final : public AbstractTreeDisplay {
 public:
  TreeDisplayOverrideLibraryHierarchies(SpaceOutliner &space_outliner);

  ListBase buildTree(const TreeSourceData &source_data) override;

  bool is_lazy_built() const override;

 private:
  ListBase build_hierarchy_for_lib_or_main(Main *bmain,
                                           TreeElement &parent_te,
                                           Library *lib = nullptr);
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
  /**
   * Helped function to put duplicate sequence in the same tree.
   */
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

  bool supportsModeColumn() const override;
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

  bool is_lazy_built() const override;
};

}  // namespace blender::ed::outliner

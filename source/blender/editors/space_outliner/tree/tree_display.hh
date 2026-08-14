/* SPDX-FileCopyrightText: 2023 Blender Authors
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
 * implement the #build_tree() function, which based on Blender data (#TreeSourceData), builds a
 * custom tree of whatever data it wants to visualize.
 * Further, they can implement display mode dependent queries and general operations using the
 * #AbstractTreeDisplay abstraction as common interface.
 *
 * Outliners keep the current tree-display object alive until the next full tree rebuild to keep
 * access to it.
 */

#pragma once

#include "DNA_listBase.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "BLI_assert.hh"
#include "BLI_function_ref.hh"

#include "tree_element.hh"

namespace blender {

struct ID;
struct LayerCollection;
struct Library;
struct Main;
struct Scene;
struct Strip;
struct SpaceOutliner;
struct ViewLayer;
struct WorkSpace;

namespace ed::outliner {

struct TreeElement;
class TreeElementID;

/**
 * \brief The data to build the tree from.
 */
struct TreeSourceData {
  Main *bmain;
  WorkSpace *workspace;
  Scene *scene;
  ViewLayer *view_layer;

  TreeSourceData(Main &bmain, WorkSpace &workspace, Scene &scene, ViewLayer &view_layer)
      : bmain(&bmain), workspace(&workspace), scene(&scene), view_layer(&view_layer)
  {
  }
};

/* -------------------------------------------------------------------- */
/* Tree-Element Creation */

/**
 * The non type specific parameters of #AbstractTreeDisplay::add_element(). Grouped into a struct
 * so the type safe overload can take the element type's own construction data as a trailing
 * parameter pack. All of them have a sensible default, so simple cases can just pass `{}`.
 */
struct TreeElementAddParams {
  /** The sub-tree to add the new element to. If null, the sub-tree of #parent is used. */
  ListBaseT<TreeElement> *lb = nullptr;
  /**
   * The parent of the new element. When adding through #AbstractTreeElement::add_element(), null
   * means the element it's called on will be used as parent.
   */
  TreeElement *parent = nullptr;
  /**
   * Index for data arrays. Part of the tree-store identity of the element, to preserve state over
   * rebuilds and file loads.
   */
  int64_t index = 0;
  /**
   * The ID owning the data this element represents, for element types that don't get it passed as
   * construction data (e.g. #TreeElementAnimData, which is constructed from the #AnimData alone).
   * Types that can derive it from their own construction data define a static `owner_id()`
   * instead, and callers don't have to repeat it, see #AbstractTreeDisplay::add_element().
   */
  ID *owner_id = nullptr;
  /**
   * If true, the element may add its own sub-tree. E.g. objects will list their animation data,
   * object data, constraints, modifiers, ... This often adds visual noise, and can be expensive to
   * add in big scenes. So prefer setting this to false when not all too relevant.
   */
  bool expand = true;
  /**
   * The tree-store identity for element types that have no owner ID, and no construction data to
   * derive one from (e.g. #TreeElementIDBase). Only needed if the element type does not define
   * `owner_id()`, see #AbstractTreeDisplay::add_element().
   *
   * \note This is stored in #TreeStoreElem.id, which is an `ID *`. It does not have to point to an
   *       actual ID (yikes!), it is only ever compared, never dereferenced. Of course identifying
   *       the element over rebuilds won't work for volatile data (like stack variables) and
   *       neither will it work over file loads for non-DNA data.
   */
  const void *persistent_ptr = nullptr;
};

namespace detail {

/**
 * Query the tree-store identity of \a ElementType from its static `owner_id()`, which mirrors the
 * type's constructor signature (minus the #TreeElement). Types without an owner ID simply don't
 * define it, and can use #TreeElementAddParams.persistent_ptr instead.
 */
template<typename ElementType, typename... Args> ID *element_owner_id(Args &...args)
{
  if constexpr (requires { ElementType::owner_id(args...); }) {
    return ElementType::owner_id(args...);
  }
  else {
    ((void)args, ...);
    return nullptr;
  }
}

/**
 * Query the tree-store identity of \a ElementType from its static `persistent_ptr()`, for types
 * that have no owner ID but can point at their own data to be identified by. Falls back to
 * #TreeElementAddParams.persistent_ptr for types that only the caller can identify.
 *
 * Having no persistent data pointer can be valid, by defining #ElementType::allow_null_identity.
 * But then state (like open/collapsed state) is not preserved over rebuilds and file loads, and
 * there may be glitches because the wrong tree-store element gets reused.
 */
template<typename ElementType, typename... Args>
const void *element_persistent_ptr(const TreeElementAddParams &params, Args &...args)
{
  if constexpr (requires { ElementType::persistent_ptr(args...); }) {
    BLI_assert_msg(params.persistent_ptr == nullptr,
                   "Element type nominates its own tree-store identity, passing one explicitly "
                   "would have no effect");
    return ElementType::persistent_ptr(args...);
  }
  else {
    ((void)args, ...);
    return params.persistent_ptr;
  }
}

}  // namespace detail

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

  static std::unique_ptr<AbstractTreeDisplay> create_from_display_mode(
      int /*eSpaceOutliner_Mode*/ mode, SpaceOutliner &space_outliner);

  /**
   * Build a tree for this display mode with the Blender context data given in \a source_data and
   * the view settings in \a space_outliner.
   */
  virtual ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) = 0;

  /**
   * Define if the display mode should be allowed to show a mode column on the left. This column
   * adds an icon to indicate which objects are in the current mode (edit mode, pose mode, etc.)
   * and allows adding other objects to the mode by clicking the icon.
   *
   * Returns false by default.
   */
  virtual bool supports_mode_column() const;

  /**
   * Some trees may want to skip building children of collapsed parents. This should be done if the
   * tree type may become very complex, which could cause noticeable slowdowns.
   * Problem: This doesn't address performance issues while searching, since all elements are
   * constructed for that. Trees of this type have to be rebuilt for any change to the collapsed
   * state of any element.
   */
  virtual bool is_lazy_built() const;

  /**
   * \note If child items are only added to the tree if the item is open, the `TSE_` type _must_ be
   *       added to #outliner_element_needs_rebuild_on_open_change().
   *
   * \param owner_id: The ID owning the represented data (or the ID itself if the element
   *                  represents an ID directly). This is crucial to recognize tree elements over
   *                  rebuilds, so that state like opened and selected is preserved. If this is not
   *                  null, the \a create_data pointer will be used instead, refer to its
   *                  description.
   * \param create_data: Data passed to the constructor of the corresponding #AbstractTreeElement
   *                     sub-type. If \a owner_id is not set, this pointer will be stored in an
   *                     attempt to identify the element over rebuilds, so that state like opened
   *                     and selected is preserved. Of course that won't work for volatile data
   *                     (like stack variables).
   * \param expand: If true, the element may add its own sub-tree. E.g. objects will list their
   *                animation data, object data, constraints, modifiers, ... This often adds visual
   *                noise, and can be expensive to add in big scenes. So prefer setting this to
   *                false.
   */
  TreeElement *add_element(ListBaseT<TreeElement> *lb,
                           ID *owner_id,
                           void *create_data,
                           TreeElement *parent,
                           short type,
                           short index,
                           const bool expand = true);

  /**
   * Add a tree element of \a ElementType, forwarding \a args to its constructor. The remaining
   * information the tree needs is queried from \a ElementType itself:
   * - `ElementType::element_type` gives the `TSE_` type to store in the tree-store.
   * - `ElementType::owner_id(args...)` gives the ID identifying the element over rebuilds and file
   *    loads, so that state like opened and selected is preserved.
   * - `ElementType::persistent_ptr(args...)` gives that identity for types that have no owner ID
   *   (see also #TreeElementAddParams.owner_id and #TreeElementAddParams.persistent_ptr, for the
   *   types only the caller can identify).
   *
   * So a call only mentions the element type and its actual data:
   * \code
   * add_element<TreeElementConstraint>({.parent = tenla, .index = index}, object, con);
   * \endcode
   *
   * \note If child items are only added to the tree if the item is open, the `TSE_` type _must_ be
   *       added to #outliner_element_needs_rebuild_on_open_change().
   */
  template<typename ElementType, typename... Args>
  TreeElement *add_element(const TreeElementAddParams &params, Args &&...args);

  /**
   * Add an element representing the ID \a id itself (`TSE_SOME_ID`). Unlike the types handled by
   * #add_element() above, the concrete #TreeElementID sub-type to construct is only known at
   * run-time, from the ID's type.
   *
   * \note Tolerates a null \a id, in which case no element is added and null is returned. Callers
   *       commonly pass an optional ID pointer (e.g. an object's data).
   */
  TreeElement *add_id_element(const TreeElementAddParams &params, ID *id);

 protected:
  /**
   * The non-template part of #add_element(), so the tree building logic stays in one place and
   * isn't instantiated per element type. #add_id_element() shares it.
   */
  TreeElement *add_element_impl(
      const TreeElementAddParams &params,
      short type,
      ID *owner_id,
      const void *persistent_ptr,
      bool allow_null_identity,
      FunctionRef<std::unique_ptr<AbstractTreeElement>(TreeElement &)> construct_fn);

  /** All derived classes will need a handle to this, so storing it in the base for convenience. */
  SpaceOutliner &space_outliner_;
};

template<typename ElementType, typename... Args>
TreeElement *AbstractTreeDisplay::add_element(const TreeElementAddParams &params, Args &&...args)
{
  ID *owner_id = detail::element_owner_id<ElementType>(args...);
  if (owner_id) {
    BLI_assert_msg(params.owner_id == nullptr,
                   "Element type derives its owner ID from its own construction data, passing one "
                   "explicitly would have no effect");
  }
  else {
    owner_id = params.owner_id;
  }

  /* Types that represent no data at all (e.g. #TreeElementLabel) have nothing to be identified by
   * and opt out of the identity requirement. */
  constexpr bool allow_null_identity = requires { ElementType::allow_null_identity; };

  return add_element_impl(params,
                          ElementType::element_type,
                          owner_id,
                          detail::element_persistent_ptr<ElementType>(params, args...),
                          allow_null_identity,
                          [&](TreeElement &legacy_te) -> std::unique_ptr<AbstractTreeElement> {
                            return std::make_unique<ElementType>(legacy_te,
                                                                 std::forward<Args>(args)...);
                          });
}

/* -------------------------------------------------------------------- */
/* View Layer Tree-Display */

/**
 * \brief Tree-Display for the View Layer display mode.
 */
class TreeDisplayViewLayer final : public AbstractTreeDisplay {
  const Main *bmain_ = nullptr;
  Scene *scene_ = nullptr;
  ViewLayer *view_layer_ = nullptr;
  bool show_objects_ = true;

 public:
  TreeDisplayViewLayer(SpaceOutliner &space_outliner);

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

  bool supports_mode_column() const override;

 private:
  void add_view_layer(Scene &, ListBaseT<TreeElement> &, TreeElement *);
  void add_layer_collections_recursive(ListBaseT<TreeElement> &,
                                       ListBaseT<LayerCollection> &,
                                       TreeElement &);
  void add_layer_collection_objects(ListBaseT<TreeElement> &, LayerCollection &, TreeElement &);
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

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

 private:
  TreeElement *add_library_contents(Main &, ListBaseT<TreeElement> &, Library *);
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

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

 private:
  ListBaseT<TreeElement> add_library_contents(Main &);
  short id_filter_get() const;
};

/**
 * \brief Tree-Display for the Library Overrides display mode, Hierarchies view mode.
 */
class TreeDisplayOverrideLibraryHierarchies final : public AbstractTreeDisplay {
 public:
  TreeDisplayOverrideLibraryHierarchies(SpaceOutliner &space_outliner);

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

  bool is_lazy_built() const override;

 private:
  ListBaseT<TreeElement> build_hierarchy_for_lib_or_main(Main *bmain,
                                                         TreeElement &parent_te,
                                                         Library *lib = nullptr);
};

/* -------------------------------------------------------------------- */
/* Video Sequencer Tree-Display */

enum class StripAddOp : int8_t { Noop = 0, Add, None };

/**
 * \brief Tree-Display for the Video Sequencer display mode
 */
class TreeDisplaySequencer final : public AbstractTreeDisplay {
 public:
  TreeDisplaySequencer(SpaceOutliner &space_outliner);

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

 private:
  TreeElement *add_sequencer_contents() const;
  /**
   * Helped function to put duplicate sequence in the same tree.
   */
  StripAddOp need_add_strip_dup(Strip *strip) const;
  void add_strip_dup(Strip *strip, TreeElement *te, short index);
};

/* -------------------------------------------------------------------- */
/* Orphaned Data Tree-Display */

/**
 * \brief Tree-Display for the Orphaned Data display mode
 */
class TreeDisplayIDOrphans final : public AbstractTreeDisplay {
 public:
  TreeDisplayIDOrphans(SpaceOutliner &space_outliner);

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

 private:
  bool datablock_has_orphans(ListBaseT<ID> &) const;
};

/* -------------------------------------------------------------------- */
/* Scenes Tree-Display */

/**
 * \brief Tree-Display for the Scenes display mode
 */
class TreeDisplayScenes final : public AbstractTreeDisplay {
 public:
  TreeDisplayScenes(SpaceOutliner &space_outliner);

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

  bool supports_mode_column() const override;
};

/* -------------------------------------------------------------------- */
/* Data API Tree-Display */

/**
 * \brief Tree-Display for the Scenes display mode
 */
class TreeDisplayDataAPI final : public AbstractTreeDisplay {
 public:
  TreeDisplayDataAPI(SpaceOutliner &space_outliner);

  ListBaseT<TreeElement> build_tree(const TreeSourceData &source_data) override;

  bool is_lazy_built() const override;
};

/* -------------------------------------------------------------------- */
/* Tree-Element Creation (needs the complete #AbstractTreeDisplay above) */

template<typename ElementType, typename... Args>
TreeElement *AbstractTreeElement::add_element(const TreeElementAddParams &params,
                                              Args &&...args) const
{
  TreeElementAddParams resolved_params = params;
  AbstractTreeDisplay *display = display_for_adding(resolved_params);
  if (!display) {
    return nullptr;
  }
  return display->add_element<ElementType>(resolved_params, std::forward<Args>(args)...);
}

}  // namespace ed::outliner
}  // namespace blender

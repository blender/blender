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
 * \ingroup editorui
 *
 * API for simple creation of tree UIs supporting typically needed features.
 * https://wiki.blender.org/wiki/Source/Interface/Views/Tree_Views
 */

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>

#include "DNA_defs.h"

#include "BLI_function_ref.hh"
#include "BLI_vector.hh"

#include "UI_resources.h"

struct bContext;
struct uiBlock;
struct uiBut;
struct uiButTreeRow;
struct uiLayout;
struct wmDrag;
struct wmEvent;

namespace blender::ui {

class AbstractTreeView;
class AbstractTreeViewItem;
class AbstractTreeViewItemDropController;
class AbstractTreeViewItemDragController;

/* ---------------------------------------------------------------------- */
/** \name Tree-View Item Container
 *
 *  Base class for tree-view and tree-view items, so both can contain children.
 * \{ */

/**
 * Both the tree-view (as the root of the tree) and the items can have children. This is the base
 * class for both, to store and manage child items. Children are owned by their parent container
 * (tree-view or item).
 *
 * That means this type can be used whenever either an #AbstractTreeView or an
 * #AbstractTreeViewItem is needed, but the #TreeViewOrItem alias is a better name to use then.
 */
class TreeViewItemContainer {
  friend class AbstractTreeView;
  friend class AbstractTreeViewItem;

  /* Private constructor, so only the friends above can create this! */
  TreeViewItemContainer() = default;

 protected:
  Vector<std::unique_ptr<AbstractTreeViewItem>> children_;
  /** Adding the first item to the root will set this, then it's passed on to all children. */
  TreeViewItemContainer *root_ = nullptr;
  /** Pointer back to the owning item. */
  AbstractTreeViewItem *parent_ = nullptr;

 public:
  enum class IterOptions {
    None = 0,
    SkipCollapsed = 1 << 0,

    /* Keep ENUM_OPERATORS() below updated! */
  };
  using ItemIterFn = FunctionRef<void(AbstractTreeViewItem &)>;

  /**
   * Convenience wrapper constructing the item by forwarding given arguments to the constructor of
   * the type (\a ItemT).
   *
   * E.g. if your tree-item type has the following constructor:
   * \code{.cpp}
   * MyTreeItem(std::string str, int i);
   * \endcode
   * You can add an item like this:
   * \code
   * add_tree_item<MyTreeItem>("blabla", 42);
   * \endcode
   */
  template<class ItemT, typename... Args> inline ItemT &add_tree_item(Args &&...args);
  /**
   * Add an already constructed tree item to this parent. Ownership is moved to it.
   * All tree items must be added through this, it handles important invariants!
   */
  AbstractTreeViewItem &add_tree_item(std::unique_ptr<AbstractTreeViewItem> item);

 protected:
  void foreach_item_recursive(ItemIterFn iter_fn, IterOptions options = IterOptions::None) const;
};

ENUM_OPERATORS(TreeViewItemContainer::IterOptions,
               TreeViewItemContainer::IterOptions::SkipCollapsed);

/** The container class is the base for both the tree-view and the items. This alias gives it a
 * clearer name for handles that accept both. Use whenever something wants to act on child-items,
 * irrespective of if they are stored at root level or as children of some other item. */
using TreeViewOrItem = TreeViewItemContainer;

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Tree-View Base Class
 * \{ */

class AbstractTreeView : public TreeViewItemContainer {
  friend class AbstractTreeViewItem;
  friend class TreeViewBuilder;

  /**
   * Only one item can be renamed at a time. So the tree is informed about the renaming state to
   * enforce that.
   */
  std::unique_ptr<std::array<char, MAX_NAME>> rename_buffer_;

  bool is_reconstructed_ = false;

 public:
  virtual ~AbstractTreeView() = default;

  void foreach_item(ItemIterFn iter_fn, IterOptions options = IterOptions::None) const;

  /** Only one item can be renamed at a time. */
  bool is_renaming() const;

 protected:
  virtual void build_tree() = 0;

  /**
   * Check if the tree is fully (re-)constructed. That means, both #build_tree() and
   * #update_from_old() have finished.
   */
  bool is_reconstructed() const;

 private:
  /**
   * Match the tree-view against an earlier version of itself (if any) and copy the old UI state
   * (e.g. collapsed, active, selected, renaming, etc.) to the new one. See
   * #AbstractTreeViewItem.update_from_old().
   */
  void update_from_old(uiBlock &new_block);
  static void update_children_from_old_recursive(const TreeViewOrItem &new_items,
                                                 const TreeViewOrItem &old_items);
  static AbstractTreeViewItem *find_matching_child(const AbstractTreeViewItem &lookup_item,
                                                   const TreeViewOrItem &items);

  /**
   * Items may want to do additional work when state changes. But these state changes can only be
   * reliably detected after the tree has completed reconstruction (see #is_reconstructed()). So
   * the actual state changes are done in a delayed manner through this function.
   */
  void change_state_delayed();
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Tree-View Item Type
 * \{ */

/** \brief Abstract base class for defining a customizable tree-view item.
 *
 * The tree-view item defines how to build its data into a tree-row. There are implementations for
 * common layouts, e.g. #BasicTreeViewItem.
 * It also stores state information that needs to be persistent over redraws, like the collapsed
 * state.
 */
class AbstractTreeViewItem : public TreeViewItemContainer {
  friend class AbstractTreeView;
  friend class TreeViewLayoutBuilder;
  /* Higher-level API. */
  friend class TreeViewItemAPIWrapper;

 private:
  bool is_open_ = false;
  bool is_active_ = false;
  bool is_renaming_ = false;

 protected:
  /** This label is used for identifying an item within its parent. */
  std::string label_{};
  /** Every visible item gets a button of type #UI_BTYPE_TREEROW during the layout building. */
  uiButTreeRow *tree_row_but_ = nullptr;

 public:
  virtual ~AbstractTreeViewItem() = default;

  virtual void build_row(uiLayout &row) = 0;
  virtual void build_context_menu(bContext &C, uiLayout &column) const;

  AbstractTreeView &get_tree_view() const;

  void begin_renaming();
  void toggle_collapsed();
  void set_collapsed(bool collapsed);
  /**
   * Requires the tree to have completed reconstruction, see #is_reconstructed(). Otherwise we
   * can't be sure about the item state.
   */
  bool is_collapsed() const;
  /**
   * Requires the tree to have completed reconstruction, see #is_reconstructed(). Otherwise we
   * can't be sure about the item state.
   */
  bool is_active() const;

 protected:
  /**
   * Called when the items state changes from inactive to active.
   */
  virtual void on_activate();
  /**
   * If the result is not empty, it controls whether the item should be active or not,
   * usually depending on the data that the view represents.
   */
  virtual std::optional<bool> should_be_active() const;

  /**
   * Queries if the tree-view item supports renaming in principle. Renaming may still fail, e.g. if
   * another item is already being renamed.
   */
  virtual bool supports_renaming() const;
  /**
   * Try renaming the item, or the data it represents. Can assume
   * #AbstractTreeViewItem::supports_renaming() returned true. Sub-classes that override this
   * should usually call this, unless they have a custom #AbstractTreeViewItem.matches().
   *
   * \return True if the renaming was successful.
   */
  virtual bool rename(StringRefNull new_name);

  /**
   * Return whether the item can be collapsed. Used to disable collapsing for items with children.
   */
  virtual bool supports_collapsing() const;

  /**
   * Copy persistent state (e.g. is-collapsed flag, selection, etc.) from a matching item of
   * the last redraw to this item. If sub-classes introduce more advanced state they should
   * override this and make it update their state accordingly.
   */
  virtual void update_from_old(const AbstractTreeViewItem &old);

  /**
   * Compare this item to \a other to check if they represent the same data.
   * Used to recognize an item from a previous redraw, to be able to keep its state (e.g.
   * open/closed, active, etc.). Items are only matched if their parents also match.
   * By default this just matches the item's label (if the parents match!). If that isn't
   * good enough for a sub-class, that can override it.
   */
  virtual bool matches(const AbstractTreeViewItem &other) const;

  /**
   * If an item wants to support being dragged, it has to return a drag controller here.
   * That is an object implementing #AbstractTreeViewItemDragController.
   */
  virtual std::unique_ptr<AbstractTreeViewItemDragController> create_drag_controller() const;
  /**
   * If an item wants to support dropping data into it, it has to return a drop controller here.
   * That is an object implementing #AbstractTreeViewItemDropController.
   *
   * \note This drop controller may be requested for each event. The tree-view doesn't keep a drop
   *       controller around currently. So it can not contain persistent state.
   */
  virtual std::unique_ptr<AbstractTreeViewItemDropController> create_drop_controller() const;

  /**
   * Activates this item, deactivates other items, calls the #AbstractTreeViewItem::on_activate()
   * function and ensures this item's parents are not collapsed (so the item is visible).
   * Requires the tree to have completed reconstruction, see #is_reconstructed(). Otherwise the
   * actual item state is unknown, possibly calling state-change update functions incorrectly.
   */
  void activate();
  void deactivate();

  /**
   * Can be called from the #AbstractTreeViewItem::build_row() implementation, but not earlier. The
   * hovered state can't be queried reliably otherwise.
   * Note that this does a linear lookup in the old block, so isn't too great performance-wise.
   */
  bool is_hovered() const;
  bool is_collapsible() const;
  bool is_renaming() const;

  void ensure_parents_uncollapsed();

  uiButTreeRow *tree_row_button();

 private:
  static void rename_button_fn(bContext *, void *, char *);
  static AbstractTreeViewItem *find_tree_item_from_rename_button(const uiBut &but);
  static void tree_row_click_fn(struct bContext *, void *, void *);
  static void collapse_chevron_click_fn(bContext *, void *but_arg1, void *);
  static bool is_collapse_chevron_but(const uiBut *but);

  /** See #AbstractTreeView::change_state_delayed() */
  void change_state_delayed();
  void end_renaming();

  void add_treerow_button(uiBlock &block);
  void add_indent(uiLayout &row) const;
  void add_collapse_chevron(uiBlock &block) const;
  void add_rename_button(uiLayout &row);

  bool matches_including_parents(const AbstractTreeViewItem &other) const;
  bool has_active_child() const;
  int count_parents() const;
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Drag 'n Drop
 * \{ */

/**
 * Class to enable dragging a tree-item. An item can return a drop controller for itself via a
 * custom implementation of #AbstractTreeViewItem::create_drag_controller().
 */
class AbstractTreeViewItemDragController {
 protected:
  AbstractTreeView &tree_view_;

 public:
  AbstractTreeViewItemDragController(AbstractTreeView &tree_view);
  virtual ~AbstractTreeViewItemDragController() = default;

  virtual int get_drag_type() const = 0;
  virtual void *create_drag_data() const = 0;
  virtual void on_drag_start();

  template<class TreeViewType> inline TreeViewType &tree_view() const;
};

/**
 * Class to customize the drop behavior of a tree-item, plus the behavior when dragging over this
 * item. An item can return a drop controller for itself via a custom implementation of
 * #AbstractTreeViewItem::create_drop_controller().
 */
class AbstractTreeViewItemDropController {
 protected:
  AbstractTreeView &tree_view_;

 public:
  AbstractTreeViewItemDropController(AbstractTreeView &tree_view);
  virtual ~AbstractTreeViewItemDropController() = default;

  /**
   * Check if the data dragged with \a drag can be dropped on the item this controller is for.
   * \param r_disabled_hint: Return a static string to display to the user, explaining why dropping
   *                         isn't possible on this item. Shouldn't be done too aggressively, e.g.
   *                         don't set this if the drag-type can't be dropped here; only if it can
   *                         but there's another reason it can't be dropped.
   *                         Can assume this is a non-null pointer.
   */
  virtual bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const = 0;
  /**
   * Custom text to display when dragging over a tree item. Should explain what happens when
   * dropping the data onto this item. Will only be used if #AbstractTreeViewItem::can_drop()
   * returns true, so the implementing override doesn't have to check that again.
   * The returned value must be a translated string.
   */
  virtual std::string drop_tooltip(const wmDrag &drag) const = 0;
  /**
   * Execute the logic to apply a drop of the data dragged with \a drag onto/into the item this
   * controller is for.
   */
  virtual bool on_drop(struct bContext *C, const wmDrag &drag) = 0;

  template<class TreeViewType> inline TreeViewType &tree_view() const;
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Predefined Tree-View Item Types
 *
 *  Common, Basic Tree-View Item Types.
 * \{ */

/**
 * The most basic type, just a label with an icon.
 */
class BasicTreeViewItem : public AbstractTreeViewItem {
 public:
  using IsActiveFn = std::function<bool()>;
  using ActivateFn = std::function<void(BasicTreeViewItem &new_active)>;
  BIFIconID icon;

  explicit BasicTreeViewItem(StringRef label, BIFIconID icon = ICON_NONE);

  void build_row(uiLayout &row) override;
  void add_label(uiLayout &layout, StringRefNull label_override = "");
  void set_on_activate_fn(ActivateFn fn);
  /**
   * Set a custom callback to check if this item should be active.
   */
  void set_is_active_fn(IsActiveFn fn);

 protected:
  /**
   * Optionally passed to the #BasicTreeViewItem constructor. Called when activating this tree
   * view item. This way users don't have to sub-class #BasicTreeViewItem, just to implement
   * custom activation behavior (a common thing to do).
   */
  ActivateFn activate_fn_;

  IsActiveFn is_active_fn_;

 private:
  static void tree_row_click_fn(struct bContext *C, void *arg1, void *arg2);

  std::optional<bool> should_be_active() const override;
  void on_activate() override;
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Tree-View Builder
 * \{ */

class TreeViewBuilder {
  uiBlock &block_;

 public:
  TreeViewBuilder(uiBlock &block);

  void build_tree_view(AbstractTreeView &tree_view);
};

/** \} */

/* ---------------------------------------------------------------------- */

template<class ItemT, typename... Args>
inline ItemT &TreeViewItemContainer::add_tree_item(Args &&...args)
{
  static_assert(std::is_base_of<AbstractTreeViewItem, ItemT>::value,
                "Type must derive from and implement the AbstractTreeViewItem interface");

  return dynamic_cast<ItemT &>(
      add_tree_item(std::make_unique<ItemT>(std::forward<Args>(args)...)));
}

template<class TreeViewType> TreeViewType &AbstractTreeViewItemDragController::tree_view() const
{
  static_assert(std::is_base_of<AbstractTreeView, TreeViewType>::value,
                "Type must derive from and implement the AbstractTreeView interface");
  return static_cast<TreeViewType &>(tree_view_);
}

template<class TreeViewType> TreeViewType &AbstractTreeViewItemDropController::tree_view() const
{
  static_assert(std::is_base_of<AbstractTreeView, TreeViewType>::value,
                "Type must derive from and implement the AbstractTreeView interface");
  return static_cast<TreeViewType &>(tree_view_);
}

}  // namespace blender::ui

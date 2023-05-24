/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 *
 * API for simple creation of tree UIs supporting typically needed features.
 * https://wiki.blender.org/wiki/Source/Interface/Views/Tree_Views
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include "DNA_defs.h"

#include "BLI_function_ref.hh"
#include "BLI_vector.hh"

#include "UI_abstract_view.hh"
#include "UI_resources.h"

struct bContext;
struct uiBlock;
struct uiBut;
struct uiButViewItem;
struct uiLayout;

namespace blender::ui {

class AbstractTreeView;
class AbstractTreeViewItem;

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
    SkipFiltered = 1 << 1,

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

class AbstractTreeView : public AbstractView, public TreeViewItemContainer {
  int min_rows_ = 0;

  friend class AbstractTreeViewItem;
  friend class TreeViewBuilder;

 public:
  virtual ~AbstractTreeView() = default;

  void foreach_item(ItemIterFn iter_fn, IterOptions options = IterOptions::None) const;

  /** Visual feature: Define a number of item rows the view will always show at minimum. If there
   * are fewer items, empty dummy items will be added. These contribute to the view bounds, so the
   * drop target of the view includes them, but they are not interactive (e.g. no mouse-hover
   * highlight). */
  void set_min_rows(int min_rows);

 protected:
  virtual void build_tree() = 0;

 private:
  void update_children_from_old(const AbstractView &old_view) override;
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
class AbstractTreeViewItem : public AbstractViewItem, public TreeViewItemContainer {
  friend class AbstractTreeView;
  friend class TreeViewLayoutBuilder;
  /* Higher-level API. */
  friend class TreeViewItemAPIWrapper;

 private:
  bool is_open_ = false;

 protected:
  /** This label is used as the default way to identifying an item within its parent. */
  std::string label_{};
  /** Every visible item gets a button of type #UI_BTYPE_VIEW_ITEM during the layout building. */
  uiButViewItem *view_item_but_ = nullptr;

 public:
  virtual ~AbstractTreeViewItem() = default;

  virtual void build_row(uiLayout &row) = 0;

  AbstractTreeView &get_tree_view() const;

  void begin_renaming();
  void toggle_collapsed();
  void set_collapsed(bool collapsed);
  /**
   * Requires the tree to have completed reconstruction, see #is_reconstructed(). Otherwise we
   * can't be sure about the item state.
   */
  bool is_collapsed() const;

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

  /** See AbstractViewItem::get_rename_string(). */
  virtual StringRef get_rename_string() const override;
  /** See AbstractViewItem::rename(). */
  virtual bool rename(StringRefNull new_name) override;

  /**
   * Return whether the item can be collapsed. Used to disable collapsing for items with children.
   */
  virtual bool supports_collapsing() const;

  /** See #AbstractViewItem::matches(). */
  virtual bool matches(const AbstractViewItem &other) const override;

  /** See #AbstractViewItem::update_from_old(). */
  virtual void update_from_old(const AbstractViewItem &old) override;

  /**
   * Compare this item to \a other to check if they represent the same data.
   * Used to recognize an item from a previous redraw, to be able to keep its state (e.g.
   * open/closed, active, etc.). Items are only matched if their parents also match.
   * By default this just matches the item's label (if the parents match!). If that isn't
   * good enough for a sub-class, that can override it.
   *
   * TODO #matches_single() is a rather temporary name, used to indicate that this only compares
   * the item itself, not the parents. Item matching is expected to change quite a bit anyway.
   */
  virtual bool matches_single(const AbstractTreeViewItem &other) const;

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

  void ensure_parents_uncollapsed();

  uiButViewItem *view_item_button();

 private:
  static void tree_row_click_fn(struct bContext *, void *, void *);
  static void collapse_chevron_click_fn(bContext *, void *but_arg1, void *);
  static bool is_collapse_chevron_but(const uiBut *but);

  /** See #AbstractTreeView::change_state_delayed() */
  void change_state_delayed();

  void add_treerow_button(uiBlock &block);
  void add_indent(uiLayout &row) const;
  void add_collapse_chevron(uiBlock &block) const;
  void add_rename_button(uiLayout &row);

  bool has_active_child() const;
  int count_parents() const;
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
 public:
  static void build_tree_view(AbstractTreeView &tree_view, uiLayout &layout);

 private:
  static void ensure_min_rows_items(AbstractTreeView &tree_view);
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

}  // namespace blender::ui

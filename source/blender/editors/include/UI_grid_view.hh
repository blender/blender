/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 *
 * API for simple creation of grid UIs, supporting typically needed features.
 * https://wiki.blender.org/wiki/Source/Interface/Views/Grid_Views
 */

#pragma once

#include "BLI_function_ref.hh"
#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "UI_resources.h"

struct bContext;
struct PreviewImage;
struct uiBlock;
struct uiButGridTile;
struct uiLayout;
struct View2D;
struct wmNotifier;

namespace blender::ui {

class AbstractGridView;

/* ---------------------------------------------------------------------- */
/** \name Grid-View Item Type
 * \{ */

class AbstractGridViewItem {
  friend class AbstractGridView;
  friend class GridViewLayoutBuilder;

  const AbstractGridView *view_;

  bool is_active_ = false;

 protected:
  /** Reference to a string that uniquely identifies this item in the view. */
  StringRef identifier_{};
  /** Every visible item gets a button of type #UI_BTYPE_GRID_TILE during the layout building. */
  uiButGridTile *grid_tile_but_ = nullptr;

 public:
  virtual ~AbstractGridViewItem() = default;

  virtual void build_grid_tile(uiLayout &layout) const = 0;

  /**
   * Compare this item's identifier to \a other to check if they represent the same data.
   * Used to recognize an item from a previous redraw, to be able to keep its state (e.g. active,
   * renaming, etc.).
   */
  bool matches(const AbstractGridViewItem &other) const;

  const AbstractGridView &get_view() const;

  /**
   * Requires the tree to have completed reconstruction, see #is_reconstructed(). Otherwise we
   * can't be sure about the item state.
   */
  bool is_active() const;

 protected:
  AbstractGridViewItem(StringRef identifier);

  /** Called when the item's state changes from inactive to active. */
  virtual void on_activate();
  /**
   * If the result is not empty, it controls whether the item should be active or not,
   * usually depending on the data that the view represents.
   */
  virtual std::optional<bool> should_be_active() const;

  /**
   * Copy persistent state (e.g. active, selection, etc.) from a matching item of
   * the last redraw to this item. If sub-classes introduce more advanced state they should
   * override this and make it update their state accordingly.
   */
  virtual void update_from_old(const AbstractGridViewItem &old);

  /**
   * Activates this item, deactivates other items, and calls the
   * #AbstractGridViewItem::on_activate() function.
   * Requires the tree to have completed reconstruction, see #is_reconstructed(). Otherwise the
   * actual item state is unknown, possibly calling state-change update functions incorrectly.
   */
  void activate();
  void deactivate();

 private:
  /** See #AbstractTreeView::change_state_delayed() */
  void change_state_delayed();
  static void grid_tile_click_fn(bContext *, void *but_arg1, void *);
  void add_grid_tile_button(uiBlock &block);
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grid-View Base Class
 * \{ */

struct GridViewStyle {
  GridViewStyle(int width, int height);
  int tile_width = 0;
  int tile_height = 0;
};

class AbstractGridView {
  friend class AbstractGridViewItem;
  friend class GridViewBuilder;
  friend class GridViewLayoutBuilder;

 protected:
  Vector<std::unique_ptr<AbstractGridViewItem>> items_;
  /** <identifier, item> map to lookup items by identifier, used for efficient lookups in
   * #update_from_old(). */
  Map<StringRef, AbstractGridViewItem *> item_map_;
  GridViewStyle style_;
  bool is_reconstructed_ = false;

 public:
  AbstractGridView();
  virtual ~AbstractGridView() = default;

  using ItemIterFn = FunctionRef<void(AbstractGridViewItem &)>;
  void foreach_item(ItemIterFn iter_fn) const;

  /** Listen to a notifier, returning true if a redraw is needed. */
  virtual bool listen(const wmNotifier &) const;

  /**
   * Convenience wrapper constructing the item by forwarding given arguments to the constructor of
   * the type (\a ItemT).
   *
   * E.g. if your grid-item type has the following constructor:
   * \code{.cpp}
   * MyGridItem(std::string str, int i);
   * \endcode
   * You can add an item like this:
   * \code
   * add_item<MyGridItem>("blabla", 42);
   * \endcode
   */
  template<class ItemT, typename... Args> inline ItemT &add_item(Args &&...args);
  const GridViewStyle &get_style() const;
  int get_item_count() const;

 protected:
  virtual void build_items() = 0;

  /**
   * Check if the view is fully (re-)constructed. That means, both #build_items() and
   * #update_from_old() have finished.
   */
  bool is_reconstructed() const;

 private:
  /**
   * Match the grid-view against an earlier version of itself (if any) and copy the old UI state
   * (e.g. active, selected, renaming, etc.) to the new one. See
   * #AbstractGridViewItem.update_from_old().
   */
  void update_from_old(uiBlock &new_block);
  AbstractGridViewItem *find_matching_item(const AbstractGridViewItem &item_to_match,
                                           const AbstractGridView &view_to_search_in) const;
  /**
   * Items may want to do additional work when state changes. But these state changes can only be
   * reliably detected after the view has completed reconstruction (see #is_reconstructed()). So
   * the actual state changes are done in a delayed manner through this function.
   */
  void change_state_delayed();

  /**
   * Add an already constructed item, moving ownership to the grid-view.
   * All items must be added through this, it handles important invariants!
   */
  AbstractGridViewItem &add_item(std::unique_ptr<AbstractGridViewItem> item);
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grid-View Builder
 *
 *  TODO unify this with `TreeViewBuilder` and call view-specific functions via type erased view?
 * \{ */

class GridViewBuilder {
  uiBlock &block_;

 public:
  GridViewBuilder(uiBlock &block);

  /** Build \a grid_view into the previously provided block, clipped by \a view_bounds (view space,
   * typically `View2D.cur`). */
  void build_grid_view(AbstractGridView &grid_view, const View2D &v2d);
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Predefined Grid-View Item Types
 *
 *  Common, Basic Grid-View Item Types.
 * \{ */

/**
 * A grid item that shows preview image icons at a nicely readable size (multiple of the normal UI
 * unit size).
 */
class PreviewGridItem : public AbstractGridViewItem {
 public:
  using IsActiveFn = std::function<bool()>;
  using ActivateFn = std::function<void(PreviewGridItem &new_active)>;

 protected:
  /** See #set_on_activate_fn() */
  ActivateFn activate_fn_;
  /** See #set_is_active_fn() */
  IsActiveFn is_active_fn_;

 public:
  std::string label{};
  int preview_icon_id = ICON_NONE;

  PreviewGridItem(StringRef identifier, StringRef label, int preview_icon_id);

  void build_grid_tile(uiLayout &layout) const override;

  /**
   * Set a custom callback to execute when activating this view item. This way users don't have to
   * sub-class #PreviewGridItem, just to implement custom activation behavior (a common thing to
   * do).
   */
  void set_on_activate_fn(ActivateFn fn);
  /**
   * Set a custom callback to check if this item should be active.
   */
  void set_is_active_fn(IsActiveFn fn);

 private:
  std::optional<bool> should_be_active() const override;
  void on_activate() override;
};

/** \} */

/* ---------------------------------------------------------------------- */

template<class ItemT, typename... Args> inline ItemT &AbstractGridView::add_item(Args &&...args)
{
  static_assert(std::is_base_of<AbstractGridViewItem, ItemT>::value,
                "Type must derive from and implement the AbstractGridViewItem interface");

  return dynamic_cast<ItemT &>(add_item(std::make_unique<ItemT>(std::forward<Args>(args)...)));
}

}  // namespace blender::ui

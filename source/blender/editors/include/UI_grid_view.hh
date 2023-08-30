/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include "UI_abstract_view.hh"
#include "UI_resources.hh"

struct bContext;
struct uiBlock;
struct uiButViewItem;
struct uiLayout;
struct View2D;

namespace blender::ui {

class AbstractGridView;
class GridViewItemDropTarget;

/* ---------------------------------------------------------------------- */
/** \name Grid-View Item Type
 * \{ */

class AbstractGridViewItem : public AbstractViewItem {
  friend class AbstractGridView;
  friend class GridViewLayoutBuilder;

 protected:
  /** Reference to a string that uniquely identifies this item in the view. */
  StringRef identifier_{};
  /** Every visible item gets a button of type #UI_BTYPE_VIEW_ITEM during the layout building. */
  uiButViewItem *view_item_but_ = nullptr;

 public:
  /* virtual */ ~AbstractGridViewItem() override = default;

  virtual void build_grid_tile(uiLayout &layout) const = 0;

  AbstractGridView &get_view() const;

 protected:
  AbstractGridViewItem(StringRef identifier);

  /** See AbstractViewItem::matches(). */
  /* virtual */ bool matches(const AbstractViewItem &other) const override;

  /* virtual */ std::unique_ptr<DropTargetInterface> create_item_drop_target() final;
  virtual std::unique_ptr<GridViewItemDropTarget> create_drop_target();

 private:
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

class AbstractGridView : public AbstractView {
  friend class AbstractGridViewItem;
  friend class GridViewBuilder;
  friend class GridViewLayoutBuilder;

 protected:
  Vector<std::unique_ptr<AbstractGridViewItem>> items_;
  /** Store this to avoid recomputing. */
  mutable std::optional<int> item_count_filtered_;
  /** <identifier, item> map to lookup items by identifier, used for efficient lookups in
   * #update_from_old(). */
  Map<StringRef, AbstractGridViewItem *> item_map_;
  GridViewStyle style_;

 public:
  AbstractGridView();
  /* virtual */ ~AbstractGridView() override = default;

  using ItemIterFn = FunctionRef<void(AbstractGridViewItem &)>;
  void foreach_item(ItemIterFn iter_fn) const;
  void foreach_filtered_item(ItemIterFn iter_fn) const;

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
  int get_item_count_filtered() const;

  void set_tile_size(int tile_width, int tile_height);

 protected:
  virtual void build_items() = 0;

 private:
  void foreach_view_item(FunctionRef<void(AbstractViewItem &)> iter_fn) const final;
  void update_children_from_old(const AbstractView &old_view) override;
  AbstractGridViewItem *find_matching_item(const AbstractGridViewItem &item_to_match,
                                           const AbstractGridView &view_to_search_in) const;

  /**
   * Add an already constructed item, moving ownership to the grid-view.
   * All items must be added through this, it handles important invariants!
   */
  AbstractGridViewItem &add_item(std::unique_ptr<AbstractGridViewItem> item);
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Drag & Drop
 * \{ */

/**
 * Class to define the behavior when dropping something onto/into a view item, plus the behavior
 * when dragging over this item. An item can return a drop target for itself via a custom
 * implementation of #AbstractGridViewItem::create_drop_target().
 */
class GridViewItemDropTarget : public DropTargetInterface {
 protected:
  AbstractGridView &view_;

 public:
  GridViewItemDropTarget(AbstractGridView &view);

  /** Request the view the item is registered for as type #ViewType. Throws a `std::bad_cast`
   * exception if the view is not of the requested type. */
  template<class ViewType> inline ViewType &get_view() const;
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Grid-View Builder
 *
 *  TODO unify this with `TreeViewBuilder` and call view-specific functions via type erased view?
 * \{ */

class GridViewBuilder {
 public:
  GridViewBuilder(uiBlock &block);

  /** Build \a grid_view into the previously provided block, clipped by \a view_bounds (view space,
   * typically `View2D.cur`). */
  void build_grid_view(AbstractGridView &grid_view, const View2D &v2d, uiLayout &layout);
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
  using ActivateFn = std::function<void(bContext &C, PreviewGridItem &new_active)>;

 protected:
  /** See #set_on_activate_fn() */
  ActivateFn activate_fn_;
  /** See #set_is_active_fn() */
  IsActiveFn is_active_fn_;
  bool hide_label_ = false;

 public:
  std::string label{};
  int preview_icon_id = ICON_NONE;

  PreviewGridItem(StringRef identifier, StringRef label, int preview_icon_id = ICON_NONE);

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

  void hide_label();

 private:
  std::optional<bool> should_be_active() const override;
  void on_activate(bContext &C) override;
};

/** \} */

/* ---------------------------------------------------------------------- */

template<class ItemT, typename... Args> inline ItemT &AbstractGridView::add_item(Args &&...args)
{
  static_assert(std::is_base_of<AbstractGridViewItem, ItemT>::value,
                "Type must derive from and implement the AbstractGridViewItem interface");

  return dynamic_cast<ItemT &>(add_item(std::make_unique<ItemT>(std::forward<Args>(args)...)));
}

template<class ViewType> ViewType &GridViewItemDropTarget::get_view() const
{
  static_assert(std::is_base_of<AbstractGridView, ViewType>::value,
                "Type must derive from and implement the ui::AbstractGridView interface");
  return dynamic_cast<ViewType &>(view_);
}

}  // namespace blender::ui

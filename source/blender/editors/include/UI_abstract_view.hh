/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 *
 * Base class for all views (UIs to display data sets) and view items, supporting common features.
 * https://wiki.blender.org/wiki/Source/Interface/Views
 *
 * One of the most important responsibilities of the base class is managing reconstruction,
 * enabling state that is persistent over reconstructions/redraws. Other features:
 * - Renaming
 * - Custom context menus
 * - Notifier listening
 * - Drag controllers (dragging view items)
 * - Drop targets (dropping onto/into view items)
 */

#pragma once

#include <array>
#include <memory>
#include <optional>

#include "DNA_defs.h"
#include "DNA_vec_types.h"

#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "UI_interface.hh"

#include "WM_types.h"

struct bContext;
struct uiBlock;
struct uiLayout;
struct uiViewItemHandle;
struct ViewLink;
struct wmDrag;
struct wmNotifier;

namespace blender::ui {

class AbstractViewItem;
class AbstractViewItemDropTarget;
class AbstractViewItemDragController;

/**
 * The view drop target can share logic with the view item drop target for now, so just an alias.
 */
using AbstractViewDropTarget = AbstractViewItemDropTarget;

class AbstractView {
  friend class AbstractViewItem;
  friend struct ::ViewLink;

  bool is_reconstructed_ = false;
  /**
   * Only one item can be renamed at a time. So rather than giving each item an own rename buffer
   * (which just adds unused memory in most cases), have one here that is managed by the view.
   *
   * This fixed-size buffer is needed because that's what the rename button requires. In future we
   * may be able to bind the button to a `std::string` or similar.
   */
  std::unique_ptr<std::array<char, MAX_NAME>> rename_buffer_;

  /* See #get_bounds(). */
  std::optional<rcti> bounds_;

 public:
  virtual ~AbstractView() = default;

  /**
   * If a view wants to support dropping data into it, it has to return a drop target here.
   * That is an object implementing #AbstractViewDropTarget.
   *
   * \note This drop target may be requested for each event. The view doesn't keep the drop target
   *       around currently. So it cannot contain persistent state.
   */
  virtual std::unique_ptr<AbstractViewDropTarget> create_drop_target();

  /** Listen to a notifier, returning true if a redraw is needed. */
  virtual bool listen(const wmNotifier &) const;

  /**
   * Enable filtering. Typically used to enable a filter text button. Triggered on Ctrl+F by
   * default.
   * \return True when filtering was enabled successfully.
   */
  virtual bool begin_filtering(const bContext &C) const;

  /**
   * Makes \a item valid for display in this view. Behavior is undefined for items not registered
   * with this.
   */
  void register_item(AbstractViewItem &item);

  /** Only one item can be renamed at a time. */
  bool is_renaming() const;
  /** \return If renaming was started successfully. */
  bool begin_renaming();
  void end_renaming();
  Span<char> get_rename_buffer() const;
  MutableSpan<char> get_rename_buffer();
  /**
   * Get the rectangle containing all the view items that are in the layout, in button space.
   * Updated as part of #UI_block_end(), before that it's unset.
   */
  std::optional<rcti> get_bounds() const;

 protected:
  AbstractView() = default;

  virtual void update_children_from_old(const AbstractView &old_view) = 0;

  /**
   * Match the view and its items against an earlier version of itself (if any) and copy the old UI
   * state (e.g. collapsed, active, selected, renaming, etc.) to the new one. See
   * #AbstractViewItem.update_from_old().
   * After this, reconstruction is complete (see #is_reconstructed()).
   */
  void update_from_old(uiBlock &new_block);
  /**
   * Check if the view is fully (re-)constructed. That means, both the build function and
   * #update_from_old() have finished.
   */
  bool is_reconstructed() const;
};

class AbstractViewItem {
  friend class AbstractView;
  friend class ViewItemAPIWrapper;

 protected:
  /**
   * The view this item is a part of, and was registered for using #AbstractView::register_item().
   * If this wasn't done, the behavior of items is undefined.
   */
  AbstractView *view_ = nullptr;
  bool is_activatable_ = true;
  bool is_interactive_ = true;
  bool is_active_ = false;
  bool is_renaming_ = false;

  /** Cache filtered state here to avoid having to re-query. */
  mutable std::optional<bool> is_filtered_visible_;

 public:
  virtual ~AbstractViewItem() = default;

  virtual void build_context_menu(bContext &C, uiLayout &column) const;

  /**
   * Queries if the view item supports renaming in principle. Renaming may still fail, e.g. if
   * another item is already being renamed.
   */
  virtual bool supports_renaming() const;
  /**
   * Try renaming the item, or the data it represents. Can assume
   * #AbstractViewItem::supports_renaming() returned true. Sub-classes that override this should
   * usually call this, unless they have a custom #AbstractViewItem.matches() implementation.
   *
   * \return True if the renaming was successful.
   */
  virtual bool rename(StringRefNull new_name);
  /**
   * Get the string that should be used for renaming, typically the item's label. This string will
   * not be modified, but if the renaming is canceled, the value will be reset to this.
   */
  virtual StringRef get_rename_string() const;

  /**
   * If an item wants to support being dragged, it has to return a drag controller here.
   * That is an object implementing #AbstractViewItemDragController.
   */
  virtual std::unique_ptr<AbstractViewItemDragController> create_drag_controller() const;
  /**
   * If an item wants to support dropping data into it, it has to return a drop target here.
   * That is an object implementing #AbstractViewItemDropTarget.
   *
   * \note This drop target may be requested for each event. The view doesn't keep a drop target
   *       around currently. So it can not contain persistent state.
   */
  virtual std::unique_ptr<AbstractViewItemDropTarget> create_drop_target();

  /** Return the result of #is_filtered_visible(), but ensure the result is cached so it's only
   * queried once per redraw. */
  bool is_filtered_visible_cached() const;

  /** Get the view this item is registered for using #AbstractView::register_item(). */
  AbstractView &get_view() const;

  /** Disable the interacting with this item, meaning the buttons drawn will be disabled and there
   * will be no mouse hover feedback for the view row. */
  void disable_interaction();
  bool is_interactive() const;

  void disable_activatable();

  /**
   * Requires the view to have completed reconstruction, see #is_reconstructed(). Otherwise we
   * can't be sure about the item state.
   */
  bool is_active() const;

  bool is_renaming() const;
  void begin_renaming();
  void end_renaming();
  void rename_apply();

  template<typename ToType = AbstractViewItem>
  static ToType *from_item_handle(uiViewItemHandle *handle);

 protected:
  AbstractViewItem() = default;

  /**
   * Compare this item's identity to \a other to check if they represent the same data.
   * Implementations can assume that the types match already (caller must check).
   *
   * Used to recognize an item from a previous redraw, to be able to keep its state (e.g. active,
   * renaming, etc.).
   */
  virtual bool matches(const AbstractViewItem &other) const = 0;

  /**
   * Copy persistent state (e.g. active, selection, etc.) from a matching item of
   * the last redraw to this item. If sub-classes introduce more advanced state they should
   * override this and make it update their state accordingly.
   *
   * \note Always call the base class implementation when overriding this!
   */
  virtual void update_from_old(const AbstractViewItem &old);

  /**
   * \note Do not call this directly to avoid constantly rechecking the filter state. Instead use
   *       #is_filtered_visible_cached() for querying.
   */
  virtual bool is_filtered_visible() const;

  /**
   * Add a text button for renaming the item to \a block. This must be used for the built-in
   * renaming to work. This button is meant to appear temporarily. It is removed when renaming is
   * done.
   */
  void add_rename_button(uiBlock &block);
};

template<typename ToType> ToType *AbstractViewItem::from_item_handle(uiViewItemHandle *handle)
{
  static_assert(std::is_base_of<AbstractViewItem, ToType>::value,
                "Type must derive from and implement the AbstractViewItem interface");

  return dynamic_cast<ToType *>(reinterpret_cast<AbstractViewItem *>(handle));
}

/* ---------------------------------------------------------------------- */
/** \name Drag 'n Drop
 * \{ */

/**
 * Class to enable dragging a view item. An item can return a drag controller for itself by
 * implementing #AbstractViewItem::create_drag_controller().
 */
class AbstractViewItemDragController {
 protected:
  AbstractView &view_;

 public:
  AbstractViewItemDragController(AbstractView &view);
  virtual ~AbstractViewItemDragController() = default;

  virtual eWM_DragDataType get_drag_type() const = 0;
  virtual void *create_drag_data(bContext &C) const = 0;
  virtual void on_drag_start();

  /** Request the view the item is registered for as type #ViewType. Throws a `std::bad_cast`
   * exception if the view is not of the requested type. */
  template<class ViewType> inline ViewType &get_view() const;
};

/**
 * Class to define the behavior when dropping something onto/into a view item, plus the behavior
 * when dragging over this item. An item can return a drop target for itself via a custom
 * implementation of #AbstractViewItem::create_drop_target().
 */
class AbstractViewItemDropTarget : public DropTargetInterface {
 protected:
  AbstractView &view_;

 public:
  AbstractViewItemDropTarget(AbstractView &view);

  /** Request the view the item is registered for as type #ViewType. Throws a `std::bad_cast`
   * exception if the view is not of the requested type. */
  template<class ViewType> inline ViewType &get_view() const;
};

template<class ViewType> ViewType &AbstractViewItemDragController::get_view() const
{
  static_assert(std::is_base_of<AbstractView, ViewType>::value,
                "Type must derive from and implement the ui::AbstractView interface");
  return dynamic_cast<ViewType &>(view_);
}

template<class ViewType> ViewType &AbstractViewItemDropTarget::get_view() const
{
  static_assert(std::is_base_of<AbstractView, ViewType>::value,
                "Type must derive from and implement the ui::AbstractView interface");
  return dynamic_cast<ViewType &>(view_);
}

/** \} */

}  // namespace blender::ui

/* SPDX-License-Identifier: GPL-2.0-or-later */

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
 * - Drop controllers (dropping onto/into view items)
 */

#pragma once

#include <array>
#include <memory>

#include "DNA_defs.h"

#include "BLI_span.hh"
#include "BLI_string_ref.hh"

struct bContext;
struct uiBlock;
struct uiLayout;
struct uiViewItemHandle;
struct wmDrag;
struct wmNotifier;

namespace blender::ui {

class AbstractViewItem;
class AbstractViewItemDropController;
class AbstractViewItemDragController;

class AbstractView {
  friend class AbstractViewItem;

  bool is_reconstructed_ = false;
  /**
   * Only one item can be renamed at a time. So rather than giving each item an own rename buffer
   * (which just adds unused memory in most cases), have one here that is managed by the view.
   *
   * This fixed-size buffer is needed because that's what the rename button requires. In future we
   * may be able to bind the button to a `std::string` or similar.
   */
  std::unique_ptr<std::array<char, MAX_NAME>> rename_buffer_;

 public:
  virtual ~AbstractView() = default;

  /** Listen to a notifier, returning true if a redraw is needed. */
  virtual bool listen(const wmNotifier &) const;

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
  bool is_active_ = false;
  bool is_renaming_ = false;

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
   * If an item wants to support dropping data into it, it has to return a drop controller here.
   * That is an object implementing #AbstractViewItemDropController.
   *
   * \note This drop controller may be requested for each event. The view doesn't keep a drop
   *       controller around currently. So it can not contain persistent state.
   */
  virtual std::unique_ptr<AbstractViewItemDropController> create_drop_controller() const;

  /** Get the view this item is registered for using #AbstractView::register_item(). */
  AbstractView &get_view() const;

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
 * Class to enable dragging a view item. An item can return a drop controller for itself by
 * implementing #AbstractViewItem::create_drag_controller().
 */
class AbstractViewItemDragController {
 protected:
  AbstractView &view_;

 public:
  AbstractViewItemDragController(AbstractView &view);
  virtual ~AbstractViewItemDragController() = default;

  virtual int get_drag_type() const = 0;
  virtual void *create_drag_data() const = 0;
  virtual void on_drag_start();

  /** Request the view the item is registered for as type #ViewType. Throws a `std::bad_cast`
   * exception if the view is not of the requested type. */
  template<class ViewType> inline ViewType &get_view() const;
};

/**
 * Class to define the behavior when dropping something onto/into a view item, plus the behavior
 * when dragging over this item. An item can return a drop controller for itself via a custom
 * implementation of #AbstractViewItem::create_drop_controller().
 */
class AbstractViewItemDropController {
 protected:
  AbstractView &view_;

 public:
  AbstractViewItemDropController(AbstractView &view);
  virtual ~AbstractViewItemDropController() = default;

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
   * Custom text to display when dragging over a view item. Should explain what happens when
   * dropping the data onto this item. Will only be used if #AbstractViewItem::can_drop()
   * returns true, so the implementing override doesn't have to check that again.
   * The returned value must be a translated string.
   */
  virtual std::string drop_tooltip(const wmDrag &drag) const = 0;
  /**
   * Execute the logic to apply a drop of the data dragged with \a drag onto/into the item this
   * controller is for.
   */
  virtual bool on_drop(struct bContext *C, const wmDrag &drag) = 0;

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

template<class ViewType> ViewType &AbstractViewItemDropController::get_view() const
{
  static_assert(std::is_base_of<AbstractView, ViewType>::value,
                "Type must derive from and implement the ui::AbstractView interface");
  return dynamic_cast<ViewType &>(view_);
}

/** \} */

}  // namespace blender::ui

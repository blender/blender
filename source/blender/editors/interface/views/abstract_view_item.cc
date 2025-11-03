/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BKE_context.hh"

#include "BLI_fnmatch.h"
#include "BLI_listbase.h"

#include "WM_api.hh"

#include "UI_interface_layout.hh"
#include "interface_intern.hh"

#include "UI_abstract_view.hh"

#include <stdexcept>

namespace blender::ui {

/* ---------------------------------------------------------------------- */
/** \name View Reconstruction
 * \{ */

void AbstractViewItem::update_from_old(const AbstractViewItem &old)
{
  is_active_ = old.is_active_;
  is_renaming_ = old.is_renaming_;
  is_highlighted_search_ = old.is_highlighted_search_;
  is_selected_ = old.is_selected_;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Active Item State
 * \{ */

void AbstractViewItem::on_activate(bContext & /*C*/)
{
  /* Do nothing by default. */
}

std::optional<bool> AbstractViewItem::should_be_active() const
{
  return std::nullopt;
}

bool AbstractViewItem::set_state_active()
{
  BLI_assert_msg(get_view().is_reconstructed(),
                 "Item activation cannot be done until reconstruction is completed");

  if (!is_activatable_) {
    return false;
  }
  if (is_active()) {
    return false;
  }

  /* Deactivate other items in the view. */
  this->get_view().foreach_view_item([](auto &item) { item.deactivate(); });

  is_active_ = true;
  return true;
}

void AbstractViewItem::activate(bContext &C)
{
  if (set_state_active() || reactivate_on_click_) {
    on_activate(C);
  }

  /* Make sure active item is selected. */
  if (is_active()) {
    set_selected(true);
  }
}

void AbstractViewItem::activate_for_context_menu(bContext &C)
{
  if (activate_for_context_menu_) {
    this->activate(C);
  }
  else {
    this->set_state_active();
  }
}

void AbstractViewItem::deactivate()
{
  is_active_ = false;
  is_selected_ = false;
}

std::optional<bool> AbstractViewItem::should_be_selected() const
{
  return std::nullopt;
}

void AbstractViewItem::set_selected(const bool select)
{
  is_selected_ = select;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name General State Management
 * \{ */

void AbstractViewItem::change_state_delayed()
{
  if (const std::optional<bool> should_be_active = this->should_be_active()) {
    if (*should_be_active) {
      /* Don't call #activate() here, since this reflects an external state change and therefore
       * shouldn't call #on_activate(). */
      set_state_active();
    }
    else if (is_active_) {
      is_active_ = false;
      this->set_selected(false);
    }
  }
  if (std::optional<bool> is_selected = should_be_selected()) {
    set_selected(is_selected.value_or(false));
  }
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Renaming
 * \{ */

bool AbstractViewItem::supports_renaming() const
{
  /* No renaming by default. */
  return false;
}
bool AbstractViewItem::rename(const bContext & /*C*/, StringRefNull /*new_name*/)
{
  /* No renaming by default. */
  return false;
}

StringRef AbstractViewItem::get_rename_string() const
{
  /* No rename string by default. */
  return {};
}

bool AbstractViewItem::is_renaming() const
{
  return is_renaming_;
}

void AbstractViewItem::begin_renaming()
{
  AbstractView &view = this->get_view();
  if (view.is_renaming() || !supports_renaming()) {
    return;
  }

  if (view.begin_renaming()) {
    is_renaming_ = true;
  }

  StringRef initial_str = this->get_rename_string();
  std::copy(std::begin(initial_str), std::end(initial_str), std::begin(view.get_rename_buffer()));
}

void AbstractViewItem::rename_apply(const bContext &C)
{
  const AbstractView &view = this->get_view();
  rename(C, view.get_rename_buffer().data());
  end_renaming();
}

void AbstractViewItem::end_renaming()
{
  if (!is_renaming()) {
    return;
  }

  is_renaming_ = false;

  AbstractView &view = this->get_view();
  view.end_renaming();
}

static AbstractViewItem *find_item_from_rename_button(const uiBut &rename_but)
{
  /* A minimal sanity check, can't do much more here. */
  BLI_assert(rename_but.type == ButType::Text && rename_but.poin);

  for (const std::unique_ptr<uiBut> &but : rename_but.block->buttons) {
    if (but->type != ButType::ViewItem) {
      continue;
    }

    uiButViewItem *view_item_but = (uiButViewItem *)but.get();
    AbstractViewItem *item = reinterpret_cast<AbstractViewItem *>(view_item_but->view_item);
    const AbstractView &view = item->get_view();

    if (item->is_renaming() && (view.get_rename_buffer().data() == rename_but.poin)) {
      return item;
    }
  }

  return nullptr;
}

static void rename_button_fn(bContext *C, void *arg, char * /*origstr*/)
{
  const uiBut *rename_but = static_cast<uiBut *>(arg);
  AbstractViewItem *item = find_item_from_rename_button(*rename_but);
  BLI_assert(item);
  item->rename_apply(*C);
}

void AbstractViewItem::add_rename_button(uiBlock &block)
{
  AbstractView &view = this->get_view();
  uiBut *rename_but = uiDefBut(&block,
                               ButType::Text,
                               1,
                               "",
                               0,
                               0,
                               UI_UNIT_X * 10,
                               UI_UNIT_Y,
                               view.get_rename_buffer().data(),
                               1.0f,
                               view.get_rename_buffer().size(),
                               "");

  /* Gotta be careful with what's passed to the `arg1` here. Any view data will be freed once the
   * callback is executed. */
  UI_but_func_rename_set(rename_but, rename_button_fn, rename_but);
  UI_but_flag_disable(rename_but, UI_BUT_UNDO);

  const bContext *evil_C = reinterpret_cast<bContext *>(block.evil_C);
  ARegion *region = CTX_wm_region_popup(evil_C) ? CTX_wm_region_popup(evil_C) :
                                                  CTX_wm_region(evil_C);
  /* Returns false if the button was removed. */
  if (UI_but_active_only(evil_C, region, &block, rename_but) == false) {
    end_renaming();
  }
}

void AbstractViewItem::delete_item(bContext * /*C*/)
{
  /* No deletion by default. Needs type specific implementation. */
}

void AbstractViewItem::on_filter()
{
  /* No action by default. Needs type specific implementation. */
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Context Menu
 * \{ */

void AbstractViewItem::build_context_menu(bContext & /*C*/, uiLayout & /*column*/) const
{
  /* No context menu by default. */
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Filtering
 * \{ */

bool AbstractViewItem::should_be_filtered_visible(const StringRefNull filter_string) const
{
  StringRef name = this->get_rename_string();
  return fnmatch(filter_string.c_str(), name.data(), FNM_CASEFOLD) == 0;
}

bool AbstractViewItem::is_filtered_visible() const
{
  BLI_assert(get_view().needs_filtering_ == false);
  return is_filtered_visible_;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Drag 'n Drop
 * \{ */

std::unique_ptr<AbstractViewItemDragController> AbstractViewItem::create_drag_controller() const
{
  /* There's no drag controller (and hence no drag support) by default. */
  return nullptr;
}

std::unique_ptr<DropTargetInterface> AbstractViewItem::create_item_drop_target()
{
  /* There's no drop target (and hence no drop support) by default. */
  return nullptr;
}

std::optional<std::string> AbstractViewItem::debug_name() const
{
  return {};
}

AbstractViewItemDragController::AbstractViewItemDragController(AbstractView &view) : view_(view) {}

void AbstractViewItemDragController::on_drag_start(bContext & /*C*/)
{
  /* Do nothing by default. */
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name General Getters & Setters
 * \{ */

AbstractView &AbstractViewItem::get_view() const
{
  if (UNLIKELY(!view_)) {
    throw std::runtime_error(
        "Invalid state, item must be registered through AbstractView::register_item()");
  }
  return *view_;
}

uiButViewItem *AbstractViewItem::view_item_button() const
{
  return view_item_but_;
}

void AbstractViewItem::disable_activatable()
{
  is_activatable_ = false;
}

void AbstractViewItem::select_on_click_set()
{
  select_on_click_ = true;
}

bool AbstractViewItem::is_select_on_click() const
{
  return select_on_click_;
}

void AbstractViewItem::always_reactivate_on_click()
{
  reactivate_on_click_ = true;
}

void AbstractViewItem::activate_for_context_menu_set()
{
  activate_for_context_menu_ = true;
}

void AbstractViewItem::disable_interaction()
{
  is_interactive_ = false;
}

bool AbstractViewItem::is_interactive() const
{
  return is_interactive_;
}

bool AbstractViewItem::is_active() const
{
  BLI_assert_msg(this->get_view().is_reconstructed(),
                 "State cannot be queried until reconstruction is completed");
  return is_active_;
}

bool AbstractViewItem::is_selected() const
{
  BLI_assert_msg(this->get_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  return is_selected_;
}

bool AbstractViewItem::is_search_highlight() const
{
  return is_highlighted_search_;
}

/** \} */

}  // namespace blender::ui

/* ---------------------------------------------------------------------- */
/** \name C-API
 * \{ */

namespace blender::ui {

/**
 * Helper class to provide a higher level public (C-)API. Has access to private/protected view item
 * members and ensures some invariants that way.
 */
class ViewItemAPIWrapper {
 public:
  static bool matches(const AbstractViewItem &a, const AbstractViewItem &b)
  {
    if (typeid(a) != typeid(b)) {
      return false;
    }
    /* TODO should match the view as well. */
    return a.matches(b);
  }

  static void swap_button_pointers(AbstractViewItem &a, AbstractViewItem &b)
  {
    std::swap(a.view_item_but_, b.view_item_but_);
  }
};

}  // namespace blender::ui

using namespace blender::ui;

bool UI_view_item_matches(const AbstractViewItem &a, const AbstractViewItem &b)
{
  return ViewItemAPIWrapper::matches(a, b);
}

void ui_view_item_swap_button_pointers(AbstractViewItem &a, AbstractViewItem &b)
{
  ViewItemAPIWrapper::swap_button_pointers(a, b);
}

bool UI_view_item_can_rename(const AbstractViewItem &item)
{
  const AbstractView &view = item.get_view();
  return !view.is_renaming() && item.supports_renaming();
}

void UI_view_item_begin_rename(AbstractViewItem &item)
{
  item.begin_renaming();
}

bool UI_view_item_supports_drag(const AbstractViewItem &item)
{
  return item.create_drag_controller() != nullptr;
}

bool UI_view_item_popup_keep_open(const AbstractViewItem &item)
{
  return item.get_view().get_popup_keep_open();
}

bool UI_view_item_drag_start(bContext &C, AbstractViewItem &item)
{
  const std::unique_ptr<AbstractViewItemDragController> drag_controller =
      item.create_drag_controller();
  if (!drag_controller) {
    return false;
  }

  if (const std::optional<eWM_DragDataType> drag_type = drag_controller->get_drag_type()) {
    WM_event_start_drag(
        &C, ICON_NONE, *drag_type, drag_controller->create_drag_data(), WM_DRAG_FREE_DATA);
  }
  drag_controller->on_drag_start(C);

  /* Make sure the view item is highlighted as active when dragging from it. This is useful user
   * feedback. */
  item.set_state_active();

  return true;
}

/** \} */

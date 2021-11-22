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
 * \ingroup edinterface
 */

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"

#include "BLT_translation.h"

#include "interface_intern.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_tree_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */

/**
 * Add a tree-item to the container. This is the only place where items should be added, it handles
 * important invariants!
 */
AbstractTreeViewItem &TreeViewItemContainer::add_tree_item(
    std::unique_ptr<AbstractTreeViewItem> item)
{
  children_.append(std::move(item));

  /* The first item that will be added to the root sets this. */
  if (root_ == nullptr) {
    root_ = this;
  }

  AbstractTreeViewItem &added_item = *children_.last();
  added_item.root_ = root_;
  if (root_ != this) {
    /* Any item that isn't the root can be assumed to the a #AbstractTreeViewItem. Not entirely
     * nice to static_cast this, but well... */
    added_item.parent_ = static_cast<AbstractTreeViewItem *>(this);
  }

  return added_item;
}

void TreeViewItemContainer::foreach_item_recursive(ItemIterFn iter_fn, IterOptions options) const
{
  for (const auto &child : children_) {
    iter_fn(*child);
    if (bool(options & IterOptions::SkipCollapsed) && child->is_collapsed()) {
      continue;
    }

    child->foreach_item_recursive(iter_fn, options);
  }
}

/* ---------------------------------------------------------------------- */

void AbstractTreeView::foreach_item(ItemIterFn iter_fn, IterOptions options) const
{
  foreach_item_recursive(iter_fn, options);
}

bool AbstractTreeView::is_renaming() const
{
  return rename_buffer_ != nullptr;
}

void AbstractTreeView::build_layout_from_tree(const TreeViewLayoutBuilder &builder)
{
  uiLayout *prev_layout = builder.current_layout();

  uiLayout *box = uiLayoutBox(prev_layout);
  uiLayoutColumn(box, false);

  foreach_item([&builder](AbstractTreeViewItem &item) { builder.build_row(item); },
               IterOptions::SkipCollapsed);

  UI_block_layout_set_current(&builder.block(), prev_layout);
}

void AbstractTreeView::update_from_old(uiBlock &new_block)
{
  uiBlock *old_block = new_block.oldblock;
  if (!old_block) {
    /* Initial construction, nothing to update. */
    is_reconstructed_ = true;
    return;
  }

  uiTreeViewHandle *old_view_handle = ui_block_view_find_matching_in_old_block(
      &new_block, reinterpret_cast<uiTreeViewHandle *>(this));
  BLI_assert(old_view_handle);

  AbstractTreeView &old_view = reinterpret_cast<AbstractTreeView &>(*old_view_handle);

  /* Update own persistent data. */
  /* Keep the rename buffer persistent while renaming! The rename button uses the buffer's
   * pointer to identify itself over redraws. */
  rename_buffer_ = std::move(old_view.rename_buffer_);
  old_view.rename_buffer_ = nullptr;

  update_children_from_old_recursive(*this, old_view);

  /* Finished (re-)constructing the tree. */
  is_reconstructed_ = true;
}

void AbstractTreeView::update_children_from_old_recursive(const TreeViewItemContainer &new_items,
                                                          const TreeViewItemContainer &old_items)
{
  for (const auto &new_item : new_items.children_) {
    AbstractTreeViewItem *matching_old_item = find_matching_child(*new_item, old_items);
    if (!matching_old_item) {
      continue;
    }

    new_item->update_from_old(*matching_old_item);

    /* Recurse into children of the matched item. */
    update_children_from_old_recursive(*new_item, *matching_old_item);
  }
}

AbstractTreeViewItem *AbstractTreeView::find_matching_child(
    const AbstractTreeViewItem &lookup_item, const TreeViewItemContainer &items)
{
  for (const auto &iter_item : items.children_) {
    if (lookup_item.matches(*iter_item)) {
      /* We have a matching item! */
      return iter_item.get();
    }
  }

  return nullptr;
}

bool AbstractTreeView::is_reconstructed() const
{
  return is_reconstructed_;
}

void AbstractTreeView::change_state_delayed()
{
  BLI_assert_msg(
      is_reconstructed(),
      "These state changes are supposed to be delayed until reconstruction is completed");
  foreach_item([](AbstractTreeViewItem &item) { item.change_state_delayed(); });
}

/* ---------------------------------------------------------------------- */

void AbstractTreeViewItem::tree_row_click_fn(struct bContext * /*C*/,
                                             void *but_arg1,
                                             void * /*arg2*/)
{
  uiButTreeRow *tree_row_but = (uiButTreeRow *)but_arg1;
  AbstractTreeViewItem &tree_item = reinterpret_cast<AbstractTreeViewItem &>(
      *tree_row_but->tree_item);

  tree_item.activate();
}

void AbstractTreeViewItem::add_treerow_button(uiBlock &block)
{
  /* For some reason a width > (UI_UNIT_X * 2) make the layout system use all available width. */
  tree_row_but_ = (uiButTreeRow *)uiDefBut(
      &block, UI_BTYPE_TREEROW, 0, "", 0, 0, UI_UNIT_X * 10, UI_UNIT_Y, nullptr, 0, 0, 0, 0, "");

  tree_row_but_->tree_item = reinterpret_cast<uiTreeViewItemHandle *>(this);
  UI_but_func_set(&tree_row_but_->but, tree_row_click_fn, tree_row_but_, nullptr);
}

void AbstractTreeViewItem::add_indent(uiLayout &row) const
{
  uiBlock *block = uiLayoutGetBlock(&row);
  uiLayout *subrow = uiLayoutRow(&row, true);
  uiLayoutSetFixedSize(subrow, true);

  const float indent_size = count_parents() * UI_DPI_ICON_SIZE;
  uiDefBut(block, UI_BTYPE_SEPR, 0, "", 0, 0, indent_size, 0, nullptr, 0.0, 0.0, 0, 0, "");

  /* Indent items without collapsing icon some more within their parent. Makes it clear that they
   * are actually nested and not just a row at the same level without a chevron. */
  if (!is_collapsible() && parent_) {
    uiDefBut(block, UI_BTYPE_SEPR, 0, "", 0, 0, 0.2f * UI_UNIT_X, 0, nullptr, 0.0, 0.0, 0, 0, "");
  }

  /* Restore. */
  UI_block_layout_set_current(block, &row);
}

void AbstractTreeViewItem::collapse_chevron_click_fn(struct bContext *C,
                                                     void * /*but_arg1*/,
                                                     void * /*arg2*/)
{
  /* There's no data we could pass to this callback. It must be either the button itself or a
   * consistent address to match buttons over redraws. So instead of passing it somehow, just
   * lookup the hovered item via context here. */

  const wmWindow *win = CTX_wm_window(C);
  const ARegion *region = CTX_wm_region(C);
  uiTreeViewItemHandle *hovered_item_handle = UI_block_tree_view_find_item_at(region,
                                                                              win->eventstate->xy);
  AbstractTreeViewItem *hovered_item = reinterpret_cast<AbstractTreeViewItem *>(
      hovered_item_handle);
  BLI_assert(hovered_item != nullptr);

  hovered_item->toggle_collapsed();
  /* When collapsing an item with an active child, make this collapsed item active instead so the
   * active item stays visible. */
  if (hovered_item->has_active_child()) {
    hovered_item->activate();
  }
}

bool AbstractTreeViewItem::is_collapse_chevron_but(const uiBut *but)
{
  return but->type == UI_BTYPE_BUT_TOGGLE && ELEM(but->icon, ICON_TRIA_RIGHT, ICON_TRIA_DOWN) &&
         (but->func == collapse_chevron_click_fn);
}

void AbstractTreeViewItem::add_collapse_chevron(uiBlock &block) const
{
  if (!is_collapsible()) {
    return;
  }

  const BIFIconID icon = is_collapsed() ? ICON_TRIA_RIGHT : ICON_TRIA_DOWN;
  uiBut *but = uiDefIconBut(
      &block, UI_BTYPE_BUT_TOGGLE, 0, icon, 0, 0, UI_UNIT_X, UI_UNIT_Y, nullptr, 0, 0, 0, 0, "");
  /* Note that we're passing the tree-row button here, not the chevron one. */
  UI_but_func_set(but, collapse_chevron_click_fn, nullptr, nullptr);
  UI_but_flag_disable(but, UI_BUT_UNDO);

  /* Check if the query for the button matches the created button. */
  BLI_assert(is_collapse_chevron_but(but));
}

AbstractTreeViewItem *AbstractTreeViewItem::find_tree_item_from_rename_button(
    const uiBut &rename_but)
{
  /* A minimal sanity check, can't do much more here. */
  BLI_assert(rename_but.type == UI_BTYPE_TEXT && rename_but.poin);

  LISTBASE_FOREACH (uiBut *, but, &rename_but.block->buttons) {
    if (but->type != UI_BTYPE_TREEROW) {
      continue;
    }

    uiButTreeRow *tree_row_but = (uiButTreeRow *)but;
    AbstractTreeViewItem *item = reinterpret_cast<AbstractTreeViewItem *>(tree_row_but->tree_item);
    const AbstractTreeView &tree_view = item->get_tree_view();

    if (item->is_renaming() && (tree_view.rename_buffer_->data() == rename_but.poin)) {
      return item;
    }
  }

  return nullptr;
}

void AbstractTreeViewItem::rename_button_fn(bContext *UNUSED(C), void *arg, char *UNUSED(origstr))
{
  const uiBut *rename_but = static_cast<uiBut *>(arg);
  AbstractTreeViewItem *item = find_tree_item_from_rename_button(*rename_but);
  BLI_assert(item);

  const AbstractTreeView &tree_view = item->get_tree_view();
  item->rename(tree_view.rename_buffer_->data());
  item->end_renaming();
}

void AbstractTreeViewItem::add_rename_button(uiLayout &row)
{
  uiBlock *block = uiLayoutGetBlock(&row);
  eUIEmbossType previous_emboss = UI_block_emboss_get(block);

  uiLayoutRow(&row, false);
  /* Enable emboss for the text button. */
  UI_block_emboss_set(block, UI_EMBOSS);

  AbstractTreeView &tree_view = get_tree_view();
  uiBut *rename_but = uiDefBut(block,
                               UI_BTYPE_TEXT,
                               1,
                               "",
                               0,
                               0,
                               UI_UNIT_X * 10,
                               UI_UNIT_Y,
                               tree_view.rename_buffer_->data(),
                               1.0f,
                               tree_view.rename_buffer_->max_size(),
                               0,
                               0,
                               "");

  /* Gotta be careful with what's passed to the `arg1` here. Any tree data will be freed once the
   * callback is executed. */
  UI_but_func_rename_set(rename_but, AbstractTreeViewItem::rename_button_fn, rename_but);
  UI_but_flag_disable(rename_but, UI_BUT_UNDO);

  const bContext *evil_C = static_cast<bContext *>(block->evil_C);
  ARegion *region = CTX_wm_region(evil_C);
  /* Returns false if the button was removed. */
  if (UI_but_active_only(evil_C, region, block, rename_but) == false) {
    end_renaming();
  }

  UI_block_emboss_set(block, previous_emboss);
  UI_block_layout_set_current(block, &row);
}

bool AbstractTreeViewItem::has_active_child() const
{
  bool found = false;
  foreach_item_recursive([&found](const AbstractTreeViewItem &item) {
    if (item.is_active()) {
      found = true;
    }
  });

  return found;
}

void AbstractTreeViewItem::on_activate()
{
  /* Do nothing by default. */
}

void AbstractTreeViewItem::is_active(IsActiveFn is_active_fn)
{
  is_active_fn_ = is_active_fn;
}

std::unique_ptr<AbstractTreeViewItemDragController> AbstractTreeViewItem::create_drag_controller()
    const
{
  /* There's no drag controller (and hence no drag support) by default. */
  return nullptr;
}

std::unique_ptr<AbstractTreeViewItemDropController> AbstractTreeViewItem::create_drop_controller()
    const
{
  /* There's no drop controller (and hence no drop support) by default. */
  return nullptr;
}

bool AbstractTreeViewItem::can_rename() const
{
  /* No renaming by default. */
  return false;
}

bool AbstractTreeViewItem::rename(StringRefNull new_name)
{
  /* It is important to update the label after renaming, so #AbstractTreeViewItem::matches()
   * recognizes the item. (It only compares labels by default.) */
  label_ = new_name;
  return true;
}

void AbstractTreeViewItem::build_context_menu(bContext & /*C*/, uiLayout & /*column*/) const
{
  /* No context menu by default. */
}

void AbstractTreeViewItem::update_from_old(const AbstractTreeViewItem &old)
{
  is_open_ = old.is_open_;
  is_active_ = old.is_active_;
  is_renaming_ = old.is_renaming_;
}

bool AbstractTreeViewItem::matches(const AbstractTreeViewItem &other) const
{
  return label_ == other.label_;
}

void AbstractTreeViewItem::begin_renaming()
{
  AbstractTreeView &tree_view = get_tree_view();
  if (tree_view.is_renaming() || !can_rename()) {
    return;
  }

  is_renaming_ = true;

  tree_view.rename_buffer_ = std::make_unique<decltype(tree_view.rename_buffer_)::element_type>();
  std::copy(std::begin(label_), std::end(label_), std::begin(*tree_view.rename_buffer_));
}

void AbstractTreeViewItem::end_renaming()
{
  if (!is_renaming()) {
    return;
  }

  is_renaming_ = false;

  AbstractTreeView &tree_view = get_tree_view();
  tree_view.rename_buffer_ = nullptr;
}

AbstractTreeView &AbstractTreeViewItem::get_tree_view() const
{
  return static_cast<AbstractTreeView &>(*root_);
}

int AbstractTreeViewItem::count_parents() const
{
  int i = 0;
  for (TreeViewItemContainer *parent = parent_; parent; parent = parent->parent_) {
    i++;
  }
  return i;
}

void AbstractTreeViewItem::activate()
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "Item activation can't be done until reconstruction is completed");

  if (is_active()) {
    return;
  }

  /* Deactivate other items in the tree. */
  get_tree_view().foreach_item([](auto &item) { item.deactivate(); });

  on_activate();
  /* Make sure the active item is always visible. */
  ensure_parents_uncollapsed();

  is_active_ = true;
}

void AbstractTreeViewItem::deactivate()
{
  is_active_ = false;
}

bool AbstractTreeViewItem::is_active() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  return is_active_;
}

bool AbstractTreeViewItem::is_hovered() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  BLI_assert_msg(tree_row_but_ != nullptr,
                 "Hovered state can't be queried before the tree row is being built");

  const uiTreeViewItemHandle *this_handle = reinterpret_cast<const uiTreeViewItemHandle *>(this);
  /* The new layout hasn't finished construction yet, so the final state of the button is unknown.
   * Get the matching button from the previous redraw instead. */
  uiButTreeRow *old_treerow_but = ui_block_view_find_treerow_in_old_block(tree_row_but_->but.block,
                                                                          this_handle);
  return old_treerow_but && (old_treerow_but->but.flag & UI_ACTIVE);
}

bool AbstractTreeViewItem::is_collapsed() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  return is_collapsible() && !is_open_;
}

void AbstractTreeViewItem::toggle_collapsed()
{
  is_open_ = !is_open_;
}

void AbstractTreeViewItem::set_collapsed(bool collapsed)
{
  is_open_ = !collapsed;
}

bool AbstractTreeViewItem::is_collapsible() const
{
  return !children_.is_empty();
}

bool AbstractTreeViewItem::is_renaming() const
{
  return is_renaming_;
}

void AbstractTreeViewItem::ensure_parents_uncollapsed()
{
  for (AbstractTreeViewItem *parent = parent_; parent; parent = parent->parent_) {
    parent->set_collapsed(false);
  }
}

bool AbstractTreeViewItem::matches_including_parents(const AbstractTreeViewItem &other) const
{
  if (!matches(other)) {
    return false;
  }
  if (count_parents() != other.count_parents()) {
    return false;
  }

  for (AbstractTreeViewItem *parent = parent_, *other_parent = other.parent_;
       parent && other_parent;
       parent = parent->parent_, other_parent = other_parent->parent_) {
    if (!parent->matches(*other_parent)) {
      return false;
    }
  }

  return true;
}

uiButTreeRow *AbstractTreeViewItem::tree_row_button()
{
  return tree_row_but_;
}

void AbstractTreeViewItem::change_state_delayed()
{
  if (is_active_fn_()) {
    activate();
  }
}
/* ---------------------------------------------------------------------- */

AbstractTreeViewItemDropController::AbstractTreeViewItemDropController(AbstractTreeView &tree_view)
    : tree_view_(tree_view)
{
}

/* ---------------------------------------------------------------------- */

TreeViewBuilder::TreeViewBuilder(uiBlock &block) : block_(block)
{
}

void TreeViewBuilder::build_tree_view(AbstractTreeView &tree_view)
{
  tree_view.build_tree();
  tree_view.update_from_old(block_);
  tree_view.change_state_delayed();
  tree_view.build_layout_from_tree(TreeViewLayoutBuilder(block_));
}

/* ---------------------------------------------------------------------- */

TreeViewLayoutBuilder::TreeViewLayoutBuilder(uiBlock &block) : block_(block)
{
}

/**
 * Moves the button following the last added chevron closer to the list item.
 *
 * Iterates backwards over buttons until finding the tree-row button, which is assumed to be the
 * first button added for the row, and can act as a delimiter that way.
 */
void TreeViewLayoutBuilder::polish_layout(const uiBlock &block)
{
  LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block.buttons) {
    if (AbstractTreeViewItem::is_collapse_chevron_but(but) && but->next &&
        /* Embossed buttons with padding-less text padding look weird, so don't touch them. */
        ELEM(but->next->emboss, UI_EMBOSS_NONE, UI_EMBOSS_NONE_OR_STATUS)) {
      UI_but_drawflag_enable(static_cast<uiBut *>(but->next), UI_BUT_NO_TEXT_PADDING);
    }

    if (but->type == UI_BTYPE_TREEROW) {
      break;
    }
  }
}

void TreeViewLayoutBuilder::build_row(AbstractTreeViewItem &item) const
{
  uiBlock &block_ = block();

  uiLayout *prev_layout = current_layout();
  eUIEmbossType previous_emboss = UI_block_emboss_get(&block_);

  uiLayout *overlap = uiLayoutOverlap(prev_layout);

  uiLayoutRow(overlap, false);
  /* Every item gets one! Other buttons can be overlapped on top. */
  item.add_treerow_button(block_);

  /* After adding tree-row button (would disable hover highlighting). */
  UI_block_emboss_set(&block_, UI_EMBOSS_NONE);

  uiLayout *row = uiLayoutRow(overlap, true);
  item.add_indent(*row);
  item.add_collapse_chevron(block_);

  if (item.is_renaming()) {
    item.add_rename_button(*row);
  }
  else {
    item.build_row(*row);
  }
  polish_layout(block_);

  UI_block_emboss_set(&block_, previous_emboss);
  UI_block_layout_set_current(&block_, prev_layout);
}

uiBlock &TreeViewLayoutBuilder::block() const
{
  return block_;
}

uiLayout *TreeViewLayoutBuilder::current_layout() const
{
  return block().curlayout;
}

/* ---------------------------------------------------------------------- */

BasicTreeViewItem::BasicTreeViewItem(StringRef label, BIFIconID icon_) : icon(icon_)
{
  label_ = label;
}

void BasicTreeViewItem::build_row(uiLayout &row)
{
  add_label(row);
}

void BasicTreeViewItem::add_label(uiLayout &layout, StringRefNull label_override)
{
  const StringRefNull label = label_override.is_empty() ? StringRefNull(label_) : label_override;

  /* Some padding for labels without collapse chevron and no icon. Looks weird without. */
  if (icon == ICON_NONE && !is_collapsible()) {
    uiItemS_ex(&layout, 0.8f);
  }
  uiItemL(&layout, IFACE_(label.c_str()), icon);
}

void BasicTreeViewItem::on_activate()
{
  if (activate_fn_) {
    activate_fn_(*this);
  }
}

void BasicTreeViewItem::on_activate(ActivateFn fn)
{
  activate_fn_ = fn;
}

}  // namespace blender::ui

using namespace blender::ui;

bool UI_tree_view_item_is_active(const uiTreeViewItemHandle *item_handle)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_handle);
  return item.is_active();
}

bool UI_tree_view_item_matches(const uiTreeViewItemHandle *a_handle,
                               const uiTreeViewItemHandle *b_handle)
{
  const AbstractTreeViewItem &a = reinterpret_cast<const AbstractTreeViewItem &>(*a_handle);
  const AbstractTreeViewItem &b = reinterpret_cast<const AbstractTreeViewItem &>(*b_handle);
  /* TODO should match the tree-view as well. */
  return a.matches_including_parents(b);
}

/**
 * Attempt to start dragging the tree-item \a item_. This will not work if the tree item doesn't
 * support dragging, i.e. it won't create a drag-controller upon request.
 * \return True if dragging started successfully, otherwise false.
 */
bool UI_tree_view_item_drag_start(bContext *C, uiTreeViewItemHandle *item_)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_);
  const std::unique_ptr<AbstractTreeViewItemDragController> drag_controller =
      item.create_drag_controller();
  if (!drag_controller) {
    return false;
  }

  WM_event_start_drag(C,
                      ICON_NONE,
                      drag_controller->get_drag_type(),
                      drag_controller->create_drag_data(),
                      0,
                      WM_DRAG_FREE_DATA);
  return true;
}

bool UI_tree_view_item_can_drop(const uiTreeViewItemHandle *item_,
                                const wmDrag *drag,
                                const char **r_disabled_hint)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_);
  const std::unique_ptr<AbstractTreeViewItemDropController> drop_controller =
      item.create_drop_controller();
  if (!drop_controller) {
    return false;
  }

  return drop_controller->can_drop(*drag, r_disabled_hint);
}

char *UI_tree_view_item_drop_tooltip(const uiTreeViewItemHandle *item_, const wmDrag *drag)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_);
  const std::unique_ptr<AbstractTreeViewItemDropController> drop_controller =
      item.create_drop_controller();
  if (!drop_controller) {
    return nullptr;
  }

  return BLI_strdup(drop_controller->drop_tooltip(*drag).c_str());
}

/**
 * Let a tree-view item handle a drop event.
 * \return True if the drop was handled by the tree-view item.
 */
bool UI_tree_view_item_drop_handle(uiTreeViewItemHandle *item_, const ListBase *drags)
{
  AbstractTreeViewItem &item = reinterpret_cast<AbstractTreeViewItem &>(*item_);
  std::unique_ptr<AbstractTreeViewItemDropController> drop_controller =
      item.create_drop_controller();

  const char *disabled_hint_dummy = nullptr;
  LISTBASE_FOREACH (const wmDrag *, drag, drags) {
    if (drop_controller->can_drop(*drag, &disabled_hint_dummy)) {
      return drop_controller->on_drop(*drag);
    }
  }

  return false;
}

/**
 * Can \a item_handle be renamed right now? Not that this isn't just a mere wrapper around
 * #AbstractTreeViewItem::can_rename(). This also checks if there is another item being renamed,
 * and returns false if so.
 */
bool UI_tree_view_item_can_rename(const uiTreeViewItemHandle *item_handle)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_handle);
  const AbstractTreeView &tree_view = item.get_tree_view();
  return !tree_view.is_renaming() && item.can_rename();
}

void UI_tree_view_item_begin_rename(uiTreeViewItemHandle *item_handle)
{
  AbstractTreeViewItem &item = reinterpret_cast<AbstractTreeViewItem &>(*item_handle);
  item.begin_renaming();
}

void UI_tree_view_item_context_menu_build(bContext *C,
                                          const uiTreeViewItemHandle *item_handle,
                                          uiLayout *column)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_handle);
  item.build_context_menu(*C, *column);
}

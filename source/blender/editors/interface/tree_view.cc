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

#include "BKE_context.h"

#include "BLT_translation.h"

#include "interface_intern.h"

#include "UI_interface.h"

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
  uiLayoutColumn(box, true);

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
  BasicTreeViewItem &tree_item = reinterpret_cast<BasicTreeViewItem &>(*tree_row_but->tree_item);

  /* Let a click on an opened item activate it, a second click will close it then.
   * TODO Should this be for asset catalogs only? */
  if (tree_item.is_collapsed() || tree_item.is_active()) {
    tree_item.toggle_collapsed();
  }
  tree_item.activate();
}

void AbstractTreeViewItem::add_treerow_button(uiBlock &block)
{
  tree_row_but_ = (uiButTreeRow *)uiDefBut(
      &block, UI_BTYPE_TREEROW, 0, "", 0, 0, UI_UNIT_X, UI_UNIT_Y, nullptr, 0, 0, 0, 0, "");

  tree_row_but_->tree_item = reinterpret_cast<uiTreeViewItemHandle *>(this);
  UI_but_func_set(&tree_row_but_->but, tree_row_click_fn, tree_row_but_, nullptr);
  UI_but_treerow_indentation_set(&tree_row_but_->but, count_parents());
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

void AbstractTreeViewItem::add_rename_button(uiBlock &block)
{
  AbstractTreeView &tree_view = get_tree_view();
  uiBut *rename_but = uiDefBut(&block,
                               UI_BTYPE_TEXT,
                               1,
                               "",
                               0,
                               0,
                               UI_UNIT_X,
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

  const bContext *evil_C = static_cast<bContext *>(block.evil_C);
  ARegion *region = CTX_wm_region(evil_C);
  /* Returns false if the button was removed. */
  if (UI_but_active_only(evil_C, region, &block, rename_but) == false) {
    end_renaming();
  }
}

void AbstractTreeViewItem::on_activate()
{
  /* Do nothing by default. */
}

void AbstractTreeViewItem::is_active(IsActiveFn is_active_fn)
{
  is_active_fn_ = is_active_fn;
}

bool AbstractTreeViewItem::on_drop(const wmDrag & /*drag*/)
{
  /* Do nothing by default. */
  return false;
}

bool AbstractTreeViewItem::can_drop(const wmDrag & /*drag*/) const
{
  return false;
}

std::string AbstractTreeViewItem::drop_tooltip(const bContext & /*C*/,
                                               const wmDrag & /*drag*/,
                                               const wmEvent & /*event*/) const
{
  return TIP_("Drop into/onto tree item");
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

const AbstractTreeView &AbstractTreeViewItem::get_tree_view() const
{
  return static_cast<AbstractTreeView &>(*root_);
}

AbstractTreeView &AbstractTreeViewItem::get_tree_view()
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

void AbstractTreeViewItem::change_state_delayed()
{
  if (is_active_fn_()) {
    activate();
  }
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

void TreeViewLayoutBuilder::build_row(AbstractTreeViewItem &item) const
{
  uiLayout *prev_layout = current_layout();
  uiLayout *row = uiLayoutRow(prev_layout, false);

  uiLayoutOverlap(row);

  uiBlock &block_ = block();

  /* Every item gets one! Other buttons can be overlapped on top. */
  item.add_treerow_button(block_);

  if (item.is_renaming()) {
    item.add_rename_button(block_);
  }
  else {
    item.build_row(*row);
  }

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

void BasicTreeViewItem::build_row(uiLayout & /*row*/)
{
  if (BIFIconID icon = get_draw_icon()) {
    ui_def_but_icon(&tree_row_but_->but, icon, UI_HAS_ICON);
  }
  tree_row_but_->but.str = BLI_strdupn(label_.c_str(), label_.length());
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

BIFIconID BasicTreeViewItem::get_draw_icon() const
{
  if (icon) {
    return icon;
  }

  if (is_collapsible()) {
    return is_collapsed() ? ICON_TRIA_RIGHT : ICON_TRIA_DOWN;
  }

  return ICON_NONE;
}

uiBut *BasicTreeViewItem::button()
{
  return &tree_row_but_->but;
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

bool UI_tree_view_item_can_drop(const uiTreeViewItemHandle *item_, const wmDrag *drag)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_);
  return item.can_drop(*drag);
}

char *UI_tree_view_item_drop_tooltip(const uiTreeViewItemHandle *item_,
                                     const bContext *C,
                                     const wmDrag *drag,
                                     const wmEvent *event)
{
  const AbstractTreeViewItem &item = reinterpret_cast<const AbstractTreeViewItem &>(*item_);
  return BLI_strdup(item.drop_tooltip(*C, *drag, *event).c_str());
}

/**
 * Let a tree-view item handle a drop event.
 * \return True if the drop was handled by the tree-view item.
 */
bool UI_tree_view_item_drop_handle(uiTreeViewItemHandle *item_, const ListBase *drags)
{
  AbstractTreeViewItem &item = reinterpret_cast<AbstractTreeViewItem &>(*item_);

  LISTBASE_FOREACH (const wmDrag *, drag, drags) {
    if (item.can_drop(*drag)) {
      return item.on_drop(*drag);
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

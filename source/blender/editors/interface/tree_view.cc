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

void AbstractTreeView::build_layout_from_tree(const TreeViewLayoutBuilder &builder)
{
  uiLayout *prev_layout = builder.current_layout();

  uiLayoutColumn(prev_layout, true);

  foreach_item([&builder](AbstractTreeViewItem &item) { builder.build_row(item); },
               IterOptions::SkipCollapsed);

  UI_block_layout_set_current(&builder.block(), prev_layout);
}

void AbstractTreeView::update_from_old(uiBlock &new_block)
{
  uiBlock *old_block = new_block.oldblock;
  if (!old_block) {
    return;
  }

  uiTreeViewHandle *old_view_handle = ui_block_view_find_matching_in_old_block(
      &new_block, reinterpret_cast<uiTreeViewHandle *>(this));
  if (!old_view_handle) {
    return;
  }

  AbstractTreeView &old_view = reinterpret_cast<AbstractTreeView &>(*old_view_handle);
  update_children_from_old_recursive(*this, old_view);
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

/* ---------------------------------------------------------------------- */

void AbstractTreeViewItem::on_activate()
{
  /* Do nothing by default. */
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

void AbstractTreeViewItem::update_from_old(const AbstractTreeViewItem &old)
{
  is_open_ = old.is_open_;
  is_active_ = old.is_active_;
}

bool AbstractTreeViewItem::matches(const AbstractTreeViewItem &other) const
{
  return label_ == other.label_;
}

const AbstractTreeView &AbstractTreeViewItem::get_tree_view() const
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

void AbstractTreeViewItem::set_active(bool value)
{
  if (value && !is_active()) {
    /* Deactivate other items in the tree. */
    get_tree_view().foreach_item([](auto &item) { item.set_active(false); });
    on_activate();
  }
  is_active_ = value;
}

bool AbstractTreeViewItem::is_active() const
{
  return is_active_;
}

bool AbstractTreeViewItem::is_collapsed() const
{
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

/* ---------------------------------------------------------------------- */

TreeViewBuilder::TreeViewBuilder(uiBlock &block) : block_(block)
{
}

void TreeViewBuilder::build_tree_view(AbstractTreeView &tree_view)
{
  tree_view.build_tree();
  tree_view.update_from_old(block_);
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

  item.build_row(*row);

  UI_block_layout_set_current(&block(), prev_layout);
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

BasicTreeViewItem::BasicTreeViewItem(StringRef label, BIFIconID icon_, ActivateFn activate_fn)
    : icon(icon_), activate_fn_(activate_fn)
{
  label_ = label;
}

static void tree_row_click_fn(struct bContext *UNUSED(C), void *but_arg1, void *UNUSED(arg2))
{
  uiButTreeRow *tree_row_but = (uiButTreeRow *)but_arg1;
  AbstractTreeViewItem &tree_item = reinterpret_cast<AbstractTreeViewItem &>(
      *tree_row_but->tree_item);

  /* Let a click on an opened item activate it, a second click will close it then.
   * TODO Should this be for asset catalogs only? */
  if (tree_item.is_collapsed() || tree_item.is_active()) {
    tree_item.toggle_collapsed();
  }
  tree_item.set_active();
}

void BasicTreeViewItem::build_row(uiLayout &row)
{
  uiBlock *block = uiLayoutGetBlock(&row);
  tree_row_but_ = (uiButTreeRow *)uiDefIconTextBut(block,
                                                   UI_BTYPE_TREEROW,
                                                   0,
                                                   /* TODO allow icon besides the chevron icon? */
                                                   get_draw_icon(),
                                                   label_.data(),
                                                   0,
                                                   0,
                                                   UI_UNIT_X,
                                                   UI_UNIT_Y,
                                                   nullptr,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   nullptr);

  tree_row_but_->tree_item = reinterpret_cast<uiTreeViewItemHandle *>(this);
  UI_but_func_set(&tree_row_but_->but, tree_row_click_fn, tree_row_but_, nullptr);
  UI_but_treerow_indentation_set(&tree_row_but_->but, count_parents());
}

void BasicTreeViewItem::on_activate()
{
  if (activate_fn_) {
    activate_fn_(*this);
  }
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
  return a.matches(b);
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

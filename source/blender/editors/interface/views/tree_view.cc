/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"

#include "BLT_translation.h"

#include "GPU_immediate.h"

#include "interface_intern.hh"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_tree_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */

AbstractTreeViewItem &TreeViewItemContainer::add_tree_item(
    std::unique_ptr<AbstractTreeViewItem> item)
{
  children_.append(std::move(item));

  /* The first item that will be added to the root sets this. */
  if (root_ == nullptr) {
    root_ = this;
  }
  AbstractTreeView &tree_view = static_cast<AbstractTreeView &>(*root_);
  AbstractTreeViewItem &added_item = *children_.last();
  added_item.root_ = root_;
  tree_view.register_item(added_item);

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
    bool skip = false;
    if (bool(options & IterOptions::SkipFiltered) && !child->is_filtered_visible_cached()) {
      skip = true;
    }

    if (!skip) {
      iter_fn(*child);
    }

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

AbstractTreeViewItem *AbstractTreeView::find_hovered(const ARegion &region, const int2 &xy)
{
  AbstractTreeViewItem *hovered_item = nullptr;
  foreach_item_recursive(
      [&](AbstractTreeViewItem &item) {
        if (hovered_item) {
          return;
        }

        std::optional<rctf> win_rect = item.get_win_rect(region);
        if (win_rect && BLI_rctf_isect_y(&*win_rect, xy[1])) {
          hovered_item = &item;
        }
      },
      IterOptions::SkipCollapsed | IterOptions::SkipFiltered);

  return hovered_item;
}

void AbstractTreeView::set_min_rows(int min_rows)
{
  min_rows_ = min_rows;
}

AbstractTreeViewItem *AbstractTreeView::find_last_visible_descendant(
    const AbstractTreeViewItem &parent) const
{
  if (parent.is_collapsed()) {
    return nullptr;
  }

  AbstractTreeViewItem *last_descendant = parent.children_.last().get();
  while (!last_descendant->children_.is_empty() && !last_descendant->is_collapsed()) {
    last_descendant = last_descendant->children_.last().get();
  }

  return last_descendant;
}

void AbstractTreeView::draw_hierarchy_lines_recursive(const ARegion &region,
                                                      const TreeViewOrItem &parent,
                                                      const uint pos) const
{
  for (const auto &item : parent.children_) {
    if (!item->is_collapsible() || item->is_collapsed()) {
      continue;
    }

    draw_hierarchy_lines_recursive(region, *item, pos);

    const AbstractTreeViewItem *first_descendant = item->children_.first().get();
    const AbstractTreeViewItem *last_descendant = find_last_visible_descendant(*item);
    if (!first_descendant->view_item_but_ || !last_descendant || !last_descendant->view_item_but_)
    {
      return;
    }
    const uiButViewItem &first_child_but = *first_descendant->view_item_button();
    const uiButViewItem &last_child_but = *last_descendant->view_item_button();

    BLI_assert(first_child_but.block == last_child_but.block);
    const uiBlock *block = first_child_but.block;

    rcti first_child_rect;
    ui_but_to_pixelrect(&first_child_rect, &region, block, &first_child_but);
    rcti last_child_rect;
    ui_but_to_pixelrect(&last_child_rect, &region, block, &last_child_but);

    /* Small vertical padding. */
    const short line_padding = UI_UNIT_Y / 4.0f;
    const float x = first_child_rect.xmin + first_descendant->indent_width() -
                    UI_ICON_SIZE * 0.5f + 2 * UI_SCALE_FAC;
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, x, first_child_rect.ymax - line_padding);
    immVertex2f(pos, x, last_child_rect.ymin + line_padding);
    immEnd();
  }
}

void AbstractTreeView::draw_hierarchy_lines(const ARegion &region) const
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uchar col[4];

  immBindBuiltinProgram(GPU_SHADER_3D_LINE_DASHED_UNIFORM_COLOR);

  float viewport_size[4];
  GPU_viewport_size_get_f(viewport_size);
  immUniform2f("viewport_size", viewport_size[2] / UI_SCALE_FAC, viewport_size[3] / UI_SCALE_FAC);
  immUniform1i("colors_len", 0); /* "simple"  mode */
  immUniform1f("dash_width", 8.0f);
  /* >= is 1.0 for un-dashed lines. */
  immUniform1f("udash_factor", 1.0f);
  UI_GetThemeColorBlend3ubv(TH_BACK, TH_TEXT, 0.4f, col);
  col[3] = 255;
  immUniformColor4ubv(col);

  GPU_line_width(1.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  draw_hierarchy_lines_recursive(region, *this, pos);
  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

void AbstractTreeView::draw_overlays(const ARegion &region) const
{
  draw_hierarchy_lines(region);
}

void AbstractTreeView::update_children_from_old(const AbstractView &old_view)
{
  const AbstractTreeView &old_tree_view = dynamic_cast<const AbstractTreeView &>(old_view);

  update_children_from_old_recursive(*this, old_tree_view);
}

void AbstractTreeView::update_children_from_old_recursive(const TreeViewOrItem &new_items,
                                                          const TreeViewOrItem &old_items)
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
    const AbstractTreeViewItem &lookup_item, const TreeViewOrItem &items)
{
  for (const auto &iter_item : items.children_) {
    if (lookup_item.matches(*iter_item)) {
      /* We have a matching item! */
      return iter_item.get();
    }
  }

  return nullptr;
}

void AbstractTreeView::change_state_delayed()
{
  BLI_assert_msg(
      is_reconstructed(),
      "These state changes are supposed to be delayed until reconstruction is completed");

/* Debug-only sanity check: Ensure only one item requests to be active. */
#ifndef NDEBUG
  bool has_active = false;
  foreach_item([&has_active](AbstractTreeViewItem &item) {
    if (item.should_be_active().value_or(false)) {
      BLI_assert_msg(
          !has_active,
          "Only one view item should ever return true for its `should_be_active()` method");
      has_active = true;
    }
  });
#endif

  foreach_item([](AbstractTreeViewItem &item) { item.change_state_delayed(); });
}

/* ---------------------------------------------------------------------- */

TreeViewItemDropTarget::TreeViewItemDropTarget(AbstractTreeView &view, DropBehavior behavior)
    : view_(view), behavior_(behavior)
{
}

std::optional<DropLocation> TreeViewItemDropTarget::choose_drop_location(
    const ARegion &region, const wmEvent &event) const
{
  if (behavior_ == DropBehavior::Insert) {
    return DropLocation::Into;
  }

  const AbstractTreeViewItem *hovered_item = view_.find_hovered(region, event.xy);
  if (!hovered_item) {
    return std::nullopt;
  }
  std::optional<rctf> win_rect = hovered_item->get_win_rect(region);
  BLI_assert(win_rect.has_value());
  const float item_height = BLI_rctf_size_y(&*win_rect);

  BLI_assert(ELEM(behavior_, DropBehavior::Reorder, DropBehavior::ReorderAndInsert));

  const int segment_count =
      (behavior_ == DropBehavior::Reorder) ?
          /* Divide into upper (insert before) and lower (insert after) half. */
          2 :
          /* Upper (insert before), middle (insert into) and lower (insert after) third. */
          3;
  const float segment_height = item_height / segment_count;

  if (event.xy[1] - win_rect->ymin > (item_height - segment_height)) {
    return DropLocation::Before;
  }
  if (event.xy[1] - win_rect->ymin <= segment_height) {
    if (behavior_ == DropBehavior::ReorderAndInsert && hovered_item->is_collapsible() &&
        !hovered_item->is_collapsed())
    {
      /* Special case: Dropping at the lower 3rd of an uncollapsed item should insert into it, not
       * after. */
      return DropLocation::Into;
    }
    return DropLocation::After;
  }

  BLI_assert(behavior_ == DropBehavior::ReorderAndInsert);
  return DropLocation::Into;
}

/* ---------------------------------------------------------------------- */

void AbstractTreeViewItem::tree_row_click_fn(bContext * /*C*/, void *but_arg1, void * /*arg2*/)
{
  uiButViewItem *item_but = (uiButViewItem *)but_arg1;
  AbstractTreeViewItem &tree_item = reinterpret_cast<AbstractTreeViewItem &>(*item_but->view_item);

  tree_item.activate();
  /* Not only activate the item, also show its children. Maybe this should be optional, or
   * controlled by the specific tree-view. */
  tree_item.set_collapsed(false);
}

void AbstractTreeViewItem::add_treerow_button(uiBlock &block)
{
  /* For some reason a width > (UI_UNIT_X * 2) make the layout system use all available width. */
  view_item_but_ = (uiButViewItem *)uiDefBut(
      &block, UI_BTYPE_VIEW_ITEM, 0, "", 0, 0, UI_UNIT_X * 10, UI_UNIT_Y, nullptr, 0, 0, 0, 0, "");

  view_item_but_->view_item = reinterpret_cast<uiViewItemHandle *>(this);
  UI_but_func_set(view_item_but_, tree_row_click_fn, view_item_but_, nullptr);
}

int AbstractTreeViewItem::indent_width() const
{
  return count_parents() * UI_ICON_SIZE;
}

void AbstractTreeViewItem::add_indent(uiLayout &row) const
{
  uiBlock *block = uiLayoutGetBlock(&row);
  uiLayout *subrow = uiLayoutRow(&row, true);
  uiLayoutSetFixedSize(subrow, true);

  uiDefBut(block, UI_BTYPE_SEPR, 0, "", 0, 0, indent_width(), 0, nullptr, 0.0, 0.0, 0, 0, "");

  /* Indent items without collapsing icon some more within their parent. Makes it clear that they
   * are actually nested and not just a row at the same level without a chevron. */
  if (!is_collapsible() && parent_) {
    uiDefBut(block, UI_BTYPE_SEPR, 0, "", 0, 0, 0.2f * UI_UNIT_X, 0, nullptr, 0.0, 0.0, 0, 0, "");
  }

  /* Restore. */
  UI_block_layout_set_current(block, &row);
}

void AbstractTreeViewItem::collapse_chevron_click_fn(bContext *C,
                                                     void * /*but_arg1*/,
                                                     void * /*arg2*/)
{
  /* There's no data we could pass to this callback. It must be either the button itself or a
   * consistent address to match buttons over redraws. So instead of passing it somehow, just
   * lookup the hovered item via context here. */

  const wmWindow *win = CTX_wm_window(C);
  const ARegion *region = CTX_wm_menu(C) ? CTX_wm_menu(C) : CTX_wm_region(C);
  uiViewItemHandle *hovered_item_handle = UI_region_views_find_item_at(region,
                                                                       win->eventstate->xy);

  AbstractTreeViewItem *hovered_item = from_item_handle<AbstractTreeViewItem>(hovered_item_handle);
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

void AbstractTreeViewItem::add_rename_button(uiLayout &row)
{
  uiBlock *block = uiLayoutGetBlock(&row);
  eUIEmbossType previous_emboss = UI_block_emboss_get(block);

  uiLayoutRow(&row, false);
  /* Enable emboss for the text button. */
  UI_block_emboss_set(block, UI_EMBOSS);

  AbstractViewItem::add_rename_button(*block);

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

std::optional<bool> AbstractTreeViewItem::should_be_active() const
{
  return std::nullopt;
}

bool AbstractTreeViewItem::supports_collapsing() const
{
  return true;
}

StringRef AbstractTreeViewItem::get_rename_string() const
{
  return label_;
}

bool AbstractTreeViewItem::rename(StringRefNull new_name)
{
  /* It is important to update the label after renaming, so #AbstractTreeViewItem::matches_single()
   * recognizes the item. (It only compares labels by default.) */
  label_ = new_name;
  return true;
}

void AbstractTreeViewItem::update_from_old(const AbstractViewItem &old)
{
  AbstractViewItem::update_from_old(old);

  const AbstractTreeViewItem &old_tree_item = dynamic_cast<const AbstractTreeViewItem &>(old);
  is_open_ = old_tree_item.is_open_;
}

bool AbstractTreeViewItem::matches_single(const AbstractTreeViewItem &other) const
{
  return label_ == other.label_;
}

std::unique_ptr<DropTargetInterface> AbstractTreeViewItem::create_item_drop_target()
{
  return create_drop_target();
}

std::unique_ptr<TreeViewItemDropTarget> AbstractTreeViewItem::create_drop_target()
{
  return nullptr;
}

AbstractTreeView &AbstractTreeViewItem::get_tree_view() const
{
  return dynamic_cast<AbstractTreeView &>(get_view());
}

std::optional<rctf> AbstractTreeViewItem::get_win_rect(const ARegion &region) const
{
  uiButViewItem *item_but = view_item_button();
  if (!item_but) {
    return std::nullopt;
  }

  rctf win_rect;
  ui_block_to_window_rctf(&region, item_but->block, &win_rect, &item_but->rect);

  return win_rect;
}

int AbstractTreeViewItem::count_parents() const
{
  int i = 0;
  for (AbstractTreeViewItem *parent = parent_; parent; parent = parent->parent_) {
    i++;
  }
  return i;
}

bool AbstractTreeViewItem::set_state_active()
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "Item activation can't be done until reconstruction is completed");

  if (!is_activatable_) {
    return false;
  }
  if (is_active()) {
    return false;
  }

  /* Deactivate other items in the tree. */
  get_tree_view().foreach_item([](auto &item) { item.deactivate(); });
  /* Make sure the active item is always visible. */
  ensure_parents_uncollapsed();

  is_active_ = true;
  return true;
}

void AbstractTreeViewItem::activate()
{
  if (set_state_active()) {
    on_activate();
  }
}

void AbstractTreeViewItem::deactivate()
{
  is_active_ = false;
}

bool AbstractTreeViewItem::is_hovered() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  BLI_assert_msg(view_item_but_ != nullptr,
                 "Hovered state can't be queried before the tree row is being built");

  const uiViewItemHandle *this_item_handle = reinterpret_cast<const uiViewItemHandle *>(this);
  /* The new layout hasn't finished construction yet, so the final state of the button is unknown.
   * Get the matching button from the previous redraw instead. */
  uiButViewItem *old_item_but = ui_block_view_find_matching_view_item_but_in_old_block(
      view_item_but_->block, this_item_handle);
  return old_item_but && (old_item_but->flag & UI_ACTIVE);
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
  if (children_.is_empty()) {
    return false;
  }
  return this->supports_collapsing();
}

void AbstractTreeViewItem::ensure_parents_uncollapsed()
{
  for (AbstractTreeViewItem *parent = parent_; parent; parent = parent->parent_) {
    parent->set_collapsed(false);
  }
}

bool AbstractTreeViewItem::matches(const AbstractViewItem &other) const
{
  const AbstractTreeViewItem &other_tree_item = dynamic_cast<const AbstractTreeViewItem &>(other);

  if (!matches_single(other_tree_item)) {
    return false;
  }
  if (count_parents() != other_tree_item.count_parents()) {
    return false;
  }

  for (AbstractTreeViewItem *parent = parent_, *other_parent = other_tree_item.parent_;
       parent && other_parent;
       parent = parent->parent_, other_parent = other_parent->parent_)
  {
    if (!parent->matches_single(*other_parent)) {
      return false;
    }
  }

  return true;
}

void AbstractTreeViewItem::change_state_delayed()
{
  const std::optional<bool> should_be_active = this->should_be_active();
  if (should_be_active.has_value() && *should_be_active) {
    /* Don't call #activate() here, since this reflects an external state change and therefore
     * shouldn't call #on_activate(). */
    set_state_active();
  }
}

/* ---------------------------------------------------------------------- */

class TreeViewLayoutBuilder {
  uiBlock &block_;

  friend TreeViewBuilder;

 public:
  void build_from_tree(const AbstractTreeView &tree_view);
  void build_row(AbstractTreeViewItem &item) const;

  uiBlock &block() const;
  uiLayout &current_layout() const;

 private:
  /* Created through #TreeViewBuilder (friend class). */
  TreeViewLayoutBuilder(uiLayout &layout);

  static void polish_layout(const uiBlock &block);
};

TreeViewLayoutBuilder::TreeViewLayoutBuilder(uiLayout &layout) : block_(*uiLayoutGetBlock(&layout))
{
}

void TreeViewLayoutBuilder::build_from_tree(const AbstractTreeView &tree_view)
{
  uiLayout &parent_layout = current_layout();

  uiLayout *box = uiLayoutBox(&parent_layout);
  uiLayoutColumn(box, false);

  tree_view.foreach_item([this](AbstractTreeViewItem &item) { build_row(item); },
                         AbstractTreeView::IterOptions::SkipCollapsed |
                             AbstractTreeView::IterOptions::SkipFiltered);

  UI_block_layout_set_current(&block(), &parent_layout);
}

void TreeViewLayoutBuilder::polish_layout(const uiBlock &block)
{
  LISTBASE_FOREACH_BACKWARD (uiBut *, but, &block.buttons) {
    if (AbstractTreeViewItem::is_collapse_chevron_but(but) && but->next &&
        /* Embossed buttons with padding-less text padding look weird, so don't touch them. */
        ELEM(but->next->emboss, UI_EMBOSS_NONE, UI_EMBOSS_NONE_OR_STATUS))
    {
      UI_but_drawflag_enable(static_cast<uiBut *>(but->next), UI_BUT_NO_TEXT_PADDING);
    }

    if (but->type == UI_BTYPE_VIEW_ITEM) {
      break;
    }
  }
}

void TreeViewLayoutBuilder::build_row(AbstractTreeViewItem &item) const
{
  uiBlock &block_ = block();

  uiLayout &prev_layout = current_layout();
  eUIEmbossType previous_emboss = UI_block_emboss_get(&block_);

  uiLayout *overlap = uiLayoutOverlap(&prev_layout);

  if (!item.is_interactive_) {
    uiLayoutSetActive(overlap, false);
  }

  uiLayout *row = uiLayoutRow(overlap, false);
  /* Enable emboss for mouse hover highlight. */
  uiLayoutSetEmboss(row, UI_EMBOSS);
  /* Every item gets one! Other buttons can be overlapped on top. */
  item.add_treerow_button(block_);

  /* After adding tree-row button (would disable hover highlighting). */
  UI_block_emboss_set(&block_, UI_EMBOSS_NONE);

  row = uiLayoutRow(overlap, true);
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
  UI_block_layout_set_current(&block_, &prev_layout);
}

uiBlock &TreeViewLayoutBuilder::block() const
{
  return block_;
}

uiLayout &TreeViewLayoutBuilder::current_layout() const
{
  return *block().curlayout;
}

/* ---------------------------------------------------------------------- */

void TreeViewBuilder::ensure_min_rows_items(AbstractTreeView &tree_view)
{
  int tot_visible_items = 0;
  tree_view.foreach_item(
      [&tot_visible_items](AbstractTreeViewItem & /*item*/) { tot_visible_items++; },
      AbstractTreeView::IterOptions::SkipCollapsed | AbstractTreeView::IterOptions::SkipFiltered);

  if (tot_visible_items >= tree_view.min_rows_) {
    return;
  }

  for (int i = 0; i < (tree_view.min_rows_ - tot_visible_items); i++) {
    BasicTreeViewItem &new_item = tree_view.add_tree_item<BasicTreeViewItem>("");
    new_item.disable_interaction();
  }
}

void TreeViewBuilder::build_tree_view(AbstractTreeView &tree_view, uiLayout &layout)
{
  uiBlock &block = *uiLayoutGetBlock(&layout);

  tree_view.build_tree();
  tree_view.update_from_old(block);
  tree_view.change_state_delayed();

  ensure_min_rows_items(tree_view);

  /* Ensure the given layout is actually active. */
  UI_block_layout_set_current(&block, &layout);

  TreeViewLayoutBuilder builder(layout);
  builder.build_from_tree(tree_view);
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

void BasicTreeViewItem::set_on_activate_fn(ActivateFn fn)
{
  activate_fn_ = fn;
}

void BasicTreeViewItem::set_is_active_fn(IsActiveFn is_active_fn)
{
  is_active_fn_ = is_active_fn;
}

std::optional<bool> BasicTreeViewItem::should_be_active() const
{
  if (is_active_fn_) {
    return is_active_fn_();
  }
  return std::nullopt;
}

}  // namespace blender::ui

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "interface_intern.hh"

#include "UI_interface_layout.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_multi_value_map.hh"
#include "BLI_string.h"

#include "UI_tree_view.hh"

namespace blender::ui {

#define UI_TREEVIEW_INDENT short(0.7f * UI_UNIT_X)
#define MIN_ROWS 4

static int unpadded_item_height()
{
  return UI_UNIT_Y;
}
static int padded_item_height()
{
  const uiStyle *style = UI_style_get_dpi();
  return unpadded_item_height() + style->buttonspacey;
}

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
    if (flag_is_set(options, IterOptions::SkipFiltered) && !child->is_filtered_visible()) {
      skip = true;
    }

    if (!skip) {
      iter_fn(*child);
    }

    if (flag_is_set(options, IterOptions::SkipCollapsed) && child->is_collapsed()) {
      continue;
    }

    child->foreach_item_recursive(iter_fn, options);
  }
}

void TreeViewItemContainer::foreach_parent(ItemIterFn iter_fn) const
{
  for (ui::AbstractTreeViewItem *item = parent_; item; item = item->parent_) {
    iter_fn(*item);
  }
}

/* ---------------------------------------------------------------------- */

void AbstractTreeView::foreach_view_item(FunctionRef<void(AbstractViewItem &)> iter_fn) const
{
  /* Implementation for the base class virtual function. More specialized iterators below. */

  this->foreach_item_recursive(iter_fn);
}

void AbstractTreeView::foreach_item(ItemIterFn iter_fn, IterOptions options) const
{
  this->foreach_item_recursive(iter_fn, options);
}

void AbstractTreeView::foreach_root_item(ItemIterFn iter_fn) const
{
  for (const auto &child : children_) {
    iter_fn(*child);
  }
}

AbstractTreeViewItem *AbstractTreeView::find_hovered(const ARegion &region, const int2 &xy)
{
  AbstractTreeViewItem *hovered_item = nullptr;
  this->foreach_item_recursive(
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

void AbstractTreeView::set_default_rows(int default_rows)
{
  BLI_assert_msg(default_rows >= MIN_ROWS,
                 "Default value is smaller than the minimum rows. Limit is required to prevent "
                 "resizing below specific height.");
  custom_height_ = std::make_unique<int>(default_rows * padded_item_height());
}

void AbstractTreeView::toggle_show_display_options()
{
  show_display_options_ = !show_display_options_;
}

std::optional<uiViewState> AbstractTreeView::persistent_state() const
{
  uiViewState state{};

  SET_FLAG_FROM_TEST(state.flag, show_display_options_, UI_VIEW_SHOW_FILTER_OPTIONS);
  BLI_strncpy(state.search_string, search_string_.get(), sizeof(state.search_string));

  if (!custom_height_ && !scroll_value_) {
    return {};
  }

  if (custom_height_) {
    state.custom_height = *custom_height_ * UI_INV_SCALE_FAC;
  }
  if (scroll_value_) {
    state.scroll_offset = *scroll_value_;
  }

  return state;
}

void AbstractTreeView::persistent_state_apply(const uiViewState &state)
{
  if (state.custom_height) {
    set_default_rows(std::max(
        MIN_ROWS, round_fl_to_int(state.custom_height * UI_SCALE_FAC) / padded_item_height()));
  }
  if (state.scroll_offset) {
    scroll_value_ = std::make_shared<int>(state.scroll_offset);
  }

  show_display_options_ = (state.flag & UI_VIEW_SHOW_FILTER_OPTIONS) != 0;
  BLI_strncpy(search_string_.get(), state.search_string, UI_MAX_NAME_STR);
}

int AbstractTreeView::count_visible_descendants(const AbstractTreeViewItem &parent) const
{
  if (parent.is_collapsed()) {
    return 0;
  }
  int count = 0;
  for (const auto &item : parent.children_) {
    if (!item->is_filtered_visible()) {
      continue;
    }
    count++;
    count += count_visible_descendants(*item);
  }

  return count;
}

void AbstractTreeView::get_hierarchy_lines(const ARegion &region,
                                           const TreeViewOrItem &parent,
                                           const float aspect,
                                           Vector<std::pair<int2, int2>> &lines,
                                           int &visible_item_index) const
{
  const int scroll_ofs = scroll_value_ ? *scroll_value_ : 0;
  const int max_visible_row_count = tot_visible_row_count().value_or(
      std::numeric_limits<int>::max());

  for (const auto &item : parent.children_) {
    if (!item->is_filtered_visible()) {
      continue;
    }

    const int item_index = visible_item_index;
    visible_item_index++;

    if (!item->is_collapsible() || item->is_collapsed()) {
      continue;
    }
    if (item->children_.is_empty()) {
      BLI_assert(item->is_always_collapsible_);
      continue;
    }

    /* Draw a hierarchy line for the descendants of this item. */

    const AbstractTreeViewItem *first_descendant = item->children_.first().get();
    const int descendant_count = count_visible_descendants(*item);

    const int first_descendant_index = item_index + 1;
    const int last_descendant_index = item_index + descendant_count;

    {
      const bool line_ends_above_visible = last_descendant_index < scroll_ofs;
      if (line_ends_above_visible) {
        /* We won't recurse into the child items even though they are present (just scrolled out of
         * view). Still update the index to be the first following item. */
        visible_item_index = last_descendant_index + 1;
        continue;
      }

      const bool line_starts_below_visible = first_descendant_index >
                                             (scroll_ofs + long(max_visible_row_count));
      /* Can return here even, following items won't be in view anymore. */
      if (line_starts_below_visible) {
        return;
      }
    }

    const int x = ((first_descendant->indent_width() + uiLayoutListItemPaddingWidth() -
                    (0.5f * UI_ICON_SIZE) + U.pixelsize + UI_SCALE_FAC) /
                   aspect);
    const int ymax = std::max(0, first_descendant_index - scroll_ofs) * padded_item_height() /
                     aspect;
    const int ymin = std::min(max_visible_row_count, last_descendant_index + 1 - scroll_ofs) *
                     padded_item_height() / aspect;
    lines.append(std::make_pair(int2(x, ymax), int2(x, ymin)));

    this->get_hierarchy_lines(region, *item, aspect, lines, visible_item_index);
  }
}

static uiButViewItem *find_first_view_item_but(const uiBlock &block, const AbstractTreeView &view)
{
  for (const std::unique_ptr<uiBut> &but : block.buttons) {
    if (but->type != ButType::ViewItem) {
      continue;
    }
    uiButViewItem *view_item_but = static_cast<uiButViewItem *>(but.get());
    if (&view_item_but->view_item->get_view() == &view) {
      return view_item_but;
    }
  }
  return nullptr;
}

void AbstractTreeView::draw_hierarchy_lines(const ARegion &region, const uiBlock &block) const
{
  const float aspect = (region.v2d.flag & V2D_IS_INIT) ?
                           BLI_rctf_size_y(&region.v2d.cur) /
                               (BLI_rcti_size_y(&region.v2d.mask) + 1) :
                           1.0f;

  uiButViewItem *first_item_but = find_first_view_item_but(block, *this);
  if (!first_item_but) {
    return;
  }

  Vector<std::pair<int2, int2>> lines;
  int index = 0;
  get_hierarchy_lines(region, *this, aspect, lines, index);
  if (lines.is_empty()) {
    return;
  }

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", blender::gpu::VertAttrType::SFLOAT_32_32);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColorAlpha(TH_TEXT, 0.2f);

  GPU_line_width(1.0f / aspect);
  GPU_blend(GPU_BLEND_ALPHA);

  rcti first_item_but_pixel_rect;
  ui_but_to_pixelrect(&first_item_but_pixel_rect, &region, &block, first_item_but);
  int2 top_left{first_item_but_pixel_rect.xmin, first_item_but_pixel_rect.ymax};

  for (const auto &line : lines) {
    immBegin(GPU_PRIM_LINES, 2);
    immVertex2f(pos, top_left.x + line.first.x, top_left.y - line.first.y);
    immVertex2f(pos, top_left.x + line.second.x, top_left.y - line.second.y);
    immEnd();
  }
  GPU_blend(GPU_BLEND_NONE);

  immUnbindProgram();
}

void AbstractTreeView::draw_overlays(const ARegion &region, const uiBlock &block) const
{
  this->draw_hierarchy_lines(region, block);
}

void AbstractTreeView::update_children_from_old(const AbstractView &old_view)
{
  const AbstractTreeView &old_tree_view = dynamic_cast<const AbstractTreeView &>(old_view);

  custom_height_ = old_tree_view.custom_height_;
  scroll_value_ = old_tree_view.scroll_value_;
  search_string_ = old_tree_view.search_string_;
  show_display_options_ = old_tree_view.show_display_options_;
  update_children_from_old_recursive(*this, old_tree_view);
}

void AbstractTreeView::update_children_from_old_recursive(const TreeViewOrItem &new_items,
                                                          const TreeViewOrItem &old_items)
{
  /* This map can't find the exact old item for a new item. However, it can drastically reduce the
   * number of items that need to be checked. */
  MultiValueMap<StringRef, AbstractTreeViewItem *> old_children_by_label;
  for (const auto &old_item : old_items.children_) {
    old_children_by_label.add(old_item->label_, old_item.get());
  }

  for (const auto &new_item : new_items.children_) {
    const Span<AbstractTreeViewItem *> possible_old_children = old_children_by_label.lookup(
        new_item->label_);
    AbstractTreeViewItem *matching_old_item = find_matching_child(*new_item,
                                                                  possible_old_children);
    if (!matching_old_item) {
      continue;
    }

    new_item->update_from_old(*matching_old_item);

    /* Recurse into children of the matched item. */
    update_children_from_old_recursive(*new_item, *matching_old_item);
  }
}

AbstractTreeViewItem *AbstractTreeView::find_matching_child(
    const AbstractTreeViewItem &lookup_item, const Span<AbstractTreeViewItem *> possible_items)
{
  for (auto *iter_item : possible_items) {
    if (lookup_item.matches(*iter_item)) {
      /* We have a matching item! */
      return iter_item;
    }
  }

  return nullptr;
}

std::optional<int> AbstractTreeView::tot_visible_row_count() const
{
  if (!custom_height_) {
    return {};
  }
  const int calculate_rows = round_fl_to_int(float(*custom_height_) / padded_item_height());
  /* Clamp value to prevent resizing below minimum number of rows. */
  return math::max(MIN_ROWS, calculate_rows);
}

bool AbstractTreeView::supports_scrolling() const
{
  return custom_height_ && scroll_value_;
}

bool AbstractTreeView::is_fully_visible() const
{
  return this->tot_visible_row_count().value_or(0) >= last_tot_items_;
}

void AbstractTreeView::scroll(ViewScrollDirection direction)
{
  if (!supports_scrolling()) {
    return;
  }
  /* Scroll value will be sanitized/clamped when drawing. */
  *scroll_value_ += ((direction == ViewScrollDirection::UP) ? -1 : 1);
}

void AbstractTreeView::scroll_active_into_view()
{
  int index = 0;
  const std::optional<int> visible_row_count = tot_visible_row_count();

  if (!custom_height_) {
    return;
  }

  if (!visible_row_count.has_value()) {
    return;
  }

  if (scroll_active_into_view_on_draw_) {
    if (!scroll_value_) {
      scroll_value_ = std::make_unique<int>(0);
    }
    foreach_item(
        [&, this](AbstractTreeViewItem &item) {
          if (item.is_active_) {
            *scroll_value_ = std::max(0, index - *visible_row_count + 1);
            return;
          }
          index++;
        },
        AbstractTreeView::IterOptions::SkipCollapsed |
            AbstractTreeView::IterOptions::SkipFiltered);
  }
}

/* ---------------------------------------------------------------------- */

TreeViewItemDropTarget::TreeViewItemDropTarget(AbstractTreeViewItem &view_item,
                                               DropBehavior behavior)
    : view_item_(view_item), behavior_(behavior)
{
}

std::optional<DropLocation> TreeViewItemDropTarget::choose_drop_location(
    const ARegion &region, const wmEvent &event) const
{
  if (behavior_ == DropBehavior::Insert) {
    return DropLocation::Into;
  }

  std::optional<rctf> win_rect = view_item_.get_win_rect(region);
  if (!win_rect) {
    BLI_assert_unreachable();
    return std::nullopt;
  }
  const float item_height = BLI_rctf_size_y(&*win_rect);

  BLI_assert(ELEM(behavior_, DropBehavior::Reorder, DropBehavior::ReorderAndInsert));

  const int segment_count =
      (behavior_ == DropBehavior::Reorder) ?
          /* Divide into upper (insert before) and lower (insert after) half. */
          2 :
          /* Upper (insert before), middle (insert into) and lower (insert after) third. */
          3;
  const float segment_height = item_height / segment_count;

  if (event.xy[1] < win_rect->ymin) {
    return DropLocation::After;
  }
  if (event.xy[1] - win_rect->ymin > (item_height - segment_height)) {
    return DropLocation::Before;
  }
  if (event.xy[1] - win_rect->ymin <= segment_height) {
    if (behavior_ == DropBehavior::ReorderAndInsert && view_item_.is_collapsible() &&
        !view_item_.is_collapsed())
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

void AbstractTreeViewItem::add_treerow_button(uiBlock &block)
{
  /* For some reason a width > (UI_UNIT_X * 2) make the layout system use all available width. */
  view_item_but_ = reinterpret_cast<uiButViewItem *>(uiDefBut(&block,
                                                              ButType::ViewItem,
                                                              0,
                                                              "",
                                                              0,
                                                              0,
                                                              UI_UNIT_X * 10,
                                                              padded_item_height(),
                                                              nullptr,
                                                              0,
                                                              0,
                                                              ""));

  view_item_but_->view_item = this;
  view_item_but_->draw_height = unpadded_item_height();
}

int AbstractTreeViewItem::indent_width() const
{
  return this->count_parents() * UI_TREEVIEW_INDENT;
}

void AbstractTreeViewItem::add_indent(uiLayout &row) const
{
  uiBlock *block = row.block();
  uiLayout *subrow = &row.row(true);
  subrow->fixed_size_set(true);

  uiDefBut(block, ButType::Sepr, 0, "", 0, 0, this->indent_width(), 0, nullptr, 0.0, 0.0, "");

  const bool is_flat_list = root_ && root_->is_flat_;
  if (!is_flat_list && !this->is_collapsible()) {
    /* Indent items without collapsing icon some more within their parent. Makes it clear that they
     * are actually nested and not just a row at the same level without a chevron. */
    uiDefBut(block, ButType::Sepr, 0, "", 0, 0, UI_TREEVIEW_INDENT, 0, nullptr, 0.0, 0.0, "");
  }

  /* Restore. */
  block_layout_set_current(block, &row);
}

void AbstractTreeViewItem::collapse_chevron_click_fn(bContext *C,
                                                     void * /*but_arg1*/,
                                                     void * /*arg2*/)
{
  /* There's no data we could pass to this callback. It must be either the button itself or a
   * consistent address to match buttons over redraws. So instead of passing it somehow, just
   * lookup the hovered item via context here. */

  const wmWindow *win = CTX_wm_window(C);
  const ARegion *region = CTX_wm_region_popup(C) ? CTX_wm_region_popup(C) : CTX_wm_region(C);
  AbstractViewItem *hovered_abstract_item = UI_region_views_find_item_at(*region,
                                                                         win->eventstate->xy);

  auto *hovered_item = reinterpret_cast<AbstractTreeViewItem *>(hovered_abstract_item);
  BLI_assert(hovered_item != nullptr);

  hovered_item->toggle_collapsed_from_view(*C);
  /* When collapsing an item with an active child, make this collapsed item active instead so the
   * active item stays visible. */
  if (hovered_item->has_active_child()) {
    hovered_item->activate(*C);
  }
}

void AbstractTreeViewItem::add_collapse_chevron(uiBlock &block) const
{
  if (!this->is_collapsible()) {
    return;
  }

  const BIFIconID icon = this->is_collapsed() ? ICON_RIGHTARROW : ICON_DOWNARROW_HLT;
  uiBut *but = uiDefIconBut(
      &block, ButType::ButToggle, 0, icon, 0, 0, UI_TREEVIEW_INDENT, UI_UNIT_Y, nullptr, 0, 0, "");
  UI_but_func_set(but, collapse_chevron_click_fn, nullptr, nullptr);
  UI_but_flag_disable(but, UI_BUT_UNDO);
}

void AbstractTreeViewItem::add_rename_button(uiLayout &row)
{
  uiBlock *block = row.block();
  EmbossType previous_emboss = UI_block_emboss_get(block);

  row.row(false);
  /* Enable emboss for the text button. */
  UI_block_emboss_set(block, EmbossType::Emboss);

  AbstractViewItem::add_rename_button(*block);

  UI_block_emboss_set(block, previous_emboss);
  block_layout_set_current(block, &row);
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

bool AbstractTreeViewItem::supports_collapsing() const
{
  return true;
}

StringRef AbstractTreeViewItem::get_rename_string() const
{
  return label_;
}

bool AbstractTreeViewItem::rename(const bContext & /*C*/, StringRefNull new_name)
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
  return this->create_drop_target();
}

std::unique_ptr<TreeViewItemDropTarget> AbstractTreeViewItem::create_drop_target()
{
  return nullptr;
}

std::optional<std::string> AbstractTreeViewItem::debug_name() const
{
  return label_;
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
  if (AbstractViewItem::set_state_active()) {
    /* Make sure the active item is always visible. */
    ensure_parents_uncollapsed();
    return true;
  }

  return false;
}

bool AbstractTreeViewItem::is_hovered() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State cannot be queried until reconstruction is completed");
  BLI_assert_msg(view_item_but_ != nullptr,
                 "Hovered state cannot be queried before the tree row is being built");

  /* The new layout hasn't finished construction yet, so the final state of the button is unknown.
   * Get the matching button from the previous redraw instead. */
  uiButViewItem *old_item_but = ui_block_view_find_matching_view_item_but_in_old_block(
      *view_item_but_->block, *this);
  return old_item_but && (old_item_but->flag & UI_HOVER);
}

bool AbstractTreeViewItem::is_collapsed() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State cannot be queried until reconstruction is completed");
  return this->is_collapsible() && !is_open_;
}

bool AbstractTreeViewItem::toggle_collapsed()
{
  return this->set_collapsed(is_open_);
}

void AbstractTreeViewItem::toggle_collapsed_from_view(bContext &C)
{
  if (this->toggle_collapsed()) {
    this->on_collapse_change(C, this->is_collapsed());
  }
}

bool AbstractTreeViewItem::set_collapsed(const bool collapsed)
{
  if (!this->is_collapsible()) {
    return false;
  }
  if (collapsed == !is_open_) {
    return false;
  }

  is_open_ = !collapsed;
  return true;
}

void AbstractTreeViewItem::on_collapse_change(bContext & /*C*/, const bool /*is_collapsed*/)
{
  /* Do nothing by default. */
}

std::optional<bool> AbstractTreeViewItem::should_be_collapsed() const
{
  return std::nullopt;
}

void AbstractTreeViewItem::uncollapse_by_default()
{
  BLI_assert_msg(this->get_tree_view().is_reconstructed() == false,
                 "Default state should only be set while building the tree");
  BLI_assert(this->supports_collapsing());
  /* Set the open state. Note that this may be overridden later by #should_be_collapsed(). */
  is_open_ = true;
}

bool AbstractTreeViewItem::is_collapsible() const
{
  BLI_assert_msg(get_tree_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  if (is_always_collapsible_) {
    return true;
  }
  if (children_.is_empty()) {
    return false;
  }
  return this->supports_collapsing();
}

void AbstractTreeViewItem::change_state_delayed()
{
  const bool prev_active_state = is_active();
  AbstractViewItem::change_state_delayed();

  if (prev_active_state != is_active()) {
    this->get_tree_view().scroll_active_into_view_on_draw_ = true;
  }

  const std::optional<bool> should_be_collapsed = this->should_be_collapsed();
  if (should_be_collapsed.has_value()) {
    /* This reflects an external state change and therefore shouldn't call #on_collapse_change().
     */
    this->set_collapsed(*should_be_collapsed);
  }
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

  if (!this->matches_single(other_tree_item)) {
    return false;
  }
  if (this->count_parents() != other_tree_item.count_parents()) {
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

void AbstractTreeViewItem::on_filter()
{
  BLI_assert(this->get_tree_view().search_string_ && this->get_tree_view().search_string_[0]);

  if (is_filtered_visible_) {
    foreach_parent([&](AbstractTreeViewItem &item) {
      item.is_filtered_visible_ = true;
      item.set_collapsed(false);
    });
  }
}

/* ---------------------------------------------------------------------- */

class TreeViewLayoutBuilder {
  uiBlock &block_;
  bool add_box_ = true;

  friend TreeViewBuilder;

 public:
  void build_from_tree(AbstractTreeView &tree_view);
  void build_row(AbstractTreeViewItem &item) const;

  uiBlock &block() const;
  uiLayout &current_layout() const;

 private:
  /* Created through #TreeViewBuilder (friend class). */
  TreeViewLayoutBuilder(uiLayout &layout);
};

TreeViewLayoutBuilder::TreeViewLayoutBuilder(uiLayout &layout) : block_(*layout.block()) {}

static int count_visible_items(AbstractTreeView &tree_view)
{
  int item_count = 0;
  tree_view.foreach_item([&](AbstractTreeViewItem &) { item_count++; },
                         AbstractTreeView::IterOptions::SkipCollapsed |
                             AbstractTreeView::IterOptions::SkipFiltered);
  return item_count;
}

static void set_filtering_collapsed_fn(bContext *C, void * /*but_arg1*/, void * /*arg2*/)
{
  const wmWindow *win = CTX_wm_window(C);
  if (!(win && win->eventstate)) {
    return;
  }
  const ARegion *region = CTX_wm_region(C);
  if (!region) {
    return;
  }

  if (AbstractView *view = UI_region_view_find_at(region, win->eventstate->xy, 2 * UI_UNIT_Y)) {
    if (AbstractTreeView *tree_view = dynamic_cast<AbstractTreeView *>(view)) {
      tree_view->toggle_show_display_options();
    }
  }
}

void TreeViewLayoutBuilder::build_from_tree(AbstractTreeView &tree_view)
{
  uiLayout &parent_layout = this->current_layout();
  uiBlock *block = parent_layout.block();

  uiLayout *col = nullptr;
  if (add_box_) {
    uiLayout *box = &parent_layout.box();
    col = &box->column(true);
  }
  else {
    col = &parent_layout.column(true);
  }
  /* Row for the tree-view and the scroll bar. */
  uiLayout *row = &col->row(false);

  const std::optional<int> visible_row_count = tree_view.tot_visible_row_count();
  const int tot_items = count_visible_items(tree_view);
  tree_view.last_tot_items_ = tot_items;

  /* Column for the tree view. */
  row->column(true);

  if (tree_view.scroll_active_into_view_on_draw_) {
    tree_view.scroll_active_into_view();
  }

  /* Clamp scroll-value to valid range. */
  if (tree_view.scroll_value_ && visible_row_count) {
    *tree_view.scroll_value_ = std::clamp(
        *tree_view.scroll_value_, 0, tot_items - *visible_row_count);
  }

  const int first_visible_index = tree_view.scroll_value_ ? *tree_view.scroll_value_ : 0;
  const int max_visible_index = visible_row_count ? first_visible_index + *visible_row_count - 1 :
                                                    std::numeric_limits<int>::max();
  int index = 0;
  tree_view.foreach_item(
      [&, this](AbstractTreeViewItem &item) {
        if ((index >= first_visible_index) && (index <= max_visible_index)) {
          if (item.is_filtered_visible()) {
            this->build_row(item);
          }
        }
        index++;
      },
      AbstractTreeView::IterOptions::SkipCollapsed | AbstractTreeView::IterOptions::SkipFiltered);

  if (tree_view.custom_height_) {

    *tree_view.custom_height_ = visible_row_count.value_or(1) * padded_item_height();
    if (!tree_view.scroll_value_) {
      tree_view.scroll_value_ = std::make_unique<int>(0);
    }

    if (visible_row_count && (tot_items > *visible_row_count)) {
      row->column(false);
      uiBut *but = uiDefButI(block,
                             ButType::Scroll,
                             0,
                             "",
                             0,
                             0,
                             V2D_SCROLL_WIDTH,
                             *tree_view.custom_height_,
                             tree_view.scroll_value_.get(),
                             0,
                             tot_items - *visible_row_count,
                             "");
      uiButScrollBar *but_scroll = reinterpret_cast<uiButScrollBar *>(but);
      but_scroll->visual_height = *visible_row_count;
    }

    block_layout_set_current(block, col);

    /* Bottom */
    uiLayout *bottom = &col->row(false);
    UI_block_emboss_set(block, ui::EmbossType::None);
    int icon = tree_view.show_display_options_ ? ICON_DISCLOSURE_TRI_DOWN :
                                                 ICON_DISCLOSURE_TRI_RIGHT;
    uiBut *but = uiDefIconBut(
        block, ButType::Toggle, 0, icon, 0, 0, UI_UNIT_X, UI_UNIT_Y * 0.3, nullptr, 0, 0, "");
    UI_but_func_set(but, set_filtering_collapsed_fn, nullptr, nullptr);
    UI_block_emboss_set(block, ui::EmbossType::Emboss);
    bottom->column(false);

    uiDefIconButI(block,
                  ButType::Grip,
                  0,
                  ICON_GRIP,
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y * 0.5f,
                  tree_view.custom_height_.get(),
                  0,
                  0,
                  "");

    if (tree_view.show_display_options_) {
      block_layout_set_current(block, col);
      uiBut *but = uiDefBut(block,
                            ButType::Text,
                            1,
                            "",
                            0,
                            0,
                            UI_TREEVIEW_INDENT,
                            UI_UNIT_Y,
                            tree_view.search_string_.get(),
                            0,
                            UI_MAX_NAME_STR,
                            "");
      UI_but_flag_enable(but, UI_BUT_TEXTEDIT_UPDATE | UI_BUT_VALUE_CLEAR);
      UI_but_flag_enable(but, UI_BUT_UNDO);
      ui_def_but_icon(but, ICON_VIEWZOOM, UI_HAS_ICON);
    }
  }

  block_layout_set_current(block, &parent_layout);
}

void TreeViewLayoutBuilder::build_row(AbstractTreeViewItem &item) const
{
  uiBlock &block_ = block();

  uiLayout &prev_layout = current_layout();

  const int width = prev_layout.width();
  if (width < int(40 * UI_SCALE_FAC)) {
    return;
  }

  EmbossType previous_emboss = UI_block_emboss_get(&block_);

  uiLayout *overlap = &prev_layout.overlap();

  if (!item.is_interactive_) {
    overlap->active_set(false);
  }

  uiLayout *row = &overlap->row(false);
  /* Enable emboss for mouse hover highlight. */
  row->emboss_set(EmbossType::Emboss);
  /* Every item gets one! Other buttons can be overlapped on top. */
  item.add_treerow_button(block_);

  /* After adding tree-row button (would disable hover highlighting). */
  UI_block_emboss_set(&block_, EmbossType::NoneOrStatus);

  /* Add little margin to align actual contents vertically. */
  uiLayout *content_col = &overlap->column(true);
  const int margin_top = (padded_item_height() - unpadded_item_height()) / 2;
  if (margin_top > 0) {
    uiDefBut(&block_, ButType::Label, 0, "", 0, 0, UI_UNIT_X, margin_top, nullptr, 0, 0, "");
  }
  row = &content_col->row(true);

  uiLayoutListItemAddPadding(row);
  item.add_indent(*row);
  item.add_collapse_chevron(block_);

  if (item.is_renaming()) {
    item.add_rename_button(*row);
  }
  else {
    item.build_row(*row);
    if (item.is_active_) {
      ui_layout_list_set_labels_active(row);
    }
  }

  uiLayoutListItemAddPadding(row);

  UI_block_emboss_set(&block_, previous_emboss);
  block_layout_set_current(&block_, &prev_layout);
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
  const std::optional<int> visible_rows = tree_view.tot_visible_row_count();
  if (!visible_rows) {
    return;
  }

  int tot_visible_items = 0;
  tree_view.foreach_item(
      [&tot_visible_items](AbstractTreeViewItem & /*item*/) { tot_visible_items++; },
      AbstractTreeView::IterOptions::SkipCollapsed | AbstractTreeView::IterOptions::SkipFiltered);

  if (tot_visible_items >= *visible_rows) {
    return;
  }

  for (int i = 0; i < (*visible_rows - tot_visible_items); i++) {
    BasicTreeViewItem &new_item = tree_view.add_tree_item<BasicTreeViewItem>("");
    new_item.disable_interaction();
  }
}

void TreeViewBuilder::build_tree_view(const bContext &C,
                                      AbstractTreeView &tree_view,
                                      uiLayout &layout,
                                      const bool add_box)
{
  uiBlock &block = *layout.block();

  const ARegion *region = CTX_wm_region_popup(&C) ? CTX_wm_region_popup(&C) : CTX_wm_region(&C);
  if (region) {
    ui_block_view_persistent_state_restore(*region, block, tree_view);
  }

  tree_view.build_tree();
  tree_view.update_from_old(block);
  tree_view.change_state_delayed();
  {
    /* Setup search string to filter out elements with matching characters. */
    char string[UI_MAX_NAME_STR];
    BLI_strncpy_ensure_pad(string, tree_view.search_string_.get(), '*', sizeof(string));
    tree_view.filter(tree_view.search_string_ ? std::optional{string} : std::nullopt);
  }
  ensure_min_rows_items(tree_view);

  /* Ensure the given layout is actually active. */
  block_layout_set_current(&block, &layout);

  TreeViewLayoutBuilder builder(layout);
  builder.add_box_ = add_box;
  UI_block_flag_enable(&block, UI_BLOCK_LIST_ITEM);
  builder.build_from_tree(tree_view);
  UI_block_flag_disable(&block, UI_BLOCK_LIST_ITEM);
}

/* ---------------------------------------------------------------------- */

BasicTreeViewItem::BasicTreeViewItem(StringRef label, BIFIconID icon_) : icon(icon_)
{
  label_ = label;
}

void BasicTreeViewItem::build_row(uiLayout &row)
{
  this->add_label(row);
}

void BasicTreeViewItem::add_label(uiLayout &layout, StringRefNull label_override)
{
  const StringRefNull label = label_override.is_empty() ? StringRefNull(label_) : label_override;
  layout.label(label, icon);
}

void BasicTreeViewItem::on_activate(bContext &C)
{
  if (activate_fn_) {
    activate_fn_(C, *this);
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

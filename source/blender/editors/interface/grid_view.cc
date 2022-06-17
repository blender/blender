/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include <limits>
#include <stdexcept>

#include "BLI_index_range.hh"

#include "WM_types.h"

#include "UI_interface.h"
#include "interface_intern.h"

#include "UI_grid_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */

AbstractGridView::AbstractGridView() : style_(UI_preview_tile_size_x(), UI_preview_tile_size_y())
{
}

AbstractGridViewItem &AbstractGridView::add_item(std::unique_ptr<AbstractGridViewItem> item)
{
  items_.append(std::move(item));

  AbstractGridViewItem &added_item = *items_.last();
  added_item.view_ = this;

  item_map_.add(added_item.identifier_, &added_item);

  return added_item;
}

void AbstractGridView::foreach_item(ItemIterFn iter_fn) const
{
  for (auto &item_ptr : items_) {
    iter_fn(*item_ptr);
  }
}

bool AbstractGridView::listen(const wmNotifier &) const
{
  /* Nothing by default. */
  return false;
}

AbstractGridViewItem *AbstractGridView::find_matching_item(
    const AbstractGridViewItem &item_to_match, const AbstractGridView &view_to_search_in) const
{
  AbstractGridViewItem *const *match = view_to_search_in.item_map_.lookup_ptr(
      item_to_match.identifier_);
  BLI_assert(!match || item_to_match.matches(**match));

  return match ? *match : nullptr;
}

void AbstractGridView::change_state_delayed()
{
  BLI_assert_msg(
      is_reconstructed(),
      "These state changes are supposed to be delayed until reconstruction is completed");
  foreach_item([](AbstractGridViewItem &item) { item.change_state_delayed(); });
}

void AbstractGridView::update_from_old(uiBlock &new_block)
{
  uiGridViewHandle *old_view_handle = ui_block_grid_view_find_matching_in_old_block(
      &new_block, reinterpret_cast<uiGridViewHandle *>(this));
  if (!old_view_handle) {
    /* Initial construction, nothing to update. */
    is_reconstructed_ = true;
    return;
  }

  AbstractGridView &old_view = reinterpret_cast<AbstractGridView &>(*old_view_handle);

  foreach_item([this, &old_view](AbstractGridViewItem &new_item) {
    const AbstractGridViewItem *matching_old_item = find_matching_item(new_item, old_view);
    if (!matching_old_item) {
      return;
    }

    new_item.update_from_old(*matching_old_item);
  });

  /* Finished (re-)constructing the tree. */
  is_reconstructed_ = true;
}

bool AbstractGridView::is_reconstructed() const
{
  return is_reconstructed_;
}

const GridViewStyle &AbstractGridView::get_style() const
{
  return style_;
}

int AbstractGridView::get_item_count() const
{
  return items_.size();
}

GridViewStyle::GridViewStyle(int width, int height) : tile_width(width), tile_height(height)
{
}

/* ---------------------------------------------------------------------- */

AbstractGridViewItem::AbstractGridViewItem(StringRef identifier) : identifier_(identifier)
{
}

bool AbstractGridViewItem::matches(const AbstractGridViewItem &other) const
{
  return identifier_ == other.identifier_;
}

void AbstractGridViewItem::grid_tile_click_fn(struct bContext * /*C*/,
                                              void *but_arg1,
                                              void * /*arg2*/)
{
  uiButGridTile *grid_tile_but = (uiButGridTile *)but_arg1;
  AbstractGridViewItem &grid_item = reinterpret_cast<AbstractGridViewItem &>(
      *grid_tile_but->view_item);

  grid_item.activate();
}

void AbstractGridViewItem::add_grid_tile_button(uiBlock &block)
{
  const GridViewStyle &style = get_view().get_style();
  grid_tile_but_ = (uiButGridTile *)uiDefBut(&block,
                                             UI_BTYPE_GRID_TILE,
                                             0,
                                             "",
                                             0,
                                             0,
                                             style.tile_width,
                                             style.tile_height,
                                             nullptr,
                                             0,
                                             0,
                                             0,
                                             0,
                                             "");

  grid_tile_but_->view_item = reinterpret_cast<uiGridViewItemHandle *>(this);
  UI_but_func_set(&grid_tile_but_->but, grid_tile_click_fn, grid_tile_but_, nullptr);
}

bool AbstractGridViewItem::is_active() const
{
  BLI_assert_msg(get_view().is_reconstructed(),
                 "State can't be queried until reconstruction is completed");
  return is_active_;
}

void AbstractGridViewItem::on_activate()
{
  /* Do nothing by default. */
}

std::optional<bool> AbstractGridViewItem::should_be_active() const
{
  return std::nullopt;
}

void AbstractGridViewItem::change_state_delayed()
{
  const std::optional<bool> should_be_active = this->should_be_active();
  if (should_be_active.has_value() && *should_be_active) {
    activate();
  }
}

void AbstractGridViewItem::update_from_old(const AbstractGridViewItem &old)
{
  is_active_ = old.is_active_;
}

void AbstractGridViewItem::activate()
{
  BLI_assert_msg(get_view().is_reconstructed(),
                 "Item activation can't be done until reconstruction is completed");

  if (is_active()) {
    return;
  }

  /* Deactivate other items in the tree. */
  get_view().foreach_item([](auto &item) { item.deactivate(); });

  on_activate();

  is_active_ = true;
}

void AbstractGridViewItem::deactivate()
{
  is_active_ = false;
}

const AbstractGridView &AbstractGridViewItem::get_view() const
{
  if (UNLIKELY(!view_)) {
    throw std::runtime_error(
        "Invalid state, item must be added through AbstractGridView::add_item()");
  }
  return *view_;
}

/* ---------------------------------------------------------------------- */

/**
 * Helper for only adding layout items for grid items that are actually in view. 3 main functions:
 * - #is_item_visible(): Query if an item of a given index is visible in the view (others should be
 * skipped when building the layout).
 * - #fill_layout_before_visible(): Add empty space to the layout before a visible row is drawn, so
 *   the layout height is the same as if all items were added (important to get the correct scroll
 *   height).
 * - #fill_layout_after_visible(): Same thing, just adds empty space for after the last visible
 *   row.
 *
 * Does two assumptions:
 * - Top-to-bottom flow (ymax = 0 and ymin < 0). If that's not good enough, View2D should
 *   probably provide queries for the scroll offset.
 * - Only vertical scrolling. For horizontal scrolling, spacers would have to be added on the
 *   side(s) as well.
 */
class BuildOnlyVisibleButtonsHelper {
  const View2D &v2d_;
  const AbstractGridView &grid_view_;
  const GridViewStyle &style_;
  const int cols_per_row_ = 0;
  /* Indices of items within the view. Calculated by constructor */
  IndexRange visible_items_range_{};

 public:
  BuildOnlyVisibleButtonsHelper(const View2D &,
                                const AbstractGridView &grid_view,
                                int cols_per_row);

  bool is_item_visible(int item_idx) const;
  void fill_layout_before_visible(uiBlock &) const;
  void fill_layout_after_visible(uiBlock &) const;

 private:
  IndexRange get_visible_range() const;
  void add_spacer_button(uiBlock &, int row_count) const;
};

BuildOnlyVisibleButtonsHelper::BuildOnlyVisibleButtonsHelper(const View2D &v2d,
                                                             const AbstractGridView &grid_view,
                                                             const int cols_per_row)
    : v2d_(v2d), grid_view_(grid_view), style_(grid_view.get_style()), cols_per_row_(cols_per_row)
{
  visible_items_range_ = get_visible_range();
}

IndexRange BuildOnlyVisibleButtonsHelper::get_visible_range() const
{
  int first_idx_in_view = 0;
  int max_items_in_view = 0;

  const float scroll_ofs_y = abs(v2d_.cur.ymax - v2d_.tot.ymax);
  if (!IS_EQF(scroll_ofs_y, 0)) {
    const int scrolled_away_rows = (int)scroll_ofs_y / style_.tile_height;

    first_idx_in_view = scrolled_away_rows * cols_per_row_;
  }

  const float view_height = BLI_rctf_size_y(&v2d_.cur);
  const int count_rows_in_view = std::max(round_fl_to_int(view_height / style_.tile_height), 1);
  max_items_in_view = (count_rows_in_view + 1) * cols_per_row_;

  BLI_assert(max_items_in_view > 0);
  return IndexRange(first_idx_in_view, max_items_in_view);
}

bool BuildOnlyVisibleButtonsHelper::is_item_visible(const int item_idx) const
{
  return visible_items_range_.contains(item_idx);
}

void BuildOnlyVisibleButtonsHelper::fill_layout_before_visible(uiBlock &block) const
{
  const float scroll_ofs_y = abs(v2d_.cur.ymax - v2d_.tot.ymax);

  if (IS_EQF(scroll_ofs_y, 0)) {
    return;
  }

  const int scrolled_away_rows = (int)scroll_ofs_y / style_.tile_height;
  add_spacer_button(block, scrolled_away_rows);
}

void BuildOnlyVisibleButtonsHelper::fill_layout_after_visible(uiBlock &block) const
{
  const int last_item_idx = grid_view_.get_item_count() - 1;
  const int last_visible_idx = visible_items_range_.last();

  if (last_item_idx > last_visible_idx) {
    const int remaining_rows = (cols_per_row_ > 0) ?
                                   (last_item_idx - last_visible_idx) / cols_per_row_ :
                                   0;
    BuildOnlyVisibleButtonsHelper::add_spacer_button(block, remaining_rows);
  }
}

void BuildOnlyVisibleButtonsHelper::add_spacer_button(uiBlock &block, const int row_count) const
{
  /* UI code only supports button dimensions of `signed short` size, the layout height we want to
   * fill may be bigger than that. So add multiple labels of the maximum size if necessary. */
  for (int remaining_rows = row_count; remaining_rows > 0;) {
    const short row_count_this_iter = std::min(
        std::numeric_limits<short>::max() / style_.tile_height, remaining_rows);

    uiDefBut(&block,
             UI_BTYPE_LABEL,
             0,
             "",
             0,
             0,
             UI_UNIT_X,
             row_count_this_iter * style_.tile_height,
             nullptr,
             0,
             0,
             0,
             0,
             "");
    remaining_rows -= row_count_this_iter;
  }
}

/* ---------------------------------------------------------------------- */

class GridViewLayoutBuilder {
  uiBlock &block_;

  friend class GridViewBuilder;

 public:
  GridViewLayoutBuilder(uiBlock &block);

  void build_from_view(const AbstractGridView &grid_view, const View2D &v2d) const;

 private:
  void build_grid_tile(uiLayout &grid_layout, AbstractGridViewItem &item) const;

  uiLayout *current_layout() const;
};

GridViewLayoutBuilder::GridViewLayoutBuilder(uiBlock &block) : block_(block)
{
}

void GridViewLayoutBuilder::build_grid_tile(uiLayout &grid_layout,
                                            AbstractGridViewItem &item) const
{
  uiLayout *overlap = uiLayoutOverlap(&grid_layout);

  item.add_grid_tile_button(block_);
  item.build_grid_tile(*uiLayoutRow(overlap, false));
}

void GridViewLayoutBuilder::build_from_view(const AbstractGridView &grid_view,
                                            const View2D &v2d) const
{
  uiLayout *prev_layout = current_layout();

  uiLayout &layout = *uiLayoutColumn(current_layout(), false);
  const GridViewStyle &style = grid_view.get_style();

  const int cols_per_row = std::max(uiLayoutGetWidth(&layout) / style.tile_width, 1);

  BuildOnlyVisibleButtonsHelper build_visible_helper(v2d, grid_view, cols_per_row);

  build_visible_helper.fill_layout_before_visible(block_);

  /* Use `-cols_per_row` because the grid layout uses a multiple of the passed absolute value for
   * the number of columns then, rather than distributing the number of items evenly over rows and
   * stretching the items to fit (see #uiLayoutItemGridFlow.columns_len). */
  uiLayout *grid_layout = uiLayoutGridFlow(&layout, true, -cols_per_row, true, true, true);

  int item_idx = 0;
  grid_view.foreach_item([&](AbstractGridViewItem &item) {
    /* Skip if item isn't visible. */
    if (!build_visible_helper.is_item_visible(item_idx)) {
      item_idx++;
      return;
    }

    build_grid_tile(*grid_layout, item);
    item_idx++;
  });

  /* If there are not enough items to fill the layout, add padding items so the layout doesn't
   * stretch over the entire width. */
  if (grid_view.get_item_count() < cols_per_row) {
    for (int padding_item_idx = 0; padding_item_idx < (cols_per_row - grid_view.get_item_count());
         padding_item_idx++) {
      uiItemS(grid_layout);
    }
  }

  UI_block_layout_set_current(&block_, prev_layout);

  build_visible_helper.fill_layout_after_visible(block_);
}

uiLayout *GridViewLayoutBuilder::current_layout() const
{
  return block_.curlayout;
}

/* ---------------------------------------------------------------------- */

GridViewBuilder::GridViewBuilder(uiBlock &block) : block_(block)
{
}

void GridViewBuilder::build_grid_view(AbstractGridView &grid_view, const View2D &v2d)
{
  grid_view.build_items();
  grid_view.update_from_old(block_);
  grid_view.change_state_delayed();

  GridViewLayoutBuilder builder(block_);
  builder.build_from_view(grid_view, v2d);
}

/* ---------------------------------------------------------------------- */

PreviewGridItem::PreviewGridItem(StringRef identifier, StringRef label, int preview_icon_id)
    : AbstractGridViewItem(identifier), label(label), preview_icon_id(preview_icon_id)
{
}

void PreviewGridItem::build_grid_tile(uiLayout &layout) const
{
  const GridViewStyle &style = get_view().get_style();
  uiBlock *block = uiLayoutGetBlock(&layout);

  uiBut *but = uiDefBut(block,
                        UI_BTYPE_PREVIEW_TILE,
                        0,
                        label.c_str(),
                        0,
                        0,
                        style.tile_width,
                        style.tile_height,
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        "");
  ui_def_but_icon(but,
                  preview_icon_id,
                  /* NOLINTNEXTLINE: bugprone-suspicious-enum-usage */
                  UI_HAS_ICON | UI_BUT_ICON_PREVIEW);
}

void PreviewGridItem::set_on_activate_fn(ActivateFn fn)
{
  activate_fn_ = fn;
}

void PreviewGridItem::set_is_active_fn(IsActiveFn fn)
{
  is_active_fn_ = fn;
}

void PreviewGridItem::on_activate()
{
  if (activate_fn_) {
    activate_fn_(*this);
  }
}

std::optional<bool> PreviewGridItem::should_be_active() const
{
  if (is_active_fn_) {
    return is_active_fn_();
  }
  return std::nullopt;
}

}  // namespace blender::ui

using namespace blender::ui;

/* ---------------------------------------------------------------------- */
/* C-API */

using namespace blender::ui;

bool UI_grid_view_item_is_active(const uiGridViewItemHandle *item_handle)
{
  const AbstractGridViewItem &item = reinterpret_cast<const AbstractGridViewItem &>(*item_handle);
  return item.is_active();
}

bool UI_grid_view_listen_should_redraw(const uiGridViewHandle *view_handle,
                                       const wmNotifier *notifier)
{
  const AbstractGridView &view = *reinterpret_cast<const AbstractGridView *>(view_handle);
  return view.listen(*notifier);
}

bool UI_grid_view_item_matches(const uiGridViewItemHandle *a_handle,
                               const uiGridViewItemHandle *b_handle)
{
  const AbstractGridViewItem &a = reinterpret_cast<const AbstractGridViewItem &>(*a_handle);
  const AbstractGridViewItem &b = reinterpret_cast<const AbstractGridViewItem &>(*b_handle);
  return a.matches(b);
}

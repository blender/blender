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

#include <stdexcept>

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

const GridViewStyle &AbstractGridView::get_style() const
{
  return style_;
}

GridViewStyle::GridViewStyle(int width, int height) : tile_width(width), tile_height(height)
{
}

/* ---------------------------------------------------------------------- */

bool AbstractGridViewItem::matches(const AbstractGridViewItem &other) const
{
  return label_ == other.label_;
}

void AbstractGridViewItem::grid_tile_click_fn(struct bContext * /*C*/,
                                              void *but_arg1,
                                              void * /*arg2*/)
{
  uiButGridTile *grid_tile_but = (uiButGridTile *)but_arg1;
  AbstractGridViewItem &grid_item = reinterpret_cast<AbstractGridViewItem &>(
      *grid_tile_but->view_item);

  //  tree_item.activate();
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

const AbstractGridView &AbstractGridViewItem::get_view() const
{
  if (UNLIKELY(!view_)) {
    throw std::runtime_error(
        "Invalid state, item must be added through AbstractGridView::add_item()");
  }
  return *view_;
}

/* ---------------------------------------------------------------------- */

class GridViewLayoutBuilder {
  uiBlock &block_;

  friend GridViewBuilder;

 public:
  GridViewLayoutBuilder(uiBlock &block);

  void build_from_view(const AbstractGridView &grid_view) const;
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

void GridViewLayoutBuilder::build_from_view(const AbstractGridView &grid_view) const
{
  uiLayout *prev_layout = current_layout();

  uiLayout &layout = *uiLayoutColumn(current_layout(), false);
  const GridViewStyle &style = grid_view.get_style();

  const int cols_per_row = uiLayoutGetWidth(&layout) / style.tile_width;
  /* Use `-cols_per_row` because the grid layout uses a multiple of the passed absolute value for
   * the number of columns then, rather than distributing the number of items evenly over rows and
   * stretching the items to fit (see #uiLayoutItemGridFlow.columns_len). */
  uiLayout *grid_layout = uiLayoutGridFlow(&layout, true, -cols_per_row, true, true, true);

  int item_count = 0;
  grid_view.foreach_item([&](AbstractGridViewItem &item) {
    build_grid_tile(*grid_layout, item);
    item_count++;
  });

  /* If there are not enough items to fill the layout, add padding items so the layout doesn't
   * stretch over the entire width. */
  if (item_count < cols_per_row) {
    for (int padding_item_idx = 0; padding_item_idx < (cols_per_row - item_count);
         padding_item_idx++) {
      uiItemS(grid_layout);
    }
  }

  UI_block_layout_set_current(&block_, prev_layout);
}

uiLayout *GridViewLayoutBuilder::current_layout() const
{
  return block_.curlayout;
}

/* ---------------------------------------------------------------------- */

GridViewBuilder::GridViewBuilder(uiBlock &block) : block_(block)
{
}

void GridViewBuilder::build_grid_view(AbstractGridView &grid_view)
{
  grid_view.build_items();

  GridViewLayoutBuilder builder(block_);
  builder.build_from_view(grid_view);
  //  grid_view.update_from_old(block_);
  //  grid_view.change_state_delayed();

  //  TreeViewLayoutBuilder builder(block_);
  //  builder.build_from_tree(tree_view);
}

/* ---------------------------------------------------------------------- */

PreviewGridItem::PreviewGridItem(StringRef label, int preview_icon_id)
    : label(label), preview_icon_id(preview_icon_id)
{
}

void PreviewGridItem::build_grid_tile(uiLayout &layout) const
{
  const GridViewStyle &style = get_view().get_style();
  uiBlock *block = uiLayoutGetBlock(&layout);
  uiBut *but = uiDefIconTextBut(block,
                                UI_BTYPE_PREVIEW_TILE,
                                0,
                                preview_icon_id,
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

}  // namespace blender::ui

using namespace blender::ui;

/* ---------------------------------------------------------------------- */
/* C-API */

using namespace blender::ui;

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

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

  void build_from_view(const AbstractGridView &grid_view);
  uiLayout *current_layout() const;
};

GridViewLayoutBuilder::GridViewLayoutBuilder(uiBlock &block) : block_(block)
{
}

void GridViewLayoutBuilder::build_from_view(const AbstractGridView &grid_view)
{
  uiLayout &layout = *uiLayoutColumn(current_layout(), false);
  const GridViewStyle &style = grid_view.get_style();

  const int cols_per_row = uiLayoutGetWidth(&layout) / style.tile_width;
  uiLayout *grid_layout = uiLayoutGridFlow(&layout, true, cols_per_row, true, true, true);

  grid_view.foreach_item([&](AbstractGridViewItem &item) { item.build_grid_tile(*grid_layout); });
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

bool UI_grid_view_listen_should_redraw(const uiGridViewHandle *view_handle,
                                       const wmNotifier *notifier)
{
  const AbstractGridView &view = *reinterpret_cast<const AbstractGridView *>(view_handle);
  return view.listen(*notifier);
}

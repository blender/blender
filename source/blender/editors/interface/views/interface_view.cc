/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Code to manage views as part of the regular screen hierarchy. E.g. managing ownership of views
 * inside blocks (#uiBlock.views), looking up items in the region, passing WM notifiers to views,
 * etc.
 *
 * Blocks and their contained views are reconstructed on every redraw. This file also contains
 * functions related to this recreation of views inside blocks. For example to query state
 * information before the view is done reconstructing (#AbstractView.is_reconstructed() returns
 * false), it may be enough to query the previous version of the block/view/view-item. Since such
 * queries rely on the details of the UI reconstruction process, they should remain internal to
 * `interface/` code.
 */

#include <memory>
#include <type_traits>
#include <variant>

#include "DNA_screen_types.h"

#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_map.hh"

#include "ED_screen.hh"

#include "interface_intern.hh"

#include "UI_interface.hh"

#include "UI_abstract_view.hh"
#include "UI_grid_view.hh"
#include "UI_tree_view.hh"

using namespace blender;
using namespace blender::ui;

/**
 * Wrapper to store views in a #ListBase, addressable via an identifier.
 */
struct ViewLink : public Link {
  std::string idname;
  std::unique_ptr<AbstractView> view;

  static void views_bounds_calc(const uiBlock &block);
};

template<class T>
static T *ui_block_add_view_impl(uiBlock &block,
                                 StringRef idname,
                                 std::unique_ptr<AbstractView> view)
{
  ViewLink *view_link = MEM_new<ViewLink>(__func__);
  BLI_addtail(&block.views, view_link);

  view_link->view = std::move(view);
  view_link->idname = idname;

  return dynamic_cast<T *>(view_link->view.get());
}

AbstractGridView *UI_block_add_view(uiBlock &block,
                                    StringRef idname,
                                    std::unique_ptr<AbstractGridView> grid_view)
{
  return ui_block_add_view_impl<AbstractGridView>(block, idname, std::move(grid_view));
}

AbstractTreeView *UI_block_add_view(uiBlock &block,
                                    StringRef idname,
                                    std::unique_ptr<AbstractTreeView> tree_view)
{
  return ui_block_add_view_impl<AbstractTreeView>(block, idname, std::move(tree_view));
}

void ui_block_free_views(uiBlock *block)
{
  LISTBASE_FOREACH_MUTABLE (ViewLink *, link, &block->views) {
    MEM_delete(link);
  }
}

void ViewLink::views_bounds_calc(const uiBlock &block)
{
  Map<AbstractView *, rcti> views_bounds;

  rcti minmax;
  BLI_rcti_init_minmax(&minmax);
  LISTBASE_FOREACH (ViewLink *, link, &block.views) {
    views_bounds.add(link->view.get(), minmax);
  }

  LISTBASE_FOREACH (uiBut *, but, &block.buttons) {
    if (but->type != UI_BTYPE_VIEW_ITEM) {
      continue;
    }
    uiButViewItem *view_item_but = static_cast<uiButViewItem *>(but);
    if (!view_item_but->view_item) {
      continue;
    }

    /* Get the view from the button. */
    AbstractViewItem &view_item = reinterpret_cast<AbstractViewItem &>(*view_item_but->view_item);
    AbstractView &view = view_item.get_view();

    rcti &bounds = views_bounds.lookup(&view);
    rcti but_rcti{};
    BLI_rcti_rctf_copy_round(&but_rcti, &view_item_but->rect);
    BLI_rcti_do_minmax_rcti(&bounds, &but_rcti);
  }

  for (const auto item : views_bounds.items()) {
    const rcti &bounds = item.value;
    if (BLI_rcti_is_empty(&bounds)) {
      continue;
    }

    AbstractView &view = *item.key;
    view.bounds_ = bounds;
  }
}

void ui_block_views_bounds_calc(const uiBlock *block)
{
  ViewLink::views_bounds_calc(*block);
}

void ui_block_views_listen(const uiBlock *block, const wmRegionListenerParams *listener_params)
{
  ARegion *region = listener_params->region;

  LISTBASE_FOREACH (ViewLink *, view_link, &block->views) {
    if (view_link->view->listen(*listener_params->notifier)) {
      ED_region_tag_redraw(region);
    }
  }
}

void ui_block_views_draw_overlays(const ARegion *region, const uiBlock *block)
{
  LISTBASE_FOREACH (ViewLink *, view_link, &block->views) {
    view_link->view->draw_overlays(*region);
  }
}

uiViewHandle *UI_region_view_find_at(const ARegion *region, const int xy[2], const int pad)
{
  /* NOTE: Similar to #ui_but_find_mouse_over_ex(). */

  if (!ui_region_contains_point_px(region, xy)) {
    return nullptr;
  }
  LISTBASE_FOREACH (uiBlock *, block, &region->uiblocks) {
    float mx = xy[0], my = xy[1];
    ui_window_to_block_fl(region, block, &mx, &my);

    LISTBASE_FOREACH (ViewLink *, view_link, &block->views) {
      std::optional<rcti> bounds = view_link->view->get_bounds();
      if (!bounds) {
        continue;
      }

      rcti padded_bounds = *bounds;
      if (pad) {
        BLI_rcti_pad(&padded_bounds, pad, pad);
      }
      if (BLI_rcti_isect_pt(&padded_bounds, mx, my)) {
        return reinterpret_cast<uiViewHandle *>(view_link->view.get());
      }
    }
  }

  return nullptr;
}

uiViewItemHandle *UI_region_views_find_item_at(const ARegion *region, const int xy[2])
{
  uiButViewItem *item_but = (uiButViewItem *)ui_view_item_find_mouse_over(region, xy);
  if (!item_but) {
    return nullptr;
  }

  return item_but->view_item;
}

uiViewItemHandle *UI_region_views_find_active_item(const ARegion *region)
{
  uiButViewItem *item_but = (uiButViewItem *)ui_view_item_find_active(region);
  if (!item_but) {
    return nullptr;
  }

  return item_but->view_item;
}

uiBut *UI_region_views_find_active_item_but(const ARegion *region)
{
  return ui_view_item_find_active(region);
}

namespace blender::ui {

std::unique_ptr<DropTargetInterface> region_views_find_drop_target_at(const ARegion *region,
                                                                      const int xy[2])
{
  uiViewItemHandle *hovered_view_item = UI_region_views_find_item_at(region, xy);
  if (hovered_view_item) {
    std::unique_ptr<DropTargetInterface> drop_target = view_item_drop_target(hovered_view_item);
    if (drop_target) {
      return drop_target;
    }
  }

  /* Get style for some sensible padding around the view items. */
  const uiStyle *style = UI_style_get_dpi();
  uiViewHandle *hovered_view = UI_region_view_find_at(region, xy, style->buttonspacex);
  if (hovered_view) {
    std::unique_ptr<DropTargetInterface> drop_target = view_drop_target(hovered_view);
    if (drop_target) {
      return drop_target;
    }
  }

  return nullptr;
}

}  // namespace blender::ui

static StringRef ui_block_view_find_idname(const uiBlock &block, const AbstractView &view)
{
  /* First get the idname the of the view we're looking for. */
  LISTBASE_FOREACH (ViewLink *, view_link, &block.views) {
    if (view_link->view.get() == &view) {
      return view_link->idname;
    }
  }

  return {};
}

template<class T>
static T *ui_block_view_find_matching_in_old_block_impl(const uiBlock &new_block,
                                                        const T &new_view)
{
  uiBlock *old_block = new_block.oldblock;
  if (!old_block) {
    return nullptr;
  }

  StringRef idname = ui_block_view_find_idname(new_block, new_view);
  if (idname.is_empty()) {
    return nullptr;
  }

  LISTBASE_FOREACH (ViewLink *, old_view_link, &old_block->views) {
    if (old_view_link->idname == idname) {
      return dynamic_cast<T *>(old_view_link->view.get());
    }
  }

  return nullptr;
}

uiViewHandle *ui_block_view_find_matching_in_old_block(const uiBlock *new_block,
                                                       const uiViewHandle *new_view_handle)
{
  BLI_assert(new_block && new_view_handle);
  const AbstractView &new_view = reinterpret_cast<const AbstractView &>(*new_view_handle);

  AbstractView *old_view = ui_block_view_find_matching_in_old_block_impl(*new_block, new_view);
  return reinterpret_cast<uiViewHandle *>(old_view);
}

uiButViewItem *ui_block_view_find_matching_view_item_but_in_old_block(
    const uiBlock *new_block, const uiViewItemHandle *new_item_handle)
{
  uiBlock *old_block = new_block->oldblock;
  if (!old_block) {
    return nullptr;
  }

  const AbstractViewItem &new_item = *reinterpret_cast<const AbstractViewItem *>(new_item_handle);
  const AbstractView *old_view = ui_block_view_find_matching_in_old_block_impl(
      *new_block, new_item.get_view());
  if (!old_view) {
    return nullptr;
  }

  LISTBASE_FOREACH (uiBut *, old_but, &old_block->buttons) {
    if (old_but->type != UI_BTYPE_VIEW_ITEM) {
      continue;
    }
    uiButViewItem *old_item_but = (uiButViewItem *)old_but;
    if (!old_item_but->view_item) {
      continue;
    }
    AbstractViewItem &old_item = *reinterpret_cast<AbstractViewItem *>(old_item_but->view_item);
    /* Check if the item is from the expected view. */
    if (&old_item.get_view() != old_view) {
      continue;
    }

    if (UI_view_item_matches(reinterpret_cast<const uiViewItemHandle *>(&new_item),
                             reinterpret_cast<const uiViewItemHandle *>(&old_item)))
    {
      return old_item_but;
    }
  }

  return nullptr;
}

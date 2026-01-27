/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Code to manage views as part of the regular screen hierarchy. E.g. managing ownership of views
 * inside blocks (#Block.views), looking up items in the region, passing WM notifiers to views,
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

#include "DNA_screen_types.h"

#include "BKE_screen.hh"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_rect.h"

#include "ED_screen.hh"

#include "interface_intern.hh"

#include "UI_abstract_view.hh"
#include "UI_grid_view.hh"
#include "UI_tree_view.hh"

namespace blender::ui {

/**
 * Wrapper to store views in a #ListBase, addressable via an identifier.
 */
struct ViewLink : public Link {
  std::string idname;
  std::unique_ptr<AbstractView> view;

  static void views_bounds_calc(const Block &block);
};

template<class T>
static T *block_add_view_impl(Block &block, StringRef idname, std::unique_ptr<AbstractView> view)
{
  BLI_assert(idname.size() < int64_t(sizeof(uiViewStateLink::idname)));

  ViewLink *view_link = MEM_new<ViewLink>(__func__);
  BLI_addtail(&block.views, view_link);

  view_link->view = std::move(view);
  view_link->idname = idname;

  return dynamic_cast<T *>(view_link->view.get());
}

AbstractGridView *block_add_view(Block &block,
                                 StringRef idname,
                                 std::unique_ptr<AbstractGridView> grid_view)
{
  return block_add_view_impl<AbstractGridView>(block, idname, std::move(grid_view));
}

AbstractTreeView *block_add_view(Block &block,
                                 StringRef idname,
                                 std::unique_ptr<AbstractTreeView> tree_view)
{
  return block_add_view_impl<AbstractTreeView>(block, idname, std::move(tree_view));
}

void block_free_views(Block *block)
{
  for (ViewLink &link : block->views.items_mutable()) {
    MEM_delete(&link);
  }
}

void ViewLink::views_bounds_calc(const Block &block)
{
  Map<AbstractView *, rcti> views_bounds;

  rcti minmax;
  BLI_rcti_init_minmax(&minmax);
  for (ViewLink &link : block.views) {
    views_bounds.add(link.view.get(), minmax);
  }

  for (const std::unique_ptr<Button> &but : block.buttons) {
    if (but->type != ButtonType::ViewItem) {
      continue;
    }
    auto *view_item_but = static_cast<ButtonViewItem *>(but.get());
    if (!view_item_but->view_item) {
      continue;
    }

    /* Get the view from the button. */
    AbstractViewItem &view_item = *view_item_but->view_item;
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

void block_view_persistent_state_restore(const ARegion &region,
                                         const Block &block,
                                         AbstractView &view)
{
  StringRef idname = [&]() -> StringRef {
    for (ViewLink &link : block.views) {
      if (link.view.get() == &view) {
        return link.idname;
      }
    }
    return "";
  }();

  if (idname.is_empty()) {
    BLI_assert_unreachable();
    return;
  }

  for (uiViewStateLink &stored_state : region.view_states) {
    if (stored_state.idname == idname) {
      view.persistent_state_apply(stored_state.state);
    }
  }
}

static uiViewStateLink *ensure_view_state(ARegion &region, const ViewLink &link)
{
  for (uiViewStateLink &stored_state : region.view_states) {
    if (link.idname == stored_state.idname) {
      return &stored_state;
    }
  }

  uiViewStateLink *new_state = MEM_new<uiViewStateLink>(__func__);
  link.idname.copy(new_state->idname, sizeof(new_state->idname));
  BLI_addhead(&region.view_states, new_state);
  return new_state;
}

void block_views_end(ARegion *region, const Block *block)
{
  ViewLink::views_bounds_calc(*block);

  if (region && region->regiontype != RGN_TYPE_TEMPORARY) {
    for (const ViewLink &link : block->views) {
      /* Ensure persistent view state storage for writing to files if needed. */
      if (std::optional<uiViewState> temp_state = link.view->persistent_state()) {
        uiViewStateLink *state_link = ensure_view_state(*region, link);
        state_link->state = *temp_state;
      }
    }
  }
}

void block_views_listen(const Block *block, const wmRegionListenerParams *listener_params)
{
  ARegion *region = listener_params->region;

  for (ViewLink &view_link : block->views) {
    if (view_link.view->listen(*listener_params->notifier)) {
      ED_region_tag_redraw(region);
    }
  }
}

void block_views_draw_overlays(const ARegion *region, const Block *block)
{
  for (ViewLink &view_link : block->views) {
    view_link.view->draw_overlays(*region, *block);
  }
}

AbstractView *region_view_find_at(const ARegion *region, const int xy[2], const int pad)
{
  /* NOTE: Similar to #ui_but_find_mouse_over_ex(). */

  if (!region_contains_point_px(region, xy)) {
    return nullptr;
  }
  for (Block &block : region->runtime->uiblocks) {
    float mx = xy[0], my = xy[1];
    window_to_block_fl(region, &block, &mx, &my);

    for (ViewLink &view_link : block.views) {
      std::optional<rcti> bounds = view_link.view->get_bounds();
      if (!bounds) {
        continue;
      }

      rcti padded_bounds = *bounds;
      if (pad) {
        BLI_rcti_pad(&padded_bounds, pad, pad);
      }
      if (BLI_rcti_isect_pt(&padded_bounds, mx, my)) {
        return view_link.view.get();
      }
    }
  }

  return nullptr;
}

AbstractViewItem *region_views_find_item_at(const ARegion &region, const int xy[2])
{
  auto *item_but = static_cast<ButtonViewItem *>(view_item_find_mouse_over(&region, xy));
  if (!item_but) {
    return nullptr;
  }

  return item_but->view_item;
}

AbstractViewItem *region_views_find_active_item(const ARegion *region)
{
  auto *item_but = static_cast<ButtonViewItem *>(view_item_find_active(region));
  if (!item_but) {
    return nullptr;
  }

  return item_but->view_item;
}

Button *region_views_find_active_item_but(const ARegion *region)
{
  return view_item_find_active(region);
}

void region_views_clear_search_highlight(const ARegion *region)
{
  for (Block &block : region->runtime->uiblocks) {
    for (ViewLink &view_link : block.views) {
      view_link.view->clear_search_highlight();
    }
  }
}

std::unique_ptr<DropTargetInterface> region_views_find_drop_target_at(const ARegion *region,
                                                                      const int xy[2])
{
  if (AbstractViewItem *item = region_views_find_item_at(*region, xy)) {
    if (std::unique_ptr<DropTargetInterface> target = item->create_item_drop_target()) {
      return target;
    }
  }

  /* Get style for some sensible padding around the view items. */
  const uiStyle *style = style_get_dpi();
  if (AbstractView *view = region_view_find_at(region, xy, style->buttonspacex)) {
    if (std::unique_ptr<DropTargetInterface> target = view->create_drop_target()) {
      return target;
    }
  }

  if (AbstractView *view = region_view_find_at(region, xy, 0)) {
    /* If we are above a tree, but not hovering any specific element, dropping something should
     * insert it after the last item. */
    if (AbstractTreeView *tree_view = dynamic_cast<AbstractTreeView *>(view)) {
      /* Find the last item which we want to drop below. */
      AbstractTreeViewItem *last_item = nullptr;
      tree_view->foreach_root_item([&](AbstractTreeViewItem &item) {
        if (!item.is_interactive()) {
          return;
        }
        last_item = &item;
      });
      if (last_item) {
        return last_item->create_item_drop_target();
      }
    }
  }

  return nullptr;
}

static StringRef block_view_find_idname(const Block &block, const AbstractView &view)
{
  /* First get the `idname` of the view we're looking for. */
  for (ViewLink &view_link : block.views) {
    if (view_link.view.get() == &view) {
      return view_link.idname;
    }
  }

  return {};
}

template<class T>
static T *block_view_find_matching_in_old_block_impl(const Block &new_block, const T &new_view)
{
  Block *old_block = new_block.oldblock;
  if (!old_block) {
    return nullptr;
  }

  StringRef idname = block_view_find_idname(new_block, new_view);
  if (idname.is_empty()) {
    return nullptr;
  }

  for (ViewLink &old_view_link : old_block->views) {
    if (old_view_link.idname == idname) {
      return dynamic_cast<T *>(old_view_link.view.get());
    }
  }

  return nullptr;
}

AbstractView *block_view_find_matching_in_old_block(const Block &new_block,
                                                    const AbstractView &new_view)
{
  return block_view_find_matching_in_old_block_impl(new_block, new_view);
}

ButtonViewItem *block_view_find_matching_view_item_but_in_old_block(
    const Block &new_block, const AbstractViewItem &new_item)
{
  Block *old_block = new_block.oldblock;
  if (!old_block) {
    return nullptr;
  }

  const AbstractView *old_view = block_view_find_matching_in_old_block_impl(new_block,
                                                                            new_item.get_view());
  if (!old_view) {
    return nullptr;
  }

  for (const std::unique_ptr<Button> &old_but : old_block->buttons) {
    if (old_but->type != ButtonType::ViewItem) {
      continue;
    }
    ButtonViewItem *old_item_but = static_cast<ButtonViewItem *>(old_but.get());
    if (!old_item_but->view_item) {
      continue;
    }
    AbstractViewItem &old_item = *old_item_but->view_item;
    /* Check if the item is from the expected view. */
    if (&old_item.get_view() != old_view) {
      continue;
    }

    if (view_item_matches(new_item, old_item)) {
      return old_item_but;
    }
  }

  return nullptr;
}

}  // namespace blender::ui

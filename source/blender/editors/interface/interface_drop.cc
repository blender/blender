/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 */

#include "BLI_listbase.hh"

#include "UI_interface.hh"
#include "UI_tree_view.hh"

namespace blender::ui {

DragInfo::DragInfo(const wmDrag &drag, const wmEvent &event, const DropLocation drop_location)
    : drag_data(drag), event(event), drop_location(drop_location)
{
}

std::optional<DropLocation> DropTargetInterface::choose_drop_location(
    const ARegion & /*region*/, const wmEvent & /*event*/) const
{
  return DropLocation::Into;
}

bool drop_target_apply_drop(bContext &C,
                            const ARegion &region,
                            const wmEvent &event,
                            const DropTargetInterface &drop_target,
                            const ListBaseT<wmDrag> &drags)
{
  const char *disabled_hint_dummy = nullptr;
  for (const wmDrag &drag : drags) {
    if (!drop_target.can_drop(drag, &disabled_hint_dummy)) {
      return false;
    }

    std::optional<DropLocation> drop_location = drop_target.choose_drop_location(region, event);
    if (!drop_location) {
      return false;
    }

    AbstractView *view = region_view_find_at(&region, event.xy, 0);
    if (AbstractTreeView *tree_view = dynamic_cast<AbstractTreeView *>(view)) {
      TreeViewSortOrder sortorder = tree_view->invert_sort_type_get();
      if (sortorder != TreeViewSortOrder::None) {
        AbstractTreeViewItem *dropitem = dynamic_cast<AbstractTreeViewItem *>(
            region_views_find_item_at(region, event.xy));

        bool change_drop_order = (sortorder == TreeViewSortOrder::InvertNested);
        if (sortorder == TreeViewSortOrder::InvertRoot) {
          tree_view->foreach_root_item(
              [&](AbstractTreeViewItem &item) { change_drop_order |= (dropitem == &item); });
        }
        if (change_drop_order) {
          /* Switch drop location when invert sorting is enabled. */
          if (*drop_location == DropLocation::After) {
            *drop_location = DropLocation::Before;
          }
          else if (*drop_location == DropLocation::Before) {
            *drop_location = DropLocation::After;
          }
        }
      }
    }

    const DragInfo drag_info{drag, event, *drop_location};
    return drop_target.on_drop(&C, drag_info);
  }

  return false;
}

std::string drop_target_tooltip(const ARegion &region,
                                const DropTargetInterface &drop_target,
                                const wmDrag &drag,
                                const wmEvent &event)
{
  const char *disabled_hint_dummy = nullptr;
  if (!drop_target.can_drop(drag, &disabled_hint_dummy)) {
    return {};
  }

  const std::optional<DropLocation> drop_location = drop_target.choose_drop_location(region,
                                                                                     event);
  if (!drop_location) {
    return {};
  }

  const DragInfo drag_info{drag, event, *drop_location};
  return drop_target.drop_tooltip(drag_info);
}

}  // namespace blender::ui

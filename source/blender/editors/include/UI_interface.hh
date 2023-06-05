/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#include <functional>
#include <memory>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "UI_resources.h"

namespace blender::nodes::geo_eval_log {
struct GeometryAttributeInfo;
}

struct ARegion;
struct bContext;
struct PointerRNA;
struct StructRNA;
struct uiBlock;
struct uiBut;
struct uiLayout;
struct uiList;
struct uiSearchItems;
struct uiViewHandle;
struct uiViewItemHandle;
struct wmDrag;

namespace blender::ui {

class AbstractGridView;
class AbstractTreeView;

void UI_but_func_pushed_state_set(uiBut *but, std::function<bool(const uiBut &)> func);

/**
 * An item in a breadcrumb-like context. Currently this struct is very simple, but more
 * could be added to it in the future, to support interactivity or tooltips, for example.
 */
struct ContextPathItem {
  /* Text to display in the UI. */
  std::string name;
  /* #BIFIconID */
  int icon;
  int icon_indicator_number;
};

void context_path_add_generic(Vector<ContextPathItem> &path,
                              StructRNA &rna_type,
                              void *ptr,
                              const BIFIconID icon_override = ICON_NONE);

void template_breadcrumbs(uiLayout &layout, Span<ContextPathItem> context_path);

void attribute_search_add_items(StringRefNull str,
                                bool can_create_attribute,
                                Span<const nodes::geo_eval_log::GeometryAttributeInfo *> infos,
                                uiSearchItems *items,
                                bool is_first);

/**
 * This provides a common interface for UI elements that want to support dragging & dropping
 * entities into/onto them. With it, the element can determine if the dragged entity can be dropped
 * onto itself, provide feedback while dragging and run custom code for the dropping.
 *
 * Note that this is just an interface. A #wmDropBox is needed to request instances of it from a UI
 * element and call its functions. For example the drop box using "UI_OT_view_drop" implements
 * dropping for views and view items via this interface. To support other kinds of UI elements,
 * similar drop boxes would be necessary.
 */
class DropTargetInterface {
 public:
  DropTargetInterface() = default;
  virtual ~DropTargetInterface() = default;

  /**
   * Check if the data dragged with \a drag can be dropped on the element this drop target is for.
   * \param r_disabled_hint: Return a static string to display to the user, explaining why dropping
   *                         isn't possible on this UI element. Shouldn't be done too aggressively,
   *                         e.g. don't set this if the drag-type can't be dropped here; only if it
   *                         can but there's another reason it can't be dropped. Can assume this is
   *                         a non-null pointer.
   */
  virtual bool can_drop(const wmDrag &drag, const char **r_disabled_hint) const = 0;
  /**
   * Custom text to display when dragging over the element using this drop target. Should
   * explain what happens when dropping the data onto this UI element. Will only be used if
   * #DropTargetInterface::can_drop() returns true, so the implementing override doesn't have
   * to check that again. The returned value must be a translated string.
   */
  virtual std::string drop_tooltip(const wmDrag &drag) const = 0;
  /**
   * Execute the logic to apply a drop of the data dragged with \a drag onto/into the UI element
   * this drop target is for.
   */
  virtual bool on_drop(bContext *C, const wmDrag &drag) const = 0;
};

/**
 * Let a drop target handle a drop event.
 * \return True if the dropping was successful.
 */
bool drop_target_apply_drop(bContext &C,
                            const DropTargetInterface &drop_target,
                            const ListBase &drags);
/**
 * Call #DropTargetInterface::drop_tooltip() and return the result as newly allocated C string
 * (unless the result is empty, returns null then). Needs freeing with MEM_freeN().
 */
char *drop_target_tooltip(const DropTargetInterface &drop_target, const wmDrag &drag);

std::unique_ptr<DropTargetInterface> view_drop_target(uiViewHandle *view_handle);
std::unique_ptr<DropTargetInterface> view_item_drop_target(uiViewItemHandle *item_handle);
/**
 * Try to find a view item with a drop target under the mouse cursor, or if not found, a view
 * with a drop target.
 * \param xy: Coordinate to find a drop target at, in window space.
 */
std::unique_ptr<DropTargetInterface> region_views_find_drop_target_at(const ARegion *region,
                                                                      const int xy[2]);

}  // namespace blender::ui

enum eUIListFilterResult {
  /** Never show this item, even when filter results are inverted (#UILST_FLT_EXCLUDE). */
  UI_LIST_ITEM_NEVER_SHOW,
  /** Show this item, unless filter results are inverted (#UILST_FLT_EXCLUDE). */
  UI_LIST_ITEM_FILTER_MATCHES,
  /** Don't show this item, unless filter results are inverted (#UILST_FLT_EXCLUDE). */
  UI_LIST_ITEM_FILTER_MISMATCHES,
};

/**
 * Function object for UI list item filtering that does the default name comparison with '*'
 * wildcards. Create an instance of this once and pass it to #UI_list_filter_and_sort_items(), do
 * NOT create an instance for every item, this would be costly.
 */
class uiListNameFilter {
  /* Storage with an inline buffer for smaller strings (small buffer optimization). */
  struct {
    char filter_buff[32];
    char *filter_dyn = nullptr;
  } storage_;
  char *filter_ = nullptr;

 public:
  uiListNameFilter(uiList &list);
  ~uiListNameFilter();

  eUIListFilterResult operator()(const PointerRNA &itemptr,
                                 blender::StringRefNull name,
                                 int index);
};

using uiListItemFilterFn = blender::FunctionRef<eUIListFilterResult(
    const PointerRNA &itemptr, blender::StringRefNull name, int index)>;
using uiListItemGetNameFn =
    blender::FunctionRef<std::string(const PointerRNA &itemptr, int index)>;

/**
 * Filter list items using \a item_filter_fn and sort the result. This respects the normal UI list
 * filter settings like alphabetical sorting (#UILST_FLT_SORT_ALPHA), and result inverting
 * (#UILST_FLT_EXCLUDE).
 *
 * Call this from a #uiListType::filter_items callback with any #item_filter_fn. #uiListNameFilter
 * can be used to apply the default name based filtering.
 *
 * \param get_name_fn: In some cases the name cannot be retrieved via RNA. This function can be set
 *                     to provide the name still.
 */
void UI_list_filter_and_sort_items(uiList *ui_list,
                                   const struct bContext *C,
                                   uiListItemFilterFn item_filter_fn,
                                   PointerRNA *dataptr,
                                   const char *propname,
                                   uiListItemGetNameFn get_name_fn = nullptr);

/**
 * Override this for all available view types.
 */
blender::ui::AbstractGridView *UI_block_add_view(
    uiBlock &block,
    blender::StringRef idname,
    std::unique_ptr<blender::ui::AbstractGridView> grid_view);
blender::ui::AbstractTreeView *UI_block_add_view(
    uiBlock &block,
    blender::StringRef idname,
    std::unique_ptr<blender::ui::AbstractTreeView> tree_view);

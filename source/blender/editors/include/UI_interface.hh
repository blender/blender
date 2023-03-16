/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editorui
 */

#pragma once

#include <memory>

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "UI_resources.h"

namespace blender::nodes::geo_eval_log {
struct GeometryAttributeInfo;
}

struct PointerRNA;
struct StructRNA;
struct uiBlock;
struct uiList;
struct uiSearchItems;

namespace blender::ui {

class AbstractGridView;
class AbstractTreeView;

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

/* SPDX-FileCopyrightText: 2023 Blender Authors
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

#include "UI_resources.hh"

#include "UI_interface_c.hh"  // IWYU pragma: export

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
struct wmDrag;
struct wmEvent;
namespace blender::ui {
class AbstractView;
class AbstractViewItem;
}  // namespace blender::ui

void UI_but_func_set(uiBut *but, std::function<void(bContext &)> func);
void UI_but_func_pushed_state_set(uiBut *but, std::function<bool(const uiBut &)> func);

/**
 * Template generating a freeing callback matching the #uiButArgNFree signature, for data created
 * with #MEM_new.
 */
template<typename T> void but_func_argN_free(void *argN)
{
  MEM_delete(static_cast<T *>(argN));
}

/**
 * Template generating a copying callback matching the #uiButArgNCopy signature, for data created
 * with #MEM_new.
 */
template<typename T> void *but_func_argN_copy(const void *argN)
{
  return MEM_new<T>(__func__, *static_cast<const T *>(argN));
}

namespace blender::ui {

class AbstractGridView;
class AbstractTreeView;
class DropTargetInterface;

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

  std::function<void(bContext &)> handle_func;
};

void context_path_add_generic(Vector<ContextPathItem> &path,
                              StructRNA &rna_type,
                              void *ptr,
                              const BIFIconID icon_override = ICON_NONE,
                              std::function<void(bContext &)> handle_func = nullptr);

void template_breadcrumbs(uiLayout &layout, Span<ContextPathItem> context_path);

void attribute_search_add_items(StringRef str,
                                bool can_create_attribute,
                                Span<const nodes::geo_eval_log::GeometryAttributeInfo *> infos,
                                uiSearchItems *items,
                                bool is_first);
void grease_pencil_layer_search_add_items(StringRef str,
                                          Span<const std::string *> layer_names,
                                          uiSearchItems &items,
                                          bool is_first);

bool asset_shelf_popover_invoke(bContext &C,
                                blender::StringRef asset_shelf_idname,
                                ReportList &reports);
/**
 * Some drop targets simply allow dropping onto/into them, others support dragging in-between them.
 * Classes implementing the drop-target interface can use this type to control the behavior by
 * letting it influence the result of #choose_drop_location().
 */
enum class DropBehavior {
  /**
   * Enable dropping before (#DropLocation::Before) and after (#DropLocation::After) the
   * drop target. Typically used for reordering items.
   */
  Reorder,
  /** Only enable dropping onto/into the drop target (#DropLocation::Into). */
  Insert,
  /**
   * Enable dropping onto/into (#DropLocation::Into), before (#DropLocation::Before) and after
   * (#DropLocation::After) the drop target. Typically used for reordering items with nesting
   * support. */
  ReorderAndInsert,
};

/**
 * Information on how dragged data should be inserted on drop, as determined through
 * #DropTargetInterface::choose_drop_location(). Also see #DropBehavior.
 */
enum class DropLocation {
  Into,
  Before,
  After,
};

/**
 * Information passed to drop targets while dragging over them.
 */
struct DragInfo {
  const wmDrag &drag_data;
  const wmEvent &event;
  const DropLocation drop_location;

  DragInfo(const wmDrag &drag, const wmEvent &event, DropLocation drop_location);
};

/**
 * This provides a common interface for UI elements that want to support dragging & dropping
 * entities into/onto them. With it, the element can determine if the dragged entity can be dropped
 * onto itself, provide feedback while dragging and run custom code for the dropping.
 *
 * By default the drop target behaves so that data can be dragged into or onto it.
 * #choose_drop_location() can be overridden to change that.
 *
 * Note that this is just an interface (not in the strict sense of a Java/C# interface though). A
 * #wmDropBox is needed to request instances of it from a UI element and call its functions. For
 * example the drop box using "UI_OT_view_drop" implements dropping for views and view items via
 * this interface. To support other kinds of UI elements, similar drop boxes would be necessary.
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
   * Once the drop target validated that it can receive the dragged data using #can_drop(), this
   * method can determine where/how the data should be dropped exactly: before, after or into the
   * drop target. Additional feedback can be drawn then while dragging, and the #on_drop() function
   * should operate accordingly. Implementations of this function may want to use #DropBehavior to
   * control which locations may be returned here.
   *
   * If the returned optional is unset, dropping will be disabled. The default implementation
   * returns #DropLocation::Into.
   */
  virtual std::optional<DropLocation> choose_drop_location(const ARegion &region,
                                                           const wmEvent &event) const;
  /**
   * Custom text to display when dragging over the element using this drop target. Should
   * explain what happens when dropping the data onto this UI element. Will only be used if
   * #DropTargetInterface::can_drop() returns true, so the implementing override doesn't have
   * to check that again. The returned value must be a translated string.
   */
  virtual std::string drop_tooltip(const DragInfo &drag) const = 0;
  /**
   * Execute the logic to apply a drop of the data dragged with \a drag onto/into the UI element
   * this drop target is for.
   */
  virtual bool on_drop(bContext *C, const DragInfo &drag) const = 0;
};

/**
 * Let a drop target handle a drop event.
 * \return True if the dropping was successful.
 */
bool drop_target_apply_drop(bContext &C,
                            const ARegion &region,
                            const wmEvent &event,
                            const DropTargetInterface &drop_target,
                            const ListBase &drags);
/**
 * Call #DropTargetInterface::drop_tooltip() and return the result as newly allocated C string
 * (unless the result is empty, returns null then). Needs freeing with MEM_freeN().
 */
std::string drop_target_tooltip(const ARegion &region,
                                const DropTargetInterface &drop_target,
                                const wmDrag &drag,
                                const wmEvent &event);

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
 * Helper to apply custom filtering to UI lists not defined in Python. Custom filtering for
 * Python UI lists has own code. This is also used as the default filtering if no
 * #uiListType::filter_items callback is set.
 *
 * Filters list items using \a item_filter_fn and sorts the result. Also handles alphabetical
 * sorting (#UILST_FLT_SORT_ALPHA), and result inverting (#UILST_FLT_EXCLUDE) if enabled, so the
 * callback doesn't have to do this (unlike the filter function in Python).
 *
 * Call this from a #uiListType::filter_items callback with any #item_filter_fn. #uiListNameFilter
 * can be used to apply the default name based filtering.
 *
 * \param get_name_fn: In some cases the name cannot be retrieved via RNA. This function can be set
 *                     to provide the name still.
 */
void UI_list_filter_and_sort_items(uiList *ui_list,
                                   const bContext *C,
                                   uiListItemFilterFn item_filter_fn,
                                   PointerRNA *dataptr,
                                   const char *propname,
                                   uiListItemGetNameFn get_name_fn = nullptr);

/**
 * Override this for all available view types.
 * \param idname: Used for restoring persistent state of this view, potentially written to files.
 * Must not be longer than #BKE_ST_MAXNAME (including 0 terminator).
 */
blender::ui::AbstractGridView *UI_block_add_view(
    uiBlock &block,
    blender::StringRef idname,
    std::unique_ptr<blender::ui::AbstractGridView> grid_view);
blender::ui::AbstractTreeView *UI_block_add_view(
    uiBlock &block,
    blender::StringRef idname,
    std::unique_ptr<blender::ui::AbstractTreeView> tree_view);

void UI_alert(bContext *C,
              blender::StringRef title,
              blender::StringRef message,
              eAlertIcon icon,
              bool compact);

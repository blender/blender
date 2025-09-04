/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * General asset shelf code, mostly region callbacks, drawing and context stuff.
 */

#include <algorithm>
#include <cfloat>

#include "AS_asset_catalog_path.hh"
#include "AS_asset_library.hh"

#include "BLI_function_ref.hh"
#include "BLI_listbase.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"

#include "ED_asset_list.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_tree_view.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "ED_asset_shelf.hh"
#include "asset_shelf.hh"

namespace blender::ed::asset::shelf {

static int asset_shelf_default_tile_height();

void send_redraw_notifier(const bContext &C)
{
  WM_event_add_notifier(&C, NC_SPACE | ND_REGIONS_ASSET_SHELF, nullptr);
}

/* -------------------------------------------------------------------- */
/** \name Shelf Type
 * \{ */

static Vector<std::unique_ptr<AssetShelfType>> &static_shelf_types()
{
  static Vector<std::unique_ptr<AssetShelfType>> shelf_types;
  return shelf_types;
}

void type_register(std::unique_ptr<AssetShelfType> type)
{
  Vector<std::unique_ptr<AssetShelfType>> &shelf_types = static_shelf_types();
  shelf_types.append(std::move(type));
}

void type_unregister(const AssetShelfType &shelf_type)
{
  Vector<std::unique_ptr<AssetShelfType>> &shelf_types = static_shelf_types();
  auto *const it = std::find_if(shelf_types.begin(),
                                shelf_types.end(),
                                [&](const std::unique_ptr<AssetShelfType> &iter_type) {
                                  return iter_type.get() == &shelf_type;
                                });
  BLI_assert(it != shelf_types.end());

  shelf_types.remove(it - shelf_types.begin());
}

static bool type_poll_no_spacetype_check(const bContext &C, const AssetShelfType *shelf_type)
{
  if (!shelf_type) {
    return false;
  }

#ifndef NDEBUG
  const Vector<std::unique_ptr<AssetShelfType>> &shelf_types = static_shelf_types();
  BLI_assert_msg(std::find_if(shelf_types.begin(),
                              shelf_types.end(),
                              [&](const std::unique_ptr<AssetShelfType> &type) {
                                return type.get() == shelf_type;
                              }) != shelf_types.end(),
                 "Asset shelf type is not registered");
#endif

  return !shelf_type->poll || shelf_type->poll(&C, shelf_type);
}

bool type_poll_for_popup(const bContext &C, const AssetShelfType *shelf_type)
{
  return type_poll_no_spacetype_check(C, shelf_type);
}

/**
 * Poll an asset shelf type for display as a permanent region in a space of a given type (the
 * type's #bl_space_type).
 *
 * Popup asset shelves should use #type_poll_for_popup() instead.
 */
static bool type_poll_for_non_popup(const bContext &C,
                                    const AssetShelfType *shelf_type,
                                    const int space_type)
{
  if (!shelf_type) {
    return false;
  }
  if (shelf_type->space_type && (space_type != shelf_type->space_type)) {
    return false;
  }

  return type_poll_no_spacetype_check(C, shelf_type);
}

AssetShelfType *type_find_from_idname(const StringRef idname)
{
  for (const std::unique_ptr<AssetShelfType> &shelf_type : static_shelf_types()) {
    if (idname == shelf_type->idname) {
      return shelf_type.get();
    }
  }
  return nullptr;
}

AssetShelfType *ensure_shelf_has_type(AssetShelf &shelf)
{
  if (shelf.type) {
    return shelf.type;
  }

  for (const std::unique_ptr<AssetShelfType> &shelf_type : static_shelf_types()) {
    if (STREQ(shelf.idname, shelf_type->idname)) {
      shelf.type = shelf_type.get();
      return shelf_type.get();
    }
  }

  return nullptr;
}

AssetShelf *create_shelf_from_type(AssetShelfType &type)
{
  AssetShelf *shelf = MEM_new<AssetShelf>(__func__);
  *shelf = dna::shallow_zero_initialize();
  shelf->settings.preview_size = type.default_preview_size ? type.default_preview_size :
                                                             ASSET_SHELF_PREVIEW_SIZE_DEFAULT;
  shelf->settings.asset_library_reference = asset_system::all_library_reference();
  shelf->type = &type;
  shelf->preferred_row_count = 1;
  STRNCPY_UTF8(shelf->idname, type.idname);
  return shelf;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Active Shelf Management
 * \{ */

/**
 * Activating a shelf means assigning it to #RegionAssetShelf.active_shelf and (re-)inserting it at
 * the beginning of the #RegionAssetShelf.shelves list. This implies that after calling this, \a
 * shelf is guaranteed to be owned by the shelves list.
 */
static void activate_shelf(RegionAssetShelf &shelf_regiondata, AssetShelf &shelf)
{
  shelf_regiondata.active_shelf = &shelf;
  BLI_assert(BLI_findindex(&shelf_regiondata.shelves, &shelf) > -1);
  BLI_remlink(&shelf_regiondata.shelves, &shelf);
  BLI_addhead(&shelf_regiondata.shelves, &shelf);
}

/**
 * Determine and set the currently active asset shelf, creating a new shelf if needed.
 *
 * The heuristic works as follows:
 * 1) If the currently active shelf is still valid (poll succeeds), keep it active.
 * 2) Otherwise, check for previously activated shelves in \a shelf_regiondata and activate the
 *    first valid one (first with a succeeding poll).
 * 3) If none is valid, check all shelf-types available for \a space_type, create a new shelf for
 *    the first type that is valid (poll succeeds), and activate it.
 * 4) If no shelf-type is valid, #RegionAssetShelf.active_shelf is set to null.
 *
 * When activating a shelf, it is moved to the beginning of the #RegionAssetShelf.shelves list, so
 * that recently activated shelves are also the first ones to be reactivated.
 *
 * The returned shelf is guaranteed to have its #AssetShelf.type pointer set.
 *
 * \param on_create: Function called when a new asset shelf is created (case 3).
 *
 * \return A non-owning pointer to the now active shelf. Might be null if no shelf is valid in
 *         current context (all polls failed).
 */
static AssetShelf *update_active_shelf(const bContext &C,
                                       const eSpace_Type space_type,
                                       RegionAssetShelf &shelf_regiondata,
                                       FunctionRef<void(AssetShelf &new_shelf)> on_create,
                                       FunctionRef<void(AssetShelf &shelf)> on_reactivate)
{
  /* NOTE: Don't access #AssetShelf.type directly, use #type_ensure(). */

  /* Case 1: */
  if (shelf_regiondata.active_shelf &&
      type_poll_for_non_popup(
          C, ensure_shelf_has_type(*shelf_regiondata.active_shelf), space_type))
  {
    /* Not a strong precondition, but if this is wrong something weird might be going on. */
    BLI_assert(shelf_regiondata.active_shelf == shelf_regiondata.shelves.first);
    return shelf_regiondata.active_shelf;
  }

  /* Case 2 (no active shelf or the poll of it isn't succeeding anymore. Poll all shelf types to
   * determine a new active one): */
  LISTBASE_FOREACH (AssetShelf *, shelf, &shelf_regiondata.shelves) {
    if (shelf == shelf_regiondata.active_shelf) {
      continue;
    }

    if (type_poll_for_non_popup(C, ensure_shelf_has_type(*shelf), space_type)) {
      /* Found a valid previously activated shelf, reactivate it. */
      activate_shelf(shelf_regiondata, *shelf);
      if (on_reactivate) {
        on_reactivate(*shelf);
      }
      return shelf;
    }
  }

  /* Case 3: */
  for (const std::unique_ptr<AssetShelfType> &shelf_type : static_shelf_types()) {
    if (type_poll_for_non_popup(C, shelf_type.get(), space_type)) {
      AssetShelf *new_shelf = create_shelf_from_type(*shelf_type);
      BLI_addhead(&shelf_regiondata.shelves, new_shelf);
      /* Moves ownership to the regiondata. */
      activate_shelf(shelf_regiondata, *new_shelf);
      if (on_create) {
        on_create(*new_shelf);
      }
      return new_shelf;
    }
  }

  shelf_regiondata.active_shelf = nullptr;
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Regions
 * \{ */

void *region_duplicate(void *regiondata)
{
  const RegionAssetShelf *shelf_regiondata = static_cast<RegionAssetShelf *>(regiondata);
  if (!shelf_regiondata) {
    return nullptr;
  }

  return regiondata_duplicate(shelf_regiondata);
}

void region_free(ARegion *region)
{
  RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(*region);
  if (shelf_regiondata) {
    regiondata_free(shelf_regiondata);
  }
  region->regiondata = nullptr;
}

/**
 * Check if there is any asset shelf type in this space returning true in its poll. If not, no
 * asset shelf region should be displayed.
 */
static bool asset_shelf_space_poll(const bContext *C, const SpaceLink *space_link)
{
  /* Is there any asset shelf type registered that returns true for it's poll? */
  for (const std::unique_ptr<AssetShelfType> &shelf_type : static_shelf_types()) {
    if (type_poll_for_non_popup(*C, shelf_type.get(), space_link->spacetype)) {
      return true;
    }
  }

  return false;
}

bool regions_poll(const RegionPollParams *params)
{
  return asset_shelf_space_poll(params->context,
                                static_cast<SpaceLink *>(params->area->spacedata.first));
}

static void asset_shelf_region_listen(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SPACE:
      if (wmn->data == ND_REGIONS_ASSET_SHELF) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      /* Asset shelf polls typically check the mode. */
      if (ELEM(wmn->data, ND_MODE)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ASSET:
      ED_region_tag_redraw(region);
      break;
  }
}

void region_listen(const wmRegionListenerParams *params)
{
  if (list::listen(params->notifier)) {
    ED_region_tag_redraw_no_rebuild(params->region);
  }
  /* If the asset list didn't catch the notifier, let the region itself listen. */
  else {
    asset_shelf_region_listen(params);
  }
}

void region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  WorkSpace *workspace = params->workspace;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;
  WM_msg_subscribe_rna_prop(
      mbus, &workspace->id, workspace, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
}

void region_init(wmWindowManager *wm, ARegion *region)
{
  /* Region-data should've been created by a previously called #region_on_poll_success(). */
  RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(*region);
  BLI_assert_msg(
      shelf_regiondata,
      "Region-data should've been created by a previously called `region_on_poll_success()`.");

  AssetShelf *active_shelf = shelf_regiondata->active_shelf;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_PANELS_UI, region->winx, region->winy);

  wmKeyMap *keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "View2D Buttons List", SPACE_EMPTY, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;
  region->v2d.keepofs |= V2D_KEEPOFS_Y;
  region->v2d.keeptot |= V2D_KEEPTOT_STRICT;

  region->v2d.flag |= V2D_SNAP_TO_PAGESIZE_Y;
  region->v2d.page_size_y = active_shelf ? tile_height(active_shelf->settings) :
                                           asset_shelf_default_tile_height();

  /* Ensure the view is snapped to a page still, especially for DPI changes. */
  UI_view2d_offset_y_snap_to_closest_page(&region->v2d);
}

static int main_region_padding_y()
{
  const uiStyle *style = UI_style_get_dpi();
  return style->buttonspacey / 2;
}

static int main_region_padding_x()
{
  /* Use the same as the height, equal padding looks nice. */
  return main_region_padding_y();
}

static int current_tile_draw_height(const ARegion *region)
{
  const RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(
      *region);
  const float aspect = BLI_rctf_size_y(&region->v2d.cur) /
                       (BLI_rcti_size_y(&region->v2d.mask) + 1);

  /* It can happen that this function is called before the region is actually initialized, when
   * user clicks & drags slightly on the 'up arrow' icon of the shelf. */
  const AssetShelf *active_shelf = shelf_regiondata ? shelf_regiondata->active_shelf : nullptr;
  return (active_shelf ? tile_height(active_shelf->settings) : asset_shelf_default_tile_height()) /
         (IS_EQF(aspect, 0) ? 1.0f : aspect);
}

/**
 * How many rows fit into the region (accounting for padding).
 */
static int calculate_row_count_from_tile_draw_height(const int region_height_scaled,
                                                     const int tile_draw_height)
{
  return std::max(1, int((region_height_scaled - 2 * main_region_padding_y()) / tile_draw_height));
}

static int calculate_scaled_region_height_from_row_count(const int row_count,
                                                         const int tile_draw_height)
{
  return (row_count * tile_draw_height + 2 * main_region_padding_y());
}

int region_snap(const ARegion *region, const int size, const int axis)
{
  /* Only on Y axis. */
  if (axis != 1) {
    return size;
  }

  /* Using scaled values only simplifies things. Simply divide the result by the scale again. */

  const int tile_height = current_tile_draw_height(region);

  const int row_count = calculate_row_count_from_tile_draw_height(size * UI_SCALE_FAC,
                                                                  tile_height);

  const int new_size_scaled = calculate_scaled_region_height_from_row_count(row_count,
                                                                            tile_height);
  return new_size_scaled / UI_SCALE_FAC;
}

/**
 * Ensure the region height matches the preferred row count (see #AssetShelf.preferred_row_count)
 * as closely as possible while still fitting within the area. In any case, this will ensure the
 * region height is snapped to a multiple of the row count (plus region padding).
 */
static void region_resize_to_preferred(ScrArea *area, ARegion *region)
{
  const RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(
      *region);
  const AssetShelf *active_shelf = shelf_regiondata->active_shelf;

  BLI_assert(active_shelf->preferred_row_count > 0);
  const int tile_height = current_tile_draw_height(region);

  /* Prevent the AssetShelf from getting too high (and thus being hidden) in case many rows are
   * used and preview size is increased. */
  const int size_y_avail = ED_area_max_regionsize(area, region, AE_TOP_TO_BOTTOMRIGHT);
  const short int max_row_count = calculate_row_count_from_tile_draw_height(
      size_y_avail * UI_SCALE_FAC, tile_height);

  const int new_size_y = calculate_scaled_region_height_from_row_count(
                             std::min(max_row_count, active_shelf->preferred_row_count),
                             tile_height) /
                         UI_SCALE_FAC;

  if (region->sizey != new_size_y) {
    region->sizey = new_size_y;
    ED_area_tag_region_size_update(area, region);
  }
}

void region_on_user_resize(const ARegion *region)
{
  const RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(
      *region);
  AssetShelf *active_shelf = shelf_regiondata->active_shelf;
  if (!active_shelf) {
    return;
  }

  const int tile_height = current_tile_draw_height(region);
  active_shelf->preferred_row_count = calculate_row_count_from_tile_draw_height(
      region->sizey * UI_SCALE_FAC, tile_height);
}

int tile_width(const AssetShelfSettings &settings)
{
  return UI_preview_tile_size_x(settings.preview_size);
}

int tile_height(const AssetShelfSettings &settings)
{
  return (settings.display_flag & ASSETSHELF_SHOW_NAMES) ?
             UI_preview_tile_size_y(settings.preview_size) :
             UI_preview_tile_size_y_no_label(settings.preview_size);
}

static int asset_shelf_default_tile_height()
{
  return UI_preview_tile_size_x(ASSET_SHELF_PREVIEW_SIZE_DEFAULT);
}

int region_prefsizey()
{
  /* One row by default (plus padding). */
  return asset_shelf_default_tile_height() + 2 * main_region_padding_y();
}

void region_layout(const bContext *C, ARegion *region)
{
  RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(*region);
  BLI_assert_msg(
      shelf_regiondata,
      "Region-data should've been created by a previously called `region_on_poll_success()`.");

  const AssetShelf *active_shelf = shelf_regiondata->active_shelf;
  if (!active_shelf) {
    return;
  }

  uiBlock *block = UI_block_begin(C, region, __func__, ui::EmbossType::Emboss);

  const uiStyle *style = UI_style_get_dpi();
  const int padding_y = main_region_padding_y();
  const int padding_x = main_region_padding_x();
  uiLayout &layout = ui::block_layout(block,
                                      ui::LayoutDirection::Vertical,
                                      ui::LayoutType::Panel,
                                      padding_x,
                                      -padding_y,
                                      region->winx - 2 * padding_x,
                                      0,
                                      0,
                                      style);

  build_asset_view(layout, active_shelf->settings.asset_library_reference, *active_shelf, *C);

  int layout_height = ui::block_layout_resolve(block).y;
  BLI_assert(layout_height <= 0);
  UI_view2d_totRect_set(&region->v2d, region->winx - 1, layout_height - padding_y);
  UI_view2d_curRect_validate(&region->v2d);

  region_resize_to_preferred(CTX_wm_area(C), region);

  /* View2D matrix might have changed due to dynamic sized regions.
   * Without this, tooltips jump around, see #129347. Reason is that #UI_but_tooltip_refresh() is
   * called as part of #UI_block_end(), so the block's window matrix needs to be up-to-date. */
  {
    UI_view2d_view_ortho(&region->v2d);
    UI_blocklist_update_window_matrix(C, &region->runtime->uiblocks);
  }

  UI_block_end(C, block);
}

void region_draw(const bContext *C, ARegion *region)
{
  ED_region_clear(C, region, TH_BACK);

  /* Set view2d view matrix for scrolling. */
  UI_view2d_view_ortho(&region->v2d);

  /* View2D matrix might have changed due to dynamic sized regions. */
  UI_blocklist_update_window_matrix(C, &region->runtime->uiblocks);

  UI_blocklist_draw(C, &region->runtime->uiblocks);

  /* Restore view matrix. */
  UI_view2d_view_restore(C);

  UI_view2d_scrollers_draw(&region->v2d, nullptr);
}

void region_on_poll_success(const bContext *C, ARegion *region)
{
  RegionAssetShelf *shelf_regiondata = RegionAssetShelf::ensure_from_asset_shelf_region(*region);
  if (!shelf_regiondata) {
    BLI_assert_unreachable();
    return;
  }

  const int old_region_flag = region->flag;

  ScrArea *area = CTX_wm_area(C);
  update_active_shelf(
      *C,
      eSpace_Type(area->spacetype),
      *shelf_regiondata,
      /*on_create=*/
      [&](AssetShelf &new_shelf) {
        /* Set region visibility for first time shelf is created (`'DEFAULT_VISIBLE'` option). */
        SET_FLAG_FROM_TEST(region->flag,
                           (new_shelf.type->flag & ASSET_SHELF_TYPE_FLAG_DEFAULT_VISIBLE) == 0,
                           RGN_FLAG_HIDDEN);
      },
      /*on_reactivate=*/
      [&](AssetShelf &shelf) {
        /* Restore region visibility from previous asset shelf instantiation when reactivating. */
        SET_FLAG_FROM_TEST(
            region->flag, shelf.instance_flag & ASSETSHELF_REGION_IS_HIDDEN, RGN_FLAG_HIDDEN);
      });

  if (old_region_flag != region->flag) {
    ED_region_visibility_change_update(const_cast<bContext *>(C), area, region);
  }

  if (shelf_regiondata->active_shelf) {
    /* Remember current visibility state of the region in the shelf, so we can restore it on
     * reactivation. */
    SET_FLAG_FROM_TEST(shelf_regiondata->active_shelf->instance_flag,
                       region->flag & (RGN_FLAG_HIDDEN | RGN_FLAG_HIDDEN_BY_USER),
                       ASSETSHELF_REGION_IS_HIDDEN);
  }
}

void header_region_listen(const wmRegionListenerParams *params)
{
  asset_shelf_region_listen(params);
}

void header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
  region->alignment |= RGN_SPLIT_SCALE_PREV;
  region->flag |= RGN_FLAG_RESIZE_RESPECT_BUTTON_SECTIONS;
}

void header_region(const bContext *C, ARegion *region)
{
  ED_region_header_with_button_sections(C, region, uiButtonSectionsAlign::Bottom);
}

int header_region_size()
{
  /* Use a height that lets widgets sit just on top of the separator line drawn at the lower edge
   * of the region (widgets will be centered).
   *
   * Note that this is usually a bit less than the header size. The asset shelf tends to look like
   * a separate area, so making the shelf header smaller than a header helps. */
  return UI_UNIT_Y + (UI_BUTTON_SECTION_SEPERATOR_LINE_WITH * 2);
}

void region_blend_read_data(BlendDataReader *reader, ARegion *region)
{
  RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(*region);
  if (!shelf_regiondata) {
    return;
  }
  regiondata_blend_read_data(reader, &shelf_regiondata);
  region->regiondata = shelf_regiondata;
}

void region_blend_write(BlendWriter *writer, ARegion *region)
{
  RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(*region);
  if (!shelf_regiondata) {
    return;
  }
  regiondata_blend_write(writer, shelf_regiondata);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Context
 * \{ */

AssetShelf *active_shelf_from_area(const ScrArea *area)
{
  const ARegion *shelf_region = BKE_area_find_region_type(area, RGN_TYPE_ASSET_SHELF);
  if (!shelf_region) {
    /* Called in wrong context, area doesn't have a shelf. */
    return nullptr;
  }

  if (shelf_region->flag & RGN_FLAG_POLL_FAILED) {
    /* Don't return data when the region "doesn't exist" (poll failed). */
    return nullptr;
  }

  const RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(
      *shelf_region);
  if (!shelf_regiondata) {
    return nullptr;
  }

  return shelf_regiondata->active_shelf;
}

int context(const bContext *C, const char *member, bContextDataResult *result)
{
  static const char *context_dir[] = {
      "asset_shelf",
      "asset_library_reference",
      "asset",
      nullptr,
  };

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, context_dir);
    return CTX_RESULT_OK;
  }

  bScreen *screen = CTX_wm_screen(C);

  if (CTX_data_equals(member, "asset_shelf")) {
    AssetShelf *active_shelf = active_shelf_from_area(CTX_wm_area(C));
    if (!active_shelf) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set(result, &screen->id, &RNA_AssetShelf, active_shelf);
    return CTX_RESULT_OK;
  }

  if (CTX_data_equals(member, "asset_library_reference")) {
    AssetShelf *active_shelf = active_shelf_from_area(CTX_wm_area(C));
    if (!active_shelf) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set(result,
                         &screen->id,
                         &RNA_AssetLibraryReference,
                         &active_shelf->settings.asset_library_reference);
    return CTX_RESULT_OK;
  }

  if (CTX_data_equals(member, "asset")) {
    const ARegion *region = CTX_wm_region(C);
    const uiBut *but = UI_region_views_find_active_item_but(region);
    if (!but) {
      return CTX_RESULT_NO_DATA;
    }

    const bContextStore *but_context = UI_but_context_get(but);
    if (!but_context) {
      return CTX_RESULT_NO_DATA;
    }

    const PointerRNA *asset_ptr = CTX_store_ptr_lookup(
        but_context, "asset", &RNA_AssetRepresentation);
    if (!asset_ptr) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set_ptr(result, asset_ptr);
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

static PointerRNA active_shelf_ptr_from_context(const bContext *C)
{
  return CTX_data_pointer_get_type(C, "asset_shelf", &RNA_AssetShelf);
}

AssetShelf *active_shelf_from_context(const bContext *C)
{
  PointerRNA shelf_settings_ptr = active_shelf_ptr_from_context(C);
  return static_cast<AssetShelf *>(shelf_settings_ptr.data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Catalog toggle buttons
 * \{ */

static uiBut *add_tab_button(uiBlock &block, StringRefNull name)
{
  const uiStyle *style = UI_style_get_dpi();
  const int string_width = UI_fontstyle_string_width(&style->widget, name.c_str());
  const int pad_x = UI_UNIT_X * 0.3f;
  const int but_width = std::min(string_width + 2 * pad_x, UI_UNIT_X * 8);

  uiBut *but = uiDefBut(
      &block,
      ButType::Tab,
      0,
      name,
      0,
      0,
      but_width,
      UI_UNIT_Y,
      nullptr,
      0,
      0,
      TIP_("Enable catalog, making contained assets visible in the asset shelf"));

  UI_but_drawflag_enable(but, UI_BUT_ALIGN_DOWN);
  UI_but_flag_disable(but, UI_BUT_UNDO);

  return but;
}

static void add_catalog_tabs(AssetShelf &shelf, uiLayout &layout)
{
  uiBlock *block = layout.block();
  AssetShelfSettings &shelf_settings = shelf.settings;

  /* "All" tab. */
  {
    uiBut *but = add_tab_button(*block, IFACE_("All"));
    UI_but_func_set(but, [&shelf_settings](bContext &C) {
      settings_set_all_catalog_active(shelf_settings);
      send_redraw_notifier(C);
    });
    UI_but_func_pushed_state_set(but, [&shelf_settings](const uiBut &) -> bool {
      return settings_is_all_catalog_active(shelf_settings);
    });
  }

  layout.separator();

  /* Regular catalog tabs. */
  settings_foreach_enabled_catalog_path(shelf, [&](const asset_system::AssetCatalogPath &path) {
    uiBut *but = add_tab_button(*block, path.name());

    UI_but_func_set(but, [&shelf_settings, path](bContext &C) {
      settings_set_active_catalog(shelf_settings, path);
      send_redraw_notifier(C);
    });
    UI_but_func_pushed_state_set(but, [&shelf_settings, path](const uiBut &) -> bool {
      return settings_is_active_catalog(shelf_settings, path);
    });
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Header Region
 *
 * Implemented as HeaderType for #RGN_TYPE_ASSET_SHELF_HEADER.
 * \{ */

static void asset_shelf_header_draw(const bContext *C, Header *header)
{
  uiLayout *layout = header->layout;
  uiBlock *block = layout->block();
  const AssetLibraryReference *library_ref = CTX_wm_asset_library_ref(C);

  list::storage_fetch(library_ref, C);

  UI_block_emboss_set(block, ui::EmbossType::None);
  layout->popover(C, "ASSETSHELF_PT_catalog_selector", "", ICON_COLLAPSEMENU);
  UI_block_emboss_set(block, ui::EmbossType::Emboss);

  layout->separator();

  PointerRNA shelf_ptr = active_shelf_ptr_from_context(C);
  if (AssetShelf *shelf = static_cast<AssetShelf *>(shelf_ptr.data)) {
    add_catalog_tabs(*shelf, *layout);
  }

  layout->separator_spacer();

  layout->popover(C, "ASSETSHELF_PT_display", "", ICON_IMGDISPLAY);
  uiLayout *sub = &layout->row(false);
  /* Same as file/asset browser header. */
  sub->ui_units_x_set(8);
  sub->prop(&shelf_ptr, "search_filter", UI_ITEM_NONE, "", ICON_VIEWZOOM);
}

static void header_regiontype_register(ARegionType *region_type, const int space_type)
{
  HeaderType *ht = MEM_callocN<HeaderType>(__func__);
  STRNCPY_UTF8(ht->idname, "ASSETSHELF_HT_settings");
  ht->space_type = space_type;
  ht->region_type = RGN_TYPE_ASSET_SHELF_HEADER;
  ht->draw = asset_shelf_header_draw;
  ht->poll = [](const bContext *C, HeaderType *) {
    return asset_shelf_space_poll(C, CTX_wm_space_data(C));
  };

  BLI_addtail(&region_type->headertypes, ht);
}

void types_register(ARegionType *region_type, const int space_type)
{
  header_regiontype_register(region_type, space_type);
  catalog_selector_panel_register(region_type);
  popover_panel_register(region_type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Type (un)registration
 * \{ */

void type_unlink(const Main &bmain, const AssetShelfType &shelf_type)
{
  LISTBASE_FOREACH (bScreen *, screen, &bmain.screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
        LISTBASE_FOREACH (ARegion *, region, regionbase) {
          if (region->regiontype != RGN_TYPE_ASSET_SHELF) {
            continue;
          }

          RegionAssetShelf *shelf_regiondata = RegionAssetShelf::get_from_asset_shelf_region(
              *region);
          if (!shelf_regiondata) {
            continue;
          }
          LISTBASE_FOREACH (AssetShelf *, shelf, &shelf_regiondata->shelves) {
            if (shelf->type == &shelf_type) {
              shelf->type = nullptr;
            }
          }

          BLI_assert((shelf_regiondata->active_shelf == nullptr) ||
                     (shelf_regiondata->active_shelf->type != &shelf_type));
        }
      }
    }
  }

  type_popup_unlink(shelf_type);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name External helpers
 * \{ */

void show_catalog_in_visible_shelves(const bContext &C, const StringRefNull catalog_path)
{
  wmWindowManager *wm = CTX_wm_manager(&C);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    const bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (AssetShelf *shelf = asset::shelf::active_shelf_from_area(area)) {
        settings_set_catalog_path_enabled(*shelf, catalog_path.c_str());
      }
    }
  }
}

/** \} */

}  // namespace blender::ed::asset::shelf

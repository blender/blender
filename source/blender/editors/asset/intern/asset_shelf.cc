/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 *
 * General asset shelf code, mostly region callbacks, drawing and context stuff.
 */

#include <algorithm>

#include "AS_asset_catalog_path.hh"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "BLT_translation.h"

#include "DNA_screen_types.h"

#include "ED_asset_list.h"
#include "ED_screen.h"

#include "RNA_prototypes.h"

#include "UI_interface.h"
#include "UI_interface.hh"
#include "UI_resources.h"
#include "UI_tree_view.hh"
#include "UI_view2d.h"

#include "WM_api.h"

#include "ED_asset_shelf.h"
#include "asset_shelf.hh"

using namespace blender;
using namespace blender::ed::asset;

namespace blender::ed::asset::shelf {

void send_redraw_notifier(const bContext &C)
{
  WM_event_add_notifier(&C, NC_SPACE | ND_SPACE_ASSET_SHELF, nullptr);
}

}  // namespace blender::ed::asset::shelf

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Regions
 * \{ */

static bool asset_shelf_poll(const bContext *C, const SpaceLink *space_link)
{
  const SpaceType *space_type = BKE_spacetype_from_id(space_link->spacetype);

  /* Is there any asset shelf type registered that returns true for it's poll? */
  LISTBASE_FOREACH (AssetShelfType *, shelf_type, &space_type->asset_shelf_types) {
    if (shelf_type->poll && shelf_type->poll(C, shelf_type)) {
      return true;
    }
  }

  return false;
}

bool ED_asset_shelf_poll(const RegionPollParams *params)
{
  return asset_shelf_poll(params->context,
                          static_cast<SpaceLink *>(params->area->spacedata.first));
}

static void asset_shelf_region_listen(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SPACE:
      if (wmn->data == ND_SPACE_ASSET_SHELF) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCENE:
      /* Asset shelf polls typically check the mode. */
      if (ELEM(wmn->data, ND_MODE)) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

void ED_asset_shelf_region_listen(const wmRegionListenerParams *params)
{
  if (ED_assetlist_listen(params->notifier)) {
    ED_region_tag_redraw_no_rebuild(params->region);
  }
  /* If the asset list didn't catch the notifier, let the region itself listen. */
  else {
    asset_shelf_region_listen(params);
  }
}

void ED_asset_shelf_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);
  region->v2d.scroll = V2D_SCROLL_RIGHT;
  region->v2d.page_size_y = ED_asset_shelf_default_tile_height();
}

static int main_region_padding_y()
{
  const uiStyle *style = UI_style_get_dpi();
  return style->buttonspacey / 2;
}

static int main_region_padding_x()
{
  const uiStyle *style = UI_style_get_dpi();
  return style->buttonspacex;
}

int ED_asset_shelf_region_snap(const ARegion *region, const int size, const int axis)
{
  /* Only on Y axis. */
  if (axis != 1) {
    return size;
  }

  const int size_scaled = size * UI_SCALE_FAC;

  const float aspect = BLI_rctf_size_y(&region->v2d.cur) /
                       (BLI_rcti_size_y(&region->v2d.mask) + 1);
  const float tile_height = ED_asset_shelf_default_tile_height() /
                            (IS_EQF(aspect, 0) ? 1.0f : aspect);

  const int region_padding = main_region_padding_y();

  /* How many rows fit into the region (accounting for padding). */
  const int rows = std::max(1, int((size_scaled - 2 * region_padding) / tile_height));

  const int new_size_scaled = (rows * tile_height + 2 * region_padding);
  return new_size_scaled / UI_SCALE_FAC;
}

int ED_asset_shelf_default_tile_width()
{
  return UI_preview_tile_size_x() * 0.65f;
}

int ED_asset_shelf_default_tile_height()
{
  return UI_preview_tile_size_y() * 0.65f;
}

int ED_asset_shelf_region_prefsizey()
{
  /* One row by default (plus padding). */
  return ED_asset_shelf_default_tile_height() + 2 * main_region_padding_y();
}

/**
 * Check if there is any asset shelf type returning true in it's poll. If not, no asset shelf
 * region should be displayed.
 */
static bool asset_shelf_region_header_type_poll(const bContext *C, HeaderType * /*header_type*/)
{
  return asset_shelf_poll(C, CTX_wm_space_data(C));
}

void ED_asset_shelf_region_draw(const bContext *C,
                                ARegion *region,
                                AssetShelfSettings *shelf_settings)
{
  AssetLibraryReference all_library_ref = {};
  all_library_ref.type = ASSET_LIBRARY_ALL;
  all_library_ref.custom_library_index = -1;

  ED_region_clear(C, region, TH_BACK);

  /* Set view2d view matrix for scrolling. */
  UI_view2d_view_ortho(&region->v2d);

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);

  const uiStyle *style = UI_style_get();
  const float padding_y = main_region_padding_y();
  const float padding_x = main_region_padding_x();
  const float scale_x = UI_view2d_scale_get_x(&region->v2d);
  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     padding_x,
                                     -padding_y,
                                     (region->winx - 2 * padding_x) / scale_x,
                                     1,
                                     0,
                                     style);

  shelf::build_asset_view(*layout, all_library_ref, shelf_settings, *C, *region);

  int layout_height;
  UI_block_layout_resolve(block, nullptr, &layout_height);
  UI_view2d_totRect_set(&region->v2d, region->winx, layout_height);
  UI_view2d_curRect_validate(&region->v2d);

  UI_block_end(C, block);
  UI_block_draw(C, block);

  /* Restore view matrix. */
  UI_view2d_view_restore(C);

  UI_view2d_scrollers_draw(&region->v2d, nullptr);
}

void ED_asset_shelf_footer_region_listen(const wmRegionListenerParams *params)
{
  asset_shelf_region_listen(params);
}

void ED_asset_shelf_footer_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

void ED_asset_shelf_footer_region(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

int ED_asset_shelf_footer_size()
{
  /* A little smaller than a regular header. */
  return ED_area_headersize() * 0.9f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Context
 * \{ */

int ED_asset_shelf_context(const bContext *C,
                           const char *member,
                           bContextDataResult *result,
                           AssetShelfSettings *shelf_settings)
{
  static const char *context_dir[] = {
      "asset_shelf_settings",
      "active_file", /* XXX yuk... */
      nullptr,
  };

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, context_dir);
    return CTX_RESULT_OK;
  }

  bScreen *screen = CTX_wm_screen(C);

  if (CTX_data_equals(member, "asset_shelf_settings")) {
    CTX_data_pointer_set(result, &screen->id, &RNA_AssetShelfSettings, shelf_settings);

    return CTX_RESULT_OK;
  }

  /* XXX hack. Get the asset from the hovered button, but needs to be the file... */
  if (CTX_data_equals(member, "active_file")) {
    const uiBut *but = UI_context_active_but_get(C);
    if (!but) {
      return CTX_RESULT_NO_DATA;
    }

    const bContextStore *but_context = UI_but_context_get(but);
    if (!but_context) {
      return CTX_RESULT_NO_DATA;
    }

    const PointerRNA *file_ptr = CTX_store_ptr_lookup(
        but_context, "active_file", &RNA_FileSelectEntry);
    if (!file_ptr) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set_ptr(result, file_ptr);
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

namespace blender::ed::asset::shelf {

AssetShelfSettings *settings_from_context(const bContext *C)
{
  PointerRNA shelf_settings_ptr = CTX_data_pointer_get_type(
      C, "asset_shelf_settings", &RNA_AssetShelfSettings);
  return static_cast<AssetShelfSettings *>(shelf_settings_ptr.data);
}

}  // namespace blender::ed::asset::shelf

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

  uiBut *but = uiDefBut(&block,
                        UI_BTYPE_TAB,
                        0,
                        name.c_str(),
                        0,
                        0,
                        but_width,
                        UI_UNIT_Y,
                        nullptr,
                        0,
                        0,
                        0,
                        0,
                        "Enable catalog, making contained assets visible in the asset shelf");

  UI_but_drawflag_enable(but, UI_BUT_ALIGN_TOP);

  return but;
}

static void add_catalog_toggle_buttons(AssetShelfSettings &shelf_settings, uiLayout &layout)
{
  uiBlock *block = uiLayoutGetBlock(&layout);

  /* "All" tab. */
  {
    uiBut *but = add_tab_button(*block, IFACE_("All"));
    UI_but_func_set(but, [&shelf_settings](bContext &C) {
      shelf::settings_set_all_catalog_active(shelf_settings);
      shelf::send_redraw_notifier(C);
    });
    UI_but_func_pushed_state_set(but, [&shelf_settings](const uiBut &) -> bool {
      return shelf::settings_is_all_catalog_active(shelf_settings);
    });
  }

  uiItemS(&layout);

  /* Regular catalog tabs. */
  shelf::settings_foreach_enabled_catalog_path(
      shelf_settings, [&shelf_settings, block](const asset_system::AssetCatalogPath &path) {
        uiBut *but = add_tab_button(*block, path.name());

        UI_but_func_set(but, [&shelf_settings, path](bContext &C) {
          shelf::settings_set_active_catalog(shelf_settings, path);
          shelf::send_redraw_notifier(C);
        });
        UI_but_func_pushed_state_set(but, [&shelf_settings, path](const uiBut &) -> bool {
          return shelf::settings_is_active_catalog(shelf_settings, path);
        });
      });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asset Shelf Footer
 *
 * Implemented as HeaderType for #RGN_TYPE_ASSET_SHELF_FOOTER.
 * \{ */

static void asset_shelf_footer_draw(const bContext *C, Header *header)
{
  uiLayout *layout = header->layout;
  uiBlock *block = uiLayoutGetBlock(layout);
  const AssetLibraryReference *library_ref = CTX_wm_asset_library_ref(C);

  ED_assetlist_storage_fetch(library_ref, C);
  uiDefIconBlockBut(block,
                    shelf::catalog_selector_block_draw,
                    nullptr,
                    0,
                    ICON_RIGHTARROW,
                    0,
                    0,
                    UI_UNIT_X * 1.5f,
                    UI_UNIT_Y,
                    TIP_("Select catalogs to display"));

  uiItemS(layout);

  AssetShelfSettings *shelf_settings = shelf::settings_from_context(C);
  if (shelf_settings) {
    add_catalog_toggle_buttons(*shelf_settings, *layout);
  }

  uiItemSpacer(layout);

  uiItemPopoverPanel(layout, C, "ASSETSHELF_PT_display", "", ICON_IMGDISPLAY);
}

void ED_asset_shelf_footer_register(ARegionType *region_type,
                                    const char *idname,
                                    const int space_type)
{
  HeaderType *ht = MEM_cnew<HeaderType>(__func__);
  strcpy(ht->idname, idname);
  ht->space_type = space_type;
  ht->region_type = RGN_TYPE_ASSET_SHELF_FOOTER;
  ht->draw = asset_shelf_footer_draw;
  ht->poll = asset_shelf_region_header_type_poll;
  BLI_addtail(&region_type->headertypes, ht);
}

/** \} */

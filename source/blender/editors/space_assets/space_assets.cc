/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */

#include <cstring>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_asset.h"
#include "BKE_screen.h"

#include "BLI_listbase.h"

#include "ED_asset.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_message.hh"

#include "asset_browser_intern.hh"
#include "asset_view.hh"

/* ---------------------------------------------------------------------- */
/* Asset Browser Space */

static SpaceLink *asset_browser_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceAssets *assets_space = MEM_cnew<SpaceAssets>("asset browser space");
  assets_space->spacetype = SPACE_ASSETS;

  BKE_asset_library_reference_init_default(&assets_space->asset_library_ref);

  {
    /* Header. */
    ARegion *region = MEM_cnew<ARegion>("asset browser header");
    BLI_addtail(&assets_space->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  }

  {
    /* Navigation region */
    ARegion *region = MEM_cnew<ARegion>("asset browser navigation region");

    BLI_addtail(&assets_space->regionbase, region);
    region->regiontype = RGN_TYPE_NAV_BAR;
    region->alignment = RGN_ALIGN_LEFT;
  }

  {
    /* Sidebar region */
    ARegion *region = MEM_cnew<ARegion>("asset browser sidebar region");

    BLI_addtail(&assets_space->regionbase, region);
    region->regiontype = RGN_TYPE_UI;
    region->alignment = RGN_ALIGN_RIGHT;
    region->flag = RGN_FLAG_HIDDEN;
  }

  {
    /* Main region. */
    ARegion *region = MEM_cnew<ARegion>("asset browser main region");
    BLI_addtail(&assets_space->regionbase, region);
    region->regiontype = RGN_TYPE_WINDOW;

    region->v2d.scroll = V2D_SCROLL_RIGHT;
    region->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
    region->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
    region->v2d.keeptot = V2D_KEEPTOT_STRICT;
    region->v2d.minzoom = region->v2d.maxzoom = 1.0f;
  }

  return (SpaceLink *)assets_space;
}

static void asset_browser_free(SpaceLink * /*sl*/) {}

static void asset_browser_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *asset_browser_duplicate(SpaceLink *sl)
{
  const SpaceAssets *asset_browser_old = (SpaceAssets *)sl;
  SpaceAssets *asset_browser_new = reinterpret_cast<SpaceAssets *>(
      MEM_dupallocN(asset_browser_old));

  return (SpaceLink *)asset_browser_new;
}

static void asset_browser_keymap(wmKeyConfig *keyconf)
{
  /* keys for all regions */
  WM_keymap_ensure(keyconf, "Asset Browser", SPACE_ASSETS, 0);
}

const char *asset_browser_context_dir[] = {
    "asset_handle",
    "asset_library_ref",
    NULL,
};

static int /*eContextResult*/ asset_browser_context(const bContext *C,
                                                    const char *member,
                                                    bContextDataResult *result)
{
  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, asset_browser_context_dir);
    return CTX_RESULT_OK;
  }

  bScreen *screen = CTX_wm_screen(C);
  SpaceAssets *assets_space = CTX_wm_space_assets(C);

  if (CTX_data_equals(member, "asset_library_ref")) {
    CTX_data_pointer_set(
        result, &screen->id, &RNA_AssetLibraryReference, &assets_space->asset_library_ref);
    return CTX_RESULT_OK;
  }

  if (CTX_data_equals(member, "asset_handle")) {
    AssetHandle *asset = ED_assetlist_asset_handle_get_by_index(&assets_space->asset_library_ref,
                                                                assets_space->active_asset_idx);
    if (!asset) {
      return CTX_RESULT_NO_DATA;
    }

    CTX_data_pointer_set(result, &screen->id, &RNA_AssetHandle, asset);
    return CTX_RESULT_OK;
  }

  return CTX_RESULT_MEMBER_NOT_FOUND;
}

/* ---------------------------------------------------------------------- */
/* Main Region */

static void asset_browser_main_region_init(wmWindowManager *wm, ARegion *region)
{
  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  {
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Asset Browser", SPACE_ASSETS, 0);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }
}

static void asset_browser_main_region_listener(const wmRegionListenerParams *params)
{
  const wmNotifier *notifier = params->notifier;
  ARegion *region = params->region;

  switch (notifier->category) {
    case NC_ASSET:
      if (ELEM(notifier->data, ND_SPACE_ASSET_PARAMS)) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void asset_browser_main_region_message_subscribe(
    const wmRegionMessageSubscribeParams *params)
{
  struct wmMsgBus *mbus = params->message_bus;
  bScreen *screen = params->screen;
  SpaceAssets *assets_space = reinterpret_cast<SpaceAssets *>(params->area->spacedata.first);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = params->region;
  msg_sub_value_region_tag_redraw.user_data = params->region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  WM_msg_subscribe_rna_prop(mbus,
                            &screen->id,
                            assets_space,
                            SpaceAssetBrowser,
                            catalog_filter,
                            &msg_sub_value_region_tag_redraw);
}

/* ---------------------------------------------------------------------- */
/* Header Region */

static void asset_browser_header_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void asset_browser_header_listener(const wmRegionListenerParams * /*params*/) {}

/* ---------------------------------------------------------------------- */
/* Navigation Region */

static void asset_browser_navigation_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  {
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Asset Browser", SPACE_ASSETS, 0);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }
}

static void asset_browser_navigation_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void asset_browser_navigation_region_listener(
    const wmRegionListenerParams * /*listener_params*/)
{
}

/* ---------------------------------------------------------------------- */
/* Sidebar Region */

static void asset_browser_sidebar_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  ED_region_panels_init(wm, region);

  {
    wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "Asset Browser", SPACE_ASSETS, 0);
    WM_event_add_keymap_handler(&region->handlers, keymap);
  }
}

static void asset_browser_sidebar_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void asset_browser_sidebar_region_listener(
    const wmRegionListenerParams * /*listener_params*/)
{
}

/* ---------------------------------------------------------------------- */
/* Asset Browser Space-Type */

void ED_spacetype_assets(void)
{
  SpaceType *st = MEM_cnew<SpaceType>("spacetype asset browser");
  ARegionType *art;

  st->spaceid = SPACE_ASSETS;
  strncpy(st->name, "Asset Browser", BKE_ST_MAXNAME);

  st->create = asset_browser_create;
  st->free = asset_browser_free;
  st->init = asset_browser_init;
  st->duplicate = asset_browser_duplicate;
  st->operatortypes = asset_browser_operatortypes;
  st->keymap = asset_browser_keymap;
  st->context = asset_browser_context;

  /* Main region. */
  art = MEM_cnew<ARegionType>("spacetype asset browser main region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = asset_browser_main_region_init;
  art->draw = asset_browser_main_region_draw;
  art->listener = asset_browser_main_region_listener;
  art->message_subscribe = asset_browser_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  BLI_addhead(&st->regiontypes, art);

  /* Header region. */
  art = MEM_cnew<ARegionType>("spacetype asset browser header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->listener = asset_browser_header_listener;
  art->init = asset_browser_header_init;
  art->layout = ED_region_header_layout;
  art->draw = ED_region_header_draw;
  BLI_addhead(&st->regiontypes, art);

  /* Navigation region */
  art = MEM_cnew<ARegionType>("spacetype asset browser navigation region");
  art->regionid = RGN_TYPE_NAV_BAR;
  art->prefsizex = 240;
  art->init = asset_browser_navigation_region_init;
  art->draw = asset_browser_navigation_region_draw;
  art->listener = asset_browser_navigation_region_listener;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_NAVBAR;
  asset_browser_navigation_region_panels_register(art);
  BLI_addhead(&st->regiontypes, art);

  /* Sidebar region */
  art = MEM_cnew<ARegionType>("spacetype asset browser sidebar region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = 240;
  art->init = asset_browser_sidebar_region_init;
  art->draw = asset_browser_sidebar_region_draw;
  art->listener = asset_browser_sidebar_region_listener;
  art->keymapflag = ED_KEYMAP_UI;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}

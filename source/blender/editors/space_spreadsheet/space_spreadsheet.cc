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

#include <cstring>

#include "BLI_listbase.h"

#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "DEG_depsgraph_query.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "spreadsheet_intern.hh"

#include "spreadsheet_from_geometry.hh"
#include "spreadsheet_intern.hh"

using namespace blender::ed::spreadsheet;

static SpaceLink *spreadsheet_create(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  SpaceSpreadsheet *spreadsheet_space = (SpaceSpreadsheet *)MEM_callocN(sizeof(SpaceSpreadsheet),
                                                                        "spreadsheet space");
  spreadsheet_space->spacetype = SPACE_SPREADSHEET;

  {
    /* Header. */
    ARegion *region = (ARegion *)MEM_callocN(sizeof(ARegion), "spreadsheet header");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  }

  {
    /* Footer. */
    ARegion *region = (ARegion *)MEM_callocN(sizeof(ARegion), "spreadsheet footer region");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_FOOTER;
    region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
  }

  {
    /* Main window. */
    ARegion *region = (ARegion *)MEM_callocN(sizeof(ARegion), "spreadsheet main region");
    BLI_addtail(&spreadsheet_space->regionbase, region);
    region->regiontype = RGN_TYPE_WINDOW;
  }

  return (SpaceLink *)spreadsheet_space;
}

static void spreadsheet_free(SpaceLink *sl)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;
  MEM_SAFE_FREE(sspreadsheet->runtime);
}

static void spreadsheet_init(wmWindowManager *UNUSED(wm), ScrArea *area)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)area->spacedata.first;
  if (sspreadsheet->runtime == nullptr) {
    sspreadsheet->runtime = (SpaceSpreadsheet_Runtime *)MEM_callocN(
        sizeof(SpaceSpreadsheet_Runtime), __func__);
  }
}

static SpaceLink *spreadsheet_duplicate(SpaceLink *sl)
{
  const SpaceSpreadsheet *sspreadsheet_old = (SpaceSpreadsheet *)sl;
  SpaceSpreadsheet *sspreadsheet_new = (SpaceSpreadsheet *)MEM_dupallocN(sspreadsheet_old);
  sspreadsheet_new->runtime = (SpaceSpreadsheet_Runtime *)MEM_dupallocN(sspreadsheet_old->runtime);

  return (SpaceLink *)sspreadsheet_new;
}

static void spreadsheet_keymap(wmKeyConfig *UNUSED(keyconf))
{
}

static void spreadsheet_main_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM;
  region->v2d.align = V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y;
  region->v2d.keepzoom = V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT;
  region->v2d.keeptot = V2D_KEEPTOT_STRICT;
  region->v2d.minzoom = region->v2d.maxzoom = 1.0f;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  wmKeyMap *keymap = WM_keymap_ensure(wm->defaultconf, "View2D Buttons List", 0, 0);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

static ID *get_used_id(const bContext *C)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  if (sspreadsheet->pinned_id != nullptr) {
    return sspreadsheet->pinned_id;
  }
  Object *active_object = CTX_data_active_object(C);
  return (ID *)active_object;
}

class FallbackSpreadsheetDrawer : public SpreadsheetDrawer {
};

static std::unique_ptr<SpreadsheetDrawer> generate_spreadsheet_drawer(const bContext *C)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  ID *used_id = get_used_id(C);
  if (used_id == nullptr) {
    return {};
  }
  const ID_Type id_type = GS(used_id->name);
  if (id_type != ID_OB) {
    return {};
  }
  Object *object_orig = (Object *)used_id;
  if (!ELEM(object_orig->type, OB_MESH, OB_POINTCLOUD)) {
    return {};
  }
  Object *object_eval = DEG_get_evaluated_object(depsgraph, object_orig);
  if (object_eval == nullptr) {
    return {};
  }

  return spreadsheet_drawer_from_geometry_attributes(C, object_eval);
}

static void spreadsheet_main_region_draw(const bContext *C, ARegion *region)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  std::unique_ptr<SpreadsheetDrawer> drawer = generate_spreadsheet_drawer(C);
  if (!drawer) {
    sspreadsheet->runtime->visible_rows = 0;
    sspreadsheet->runtime->tot_columns = 0;
    sspreadsheet->runtime->tot_rows = 0;
    drawer = std::make_unique<FallbackSpreadsheetDrawer>();
  }
  draw_spreadsheet_in_region(C, region, *drawer);

  /* Tag footer for redraw, because the main region updates data for the footer. */
  ARegion *footer = BKE_area_find_region_type(CTX_wm_area(C), RGN_TYPE_FOOTER);
  ED_region_tag_redraw(footer);
}

static void spreadsheet_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SCENE: {
      switch (wmn->data) {
        case ND_MODE:
        case ND_FRAME:
        case ND_OB_ACTIVE: {
          ED_region_tag_redraw(region);
          break;
        }
      }
      break;
    }
    case NC_OBJECT: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_SPACE: {
      if (wmn->data == ND_SPACE_SPREADSHEET) {
        ED_region_tag_redraw(region);
      }
      break;
    }
    case NC_GEOM: {
      ED_region_tag_redraw(region);
      break;
    }
  }
}

static void spreadsheet_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void spreadsheet_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void spreadsheet_header_region_free(ARegion *UNUSED(region))
{
}

static void spreadsheet_header_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  wmNotifier *wmn = params->notifier;

  switch (wmn->category) {
    case NC_SCENE: {
      switch (wmn->data) {
        case ND_MODE:
        case ND_OB_ACTIVE: {
          ED_region_tag_redraw(region);
          break;
        }
      }
      break;
    }
    case NC_OBJECT: {
      ED_region_tag_redraw(region);
      break;
    }
    case NC_SPACE: {
      if (wmn->data == ND_SPACE_SPREADSHEET) {
        ED_region_tag_redraw(region);
      }
      break;
    }
    case NC_GEOM: {
      ED_region_tag_redraw(region);
      break;
    }
  }
}

static void spreadsheet_footer_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void spreadsheet_footer_region_draw(const bContext *C, ARegion *region)
{
  SpaceSpreadsheet *sspreadsheet = CTX_wm_space_spreadsheet(C);
  SpaceSpreadsheet_Runtime *runtime = sspreadsheet->runtime;
  std::stringstream ss;
  ss << "Rows: " << runtime->visible_rows << " / " << runtime->tot_rows
     << "   |   Columns: " << runtime->tot_columns;
  std::string stats_str = ss.str();

  UI_ThemeClearColor(TH_BACK);

  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  const uiStyle *style = UI_style_get_dpi();
  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_HORIZONTAL,
                                     UI_LAYOUT_HEADER,
                                     UI_HEADER_OFFSET,
                                     region->winy - (region->winy - UI_UNIT_Y) / 2.0f,
                                     region->sizex,
                                     1,
                                     0,
                                     style);
  uiItemSpacer(layout);
  uiLayoutSetAlignment(layout, UI_LAYOUT_ALIGN_RIGHT);
  uiItemL(layout, stats_str.c_str(), ICON_NONE);
  UI_block_layout_resolve(block, nullptr, nullptr);
  UI_block_align_end(block);
  UI_block_end(C, block);
  UI_block_draw(C, block);
}

static void spreadsheet_footer_region_free(ARegion *UNUSED(region))
{
}

static void spreadsheet_footer_region_listener(const wmRegionListenerParams *UNUSED(params))
{
}

void ED_spacetype_spreadsheet(void)
{
  SpaceType *st = (SpaceType *)MEM_callocN(sizeof(SpaceType), "spacetype spreadsheet");
  ARegionType *art;

  st->spaceid = SPACE_SPREADSHEET;
  strncpy(st->name, "Spreadsheet", BKE_ST_MAXNAME);

  st->create = spreadsheet_create;
  st->free = spreadsheet_free;
  st->init = spreadsheet_init;
  st->duplicate = spreadsheet_duplicate;
  st->operatortypes = spreadsheet_operatortypes;
  st->keymap = spreadsheet_keymap;

  /* regions: main window */
  art = (ARegionType *)MEM_callocN(sizeof(ARegionType), "spacetype spreadsheet region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  art->init = spreadsheet_main_region_init;
  art->draw = spreadsheet_main_region_draw;
  art->listener = spreadsheet_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = (ARegionType *)MEM_callocN(sizeof(ARegionType), "spacetype spreadsheet header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = spreadsheet_header_region_init;
  art->draw = spreadsheet_header_region_draw;
  art->free = spreadsheet_header_region_free;
  art->listener = spreadsheet_header_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: footer */
  art = (ARegionType *)MEM_callocN(sizeof(ARegionType), "spacetype spreadsheet footer region");
  art->regionid = RGN_TYPE_FOOTER;
  art->prefsizey = HEADERY;
  art->keymapflag = 0;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = spreadsheet_footer_region_init;
  art->draw = spreadsheet_footer_region_draw;
  art->free = spreadsheet_footer_region_free;
  art->listener = spreadsheet_footer_region_listener;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}

/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.hh"
#include "BLI_string.hh"

#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_view2d.hh"

#include "BLO_read_write.hh"

namespace blender {

static SpaceLink *project_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  SpaceProject *project_space = MEM_new<SpaceProject>("project space");
  project_space->spacetype = SPACE_PROJECT;

  {
    /* Header. */
    ARegion *region = BKE_area_region_new();

    BLI_addtail(&project_space->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    /* Always on bottom for new windows. */
    region->alignment = RGN_ALIGN_BOTTOM;
  }

  {
    /* Navigation region. */
    ARegion *region = BKE_area_region_new();
    BLI_addtail(&project_space->regionbase, region);
    region->regiontype = RGN_TYPE_UI;
    region->alignment = RGN_ALIGN_LEFT;
    region->flag &= ~RGN_FLAG_HIDDEN;
  }

  {
    /* Execution region. */
    ARegion *region = BKE_area_region_new();

    BLI_addtail(&project_space->regionbase, region);
    region->regiontype = RGN_TYPE_EXECUTE;
    region->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
    region->flag |= RGN_FLAG_DYNAMIC_SIZE | RGN_FLAG_NO_USER_RESIZE;
  }

  {
    /* Main region. */
    ARegion *region = BKE_area_region_new();
    BLI_addtail(&project_space->regionbase, region);
    region->regiontype = RGN_TYPE_WINDOW;
  }

  return (SpaceLink *)project_space;
}

static void project_free(SpaceLink * /*sl*/) {}

static void project_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *project_duplicate(SpaceLink *sl)
{
  SpaceProject *space_project = MEM_dupalloc(reinterpret_cast<SpaceProject *>(sl));

  return (SpaceLink *)space_project;
}

static void project_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  writer->write_struct_cast<SpaceProject>(sl);
}

static void project_operatortypes() {}

static void project_keymap(wmKeyConfig * /*keyconf*/) {}

/* -------------------------------------------------------------------- */
/** \name Project Main Region
 * \{ */

static void project_main_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  ED_region_panels_init(wm, region);
}

static void project_main_region_layout(const bContext *C, ARegion *region)
{
  ED_region_panels_layout(C, region);
}

static void project_main_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void project_main_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Project Execute Region
 * \{ */

static bool project_execute_region_poll(const RegionPollParams *params)
{
  const ARegion *region_header = BKE_area_find_region_type(params->area, RGN_TYPE_HEADER);
  return !region_header->runtime->visible;
}

static void project_execute_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;
}

static void project_execute_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Project Header Region
 * \{ */

static void project_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void project_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void project_header_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Project Navigation Region
 * \{ */

static void project_navigation_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  ED_region_panels_init(wm, region);
}

static void project_navigation_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void project_navigation_region_listener(const wmRegionListenerParams * /*params*/) {}

/** \} */

void ED_spacetype_project()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_PROJECT;
  STRNCPY(st->name, "Project Setup");

  st->create = project_create;
  st->free = project_free;
  st->init = project_init;
  st->duplicate = project_duplicate;
  st->operatortypes = project_operatortypes;
  st->keymap = project_keymap;
  st->blend_write = project_space_blend_write;

  /* regions: main window */
  art = MEM_new_zeroed<ARegionType>("spacetype project region");
  art->regionid = RGN_TYPE_WINDOW;
  art->flag = ARegionTypeFlag::UsePanelCategories;
  art->init = project_main_region_init;
  art->layout = project_main_region_layout;
  art->draw = project_main_region_draw;
  art->listener = project_main_region_listener;
  art->keymapflag = ED_KEYMAP_UI;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_new_zeroed<ARegionType>("spacetype project region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->init = project_header_region_init;
  art->draw = project_header_region_draw;
  art->listener = project_header_region_listener;

  BLI_addhead(&st->regiontypes, art);

  /* regions: navigation window */
  art = MEM_new_zeroed<ARegionType>("spacetype project region");
  art->regionid = RGN_TYPE_UI;
  art->flag = ARegionTypeFlag::HideSinglePanelCategories;
  art->prefsizex = UI_NAVIGATION_REGION_WIDTH;
  art->init = project_navigation_region_init;
  art->draw = project_navigation_region_draw;
  art->listener = project_navigation_region_listener;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_NAVBAR;

  BLI_addhead(&st->regiontypes, art);

  /* regions: execution window */
  art = MEM_new_zeroed<ARegionType>("spacetype project region");
  art->regionid = RGN_TYPE_EXECUTE;
  art->prefsizey = HEADERY;
  art->poll = project_execute_region_poll;
  art->init = project_execute_region_init;
  art->layout = ED_region_panels_layout;
  art->draw = ED_region_panels_draw;
  art->listener = project_execute_region_listener;
  art->keymapflag = ED_KEYMAP_UI;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

}  // namespace blender

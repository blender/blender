/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_screen.h"

#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BLO_read_write.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"

using namespace blender;

static SpaceLink *project_settings_create(const ScrArea *area, const Scene * /*scene*/)
{
  SpaceProjectSettings *project_settings_space = MEM_cnew<SpaceProjectSettings>(
      "project settings space");
  project_settings_space->spacetype = SPACE_PROJECT_SETTINGS;

  {
    /* Header. */
    ARegion *region = MEM_cnew<ARegion>("project settings header");
    BLI_addtail(&project_settings_space->regionbase, region);
    region->regiontype = RGN_TYPE_HEADER;
    /* Ignore preference "USER_HEADER_BOTTOM" here (always show bottom for new types). */
    region->alignment = RGN_ALIGN_BOTTOM;
  }

  {
    /* navigation region */
    ARegion *region = MEM_cnew<ARegion>("project settings navigation region");
    BLI_addtail(&project_settings_space->regionbase, region);
    region->regiontype = RGN_TYPE_NAV_BAR;
    region->alignment = RGN_ALIGN_LEFT;

    /* Use smaller size when opened in area like properties editor (same as preferences do). */
    if (area->winx && area->winx < 3.0f * UI_NAVIGATION_REGION_WIDTH * UI_SCALE_FAC) {
      region->sizex = UI_NARROW_NAVIGATION_REGION_WIDTH;
    }
  }
  {
    /* execution region */
    ARegion *region = MEM_cnew<ARegion>("project settings execution region");
    BLI_addtail(&project_settings_space->regionbase, region);
    region->regiontype = RGN_TYPE_EXECUTE;
    region->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
    region->flag |= RGN_FLAG_DYNAMIC_SIZE | RGN_FLAG_HIDDEN;
  }

  {
    /* Main window. */
    ARegion *region = MEM_cnew<ARegion>("project settings main region");
    BLI_addtail(&project_settings_space->regionbase, region);
    region->regiontype = RGN_TYPE_WINDOW;
  }

  return reinterpret_cast<SpaceLink *>(project_settings_space);
}

static void project_settings_free(SpaceLink * /*sl*/) {}

static void project_settings_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *project_settings_duplicate(SpaceLink *sl)
{
  const SpaceProjectSettings *sproject_settings_old = reinterpret_cast<SpaceProjectSettings *>(sl);
  SpaceProjectSettings *sproject_settings_new = reinterpret_cast<SpaceProjectSettings *>(
      MEM_dupallocN(sproject_settings_old));

  return reinterpret_cast<SpaceLink *>(sproject_settings_new);
}

static void project_settings_listener(const wmSpaceTypeListenerParams *params)
{
  const wmNotifier *wmn = params->notifier;
  ScrArea *area = params->area;

  switch (wmn->category) {
    case NC_PROJECT:
      ED_area_tag_redraw(area);
      break;
  }
}

static void project_settings_operatortypes(void) {}

static void project_settings_keymap(struct wmKeyConfig * /*keyconf*/) {}

static void project_settings_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceProjectSettings, sl);
}

/* add handlers, stuff you only do once or on area/region changes */
static void project_settings_main_region_init(wmWindowManager *wm, ARegion *region)
{
  /* do not use here, the properties changed in user-preferences do a system-wide refresh,
   * then scroller jumps back */
  // region->v2d.flag &= ~V2D_IS_INIT;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  ED_region_panels_init(wm, region);
}

static void project_settings_main_region_layout(const bContext *C, ARegion *region)
{
  SpaceProjectSettings *sproject_settings = CTX_wm_space_project_settings(C);

  char id_lower[64];
  const char *contexts[2] = {id_lower, NULL};

  if (!CTX_wm_project()) {
    /* Special context for when there is no project. UI can draw a special panel then. */
    STRNCPY(id_lower, "no_project");
  }
  else {
    /* Avoid duplicating identifiers, use existing RNA enum. */
    const EnumPropertyItem *items = rna_enum_project_settings_section_items;
    int i = RNA_enum_from_value(items, sproject_settings->active_section);
    /* Enum value not found: File is from the future. */
    if (i == -1) {
      i = 0;
    }
    StringRefNull id = items[i].identifier;
    BLI_assert(id.size() < (int64_t)sizeof(id_lower));
    STRNCPY(id_lower, id.c_str());
    BLI_str_tolower_ascii(id_lower, strlen(id_lower));
  }

  ED_region_panels_layout_ex(C, region, &region->type->paneltypes, contexts, NULL);
}

static void project_settings_main_region_listener(const wmRegionListenerParams * /*params*/) {}

static void project_settings_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void project_settings_header_region_listener(const wmRegionListenerParams * /*params*/) {}

/* add handlers, stuff you only do once or on area/region changes */
static void project_settings_navigation_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  ED_region_panels_init(wm, region);
}

static void project_settings_navigation_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static void project_settings_navigation_region_listener(const wmRegionListenerParams * /*params*/)
{
}

/* add handlers, stuff you only do once or on area/region changes */
static void project_settings_execute_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;
}

static void project_settings_execute_region_listener(const wmRegionListenerParams * /*params*/) {}

void ED_spacetype_project_settings()
{
  SpaceType *st = MEM_cnew<SpaceType>("spacetype project settings");

  st->spaceid = SPACE_PROJECT_SETTINGS;
  STRNCPY(st->name, "Project Settings");

  st->create = project_settings_create;
  st->free = project_settings_free;
  st->init = project_settings_init;
  st->duplicate = project_settings_duplicate;
  st->listener = project_settings_listener;
  st->operatortypes = project_settings_operatortypes;
  st->keymap = project_settings_keymap;
  st->blend_write = project_settings_blend_write;

  ARegionType *art;

  /* regions: main window */
  art = MEM_cnew<ARegionType>("spacetype project settings region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI;

  art->init = project_settings_main_region_init;
  art->layout = project_settings_main_region_layout;
  art->draw = ED_region_panels_draw;
  art->listener = project_settings_main_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_cnew<ARegionType>("spacetype project settings header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->listener = project_settings_header_region_listener;
  art->init = project_settings_header_region_init;
  art->draw = ED_region_header;
  BLI_addhead(&st->regiontypes, art);

  /* regions: navigation window */
  art = MEM_cnew<ARegionType>("spacetype project settings navigation region");
  art->regionid = RGN_TYPE_NAV_BAR;
  art->prefsizex = UI_NAVIGATION_REGION_WIDTH;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_NAVBAR;

  art->init = project_settings_navigation_region_init;
  art->draw = project_settings_navigation_region_draw;
  art->listener = project_settings_navigation_region_listener;
  BLI_addhead(&st->regiontypes, art);

  /* regions: execution window */
  art = MEM_cnew<ARegionType>("spacetype project settings execute region");
  art->regionid = RGN_TYPE_EXECUTE;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI;

  art->init = project_settings_execute_region_init;
  art->layout = ED_region_panels_layout;
  art->draw = ED_region_panels_draw;
  art->listener = project_settings_execute_region_listener;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}

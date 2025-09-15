/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_types.hh"

#include "UI_interface.hh"

#include "BLO_read_write.hh"

/* ******************** default callbacks for userpref space ***************** */

static SpaceLink *userpref_create(const ScrArea *area, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceUserPref *spref;

  spref = MEM_callocN<SpaceUserPref>("inituserpref");
  spref->spacetype = SPACE_USERPREF;

  /* header */
  region = BKE_area_region_new();

  BLI_addtail(&spref->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  /* Ignore user preference "USER_HEADER_BOTTOM" here (always show bottom for new types). */
  region->alignment = RGN_ALIGN_BOTTOM;

  /* navigation region */
  region = BKE_area_region_new();

  BLI_addtail(&spref->regionbase, region);
  region->regiontype = RGN_TYPE_UI;
  region->alignment = RGN_ALIGN_LEFT;
  region->flag &= ~RGN_FLAG_HIDDEN;

  /* Use smaller size when opened in area like properties editor. */
  if (area->winx && area->winx < 3.0f * UI_NAVIGATION_REGION_WIDTH * UI_SCALE_FAC) {
    region->sizex = UI_NARROW_NAVIGATION_REGION_WIDTH;
  }

  /* execution region */
  region = BKE_area_region_new();

  BLI_addtail(&spref->regionbase, region);
  region->regiontype = RGN_TYPE_EXECUTE;
  region->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
  region->flag |= RGN_FLAG_DYNAMIC_SIZE | RGN_FLAG_NO_USER_RESIZE;

  /* main region */
  region = BKE_area_region_new();

  BLI_addtail(&spref->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)spref;
}

/* Doesn't free the space-link itself. */
static void userpref_free(SpaceLink * /*sl*/)
{
  //  SpaceUserPref *spref = (SpaceUserPref *)sl;
}

/* spacetype; init callback */
static void userpref_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *userpref_duplicate(SpaceLink *sl)
{
  SpaceUserPref *sprefn = static_cast<SpaceUserPref *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */

  return (SpaceLink *)sprefn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void userpref_main_region_init(wmWindowManager *wm, ARegion *region)
{
  /* do not use here, the properties changed in user-preferences do a system-wide refresh,
   * then scroller jumps back */
  // region->v2d.flag &= ~V2D_IS_INIT;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  ED_region_panels_init(wm, region);
}

static void userpref_main_region_layout(const bContext *C, ARegion *region)
{
  char id_lower[64];
  const char *contexts[2] = {id_lower, nullptr};

  region->flag |= RGN_FLAG_INDICATE_OVERFLOW;

  /* Avoid duplicating identifiers, use existing RNA enum. */
  {
    const EnumPropertyItem *items = rna_enum_preference_section_items;
    int i = RNA_enum_from_value(items, U.space_data.section_active);
    /* File is from the future. */
    if (i == -1) {
      i = 0;
    }
    const char *id = items[i].identifier;
    BLI_assert(strlen(id) < sizeof(id_lower));
    STRNCPY_UTF8(id_lower, id);
    BLI_str_tolower_ascii(id_lower, strlen(id_lower));
  }

  ED_region_panels_layout_ex(C,
                             region,
                             &region->runtime->type->paneltypes,
                             blender::wm::OpCallContext::InvokeRegionWin,
                             contexts,
                             nullptr);
}

static void userpref_operatortypes() {}

static void userpref_keymap(wmKeyConfig * /*keyconf*/) {}

/* add handlers, stuff you only do once or on area/region changes */
static void userpref_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void userpref_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

/* add handlers, stuff you only do once or on area/region changes */
static void userpref_navigation_region_init(wmWindowManager *wm, ARegion *region)
{
  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;
  region->flag |= RGN_FLAG_INDICATE_OVERFLOW;

  ED_region_panels_init(wm, region);
}

static void userpref_navigation_region_draw(const bContext *C, ARegion *region)
{
  ED_region_panels(C, region);
}

static bool userpref_execute_region_poll(const RegionPollParams *params)
{
  const ARegion *region_header = BKE_area_find_region_type(params->area, RGN_TYPE_HEADER);
  return !region_header->runtime->visible;
}

/* add handlers, stuff you only do once or on area/region changes */
static void userpref_execute_region_init(wmWindowManager *wm, ARegion *region)
{
  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;
}

static void userpref_main_region_listener(const wmRegionListenerParams * /*params*/) {}

static void userpref_header_listener(const wmRegionListenerParams * /*params*/) {}

static void userpref_navigation_region_listener(const wmRegionListenerParams * /*params*/) {}

static void userpref_execute_region_listener(const wmRegionListenerParams * /*params*/) {}

static void userpref_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceUserPref, sl);
}

void ED_spacetype_userpref()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_USERPREF;
  STRNCPY_UTF8(st->name, "Userpref");

  st->create = userpref_create;
  st->free = userpref_free;
  st->init = userpref_init;
  st->duplicate = userpref_duplicate;
  st->operatortypes = userpref_operatortypes;
  st->keymap = userpref_keymap;
  st->blend_write = userpref_space_blend_write;

  /* regions: main window */
  art = MEM_callocN<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = userpref_main_region_init;
  art->layout = userpref_main_region_layout;
  art->draw = ED_region_panels_draw;
  art->listener = userpref_main_region_listener;
  art->keymapflag = ED_KEYMAP_UI;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->listener = userpref_header_listener;
  art->init = userpref_header_region_init;
  art->draw = userpref_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: navigation window */
  art = MEM_callocN<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_NAVIGATION_REGION_WIDTH;
  art->init = userpref_navigation_region_init;
  art->draw = userpref_navigation_region_draw;
  art->listener = userpref_navigation_region_listener;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_NAVBAR;

  BLI_addhead(&st->regiontypes, art);

  /* regions: execution window */
  art = MEM_callocN<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_EXECUTE;
  art->prefsizey = HEADERY;
  art->poll = userpref_execute_region_poll;
  art->init = userpref_execute_region_init;
  art->layout = ED_region_panels_layout;
  art->draw = ED_region_panels_draw;
  art->listener = userpref_execute_region_listener;
  art->keymapflag = ED_KEYMAP_UI;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

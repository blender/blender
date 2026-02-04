/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spuserpref
 */

#include <cstring>

#include "DNA_space_types.h"
#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_userpref.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_interface.hh"

#include "BLO_read_write.hh"

#include "userpref_intern.hh"

namespace blender {

/* ******************** default callbacks for userpref space ***************** */

static SpaceLink *userpref_create(const ScrArea *area, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceUserPref *spref;

  spref = MEM_new<SpaceUserPref>("inituserpref");
  spref->runtime = MEM_new<SpaceUserPref_Runtime>(__func__);
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

  return reinterpret_cast<SpaceLink *>(spref);
}

/* Doesn't free the space-link itself. */
static void userpref_free(SpaceLink *sl)
{
  SpaceUserPref *spref = (SpaceUserPref *)sl;
  MEM_delete(spref->runtime);
}

/* spacetype; init callback */
static void userpref_init(wmWindowManager * /*wm*/, ScrArea * /*area*/) {}

static SpaceLink *userpref_duplicate(SpaceLink *sl)
{
  SpaceUserPref *sprefn_old = (SpaceUserPref *)sl;
  SpaceUserPref *sprefn = static_cast<SpaceUserPref *>(MEM_dupalloc(sprefn_old));

  sprefn->runtime = MEM_new<SpaceUserPref_Runtime>(__func__);

  return reinterpret_cast<SpaceLink *>(sprefn);
}

/* add handlers, stuff you only do once or on area/region changes */
static void userpref_main_region_init(wmWindowManager *wm, ARegion *region)
{
  /* do not use here, the properties changed in user-preferences do a system-wide refresh,
   * then scroller jumps back */
  // region->v2d.flag &= ~V2D_IS_INIT;

  region->v2d.scroll = V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HIDE;

  wmKeyMap *keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Preferences", SPACE_USERPREF, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);

  ED_region_panels_init(wm, region);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Userpref Search Access API
 * \{ */

const char *ED_userpref_search_string_get(SpaceUserPref *spref)
{
  return spref->runtime->search_string.c_str();
}

int ED_userpref_search_string_length(SpaceUserPref *spref)
{
  return spref->runtime->search_string.size();
}

void ED_userpref_search_string_set(SpaceUserPref *spref, const char *value)
{
  spref->runtime->search_string = value ? value : "";
}

bool ED_userpref_tab_has_search_result(SpaceUserPref *spref, const int index)
{
  return spref->runtime->tab_search_results[index];
}

/** \} */

Vector<int> ED_userpref_tabs_list(SpaceUserPref * /*prefs*/)
{
  Vector<int> result;
  for (const EnumPropertyItem *it = rna_enum_preference_section_items; it->identifier != nullptr;
       it++)
  {
    if (it->name) {
      result.append(eUserPref_Section(it->value));
    }
    else {
      result.append(-1);
    }
  }
  return result;
}

/* -------------------------------------------------------------------- */
/** \name "Off Screen" Layout Generation for Userpref Search
 * \{ */

static bool property_search_for_context(const bContext *C, ARegion *region, short section)
{
  char lower[64] = {0};
  const char *name = nullptr;
  RNA_enum_id_from_value(rna_enum_preference_section_items, section, &name);
  STRNCPY(lower, name);
  BLI_str_tolower_ascii(lower, sizeof(lower));
  const char *contexts[2] = {lower, nullptr};
  return ED_region_property_search(
      C, region, &region->runtime->type->paneltypes, contexts, nullptr);
}

static void userpref_search_move_to_next_tab_with_results(SpaceUserPref *sbuts,
                                                          const Span<int> context_tabs_array)
{
  int current_tab_index = 0;
  for (const int i : context_tabs_array.index_range()) {
    if (U.space_data.section_active == context_tabs_array[i]) {
      current_tab_index = i;
      break;
    }
  }
  /* Try the tabs after the current tab. */
  for (int i = current_tab_index + 1; i < context_tabs_array.size(); i++) {
    if (sbuts->runtime->tab_search_results[i]) {
      U.space_data.section_active = context_tabs_array[i];
      return;
    }
  }
  /* Try the tabs before the current tab. */
  for (int i = 0; i < current_tab_index; i++) {
    if (sbuts->runtime->tab_search_results[i]) {
      U.space_data.section_active = context_tabs_array[i];
      return;
    }
  }
}

static void userpref_search_all_tabs(const bContext *C,
                                     SpaceUserPref *sprefs,
                                     ARegion *region_original,
                                     const Span<int> context_tabs_array)
{
  /* Use local copies of the area and duplicate the region as a mainly-paranoid protection
   * against changing any of the space / region data while running the search. */
  ScrArea *area_original = CTX_wm_area(C);
  ScrArea area_copy = dna::shallow_copy(*area_original);
  ARegion *region_copy = BKE_area_region_copy(area_copy.type, region_original);
  /* Set the region visible field. Otherwise some layout code thinks we're drawing in a popup.
   * This likely isn't necessary, but it's nice to emulate a "real" region where possible. */
  region_copy->runtime->visible = true;
  CTX_wm_area_set(const_cast<bContext *>(C), &area_copy);
  CTX_wm_region_set(const_cast<bContext *>(C), region_copy);
  SpaceUserPref sprefs_copy = dna::shallow_copy(*sprefs);
  sprefs_copy.runtime = MEM_new<SpaceUserPref_Runtime>(__func__, *sprefs->runtime);
  sprefs_copy.runtime->tab_search_results.fill(false);
  BLI_listbase_clear(&area_copy.spacedata);
  BLI_addtail(&area_copy.spacedata, &sprefs_copy);
  /* Loop through the tabs. */
  for (const int i : context_tabs_array.index_range()) {
    /* -1 corresponds to a spacer. */
    if (context_tabs_array[i] == -1) {
      continue;
    }
    if (ELEM(context_tabs_array[i], USER_SECTION_EXTENSIONS, USER_SECTION_ADDONS)) {
      continue;
    }
    /* Handle search for the current tab in the normal layout pass. */
    if (context_tabs_array[i] == U.space_data.section_active) {
      continue;
    }
    /* Actually do the search and store the result in the bitmap. */
    const bool found = property_search_for_context(C, region_copy, context_tabs_array[i]);
    sprefs->runtime->tab_search_results[i] = found;
    ui::blocklist_free(C, region_copy);
  }
  BKE_area_region_free(area_copy.type, region_copy);
  MEM_delete(region_copy);
  userpref_free(reinterpret_cast<SpaceLink *>(&sprefs_copy));
  CTX_wm_area_set(const_cast<bContext *>(C), area_original);
  CTX_wm_region_set(const_cast<bContext *>(C), region_original);
}

/**
 * Handle userpref search for the layout pass, including finding which tabs have
 * search results and switching if the current tab doesn't have a result.
 */
static void userpref_main_region_property_search(const bContext *C,
                                                 SpaceUserPref *sprefs,
                                                 ARegion *region)
{
  Vector<int> tabs = ED_userpref_tabs_list(sprefs);
  userpref_search_all_tabs(C, sprefs, region, tabs);
  /* Check whether the current tab has a search match. */
  bool current_tab_has_search_match = false;
  for (Panel &panel : region->panels) {
    if (ui::panel_is_active(&panel) && ui::panel_matches_search_filter(&panel)) {
      current_tab_has_search_match = true;
    }
  }
  /* Find which index in the list the current tab corresponds to. */
  int current_tab_index = -1;
  for (const int i : tabs.index_range()) {
    if (tabs[i] == U.space_data.section_active) {
      current_tab_index = i;
    }
  }
  BLI_assert(current_tab_index != -1);
  /* Update the tab search match flag for the current tab. */
  sprefs->runtime->tab_search_results[current_tab_index] = current_tab_has_search_match;
  /* Move to the next tab with a result */
  if (!current_tab_has_search_match) {
    if (region->flag & RGN_FLAG_SEARCH_FILTER_UPDATE) {
      userpref_search_move_to_next_tab_with_results(sprefs, tabs);
    }
  }
}

/** \} */

static void userpref_main_region_layout(const bContext *C, ARegion *region)
{
  char id_lower[64];
  const char *contexts[2] = {id_lower, nullptr};
  SpaceUserPref *spref = CTX_wm_space_userpref(C);

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
                             wm::OpCallContext::InvokeRegionWin,
                             contexts,
                             nullptr);

  if (region->flag & RGN_FLAG_SEARCH_FILTER_ACTIVE) {
    userpref_main_region_property_search(C, spref, region);
  }
}

static void userpref_operatortypes() {}

static void userpref_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Preferences", SPACE_USERPREF, RGN_TYPE_WINDOW);
}

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

  wmKeyMap *keymap = WM_keymap_ensure(
      wm->runtime->defaultconf, "Preferences_nav", SPACE_USERPREF, RGN_TYPE_UI);
  WM_event_add_keymap_handler(&region->runtime->handlers, keymap);

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

static void userpref_blend_read_data(BlendDataReader * /*reader*/, SpaceLink *sl)
{
  SpaceUserPref *spref = reinterpret_cast<SpaceUserPref *>(sl);
  spref->runtime = MEM_new<SpaceUserPref_Runtime>(__func__);
}

static void userpref_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  writer->write_struct_cast<SpaceUserPref>(sl);
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
  st->blend_read_data = userpref_blend_read_data;
  st->blend_write = userpref_space_blend_write;

  /* regions: main window */
  art = MEM_new_zeroed<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = userpref_main_region_init;
  art->layout = userpref_main_region_layout;
  art->draw = ED_region_panels_draw;
  art->listener = userpref_main_region_listener;
  art->keymapflag = ED_KEYMAP_UI;

  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_new_zeroed<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
  art->listener = userpref_header_listener;
  art->init = userpref_header_region_init;
  art->draw = userpref_header_region_draw;

  BLI_addhead(&st->regiontypes, art);

  /* regions: navigation window */
  art = MEM_new_zeroed<ARegionType>("spacetype userpref region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_NAVIGATION_REGION_WIDTH;
  art->init = userpref_navigation_region_init;
  art->draw = userpref_navigation_region_draw;
  art->listener = userpref_navigation_region_listener;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_NAVBAR;

  BLI_addhead(&st->regiontypes, art);

  /* regions: execution window */
  art = MEM_new_zeroed<ARegionType>("spacetype userpref region");
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

}  // namespace blender

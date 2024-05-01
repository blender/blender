/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_gpencil_modifier_legacy.h" /* Types for registering panels. */
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"
#include "BKE_shader_fx.h"

#include "ED_buttons.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh" /* To draw toolbar UI. */

#include "WM_api.hh"
#include "WM_message.hh"
#include "WM_types.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLO_read_write.hh"

#include "buttons_intern.hh" /* own include */

/* -------------------------------------------------------------------- */
/** \name Default Callbacks for Properties Space
 * \{ */

static SpaceLink *buttons_create(const ScrArea * /*area*/, const Scene * /*scene*/)
{
  ARegion *region;
  SpaceProperties *sbuts;

  sbuts = static_cast<SpaceProperties *>(MEM_callocN(sizeof(SpaceProperties), "initbuts"));
  sbuts->spacetype = SPACE_PROPERTIES;

  sbuts->mainb = sbuts->mainbuser = BCONTEXT_OBJECT;

  /* header */
  region = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), "header for buts"));

  BLI_addtail(&sbuts->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* navigation bar */
  region = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), "navigation bar for buts"));

  BLI_addtail(&sbuts->regionbase, region);
  region->regiontype = RGN_TYPE_NAV_BAR;
  region->alignment = RGN_ALIGN_LEFT;

#if 0
  /* context region */
  region = MEM_callocN(sizeof(ARegion), "context region for buts");
  BLI_addtail(&sbuts->regionbase, region);
  region->regiontype = RGN_TYPE_CHANNELS;
  region->alignment = RGN_ALIGN_TOP;
#endif

  /* main region */
  region = static_cast<ARegion *>(MEM_callocN(sizeof(ARegion), "main region for buts"));

  BLI_addtail(&sbuts->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)sbuts;
}

/* Doesn't free the space-link itself. */
static void buttons_free(SpaceLink *sl)
{
  SpaceProperties *sbuts = (SpaceProperties *)sl;

  if (sbuts->path) {
    MEM_freeN(sbuts->path);
  }

  if (sbuts->texuser) {
    ButsContextTexture *ct = static_cast<ButsContextTexture *>(sbuts->texuser);
    BLI_freelistN(&ct->users);
    MEM_freeN(ct);
  }

  if (sbuts->runtime != nullptr) {
    MEM_SAFE_FREE(sbuts->runtime->tab_search_results);
    MEM_freeN(sbuts->runtime);
  }
}

/* spacetype; init callback */
static void buttons_init(wmWindowManager * /*wm*/, ScrArea *area)
{
  SpaceProperties *sbuts = (SpaceProperties *)area->spacedata.first;

  if (sbuts->runtime == nullptr) {
    sbuts->runtime = static_cast<SpaceProperties_Runtime *>(
        MEM_mallocN(sizeof(SpaceProperties_Runtime), __func__));
    sbuts->runtime->search_string[0] = '\0';
    sbuts->runtime->tab_search_results = BLI_BITMAP_NEW(BCONTEXT_TOT * 2, __func__);
  }
}

static SpaceLink *buttons_duplicate(SpaceLink *sl)
{
  SpaceProperties *sfile_old = (SpaceProperties *)sl;
  SpaceProperties *sbutsn = static_cast<SpaceProperties *>(MEM_dupallocN(sl));

  /* clear or remove stuff from old */
  sbutsn->path = nullptr;
  sbutsn->texuser = nullptr;
  if (sfile_old->runtime != nullptr) {
    sbutsn->runtime = static_cast<SpaceProperties_Runtime *>(MEM_dupallocN(sfile_old->runtime));
    sbutsn->runtime->search_string[0] = '\0';
    sbutsn->runtime->tab_search_results = BLI_BITMAP_NEW(BCONTEXT_TOT, __func__);
  }

  return (SpaceLink *)sbutsn;
}

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_main_region_init(wmWindowManager *wm, ARegion *region)
{
  wmKeyMap *keymap;

  ED_region_panels_init(wm, region);

  keymap = WM_keymap_ensure(wm->defaultconf, "Property Editor", SPACE_PROPERTIES, RGN_TYPE_WINDOW);
  WM_event_add_keymap_handler(&region->handlers, keymap);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Property Editor Layout
 * \{ */

int ED_buttons_tabs_list(SpaceProperties *sbuts, short *context_tabs_array)
{
  int length = 0;
  if (sbuts->pathflag & (1 << BCONTEXT_TOOL)) {
    context_tabs_array[length] = BCONTEXT_TOOL;
    length++;
  }
  if (length != 0) {
    context_tabs_array[length] = -1;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_RENDER)) {
    context_tabs_array[length] = BCONTEXT_RENDER;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_OUTPUT)) {
    context_tabs_array[length] = BCONTEXT_OUTPUT;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_VIEW_LAYER)) {
    context_tabs_array[length] = BCONTEXT_VIEW_LAYER;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_SCENE)) {
    context_tabs_array[length] = BCONTEXT_SCENE;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_WORLD)) {
    context_tabs_array[length] = BCONTEXT_WORLD;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_COLLECTION)) {
    if (length != 0) {
      context_tabs_array[length] = -1;
      length++;
    }
    context_tabs_array[length] = BCONTEXT_COLLECTION;
    length++;
  }
  if (length != 0) {
    context_tabs_array[length] = -1;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_OBJECT)) {
    context_tabs_array[length] = BCONTEXT_OBJECT;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_MODIFIER)) {
    context_tabs_array[length] = BCONTEXT_MODIFIER;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_SHADERFX)) {
    context_tabs_array[length] = BCONTEXT_SHADERFX;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_PARTICLE)) {
    context_tabs_array[length] = BCONTEXT_PARTICLE;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_PHYSICS)) {
    context_tabs_array[length] = BCONTEXT_PHYSICS;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_CONSTRAINT)) {
    context_tabs_array[length] = BCONTEXT_CONSTRAINT;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_DATA)) {
    context_tabs_array[length] = BCONTEXT_DATA;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_BONE)) {
    context_tabs_array[length] = BCONTEXT_BONE;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_BONE_CONSTRAINT)) {
    context_tabs_array[length] = BCONTEXT_BONE_CONSTRAINT;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_MATERIAL)) {
    context_tabs_array[length] = BCONTEXT_MATERIAL;
    length++;
  }
  if (length != 0) {
    context_tabs_array[length] = -1;
    length++;
  }
  if (sbuts->pathflag & (1 << BCONTEXT_TEXTURE)) {
    context_tabs_array[length] = BCONTEXT_TEXTURE;
    length++;
  }

  return length;
}

static const char *buttons_main_region_context_string(const short mainb)
{
  switch (mainb) {
    case BCONTEXT_SCENE:
      return "scene";
    case BCONTEXT_RENDER:
      return "render";
    case BCONTEXT_OUTPUT:
      return "output";
    case BCONTEXT_VIEW_LAYER:
      return "view_layer";
    case BCONTEXT_WORLD:
      return "world";
    case BCONTEXT_COLLECTION:
      return "collection";
    case BCONTEXT_OBJECT:
      return "object";
    case BCONTEXT_DATA:
      return "data";
    case BCONTEXT_MATERIAL:
      return "material";
    case BCONTEXT_TEXTURE:
      return "texture";
    case BCONTEXT_PARTICLE:
      return "particle";
    case BCONTEXT_PHYSICS:
      return "physics";
    case BCONTEXT_BONE:
      return "bone";
    case BCONTEXT_MODIFIER:
      return "modifier";
    case BCONTEXT_SHADERFX:
      return "shaderfx";
    case BCONTEXT_CONSTRAINT:
      return "constraint";
    case BCONTEXT_BONE_CONSTRAINT:
      return "bone_constraint";
    case BCONTEXT_TOOL:
      return "tool";
  }

  /* All the cases should be handled. */
  BLI_assert(false);
  return "";
}

static void buttons_main_region_layout_properties(const bContext *C,
                                                  SpaceProperties *sbuts,
                                                  ARegion *region)
{
  buttons_context_compute(C, sbuts);

  const char *contexts[2] = {buttons_main_region_context_string(sbuts->mainb), nullptr};

  ED_region_panels_layout_ex(
      C, region, &region->type->paneltypes, WM_OP_INVOKE_REGION_WIN, contexts, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Property Search Access API
 * \{ */

const char *ED_buttons_search_string_get(SpaceProperties *sbuts)
{
  return sbuts->runtime->search_string;
}

int ED_buttons_search_string_length(SpaceProperties *sbuts)
{
  return BLI_strnlen(sbuts->runtime->search_string, sizeof(sbuts->runtime->search_string));
}

void ED_buttons_search_string_set(SpaceProperties *sbuts, const char *value)
{
  STRNCPY(sbuts->runtime->search_string, value);
}

bool ED_buttons_tab_has_search_result(SpaceProperties *sbuts, const int index)
{
  return BLI_BITMAP_TEST(sbuts->runtime->tab_search_results, index);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name "Off Screen" Layout Generation for Property Search
 * \{ */

static bool property_search_for_context(const bContext *C, ARegion *region, SpaceProperties *sbuts)
{
  const char *contexts[2] = {buttons_main_region_context_string(sbuts->mainb), nullptr};

  if (sbuts->mainb == BCONTEXT_TOOL) {
    return false;
  }

  buttons_context_compute(C, sbuts);
  return ED_region_property_search(C, region, &region->type->paneltypes, contexts, nullptr);
}

static void property_search_move_to_next_tab_with_results(SpaceProperties *sbuts,
                                                          const short *context_tabs_array,
                                                          const int tabs_len)
{
  /* As long as all-tab search in the tool is disabled in the tool context, don't move from it. */
  if (sbuts->mainb == BCONTEXT_TOOL) {
    return;
  }

  int current_tab_index = 0;
  for (int i = 0; i < tabs_len; i++) {
    if (sbuts->mainb == context_tabs_array[i]) {
      current_tab_index = i;
      break;
    }
  }

  /* Try the tabs after the current tab. */
  for (int i = current_tab_index; i < tabs_len; i++) {
    if (BLI_BITMAP_TEST(sbuts->runtime->tab_search_results, i)) {
      sbuts->mainbuser = context_tabs_array[i];
      return;
    }
  }

  /* Try the tabs before the current tab. */
  for (int i = 0; i < current_tab_index; i++) {
    if (BLI_BITMAP_TEST(sbuts->runtime->tab_search_results, i)) {
      sbuts->mainbuser = context_tabs_array[i];
      return;
    }
  }
}

static void property_search_all_tabs(const bContext *C,
                                     SpaceProperties *sbuts,
                                     ARegion *region_original,
                                     const short *context_tabs_array,
                                     const int tabs_len)
{
  /* Use local copies of the area and duplicate the region as a mainly-paranoid protection
   * against changing any of the space / region data while running the search. */
  ScrArea *area_original = CTX_wm_area(C);
  ScrArea area_copy = blender::dna::shallow_copy(*area_original);
  ARegion *region_copy = BKE_area_region_copy(area_copy.type, region_original);
  /* Set the region visible field. Otherwise some layout code thinks we're drawing in a popup.
   * This likely isn't necessary, but it's nice to emulate a "real" region where possible. */
  region_copy->visible = true;
  CTX_wm_area_set((bContext *)C, &area_copy);
  CTX_wm_region_set((bContext *)C, region_copy);

  SpaceProperties sbuts_copy = blender::dna::shallow_copy(*sbuts);
  sbuts_copy.path = nullptr;
  sbuts_copy.texuser = nullptr;
  sbuts_copy.runtime = static_cast<SpaceProperties_Runtime *>(MEM_dupallocN(sbuts->runtime));
  sbuts_copy.runtime->tab_search_results = nullptr;
  BLI_listbase_clear(&area_copy.spacedata);
  BLI_addtail(&area_copy.spacedata, &sbuts_copy);

  /* Loop through the tabs added to the properties editor. */
  for (int i = 0; i < tabs_len; i++) {
    /* -1 corresponds to a spacer. */
    if (context_tabs_array[i] == -1) {
      continue;
    }

    /* Handle search for the current tab in the normal layout pass. */
    if (context_tabs_array[i] == sbuts->mainb) {
      continue;
    }

    sbuts_copy.mainb = sbuts_copy.mainbo = sbuts_copy.mainbuser = context_tabs_array[i];

    /* Actually do the search and store the result in the bitmap. */
    BLI_BITMAP_SET(sbuts->runtime->tab_search_results,
                   i,
                   property_search_for_context(C, region_copy, &sbuts_copy));

    UI_blocklist_free(C, region_copy);
  }

  BKE_area_region_free(area_copy.type, region_copy);
  MEM_freeN(region_copy);
  buttons_free((SpaceLink *)&sbuts_copy);

  CTX_wm_area_set((bContext *)C, area_original);
  CTX_wm_region_set((bContext *)C, region_original);
}

/**
 * Handle property search for the layout pass, including finding which tabs have
 * search results and switching if the current tab doesn't have a result.
 */
static void buttons_main_region_property_search(const bContext *C,
                                                SpaceProperties *sbuts,
                                                ARegion *region)
{
  /* Theoretical maximum of every context shown with a spacer between every tab. */
  short context_tabs_array[BCONTEXT_TOT * 2];
  int tabs_len = ED_buttons_tabs_list(sbuts, context_tabs_array);

  property_search_all_tabs(C, sbuts, region, context_tabs_array, tabs_len);

  /* Check whether the current tab has a search match. */
  bool current_tab_has_search_match = false;
  LISTBASE_FOREACH (Panel *, panel, &region->panels) {
    if (UI_panel_is_active(panel) && UI_panel_matches_search_filter(panel)) {
      current_tab_has_search_match = true;
    }
  }

  /* Find which index in the list the current tab corresponds to. */
  int current_tab_index = -1;
  for (int i = 0; i < tabs_len; i++) {
    if (context_tabs_array[i] == sbuts->mainb) {
      current_tab_index = i;
    }
  }
  BLI_assert(current_tab_index != -1);

  /* Update the tab search match flag for the current tab. */
  BLI_BITMAP_SET(
      sbuts->runtime->tab_search_results, current_tab_index, current_tab_has_search_match);

  /* Move to the next tab with a result */
  if (!current_tab_has_search_match) {
    if (region->flag & RGN_FLAG_SEARCH_FILTER_UPDATE) {
      property_search_move_to_next_tab_with_results(sbuts, context_tabs_array, tabs_len);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Main Region Layout and Listener
 * \{ */

static void buttons_main_region_layout(const bContext *C, ARegion *region)
{
  /* draw entirely, view changes should be handled here */
  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  if (sbuts->mainb == BCONTEXT_TOOL) {
    ED_view3d_buttons_region_layout_ex(C, region, "Tool");
  }
  else {
    buttons_main_region_layout_properties(C, sbuts, region);
  }

  if (region->flag & RGN_FLAG_SEARCH_FILTER_ACTIVE) {
    buttons_main_region_property_search(C, sbuts, region);
  }

  sbuts->mainbo = sbuts->mainb;
}

static void buttons_main_region_listener(const wmRegionListenerParams *params)
{
  ARegion *region = params->region;
  const wmNotifier *wmn = params->notifier;

  /* context changes */
  switch (wmn->category) {
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYER)) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void buttons_operatortypes()
{
  WM_operatortype_append(BUTTONS_OT_start_filter);
  WM_operatortype_append(BUTTONS_OT_clear_filter);
  WM_operatortype_append(BUTTONS_OT_toggle_pin);
  WM_operatortype_append(BUTTONS_OT_context_menu);
  WM_operatortype_append(BUTTONS_OT_file_browse);
  WM_operatortype_append(BUTTONS_OT_directory_browse);
}

static void buttons_keymap(wmKeyConfig *keyconf)
{
  WM_keymap_ensure(keyconf, "Property Editor", SPACE_PROPERTIES, RGN_TYPE_WINDOW);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Header Region Callbacks
 * \{ */

/* add handlers, stuff you only do once or on area/region changes */
static void buttons_header_region_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

static void buttons_header_region_draw(const bContext *C, ARegion *region)
{
  SpaceProperties *sbuts = CTX_wm_space_properties(C);

  /* Needed for RNA to get the good values! */
  buttons_context_compute(C, sbuts);

  ED_region_header(C, region);
}

static void buttons_header_region_message_subscribe(const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  ScrArea *area = params->area;
  ARegion *region = params->region;
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  /* Don't check for SpaceProperties.mainb here, we may toggle between view-layers
   * where one has no active object, so that available contexts changes. */
  WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_region_tag_redraw);

  if (!ELEM(sbuts->mainb, BCONTEXT_RENDER, BCONTEXT_OUTPUT, BCONTEXT_SCENE, BCONTEXT_WORLD)) {
    WM_msg_subscribe_rna_anon_prop(mbus, ViewLayer, name, &msg_sub_value_region_tag_redraw);
  }

  if (sbuts->mainb == BCONTEXT_TOOL) {
    WM_msg_subscribe_rna_anon_prop(mbus, WorkSpace, tools, &msg_sub_value_region_tag_redraw);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navigation Region Callbacks
 * \{ */

static void buttons_navigation_bar_region_init(wmWindowManager *wm, ARegion *region)
{
  region->flag |= RGN_FLAG_NO_USER_RESIZE;

  ED_region_panels_init(wm, region);
  region->v2d.keepzoom |= V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y;
}

static void buttons_navigation_bar_region_draw(const bContext *C, ARegion *region)
{
  LISTBASE_FOREACH (PanelType *, pt, &region->type->paneltypes) {
    pt->flag |= PANEL_TYPE_LAYOUT_VERT_BAR;
  }

  ED_region_panels_layout(C, region);
  /* #ED_region_panels_layout adds vertical scroll-bars, we don't want them. */
  region->v2d.scroll &= ~V2D_SCROLL_VERTICAL;
  ED_region_panels_draw(C, region);
}

static void buttons_navigation_bar_region_message_subscribe(
    const wmRegionMessageSubscribeParams *params)
{
  wmMsgBus *mbus = params->message_bus;
  ARegion *region = params->region;

  wmMsgSubscribeValue msg_sub_value_region_tag_redraw{};
  msg_sub_value_region_tag_redraw.owner = region;
  msg_sub_value_region_tag_redraw.user_data = region;
  msg_sub_value_region_tag_redraw.notify = ED_region_do_msg_notify_tag_redraw;

  WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_region_tag_redraw);
}

/* draw a certain button set only if properties area is currently
 * showing that button set, to reduce unnecessary drawing. */
static void buttons_area_redraw(ScrArea *area, short buttons)
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);

  /* if the area's current button set is equal to the one to redraw */
  if (sbuts->mainb == buttons) {
    ED_area_tag_redraw(area);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Area-Level Code
 * \{ */

/* reused! */
static void buttons_area_listener(const wmSpaceTypeListenerParams *params)
{
  ScrArea *area = params->area;
  const wmNotifier *wmn = params->notifier;
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);

  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_RENDER_OPTIONS:
          buttons_area_redraw(area, BCONTEXT_RENDER);
          buttons_area_redraw(area, BCONTEXT_OUTPUT);
          buttons_area_redraw(area, BCONTEXT_VIEW_LAYER);
          break;
        case ND_WORLD:
          buttons_area_redraw(area, BCONTEXT_WORLD);
          sbuts->preview = 1;
          break;
        case ND_FRAME:
          /* any buttons area can have animated properties so redraw all */
          ED_area_tag_redraw(area);
          sbuts->preview = 1;
          break;
        case ND_OB_ACTIVE:
          ED_area_tag_redraw(area);
          sbuts->preview = 1;
          break;
        case ND_KEYINGSET:
          buttons_area_redraw(area, BCONTEXT_SCENE);
          break;
        case ND_RENDER_RESULT:
          break;
        case ND_MODE:
        case ND_LAYER:
        default:
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_TRANSFORM:
          buttons_area_redraw(area, BCONTEXT_OBJECT);
          buttons_area_redraw(area, BCONTEXT_DATA); /* Auto-texture-space flag. */
          break;
        case ND_POSE:
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_BONE_COLLECTION:
          buttons_area_redraw(area, BCONTEXT_BONE);
          buttons_area_redraw(area, BCONTEXT_BONE_CONSTRAINT);
          buttons_area_redraw(area, BCONTEXT_DATA);
          break;
        case ND_MODIFIER:
          if (wmn->action == NA_RENAME) {
            ED_area_tag_redraw(area);
          }
          else {
            buttons_area_redraw(area, BCONTEXT_MODIFIER);
          }
          buttons_area_redraw(area, BCONTEXT_PHYSICS);
          break;
        case ND_CONSTRAINT:
          buttons_area_redraw(area, BCONTEXT_CONSTRAINT);
          buttons_area_redraw(area, BCONTEXT_BONE_CONSTRAINT);
          break;
        case ND_SHADERFX:
          buttons_area_redraw(area, BCONTEXT_SHADERFX);
          break;
        case ND_PARTICLE:
          if (wmn->action == NA_EDITED) {
            buttons_area_redraw(area, BCONTEXT_PARTICLE);
          }
          sbuts->preview = 1;
          break;
        case ND_DRAW:
          buttons_area_redraw(area, BCONTEXT_OBJECT);
          buttons_area_redraw(area, BCONTEXT_DATA);
          buttons_area_redraw(area, BCONTEXT_PHYSICS);
          /* Needed to refresh context path when changing active particle system index. */
          buttons_area_redraw(area, BCONTEXT_PARTICLE);
          buttons_area_redraw(area, BCONTEXT_TOOL);
          break;
        case ND_DRAW_ANIMVIZ:
          buttons_area_redraw(area, BCONTEXT_OBJECT);
          break;
        default:
          /* Not all object RNA props have a ND_ notifier (yet) */
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_SELECT:
        case ND_DATA:
        case ND_VERTEX_GROUP:
          ED_area_tag_redraw(area);
          break;
      }
      break;
    case NC_MATERIAL:
      ED_area_tag_redraw(area);
      switch (wmn->data) {
        case ND_SHADING:
        case ND_SHADING_DRAW:
        case ND_SHADING_LINKS:
        case ND_SHADING_PREVIEW:
        case ND_NODES:
          /* currently works by redraws... if preview is set, it (re)starts job */
          sbuts->preview = 1;
          break;
      }
      break;
    case NC_WORLD:
      buttons_area_redraw(area, BCONTEXT_WORLD);
      sbuts->preview = 1;
      break;
    case NC_LAMP:
      buttons_area_redraw(area, BCONTEXT_DATA);
      sbuts->preview = 1;
      break;
    case NC_GROUP:
      buttons_area_redraw(area, BCONTEXT_OBJECT);
      break;
    case NC_BRUSH:
      buttons_area_redraw(area, BCONTEXT_TEXTURE);
      buttons_area_redraw(area, BCONTEXT_TOOL);
      sbuts->preview = 1;
      break;
    case NC_TEXTURE:
    case NC_IMAGE:
      if (wmn->action != NA_PAINTING) {
        ED_area_tag_redraw(area);
        sbuts->preview = 1;
      }
      break;
    case NC_WORKSPACE:
      buttons_area_redraw(area, BCONTEXT_TOOL);
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_PROPERTIES) {
        ED_area_tag_redraw(area);
      }
      else if (wmn->data == ND_SPACE_CHANGED) {
        ED_area_tag_redraw(area);
        sbuts->preview = 1;
      }
      break;
    case NC_ID:
      if (ELEM(wmn->action, NA_RENAME, NA_EDITED)) {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_NLA_ACTCHANGE:
          ED_area_tag_redraw(area);
          break;
        case ND_KEYFRAME:
          if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED)) {
            ED_area_tag_redraw(area);
          }
          break;
      }
      break;
    case NC_GPENCIL:
      if (wmn->data == ND_DATA) {
        if (ELEM(wmn->action, NA_EDITED, NA_ADDED, NA_REMOVED, NA_SELECTED, NA_RENAME)) {
          ED_area_tag_redraw(area);
        }
      }
      else if (wmn->action == NA_EDITED) {
        ED_area_tag_redraw(area);
      }
      break;
    case NC_NODE:
      if (wmn->action == NA_SELECTED) {
        ED_area_tag_redraw(area);
        /* new active node, update texture preview */
        if (sbuts->mainb == BCONTEXT_TEXTURE) {
          sbuts->preview = 1;
        }
      }
      break;
    /* Listener for preview render, when doing an global undo. */
    case NC_WM:
      if (wmn->data == ND_UNDO) {
        ED_area_tag_redraw(area);
        sbuts->preview = 1;
      }
      break;
    case NC_SCREEN:
      if (wmn->data == ND_LAYOUTSET) {
        ED_area_tag_redraw(area);
        sbuts->preview = 1;
      }
      break;
#ifdef WITH_FREESTYLE
    case NC_LINESTYLE:
      ED_area_tag_redraw(area);
      sbuts->preview = 1;
      break;
#endif
  }

  if (wmn->data == ND_KEYS) {
    ED_area_tag_redraw(area);
  }
}

static void buttons_id_remap(ScrArea * /*area*/,
                             SpaceLink *slink,
                             const blender::bke::id::IDRemapper &mappings)
{
  SpaceProperties *sbuts = (SpaceProperties *)slink;

  if (mappings.apply(&sbuts->pinid, ID_REMAP_APPLY_DEFAULT) == ID_REMAP_RESULT_SOURCE_UNASSIGNED) {
    sbuts->flag &= ~SB_PIN_CONTEXT;
  }

  if (sbuts->path) {
    ButsContextPath *path = static_cast<ButsContextPath *>(sbuts->path);
    for (int i = 0; i < path->len; i++) {
      switch (mappings.apply(&path->ptr[i].owner_id, ID_REMAP_APPLY_DEFAULT)) {
        case ID_REMAP_RESULT_SOURCE_UNASSIGNED: {
          path->len = i;
          if (i != 0) {
            /* If the first item in the path is cleared, the whole path is cleared, so no need to
             * clear further items here, see also at the end of this block. */
            memset(&path->ptr[i], 0, sizeof(path->ptr[i]) * (path->len - i));
          }
          break;
        }
        case ID_REMAP_RESULT_SOURCE_REMAPPED: {
          path->ptr[i] = RNA_id_pointer_create(path->ptr[i].owner_id);
          /* There is no easy way to check/make path downwards valid, just nullify it.
           * Next redraw will rebuild this anyway. */
          i++;
          memset(&path->ptr[i], 0, sizeof(path->ptr[i]) * (path->len - i));
          path->len = i;
          break;
        }

        case ID_REMAP_RESULT_SOURCE_NOT_MAPPABLE:
        case ID_REMAP_RESULT_SOURCE_UNAVAILABLE: {
          /* Nothing to do. */
          break;
        }
      }
    }
    if (path->len == 0) {
      MEM_SAFE_FREE(sbuts->path);
    }
  }

  if (sbuts->texuser) {
    ButsContextTexture *ct = static_cast<ButsContextTexture *>(sbuts->texuser);
    mappings.apply(reinterpret_cast<ID **>(&ct->texture), ID_REMAP_APPLY_DEFAULT);
    BLI_freelistN(&ct->users);
    ct->user = nullptr;
  }
}

static void buttons_foreach_id(SpaceLink *space_link, LibraryForeachIDData *data)
{
  SpaceProperties *sbuts = reinterpret_cast<SpaceProperties *>(space_link);
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);
  const bool is_readonly = (data_flags & IDWALK_READONLY) != 0;

  BKE_LIB_FOREACHID_PROCESS_ID(data, sbuts->pinid, IDWALK_CB_NOP);
  if (!is_readonly) {
    if (sbuts->pinid == nullptr) {
      sbuts->flag &= ~SB_PIN_CONTEXT;
    }
    /* NOTE: Restoring path pointers is complicated, if not impossible, because this contains
     * data pointers too, not just ID ones. See #40046. */
    MEM_SAFE_FREE(sbuts->path);
  }

  if (sbuts->texuser) {
    ButsContextTexture *ct = static_cast<ButsContextTexture *>(sbuts->texuser);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, ct->texture, IDWALK_CB_NOP);

    if (!is_readonly) {
      BLI_freelistN(&ct->users);
      ct->user = nullptr;
    }
  }
}

static void buttons_space_blend_read_data(BlendDataReader * /*reader*/, SpaceLink *sl)
{
  SpaceProperties *sbuts = (SpaceProperties *)sl;

  sbuts->path = nullptr;
  sbuts->texuser = nullptr;
  sbuts->mainbo = sbuts->mainb;
  sbuts->mainbuser = sbuts->mainb;
  sbuts->runtime = nullptr;
}

static void buttons_space_blend_read_after_liblink(BlendLibReader * /*reader*/,
                                                   ID * /*parent_id*/,
                                                   SpaceLink *sl)
{
  SpaceProperties *sbuts = reinterpret_cast<SpaceProperties *>(sl);

  if (sbuts->pinid == nullptr) {
    sbuts->flag &= ~SB_PIN_CONTEXT;
  }
}

static void buttons_space_blend_write(BlendWriter *writer, SpaceLink *sl)
{
  BLO_write_struct(writer, SpaceProperties, sl);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Type Initialization
 * \{ */

void ED_spacetype_buttons()
{
  std::unique_ptr<SpaceType> st = std::make_unique<SpaceType>();
  ARegionType *art;

  st->spaceid = SPACE_PROPERTIES;
  STRNCPY(st->name, "Buttons");

  st->create = buttons_create;
  st->free = buttons_free;
  st->init = buttons_init;
  st->duplicate = buttons_duplicate;
  st->operatortypes = buttons_operatortypes;
  st->keymap = buttons_keymap;
  st->listener = buttons_area_listener;
  st->context = buttons_context;
  st->id_remap = buttons_id_remap;
  st->foreach_id = buttons_foreach_id;
  st->blend_read_data = buttons_space_blend_read_data;
  st->blend_read_after_liblink = buttons_space_blend_read_after_liblink;
  st->blend_write = buttons_space_blend_write;

  /* regions: main window */
  art = static_cast<ARegionType *>(MEM_callocN(sizeof(ARegionType), "spacetype buttons region"));
  art->regionid = RGN_TYPE_WINDOW;
  art->init = buttons_main_region_init;
  art->layout = buttons_main_region_layout;
  art->draw = ED_region_panels_draw;
  art->listener = buttons_main_region_listener;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  buttons_context_register(art);
  BLI_addhead(&st->regiontypes, art);

  /* Register the panel types from modifiers. The actual panels are built per modifier rather
   * than per modifier type. */
  for (int i = 0; i < NUM_MODIFIER_TYPES; i++) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(ModifierType(i));
    if (mti != nullptr && mti->panel_register != nullptr) {
      mti->panel_register(art);
    }
  }
  for (int i = 0; i < NUM_GREASEPENCIL_MODIFIER_TYPES; i++) {
    const GpencilModifierTypeInfo *mti = BKE_gpencil_modifier_get_info(GpencilModifierType(i));
    if (mti != nullptr && mti->panel_register != nullptr) {
      mti->panel_register(art);
    }
  }
  for (int i = 0; i < NUM_SHADER_FX_TYPES; i++) {
    if (i == eShaderFxType_Light_deprecated) {
      continue;
    }
    const ShaderFxTypeInfo *fxti = BKE_shaderfx_get_info(ShaderFxType(i));
    if (fxti != nullptr && fxti->panel_register != nullptr) {
      fxti->panel_register(art);
    }
  }

  /* regions: header */
  art = static_cast<ARegionType *>(MEM_callocN(sizeof(ARegionType), "spacetype buttons region"));
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = buttons_header_region_init;
  art->draw = buttons_header_region_draw;
  art->message_subscribe = buttons_header_region_message_subscribe;
  BLI_addhead(&st->regiontypes, art);

  /* regions: navigation bar */
  art = static_cast<ARegionType *>(
      MEM_callocN(sizeof(ARegionType), "spacetype nav buttons region"));
  art->regionid = RGN_TYPE_NAV_BAR;
  art->prefsizex = AREAMINX;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES | ED_KEYMAP_NAVBAR;
  art->init = buttons_navigation_bar_region_init;
  art->draw = buttons_navigation_bar_region_draw;
  art->message_subscribe = buttons_navigation_bar_region_message_subscribe;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(std::move(st));
}

/** \} */

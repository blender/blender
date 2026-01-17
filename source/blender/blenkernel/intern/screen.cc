/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include <fmt/format.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_idprop.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_preview_image.hh"
#include "BKE_screen.hh"

#include "BLO_read_write.hh"

/* TODO(@JulianEisel): For asset shelf region reading/writing. Region read/write should be done via
 * a #ARegionType callback. */
#include "../editors/asset/ED_asset_shelf.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

#include "WM_types.hh"

#include "CLG_log.h"

namespace blender {

static CLG_LogRef LOG_BLEND_DOVERSION = {"blend.doversion"};

/* -------------------------------------------------------------------- */
/** \name ID Type Implementation
 * \{ */

static void screen_init_data(ID *id)
{
  bScreen *screen = id_cast<bScreen *>(id);

  screen->do_draw = true;
  screen->do_refresh = true;
  screen->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;
}

static void screen_free_data(ID *id)
{
  bScreen *screen = id_cast<bScreen *>(id);

  /* No animation-data here. */

  for (ARegion &region : screen->regionbase) {
    BKE_area_region_free(nullptr, &region);
  }

  BLI_freelistN(&screen->regionbase);

  BKE_screen_area_map_free(AREAMAP_FROM_SCREEN(screen));

  BKE_previewimg_free(&screen->preview);

  /* Region and timer are freed by the window manager. */
  /* Cannot use MEM_SAFE_FREE, as #wmTooltipState type is only defined in `WM_types.hh`, which is
   * currently not included here. */
  if (screen->tool_tip) {
    MEM_freeN(static_cast<void *>(screen->tool_tip));
    screen->tool_tip = nullptr;
  }
}

static void screen_copy_data(Main * /*bmain*/,
                             std::optional<Library *> owner_library,
                             ID *id_dst,
                             const ID *id_src,
                             int /*flag*/)
{
  /* Workspaces should always be local data currently. */
  BLI_assert(!owner_library || owner_library == nullptr);
  UNUSED_VARS_NDEBUG(owner_library);

  bScreen *screen_dst = id_cast<bScreen *>(id_dst);
  const bScreen *screen_src = id_cast<const bScreen *>(id_src);

  screen_dst->do_draw = true;
  screen_dst->do_refresh = true;
  screen_dst->redraws_flag = TIME_ALL_3D_WIN | TIME_ALL_ANIM_WIN;

  screen_dst->state = SCREENNORMAL;

  screen_dst->flag = screen_src->flag;

  BLI_duplicatelist(&screen_dst->vertbase, &screen_src->vertbase);
  BLI_duplicatelist(&screen_dst->edgebase, &screen_src->edgebase);
  BLI_duplicatelist(&screen_dst->areabase, &screen_src->areabase);
  BLI_listbase_clear(&screen_dst->regionbase);

  {
    ScrVert *sv_dst = static_cast<ScrVert *>(screen_dst->vertbase.first);
    ScrVert *sv_src = static_cast<ScrVert *>(screen_src->vertbase.first);
    for (; sv_dst && sv_src; sv_dst = sv_dst->next, sv_src = sv_src->next) {
      sv_src->newv = sv_dst;
    }
  }

  for (ScrEdge &se_dst : screen_dst->edgebase) {
    se_dst.v1 = se_dst.v1->newv;
    se_dst.v2 = se_dst.v2->newv;
    BKE_screen_sort_scrvert(&(se_dst.v1), &(se_dst.v2));
  }

  {
    ScrArea *area_dst = static_cast<ScrArea *>(screen_dst->areabase.first);
    ScrArea *area_src = static_cast<ScrArea *>(screen_src->areabase.first);
    for (; area_dst && area_src; area_dst = area_dst->next, area_src = area_src->next) {
      area_dst->v1 = area_dst->v1->newv;
      area_dst->v2 = area_dst->v2->newv;
      area_dst->v3 = area_dst->v3->newv;
      area_dst->v4 = area_dst->v4->newv;

      BLI_listbase_clear(&area_dst->spacedata);
      BLI_listbase_clear(&area_dst->regionbase);
      BLI_listbase_clear(&area_dst->actionzones);
      BLI_listbase_clear(&area_dst->handlers);

      BKE_area_copy(area_dst, area_src);
    }
  }

  /* Cleanup: reset temp data. */
  for (ScrVert &sv_src : screen_src->vertbase) {
    sv_src.newv = nullptr;
  }
}

void BKE_screen_foreach_id_screen_area(LibraryForeachIDData *data, ScrArea *area)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, area->full, IDWALK_CB_NOP);

  for (SpaceLink &sl : area->spacedata) {
    SpaceType *space_type = BKE_spacetype_from_id(sl.spacetype);

    if (space_type && space_type->foreach_id) {
      space_type->foreach_id(&sl, data);
    }
  }
}

static void screen_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bScreen *screen = reinterpret_cast<bScreen *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, screen->scene, IDWALK_CB_NOP);
  }

  if (flag & IDWALK_INCLUDE_UI) {
    for (ScrArea &area : screen->areabase) {
      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                              BKE_screen_foreach_id_screen_area(data, &area));
    }
  }
}

static void screen_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bScreen *screen = id_cast<bScreen *>(id);

  /* write LibData */
  /* in 2.50+ files, the file identifier for screens is patched, forward compatibility */
  BLO_write_struct_at_address_with_filecode(writer, ID_SCRN, bScreen, id_address, screen);
  BKE_id_blend_write(writer, &screen->id);

  BKE_previewimg_blend_write(writer, screen->preview);

  /* direct data */
  BKE_screen_area_map_blend_write(writer, AREAMAP_FROM_SCREEN(screen));
}

bool BKE_screen_blend_read_data(BlendDataReader *reader, bScreen *screen)
{
  bool success = true;

  screen->regionbase.first = screen->regionbase.last = nullptr;
  screen->context = nullptr;
  screen->active_region = nullptr;
  screen->animtimer = nullptr; /* saved in rare cases */
  screen->tool_tip = nullptr;
  screen->scrubbing = false;

  BLO_read_struct(reader, PreviewImage, &screen->preview);
  BKE_previewimg_blend_read(reader, screen->preview);

  if (!BKE_screen_area_map_blend_read_data(reader, AREAMAP_FROM_SCREEN(screen))) {
    printf("Error reading Screen %s... removing it.\n", screen->id.name + 2);
    success = false;
  }

  return success;
}

/* NOTE: file read without screens option G_FILE_NO_UI;
 * check lib pointers in call below */
static void screen_blend_read_after_liblink(BlendLibReader *reader, ID *id)
{
  bScreen *screen = reinterpret_cast<bScreen *>(id);

  for (ScrArea &area : screen->areabase) {
    BKE_screen_area_blend_read_after_liblink(reader, &screen->id, &area);
  }
}

IDTypeInfo IDType_ID_SCR = {
    /*id_code*/ bScreen::id_type,
    /*id_filter*/ FILTER_ID_SCR,
    /* NOTE: Can actually link to any ID type through UI (e.g. Outliner Editor).
     * This is handled separately though. */
    /*dependencies_id_types*/ FILTER_ID_SCE,
    /*main_listbase_index*/ INDEX_ID_SCR,
    /*struct_size*/ sizeof(bScreen),
    /*name*/ "Screen",
    /*name_plural*/ N_("screens"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_SCREEN,
    /*flags*/ IDTYPE_FLAGS_ONLY_APPEND | IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_NO_MEMFILE_UNDO,
    /*asset_type_info*/ nullptr,

    /*init_data*/ screen_init_data,
    /*copy_data*/ screen_copy_data,
    /*free_data*/ screen_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ screen_foreach_id,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*foreach_working_space_color*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ screen_blend_write,
    /* Cannot be used yet, because #direct_link_screen has a return value. */
    /*blend_read_data*/ nullptr,
    /*blend_read_after_liblink*/ screen_blend_read_after_liblink,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space-type/region-type handling
 * \{ */

/** Keep global; this has to be accessible outside of window-manager. */
static Vector<std::unique_ptr<SpaceType>> &get_space_types()
{
  static Vector<std::unique_ptr<SpaceType>> space_types;
  return space_types;
}

/* not SpaceType itself */
SpaceType::~SpaceType()
{
  for (ARegionType &art : this->regiontypes) {
#ifdef WITH_PYTHON
    BPY_callback_screen_free(&art);
#endif
    BLI_freelistN(&art.drawcalls);

    for (PanelType &pt : art.paneltypes) {
      if (pt.rna_ext.free) {
        pt.rna_ext.free(pt.rna_ext.data);
      }

      BLI_freelistN(&pt.children);
    }

    for (HeaderType &ht : art.headertypes) {
      if (ht.rna_ext.free) {
        ht.rna_ext.free(ht.rna_ext.data);
      }
    }

    BLI_freelistN(&art.paneltypes);
    BLI_freelistN(&art.headertypes);
  }

  BLI_freelistN(&this->regiontypes);
}

void BKE_spacetypes_free()
{
  get_space_types().clear_and_shrink();
}

SpaceType *BKE_spacetype_from_id(int spaceid)
{
  for (std::unique_ptr<SpaceType> &st : get_space_types()) {
    if (st->spaceid == spaceid) {
      return st.get();
    }
  }
  return nullptr;
}

ARegionType *BKE_regiontype_from_id(const SpaceType *st, int regionid)
{
  for (ARegionType &art : st->regiontypes) {
    if (art.regionid == regionid) {
      return &art;
    }
  }
  return nullptr;
}

Span<std::unique_ptr<SpaceType>> BKE_spacetypes_list()
{
  return get_space_types();
}

void BKE_spacetype_register(std::unique_ptr<SpaceType> st)
{
  /* sanity check */
  SpaceType *stype = BKE_spacetype_from_id(st->spaceid);
  if (stype) {
    printf("error: redefinition of spacetype %s\n", stype->name);
    return;
  }

  get_space_types().append(std::move(st));
}

bool BKE_spacetype_exists(int spaceid)
{
  return BKE_spacetype_from_id(spaceid) != nullptr;
}

bool BKE_regiontype_uses_categories(const ARegionType *region_type)
{
  if (BKE_regiontype_uses_category_tabs(region_type)) {
    return true;
  }

  return bool(region_type->flag & ARegionTypeFlag::UsePanelCategories);
}

bool BKE_regiontype_uses_category_tabs(const ARegionType *region_type)
{
  /* Some region types always support category tabs. */
  if (ELEM(region_type->regionid, RGN_TYPE_UI)) {
    return true;
  }

  return bool(region_type->flag & ARegionTypeFlag::UsePanelCategoryTabs);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space handling
 * \{ */

void BKE_spacedata_freelist(ListBaseT<SpaceLink> *lb)
{
  for (SpaceLink &sl : *lb) {
    SpaceType *st = BKE_spacetype_from_id(sl.spacetype);

    /* free regions for pushed spaces */
    for (ARegion &region : sl.regionbase) {
      BKE_area_region_free(st, &region);
    }

    BLI_freelistN(&sl.regionbase);

    if (st && st->free) {
      st->free(&sl);
    }
  }

  BLI_freelistN(lb);
}

static void panel_list_copy(ListBaseT<Panel> *newlb, const ListBaseT<Panel> *lb)
{
  BLI_listbase_clear(newlb);

  for (const Panel &old_panel : *lb) {
    Panel *new_panel = BKE_panel_new(old_panel.type);
    Panel_Runtime *new_runtime = new_panel->runtime;
    *new_panel = old_panel;
    new_panel->runtime = new_runtime;
    new_panel->activedata = nullptr;
    new_panel->drawname = nullptr;

    BLI_listbase_clear(&new_panel->layout_panel_states);
    new_panel->layout_panel_states_clock = old_panel.layout_panel_states_clock;
    for (LayoutPanelState &src_state : old_panel.layout_panel_states) {
      LayoutPanelState *new_state = MEM_dupallocN<LayoutPanelState>(__func__, src_state);
      new_state->idname = BLI_strdup(src_state.idname);
      BLI_addtail(&new_panel->layout_panel_states, new_state);
    }

    BLI_addtail(newlb, new_panel);
    panel_list_copy(&new_panel->children, &old_panel.children);
  }
}

ARegion *BKE_area_region_copy(const SpaceType *st, const ARegion *region)
{
  ARegion *dst = static_cast<ARegion *>(MEM_dupallocN(region));

  dst->runtime = MEM_new<bke::ARegionRuntime>(__func__);
  dst->runtime->type = region->runtime->type;
  dst->runtime->do_draw = region->runtime->do_draw;

  dst->prev = dst->next = nullptr;
  BLI_listbase_clear(&dst->panels_category_active);
  BLI_listbase_clear(&dst->ui_lists);

  /* use optional regiondata callback */
  if (region->regiondata) {
    ARegionType *art = BKE_regiontype_from_id(st, region->regiontype);

    if (art && art->duplicate) {
      dst->regiondata = art->duplicate(region->regiondata);
    }
    else if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
      dst->regiondata = nullptr;
    }
    else {
      dst->regiondata = MEM_dupallocN(region->regiondata);
    }
  }

  panel_list_copy(&dst->panels, &region->panels);

  BLI_listbase_clear(&dst->ui_previews);
  BLI_duplicatelist(&dst->ui_previews, &region->ui_previews);
  BLI_listbase_clear(&dst->view_states);
  BLI_duplicatelist(&dst->view_states, &region->view_states);

  return dst;
}

ARegion *BKE_area_region_new()
{
  ARegion *region = MEM_new_for_free<ARegion>(__func__);
  region->runtime = MEM_new<bke::ARegionRuntime>(__func__);
  return region;
}

/* from lb_src to lb_dst, lb_dst is supposed to be freed */
static void region_copylist(SpaceType *st, ListBaseT<ARegion> *lb_dst, ListBaseT<ARegion> *lb_src)
{
  /* to be sure */
  BLI_listbase_clear(lb_dst);

  for (ARegion &region : *lb_src) {
    ARegion *region_new = BKE_area_region_copy(st, &region);
    BLI_addtail(lb_dst, region_new);
  }
}

void BKE_spacedata_copylist(ListBaseT<SpaceLink> *lb_dst, ListBaseT<SpaceLink> *lb_src)
{
  BLI_listbase_clear(lb_dst); /* to be sure */

  for (SpaceLink &sl : *lb_src) {
    SpaceType *st = BKE_spacetype_from_id(sl.spacetype);

    if (st && st->duplicate) {
      SpaceLink *slnew = st->duplicate(&sl);

      BLI_addtail(lb_dst, slnew);

      region_copylist(st, &slnew->regionbase, &sl.regionbase);
    }
  }
}

void BKE_spacedata_draw_locks(ARegionDrawLockFlags lock_flags)
{
  for (std::unique_ptr<SpaceType> &st : get_space_types()) {
    for (ARegionType &art : st->regiontypes) {
      if (lock_flags != 0) {
        art.do_lock = (art.lock & lock_flags);
      }
      else {
        art.do_lock = 0;
      }
    }
  }
}

ARegion *BKE_spacedata_find_region_type(const SpaceLink *slink,
                                        const ScrArea *area,
                                        int region_type)
{
  const bool is_slink_active = slink == area->spacedata.first;
  const ListBaseT<ARegion> *regionbase = (is_slink_active) ? &area->regionbase :
                                                             &slink->regionbase;
  ARegion *region = nullptr;

  BLI_assert(BLI_findindex(&area->spacedata, slink) != -1);

  for (ARegion &region_iter : *regionbase) {
    if (region_iter.regiontype == region_type) {
      region = &region_iter;
      break;
    }
  }

  /* Should really unit test this instead. */
  BLI_assert(!is_slink_active || region == BKE_area_find_region_type(area, region_type));

  return region;
}

static void (*spacedata_id_remap_cb)(ScrArea *area,
                                     SpaceLink *sl,
                                     ID *old_id,
                                     ID *new_id) = nullptr;

void BKE_spacedata_callback_id_remap_set(void (*func)(ScrArea *area, SpaceLink *sl, ID *, ID *))
{
  spacedata_id_remap_cb = func;
}

void BKE_spacedata_id_unref(ScrArea *area, SpaceLink *sl, ID *id)
{
  if (spacedata_id_remap_cb) {
    spacedata_id_remap_cb(area, sl, id, nullptr);
  }
}

/**
 * Avoid bad-level calls to #WM_gizmomap_tag_refresh.
 */
static void (*region_refresh_tag_gizmomap_callback)(wmGizmoMap *) = nullptr;

void BKE_region_callback_refresh_tag_gizmomap_set(void (*callback)(wmGizmoMap *))
{
  region_refresh_tag_gizmomap_callback = callback;
}

void BKE_screen_gizmo_tag_refresh(bScreen *screen)
{
  if (region_refresh_tag_gizmomap_callback == nullptr) {
    return;
  }

  for (ScrArea &area : screen->areabase) {
    for (ARegion &region : area.regionbase) {
      if (region.runtime->gizmo_map != nullptr) {
        region_refresh_tag_gizmomap_callback(region.runtime->gizmo_map);
      }
    }
  }
}

void BKE_screen_runtime_refresh_for_blendfile(bScreen *screen)
{
  for (ScrArea &area : screen->areabase) {
    area.runtime.tool = nullptr;
    area.runtime.is_tool_set = false;
  }
}

/**
 * Avoid bad-level calls to #WM_gizmomap_delete.
 */
static void (*region_free_gizmomap_callback)(wmGizmoMap *) = nullptr;

void BKE_region_callback_free_gizmomap_set(void (*callback)(wmGizmoMap *))
{
  region_free_gizmomap_callback = callback;
}

LayoutPanelState *BKE_panel_layout_panel_state_ensure(Panel *panel,
                                                      const StringRef idname,
                                                      const bool default_closed)
{
  ListBaseT<LayoutPanelState> &layout_panel_states =
      panel->runtime->popup_layout_panel_states ? *panel->runtime->popup_layout_panel_states :
                                                  panel->layout_panel_states;
  const uint32_t logical_time = ++panel->layout_panel_states_clock;
  /* Overflow happened, reset all last used times. Not sure if this will ever happen in practice,
   * but better handle the overflow explicitly. */
  if (logical_time == 0) {
    for (LayoutPanelState &state : layout_panel_states) {
      state.last_used = 0;
    }
  }
  for (LayoutPanelState &state : layout_panel_states) {
    if (state.idname == idname) {
      state.last_used = logical_time;
      return &state;
    }
  }
  LayoutPanelState *state = MEM_new_for_free<LayoutPanelState>(__func__);
  state->idname = BLI_strdupn(idname.data(), idname.size());
  SET_FLAG_FROM_TEST(state->flag, !default_closed, LAYOUT_PANEL_STATE_FLAG_OPEN);
  state->last_used = logical_time;
  BLI_addtail(&layout_panel_states, state);
  return state;
}

Panel *BKE_panel_new(PanelType *panel_type)
{
  Panel *panel = MEM_new_for_free<Panel>(__func__);
  panel->runtime = MEM_new<Panel_Runtime>(__func__);
  panel->type = panel_type;
  if (panel_type) {
    STRNCPY_UTF8(panel->panelname, panel_type->idname);
  }
  return panel;
}

static void layout_panel_state_delete(LayoutPanelState *state)
{
  MEM_freeN(state->idname);
  MEM_freeN(state);
}

void BKE_panel_free(Panel *panel)
{
  MEM_SAFE_FREE(panel->activedata);
  MEM_SAFE_FREE(panel->drawname);

  for (LayoutPanelState &state : panel->layout_panel_states.items_mutable()) {
    BLI_remlink(&panel->layout_panel_states, &state);
    layout_panel_state_delete(&state);
  }

  MEM_delete(panel->runtime);
  MEM_freeN(panel);
}

static void area_region_panels_free_recursive(Panel *panel)
{
  for (Panel &child_panel : panel->children.items_mutable()) {
    area_region_panels_free_recursive(&child_panel);
  }
  BKE_panel_free(panel);
}

void BKE_area_region_panels_free(ListBaseT<Panel> *panels)
{
  for (Panel &panel : panels->items_mutable()) {
    /* Delete custom data just for parent panels to avoid a double deletion. */
    if (panel.runtime->custom_data_ptr) {
      MEM_delete(panel.runtime->custom_data_ptr);
    }
    area_region_panels_free_recursive(&panel);
  }
  BLI_listbase_clear(panels);
}

void BKE_area_region_free(SpaceType *st, ARegion *region)
{
  if (st) {
    ARegionType *art = BKE_regiontype_from_id(st, region->regiontype);

    if (art && art->free) {
      art->free(region);
    }

    if (region->regiondata && !(region->flag & RGN_FLAG_TEMP_REGIONDATA)) {
      printf("regiondata free error\n");
    }
  }
  else if (region->runtime->type && region->runtime->type->free) {
    region->runtime->type->free(region);
  }

  BKE_area_region_panels_free(&region->panels);

  for (uiList &uilst : region->ui_lists) {
    if (uilst.dyn_data && uilst.dyn_data->free_runtime_data_fn) {
      uilst.dyn_data->free_runtime_data_fn(&uilst);
    }
    if (uilst.properties) {
      IDP_FreeProperty(uilst.properties);
    }
    MEM_SAFE_FREE(uilst.dyn_data);
  }

  if (region->runtime->gizmo_map != nullptr) {
    region_free_gizmomap_callback(region->runtime->gizmo_map);
  }

  BLI_freelistN(&region->ui_lists);
  BLI_freelistN(&region->ui_previews);
  BLI_freelistN(&region->runtime->panels_category);
  BLI_freelistN(&region->panels_category_active);
  BLI_freelistN(&region->view_states);
  MEM_delete(region->runtime);
}

void BKE_screen_area_free(ScrArea *area)
{
  SpaceType *st = BKE_spacetype_from_id(area->spacetype);

  for (ARegion &region : area->regionbase) {
    BKE_area_region_free(st, &region);
  }

  MEM_SAFE_FREE(area->global);
  BLI_freelistN(&area->regionbase);

  BKE_spacedata_freelist(&area->spacedata);

  BLI_freelistN(&area->actionzones);
}

void BKE_screen_area_map_free(ScrAreaMap *area_map)
{
  for (ScrArea &area : area_map->areabase.items_mutable()) {
    BKE_screen_area_free(&area);
  }

  BLI_freelistN(&area_map->vertbase);
  BLI_freelistN(&area_map->edgebase);
  BLI_freelistN(&area_map->areabase);
}

void BKE_screen_free_data(bScreen *screen)
{
  screen_free_data(&screen->id);
}

void BKE_screen_copy_data(bScreen *screen_dst, const bScreen *screen_src)
{
  screen_copy_data(nullptr, std::nullopt, &screen_dst->id, &screen_src->id, 0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen edges & verts
 * \{ */

ScrEdge *BKE_screen_find_edge(const bScreen *screen, ScrVert *v1, ScrVert *v2)
{
  BKE_screen_sort_scrvert(&v1, &v2);
  for (ScrEdge &se : screen->edgebase) {
    if (se.v1 == v1 && se.v2 == v2) {
      return &se;
    }
  }

  return nullptr;
}

void BKE_screen_sort_scrvert(ScrVert **v1, ScrVert **v2)
{
  if (*v1 > *v2) {
    ScrVert *tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
  }
}

void BKE_screen_remove_double_scrverts(bScreen *screen)
{
  for (ScrVert &verg : screen->vertbase) {
    if (verg.newv == nullptr) { /* !!! */
      ScrVert *v1 = verg.next;
      while (v1) {
        if (v1->newv == nullptr) { /* !?! */
          if (v1->vec.x == verg.vec.x && v1->vec.y == verg.vec.y) {
            // printf("doublevert\n");
            v1->newv = &verg;
          }
        }
        v1 = v1->next;
      }
    }
  }

  /* replace pointers in edges and faces */
  for (ScrEdge &se : screen->edgebase) {
    if (se.v1->newv) {
      se.v1 = se.v1->newv;
    }
    if (se.v2->newv) {
      se.v2 = se.v2->newv;
    }
    /* edges changed: so.... */
    BKE_screen_sort_scrvert(&(se.v1), &(se.v2));
  }
  for (ScrArea &area : screen->areabase) {
    if (area.v1->newv) {
      area.v1 = area.v1->newv;
    }
    if (area.v2->newv) {
      area.v2 = area.v2->newv;
    }
    if (area.v3->newv) {
      area.v3 = area.v3->newv;
    }
    if (area.v4->newv) {
      area.v4 = area.v4->newv;
    }
  }

  /* remove */
  for (ScrVert &verg : screen->vertbase.items_mutable()) {
    if (verg.newv) {
      BLI_remlink(&screen->vertbase, &verg);
      MEM_freeN(&verg);
    }
  }
}

void BKE_screen_remove_double_scredges(bScreen *screen)
{
  /* compare */
  for (ScrEdge &verg : screen->edgebase) {
    ScrEdge *se = verg.next;
    while (se) {
      ScrEdge *sn = se->next;
      if (verg.v1 == se->v1 && verg.v2 == se->v2) {
        BLI_remlink(&screen->edgebase, se);
        MEM_freeN(se);
      }
      se = sn;
    }
  }
}

void BKE_screen_remove_unused_scredges(bScreen *screen)
{
  /* sets flags when edge is used in area */

  for (const auto [a, area] : screen->areabase.enumerate()) {
    ScrEdge *se = BKE_screen_find_edge(screen, area.v1, area.v2);
    if (se == nullptr) {
      printf("error: area %d edge 1 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area.v2, area.v3);
    if (se == nullptr) {
      printf("error: area %d edge 2 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area.v3, area.v4);
    if (se == nullptr) {
      printf("error: area %d edge 3 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area.v4, area.v1);
    if (se == nullptr) {
      printf("error: area %d edge 4 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
  }
  for (ScrEdge &se : screen->edgebase.items_mutable()) {
    if (se.flag == 0) {
      BLI_remlink(&screen->edgebase, &se);
      MEM_freeN(&se);
    }
    else {
      se.flag = 0;
    }
  }
}

void BKE_screen_remove_unused_scrverts(bScreen *screen)
{
  /* we assume edges are ok */
  for (ScrEdge &se : screen->edgebase) {
    se.v1->flag = 1;
    se.v2->flag = 1;
  }

  for (ScrVert &sv : screen->vertbase.items_mutable()) {
    if (sv.flag == 0) {
      BLI_remlink(&screen->vertbase, &sv);
      MEM_freeN(&sv);
    }
    else {
      sv.flag = 0;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

ARegion *BKE_region_find_in_listbase_by_type(const ListBaseT<ARegion> *regionbase,
                                             const int region_type)
{
  for (ARegion &region : *regionbase) {
    if (region.regiontype == region_type) {
      return &region;
    }
  }

  return nullptr;
}

void BKE_area_copy(ScrArea *area_dst, ScrArea *area_src)
{
  constexpr short flag_copy = HEADER_NO_PULLDOWN;

  area_dst->spacetype = area_src->spacetype;
  area_dst->type = area_src->type;

  /* Remove 'restore from fullscreen' data from the new copy. */
  area_dst->full = nullptr;

  area_dst->flag = (area_dst->flag & ~flag_copy) | (area_src->flag & flag_copy);

  /* Spaces. */
  BKE_spacedata_copylist(&area_dst->spacedata, &area_src->spacedata);

  /* Regions. */
  BLI_listbase_clear(&area_dst->regionbase);
  /* NOTE: SPACE_EMPTY is possible on new screens. */
  SpaceType *st = BKE_spacetype_from_id(area_src->spacetype);
  for (ARegion &region_src : area_src->regionbase) {
    ARegion *region_dst = BKE_area_region_copy(st, &region_src);
    BLI_addtail(&area_dst->regionbase, region_dst);
  }
}

ARegion *BKE_area_find_region_type(const ScrArea *area, int region_type)
{
  if (area) {
    for (ARegion &region : area->regionbase) {
      if (region.regiontype == region_type) {
        return &region;
      }
    }
  }

  return nullptr;
}

ARegion *BKE_area_find_region_active_win(const ScrArea *area)
{
  if (area == nullptr) {
    return nullptr;
  }

  ARegion *region = static_cast<ARegion *>(
      BLI_findlink(&area->regionbase, area->region_active_win));
  if (region && (region->regiontype == RGN_TYPE_WINDOW)) {
    return region;
  }

  /* fall back to any */
  return BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
}

ARegion *BKE_area_find_region_xy(const ScrArea *area, const int regiontype, const int xy[2])
{
  if (area == nullptr) {
    return nullptr;
  }

  for (ARegion &region : area->regionbase) {
    if (ELEM(regiontype, RGN_TYPE_ANY, region.regiontype)) {
      if (BLI_rcti_isect_pt_v(&region.winrct, xy)) {
        return &region;
      }
    }
  }
  return nullptr;
}

ARegion *BKE_screen_find_region_type(const bScreen *screen, const int region_type)
{
  for (ARegion &region : screen->regionbase) {
    if (region_type == region.regiontype) {
      return &region;
    }
  }
  return nullptr;
}

ARegion *BKE_screen_find_region_xy(const bScreen *screen, const int regiontype, const int xy[2])
{
  for (ARegion &region : screen->regionbase) {
    if (ELEM(regiontype, RGN_TYPE_ANY, region.regiontype)) {
      if (BLI_rcti_isect_pt_v(&region.winrct, xy)) {
        return &region;
      }
    }
  }
  return nullptr;
}

ScrArea *BKE_screen_find_area_from_space(const bScreen *screen, const SpaceLink *sl)
{
  for (ScrArea &area : screen->areabase) {
    if (BLI_findindex(&area.spacedata, sl) != -1) {
      return &area;
    }
  }

  return nullptr;
}

ARegion *BKE_screen_find_region_in_space(const bScreen *screen,
                                         const SpaceLink *sl,
                                         const int region_type)
{
  for (ScrArea &area : screen->areabase) {
    for (SpaceLink &slink : area.spacedata) {
      if (&slink == sl) {
        ListBaseT<ARegion> *regionbase = (&slink == area.spacedata.first) ? &area.regionbase :
                                                                            &slink.regionbase;
        return BKE_region_find_in_listbase_by_type(regionbase, region_type);
      }
    }
  }
  return nullptr;
}

std::optional<std::string> BKE_screen_path_from_screen_to_space(const PointerRNA *ptr)
{
  if (GS(ptr->owner_id->name) != ID_SCR) {
    BLI_assert_unreachable();
    return std::nullopt;
  }

  const bScreen *screen = reinterpret_cast<const bScreen *>(ptr->owner_id);
  const SpaceLink *link = static_cast<const SpaceLink *>(ptr->data);

  for (const auto [area_index, area] : screen->areabase.enumerate()) {
    const int space_index = BLI_findindex(&area.spacedata, link);
    if (space_index != -1) {
      return fmt::format("areas[{}].spaces[{}]", area_index, space_index);
    }
  }
  return std::nullopt;
}

ScrArea *BKE_screen_find_big_area(const bScreen *screen, const int spacetype, const short min)
{
  ScrArea *big = nullptr;
  int maxsize = 0;

  for (ScrArea &area : screen->areabase) {
    if (ELEM(spacetype, SPACE_TYPE_ANY, area.spacetype)) {
      if (min <= area.winx && min <= area.winy) {
        int size = area.winx * area.winy;
        if (size > maxsize) {
          maxsize = size;
          big = &area;
        }
      }
    }
  }

  return big;
}

ScrArea *BKE_screen_area_map_find_area_xy(const ScrAreaMap *areamap,
                                          const int spacetype,
                                          const int xy[2])
{
  for (ScrArea &area : areamap->areabase) {
    /* Test area's outer screen verts, not inner `area->totrct`. */
    if (xy[0] >= area.v1->vec.x && xy[0] <= area.v4->vec.x && xy[1] >= area.v1->vec.y &&
        xy[1] <= area.v2->vec.y)
    {
      if (ELEM(spacetype, SPACE_TYPE_ANY, area.spacetype)) {
        return &area;
      }
      break;
    }
  }
  return nullptr;
}
ScrArea *BKE_screen_find_area_xy(const bScreen *screen, const int spacetype, const int xy[2])
{
  return BKE_screen_area_map_find_area_xy(AREAMAP_FROM_SCREEN(screen), spacetype, xy);
}

void BKE_screen_view3d_sync(View3D *v3d, Scene *scene)
{
  if (v3d->scenelock && v3d->localvd == nullptr) {
    v3d->camera = scene->camera;

    if (v3d->camera == nullptr) {
      for (ARegion &region : v3d->regionbase) {
        if (region.regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region.regiondata);
          if (rv3d->persp == RV3D_CAMOB) {
            rv3d->persp = RV3D_PERSP;
          }
        }
      }
    }
  }
}

void BKE_screen_view3d_scene_sync(bScreen *screen, Scene *scene)
{
  /* are there cameras in the views that are not in the scene? */
  for (ScrArea &area : screen->areabase) {
    for (SpaceLink &sl : area.spacedata) {
      if (sl.spacetype == SPACE_VIEW3D) {
        View3D *v3d = reinterpret_cast<View3D *>(&sl);
        BKE_screen_view3d_sync(v3d, scene);
      }
    }
  }
}

void BKE_screen_view3d_shading_init(View3DShading *shading)
{
  *shading = View3DShading();
}

ARegion *BKE_screen_find_main_region_at_xy(const bScreen *screen,
                                           const int space_type,
                                           const int xy[2])
{
  ScrArea *area = BKE_screen_find_area_xy(screen, space_type, xy);
  if (!area) {
    return nullptr;
  }
  return BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, xy);
}

/* Magic zoom calculation, no idea what it signifies, if you find out, tell me! -zr
 *
 * Simple, its magic dude! Well, to be honest,
 * this gives a natural feeling zooming with multiple keypad presses (ton). */

float BKE_screen_view3d_zoom_to_fac(float camzoom)
{
  return powf((float(M_SQRT2) + camzoom / 50.0f), 2.0f) / 4.0f;
}

float BKE_screen_view3d_zoom_from_fac(float zoomfac)
{
  return ((sqrtf(4.0f * zoomfac) - float(M_SQRT2)) * 50.0f);
}

bool BKE_screen_is_fullscreen_area(const bScreen *screen)
{
  return ELEM(screen->state, SCREENMAXIMIZED, SCREENFULL);
}

bool BKE_screen_is_used(const bScreen *screen)
{
  return (screen->winid != 0);
}

void BKE_screen_header_alignment_reset(bScreen *screen)
{
  int alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;
  for (ScrArea &area : screen->areabase) {
    for (ARegion &region : area.regionbase) {
      if (ELEM(region.regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
        if (ELEM(area.spacetype, SPACE_FILE, SPACE_USERPREF, SPACE_OUTLINER, SPACE_PROPERTIES)) {
          region.alignment = RGN_ALIGN_TOP;
          continue;
        }
        region.alignment = alignment;
      }
      if (region.regiontype == RGN_TYPE_FOOTER) {
        if (ELEM(area.spacetype, SPACE_FILE, SPACE_USERPREF, SPACE_OUTLINER, SPACE_PROPERTIES)) {
          region.alignment = RGN_ALIGN_BOTTOM;
          continue;
        }
        region.alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
      }
    }
  }
  screen->do_refresh = true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend File IO (Screen & Related Data)
 * \{ */

void BKE_screen_view3d_shading_blend_write(BlendWriter *writer, View3DShading *shading)
{
  if (shading->prop) {
    IDP_BlendWrite(writer, shading->prop);
  }
}

void BKE_screen_view3d_shading_blend_read_data(BlendDataReader *reader, View3DShading *shading)
{
  if (shading->prop) {
    BLO_read_struct(reader, IDProperty, &shading->prop);
    IDP_BlendDataRead(reader, &shading->prop);
  }
}

static void write_region(BlendWriter *writer, ARegion *region, int spacetype)
{
  ARegion region_copy = *region;
  region_copy.runtime = nullptr;
  BLO_write_struct_at_address(writer, ARegion, region, &region_copy);

  if (region->regiondata) {
    if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
      return;
    }

    if (region->regiontype == RGN_TYPE_ASSET_SHELF) {
      ed::asset::shelf::region_blend_write(writer, region);
      return;
    }

    switch (spacetype) {
      case SPACE_VIEW3D:
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          writer->write_struct(rv3d);

          if (rv3d->localvd) {
            writer->write_struct(rv3d->localvd);
          }
          if (rv3d->clipbb) {
            writer->write_struct(rv3d->clipbb);
          }
        }
        else {
          printf("regiondata write missing!\n");
        }
        break;
      default:
        printf("regiondata write missing!\n");
    }
  }
}

static void write_uilist(BlendWriter *writer, uiList *ui_list)
{
  writer->write_struct(ui_list);

  if (ui_list->properties) {
    IDP_BlendWrite(writer, ui_list->properties);
  }
}

static void write_panel_list(BlendWriter *writer, ListBaseT<Panel> *lb)
{
  for (Panel &panel : *lb) {
    Panel panel_copy = panel;
    panel_copy.runtime_flag = 0;
    panel_copy.runtime = nullptr;
    BLO_write_struct_at_address(writer, Panel, &panel, &panel_copy);
    BLO_write_struct_list(writer, LayoutPanelState, &panel.layout_panel_states);
    for (LayoutPanelState &state : panel.layout_panel_states) {
      BLO_write_string(writer, state.idname);
    }
    write_panel_list(writer, &panel.children);
  }
}

static void write_area(BlendWriter *writer, ScrArea *area)
{
  for (ARegion &region : area->regionbase) {
    write_region(writer, &region, area->spacetype);
    write_panel_list(writer, &region.panels);

    for (PanelCategoryStack &pc_act : region.panels_category_active) {
      writer->write_struct(&pc_act);
    }

    for (uiList &ui_list : region.ui_lists) {
      write_uilist(writer, &ui_list);
    }

    for (uiPreview &ui_preview : region.ui_previews) {
      writer->write_struct(&ui_preview);
    }

    for (uiViewStateLink &view_state : region.view_states) {
      writer->write_struct(&view_state);
    }
  }

  for (SpaceLink &sl : area->spacedata) {
    for (ARegion &region : sl.regionbase) {
      write_region(writer, &region, sl.spacetype);
    }

    SpaceType *space_type = BKE_spacetype_from_id(sl.spacetype);
    if (space_type && space_type->blend_write) {
      space_type->blend_write(writer, &sl);
    }
  }
}

void BKE_screen_area_map_blend_write(BlendWriter *writer, ScrAreaMap *area_map)
{
  BLO_write_struct_list(writer, ScrVert, &area_map->vertbase);
  BLO_write_struct_list(writer, ScrEdge, &area_map->edgebase);
  for (ScrArea &area : area_map->areabase) {
    area.butspacetype = area.spacetype; /* Just for compatibility, will be reset below. */

    writer->write_struct(&area);

    writer->write_struct(area.global);

    write_area(writer, &area);

    area.butspacetype = SPACE_EMPTY; /* Unset again, was changed above. */
  }
}

static void remove_least_recently_used_panel_states(Panel &panel, const int64_t max_kept)
{
  Vector<LayoutPanelState *, 1024> all_states;
  for (LayoutPanelState &state : panel.layout_panel_states) {
    all_states.append(&state);
  }
  if (all_states.size() <= max_kept) {
    return;
  }
  std::sort(all_states.begin(),
            all_states.end(),
            [](const LayoutPanelState *a, const LayoutPanelState *b) {
              return a->last_used < b->last_used;
            });
  for (LayoutPanelState *state : all_states.as_span().drop_back(max_kept)) {
    BLI_remlink(&panel.layout_panel_states, state);
    layout_panel_state_delete(state);
  }
}

static void direct_link_panel_list(BlendDataReader *reader, ListBaseT<Panel> *lb)
{
  BLO_read_struct_list(reader, Panel, lb);

  for (Panel &panel : *lb) {
    panel.runtime = MEM_new<Panel_Runtime>(__func__);
    panel.runtime_flag = 0;
    panel.activedata = nullptr;
    panel.type = nullptr;
    panel.drawname = nullptr;
    BLO_read_struct_list(reader, LayoutPanelState, &panel.layout_panel_states);
    for (LayoutPanelState &state : panel.layout_panel_states) {
      BLO_read_string(reader, &state.idname);
    }
    /* Reduce the number of panel states to a reasonable number. This avoids the list getting
     * arbitrarily large over time. Ideally this could be done more eagerly and not only when
     * loading the file. However, it's hard to make sure that no other code is currently
     * referencing the panel states in other cases. */
    remove_least_recently_used_panel_states(panel, 200);
    direct_link_panel_list(reader, &panel.children);
  }
}

static void direct_link_region(BlendDataReader *reader, ARegion *region, int spacetype)
{
  direct_link_panel_list(reader, &region->panels);

  BLO_read_struct_list(reader, PanelCategoryStack, &region->panels_category_active);

  BLO_read_struct_list(reader, uiList, &region->ui_lists);
  BLO_read_struct_list(reader, uiViewStateLink, &region->view_states);

  /* The area's search filter is runtime only, so we need to clear the active flag on read. */
  /* Clear runtime flags (e.g. search filter is runtime only). */
  region->flag &= ~(RGN_FLAG_SEARCH_FILTER_ACTIVE | RGN_FLAG_POLL_FAILED);

  for (uiList &ui_list : region->ui_lists) {
    ui_list.type = nullptr;
    ui_list.dyn_data = nullptr;
    BLO_read_struct(reader, IDProperty, &ui_list.properties);
    IDP_BlendDataRead(reader, &ui_list.properties);
  }

  BLO_read_struct_list(reader, uiPreview, &region->ui_previews);
  for (uiPreview &ui_preview : region->ui_previews) {
    ui_preview.id_session_uid = MAIN_ID_SESSION_UID_UNSET;
    ui_preview.tag = 0;
  }

  if (spacetype == SPACE_EMPTY) {
    /* unknown space type, don't leak regiondata */
    region->regiondata = nullptr;
  }
  else if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
    /* Runtime data, don't use. */
    region->regiondata = nullptr;
  }
  else {
    if (spacetype == SPACE_VIEW3D) {
      if (region->regiontype == RGN_TYPE_WINDOW) {
        BLO_read_struct(reader, RegionView3D, &region->regiondata);

        if (region->regiondata == nullptr) {
          /* To avoid crashing on some old files. */
          region->regiondata = MEM_new_for_free<RegionView3D>("region view3d");
        }

        RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

        BLO_read_struct(reader, RegionView3D, &rv3d->localvd);
        BLO_read_struct(reader, BoundBox, &rv3d->clipbb);

        rv3d->view_render = nullptr;
        rv3d->sms = nullptr;
        rv3d->smooth_timer = nullptr;

        rv3d->rflag &= ~(RV3D_NAVIGATING | RV3D_PAINTING);
        rv3d->runtime_viewlock = 0;
      }
    }
    if (region->regiontype == RGN_TYPE_ASSET_SHELF) {
      ed::asset::shelf::region_blend_read_data(reader, region);
    }
  }

  region->runtime = MEM_new<bke::ARegionRuntime>(__func__);
  region->v2d.sms = nullptr;
  region->v2d.alpha_hor = region->v2d.alpha_vert = 255; /* visible by default */
}

void BKE_screen_view3d_do_versions_250(View3D *v3d, ListBaseT<ARegion> *regions)
{
  for (ARegion &region : *regions) {
    if (region.regiontype == RGN_TYPE_WINDOW && region.regiondata == nullptr) {
      RegionView3D *rv3d;

      rv3d = MEM_new_for_free<RegionView3D>("region v3d patch");
      rv3d->persp = char(v3d->persp);
      rv3d->view = char(v3d->view);
      rv3d->dist = v3d->dist;
      copy_v3_v3(rv3d->ofs, v3d->ofs);
      copy_qt_qt(rv3d->viewquat, v3d->viewquat);
      region.regiondata = rv3d;
    }
  }

  /* this was not initialized correct always */
  if (v3d->gridsubdiv == 0) {
    v3d->gridsubdiv = 10;
  }
}

static void direct_link_area(BlendDataReader *reader, ScrArea *area)
{
  BLO_read_struct_list(reader, SpaceLink, &(area->spacedata));
  BLO_read_struct_list(reader, ARegion, &(area->regionbase));

  BLI_listbase_clear(&area->handlers);
  area->type = nullptr; /* spacetype callbacks */

  area->runtime = ScrArea_Runtime{};

  /* Should always be unset so that rna_Area_type_get works correctly. */
  area->butspacetype = SPACE_EMPTY;

  area->region_active_win = -1;

  area->flag &= ~AREA_FLAG_ACTIVE_TOOL_UPDATE;

  BLO_read_struct(reader, ScrGlobalAreaData, &area->global);

  /* if we do not have the spacetype registered we cannot
   * free it, so don't allocate any new memory for such spacetypes. */
  if (!BKE_spacetype_exists(area->spacetype)) {
    /* Hint for versioning code to replace deprecated space types. */
    area->butspacetype = area->spacetype;

    area->spacetype = SPACE_EMPTY;
  }

  for (ARegion &region : area->regionbase) {
    direct_link_region(reader, &region, area->spacetype);
  }

  /* accident can happen when read/save new file with older version */
  /* 2.50: we now always add spacedata for info */
  if (area->spacedata.first == nullptr) {
    SpaceInfo *sinfo = MEM_new_for_free<SpaceInfo>("spaceinfo");
    area->spacetype = sinfo->spacetype = SPACE_INFO;
    BLI_addtail(&area->spacedata, sinfo);
  }
  /* add local view3d too */
  else if (area->spacetype == SPACE_VIEW3D) {
    BKE_screen_view3d_do_versions_250(static_cast<View3D *>(area->spacedata.first),
                                      &area->regionbase);
  }

  for (SpaceLink &sl : area->spacedata) {
    BLO_read_struct_list(reader, ARegion, &(sl.regionbase));

    /* if we do not have the spacetype registered we cannot
     * free it, so don't allocate any new memory for such spacetypes. */
    if (!BKE_spacetype_exists(sl.spacetype)) {
      sl.spacetype = SPACE_EMPTY;
    }

    for (ARegion &region : sl.regionbase) {
      direct_link_region(reader, &region, sl.spacetype);
    }

    SpaceType *space_type = BKE_spacetype_from_id(sl.spacetype);
    if (space_type && space_type->blend_read_data) {
      space_type->blend_read_data(reader, &sl);
    }
  }

  BLI_listbase_clear(&area->actionzones);

  BLO_read_struct(reader, ScrVert, &area->v1);
  BLO_read_struct(reader, ScrVert, &area->v2);
  BLO_read_struct(reader, ScrVert, &area->v3);
  BLO_read_struct(reader, ScrVert, &area->v4);
}

bool BKE_screen_area_map_blend_read_data(BlendDataReader *reader, ScrAreaMap *area_map)
{
  BLO_read_struct_list(reader, ScrVert, &area_map->vertbase);
  BLO_read_struct_list(reader, ScrEdge, &area_map->edgebase);
  BLO_read_struct_list(reader, ScrArea, &area_map->areabase);
  for (ScrArea &area : area_map->areabase) {
    direct_link_area(reader, &area);
  }

  /* edges */
  for (ScrEdge &se : area_map->edgebase) {
    BLO_read_struct(reader, ScrVert, &se.v1);
    BLO_read_struct(reader, ScrVert, &se.v2);
    BKE_screen_sort_scrvert(&se.v1, &se.v2);

    if (se.v1 == nullptr) {
      BLI_remlink(&area_map->edgebase, &se);

      return false;
    }
  }

  return true;
}

/**
 * Removes all regions whose type cannot be reconstructed. For example files from new versions may
 * be stored with a newly introduced region type that this version cannot handle.
 */
static void regions_remove_invalid(SpaceType *space_type, ListBaseT<ARegion> *regionbase)
{
  for (ARegion &region : regionbase->items_mutable()) {
    if (BKE_regiontype_from_id(space_type, region.regiontype) != nullptr) {
      continue;
    }

    CLOG_WARN(&LOG_BLEND_DOVERSION,
              "Region type %d missing in space type \"%s\" (id: %d) - removing region",
              region.regiontype,
              space_type->name,
              space_type->spaceid);

    BKE_area_region_free(space_type, &region);
    BLI_freelinkN(regionbase, &region);
  }
}

void BKE_screen_area_blend_read_after_liblink(BlendLibReader *reader, ID *parent_id, ScrArea *area)
{
  for (SpaceLink &sl : area->spacedata) {
    SpaceType *space_type = BKE_spacetype_from_id(sl.spacetype);
    ListBaseT<ARegion> *regionbase = (&sl == area->spacedata.first) ? &area->regionbase :
                                                                      &sl.regionbase;

    /* We cannot restore the region type without a valid space type. So delete all regions to make
     * sure no data is kept around that can't be restored safely (like the type dependent
     * #ARegion.regiondata). */
    if (!space_type) {
      for (ARegion &region : regionbase->items_mutable()) {
        BKE_area_region_free(nullptr, &region);
        BLI_freelinkN(regionbase, &region);
      }

      continue;
    }

    if (space_type->blend_read_after_liblink) {
      space_type->blend_read_after_liblink(reader, parent_id, &sl);
    }

    regions_remove_invalid(space_type, regionbase);
  }
}

}  // namespace blender

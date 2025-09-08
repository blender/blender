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

#include "DNA_defaults.h"
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

static CLG_LogRef LOG_BLEND_DOVERSION = {"blend.doversion"};

using blender::Span;
using blender::StringRef;
using blender::Vector;

/* -------------------------------------------------------------------- */
/** \name ID Type Implementation
 * \{ */

static void screen_free_data(ID *id)
{
  bScreen *screen = (bScreen *)id;

  /* No animation-data here. */

  LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
    BKE_area_region_free(nullptr, region);
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

void BKE_screen_foreach_id_screen_area(LibraryForeachIDData *data, ScrArea *area)
{
  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, area->full, IDWALK_CB_NOP);

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    SpaceType *space_type = BKE_spacetype_from_id(sl->spacetype);

    if (space_type && space_type->foreach_id) {
      space_type->foreach_id(sl, data);
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
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_screen_foreach_id_screen_area(data, area));
    }
  }
}

static void screen_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bScreen *screen = (bScreen *)id;

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

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    BKE_screen_area_blend_read_after_liblink(reader, &screen->id, area);
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
    /*flags*/ IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_ONLY_APPEND | IDTYPE_FLAGS_NO_ANIMDATA |
        IDTYPE_FLAGS_NO_MEMFILE_UNDO,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ nullptr,
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
  LISTBASE_FOREACH (ARegionType *, art, &this->regiontypes) {
#ifdef WITH_PYTHON
    BPY_callback_screen_free(art);
#endif
    BLI_freelistN(&art->drawcalls);

    LISTBASE_FOREACH (PanelType *, pt, &art->paneltypes) {
      if (pt->rna_ext.free) {
        pt->rna_ext.free(pt->rna_ext.data);
      }

      BLI_freelistN(&pt->children);
    }

    LISTBASE_FOREACH (HeaderType *, ht, &art->headertypes) {
      if (ht->rna_ext.free) {
        ht->rna_ext.free(ht->rna_ext.data);
      }
    }

    BLI_freelistN(&art->paneltypes);
    BLI_freelistN(&art->headertypes);
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
  LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
    if (art->regionid == regionid) {
      return art;
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

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space handling
 * \{ */

void BKE_spacedata_freelist(ListBase *lb)
{
  LISTBASE_FOREACH (SpaceLink *, sl, lb) {
    SpaceType *st = BKE_spacetype_from_id(sl->spacetype);

    /* free regions for pushed spaces */
    LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
      BKE_area_region_free(st, region);
    }

    BLI_freelistN(&sl->regionbase);

    if (st && st->free) {
      st->free(sl);
    }
  }

  BLI_freelistN(lb);
}

static void panel_list_copy(ListBase *newlb, const ListBase *lb)
{
  BLI_listbase_clear(newlb);

  LISTBASE_FOREACH (const Panel *, old_panel, lb) {
    Panel *new_panel = BKE_panel_new(old_panel->type);
    Panel_Runtime *new_runtime = new_panel->runtime;
    *new_panel = *old_panel;
    new_panel->runtime = new_runtime;
    new_panel->activedata = nullptr;
    new_panel->drawname = nullptr;

    BLI_listbase_clear(&new_panel->layout_panel_states);
    new_panel->layout_panel_states_clock = old_panel->layout_panel_states_clock;
    LISTBASE_FOREACH (LayoutPanelState *, src_state, &old_panel->layout_panel_states) {
      LayoutPanelState *new_state = MEM_dupallocN<LayoutPanelState>(__func__, *src_state);
      new_state->idname = BLI_strdup(src_state->idname);
      BLI_addtail(&new_panel->layout_panel_states, new_state);
    }

    BLI_addtail(newlb, new_panel);
    panel_list_copy(&new_panel->children, &old_panel->children);
  }
}

ARegion *BKE_area_region_copy(const SpaceType *st, const ARegion *region)
{
  ARegion *dst = static_cast<ARegion *>(MEM_dupallocN(region));

  dst->runtime = MEM_new<blender::bke::ARegionRuntime>(__func__);
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
  ARegion *region = MEM_callocN<ARegion>(__func__);
  region->runtime = MEM_new<blender::bke::ARegionRuntime>(__func__);
  return region;
}

/* from lb_src to lb_dst, lb_dst is supposed to be freed */
static void region_copylist(SpaceType *st, ListBase *lb_dst, ListBase *lb_src)
{
  /* to be sure */
  BLI_listbase_clear(lb_dst);

  LISTBASE_FOREACH (ARegion *, region, lb_src) {
    ARegion *region_new = BKE_area_region_copy(st, region);
    BLI_addtail(lb_dst, region_new);
  }
}

void BKE_spacedata_copylist(ListBase *lb_dst, ListBase *lb_src)
{
  BLI_listbase_clear(lb_dst); /* to be sure */

  LISTBASE_FOREACH (SpaceLink *, sl, lb_src) {
    SpaceType *st = BKE_spacetype_from_id(sl->spacetype);

    if (st && st->duplicate) {
      SpaceLink *slnew = st->duplicate(sl);

      BLI_addtail(lb_dst, slnew);

      region_copylist(st, &slnew->regionbase, &sl->regionbase);
    }
  }
}

void BKE_spacedata_draw_locks(ARegionDrawLockFlags lock_flags)
{
  for (std::unique_ptr<SpaceType> &st : get_space_types()) {
    LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
      if (lock_flags != 0) {
        art->do_lock = (art->lock & lock_flags);
      }
      else {
        art->do_lock = 0;
      }
    }
  }
}

ARegion *BKE_spacedata_find_region_type(const SpaceLink *slink,
                                        const ScrArea *area,
                                        int region_type)
{
  const bool is_slink_active = slink == area->spacedata.first;
  const ListBase *regionbase = (is_slink_active) ? &area->regionbase : &slink->regionbase;
  ARegion *region = nullptr;

  BLI_assert(BLI_findindex(&area->spacedata, slink) != -1);

  LISTBASE_FOREACH (ARegion *, region_iter, regionbase) {
    if (region_iter->regiontype == region_type) {
      region = region_iter;
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

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region->runtime->gizmo_map != nullptr) {
        region_refresh_tag_gizmomap_callback(region->runtime->gizmo_map);
      }
    }
  }
}

void BKE_screen_runtime_refresh_for_blendfile(bScreen *screen)
{
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    area->runtime.tool = nullptr;
    area->runtime.is_tool_set = false;
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
  const uint32_t logical_time = ++panel->layout_panel_states_clock;
  /* Overflow happened, reset all last used times. Not sure if this will ever happen in practice,
   * but better handle the overflow explicitly. */
  if (logical_time == 0) {
    LISTBASE_FOREACH (LayoutPanelState *, state, &panel->layout_panel_states) {
      state->last_used = 0;
    }
  }
  LISTBASE_FOREACH (LayoutPanelState *, state, &panel->layout_panel_states) {
    if (state->idname == idname) {
      state->last_used = logical_time;
      return state;
    }
  }
  LayoutPanelState *state = MEM_callocN<LayoutPanelState>(__func__);
  state->idname = BLI_strdupn(idname.data(), idname.size());
  SET_FLAG_FROM_TEST(state->flag, !default_closed, LAYOUT_PANEL_STATE_FLAG_OPEN);
  state->last_used = logical_time;
  BLI_addtail(&panel->layout_panel_states, state);
  return state;
}

Panel *BKE_panel_new(PanelType *panel_type)
{
  Panel *panel = MEM_callocN<Panel>(__func__);
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

  LISTBASE_FOREACH_MUTABLE (LayoutPanelState *, state, &panel->layout_panel_states) {
    BLI_remlink(&panel->layout_panel_states, state);
    layout_panel_state_delete(state);
  }

  MEM_delete(panel->runtime);
  MEM_freeN(panel);
}

static void area_region_panels_free_recursive(Panel *panel)
{
  LISTBASE_FOREACH_MUTABLE (Panel *, child_panel, &panel->children) {
    area_region_panels_free_recursive(child_panel);
  }
  BKE_panel_free(panel);
}

void BKE_area_region_panels_free(ListBase *panels)
{
  LISTBASE_FOREACH_MUTABLE (Panel *, panel, panels) {
    /* Delete custom data just for parent panels to avoid a double deletion. */
    if (panel->runtime->custom_data_ptr) {
      MEM_delete(panel->runtime->custom_data_ptr);
    }
    area_region_panels_free_recursive(panel);
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

  LISTBASE_FOREACH (uiList *, uilst, &region->ui_lists) {
    if (uilst->dyn_data && uilst->dyn_data->free_runtime_data_fn) {
      uilst->dyn_data->free_runtime_data_fn(uilst);
    }
    if (uilst->properties) {
      IDP_FreeProperty(uilst->properties);
    }
    MEM_SAFE_FREE(uilst->dyn_data);
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

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    BKE_area_region_free(st, region);
  }

  MEM_SAFE_FREE(area->global);
  BLI_freelistN(&area->regionbase);

  BKE_spacedata_freelist(&area->spacedata);

  BLI_freelistN(&area->actionzones);
}

void BKE_screen_area_map_free(ScrAreaMap *area_map)
{
  LISTBASE_FOREACH_MUTABLE (ScrArea *, area, &area_map->areabase) {
    BKE_screen_area_free(area);
  }

  BLI_freelistN(&area_map->vertbase);
  BLI_freelistN(&area_map->edgebase);
  BLI_freelistN(&area_map->areabase);
}

void BKE_screen_free_data(bScreen *screen)
{
  screen_free_data(&screen->id);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen edges & verts
 * \{ */

ScrEdge *BKE_screen_find_edge(const bScreen *screen, ScrVert *v1, ScrVert *v2)
{
  BKE_screen_sort_scrvert(&v1, &v2);
  LISTBASE_FOREACH (ScrEdge *, se, &screen->edgebase) {
    if (se->v1 == v1 && se->v2 == v2) {
      return se;
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
  LISTBASE_FOREACH (ScrVert *, verg, &screen->vertbase) {
    if (verg->newv == nullptr) { /* !!! */
      ScrVert *v1 = verg->next;
      while (v1) {
        if (v1->newv == nullptr) { /* !?! */
          if (v1->vec.x == verg->vec.x && v1->vec.y == verg->vec.y) {
            // printf("doublevert\n");
            v1->newv = verg;
          }
        }
        v1 = v1->next;
      }
    }
  }

  /* replace pointers in edges and faces */
  LISTBASE_FOREACH (ScrEdge *, se, &screen->edgebase) {
    if (se->v1->newv) {
      se->v1 = se->v1->newv;
    }
    if (se->v2->newv) {
      se->v2 = se->v2->newv;
    }
    /* edges changed: so.... */
    BKE_screen_sort_scrvert(&(se->v1), &(se->v2));
  }
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->v1->newv) {
      area->v1 = area->v1->newv;
    }
    if (area->v2->newv) {
      area->v2 = area->v2->newv;
    }
    if (area->v3->newv) {
      area->v3 = area->v3->newv;
    }
    if (area->v4->newv) {
      area->v4 = area->v4->newv;
    }
  }

  /* remove */
  LISTBASE_FOREACH_MUTABLE (ScrVert *, verg, &screen->vertbase) {
    if (verg->newv) {
      BLI_remlink(&screen->vertbase, verg);
      MEM_freeN(verg);
    }
  }
}

void BKE_screen_remove_double_scredges(bScreen *screen)
{
  /* compare */
  LISTBASE_FOREACH (ScrEdge *, verg, &screen->edgebase) {
    ScrEdge *se = verg->next;
    while (se) {
      ScrEdge *sn = se->next;
      if (verg->v1 == se->v1 && verg->v2 == se->v2) {
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
  int a = 0;
  LISTBASE_FOREACH_INDEX (ScrArea *, area, &screen->areabase, a) {
    ScrEdge *se = BKE_screen_find_edge(screen, area->v1, area->v2);
    if (se == nullptr) {
      printf("error: area %d edge 1 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area->v2, area->v3);
    if (se == nullptr) {
      printf("error: area %d edge 2 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area->v3, area->v4);
    if (se == nullptr) {
      printf("error: area %d edge 3 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area->v4, area->v1);
    if (se == nullptr) {
      printf("error: area %d edge 4 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
  }
  LISTBASE_FOREACH_MUTABLE (ScrEdge *, se, &screen->edgebase) {
    if (se->flag == 0) {
      BLI_remlink(&screen->edgebase, se);
      MEM_freeN(se);
    }
    else {
      se->flag = 0;
    }
  }
}

void BKE_screen_remove_unused_scrverts(bScreen *screen)
{
  /* we assume edges are ok */
  LISTBASE_FOREACH (ScrEdge *, se, &screen->edgebase) {
    se->v1->flag = 1;
    se->v2->flag = 1;
  }

  LISTBASE_FOREACH_MUTABLE (ScrVert *, sv, &screen->vertbase) {
    if (sv->flag == 0) {
      BLI_remlink(&screen->vertbase, sv);
      MEM_freeN(sv);
    }
    else {
      sv->flag = 0;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

ARegion *BKE_region_find_in_listbase_by_type(const ListBase *regionbase, const int region_type)
{
  LISTBASE_FOREACH (ARegion *, region, regionbase) {
    if (region->regiontype == region_type) {
      return region;
    }
  }

  return nullptr;
}

ARegion *BKE_area_find_region_type(const ScrArea *area, int region_type)
{
  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region->regiontype == region_type) {
        return region;
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

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (ELEM(regiontype, RGN_TYPE_ANY, region->regiontype)) {
      if (BLI_rcti_isect_pt_v(&region->winrct, xy)) {
        return region;
      }
    }
  }
  return nullptr;
}

ARegion *BKE_screen_find_region_type(const bScreen *screen, const int region_type)
{
  LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
    if (region_type == region->regiontype) {
      return region;
    }
  }
  return nullptr;
}

ARegion *BKE_screen_find_region_xy(const bScreen *screen, const int regiontype, const int xy[2])
{
  LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
    if (ELEM(regiontype, RGN_TYPE_ANY, region->regiontype)) {
      if (BLI_rcti_isect_pt_v(&region->winrct, xy)) {
        return region;
      }
    }
  }
  return nullptr;
}

ScrArea *BKE_screen_find_area_from_space(const bScreen *screen, const SpaceLink *sl)
{
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (BLI_findindex(&area->spacedata, sl) != -1) {
      return area;
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

  int area_index;
  LISTBASE_FOREACH_INDEX (const ScrArea *, area, &screen->areabase, area_index) {
    const int space_index = BLI_findindex(&area->spacedata, link);
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

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (ELEM(spacetype, SPACE_TYPE_ANY, area->spacetype)) {
      if (min <= area->winx && min <= area->winy) {
        int size = area->winx * area->winy;
        if (size > maxsize) {
          maxsize = size;
          big = area;
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
  LISTBASE_FOREACH (ScrArea *, area, &areamap->areabase) {
    /* Test area's outer screen verts, not inner `area->totrct`. */
    if (xy[0] >= area->v1->vec.x && xy[0] <= area->v4->vec.x && xy[1] >= area->v1->vec.y &&
        xy[1] <= area->v2->vec.y)
    {
      if (ELEM(spacetype, SPACE_TYPE_ANY, area->spacetype)) {
        return area;
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
      LISTBASE_FOREACH (ARegion *, region, &v3d->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
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
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        BKE_screen_view3d_sync(v3d, scene);
      }
    }
  }
}

void BKE_screen_view3d_shading_init(View3DShading *shading)
{
  const View3DShading *shading_default = DNA_struct_default_get(View3DShading);
  memcpy(shading, shading_default, sizeof(*shading));
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
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
        if (ELEM(area->spacetype, SPACE_FILE, SPACE_USERPREF, SPACE_OUTLINER, SPACE_PROPERTIES)) {
          region->alignment = RGN_ALIGN_TOP;
          continue;
        }
        region->alignment = alignment;
      }
      if (region->regiontype == RGN_TYPE_FOOTER) {
        if (ELEM(area->spacetype, SPACE_FILE, SPACE_USERPREF, SPACE_OUTLINER, SPACE_PROPERTIES)) {
          region->alignment = RGN_ALIGN_BOTTOM;
          continue;
        }
        region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
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
      blender::ed::asset::shelf::region_blend_write(writer, region);
      return;
    }

    switch (spacetype) {
      case SPACE_VIEW3D:
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          BLO_write_struct(writer, RegionView3D, rv3d);

          if (rv3d->localvd) {
            BLO_write_struct(writer, RegionView3D, rv3d->localvd);
          }
          if (rv3d->clipbb) {
            BLO_write_struct(writer, BoundBox, rv3d->clipbb);
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
  BLO_write_struct(writer, uiList, ui_list);

  if (ui_list->properties) {
    IDP_BlendWrite(writer, ui_list->properties);
  }
}

static void write_panel_list(BlendWriter *writer, ListBase *lb)
{
  LISTBASE_FOREACH (Panel *, panel, lb) {
    Panel panel_copy = *panel;
    panel_copy.runtime_flag = 0;
    panel_copy.runtime = nullptr;
    BLO_write_struct_at_address(writer, Panel, panel, &panel_copy);
    BLO_write_struct_list(writer, LayoutPanelState, &panel->layout_panel_states);
    LISTBASE_FOREACH (LayoutPanelState *, state, &panel->layout_panel_states) {
      BLO_write_string(writer, state->idname);
    }
    write_panel_list(writer, &panel->children);
  }
}

static void write_area(BlendWriter *writer, ScrArea *area)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    write_region(writer, region, area->spacetype);
    write_panel_list(writer, &region->panels);

    LISTBASE_FOREACH (PanelCategoryStack *, pc_act, &region->panels_category_active) {
      BLO_write_struct(writer, PanelCategoryStack, pc_act);
    }

    LISTBASE_FOREACH (uiList *, ui_list, &region->ui_lists) {
      write_uilist(writer, ui_list);
    }

    LISTBASE_FOREACH (uiPreview *, ui_preview, &region->ui_previews) {
      BLO_write_struct(writer, uiPreview, ui_preview);
    }

    LISTBASE_FOREACH (uiViewStateLink *, view_state, &region->view_states) {
      BLO_write_struct(writer, uiViewStateLink, view_state);
    }
  }

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
      write_region(writer, region, sl->spacetype);
    }

    SpaceType *space_type = BKE_spacetype_from_id(sl->spacetype);
    if (space_type && space_type->blend_write) {
      space_type->blend_write(writer, sl);
    }
  }
}

void BKE_screen_area_map_blend_write(BlendWriter *writer, ScrAreaMap *area_map)
{
  BLO_write_struct_list(writer, ScrVert, &area_map->vertbase);
  BLO_write_struct_list(writer, ScrEdge, &area_map->edgebase);
  LISTBASE_FOREACH (ScrArea *, area, &area_map->areabase) {
    area->butspacetype = area->spacetype; /* Just for compatibility, will be reset below. */

    BLO_write_struct(writer, ScrArea, area);

    BLO_write_struct(writer, ScrGlobalAreaData, area->global);

    write_area(writer, area);

    area->butspacetype = SPACE_EMPTY; /* Unset again, was changed above. */
  }
}

static void remove_least_recently_used_panel_states(Panel &panel, const int64_t max_kept)
{
  Vector<LayoutPanelState *, 1024> all_states;
  LISTBASE_FOREACH (LayoutPanelState *, state, &panel.layout_panel_states) {
    all_states.append(state);
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

static void direct_link_panel_list(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_struct_list(reader, Panel, lb);

  LISTBASE_FOREACH (Panel *, panel, lb) {
    panel->runtime = MEM_new<Panel_Runtime>(__func__);
    panel->runtime_flag = 0;
    panel->activedata = nullptr;
    panel->type = nullptr;
    panel->drawname = nullptr;
    BLO_read_struct_list(reader, LayoutPanelState, &panel->layout_panel_states);
    LISTBASE_FOREACH (LayoutPanelState *, state, &panel->layout_panel_states) {
      BLO_read_string(reader, &state->idname);
    }
    /* Reduce the number of panel states to a reasonable number. This avoids the list getting
     * arbitrarily large over time. Ideally this could be done more eagerly and not only when
     * loading the file. However, it's hard to make sure that no other code is currently
     * referencing the panel states in other cases. */
    remove_least_recently_used_panel_states(*panel, 200);
    direct_link_panel_list(reader, &panel->children);
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

  LISTBASE_FOREACH (uiList *, ui_list, &region->ui_lists) {
    ui_list->type = nullptr;
    ui_list->dyn_data = nullptr;
    BLO_read_struct(reader, IDProperty, &ui_list->properties);
    IDP_BlendDataRead(reader, &ui_list->properties);
  }

  BLO_read_struct_list(reader, uiPreview, &region->ui_previews);
  LISTBASE_FOREACH (uiPreview *, ui_preview, &region->ui_previews) {
    ui_preview->id_session_uid = MAIN_ID_SESSION_UID_UNSET;
    ui_preview->tag = 0;
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
          region->regiondata = MEM_callocN<RegionView3D>("region view3d");
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
      blender::ed::asset::shelf::region_blend_read_data(reader, region);
    }
  }

  region->runtime = MEM_new<blender::bke::ARegionRuntime>(__func__);
  region->v2d.sms = nullptr;
  region->v2d.alpha_hor = region->v2d.alpha_vert = 255; /* visible by default */
}

void BKE_screen_view3d_do_versions_250(View3D *v3d, ListBase *regions)
{
  LISTBASE_FOREACH (ARegion *, region, regions) {
    if (region->regiontype == RGN_TYPE_WINDOW && region->regiondata == nullptr) {
      RegionView3D *rv3d;

      rv3d = MEM_callocN<RegionView3D>("region v3d patch");
      rv3d->persp = char(v3d->persp);
      rv3d->view = char(v3d->view);
      rv3d->dist = v3d->dist;
      copy_v3_v3(rv3d->ofs, v3d->ofs);
      copy_qt_qt(rv3d->viewquat, v3d->viewquat);
      region->regiondata = rv3d;
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

  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    direct_link_region(reader, region, area->spacetype);
  }

  /* accident can happen when read/save new file with older version */
  /* 2.50: we now always add spacedata for info */
  if (area->spacedata.first == nullptr) {
    SpaceInfo *sinfo = MEM_callocN<SpaceInfo>("spaceinfo");
    area->spacetype = sinfo->spacetype = SPACE_INFO;
    BLI_addtail(&area->spacedata, sinfo);
  }
  /* add local view3d too */
  else if (area->spacetype == SPACE_VIEW3D) {
    BKE_screen_view3d_do_versions_250(static_cast<View3D *>(area->spacedata.first),
                                      &area->regionbase);
  }

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    BLO_read_struct_list(reader, ARegion, &(sl->regionbase));

    /* if we do not have the spacetype registered we cannot
     * free it, so don't allocate any new memory for such spacetypes. */
    if (!BKE_spacetype_exists(sl->spacetype)) {
      sl->spacetype = SPACE_EMPTY;
    }

    LISTBASE_FOREACH (ARegion *, region, &sl->regionbase) {
      direct_link_region(reader, region, sl->spacetype);
    }

    SpaceType *space_type = BKE_spacetype_from_id(sl->spacetype);
    if (space_type && space_type->blend_read_data) {
      space_type->blend_read_data(reader, sl);
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
  LISTBASE_FOREACH (ScrArea *, area, &area_map->areabase) {
    direct_link_area(reader, area);
  }

  /* edges */
  LISTBASE_FOREACH (ScrEdge *, se, &area_map->edgebase) {
    BLO_read_struct(reader, ScrVert, &se->v1);
    BLO_read_struct(reader, ScrVert, &se->v2);
    BKE_screen_sort_scrvert(&se->v1, &se->v2);

    if (se->v1 == nullptr) {
      BLI_remlink(&area_map->edgebase, se);

      return false;
    }
  }

  return true;
}

/**
 * Removes all regions whose type cannot be reconstructed. For example files from new versions may
 * be stored with a newly introduced region type that this version cannot handle.
 */
static void regions_remove_invalid(SpaceType *space_type, ListBase *regionbase)
{
  LISTBASE_FOREACH_MUTABLE (ARegion *, region, regionbase) {
    if (BKE_regiontype_from_id(space_type, region->regiontype) != nullptr) {
      continue;
    }

    CLOG_WARN(&LOG_BLEND_DOVERSION,
              "Region type %d missing in space type \"%s\" (id: %d) - removing region",
              region->regiontype,
              space_type->name,
              space_type->spaceid);

    BKE_area_region_free(space_type, region);
    BLI_freelinkN(regionbase, region);
  }
}

void BKE_screen_area_blend_read_after_liblink(BlendLibReader *reader, ID *parent_id, ScrArea *area)
{
  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    SpaceType *space_type = BKE_spacetype_from_id(sl->spacetype);
    ListBase *regionbase = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;

    /* We cannot restore the region type without a valid space type. So delete all regions to make
     * sure no data is kept around that can't be restored safely (like the type dependent
     * #ARegion.regiondata). */
    if (!space_type) {
      LISTBASE_FOREACH_MUTABLE (ARegion *, region, regionbase) {
        BKE_area_region_free(nullptr, region);
        BLI_freelinkN(regionbase, region);
      }

      continue;
    }

    if (space_type->blend_read_after_liblink) {
      space_type->blend_read_after_liblink(reader, parent_id, sl);
    }

    regions_remove_invalid(space_type, regionbase);
  }
}

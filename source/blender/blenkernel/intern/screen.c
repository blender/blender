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
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"
#include "DNA_defaults.h"

#include "BLI_math_vector.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_screen.h"
#include "BKE_workspace.h"

static void screen_free_data(ID *id)
{
  bScreen *screen = (bScreen *)id;
  ARegion *region;

  /* No animdata here. */

  for (region = screen->regionbase.first; region; region = region->next) {
    BKE_area_region_free(NULL, region);
  }

  BLI_freelistN(&screen->regionbase);

  BKE_screen_area_map_free(AREAMAP_FROM_SCREEN(screen));

  BKE_previewimg_free(&screen->preview);

  /* Region and timer are freed by the window manager. */
  MEM_SAFE_FREE(screen->tool_tip);
}

IDTypeInfo IDType_ID_SCR = {
    .id_code = ID_SCR,
    .id_filter = 0,
    .main_listbase_index = INDEX_ID_SCR,
    .struct_size = sizeof(bScreen),
    .name = "Screen",
    .name_plural = "screens",
    .translation_context = BLT_I18NCONTEXT_ID_SCREEN,
    .flags = IDTYPE_FLAGS_NO_COPY | IDTYPE_FLAGS_NO_MAKELOCAL,

    .init_data = NULL,
    .copy_data = NULL,
    .free_data = screen_free_data,
    .make_local = NULL,
};

/* ************ Spacetype/regiontype handling ************** */

/* keep global; this has to be accessible outside of windowmanager */
static ListBase spacetypes = {NULL, NULL};

/* not SpaceType itself */
static void spacetype_free(SpaceType *st)
{
  ARegionType *art;
  PanelType *pt;
  HeaderType *ht;

  for (art = st->regiontypes.first; art; art = art->next) {
    BLI_freelistN(&art->drawcalls);

    for (pt = art->paneltypes.first; pt; pt = pt->next) {
      if (pt->ext.free) {
        pt->ext.free(pt->ext.data);
      }

      BLI_freelistN(&pt->children);
    }

    for (ht = art->headertypes.first; ht; ht = ht->next) {
      if (ht->ext.free) {
        ht->ext.free(ht->ext.data);
      }
    }

    BLI_freelistN(&art->paneltypes);
    BLI_freelistN(&art->headertypes);
  }

  BLI_freelistN(&st->regiontypes);
}

void BKE_spacetypes_free(void)
{
  SpaceType *st;

  for (st = spacetypes.first; st; st = st->next) {
    spacetype_free(st);
  }

  BLI_freelistN(&spacetypes);
}

SpaceType *BKE_spacetype_from_id(int spaceid)
{
  SpaceType *st;

  for (st = spacetypes.first; st; st = st->next) {
    if (st->spaceid == spaceid) {
      return st;
    }
  }
  return NULL;
}

ARegionType *BKE_regiontype_from_id_or_first(SpaceType *st, int regionid)
{
  ARegionType *art;

  for (art = st->regiontypes.first; art; art = art->next) {
    if (art->regionid == regionid) {
      return art;
    }
  }

  printf(
      "Error, region type %d missing in - name:\"%s\", id:%d\n", regionid, st->name, st->spaceid);
  return st->regiontypes.first;
}

ARegionType *BKE_regiontype_from_id(SpaceType *st, int regionid)
{
  ARegionType *art;

  for (art = st->regiontypes.first; art; art = art->next) {
    if (art->regionid == regionid) {
      return art;
    }
  }
  return NULL;
}

const ListBase *BKE_spacetypes_list(void)
{
  return &spacetypes;
}

void BKE_spacetype_register(SpaceType *st)
{
  SpaceType *stype;

  /* sanity check */
  stype = BKE_spacetype_from_id(st->spaceid);
  if (stype) {
    printf("error: redefinition of spacetype %s\n", stype->name);
    spacetype_free(stype);
    MEM_freeN(stype);
  }

  BLI_addtail(&spacetypes, st);
}

bool BKE_spacetype_exists(int spaceid)
{
  return BKE_spacetype_from_id(spaceid) != NULL;
}

/* ***************** Space handling ********************** */

void BKE_spacedata_freelist(ListBase *lb)
{
  SpaceLink *sl;
  ARegion *region;

  for (sl = lb->first; sl; sl = sl->next) {
    SpaceType *st = BKE_spacetype_from_id(sl->spacetype);

    /* free regions for pushed spaces */
    for (region = sl->regionbase.first; region; region = region->next) {
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
  BLI_duplicatelist(newlb, lb);

  /* copy panel pointers */
  Panel *newpa = newlb->first;
  Panel *pa = lb->first;
  for (; newpa; newpa = newpa->next, pa = pa->next) {
    newpa->activedata = NULL;
    panel_list_copy(&newpa->children, &pa->children);
  }
}

ARegion *BKE_area_region_copy(SpaceType *st, ARegion *region)
{
  ARegion *newar = MEM_dupallocN(region);

  newar->prev = newar->next = NULL;
  BLI_listbase_clear(&newar->handlers);
  BLI_listbase_clear(&newar->uiblocks);
  BLI_listbase_clear(&newar->panels_category);
  BLI_listbase_clear(&newar->panels_category_active);
  BLI_listbase_clear(&newar->ui_lists);
  newar->visible = 0;
  newar->gizmo_map = NULL;
  newar->regiontimer = NULL;
  newar->headerstr = NULL;
  newar->draw_buffer = NULL;

  /* use optional regiondata callback */
  if (region->regiondata) {
    ARegionType *art = BKE_regiontype_from_id(st, region->regiontype);

    if (art && art->duplicate) {
      newar->regiondata = art->duplicate(region->regiondata);
    }
    else if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
      newar->regiondata = NULL;
    }
    else {
      newar->regiondata = MEM_dupallocN(region->regiondata);
    }
  }

  if (region->v2d.tab_offset) {
    newar->v2d.tab_offset = MEM_dupallocN(region->v2d.tab_offset);
  }

  panel_list_copy(&newar->panels, &region->panels);

  BLI_listbase_clear(&newar->ui_previews);
  BLI_duplicatelist(&newar->ui_previews, &region->ui_previews);

  return newar;
}

/* from lb2 to lb1, lb1 is supposed to be freed */
static void region_copylist(SpaceType *st, ListBase *lb1, ListBase *lb2)
{
  ARegion *region;

  /* to be sure */
  BLI_listbase_clear(lb1);

  for (region = lb2->first; region; region = region->next) {
    ARegion *arnew = BKE_area_region_copy(st, region);
    BLI_addtail(lb1, arnew);
  }
}

/* lb1 should be empty */
void BKE_spacedata_copylist(ListBase *lb1, ListBase *lb2)
{
  SpaceLink *sl;

  BLI_listbase_clear(lb1); /* to be sure */

  for (sl = lb2->first; sl; sl = sl->next) {
    SpaceType *st = BKE_spacetype_from_id(sl->spacetype);

    if (st && st->duplicate) {
      SpaceLink *slnew = st->duplicate(sl);

      BLI_addtail(lb1, slnew);

      region_copylist(st, &slnew->regionbase, &sl->regionbase);
    }
  }
}

/* facility to set locks for drawing to survive (render) threads accessing drawing data */
/* lock can become bitflag too */
/* should be replaced in future by better local data handling for threads */
void BKE_spacedata_draw_locks(int set)
{
  SpaceType *st;

  for (st = spacetypes.first; st; st = st->next) {
    ARegionType *art;

    for (art = st->regiontypes.first; art; art = art->next) {
      if (set) {
        art->do_lock = art->lock;
      }
      else {
        art->do_lock = false;
      }
    }
  }
}

/**
 * Version of #BKE_area_find_region_type that also works if \a slink
 * is not the active space of \a sa.
 */
ARegion *BKE_spacedata_find_region_type(const SpaceLink *slink, const ScrArea *sa, int region_type)
{
  const bool is_slink_active = slink == sa->spacedata.first;
  const ListBase *regionbase = (is_slink_active) ? &sa->regionbase : &slink->regionbase;
  ARegion *region = NULL;

  BLI_assert(BLI_findindex(&sa->spacedata, slink) != -1);
  for (region = regionbase->first; region; region = region->next) {
    if (region->regiontype == region_type) {
      break;
    }
  }

  /* Should really unit test this instead. */
  BLI_assert(!is_slink_active || region == BKE_area_find_region_type(sa, region_type));

  return region;
}

static void (*spacedata_id_remap_cb)(struct ScrArea *sa,
                                     struct SpaceLink *sl,
                                     ID *old_id,
                                     ID *new_id) = NULL;

void BKE_spacedata_callback_id_remap_set(void (*func)(ScrArea *sa, SpaceLink *sl, ID *, ID *))
{
  spacedata_id_remap_cb = func;
}

/* UNUSED!!! */
void BKE_spacedata_id_unref(struct ScrArea *sa, struct SpaceLink *sl, struct ID *id)
{
  if (spacedata_id_remap_cb) {
    spacedata_id_remap_cb(sa, sl, id, NULL);
  }
}

/**
 * Avoid bad-level calls to #WM_gizmomap_tag_refresh.
 */
static void (*region_refresh_tag_gizmomap_callback)(struct wmGizmoMap *) = NULL;

void BKE_region_callback_refresh_tag_gizmomap_set(void (*callback)(struct wmGizmoMap *))
{
  region_refresh_tag_gizmomap_callback = callback;
}

void BKE_screen_gizmo_tag_refresh(struct bScreen *sc)
{
  if (region_refresh_tag_gizmomap_callback == NULL) {
    return;
  }

  ScrArea *sa;
  ARegion *region;
  for (sa = sc->areabase.first; sa; sa = sa->next) {
    for (region = sa->regionbase.first; region; region = region->next) {
      if (region->gizmo_map != NULL) {
        region_refresh_tag_gizmomap_callback(region->gizmo_map);
      }
    }
  }
}

/**
 * Avoid bad-level calls to #WM_gizmomap_delete.
 */
static void (*region_free_gizmomap_callback)(struct wmGizmoMap *) = NULL;

void BKE_region_callback_free_gizmomap_set(void (*callback)(struct wmGizmoMap *))
{
  region_free_gizmomap_callback = callback;
}

void BKE_area_region_panels_free(ListBase *lb)
{
  Panel *pa, *pa_next;
  for (pa = lb->first; pa; pa = pa_next) {
    pa_next = pa->next;
    if (pa->activedata) {
      MEM_freeN(pa->activedata);
    }
    BKE_area_region_panels_free(&pa->children);
  }

  BLI_freelistN(lb);
}

/* not region itself */
void BKE_area_region_free(SpaceType *st, ARegion *region)
{
  uiList *uilst;

  if (st) {
    ARegionType *art = BKE_regiontype_from_id(st, region->regiontype);

    if (art && art->free) {
      art->free(region);
    }

    if (region->regiondata) {
      printf("regiondata free error\n");
    }
  }
  else if (region->type && region->type->free) {
    region->type->free(region);
  }

  if (region->v2d.tab_offset) {
    MEM_freeN(region->v2d.tab_offset);
    region->v2d.tab_offset = NULL;
  }

  BKE_area_region_panels_free(&region->panels);

  for (uilst = region->ui_lists.first; uilst; uilst = uilst->next) {
    if (uilst->dyn_data) {
      uiListDyn *dyn_data = uilst->dyn_data;
      if (dyn_data->items_filter_flags) {
        MEM_freeN(dyn_data->items_filter_flags);
      }
      if (dyn_data->items_filter_neworder) {
        MEM_freeN(dyn_data->items_filter_neworder);
      }
      MEM_freeN(dyn_data);
    }
    if (uilst->properties) {
      IDP_FreeProperty(uilst->properties);
    }
  }

  if (region->gizmo_map != NULL) {
    region_free_gizmomap_callback(region->gizmo_map);
  }

  BLI_freelistN(&region->ui_lists);
  BLI_freelistN(&region->ui_previews);
  BLI_freelistN(&region->panels_category);
  BLI_freelistN(&region->panels_category_active);
}

/* not area itself */
void BKE_screen_area_free(ScrArea *sa)
{
  SpaceType *st = BKE_spacetype_from_id(sa->spacetype);
  ARegion *region;

  for (region = sa->regionbase.first; region; region = region->next) {
    BKE_area_region_free(st, region);
  }

  MEM_SAFE_FREE(sa->global);
  BLI_freelistN(&sa->regionbase);

  BKE_spacedata_freelist(&sa->spacedata);

  BLI_freelistN(&sa->actionzones);
}

void BKE_screen_area_map_free(ScrAreaMap *area_map)
{
  for (ScrArea *area = area_map->areabase.first, *area_next; area; area = area_next) {
    area_next = area->next;
    BKE_screen_area_free(area);
  }

  BLI_freelistN(&area_map->vertbase);
  BLI_freelistN(&area_map->edgebase);
  BLI_freelistN(&area_map->areabase);
}

/** Free (or release) any data used by this screen (does not free the screen itself). */
void BKE_screen_free(bScreen *sc)
{
  screen_free_data(&sc->id);
}

/* ***************** Screen edges & verts ***************** */

ScrEdge *BKE_screen_find_edge(bScreen *sc, ScrVert *v1, ScrVert *v2)
{
  ScrEdge *se;

  BKE_screen_sort_scrvert(&v1, &v2);
  for (se = sc->edgebase.first; se; se = se->next) {
    if (se->v1 == v1 && se->v2 == v2) {
      return se;
    }
  }

  return NULL;
}

void BKE_screen_sort_scrvert(ScrVert **v1, ScrVert **v2)
{
  ScrVert *tmp;

  if (*v1 > *v2) {
    tmp = *v1;
    *v1 = *v2;
    *v2 = tmp;
  }
}

void BKE_screen_remove_double_scrverts(bScreen *sc)
{
  ScrVert *v1, *verg;
  ScrEdge *se;
  ScrArea *sa;

  verg = sc->vertbase.first;
  while (verg) {
    if (verg->newv == NULL) { /* !!! */
      v1 = verg->next;
      while (v1) {
        if (v1->newv == NULL) { /* !?! */
          if (v1->vec.x == verg->vec.x && v1->vec.y == verg->vec.y) {
            /* printf("doublevert\n"); */
            v1->newv = verg;
          }
        }
        v1 = v1->next;
      }
    }
    verg = verg->next;
  }

  /* replace pointers in edges and faces */
  se = sc->edgebase.first;
  while (se) {
    if (se->v1->newv) {
      se->v1 = se->v1->newv;
    }
    if (se->v2->newv) {
      se->v2 = se->v2->newv;
    }
    /* edges changed: so.... */
    BKE_screen_sort_scrvert(&(se->v1), &(se->v2));
    se = se->next;
  }
  sa = sc->areabase.first;
  while (sa) {
    if (sa->v1->newv) {
      sa->v1 = sa->v1->newv;
    }
    if (sa->v2->newv) {
      sa->v2 = sa->v2->newv;
    }
    if (sa->v3->newv) {
      sa->v3 = sa->v3->newv;
    }
    if (sa->v4->newv) {
      sa->v4 = sa->v4->newv;
    }
    sa = sa->next;
  }

  /* remove */
  verg = sc->vertbase.first;
  while (verg) {
    v1 = verg->next;
    if (verg->newv) {
      BLI_remlink(&sc->vertbase, verg);
      MEM_freeN(verg);
    }
    verg = v1;
  }
}

void BKE_screen_remove_double_scredges(bScreen *sc)
{
  ScrEdge *verg, *se, *sn;

  /* compare */
  verg = sc->edgebase.first;
  while (verg) {
    se = verg->next;
    while (se) {
      sn = se->next;
      if (verg->v1 == se->v1 && verg->v2 == se->v2) {
        BLI_remlink(&sc->edgebase, se);
        MEM_freeN(se);
      }
      se = sn;
    }
    verg = verg->next;
  }
}

void BKE_screen_remove_unused_scredges(bScreen *sc)
{
  ScrEdge *se, *sen;
  ScrArea *sa;
  int a = 0;

  /* sets flags when edge is used in area */
  sa = sc->areabase.first;
  while (sa) {
    se = BKE_screen_find_edge(sc, sa->v1, sa->v2);
    if (se == NULL) {
      printf("error: area %d edge 1 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(sc, sa->v2, sa->v3);
    if (se == NULL) {
      printf("error: area %d edge 2 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(sc, sa->v3, sa->v4);
    if (se == NULL) {
      printf("error: area %d edge 3 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(sc, sa->v4, sa->v1);
    if (se == NULL) {
      printf("error: area %d edge 4 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    sa = sa->next;
    a++;
  }
  se = sc->edgebase.first;
  while (se) {
    sen = se->next;
    if (se->flag == 0) {
      BLI_remlink(&sc->edgebase, se);
      MEM_freeN(se);
    }
    else {
      se->flag = 0;
    }
    se = sen;
  }
}

void BKE_screen_remove_unused_scrverts(bScreen *sc)
{
  ScrVert *sv, *svn;
  ScrEdge *se;

  /* we assume edges are ok */

  se = sc->edgebase.first;
  while (se) {
    se->v1->flag = 1;
    se->v2->flag = 1;
    se = se->next;
  }

  sv = sc->vertbase.first;
  while (sv) {
    svn = sv->next;
    if (sv->flag == 0) {
      BLI_remlink(&sc->vertbase, sv);
      MEM_freeN(sv);
    }
    else {
      sv->flag = 0;
    }
    sv = svn;
  }
}

/* ***************** Utilities ********************** */

/**
 * Find a region of type \a region_type in the currently active space of \a sa.
 *
 * \note This does _not_ work if the region to look up is not in the active
 *       space. Use #BKE_spacedata_find_region_type if that may be the case.
 */
ARegion *BKE_area_find_region_type(const ScrArea *sa, int region_type)
{
  if (sa) {
    for (ARegion *region = sa->regionbase.first; region; region = region->next) {
      if (region->regiontype == region_type) {
        return region;
      }
    }
  }

  return NULL;
}

ARegion *BKE_area_find_region_active_win(ScrArea *sa)
{
  if (sa) {
    ARegion *region = BLI_findlink(&sa->regionbase, sa->region_active_win);
    if (region && (region->regiontype == RGN_TYPE_WINDOW)) {
      return region;
    }

    /* fallback to any */
    return BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
  }
  return NULL;
}

ARegion *BKE_area_find_region_xy(ScrArea *sa, const int regiontype, int x, int y)
{
  ARegion *ar_found = NULL;
  if (sa) {
    ARegion *region;
    for (region = sa->regionbase.first; region; region = region->next) {
      if ((regiontype == RGN_TYPE_ANY) || (region->regiontype == regiontype)) {
        if (BLI_rcti_isect_pt(&region->winrct, x, y)) {
          ar_found = region;
          break;
        }
      }
    }
  }
  return ar_found;
}

/**
 * \note This is only for screen level regions (typically menus/popups).
 */
ARegion *BKE_screen_find_region_xy(bScreen *sc, const int regiontype, int x, int y)
{
  ARegion *ar_found = NULL;
  for (ARegion *region = sc->regionbase.first; region; region = region->next) {
    if ((regiontype == RGN_TYPE_ANY) || (region->regiontype == regiontype)) {
      if (BLI_rcti_isect_pt(&region->winrct, x, y)) {
        ar_found = region;
        break;
      }
    }
  }
  return ar_found;
}

/**
 * \note Ideally we can get the area from the context,
 * there are a few places however where this isn't practical.
 */
ScrArea *BKE_screen_find_area_from_space(struct bScreen *sc, SpaceLink *sl)
{
  ScrArea *sa;

  for (sa = sc->areabase.first; sa; sa = sa->next) {
    if (BLI_findindex(&sa->spacedata, sl) != -1) {
      break;
    }
  }

  return sa;
}

/**
 * \note Using this function is generally a last resort, you really want to be
 * using the context when you can - campbell
 */
ScrArea *BKE_screen_find_big_area(bScreen *sc, const int spacetype, const short min)
{
  ScrArea *sa, *big = NULL;
  int size, maxsize = 0;

  for (sa = sc->areabase.first; sa; sa = sa->next) {
    if ((spacetype == SPACE_TYPE_ANY) || (sa->spacetype == spacetype)) {
      if (min <= sa->winx && min <= sa->winy) {
        size = sa->winx * sa->winy;
        if (size > maxsize) {
          maxsize = size;
          big = sa;
        }
      }
    }
  }

  return big;
}

ScrArea *BKE_screen_area_map_find_area_xy(const ScrAreaMap *areamap,
                                          const int spacetype,
                                          int x,
                                          int y)
{
  for (ScrArea *sa = areamap->areabase.first; sa; sa = sa->next) {
    if (BLI_rcti_isect_pt(&sa->totrct, x, y)) {
      if ((spacetype == SPACE_TYPE_ANY) || (sa->spacetype == spacetype)) {
        return sa;
      }
      break;
    }
  }
  return NULL;
}
ScrArea *BKE_screen_find_area_xy(bScreen *sc, const int spacetype, int x, int y)
{
  return BKE_screen_area_map_find_area_xy(AREAMAP_FROM_SCREEN(sc), spacetype, x, y);
}

void BKE_screen_view3d_sync(View3D *v3d, struct Scene *scene)
{
  if (v3d->scenelock && v3d->localvd == NULL) {
    v3d->camera = scene->camera;

    if (v3d->camera == NULL) {
      ARegion *region;

      for (region = v3d->regionbase.first; region; region = region->next) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          RegionView3D *rv3d = region->regiondata;
          if (rv3d->persp == RV3D_CAMOB) {
            rv3d->persp = RV3D_PERSP;
          }
        }
      }
    }
  }
}

void BKE_screen_view3d_scene_sync(bScreen *sc, Scene *scene)
{
  /* are there cameras in the views that are not in the scene? */
  ScrArea *sa;
  for (sa = sc->areabase.first; sa; sa = sa->next) {
    SpaceLink *sl;
    for (sl = sa->spacedata.first; sl; sl = sl->next) {
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

/* magic zoom calculation, no idea what
 * it signifies, if you find out, tell me! -zr
 */

/* simple, its magic dude!
 * well, to be honest, this gives a natural feeling zooming
 * with multiple keypad presses (ton)
 */
float BKE_screen_view3d_zoom_to_fac(float camzoom)
{
  return powf(((float)M_SQRT2 + camzoom / 50.0f), 2.0f) / 4.0f;
}

float BKE_screen_view3d_zoom_from_fac(float zoomfac)
{
  return ((sqrtf(4.0f * zoomfac) - (float)M_SQRT2) * 50.0f);
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
  for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
    for (ARegion *region = sa->regionbase.first; region; region = region->next) {
      if (ELEM(region->regiontype, RGN_TYPE_HEADER, RGN_TYPE_TOOL_HEADER)) {
        if (ELEM(sa->spacetype, SPACE_FILE, SPACE_USERPREF, SPACE_OUTLINER, SPACE_PROPERTIES)) {
          region->alignment = RGN_ALIGN_TOP;
          continue;
        }
        region->alignment = alignment;
      }
      if (region->regiontype == RGN_TYPE_FOOTER) {
        if (ELEM(sa->spacetype, SPACE_FILE, SPACE_USERPREF, SPACE_OUTLINER, SPACE_PROPERTIES)) {
          region->alignment = RGN_ALIGN_BOTTOM;
          continue;
        }
        region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_TOP : RGN_ALIGN_BOTTOM;
      }
    }
  }
  screen->do_refresh = true;
}

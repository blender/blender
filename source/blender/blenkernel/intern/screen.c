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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_defaults.h"
#include "DNA_gpencil_types.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_mempool.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_query.h"
#include "BKE_node.h"
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

static void screen_foreach_id_dopesheet(LibraryForeachIDData *data, bDopeSheet *ads)
{
  if (ads != NULL) {
    BKE_LIB_FOREACHID_PROCESS_ID(data, ads->source, IDWALK_CB_NOP);
    BKE_LIB_FOREACHID_PROCESS(data, ads->filter_grp, IDWALK_CB_NOP);
  }
}

void BKE_screen_foreach_id_screen_area(LibraryForeachIDData *data, ScrArea *area)
{
  BKE_LIB_FOREACHID_PROCESS(data, area->full, IDWALK_CB_NOP);

  /* TODO this should be moved to a callback in `SpaceType`, defined in each editor's own code.
   * Will be for a later round of cleanup though... */
  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    switch (sl->spacetype) {
      case SPACE_VIEW3D: {
        View3D *v3d = (View3D *)sl;

        BKE_LIB_FOREACHID_PROCESS(data, v3d->camera, IDWALK_CB_NOP);
        BKE_LIB_FOREACHID_PROCESS(data, v3d->ob_center, IDWALK_CB_NOP);

        if (v3d->localvd) {
          BKE_LIB_FOREACHID_PROCESS(data, v3d->localvd->camera, IDWALK_CB_NOP);
        }
        break;
      }
      case SPACE_GRAPH: {
        SpaceGraph *sipo = (SpaceGraph *)sl;

        screen_foreach_id_dopesheet(data, sipo->ads);
        break;
      }
      case SPACE_PROPERTIES: {
        SpaceProperties *sbuts = (SpaceProperties *)sl;

        BKE_LIB_FOREACHID_PROCESS_ID(data, sbuts->pinid, IDWALK_CB_NOP);
        break;
      }
      case SPACE_FILE:
        break;
      case SPACE_ACTION: {
        SpaceAction *saction = (SpaceAction *)sl;

        screen_foreach_id_dopesheet(data, &saction->ads);
        BKE_LIB_FOREACHID_PROCESS(data, saction->action, IDWALK_CB_NOP);
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = (SpaceImage *)sl;

        BKE_LIB_FOREACHID_PROCESS(data, sima->image, IDWALK_CB_USER_ONE);
        BKE_LIB_FOREACHID_PROCESS(data, sima->mask_info.mask, IDWALK_CB_USER_ONE);
        BKE_LIB_FOREACHID_PROCESS(data, sima->gpd, IDWALK_CB_USER);
        break;
      }
      case SPACE_SEQ: {
        SpaceSeq *sseq = (SpaceSeq *)sl;

        BKE_LIB_FOREACHID_PROCESS(data, sseq->gpd, IDWALK_CB_USER);
        break;
      }
      case SPACE_NLA: {
        SpaceNla *snla = (SpaceNla *)sl;

        screen_foreach_id_dopesheet(data, snla->ads);
        break;
      }
      case SPACE_TEXT: {
        SpaceText *st = (SpaceText *)sl;

        BKE_LIB_FOREACHID_PROCESS(data, st->text, IDWALK_CB_NOP);
        break;
      }
      case SPACE_SCRIPT: {
        SpaceScript *scpt = (SpaceScript *)sl;

        BKE_LIB_FOREACHID_PROCESS(data, scpt->script, IDWALK_CB_NOP);
        break;
      }
      case SPACE_OUTLINER: {
        SpaceOutliner *space_outliner = (SpaceOutliner *)sl;

        BKE_LIB_FOREACHID_PROCESS_ID(data, space_outliner->search_tse.id, IDWALK_CB_NOP);

        if (space_outliner->treestore != NULL) {
          TreeStoreElem *tselem;
          BLI_mempool_iter iter;

          BLI_mempool_iternew(space_outliner->treestore, &iter);
          while ((tselem = BLI_mempool_iterstep(&iter))) {
            BKE_LIB_FOREACHID_PROCESS_ID(data, tselem->id, IDWALK_CB_NOP);
          }
        }
        break;
      }
      case SPACE_NODE: {
        SpaceNode *snode = (SpaceNode *)sl;

        const bool is_private_nodetree = snode->id != NULL &&
                                         ntreeFromID(snode->id) == snode->nodetree;

        BKE_LIB_FOREACHID_PROCESS_ID(data, snode->id, IDWALK_CB_NOP);
        BKE_LIB_FOREACHID_PROCESS_ID(data, snode->from, IDWALK_CB_NOP);

        BKE_LIB_FOREACHID_PROCESS(
            data, snode->nodetree, is_private_nodetree ? IDWALK_CB_EMBEDDED : IDWALK_CB_USER_ONE);

        LISTBASE_FOREACH (bNodeTreePath *, path, &snode->treepath) {
          if (path == snode->treepath.first) {
            /* first nodetree in path is same as snode->nodetree */
            BKE_LIB_FOREACHID_PROCESS(data,
                                      path->nodetree,
                                      is_private_nodetree ? IDWALK_CB_EMBEDDED :
                                                            IDWALK_CB_USER_ONE);
          }
          else {
            BKE_LIB_FOREACHID_PROCESS(data, path->nodetree, IDWALK_CB_USER_ONE);
          }

          if (path->nodetree == NULL) {
            break;
          }
        }

        BKE_LIB_FOREACHID_PROCESS(data, snode->edittree, IDWALK_CB_NOP);
        break;
      }
      case SPACE_CLIP: {
        SpaceClip *sclip = (SpaceClip *)sl;

        BKE_LIB_FOREACHID_PROCESS(data, sclip->clip, IDWALK_CB_USER_ONE);
        BKE_LIB_FOREACHID_PROCESS(data, sclip->mask_info.mask, IDWALK_CB_USER_ONE);
        break;
      }
      default:
        break;
    }
  }
}

static void screen_foreach_id(ID *id, LibraryForeachIDData *data)
{
  if (BKE_lib_query_foreachid_process_flags_get(data) & IDWALK_INCLUDE_UI) {
    bScreen *screen = (bScreen *)id;

    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      BKE_screen_foreach_id_screen_area(data, area);
    }
  }
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
    .foreach_id = screen_foreach_id,
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
      if (pt->rna_ext.free) {
        pt->rna_ext.free(pt->rna_ext.data);
      }

      BLI_freelistN(&pt->children);
    }

    for (ht = art->headertypes.first; ht; ht = ht->next) {
      if (ht->rna_ext.free) {
        ht->rna_ext.free(ht->rna_ext.data);
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
  Panel *new_panel = newlb->first;
  Panel *panel = lb->first;
  for (; new_panel; new_panel = new_panel->next, panel = panel->next) {
    new_panel->activedata = NULL;
    new_panel->runtime.custom_data_ptr = NULL;
    panel_list_copy(&new_panel->children, &panel->children);
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
 * is not the active space of \a area.
 */
ARegion *BKE_spacedata_find_region_type(const SpaceLink *slink,
                                        const ScrArea *area,
                                        int region_type)
{
  const bool is_slink_active = slink == area->spacedata.first;
  const ListBase *regionbase = (is_slink_active) ? &area->regionbase : &slink->regionbase;
  ARegion *region = NULL;

  BLI_assert(BLI_findindex(&area->spacedata, slink) != -1);
  for (region = regionbase->first; region; region = region->next) {
    if (region->regiontype == region_type) {
      break;
    }
  }

  /* Should really unit test this instead. */
  BLI_assert(!is_slink_active || region == BKE_area_find_region_type(area, region_type));

  return region;
}

static void (*spacedata_id_remap_cb)(struct ScrArea *area,
                                     struct SpaceLink *sl,
                                     ID *old_id,
                                     ID *new_id) = NULL;

void BKE_spacedata_callback_id_remap_set(void (*func)(ScrArea *area, SpaceLink *sl, ID *, ID *))
{
  spacedata_id_remap_cb = func;
}

/* UNUSED!!! */
void BKE_spacedata_id_unref(struct ScrArea *area, struct SpaceLink *sl, struct ID *id)
{
  if (spacedata_id_remap_cb) {
    spacedata_id_remap_cb(area, sl, id, NULL);
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

void BKE_screen_gizmo_tag_refresh(struct bScreen *screen)
{
  if (region_refresh_tag_gizmomap_callback == NULL) {
    return;
  }

  ScrArea *area;
  ARegion *region;
  for (area = screen->areabase.first; area; area = area->next) {
    for (region = area->regionbase.first; region; region = region->next) {
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

static void area_region_panels_free_recursive(Panel *panel)
{
  MEM_SAFE_FREE(panel->activedata);

  LISTBASE_FOREACH_MUTABLE (Panel *, child_panel, &panel->children) {
    area_region_panels_free_recursive(child_panel);
  }

  MEM_freeN(panel);
}

void BKE_area_region_panels_free(ListBase *lb)
{
  LISTBASE_FOREACH_MUTABLE (Panel *, panel, lb) {
    /* Free custom data just for parent panels to avoid a double free. */
    MEM_SAFE_FREE(panel->runtime.custom_data_ptr);
    area_region_panels_free_recursive(panel);
  }
  BLI_listbase_clear(lb);
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
void BKE_screen_area_free(ScrArea *area)
{
  SpaceType *st = BKE_spacetype_from_id(area->spacetype);
  ARegion *region;

  for (region = area->regionbase.first; region; region = region->next) {
    BKE_area_region_free(st, region);
  }

  MEM_SAFE_FREE(area->global);
  BLI_freelistN(&area->regionbase);

  BKE_spacedata_freelist(&area->spacedata);

  BLI_freelistN(&area->actionzones);
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
void BKE_screen_free(bScreen *screen)
{
  screen_free_data(&screen->id);
}

/* ***************** Screen edges & verts ***************** */

ScrEdge *BKE_screen_find_edge(bScreen *screen, ScrVert *v1, ScrVert *v2)
{
  ScrEdge *se;

  BKE_screen_sort_scrvert(&v1, &v2);
  for (se = screen->edgebase.first; se; se = se->next) {
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

void BKE_screen_remove_double_scrverts(bScreen *screen)
{
  ScrVert *v1, *verg;
  ScrEdge *se;
  ScrArea *area;

  verg = screen->vertbase.first;
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
  se = screen->edgebase.first;
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
  area = screen->areabase.first;
  while (area) {
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
    area = area->next;
  }

  /* remove */
  verg = screen->vertbase.first;
  while (verg) {
    v1 = verg->next;
    if (verg->newv) {
      BLI_remlink(&screen->vertbase, verg);
      MEM_freeN(verg);
    }
    verg = v1;
  }
}

void BKE_screen_remove_double_scredges(bScreen *screen)
{
  ScrEdge *verg, *se, *sn;

  /* compare */
  verg = screen->edgebase.first;
  while (verg) {
    se = verg->next;
    while (se) {
      sn = se->next;
      if (verg->v1 == se->v1 && verg->v2 == se->v2) {
        BLI_remlink(&screen->edgebase, se);
        MEM_freeN(se);
      }
      se = sn;
    }
    verg = verg->next;
  }
}

void BKE_screen_remove_unused_scredges(bScreen *screen)
{
  ScrEdge *se, *sen;
  ScrArea *area;
  int a = 0;

  /* sets flags when edge is used in area */
  area = screen->areabase.first;
  while (area) {
    se = BKE_screen_find_edge(screen, area->v1, area->v2);
    if (se == NULL) {
      printf("error: area %d edge 1 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area->v2, area->v3);
    if (se == NULL) {
      printf("error: area %d edge 2 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area->v3, area->v4);
    if (se == NULL) {
      printf("error: area %d edge 3 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    se = BKE_screen_find_edge(screen, area->v4, area->v1);
    if (se == NULL) {
      printf("error: area %d edge 4 doesn't exist\n", a);
    }
    else {
      se->flag = 1;
    }
    area = area->next;
    a++;
  }
  se = screen->edgebase.first;
  while (se) {
    sen = se->next;
    if (se->flag == 0) {
      BLI_remlink(&screen->edgebase, se);
      MEM_freeN(se);
    }
    else {
      se->flag = 0;
    }
    se = sen;
  }
}

void BKE_screen_remove_unused_scrverts(bScreen *screen)
{
  ScrVert *sv, *svn;
  ScrEdge *se;

  /* we assume edges are ok */

  se = screen->edgebase.first;
  while (se) {
    se->v1->flag = 1;
    se->v2->flag = 1;
    se = se->next;
  }

  sv = screen->vertbase.first;
  while (sv) {
    svn = sv->next;
    if (sv->flag == 0) {
      BLI_remlink(&screen->vertbase, sv);
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
 * Find a region of type \a region_type in the currently active space of \a area.
 *
 * \note This does _not_ work if the region to look up is not in the active
 *       space. Use #BKE_spacedata_find_region_type if that may be the case.
 */
ARegion *BKE_area_find_region_type(const ScrArea *area, int region_type)
{
  if (area) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region->regiontype == region_type) {
        return region;
      }
    }
  }

  return NULL;
}

ARegion *BKE_area_find_region_active_win(ScrArea *area)
{
  if (area) {
    ARegion *region = BLI_findlink(&area->regionbase, area->region_active_win);
    if (region && (region->regiontype == RGN_TYPE_WINDOW)) {
      return region;
    }

    /* fallback to any */
    return BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  }
  return NULL;
}

ARegion *BKE_area_find_region_xy(ScrArea *area, const int regiontype, int x, int y)
{
  ARegion *region_found = NULL;
  if (area) {
    ARegion *region;
    for (region = area->regionbase.first; region; region = region->next) {
      if ((regiontype == RGN_TYPE_ANY) || (region->regiontype == regiontype)) {
        if (BLI_rcti_isect_pt(&region->winrct, x, y)) {
          region_found = region;
          break;
        }
      }
    }
  }
  return region_found;
}

/**
 * \note This is only for screen level regions (typically menus/popups).
 */
ARegion *BKE_screen_find_region_xy(bScreen *screen, const int regiontype, int x, int y)
{
  ARegion *region_found = NULL;
  LISTBASE_FOREACH (ARegion *, region, &screen->regionbase) {
    if ((regiontype == RGN_TYPE_ANY) || (region->regiontype == regiontype)) {
      if (BLI_rcti_isect_pt(&region->winrct, x, y)) {
        region_found = region;
        break;
      }
    }
  }
  return region_found;
}

/**
 * \note Ideally we can get the area from the context,
 * there are a few places however where this isn't practical.
 */
ScrArea *BKE_screen_find_area_from_space(struct bScreen *screen, SpaceLink *sl)
{
  ScrArea *area;

  for (area = screen->areabase.first; area; area = area->next) {
    if (BLI_findindex(&area->spacedata, sl) != -1) {
      break;
    }
  }

  return area;
}

/**
 * \note Using this function is generally a last resort, you really want to be
 * using the context when you can - campbell
 */
ScrArea *BKE_screen_find_big_area(bScreen *screen, const int spacetype, const short min)
{
  ScrArea *area, *big = NULL;
  int size, maxsize = 0;

  for (area = screen->areabase.first; area; area = area->next) {
    if ((spacetype == SPACE_TYPE_ANY) || (area->spacetype == spacetype)) {
      if (min <= area->winx && min <= area->winy) {
        size = area->winx * area->winy;
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
                                          int x,
                                          int y)
{
  LISTBASE_FOREACH (ScrArea *, area, &areamap->areabase) {
    if (BLI_rcti_isect_pt(&area->totrct, x, y)) {
      if ((spacetype == SPACE_TYPE_ANY) || (area->spacetype == spacetype)) {
        return area;
      }
      break;
    }
  }
  return NULL;
}
ScrArea *BKE_screen_find_area_xy(bScreen *screen, const int spacetype, int x, int y)
{
  return BKE_screen_area_map_find_area_xy(AREAMAP_FROM_SCREEN(screen), spacetype, x, y);
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

void BKE_screen_view3d_scene_sync(bScreen *screen, Scene *scene)
{
  /* are there cameras in the views that are not in the scene? */
  ScrArea *area;
  for (area = screen->areabase.first; area; area = area->next) {
    SpaceLink *sl;
    for (sl = area->spacedata.first; sl; sl = sl->next) {
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

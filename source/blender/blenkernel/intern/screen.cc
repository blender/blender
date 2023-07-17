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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_collection_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mask_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_mempool.h"
#include "BLI_rect.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_gpencil_legacy.h"
#include "BKE_icons.h"
#include "BKE_idprop.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_node.h"
#include "BKE_screen.h"
#include "BKE_viewer_path.h"
#include "BKE_workspace.h"

#include "BLO_read_write.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.h"
#endif

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
  MEM_SAFE_FREE(screen->tool_tip);
}

static void screen_foreach_id_dopesheet(LibraryForeachIDData *data, bDopeSheet *ads)
{
  if (ads != nullptr) {
    BKE_LIB_FOREACHID_PROCESS_ID(data, ads->source, IDWALK_CB_NOP);
    BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, ads->filter_grp, IDWALK_CB_NOP);
  }
}

void BKE_screen_foreach_id_screen_area(LibraryForeachIDData *data, ScrArea *area)
{
  const int data_flags = BKE_lib_query_foreachid_process_flags_get(data);
  const bool is_readonly = (data_flags & IDWALK_READONLY) != 0;
  const bool allow_pointer_access = (data_flags & IDWALK_NO_ORIG_POINTERS_ACCESS) == 0;

  BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, area->full, IDWALK_CB_NOP);

  /* TODO: this should be moved to a callback in `SpaceType`, defined in each editor's own code.
   * Will be for a later round of cleanup though... */
  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    switch (sl->spacetype) {
      case SPACE_VIEW3D: {
        View3D *v3d = (View3D *)sl;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, v3d->camera, IDWALK_CB_NOP);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, v3d->ob_center, IDWALK_CB_NOP);
        if (v3d->localvd) {
          BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, v3d->localvd->camera, IDWALK_CB_NOP);
        }
        BKE_viewer_path_foreach_id(data, &v3d->viewer_path);
        break;
      }
      case SPACE_GRAPH: {
        SpaceGraph *sipo = (SpaceGraph *)sl;
        BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                                screen_foreach_id_dopesheet(data, sipo->ads));

        if (!is_readonly) {
          /* Force recalc of list of channels (i.e. including calculating F-Curve colors) to
           * prevent the "black curves" problem post-undo. */
          sipo->runtime.flag |= SIPO_RUNTIME_FLAG_NEED_CHAN_SYNC_COLOR;
        }
        break;
      }
      case SPACE_PROPERTIES: {
        SpaceProperties *sbuts = (SpaceProperties *)sl;
        BKE_LIB_FOREACHID_PROCESS_ID(data, sbuts->pinid, IDWALK_CB_NOP);
        if (!is_readonly) {
          if (sbuts->pinid == nullptr) {
            sbuts->flag &= ~SB_PIN_CONTEXT;
          }
          /* Note: Restoring path pointers is complicated, if not impossible, because this contains
           * data pointers too, not just ID ones. See #40046. */
          MEM_SAFE_FREE(sbuts->path);
        }
        break;
      }
      case SPACE_FILE: {
        if (!is_readonly) {
          SpaceFile *sfile = (SpaceFile *)sl;
          sfile->op = nullptr;
          sfile->tags = FILE_TAG_REBUILD_MAIN_FILES;
        }
        break;
      }
      case SPACE_ACTION: {
        SpaceAction *saction = (SpaceAction *)sl;
        screen_foreach_id_dopesheet(data, &saction->ads);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, saction->action, IDWALK_CB_NOP);
        if (!is_readonly) {
          /* Force recalc of list of channels, potentially updating the active action while we're
           * at it (as it can only be updated that way) #28962. */
          saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;
        }
        break;
      }
      case SPACE_IMAGE: {
        SpaceImage *sima = (SpaceImage *)sl;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sima->image, IDWALK_CB_USER_ONE);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sima->iuser.scene, IDWALK_CB_NOP);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sima->mask_info.mask, IDWALK_CB_USER_ONE);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sima->gpd, IDWALK_CB_USER);
        if (!is_readonly) {
          sima->scopes.ok = 0;
        }
        break;
      }
      case SPACE_SEQ: {
        SpaceSeq *sseq = (SpaceSeq *)sl;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sseq->gpd, IDWALK_CB_USER);
        break;
      }
      case SPACE_NLA: {
        SpaceNla *snla = (SpaceNla *)sl;
        BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data,
                                                screen_foreach_id_dopesheet(data, snla->ads));
        break;
      }
      case SPACE_TEXT: {
        SpaceText *st = (SpaceText *)sl;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, st->text, IDWALK_CB_NOP);
        break;
      }
      case SPACE_SCRIPT: {
        SpaceScript *scpt = (SpaceScript *)sl;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, scpt->script, IDWALK_CB_NOP);
        break;
      }
      case SPACE_OUTLINER: {
        SpaceOutliner *space_outliner = (SpaceOutliner *)sl;
        if (space_outliner->treestore != nullptr) {
          TreeStoreElem *tselem;
          BLI_mempool_iter iter;

          BLI_mempool_iternew(space_outliner->treestore, &iter);
          while ((tselem = static_cast<TreeStoreElem *>(BLI_mempool_iterstep(&iter)))) {
            /* Do not try to restore non-ID pointers (drivers/sequence/etc.). */
            if (TSE_IS_REAL_ID(tselem)) {
              const int cb_flag = (tselem->id != nullptr && allow_pointer_access &&
                                   (tselem->id->flag & LIB_EMBEDDED_DATA) != 0) ?
                                      IDWALK_CB_EMBEDDED_NOT_OWNING :
                                      IDWALK_CB_NOP;
              BKE_LIB_FOREACHID_PROCESS_ID(data, tselem->id, cb_flag);
            }
            else if (!is_readonly) {
              tselem->id = nullptr;
            }
          }
          if (!is_readonly) {
            /* rebuild hash table, because it depends on ids too */
            space_outliner->storeflag |= SO_TREESTORE_REBUILD;
          }
        }
        break;
      }
      case SPACE_NODE: {
        SpaceNode *snode = (SpaceNode *)sl;
        const bool is_embedded_nodetree = snode->id != nullptr && allow_pointer_access &&
                                          ntreeFromID(snode->id) == snode->nodetree;

        BKE_LIB_FOREACHID_PROCESS_ID(data, snode->id, IDWALK_CB_NOP);
        BKE_LIB_FOREACHID_PROCESS_ID(data, snode->from, IDWALK_CB_NOP);

        bNodeTreePath *path = static_cast<bNodeTreePath *>(snode->treepath.first);
        BLI_assert(path == nullptr || path->nodetree == snode->nodetree);

        if (is_embedded_nodetree) {
          BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, snode->nodetree, IDWALK_CB_EMBEDDED_NOT_OWNING);
          if (path != nullptr) {
            BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, path->nodetree, IDWALK_CB_EMBEDDED_NOT_OWNING);
          }

          /* Embedded ID pointers are not remapped (besides exceptions), ensure it still matches
           * actual data. Note that `snode->id` was already processed (and therefore potentially
           * remapped) above. */
          if (!is_readonly) {
            snode->nodetree = (snode->id == nullptr) ? nullptr : ntreeFromID(snode->id);
            if (path != nullptr) {
              path->nodetree = snode->nodetree;
            }
          }
        }
        else {
          BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, snode->nodetree, IDWALK_CB_USER_ONE);
          if (path != nullptr) {
            BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, path->nodetree, IDWALK_CB_USER_ONE);
          }
        }

        /* Both `snode->id` and `snode->nodetree` have been remapped now, so their data can be
         * accessed. */
        BLI_assert(snode->id == nullptr || snode->nodetree == nullptr ||
                   (snode->nodetree->id.flag & LIB_EMBEDDED_DATA) == 0 ||
                   snode->nodetree == ntreeFromID(snode->id));

        if (path != nullptr) {
          for (path = path->next; path != nullptr; path = path->next) {
            BLI_assert(path->nodetree != nullptr);
            if ((data_flags & IDWALK_NO_ORIG_POINTERS_ACCESS) == 0) {
              BLI_assert((path->nodetree->id.flag & LIB_EMBEDDED_DATA) == 0);
            }

            BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, path->nodetree, IDWALK_CB_USER_ONE);

            if (path->nodetree == nullptr) {
              BLI_assert(!is_readonly);
              /* Remaining path entries are invalid, remove them. */
              for (bNodeTreePath *path_next; path; path = path_next) {
                path_next = path->next;
                BLI_remlink(&snode->treepath, path);
                MEM_freeN(path);
              }
              break;
            }
          }
        }
        BLI_assert(path == nullptr);

        if (!is_readonly) {
          /* `edittree` is just the last in the path, set this directly since the path may have
           * been shortened above. */
          if (snode->treepath.last != nullptr) {
            path = static_cast<bNodeTreePath *>(snode->treepath.last);
            snode->edittree = path->nodetree;
          }
          else {
            snode->edittree = nullptr;
          }
        }
        else {
          /* Only process this pointer in readonly case, otherwise could lead to a bad
           * double-remapping e.g. */
          if (is_embedded_nodetree && snode->edittree == snode->nodetree) {
            BKE_LIB_FOREACHID_PROCESS_IDSUPER(
                data, snode->edittree, IDWALK_CB_EMBEDDED_NOT_OWNING);
          }
          else {
            BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, snode->edittree, IDWALK_CB_NOP);
          }
        }
        break;
      }
      case SPACE_CLIP: {
        SpaceClip *sclip = (SpaceClip *)sl;
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sclip->clip, IDWALK_CB_USER_ONE);
        BKE_LIB_FOREACHID_PROCESS_IDSUPER(data, sclip->mask_info.mask, IDWALK_CB_USER_ONE);

        if (!is_readonly) {
          sclip->scopes.ok = 0;
        }
        break;
      }
      case SPACE_SPREADSHEET: {
        SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)sl;
        BKE_viewer_path_foreach_id(data, &sspreadsheet->viewer_path);
        break;
      }
      default:
        break;
    }
  }
}

static void screen_foreach_id(ID *id, LibraryForeachIDData *data)
{
  if ((BKE_lib_query_foreachid_process_flags_get(data) & IDWALK_INCLUDE_UI) == 0) {
    return;
  }
  bScreen *screen = (bScreen *)id;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    BKE_LIB_FOREACHID_PROCESS_FUNCTION_CALL(data, BKE_screen_foreach_id_screen_area(data, area));
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

  BLO_read_data_address(reader, &screen->preview);
  BKE_previewimg_blend_read(reader, screen->preview);

  if (!BKE_screen_area_map_blend_read_data(reader, AREAMAP_FROM_SCREEN(screen))) {
    printf("Error reading Screen %s... removing it.\n", screen->id.name + 2);
    success = false;
  }

  return success;
}

/* NOTE: file read without screens option G_FILE_NO_UI;
 * check lib pointers in call below */
static void screen_blend_read_lib(BlendLibReader *reader, ID *id)
{
  bScreen *screen = (bScreen *)id;
  /* deprecated, but needed for versioning (will be nullptr'ed then) */
  BLO_read_id_address(reader, id, &screen->scene);

  screen->animtimer = nullptr; /* saved in rare cases */
  screen->tool_tip = nullptr;
  screen->scrubbing = false;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    BKE_screen_area_blend_read_lib(reader, &screen->id, area);
  }
}

IDTypeInfo IDType_ID_SCR = {
    /*id_code*/ ID_SCR,
    /*id_filter*/ FILTER_ID_SCR,
    /*main_listbase_index*/ INDEX_ID_SCR,
    /*struct_size*/ sizeof(bScreen),
    /*name*/ "Screen",
    /*name_plural*/ "screens",
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
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ screen_blend_write,
    /* Cannot be used yet, because #direct_link_screen has a return value. */
    /*blend_read_data*/ nullptr,
    /*blend_read_lib*/ screen_blend_read_lib,
    /*blend_read_expand*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

/* ************ Space-type/region-type handling ************** */

/** Keep global; this has to be accessible outside of window-manager. */
static ListBase spacetypes = {nullptr, nullptr};

/* not SpaceType itself */
static void spacetype_free(SpaceType *st)
{
  LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
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

  BLI_freelistN(&st->regiontypes);
  BLI_freelistN(&st->asset_shelf_types);
}

void BKE_spacetypes_free(void)
{
  LISTBASE_FOREACH (SpaceType *, st, &spacetypes) {
    spacetype_free(st);
  }

  BLI_freelistN(&spacetypes);
}

SpaceType *BKE_spacetype_from_id(int spaceid)
{
  LISTBASE_FOREACH (SpaceType *, st, &spacetypes) {
    if (st->spaceid == spaceid) {
      return st;
    }
  }
  return nullptr;
}

ARegionType *BKE_regiontype_from_id_or_first(const SpaceType *st, int regionid)
{
  LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
    if (art->regionid == regionid) {
      return art;
    }
  }

  printf(
      "Error, region type %d missing in - name:\"%s\", id:%d\n", regionid, st->name, st->spaceid);
  return static_cast<ARegionType *>(st->regiontypes.first);
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

const ListBase *BKE_spacetypes_list(void)
{
  return &spacetypes;
}

void BKE_spacetype_register(SpaceType *st)
{
  /* sanity check */
  SpaceType *stype = BKE_spacetype_from_id(st->spaceid);
  if (stype) {
    printf("error: redefinition of spacetype %s\n", stype->name);
    spacetype_free(stype);
    MEM_freeN(stype);
  }

  BLI_addtail(&spacetypes, st);
}

bool BKE_spacetype_exists(int spaceid)
{
  return BKE_spacetype_from_id(spaceid) != nullptr;
}

/* ***************** Space handling ********************** */

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
  BLI_duplicatelist(newlb, lb);

  /* copy panel pointers */
  Panel *new_panel = static_cast<Panel *>(newlb->first);
  Panel *panel = static_cast<Panel *>(lb->first);
  for (; new_panel; new_panel = new_panel->next, panel = panel->next) {
    new_panel->activedata = nullptr;
    memset(&new_panel->runtime, 0x0, sizeof(new_panel->runtime));
    panel_list_copy(&new_panel->children, &panel->children);
  }
}

ARegion *BKE_area_region_copy(const SpaceType *st, const ARegion *region)
{
  ARegion *newar = static_cast<ARegion *>(MEM_dupallocN(region));

  memset(&newar->runtime, 0x0, sizeof(newar->runtime));

  newar->prev = newar->next = nullptr;
  BLI_listbase_clear(&newar->handlers);
  BLI_listbase_clear(&newar->uiblocks);
  BLI_listbase_clear(&newar->panels_category);
  BLI_listbase_clear(&newar->panels_category_active);
  BLI_listbase_clear(&newar->ui_lists);
  newar->visible = 0;
  newar->gizmo_map = nullptr;
  newar->regiontimer = nullptr;
  newar->headerstr = nullptr;
  newar->draw_buffer = nullptr;

  /* use optional regiondata callback */
  if (region->regiondata) {
    ARegionType *art = BKE_regiontype_from_id(st, region->regiontype);

    if (art && art->duplicate) {
      newar->regiondata = art->duplicate(region->regiondata);
    }
    else if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
      newar->regiondata = nullptr;
    }
    else {
      newar->regiondata = MEM_dupallocN(region->regiondata);
    }
  }

  panel_list_copy(&newar->panels, &region->panels);

  BLI_listbase_clear(&newar->ui_previews);
  BLI_duplicatelist(&newar->ui_previews, &region->ui_previews);

  return newar;
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

void BKE_spacedata_draw_locks(bool set)
{
  LISTBASE_FOREACH (SpaceType *, st, &spacetypes) {
    LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
      if (set) {
        art->do_lock = art->lock;
      }
      else {
        art->do_lock = false;
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
static void (*region_refresh_tag_gizmomap_callback)(struct wmGizmoMap *) = nullptr;

void BKE_region_callback_refresh_tag_gizmomap_set(void (*callback)(struct wmGizmoMap *))
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
      if (region->gizmo_map != nullptr) {
        region_refresh_tag_gizmomap_callback(region->gizmo_map);
      }
    }
  }
}

/**
 * Avoid bad-level calls to #WM_gizmomap_delete.
 */
static void (*region_free_gizmomap_callback)(struct wmGizmoMap *) = nullptr;

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

void BKE_area_region_panels_free(ListBase *panels)
{
  LISTBASE_FOREACH_MUTABLE (Panel *, panel, panels) {
    /* Free custom data just for parent panels to avoid a double free. */
    MEM_SAFE_FREE(panel->runtime.custom_data_ptr);
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
  else if (region->type && region->type->free) {
    region->type->free(region);
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

  if (region->gizmo_map != nullptr) {
    region_free_gizmomap_callback(region->gizmo_map);
  }

  if (region->runtime.block_name_map != nullptr) {
    BLI_ghash_free(region->runtime.block_name_map, nullptr, nullptr);
    region->runtime.block_name_map = nullptr;
  }

  BLI_freelistN(&region->ui_lists);
  BLI_freelistN(&region->ui_previews);
  BLI_freelistN(&region->panels_category);
  BLI_freelistN(&region->panels_category_active);
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

/* ***************** Screen edges & verts ***************** */

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

/* ***************** Utilities ********************** */

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

ARegion *BKE_area_find_region_active_win(ScrArea *area)
{
  if (area == nullptr) {
    return nullptr;
  }

  ARegion *region = static_cast<ARegion *>(
      BLI_findlink(&area->regionbase, area->region_active_win));
  if (region && (region->regiontype == RGN_TYPE_WINDOW)) {
    return region;
  }

  /* fallback to any */
  return BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
}

ARegion *BKE_area_find_region_xy(ScrArea *area, const int regiontype, const int xy[2])
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

ARegion *BKE_screen_find_region_xy(bScreen *screen, const int regiontype, const int xy[2])
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

ScrArea *BKE_screen_find_area_from_space(bScreen *screen, SpaceLink *sl)
{
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (BLI_findindex(&area->spacedata, sl) != -1) {
      return area;
    }
  }

  return nullptr;
}

ScrArea *BKE_screen_find_big_area(bScreen *screen, const int spacetype, const short min)
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
ScrArea *BKE_screen_find_area_xy(bScreen *screen, const int spacetype, const int xy[2])
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

ARegion *BKE_screen_find_main_region_at_xy(bScreen *screen, const int space_type, const int xy[2])
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

void BKE_screen_view3d_shading_blend_write(BlendWriter *writer, View3DShading *shading)
{
  if (shading->prop) {
    IDP_BlendWrite(writer, shading->prop);
  }
}

void BKE_screen_view3d_shading_blend_read_data(BlendDataReader *reader, View3DShading *shading)
{
  if (shading->prop) {
    BLO_read_data_address(reader, &shading->prop);
    IDP_BlendDataRead(reader, &shading->prop);
  }
}

static void write_region(BlendWriter *writer, ARegion *region, int spacetype)
{
  BLO_write_struct(writer, ARegion, region);

  if (region->regiondata) {
    if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
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
    BLO_write_struct(writer, Panel, panel);
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

static void direct_link_panel_list(BlendDataReader *reader, ListBase *lb)
{
  BLO_read_list(reader, lb);

  LISTBASE_FOREACH (Panel *, panel, lb) {
    panel->runtime_flag = 0;
    panel->activedata = nullptr;
    panel->type = nullptr;
    panel->runtime.custom_data_ptr = nullptr;
    direct_link_panel_list(reader, &panel->children);
  }
}

static void direct_link_region(BlendDataReader *reader, ARegion *region, int spacetype)
{
  memset(&region->runtime, 0x0, sizeof(region->runtime));

  direct_link_panel_list(reader, &region->panels);

  BLO_read_list(reader, &region->panels_category_active);

  BLO_read_list(reader, &region->ui_lists);

  /* The area's search filter is runtime only, so we need to clear the active flag on read. */
  /* Clear runtime flags (e.g. search filter is runtime only). */
  region->flag &= ~(RGN_FLAG_SEARCH_FILTER_ACTIVE | RGN_FLAG_POLL_FAILED);

  LISTBASE_FOREACH (uiList *, ui_list, &region->ui_lists) {
    ui_list->type = nullptr;
    ui_list->dyn_data = nullptr;
    BLO_read_data_address(reader, &ui_list->properties);
    IDP_BlendDataRead(reader, &ui_list->properties);
  }

  BLO_read_list(reader, &region->ui_previews);

  if (spacetype == SPACE_EMPTY) {
    /* unknown space type, don't leak regiondata */
    region->regiondata = nullptr;
  }
  else if (region->flag & RGN_FLAG_TEMP_REGIONDATA) {
    /* Runtime data, don't use. */
    region->regiondata = nullptr;
  }
  else {
    BLO_read_data_address(reader, &region->regiondata);
    if (region->regiondata) {
      if (spacetype == SPACE_VIEW3D) {
        RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);

        BLO_read_data_address(reader, &rv3d->localvd);
        BLO_read_data_address(reader, &rv3d->clipbb);

        rv3d->render_engine = nullptr;
        rv3d->sms = nullptr;
        rv3d->smooth_timer = nullptr;

        rv3d->rflag &= ~(RV3D_NAVIGATING | RV3D_PAINTING);
        rv3d->runtime_viewlock = 0;
      }
    }
  }

  region->v2d.sms = nullptr;
  region->v2d.alpha_hor = region->v2d.alpha_vert = 255; /* visible by default */
  BLI_listbase_clear(&region->panels_category);
  BLI_listbase_clear(&region->handlers);
  BLI_listbase_clear(&region->uiblocks);
  region->headerstr = nullptr;
  region->visible = 0;
  region->type = nullptr;
  region->do_draw = 0;
  region->gizmo_map = nullptr;
  region->regiontimer = nullptr;
  region->draw_buffer = nullptr;
  memset(&region->drawrct, 0, sizeof(region->drawrct));
}

void BKE_screen_view3d_do_versions_250(View3D *v3d, ListBase *regions)
{
  LISTBASE_FOREACH (ARegion *, region, regions) {
    if (region->regiontype == RGN_TYPE_WINDOW && region->regiondata == nullptr) {
      RegionView3D *rv3d;

      rv3d = static_cast<RegionView3D *>(
          region->regiondata = MEM_callocN(sizeof(RegionView3D), "region v3d patch"));
      rv3d->persp = (char)v3d->persp;
      rv3d->view = (char)v3d->view;
      rv3d->dist = v3d->dist;
      copy_v3_v3(rv3d->ofs, v3d->ofs);
      copy_qt_qt(rv3d->viewquat, v3d->viewquat);
    }
  }

  /* this was not initialized correct always */
  if (v3d->gridsubdiv == 0) {
    v3d->gridsubdiv = 10;
  }
}

static void direct_link_area(BlendDataReader *reader, ScrArea *area)
{
  BLO_read_list(reader, &(area->spacedata));
  BLO_read_list(reader, &(area->regionbase));

  BLI_listbase_clear(&area->handlers);
  area->type = nullptr; /* spacetype callbacks */

  /* Should always be unset so that rna_Area_type_get works correctly. */
  area->butspacetype = SPACE_EMPTY;

  area->region_active_win = -1;

  area->flag &= ~AREA_FLAG_ACTIVE_TOOL_UPDATE;

  BLO_read_data_address(reader, &area->global);

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
    SpaceInfo *sinfo = static_cast<SpaceInfo *>(MEM_callocN(sizeof(SpaceInfo), "spaceinfo"));
    area->spacetype = sinfo->spacetype = SPACE_INFO;
    BLI_addtail(&area->spacedata, sinfo);
  }
  /* add local view3d too */
  else if (area->spacetype == SPACE_VIEW3D) {
    BKE_screen_view3d_do_versions_250(static_cast<View3D *>(area->spacedata.first),
                                      &area->regionbase);
  }

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    BLO_read_list(reader, &(sl->regionbase));

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

  BLO_read_data_address(reader, &area->v1);
  BLO_read_data_address(reader, &area->v2);
  BLO_read_data_address(reader, &area->v3);
  BLO_read_data_address(reader, &area->v4);
}

bool BKE_screen_area_map_blend_read_data(BlendDataReader *reader, ScrAreaMap *area_map)
{
  BLO_read_list(reader, &area_map->vertbase);
  BLO_read_list(reader, &area_map->edgebase);
  BLO_read_list(reader, &area_map->areabase);
  LISTBASE_FOREACH (ScrArea *, area, &area_map->areabase) {
    direct_link_area(reader, area);
  }

  /* edges */
  LISTBASE_FOREACH (ScrEdge *, se, &area_map->edgebase) {
    BLO_read_data_address(reader, &se->v1);
    BLO_read_data_address(reader, &se->v2);
    BKE_screen_sort_scrvert(&se->v1, &se->v2);

    if (se->v1 == nullptr) {
      BLI_remlink(&area_map->edgebase, se);

      return false;
    }
  }

  return true;
}

void BKE_screen_area_blend_read_lib(BlendLibReader *reader, ID *parent_id, ScrArea *area)
{
  BLO_read_id_address(reader, parent_id, &area->full);

  memset(&area->runtime, 0x0, sizeof(area->runtime));

  LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
    SpaceType *space_type = BKE_spacetype_from_id(sl->spacetype);

    if (space_type && space_type->blend_read_lib) {
      space_type->blend_read_lib(reader, parent_id, sl);
    }
  }
}

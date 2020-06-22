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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spoutliner
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_mempool.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_outliner_treehash.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_screen.h"
#include "ED_space_api.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_types.h"

#include "RNA_access.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_framebuffer.h"
#include "outliner_intern.h"

static void outliner_main_region_init(wmWindowManager *wm, ARegion *region)
{
  ListBase *lb;
  wmKeyMap *keymap;

  /* make sure we keep the hide flags */
  region->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
  region->v2d.scroll &= ~(V2D_SCROLL_LEFT | V2D_SCROLL_TOP); /* prevent any noise of past */
  region->v2d.scroll |= V2D_SCROLL_HORIZONTAL_HIDE;
  region->v2d.scroll |= V2D_SCROLL_VERTICAL_HIDE;

  region->v2d.align = (V2D_ALIGN_NO_NEG_X | V2D_ALIGN_NO_POS_Y);
  region->v2d.keepzoom = (V2D_LOCKZOOM_X | V2D_LOCKZOOM_Y | V2D_LIMITZOOM | V2D_KEEPASPECT);
  region->v2d.keeptot = V2D_KEEPTOT_STRICT;
  region->v2d.minzoom = region->v2d.maxzoom = 1.0f;

  UI_view2d_region_reinit(&region->v2d, V2D_COMMONVIEW_LIST, region->winx, region->winy);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Outliner", SPACE_OUTLINER, 0);
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);

  /* Add dropboxes */
  lb = WM_dropboxmap_find("Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);
  WM_event_add_dropbox_handler(&region->handlers, lb);
}

static void outliner_main_region_draw(const bContext *C, ARegion *region)
{
  View2D *v2d = &region->v2d;

  /* clear */
  UI_ThemeClearColor(TH_BACK);
  GPU_clear(GPU_COLOR_BIT);

  draw_outliner(C);

  /* reset view matrix */
  UI_view2d_view_restore(C);

  /* scrollers */
  UI_view2d_scrollers_draw(v2d, NULL);
}

static void outliner_main_region_free(ARegion *UNUSED(region))
{
}

static void outliner_main_region_listener(wmWindow *UNUSED(win),
                                          ScrArea *UNUSED(area),
                                          ARegion *region,
                                          wmNotifier *wmn,
                                          const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_OB_ACTIVE:
        case ND_OB_SELECT:
        case ND_OB_VISIBLE:
        case ND_OB_RENDER:
        case ND_MODE:
        case ND_KEYINGSET:
        case ND_FRAME:
        case ND_RENDER_OPTIONS:
        case ND_SEQUENCER:
        case ND_LAYER:
        case ND_LAYER_CONTENT:
        case ND_WORLD:
        case ND_SCENEBROWSE:
          ED_region_tag_redraw(region);
          break;
      }
      if (wmn->action & NA_EDITED) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_OBJECT:
      switch (wmn->data) {
        case ND_TRANSFORM:
          /* transform doesn't change outliner data */
          break;
        case ND_BONE_ACTIVE:
        case ND_BONE_SELECT:
        case ND_DRAW:
        case ND_PARENT:
        case ND_OB_SHADING:
          ED_region_tag_redraw(region);
          break;
        case ND_CONSTRAINT:
          /* all constraint actions now, for reordering */
          ED_region_tag_redraw(region);
          break;
        case ND_MODIFIER:
          /* all modifier actions now */
          ED_region_tag_redraw(region);
          break;
        default:
          /* Trigger update for NC_OBJECT itself */
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_GROUP:
      /* all actions now, todo: check outliner view mode? */
      ED_region_tag_redraw(region);
      break;
    case NC_LAMP:
      /* For updating light icons, when changing light type */
      if (wmn->data == ND_LIGHTING_DRAW) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_OUTLINER) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_MATERIAL:
      switch (wmn->data) {
        case ND_SHADING_LINKS:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_GEOM:
      switch (wmn->data) {
        case ND_VERTEX_GROUP:
        case ND_DATA:
          ED_region_tag_redraw(region);
          break;
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_NLA_ACTCHANGE:
        case ND_KEYFRAME:
          ED_region_tag_redraw(region);
          break;
        case ND_ANIMCHAN:
          if (wmn->action == NA_SELECTED) {
            ED_region_tag_redraw(region);
          }
          break;
      }
      break;
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_LAYOUTDELETE, ND_LAYER)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_MASK:
      if (ELEM(wmn->action, NA_ADDED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_PAINTCURVE:
      if (ELEM(wmn->action, NA_ADDED)) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_TEXT:
      if (ELEM(wmn->action, NA_ADDED, NA_REMOVED)) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

static void outliner_main_region_message_subscribe(const struct bContext *UNUSED(C),
                                                   struct WorkSpace *UNUSED(workspace),
                                                   struct Scene *UNUSED(scene),
                                                   struct bScreen *UNUSED(screen),
                                                   struct ScrArea *area,
                                                   struct ARegion *region,
                                                   struct wmMsgBus *mbus)
{
  SpaceOutliner *soops = area->spacedata.first;
  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = region,
      .user_data = region,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  if (ELEM(soops->outlinevis, SO_VIEW_LAYER, SO_SCENES)) {
    WM_msg_subscribe_rna_anon_prop(mbus, Window, view_layer, &msg_sub_value_region_tag_redraw);
  }
}

/* ************************ header outliner area region *********************** */

/* add handlers, stuff you only do once or on area/region changes */
static void outliner_header_region_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void outliner_header_region_draw(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

static void outliner_header_region_free(ARegion *UNUSED(region))
{
}

static void outliner_header_region_listener(wmWindow *UNUSED(win),
                                            ScrArea *UNUSED(area),
                                            ARegion *region,
                                            wmNotifier *wmn,
                                            const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      if (wmn->data == ND_KEYINGSET) {
        ED_region_tag_redraw(region);
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_OUTLINER) {
        ED_region_tag_redraw(region);
      }
      break;
  }
}

/* ******************** default callbacks for outliner space ***************** */

static SpaceLink *outliner_new(const ScrArea *UNUSED(area), const Scene *UNUSED(scene))
{
  ARegion *region;
  SpaceOutliner *soutliner;

  soutliner = MEM_callocN(sizeof(SpaceOutliner), "initoutliner");
  soutliner->spacetype = SPACE_OUTLINER;
  soutliner->filter_id_type = ID_GR;
  soutliner->show_restrict_flags = SO_RESTRICT_ENABLE | SO_RESTRICT_HIDE;
  soutliner->outlinevis = SO_VIEW_LAYER;
  soutliner->sync_select_dirty |= WM_OUTLINER_SYNC_SELECT_FROM_ALL;
  soutliner->flag |= SO_SYNC_SELECT;

  /* header */
  region = MEM_callocN(sizeof(ARegion), "header for outliner");

  BLI_addtail(&soutliner->regionbase, region);
  region->regiontype = RGN_TYPE_HEADER;
  region->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* main region */
  region = MEM_callocN(sizeof(ARegion), "main region for outliner");

  BLI_addtail(&soutliner->regionbase, region);
  region->regiontype = RGN_TYPE_WINDOW;

  return (SpaceLink *)soutliner;
}

/* not spacelink itself */
static void outliner_free(SpaceLink *sl)
{
  SpaceOutliner *soutliner = (SpaceOutliner *)sl;

  outliner_free_tree(&soutliner->tree);
  if (soutliner->treestore) {
    BLI_mempool_destroy(soutliner->treestore);
  }
  if (soutliner->treehash) {
    BKE_outliner_treehash_free(soutliner->treehash);
  }
}

/* spacetype; init callback */
static void outliner_init(wmWindowManager *UNUSED(wm), ScrArea *UNUSED(area))
{
}

static SpaceLink *outliner_duplicate(SpaceLink *sl)
{
  SpaceOutliner *soutliner = (SpaceOutliner *)sl;
  SpaceOutliner *soutlinern = MEM_dupallocN(soutliner);

  BLI_listbase_clear(&soutlinern->tree);
  soutlinern->treestore = NULL;
  soutlinern->treehash = NULL;

  soutlinern->flag |= (soutliner->flag & SO_SYNC_SELECT);
  soutlinern->sync_select_dirty = WM_OUTLINER_SYNC_SELECT_FROM_ALL;

  return (SpaceLink *)soutlinern;
}

static void outliner_id_remap(ScrArea *UNUSED(area), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceOutliner *so = (SpaceOutliner *)slink;

  /* Some early out checks. */
  if (!TREESTORE_ID_TYPE(old_id)) {
    return; /* ID type is not used by outilner... */
  }

  if (so->search_tse.id == old_id) {
    so->search_tse.id = new_id;
  }

  if (so->treestore) {
    TreeStoreElem *tselem;
    BLI_mempool_iter iter;
    bool changed = false;

    BLI_mempool_iternew(so->treestore, &iter);
    while ((tselem = BLI_mempool_iterstep(&iter))) {
      if (tselem->id == old_id) {
        tselem->id = new_id;
        changed = true;
      }
    }
    if (so->treehash && changed) {
      /* rebuild hash table, because it depends on ids too */
      /* postpone a full rebuild because this can be called many times on-free */
      so->storeflag |= SO_TREESTORE_REBUILD;
    }
  }
}

static void outliner_deactivate(struct ScrArea *area)
{
  /* Remove hover highlights */
  SpaceOutliner *soops = area->spacedata.first;
  outliner_flag_set(&soops->tree, TSE_HIGHLIGHTED, false);
  ED_region_tag_redraw_no_rebuild(BKE_area_find_region_type(area, RGN_TYPE_WINDOW));
}

/* only called once, from space_api/spacetypes.c */
void ED_spacetype_outliner(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype time");
  ARegionType *art;

  st->spaceid = SPACE_OUTLINER;
  strncpy(st->name, "Outliner", BKE_ST_MAXNAME);

  st->new = outliner_new;
  st->free = outliner_free;
  st->init = outliner_init;
  st->duplicate = outliner_duplicate;
  st->operatortypes = outliner_operatortypes;
  st->keymap = outliner_keymap;
  st->dropboxes = outliner_dropboxes;
  st->id_remap = outliner_id_remap;
  st->deactivate = outliner_deactivate;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype outliner region");
  art->regionid = RGN_TYPE_WINDOW;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

  art->init = outliner_main_region_init;
  art->draw = outliner_main_region_draw;
  art->free = outliner_main_region_free;
  art->listener = outliner_main_region_listener;
  art->message_subscribe = outliner_main_region_message_subscribe;
  BLI_addhead(&st->regiontypes, art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype outliner header region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;

  art->init = outliner_header_region_init;
  art->draw = outliner_header_region_draw;
  art->free = outliner_header_region_free;
  art->listener = outliner_header_region_listener;
  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);
}

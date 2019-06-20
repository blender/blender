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
 * \ingroup spseq
 */

#include <string.h>
#include <stdio.h>

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_mask_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_screen.h"
#include "BKE_sequencer.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_view3d.h" /* only for sequencer view3d drawing callback */

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "RNA_access.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "IMB_imbuf.h"

#include "sequencer_intern.h"  // own include

/**************************** common state *****************************/

static void sequencer_scopes_tag_refresh(ScrArea *sa)
{
  SpaceSeq *sseq = (SpaceSeq *)sa->spacedata.first;

  sseq->scopes.reference_ibuf = NULL;
}

/* ******************** manage regions ********************* */

static ARegion *sequencer_find_region(ScrArea *sa, short type)
{
  ARegion *ar = NULL;

  for (ar = sa->regionbase.first; ar; ar = ar->next) {
    if (ar->regiontype == type) {
      return ar;
    }
  }

  return ar;
}

/* ******************** default callbacks for sequencer space ***************** */

static SpaceLink *sequencer_new(const ScrArea *UNUSED(sa), const Scene *scene)
{
  ARegion *ar;
  SpaceSeq *sseq;

  sseq = MEM_callocN(sizeof(SpaceSeq), "initsequencer");
  sseq->spacetype = SPACE_SEQ;
  sseq->chanshown = 0;
  sseq->view = SEQ_VIEW_SEQUENCE;
  sseq->mainb = SEQ_DRAW_IMG_IMBUF;
  sseq->flag = SEQ_SHOW_GPENCIL | SEQ_USE_ALPHA | SEQ_SHOW_MARKER_LINES;

  /* header */
  ar = MEM_callocN(sizeof(ARegion), "header for sequencer");

  BLI_addtail(&sseq->regionbase, ar);
  ar->regiontype = RGN_TYPE_HEADER;
  ar->alignment = (U.uiflag & USER_HEADER_BOTTOM) ? RGN_ALIGN_BOTTOM : RGN_ALIGN_TOP;

  /* buttons/list view */
  ar = MEM_callocN(sizeof(ARegion), "buttons for sequencer");

  BLI_addtail(&sseq->regionbase, ar);
  ar->regiontype = RGN_TYPE_UI;
  ar->alignment = RGN_ALIGN_RIGHT;
  ar->flag = RGN_FLAG_HIDDEN;

  /* preview region */
  /* NOTE: if you change values here, also change them in sequencer_init_preview_region */
  ar = MEM_callocN(sizeof(ARegion), "preview region for sequencer");
  BLI_addtail(&sseq->regionbase, ar);
  ar->regiontype = RGN_TYPE_PREVIEW;
  ar->alignment = RGN_ALIGN_TOP;
  ar->flag |= RGN_FLAG_HIDDEN;
  /* for now, aspect ratio should be maintained, and zoom is clamped within sane default limits */
  ar->v2d.keepzoom = V2D_KEEPASPECT | V2D_KEEPZOOM | V2D_LIMITZOOM;
  ar->v2d.minzoom = 0.001f;
  ar->v2d.maxzoom = 1000.0f;
  ar->v2d.tot.xmin = -960.0f; /* 1920 width centered */
  ar->v2d.tot.ymin = -540.0f; /* 1080 height centered */
  ar->v2d.tot.xmax = 960.0f;
  ar->v2d.tot.ymax = 540.0f;
  ar->v2d.min[0] = 0.0f;
  ar->v2d.min[1] = 0.0f;
  ar->v2d.max[0] = 12000.0f;
  ar->v2d.max[1] = 12000.0f;
  ar->v2d.cur = ar->v2d.tot;
  ar->v2d.align = V2D_ALIGN_FREE;
  ar->v2d.keeptot = V2D_KEEPTOT_FREE;

  /* main region */
  ar = MEM_callocN(sizeof(ARegion), "main region for sequencer");

  BLI_addtail(&sseq->regionbase, ar);
  ar->regiontype = RGN_TYPE_WINDOW;

  /* seq space goes from (0,8) to (0, efra) */

  ar->v2d.tot.xmin = 0.0f;
  ar->v2d.tot.ymin = 0.0f;
  ar->v2d.tot.xmax = scene->r.efra;
  ar->v2d.tot.ymax = 8.0f;

  ar->v2d.cur = ar->v2d.tot;

  ar->v2d.min[0] = 10.0f;
  ar->v2d.min[1] = 0.5f;

  ar->v2d.max[0] = MAXFRAMEF;
  ar->v2d.max[1] = MAXSEQ;

  ar->v2d.minzoom = 0.01f;
  ar->v2d.maxzoom = 100.0f;

  ar->v2d.scroll |= (V2D_SCROLL_BOTTOM | V2D_SCROLL_HORIZONTAL_HANDLES);
  ar->v2d.scroll |= (V2D_SCROLL_RIGHT | V2D_SCROLL_VERTICAL_HANDLES);
  ar->v2d.keepzoom = 0;
  ar->v2d.keeptot = 0;
  ar->v2d.align = V2D_ALIGN_NO_NEG_Y;

  return (SpaceLink *)sseq;
}

/* not spacelink itself */
static void sequencer_free(SpaceLink *sl)
{
  SpaceSeq *sseq = (SpaceSeq *)sl;
  SequencerScopes *scopes = &sseq->scopes;

  // XXX  if (sseq->gpd) BKE_gpencil_free(sseq->gpd);

  if (scopes->zebra_ibuf) {
    IMB_freeImBuf(scopes->zebra_ibuf);
  }

  if (scopes->waveform_ibuf) {
    IMB_freeImBuf(scopes->waveform_ibuf);
  }

  if (scopes->sep_waveform_ibuf) {
    IMB_freeImBuf(scopes->sep_waveform_ibuf);
  }

  if (scopes->vector_ibuf) {
    IMB_freeImBuf(scopes->vector_ibuf);
  }

  if (scopes->histogram_ibuf) {
    IMB_freeImBuf(scopes->histogram_ibuf);
  }
}

/* spacetype; init callback */
static void sequencer_init(struct wmWindowManager *UNUSED(wm), ScrArea *UNUSED(sa))
{
}

static void sequencer_refresh(const bContext *C, ScrArea *sa)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *window = CTX_wm_window(C);
  SpaceSeq *sseq = (SpaceSeq *)sa->spacedata.first;
  ARegion *ar_main = sequencer_find_region(sa, RGN_TYPE_WINDOW);
  ARegion *ar_preview = sequencer_find_region(sa, RGN_TYPE_PREVIEW);
  bool view_changed = false;

  switch (sseq->view) {
    case SEQ_VIEW_SEQUENCE:
      if (ar_main && (ar_main->flag & RGN_FLAG_HIDDEN)) {
        ar_main->flag &= ~RGN_FLAG_HIDDEN;
        ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
        view_changed = true;
      }
      if (ar_preview && !(ar_preview->flag & RGN_FLAG_HIDDEN)) {
        ar_preview->flag |= RGN_FLAG_HIDDEN;
        ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
        WM_event_remove_handlers((bContext *)C, &ar_preview->handlers);
        view_changed = true;
      }
      if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
        ar_main->alignment = RGN_ALIGN_NONE;
        view_changed = true;
      }
      if (ar_preview && ar_preview->alignment != RGN_ALIGN_NONE) {
        ar_preview->alignment = RGN_ALIGN_NONE;
        view_changed = true;
      }
      break;
    case SEQ_VIEW_PREVIEW:
      if (ar_main && !(ar_main->flag & RGN_FLAG_HIDDEN)) {
        ar_main->flag |= RGN_FLAG_HIDDEN;
        ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
        WM_event_remove_handlers((bContext *)C, &ar_main->handlers);
        view_changed = true;
      }
      if (ar_preview && (ar_preview->flag & RGN_FLAG_HIDDEN)) {
        ar_preview->flag &= ~RGN_FLAG_HIDDEN;
        ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
        ar_preview->v2d.cur = ar_preview->v2d.tot;
        view_changed = true;
      }
      if (ar_main && ar_main->alignment != RGN_ALIGN_NONE) {
        ar_main->alignment = RGN_ALIGN_NONE;
        view_changed = true;
      }
      if (ar_preview && ar_preview->alignment != RGN_ALIGN_NONE) {
        ar_preview->alignment = RGN_ALIGN_NONE;
        view_changed = true;
      }
      break;
    case SEQ_VIEW_SEQUENCE_PREVIEW:
      if (ar_main && ar_preview) {
        /* Get available height (without DPI correction). */
        const float height = (sa->winy - ED_area_headersize()) / UI_DPI_FAC;

        /* We reuse hidden region's size, allows to find same layout as before if we just switch
         * between one 'full window' view and the combined one. This gets lost if we switch to both
         * 'full window' views before, though... Better than nothing. */
        if (ar_main->flag & RGN_FLAG_HIDDEN) {
          ar_main->flag &= ~RGN_FLAG_HIDDEN;
          ar_main->v2d.flag &= ~V2D_IS_INITIALISED;
          ar_preview->sizey = (int)(height - ar_main->sizey);
          view_changed = true;
        }
        if (ar_preview->flag & RGN_FLAG_HIDDEN) {
          ar_preview->flag &= ~RGN_FLAG_HIDDEN;
          ar_preview->v2d.flag &= ~V2D_IS_INITIALISED;
          ar_preview->v2d.cur = ar_preview->v2d.tot;
          ar_main->sizey = (int)(height - ar_preview->sizey);
          view_changed = true;
        }
        if (ar_main->alignment != RGN_ALIGN_NONE) {
          ar_main->alignment = RGN_ALIGN_NONE;
          view_changed = true;
        }
        if (ar_preview->alignment != RGN_ALIGN_TOP) {
          ar_preview->alignment = RGN_ALIGN_TOP;
          view_changed = true;
        }
        /* Final check that both preview and main height are reasonable! */
        if (ar_preview->sizey < 10 || ar_main->sizey < 10 ||
            ar_preview->sizey + ar_main->sizey > height) {
          ar_preview->sizey = (int)(height * 0.4f + 0.5f);
          ar_main->sizey = (int)(height - ar_preview->sizey);
          view_changed = true;
        }
      }
      break;
  }

  if (view_changed) {
    ED_area_initialize(wm, window, sa);
    ED_area_tag_redraw(sa);
  }
}

static SpaceLink *sequencer_duplicate(SpaceLink *sl)
{
  SpaceSeq *sseqn = MEM_dupallocN(sl);

  /* clear or remove stuff from old */
  // XXX  sseq->gpd = gpencil_data_duplicate(sseq->gpd, false);

  memset(&sseqn->scopes, 0, sizeof(sseqn->scopes));

  return (SpaceLink *)sseqn;
}

static void sequencer_listener(wmWindow *UNUSED(win),
                               ScrArea *sa,
                               wmNotifier *wmn,
                               Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_SEQUENCER:
          sequencer_scopes_tag_refresh(sa);
          break;
      }
      break;
    case NC_WINDOW:
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        sequencer_scopes_tag_refresh(sa);
      }
      break;
    case NC_GPENCIL:
      if (wmn->data & ND_GPENCIL_EDITMODE) {
        ED_area_tag_redraw(sa);
      }
      break;
  }
}

/* ************* dropboxes ************* */

static bool image_drop_poll(bContext *C,
                            wmDrag *drag,
                            const wmEvent *event,
                            const char **UNUSED(tooltip))
{
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int hand;

  if (drag->type == WM_DRAG_PATH) {
    if (ELEM(drag->icon, ICON_FILE_IMAGE, ICON_FILE_BLANK)) { /* rule might not work? */
      if (find_nearest_seq(scene, &ar->v2d, &hand, event->mval) == NULL) {
        return 1;
      }
    }
  }

  return 0;
}

static bool movie_drop_poll(bContext *C,
                            wmDrag *drag,
                            const wmEvent *event,
                            const char **UNUSED(tooltip))
{
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int hand;

  if (drag->type == WM_DRAG_PATH) {
    if (ELEM(drag->icon, 0, ICON_FILE_MOVIE, ICON_FILE_BLANK)) { /* rule might not work? */
      if (find_nearest_seq(scene, &ar->v2d, &hand, event->mval) == NULL) {
        return 1;
      }
    }
  }
  return 0;
}

static bool sound_drop_poll(bContext *C,
                            wmDrag *drag,
                            const wmEvent *event,
                            const char **UNUSED(tooltip))
{
  ARegion *ar = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int hand;

  if (drag->type == WM_DRAG_PATH) {
    if (ELEM(drag->icon, ICON_FILE_SOUND, ICON_FILE_BLANK)) { /* rule might not work? */
      if (find_nearest_seq(scene, &ar->v2d, &hand, event->mval) == NULL) {
        return 1;
      }
    }
  }
  return 0;
}

static void sequencer_drop_copy(wmDrag *drag, wmDropBox *drop)
{
  /* copy drag path to properties */
  if (RNA_struct_find_property(drop->ptr, "filepath")) {
    RNA_string_set(drop->ptr, "filepath", drag->path);
  }

  if (RNA_struct_find_property(drop->ptr, "directory")) {
    PointerRNA itemptr;
    char dir[FILE_MAX], file[FILE_MAX];

    BLI_split_dirfile(drag->path, dir, file, sizeof(dir), sizeof(file));

    RNA_string_set(drop->ptr, "directory", dir);

    RNA_collection_clear(drop->ptr, "files");
    RNA_collection_add(drop->ptr, "files", &itemptr);
    RNA_string_set(&itemptr, "name", file);
  }
}

/* this region dropbox definition */
static void sequencer_dropboxes(void)
{
  ListBase *lb = WM_dropboxmap_find("Sequencer", SPACE_SEQ, RGN_TYPE_WINDOW);

  WM_dropbox_add(lb, "SEQUENCER_OT_image_strip_add", image_drop_poll, sequencer_drop_copy);
  WM_dropbox_add(lb, "SEQUENCER_OT_movie_strip_add", movie_drop_poll, sequencer_drop_copy);
  WM_dropbox_add(lb, "SEQUENCER_OT_sound_strip_add", sound_drop_poll, sequencer_drop_copy);
}

/* ************* end drop *********** */

/* DO NOT make this static, this hides the symbol and breaks API generation script. */
extern const char *sequencer_context_dir[]; /* quiet warning. */
const char *sequencer_context_dir[] = {"edit_mask", NULL};

static int sequencer_context(const bContext *C, const char *member, bContextDataResult *result)
{
  Scene *scene = CTX_data_scene(C);

  if (CTX_data_dir(member)) {
    CTX_data_dir_set(result, sequencer_context_dir);

    return true;
  }
  else if (CTX_data_equals(member, "edit_mask")) {
    Mask *mask = BKE_sequencer_mask_get(scene);
    if (mask) {
      CTX_data_id_pointer_set(result, &mask->id);
    }
    return true;
  }

  return false;
}

static void SEQUENCER_GGT_navigate(wmGizmoGroupType *gzgt)
{
  VIEW2D_GGT_navigate_impl(gzgt, "SEQUENCER_GGT_navigate");
}

static void sequencer_gizmos(void)
{
  wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(
      &(const struct wmGizmoMapType_Params){SPACE_SEQ, RGN_TYPE_PREVIEW});

  WM_gizmogrouptype_append_and_link(gzmap_type, SEQUENCER_GGT_navigate);
}

/* *********************** sequencer (main) region ************************ */
/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_main_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;
  ListBase *lb;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

#if 0
  keymap = WM_keymap_ensure(wm->defaultconf, "Mask Editing", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
#endif

  keymap = WM_keymap_ensure(wm->defaultconf, "SequencerCommon", SPACE_SEQ, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "Sequencer", SPACE_SEQ, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  /* add drop boxes */
  lb = WM_dropboxmap_find("Sequencer", SPACE_SEQ, RGN_TYPE_WINDOW);

  WM_event_add_dropbox_handler(&ar->handlers, lb);
}

static void sequencer_main_region_draw(const bContext *C, ARegion *ar)
{
  /* NLE - strip editing timeline interface */
  draw_timeline_seq(C, ar);
}

static void sequencer_main_region_listener(wmWindow *UNUSED(win),
                                           ScrArea *UNUSED(sa),
                                           ARegion *ar,
                                           wmNotifier *wmn,
                                           const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_FRAME_RANGE:
        case ND_MARKERS:
        case ND_RENDER_OPTIONS: /* for FPS and FPS Base */
        case ND_SEQUENCER:
        case ND_RENDER_RESULT:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCREEN:
      if (ELEM(wmn->data, ND_ANIMPLAY)) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

static void sequencer_main_region_message_subscribe(const struct bContext *UNUSED(C),
                                                    struct WorkSpace *UNUSED(workspace),
                                                    struct Scene *scene,
                                                    struct bScreen *UNUSED(screen),
                                                    struct ScrArea *UNUSED(sa),
                                                    struct ARegion *ar,
                                                    struct wmMsgBus *mbus)
{
  wmMsgSubscribeValue msg_sub_value_region_tag_redraw = {
      .owner = ar,
      .user_data = ar,
      .notify = ED_region_do_msg_notify_tag_redraw,
  };

  /* Timeline depends on scene properties. */
  {
    bool use_preview = (scene->r.flag & SCER_PRV_RANGE);
    extern PropertyRNA rna_Scene_frame_start;
    extern PropertyRNA rna_Scene_frame_end;
    extern PropertyRNA rna_Scene_frame_preview_start;
    extern PropertyRNA rna_Scene_frame_preview_end;
    extern PropertyRNA rna_Scene_use_preview_range;
    extern PropertyRNA rna_Scene_frame_current;
    const PropertyRNA *props[] = {
        use_preview ? &rna_Scene_frame_preview_start : &rna_Scene_frame_start,
        use_preview ? &rna_Scene_frame_preview_end : &rna_Scene_frame_end,
        &rna_Scene_use_preview_range,
        &rna_Scene_frame_current,
    };

    PointerRNA idptr;
    RNA_id_pointer_create(&scene->id, &idptr);

    for (int i = 0; i < ARRAY_SIZE(props); i++) {
      WM_msg_subscribe_rna(mbus, &idptr, props[i], &msg_sub_value_region_tag_redraw, __func__);
    }
  }

  {
    StructRNA *type_array[] = {
        &RNA_SequenceEditor,

        &RNA_Sequence,
        /* Members of 'Sequence'. */
        &RNA_SequenceCrop,
        &RNA_SequenceTransform,
        &RNA_SequenceModifier,
        &RNA_SequenceColorBalanceData,
    };
    wmMsgParams_RNA msg_key_params = {{{0}}};
    for (int i = 0; i < ARRAY_SIZE(type_array); i++) {
      msg_key_params.ptr.type = type_array[i];
      WM_msg_subscribe_rna_params(
          mbus, &msg_key_params, &msg_sub_value_region_tag_redraw, __func__);
    }
  }
}

/* *********************** header region ************************ */
/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
  ED_region_header_init(ar);
}

static void sequencer_header_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_header(C, ar);
}

/* *********************** preview region ************************ */
static void sequencer_preview_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);

#if 0
  keymap = WM_keymap_ensure(wm->defaultconf, "Mask Editing", 0, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
#endif

  keymap = WM_keymap_ensure(wm->defaultconf, "SequencerCommon", SPACE_SEQ, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  /* own keymap */
  keymap = WM_keymap_ensure(wm->defaultconf, "SequencerPreview", SPACE_SEQ, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);
}

static void sequencer_preview_region_draw(const bContext *C, ARegion *ar)
{
  ScrArea *sa = CTX_wm_area(C);
  SpaceSeq *sseq = sa->spacedata.first;
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const bool show_split = (scene->ed && (scene->ed->over_flag & SEQ_EDIT_OVERLAY_SHOW) &&
                           (sseq->mainb == SEQ_DRAW_IMG_IMBUF));

  /* XXX temp fix for wrong setting in sseq->mainb */
  if (sseq->mainb == SEQ_DRAW_SEQUENCE) {
    sseq->mainb = SEQ_DRAW_IMG_IMBUF;
  }

  if (!show_split || sseq->overlay_type != SEQ_DRAW_OVERLAY_REFERENCE) {
    sequencer_draw_preview(C, scene, ar, sseq, scene->r.cfra, 0, false, false);
  }

  if (show_split && sseq->overlay_type != SEQ_DRAW_OVERLAY_CURRENT) {
    int over_cfra;

    if (scene->ed->over_flag & SEQ_EDIT_OVERLAY_ABS) {
      over_cfra = scene->ed->over_cfra;
    }
    else {
      over_cfra = scene->r.cfra + scene->ed->over_ofs;
    }

    if (over_cfra != scene->r.cfra || sseq->overlay_type != SEQ_DRAW_OVERLAY_RECT) {
      sequencer_draw_preview(
          C, scene, ar, sseq, scene->r.cfra, over_cfra - scene->r.cfra, true, false);
    }
  }

  WM_gizmomap_draw(ar->gizmo_map, C, WM_GIZMOMAP_DRAWSTEP_2D);

  if ((U.uiflag & USER_SHOW_FPS) && ED_screen_animation_no_scrub(wm)) {
    rcti rect;
    ED_region_visible_rect(ar, &rect);
    int xoffset = rect.xmin + U.widget_unit;
    int yoffset = rect.ymax;
    ED_scene_draw_fps(scene, xoffset, &yoffset);
  }
}

static void sequencer_preview_region_listener(wmWindow *UNUSED(win),
                                              ScrArea *UNUSED(sa),
                                              ARegion *ar,
                                              wmNotifier *wmn,
                                              const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_MARKERS:
        case ND_SEQUENCER:
        case ND_RENDER_OPTIONS:
        case ND_DRAW_RENDER_VIEWPORT:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_ANIMATION:
      switch (wmn->data) {
        case ND_KEYFRAME:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_ID:
      switch (wmn->data) {
        case NA_RENAME:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_MASK:
      if (wmn->action == NA_EDITED) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

/* *********************** buttons region ************************ */

/* add handlers, stuff you only do once or on area/region changes */
static void sequencer_buttons_region_init(wmWindowManager *wm, ARegion *ar)
{
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(wm->defaultconf, "SequencerCommon", SPACE_SEQ, 0);
  WM_event_add_keymap_handler_v2d_mask(&ar->handlers, keymap);

  UI_panel_category_active_set_default(ar, "Strip");
  ED_region_panels_init(wm, ar);
}

static void sequencer_buttons_region_draw(const bContext *C, ARegion *ar)
{
  ED_region_panels(C, ar);
}

static void sequencer_buttons_region_listener(wmWindow *UNUSED(win),
                                              ScrArea *UNUSED(sa),
                                              ARegion *ar,
                                              wmNotifier *wmn,
                                              const Scene *UNUSED(scene))
{
  /* context changes */
  switch (wmn->category) {
    case NC_GPENCIL:
      if (ELEM(wmn->action, NA_EDITED, NA_SELECTED)) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_SCENE:
      switch (wmn->data) {
        case ND_FRAME:
        case ND_SEQUENCER:
          ED_region_tag_redraw(ar);
          break;
      }
      break;
    case NC_SPACE:
      if (wmn->data == ND_SPACE_SEQUENCER) {
        ED_region_tag_redraw(ar);
      }
      break;
    case NC_ID:
      if (wmn->action == NA_RENAME) {
        ED_region_tag_redraw(ar);
      }
      break;
  }
}

static void sequencer_id_remap(ScrArea *UNUSED(sa), SpaceLink *slink, ID *old_id, ID *new_id)
{
  SpaceSeq *sseq = (SpaceSeq *)slink;

  if (!ELEM(GS(old_id->name), ID_GD)) {
    return;
  }

  if ((ID *)sseq->gpd == old_id) {
    sseq->gpd = (bGPdata *)new_id;
    id_us_min(old_id);
    id_us_plus(new_id);
  }
}

/* ************************************* */

/* only called once, from space/spacetypes.c */
void ED_spacetype_sequencer(void)
{
  SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype sequencer");
  ARegionType *art;

  st->spaceid = SPACE_SEQ;
  strncpy(st->name, "Sequencer", BKE_ST_MAXNAME);

  st->new = sequencer_new;
  st->free = sequencer_free;
  st->init = sequencer_init;
  st->duplicate = sequencer_duplicate;
  st->operatortypes = sequencer_operatortypes;
  st->keymap = sequencer_keymap;
  st->context = sequencer_context;
  st->gizmos = sequencer_gizmos;
  st->dropboxes = sequencer_dropboxes;
  st->refresh = sequencer_refresh;
  st->listener = sequencer_listener;
  st->id_remap = sequencer_id_remap;

  /* regions: main window */
  art = MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
  art->regionid = RGN_TYPE_WINDOW;
  art->init = sequencer_main_region_init;
  art->draw = sequencer_main_region_draw;
  art->listener = sequencer_main_region_listener;
  art->message_subscribe = sequencer_main_region_message_subscribe;
  art->keymapflag = ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_ANIMATION;

  BLI_addhead(&st->regiontypes, art);

  /* preview */
  art = MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
  art->regionid = RGN_TYPE_PREVIEW;
  art->init = sequencer_preview_region_init;
  art->draw = sequencer_preview_region_draw;
  art->listener = sequencer_preview_region_listener;
  art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_GPENCIL;
  BLI_addhead(&st->regiontypes, art);

  /* regions: listview/buttons */
  art = MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
  art->regionid = RGN_TYPE_UI;
  art->prefsizex = UI_SIDEBAR_PANEL_WIDTH * 1.3f;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
  art->listener = sequencer_buttons_region_listener;
  art->init = sequencer_buttons_region_init;
  art->draw = sequencer_buttons_region_draw;
  BLI_addhead(&st->regiontypes, art);

  sequencer_buttons_register(art);

  /* regions: header */
  art = MEM_callocN(sizeof(ARegionType), "spacetype sequencer region");
  art->regionid = RGN_TYPE_HEADER;
  art->prefsizey = HEADERY;
  art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;

  art->init = sequencer_header_region_init;
  art->draw = sequencer_header_region_draw;
  art->listener = sequencer_main_region_listener;

  BLI_addhead(&st->regiontypes, art);

  BKE_spacetype_register(st);

  /* set the sequencer callback when not in background mode */
  if (G.background == 0) {
    sequencer_view3d_cb = ED_view3d_draw_offscreen_imbuf_simple;
  }
}

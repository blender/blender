/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 */

#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_movieclip.h"
#include "BKE_screen.hh"
#include "BKE_tracking.h"

#include "DEG_depsgraph.hh"

#include "ED_clip.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "clip_intern.hh" /* own include */

using blender::StringRefNull;

/* Panels */

static bool metadata_panel_context_poll(const bContext *C, PanelType * /*pt*/)
{
  return ED_space_clip_poll((bContext *)C);
}

static void metadata_panel_context_draw(const bContext *C, Panel *panel)
{
  SpaceClip *space_clip = CTX_wm_space_clip(C);
  /* NOTE: This might not be exactly the same image buffer as shown in the
   * clip editor itself, since that might be coming from proxy, or being
   * postprocessed (stabilized or undistorted).
   * Ideally we need to query metadata from an original image or movie without
   * reading actual pixels to speed up the process. */
  ImBuf *ibuf = ED_space_clip_get_buffer(space_clip);
  if (ibuf != nullptr) {
    ED_region_image_metadata_panel_draw(ibuf, panel->layout);
    IMB_freeImBuf(ibuf);
  }
}

void ED_clip_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN<PanelType>("spacetype clip panel metadata");
  STRNCPY_UTF8(pt->idname, "CLIP_PT_metadata");
  STRNCPY_UTF8(pt->label, N_("Metadata"));
  STRNCPY_UTF8(pt->category, "Footage");
  STRNCPY_UTF8(pt->translation_context, BLT_I18NCONTEXT_DEFAULT_BPYRNA);
  pt->poll = metadata_panel_context_poll;
  pt->draw = metadata_panel_context_draw;
  pt->flag |= PANEL_TYPE_DEFAULT_CLOSED;
  BLI_addtail(&art->paneltypes, pt);
}

/********************* MovieClip Template ************************/

void uiTemplateMovieClip(uiLayout *layout,
                         bContext *C,
                         PointerRNA *ptr,
                         const blender::StringRefNull propname,
                         bool compact)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  PointerRNA clipptr = RNA_property_pointer_get(ptr, prop);
  MovieClip *clip = static_cast<MovieClip *>(clipptr.data);

  layout->context_ptr_set("edit_movieclip", &clipptr);

  if (!compact) {
    uiTemplateID(layout, C, ptr, propname, nullptr, "CLIP_OT_open", nullptr);
  }

  if (clip) {
    uiLayout *row = &layout->row(false);
    uiBlock *block = row->block();
    uiDefBut(block, ButType::Label, 0, IFACE_("File Path:"), 0, 19, 145, 19, nullptr, 0, 0, "");

    row = &layout->row(false);
    uiLayout *split = &row->split(0.0f, false);
    row = &split->row(true);

    row->prop(&clipptr, "filepath", UI_ITEM_NONE, "", ICON_NONE);
    row->op("clip.reload", "", ICON_FILE_REFRESH);

    uiLayout *col = &layout->column(true);
    col->separator();
    col->prop(&clipptr, "frame_start", UI_ITEM_NONE, IFACE_("Start Frame"), ICON_NONE);
    col->prop(&clipptr, "frame_offset", UI_ITEM_NONE, IFACE_("Frame Offset"), ICON_NONE);
    col->separator();
    uiTemplateColorspaceSettings(col, &clipptr, "colorspace_settings");
  }
}

/********************* Track Template ************************/

void uiTemplateTrack(uiLayout *layout, PointerRNA *ptr, const StringRefNull propname)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  PointerRNA scopesptr = RNA_property_pointer_get(ptr, prop);
  MovieClipScopes *scopes = (MovieClipScopes *)scopesptr.data;

  if (scopes->track_preview_height < UI_UNIT_Y) {
    scopes->track_preview_height = UI_UNIT_Y;
  }
  else if (scopes->track_preview_height > UI_UNIT_Y * 20) {
    scopes->track_preview_height = UI_UNIT_Y * 20;
  }

  uiLayout *col = &layout->column(true);
  uiBlock *block = col->block();

  uiDefBut(block,
           ButType::TrackPreview,
           0,
           "",
           0,
           0,
           UI_UNIT_X * 10,
           scopes->track_preview_height,
           scopes,
           0,
           0,
           "");

  /* Resize grip. */
  uiDefIconButI(block,
                ButType::Grip,
                0,
                ICON_GRIP,
                0,
                0,
                UI_UNIT_X * 10,
                short(UI_UNIT_Y * 0.8f),
                &scopes->track_preview_height,
                UI_UNIT_Y,
                UI_UNIT_Y * 20.0f,
                "");
}

/********************* Marker Template ************************/

#define B_MARKER_POS 3
#define B_MARKER_OFFSET 4
#define B_MARKER_PAT_DIM 5
#define B_MARKER_SEARCH_POS 6
#define B_MARKER_SEARCH_DIM 7
#define B_MARKER_FLAG 8

struct MarkerUpdateCb {
  /** compact mode */
  int compact;

  MovieClip *clip;
  /** user of clip */
  MovieClipUser *user;
  MovieTrackingTrack *track;
  MovieTrackingMarker *marker;

  /** current frame number */
  int framenr;
  /** position of marker in pixel coords */
  float marker_pos[2];
  /** position and dimensions of marker pattern in pixel coords */
  float marker_pat[2];
  /** offset of "parenting" point */
  float track_offset[2];
  /** position and dimensions of marker search in pixel coords */
  float marker_search_pos[2], marker_search[2];
  /** marker's flags */
  int marker_flag;
};

static void to_pixel_space(float r[2], const float a[2], int width, int height)
{
  copy_v2_v2(r, a);
  r[0] *= width;
  r[1] *= height;
}

static void marker_update_cb(bContext *C, void *arg_cb, void * /*arg*/)
{
  MarkerUpdateCb *cb = (MarkerUpdateCb *)arg_cb;

  if (!cb->compact) {
    return;
  }

  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(cb->clip, cb->framenr);
  MovieTrackingMarker *marker = BKE_tracking_marker_ensure(cb->track, clip_framenr);
  marker->flag = cb->marker_flag;

  WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, nullptr);
}

static void marker_block_handler(bContext *C, void *arg_cb, int event)
{
  MarkerUpdateCb *cb = (MarkerUpdateCb *)arg_cb;
  int width, height;
  bool ok = false;

  BKE_movieclip_get_size(cb->clip, cb->user, &width, &height);

  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(cb->clip, cb->framenr);
  MovieTrackingMarker *marker = BKE_tracking_marker_ensure(cb->track, clip_framenr);

  if (event == B_MARKER_POS) {
    marker->pos[0] = cb->marker_pos[0] / width;
    marker->pos[1] = cb->marker_pos[1] / height;

    /* to update position of "parented" objects */
    DEG_id_tag_update(&cb->clip->id, 0);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

    ok = true;
  }
  else if (event == B_MARKER_PAT_DIM) {
    float dim[2], pat_dim[2], pat_min[2], pat_max[2];

    BKE_tracking_marker_pattern_minmax(cb->marker, pat_min, pat_max);

    sub_v2_v2v2(pat_dim, pat_max, pat_min);

    dim[0] = cb->marker_pat[0] / width;
    dim[1] = cb->marker_pat[1] / height;

    float scale_x = dim[0] / pat_dim[0];
    float scale_y = dim[1] / pat_dim[1];

    for (int a = 0; a < 4; a++) {
      cb->marker->pattern_corners[a][0] *= scale_x;
      cb->marker->pattern_corners[a][1] *= scale_y;
    }

    BKE_tracking_marker_clamp_search_size(cb->marker);

    ok = true;
  }
  else if (event == B_MARKER_SEARCH_POS) {
    float delta[2], side[2];

    sub_v2_v2v2(side, cb->marker->search_max, cb->marker->search_min);
    mul_v2_fl(side, 0.5f);

    delta[0] = cb->marker_search_pos[0] / width;
    delta[1] = cb->marker_search_pos[1] / height;

    sub_v2_v2v2(cb->marker->search_min, delta, side);
    add_v2_v2v2(cb->marker->search_max, delta, side);

    BKE_tracking_marker_clamp_search_position(cb->marker);

    ok = true;
  }
  else if (event == B_MARKER_SEARCH_DIM) {
    float dim[2], search_dim[2];

    sub_v2_v2v2(search_dim, cb->marker->search_max, cb->marker->search_min);

    dim[0] = cb->marker_search[0] / width;
    dim[1] = cb->marker_search[1] / height;

    sub_v2_v2(dim, search_dim);
    mul_v2_fl(dim, 0.5f);

    cb->marker->search_min[0] -= dim[0];
    cb->marker->search_min[1] -= dim[1];

    cb->marker->search_max[0] += dim[0];
    cb->marker->search_max[1] += dim[1];

    BKE_tracking_marker_clamp_search_size(cb->marker);

    ok = true;
  }
  else if (event == B_MARKER_FLAG) {
    marker->flag = cb->marker_flag;

    ok = true;
  }
  else if (event == B_MARKER_OFFSET) {
    float offset[2], delta[2];

    offset[0] = cb->track_offset[0] / width;
    offset[1] = cb->track_offset[1] / height;

    sub_v2_v2v2(delta, offset, cb->track->offset);
    copy_v2_v2(cb->track->offset, offset);

    for (int i = 0; i < cb->track->markersnr; i++) {
      sub_v2_v2(cb->track->markers[i].pos, delta);
    }

    /* to update position of "parented" objects */
    DEG_id_tag_update(&cb->clip->id, 0);
    WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

    ok = true;
  }

  if (ok) {
    WM_event_add_notifier(C, NC_MOVIECLIP | NA_EDITED, cb->clip);
  }
}

void uiTemplateMarker(uiLayout *layout,
                      PointerRNA *ptr,
                      const StringRefNull propname,
                      PointerRNA *userptr,
                      PointerRNA *trackptr,
                      bool compact)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  PointerRNA clipptr = RNA_property_pointer_get(ptr, prop);
  MovieClip *clip = (MovieClip *)clipptr.data;
  MovieClipUser *user = static_cast<MovieClipUser *>(userptr->data);
  MovieTrackingTrack *track = static_cast<MovieTrackingTrack *>(trackptr->data);

  int clip_framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
  MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_framenr);

  MarkerUpdateCb *cb = MEM_callocN<MarkerUpdateCb>("uiTemplateMarker update_cb");
  cb->compact = compact;
  cb->clip = clip;
  cb->user = user;
  cb->track = track;
  cb->marker = marker;
  cb->marker_flag = marker->flag;
  cb->framenr = user->framenr;

  if (compact) {
    uiBlock *block = layout->block();

    blender::StringRef tip;
    if (cb->marker_flag & MARKER_DISABLED) {
      tip = TIP_("Marker is disabled at current frame");
    }
    else {
      tip = TIP_("Marker is enabled at current frame");
    }

    uiBut *bt = uiDefIconButBitI(block,
                                 ButType::ToggleN,
                                 MARKER_DISABLED,
                                 0,
                                 ICON_HIDE_OFF,
                                 0,
                                 0,
                                 UI_UNIT_X,
                                 UI_UNIT_Y,
                                 &cb->marker_flag,
                                 0,
                                 0,
                                 tip);
    UI_but_funcN_set(bt, marker_update_cb, cb, nullptr);
    UI_but_drawflag_enable(bt, UI_BUT_ICON_REVERSE);
  }
  else {
    int width, height;

    BKE_movieclip_get_size(clip, user, &width, &height);

    if (track->flag & TRACK_LOCKED) {
      layout->active_set(false);
      uiBlock *block = layout->absolute_block();
      uiDefBut(block,
               ButType::Label,
               0,
               IFACE_("Track is locked"),
               0,
               0,
               UI_UNIT_X * 15.0f,
               UI_UNIT_Y,
               nullptr,
               0,
               0,
               "");
      MEM_freeN(cb);
      return;
    }

    float pat_min[2], pat_max[2];
    float pat_dim[2], search_dim[2], search_pos[2];

    BKE_tracking_marker_pattern_minmax(marker, pat_min, pat_max);

    sub_v2_v2v2(pat_dim, pat_max, pat_min);
    sub_v2_v2v2(search_dim, marker->search_max, marker->search_min);

    add_v2_v2v2(search_pos, marker->search_max, marker->search_min);
    mul_v2_fl(search_pos, 0.5);

    to_pixel_space(cb->marker_pos, marker->pos, width, height);
    to_pixel_space(cb->marker_pat, pat_dim, width, height);
    to_pixel_space(cb->marker_search, search_dim, width, height);
    to_pixel_space(cb->marker_search_pos, search_pos, width, height);
    to_pixel_space(cb->track_offset, track->offset, width, height);

    cb->marker_flag = marker->flag;

    uiBlock *block = layout->absolute_block();
    UI_block_func_handle_set(block, marker_block_handler, cb);
    UI_block_funcN_set(block, marker_update_cb, cb, nullptr);

    blender::StringRef tip;
    int step = 100;
    int digits = 2;

    if (cb->marker_flag & MARKER_DISABLED) {
      tip = TIP_("Marker is disabled at current frame");
    }
    else {
      tip = TIP_("Marker is enabled at current frame");
    }

    uiDefButBitI(block,
                 ButType::CheckboxN,
                 MARKER_DISABLED,
                 B_MARKER_FLAG,
                 IFACE_("Enabled"),
                 0.5 * UI_UNIT_X,
                 9.5 * UI_UNIT_Y,
                 7.25 * UI_UNIT_X,
                 UI_UNIT_Y,
                 &cb->marker_flag,
                 0,
                 0,
                 tip);

    uiLayout *col = &layout->column(true);
    col->active_set((cb->marker_flag & MARKER_DISABLED) == 0);

    block = col->absolute_block();
    UI_block_align_begin(block);

    uiDefBut(block,
             ButType::Label,
             0,
             IFACE_("Position:"),
             0,
             10 * UI_UNIT_Y,
             15 * UI_UNIT_X,
             UI_UNIT_Y,
             nullptr,
             0,
             0,
             "");
    uiBut *bt = uiDefButF(block,
                          ButType::Num,
                          B_MARKER_POS,
                          IFACE_("X:"),
                          0.5 * UI_UNIT_X,
                          9 * UI_UNIT_Y,
                          7.25 * UI_UNIT_X,
                          UI_UNIT_Y,
                          &cb->marker_pos[0],
                          -10 * width,
                          10.0 * width,
                          TIP_("X-position of marker at frame in screen coordinates"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_POS,
                   IFACE_("Y:"),
                   8.25 * UI_UNIT_X,
                   9 * UI_UNIT_Y,
                   7.25 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_pos[1],
                   -10 * height,
                   10.0 * height,
                   TIP_("Y-position of marker at frame in screen coordinates"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);

    uiDefBut(block,
             ButType::Label,
             0,
             IFACE_("Offset:"),
             0,
             8 * UI_UNIT_Y,
             15 * UI_UNIT_X,
             UI_UNIT_Y,
             nullptr,
             0,
             0,
             "");
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_OFFSET,
                   IFACE_("X:"),
                   0.5 * UI_UNIT_X,
                   7 * UI_UNIT_Y,
                   7.25 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->track_offset[0],
                   -10 * width,
                   10.0 * width,
                   TIP_("X-offset to parenting point"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_OFFSET,
                   IFACE_("Y:"),
                   8.25 * UI_UNIT_X,
                   7 * UI_UNIT_Y,
                   7.25 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->track_offset[1],
                   -10 * height,
                   10.0 * height,
                   TIP_("Y-offset to parenting point"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);

    uiDefBut(block,
             ButType::Label,
             0,
             IFACE_("Pattern Area:"),
             0,
             6 * UI_UNIT_Y,
             15 * UI_UNIT_X,
             UI_UNIT_Y,
             nullptr,
             0,
             0,
             "");
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_PAT_DIM,
                   IFACE_("Width:"),
                   0.5 * UI_UNIT_X,
                   5 * UI_UNIT_Y,
                   15 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_pat[0],
                   3.0f,
                   10.0 * width,
                   TIP_("Width of marker's pattern in screen coordinates"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_PAT_DIM,
                   IFACE_("Height:"),
                   0.5 * UI_UNIT_X,
                   4 * UI_UNIT_Y,
                   15 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_pat[1],
                   3.0f,
                   10.0 * height,
                   TIP_("Height of marker's pattern in screen coordinates"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);

    uiDefBut(block,
             ButType::Label,
             0,
             IFACE_("Search Area:"),
             0,
             3 * UI_UNIT_Y,
             15 * UI_UNIT_X,
             UI_UNIT_Y,
             nullptr,
             0,
             0,
             "");
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_SEARCH_POS,
                   IFACE_("X:"),
                   0.5 * UI_UNIT_X,
                   2 * UI_UNIT_Y,
                   7.25 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_search_pos[0],
                   -width,
                   width,
                   TIP_("X-position of search at frame relative to marker's position"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_SEARCH_POS,
                   IFACE_("Y:"),
                   8.25 * UI_UNIT_X,
                   2 * UI_UNIT_Y,
                   7.25 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_search_pos[1],
                   -height,
                   height,
                   TIP_("Y-position of search at frame relative to marker's position"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_SEARCH_DIM,
                   IFACE_("Width:"),
                   0.5 * UI_UNIT_X,
                   1 * UI_UNIT_Y,
                   15 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_search[0],
                   3.0f,
                   10.0 * width,
                   TIP_("Width of marker's search in screen coordinates"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);
    bt = uiDefButF(block,
                   ButType::Num,
                   B_MARKER_SEARCH_DIM,
                   IFACE_("Height:"),
                   0.5 * UI_UNIT_X,
                   0 * UI_UNIT_Y,
                   15 * UI_UNIT_X,
                   UI_UNIT_Y,
                   &cb->marker_search[1],
                   3.0f,
                   10.0 * height,
                   TIP_("Height of marker's search in screen coordinates"));
    UI_but_number_step_size_set(bt, step);
    UI_but_number_precision_set(bt, digits);

    UI_block_align_end(block);
  }
}

/********************* Footage Information Template ************************/

void uiTemplateMovieclipInformation(uiLayout *layout,
                                    PointerRNA *ptr,
                                    const StringRefNull propname,
                                    PointerRNA *userptr)
{
  if (!ptr->data) {
    return;
  }

  PropertyRNA *prop = RNA_struct_find_property(ptr, propname.c_str());
  if (!prop) {
    printf("%s: property not found: %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  if (RNA_property_type(prop) != PROP_POINTER) {
    printf("%s: expected pointer property for %s.%s\n",
           __func__,
           RNA_struct_identifier(ptr->type),
           propname.c_str());
    return;
  }

  PointerRNA clipptr = RNA_property_pointer_get(ptr, prop);
  MovieClip *clip = static_cast<MovieClip *>(clipptr.data);
  MovieClipUser *user = static_cast<MovieClipUser *>(userptr->data);

  uiLayout *col = &layout->column(false);
  col->alignment_set(blender::ui::LayoutAlign::Right);

  /* NOTE: Put the frame to cache. If the panel is drawn, the display will also be shown, as well
   * as metadata panel. So if the cache is skipped here it is not really a memory saver, but
   * skipping the cache could lead to a performance impact depending on the order in which panels
   * and the main area is drawn. Basically, if it is this template drawn first and then the main
   * area it will lead to frame read and processing happening twice. */
  ImBuf *ibuf = BKE_movieclip_get_ibuf_flag(clip, user, clip->flag, 0);

  int width, height;
  /* Display frame dimensions, channels number and buffer type. */
  BKE_movieclip_get_size(clip, user, &width, &height);

  char str[1024];
  size_t ofs = 0;
  ofs += BLI_snprintf_utf8_rlen(str + ofs, sizeof(str) - ofs, RPT_("%d x %d"), width, height);

  if (ibuf) {
    if (ibuf->float_buffer.data) {
      if (ibuf->channels != 4) {
        ofs += BLI_snprintf_utf8_rlen(
            str + ofs, sizeof(str) - ofs, RPT_(", %d float channel(s)"), ibuf->channels);
      }
      else if (ibuf->planes == R_IMF_PLANES_RGBA) {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(", RGBA float"), sizeof(str) - ofs);
      }
      else {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(", RGB float"), sizeof(str) - ofs);
      }
    }
    else {
      if (ibuf->planes == R_IMF_PLANES_RGBA) {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(", RGBA byte"), sizeof(str) - ofs);
      }
      else {
        ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(", RGB byte"), sizeof(str) - ofs);
      }
    }

    if (clip->anim != nullptr) {
      float fps = MOV_get_fps(clip->anim);
      if (fps > 0.0f) {
        ofs += BLI_snprintf_utf8_rlen(str + ofs, sizeof(str) - ofs, RPT_(", %.2f fps"), fps);
      }
    }
  }
  else {
    ofs += BLI_strncpy_utf8_rlen(str + ofs, RPT_(", failed to load"), sizeof(str) - ofs);
  }
  UNUSED_VARS(ofs);

  col->label(str, ICON_NONE);

  /* Display current frame number. */
  int framenr = BKE_movieclip_remap_scene_to_clip_frame(clip, user->framenr);
  if (framenr <= clip->len) {
    SNPRINTF_UTF8(str, RPT_("Frame: %d / %d"), framenr, clip->len);
  }
  else {
    SNPRINTF_UTF8(str, RPT_("Frame: - / %d"), clip->len);
  }
  col->label(str, ICON_NONE);

  /* Display current file name if it's a sequence clip. */
  if (clip->source == MCLIP_SRC_SEQUENCE) {
    char filepath[FILE_MAX];
    const char *file;

    if (framenr <= clip->len) {
      BKE_movieclip_filepath_for_frame(clip, user, filepath);
      file = BLI_path_basename(filepath);
    }
    else {
      file = "-";
    }

    SNPRINTF(str, RPT_("File: %s"), file);

    col->label(str, ICON_NONE);
  }

  IMB_freeImBuf(ibuf);
}

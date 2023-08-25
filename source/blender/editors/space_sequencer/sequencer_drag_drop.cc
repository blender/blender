/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sound_types.h"

#include "BLI_blenlib.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"

#include "SEQ_channels.h"
#include "SEQ_iterator.h"
#include "SEQ_sequencer.h"
#include "SEQ_transform.h"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "GPU_immediate.h"
#include "GPU_matrix.h"

#include "ED_screen.hh"
#include "ED_transform.hh"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "WM_api.hh"
#include "WM_types.hh"

/* For querying audio files. */
#ifdef WITH_AUDASPACE
#  include "BKE_sound.h"
#  include <AUD_Sound.h>
#  include <AUD_Special.h>
#endif

/* Own include. */
#include "sequencer_intern.h"

struct SeqDropCoords {
  float start_frame, channel;
  int strip_len, channel_len;
  float playback_rate;
  bool in_use = false;
  bool has_read_mouse_pos = false;
  bool is_intersecting;
  bool use_snapping;
  float snap_point_x;
  uint8_t type;
};

/* The current drag and drop API doesn't allow us to easily pass along the
 * required custom data to all callbacks that need it. Especially when
 * preloading data on drag start.
 * Therefore we will for now use a global variable for this.
 */
static SeqDropCoords g_drop_coords{};

static void generic_poll_operations(const wmEvent *event, uint8_t type)
{
  g_drop_coords.type = type;
  /* We purposely ignore the snapping tool setting here as currently other drag&drop operators only
   * snaps when holding down Ctrl. */
  g_drop_coords.use_snapping = event->modifier & KM_CTRL;
}

static bool image_drop_poll(bContext * /*C*/, wmDrag *drag, const wmEvent *event)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, 0, FILE_TYPE_IMAGE)) {
      generic_poll_operations(event, TH_SEQ_IMAGE);
      return true;
    }
  }

  if (WM_drag_is_ID_type(drag, ID_IM)) {
    generic_poll_operations(event, TH_SEQ_IMAGE);
    return true;
  }

  return false;
}

static bool is_movie(wmDrag *drag)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, 0, FILE_TYPE_MOVIE)) {
      return true;
    }
  }
  if (WM_drag_is_ID_type(drag, ID_MC)) {
    return true;
  }
  return false;
}

static bool movie_drop_poll(bContext * /*C*/, wmDrag *drag, const wmEvent *event)
{
  if (is_movie(drag)) {
    generic_poll_operations(event, TH_SEQ_MOVIE);
    return true;
  }

  return false;
}

static bool is_sound(wmDrag *drag)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, 0, FILE_TYPE_SOUND)) {
      return true;
    }
  }
  if (WM_drag_is_ID_type(drag, ID_SO)) {
    return true;
  }
  return false;
}

static bool sound_drop_poll(bContext * /*C*/, wmDrag *drag, const wmEvent *event)
{
  if (is_sound(drag)) {
    generic_poll_operations(event, TH_SEQ_AUDIO);
    return true;
  }

  return false;
}

static float update_overlay_strip_position_data(bContext *C, const int mval[2])
{
  SeqDropCoords *coords = &g_drop_coords;
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_scene(C);
  int hand;
  View2D *v2d = &region->v2d;

  /* Update the position were we would place the strip if we complete the drag and drop action.
   */
  UI_view2d_region_to_view(v2d, mval[0], mval[1], &coords->start_frame, &coords->channel);
  coords->start_frame = roundf(coords->start_frame);
  if (coords->channel < 1.0f) {
    coords->channel = 1;
  }

  float start_frame = coords->start_frame;
  float end_frame;
  float strip_len;

  if (coords->playback_rate != 0.0f) {
    float scene_playback_rate = float(scene->r.frs_sec) / scene->r.frs_sec_base;
    strip_len = coords->strip_len / (coords->playback_rate / scene_playback_rate);
  }
  else {
    strip_len = coords->strip_len;
  }

  end_frame = coords->start_frame + strip_len;

  if (coords->use_snapping) {
    /* Do snapping via the existing transform code. */
    int snap_delta;
    float snap_frame;
    bool valid_snap;

    valid_snap = ED_transform_snap_sequencer_to_closest_strip_calc(
        scene, region, start_frame, end_frame, &snap_delta, &snap_frame);

    if (valid_snap) {
      /* We snapped onto something! */
      start_frame += snap_delta;
      coords->start_frame = start_frame;
      end_frame = start_frame + strip_len;
      coords->snap_point_x = snap_frame;
    }
    else {
      /* Nothing was snapped to, disable snap drawing. */
      coords->use_snapping = false;
    }
  }

  if (strip_len < 1) {
    /* Only check if there is a strip already under the mouse cursor. */
    coords->is_intersecting = find_nearest_seq(scene, &region->v2d, &hand, mval);
  }
  else {
    /* Check if there is a strip that would intersect with the new strip(s). */
    coords->is_intersecting = false;
    Sequence dummy_seq{};
    dummy_seq.machine = coords->channel;
    dummy_seq.start = coords->start_frame;
    dummy_seq.len = coords->strip_len;
    dummy_seq.speed_factor = 1.0f;
    dummy_seq.media_playback_rate = coords->playback_rate;
    dummy_seq.flag = SEQ_AUTO_PLAYBACK_RATE;
    Editing *ed = SEQ_editing_ensure(scene);

    for (int i = 0; i < coords->channel_len && !coords->is_intersecting; i++) {
      coords->is_intersecting = SEQ_transform_test_overlap(scene, ed->seqbasep, &dummy_seq);
      dummy_seq.machine++;
    }
  }

  return strip_len;
}

static void sequencer_drop_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, 0);
  /* ID dropped. */
  if (id != nullptr) {
    const ID_Type id_type = GS(id->name);
    if (id_type == ID_IM) {
      Image *ima = (Image *)id;
      PointerRNA itemptr;
      char dir[FILE_MAX], file[FILE_MAX];
      BLI_path_split_dir_file(ima->filepath, dir, sizeof(dir), file, sizeof(file));
      RNA_string_set(drop->ptr, "directory", dir);
      RNA_collection_clear(drop->ptr, "files");
      RNA_collection_add(drop->ptr, "files", &itemptr);
      RNA_string_set(&itemptr, "name", file);
    }
    else if (id_type == ID_MC) {
      MovieClip *clip = (MovieClip *)id;
      RNA_string_set(drop->ptr, "filepath", clip->filepath);
      RNA_struct_property_unset(drop->ptr, "name");
    }
    else if (id_type == ID_SO) {
      bSound *sound = (bSound *)id;
      RNA_string_set(drop->ptr, "filepath", sound->filepath);
      RNA_struct_property_unset(drop->ptr, "name");
    }

    return;
  }

  const char *path = WM_drag_get_path(drag);
  /* Path dropped. */
  if (path) {
    if (RNA_struct_find_property(drop->ptr, "filepath")) {
      RNA_string_set(drop->ptr, "filepath", path);
    }
    if (RNA_struct_find_property(drop->ptr, "directory")) {
      PointerRNA itemptr;
      char dir[FILE_MAX], file[FILE_MAX];

      BLI_path_split_dir_file(path, dir, sizeof(dir), file, sizeof(file));

      RNA_string_set(drop->ptr, "directory", dir);

      RNA_collection_clear(drop->ptr, "files");
      RNA_collection_add(drop->ptr, "files", &itemptr);
      RNA_string_set(&itemptr, "name", file);
    }
  }

  if (g_drop_coords.in_use) {
    if (!g_drop_coords.has_read_mouse_pos) {
      /* We didn't read the mouse position, so we need to do it manually here. */
      int xy[2];
      wmWindow *win = CTX_wm_window(C);
      xy[0] = win->eventstate->xy[0];
      xy[1] = win->eventstate->xy[1];

      ARegion *region = CTX_wm_region(C);
      int mval[2];
      /* Convert mouse coordinates to region local coordinates. */
      mval[0] = xy[0] - region->winrct.xmin;
      mval[1] = xy[1] - region->winrct.ymin;

      update_overlay_strip_position_data(C, mval);
    }

    RNA_int_set(drop->ptr, "frame_start", g_drop_coords.start_frame);
    RNA_int_set(drop->ptr, "channel", g_drop_coords.channel);
    RNA_boolean_set(drop->ptr, "overlap_shuffle_override", true);
  }
  else {
    /* We are dropped inside the preview region. Put the strip on top of the
     * current displayed frame. */
    Scene *scene = CTX_data_scene(C);
    Editing *ed = SEQ_editing_ensure(scene);
    ListBase *seqbase = SEQ_active_seqbase_get(ed);
    ListBase *channels = SEQ_channels_displayed_get(ed);
    SpaceSeq *sseq = CTX_wm_space_seq(C);

    SeqCollection *strips = SEQ_query_rendered_strips(
        scene, channels, seqbase, scene->r.cfra, sseq->chanshown);

    /* Get the top most strip channel that is in view. */
    Sequence *seq;
    int max_channel = -1;
    SEQ_ITERATOR_FOREACH (seq, strips) {
      max_channel = max_ii(seq->machine, max_channel);
    }

    if (max_channel != -1) {
      RNA_int_set(drop->ptr, "channel", max_channel);
    }
    SEQ_collection_free(strips);
  }
}

static void get_drag_path(const bContext *C, wmDrag *drag, char r_path[FILE_MAX])
{
  ID *id = WM_drag_get_local_ID_or_import_from_asset(C, drag, 0);
  /* ID dropped. */
  if (id != nullptr) {
    const ID_Type id_type = GS(id->name);
    if (id_type == ID_IM) {
      Image *ima = (Image *)id;
      BLI_strncpy(r_path, ima->filepath, FILE_MAX);
    }
    else if (id_type == ID_MC) {
      MovieClip *clip = (MovieClip *)id;
      BLI_strncpy(r_path, clip->filepath, FILE_MAX);
    }
    else if (id_type == ID_SO) {
      bSound *sound = (bSound *)id;
      BLI_strncpy(r_path, sound->filepath, FILE_MAX);
    }
    BLI_path_abs(r_path, BKE_main_blendfile_path_from_global());
  }
  else {
    BLI_strncpy(r_path, WM_drag_get_path(drag), FILE_MAX);
  }
}

static void draw_seq_in_view(bContext *C, wmWindow * /*win*/, wmDrag *drag, const int xy[2])
{
  SeqDropCoords *coords = &g_drop_coords;
  if (!coords->in_use) {
    return;
  }

  ARegion *region = CTX_wm_region(C);
  int mval[2];
  /* Convert mouse coordinates to region local coordinates. */
  mval[0] = xy[0] - region->winrct.xmin;
  mval[1] = xy[1] - region->winrct.ymin;

  float strip_len = update_overlay_strip_position_data(C, mval);

  GPU_matrix_push();
  UI_view2d_view_ortho(&region->v2d);

  /* Sometimes the active theme is not the sequencer theme, e.g. when an operator invokes the
   * file browser. This makes sure we get the right color values for the theme. */
  bThemeState theme_state;
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_SEQ, RGN_TYPE_WINDOW);

  if (coords->use_snapping) {
    ED_draw_sequencer_snap_point(region, coords->snap_point_x);
  }

  /* Init GPU drawing. */
  GPU_line_width(2.0f);
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);

  /* Draw strips. The code here is taken from sequencer_draw. */
  float x1 = coords->start_frame;
  float x2 = coords->start_frame + floorf(strip_len);
  float strip_color[3];
  uchar text_color[4] = {255, 255, 255, 255};
  float pixelx = BLI_rctf_size_x(&region->v2d.cur) / BLI_rcti_size_x(&region->v2d.mask);
  float pixely = BLI_rctf_size_y(&region->v2d.cur) / BLI_rcti_size_y(&region->v2d.mask);

  for (int i = 0; i < coords->channel_len; i++) {
    float y1 = floorf(coords->channel) + i + SEQ_STRIP_OFSBOTTOM;
    float y2 = floorf(coords->channel) + i + SEQ_STRIP_OFSTOP;

    if (coords->type == TH_SEQ_MOVIE && i == 0 && coords->channel_len > 1) {
      /* Assume only video strips occupies two channels.
       * One for video and the other for audio.
       * The audio channel is added first.
       */
      UI_GetThemeColor3fv(TH_SEQ_AUDIO, strip_color);
    }
    else {
      UI_GetThemeColor3fv(coords->type, strip_color);
    }

    immUniformColor3fvAlpha(strip_color, 0.8f);
    immRectf(pos, x1, y1, x2, y2);

    if (coords->is_intersecting) {
      strip_color[0] = 1.0f;
      strip_color[1] = strip_color[2] = 0.3f;
    }
    else {
      if (coords->channel_len - 1 == i) {
        text_color[0] = text_color[1] = text_color[2] = 255;
        UI_GetThemeColor3fv(TH_SEQ_ACTIVE, strip_color);
      }
      else {
        text_color[0] = text_color[1] = text_color[2] = 10;
        UI_GetThemeColor3fv(TH_SEQ_SELECTED, strip_color);
      }
    }

    /* Draw a 2 pixel border around the strip. */
    immUniformColor3fvAlpha(strip_color, 0.8f);
    /* Left */
    immRectf(pos, x1 - pixelx, y1, x1 + pixelx, y2);
    /* Bottom */
    immRectf(pos, x1 - pixelx, y1, x2 + pixelx, y1 + 2 * pixely);
    /* Right */
    immRectf(pos, x2 - pixelx, y1, x2 + pixelx, y2);
    /* Top */
    immRectf(pos, x1 - pixelx, y2 - 2 * pixely, x2 + pixelx, y2);

    float handle_size = 8.0f; /* SEQ_HANDLE_SIZE */

    /* Calculate height needed for drawing text on strip. */
    float text_margin_y = y2 - min_ff(0.40f, 20 * UI_SCALE_FAC * pixely);
    float text_margin_x = 2.0f * (pixelx * handle_size) * U.pixelsize;

    rctf rect;
    rect.xmin = x1 + text_margin_x;
    rect.ymin = text_margin_y;
    rect.xmax = x2 - text_margin_x;
    rect.ymax = y2;

    if (rect.xmax <= rect.xmin) {
      /* Exit early and skip text drawing if the strip doesn't have any space to put the text
       * into.
       */
      break;
    }

    SpaceSeq *sseq = CTX_wm_space_seq(C);
    const char *text_sep = " | ";
    const char *text_array[5];
    char text_display[FILE_MAX];
    char filename[FILE_MAX];
    char path[FILE_MAX];
    char strip_duration_text[16];
    int len_text_arr = 0;

    get_drag_path(C, drag, path);

    if (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_NAME) {
      BLI_path_split_file_part(path, filename, FILE_MAX);
      text_array[len_text_arr++] = filename;
    }

    if (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_SOURCE) {
      Main *bmain = CTX_data_main(C);
      BLI_path_rel(path, BKE_main_blendfile_path(bmain));
      text_array[len_text_arr++] = text_sep;
      text_array[len_text_arr++] = path;
    }

    if (sseq->timeline_overlay.flag & SEQ_TIMELINE_SHOW_STRIP_DURATION) {
      SNPRINTF(strip_duration_text, "%d", int(x2 - x1));
      text_array[len_text_arr++] = text_sep;
      text_array[len_text_arr++] = strip_duration_text;
    }

    BLI_assert(len_text_arr <= ARRAY_SIZE(text_array));

    BLI_string_join_array(text_display, FILE_MAX, text_array, len_text_arr);

    UI_view2d_text_cache_add_rectf(
        &region->v2d, &rect, text_display, strlen(text_display), text_color);
  }

  /* Clean after drawing up. */
  UI_Theme_Restore(&theme_state);
  GPU_matrix_pop();
  immUnbindProgram();
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);

  UI_view2d_text_cache_draw(region);
}

static bool generic_drop_draw_handling(wmDropBox *drop)
{
  SeqDropCoords *coords = static_cast<SeqDropCoords *>(drop->draw_data);
  if (coords && coords->in_use) {
    return true;
  }

  coords = &g_drop_coords;
  drop->draw_data = static_cast<void *>(&g_drop_coords);
  coords->in_use = true;

  return false;
}

struct DropJobData {
  /**
   * This is practically always a `filepath`, however that isn't a requirement
   * for drag-and-drop, so keep the name generic.
   */
  char path[FILE_MAX];
  bool only_audio;
  float scene_fps;
};

static void prefetch_data_fn(void *custom_data,
                             bool * /*stop*/,
                             bool * /*do_update*/,
                             float * /*progress*/)
{
  DropJobData *job_data = (DropJobData *)custom_data;

  if (job_data->only_audio) {
#ifdef WITH_AUDASPACE
    /* Get the sound file length */
    AUD_Sound *sound = AUD_Sound_file(job_data->path);
    if (sound != nullptr) {

      AUD_SoundInfo info = AUD_getInfo(sound);
      if ((eSoundChannels)info.specs.channels != SOUND_CHANNELS_INVALID) {
        g_drop_coords.strip_len = max_ii(1, round((info.length) * job_data->scene_fps));
      }
      AUD_Sound_free(sound);
      return;
    }
#endif
  }

  char colorspace[64] = "\0"; /* 64 == MAX_COLORSPACE_NAME length. */
  anim *anim = openanim(job_data->path, IB_rect, 0, colorspace);

  if (anim != nullptr) {
    g_drop_coords.strip_len = IMB_anim_get_duration(anim, IMB_TC_NONE);
    short frs_sec;
    float frs_sec_base;
    if (IMB_anim_get_fps(anim, &frs_sec, &frs_sec_base, true)) {
      g_drop_coords.playback_rate = float(frs_sec) / frs_sec_base;
    }
    else {
      g_drop_coords.playback_rate = 0;
    }
    IMB_free_anim(anim);
#ifdef WITH_AUDASPACE
    /* Try to load sound and see if the video has a sound channel. */
    AUD_Sound *sound = AUD_Sound_file(job_data->path);
    if (sound != nullptr) {

      AUD_SoundInfo info = AUD_getInfo(sound);
      if ((eSoundChannels)info.specs.channels != SOUND_CHANNELS_INVALID) {
        g_drop_coords.channel_len = 2;
      }
      AUD_Sound_free(sound);
    }
#endif
  }
}

static void free_prefetch_data_fn(void *custom_data)
{
  DropJobData *job_data = (DropJobData *)custom_data;
  MEM_freeN(job_data);
}

static void start_audio_video_job(bContext *C, wmDrag *drag, bool only_audio)
{
  g_drop_coords.strip_len = 0;
  g_drop_coords.channel_len = 1;

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  Scene *scene = CTX_data_scene(C);

  wmJob *wm_job = WM_jobs_get(
      wm, win, nullptr, "Load Previews", eWM_JobFlag(0), WM_JOB_TYPE_SEQ_DRAG_DROP_PREVIEW);

  DropJobData *job_data = (DropJobData *)MEM_mallocN(sizeof(DropJobData),
                                                     "SeqDragDropPreviewData");
  get_drag_path(C, drag, job_data->path);

  job_data->only_audio = only_audio;
  job_data->scene_fps = FPS;

  WM_jobs_customdata_set(wm_job, job_data, free_prefetch_data_fn);
  WM_jobs_timer(wm_job, 0.1, NC_WINDOW, NC_WINDOW);
  WM_jobs_callbacks(wm_job, prefetch_data_fn, nullptr, nullptr, nullptr);

  WM_jobs_start(wm, wm_job);
}

static void video_prefetch(bContext *C, wmDrag *drag)
{
  if (is_movie(drag)) {
    start_audio_video_job(C, drag, false);
  }
}

static void audio_prefetch(bContext *C, wmDrag *drag)
{
  if (is_sound(drag)) {
    start_audio_video_job(C, drag, true);
  }
}

static void movie_drop_draw_activate(wmDropBox *drop, wmDrag * /*drag*/)
{
  if (generic_drop_draw_handling(drop)) {
    return;
  }
}

static void sound_drop_draw_activate(wmDropBox *drop, wmDrag * /*drag*/)
{
  if (generic_drop_draw_handling(drop)) {
    return;
  }
}

static void image_drop_draw_activate(wmDropBox *drop, wmDrag * /*drag*/)
{
  if (generic_drop_draw_handling(drop)) {
    return;
  }

  SeqDropCoords *coords = static_cast<SeqDropCoords *>(drop->draw_data);
  coords->strip_len = DEFAULT_IMG_STRIP_LENGTH;
  coords->channel_len = 1;
}

static void sequencer_drop_draw_deactivate(wmDropBox *drop, wmDrag * /*drag*/)
{
  SeqDropCoords *coords = static_cast<SeqDropCoords *>(drop->draw_data);
  if (coords) {
    coords->in_use = false;
    coords->has_read_mouse_pos = false;
    drop->draw_data = nullptr;
  }
}

static void nop_draw_droptip_fn(bContext * /*C*/,
                                wmWindow * /*win*/,
                                wmDrag * /*drag*/,
                                const int /*xy*/[2])
{
  /* Do nothing in here.
   * This is to prevent the default drag and drop mouse overlay to be drawn.
   */
}

/* This region dropbox definition. */
static void sequencer_dropboxes_add_to_lb(ListBase *lb)
{
  wmDropBox *drop;
  drop = WM_dropbox_add(
      lb, "SEQUENCER_OT_image_strip_add", image_drop_poll, sequencer_drop_copy, nullptr, nullptr);
  drop->draw_droptip = nop_draw_droptip_fn;
  drop->draw_in_view = draw_seq_in_view;
  drop->draw_activate = image_drop_draw_activate;
  drop->draw_deactivate = sequencer_drop_draw_deactivate;

  drop->on_drag_start = audio_prefetch;

  drop = WM_dropbox_add(
      lb, "SEQUENCER_OT_movie_strip_add", movie_drop_poll, sequencer_drop_copy, nullptr, nullptr);
  drop->draw_droptip = nop_draw_droptip_fn;
  drop->draw_in_view = draw_seq_in_view;
  drop->draw_activate = movie_drop_draw_activate;
  drop->draw_deactivate = sequencer_drop_draw_deactivate;

  drop->on_drag_start = video_prefetch;

  drop = WM_dropbox_add(
      lb, "SEQUENCER_OT_sound_strip_add", sound_drop_poll, sequencer_drop_copy, nullptr, nullptr);
  drop->draw_droptip = nop_draw_droptip_fn;
  drop->draw_in_view = draw_seq_in_view;
  drop->draw_activate = sound_drop_draw_activate;
  drop->draw_deactivate = sequencer_drop_draw_deactivate;
}

static bool image_drop_preview_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, 0, FILE_TYPE_IMAGE)) {
      return true;
    }
  }

  return WM_drag_is_ID_type(drag, ID_IM);
}

static bool movie_drop_preview_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, 0, FILE_TYPE_MOVIE)) {
      return true;
    }
  }

  return WM_drag_is_ID_type(drag, ID_MC);
}

static bool sound_drop_preview_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (ELEM(file_type, 0, FILE_TYPE_SOUND)) {
      return true;
    }
  }

  return WM_drag_is_ID_type(drag, ID_SO);
}

static void sequencer_preview_dropboxes_add_to_lb(ListBase *lb)
{
  WM_dropbox_add(lb,
                 "SEQUENCER_OT_image_strip_add",
                 image_drop_preview_poll,
                 sequencer_drop_copy,
                 nullptr,
                 nullptr);

  WM_dropbox_add(lb,
                 "SEQUENCER_OT_movie_strip_add",
                 movie_drop_preview_poll,
                 sequencer_drop_copy,
                 nullptr,
                 nullptr);

  WM_dropbox_add(lb,
                 "SEQUENCER_OT_sound_strip_add",
                 sound_drop_preview_poll,
                 sequencer_drop_copy,
                 nullptr,
                 nullptr);
}

void sequencer_dropboxes()
{
  ListBase *lb = WM_dropboxmap_find("Sequencer", SPACE_SEQ, RGN_TYPE_WINDOW);
  sequencer_dropboxes_add_to_lb(lb);
  lb = WM_dropboxmap_find("Sequencer", SPACE_SEQ, RGN_TYPE_PREVIEW);
  sequencer_preview_dropboxes_add_to_lb(lb);
}

/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sound_types.h"

#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"

#include "BKE_context.hh"
#include "BKE_file_handler.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"

#include "SEQ_channels.hh"
#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_transform.hh"

#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "GPU_matrix.hh"
#include "GPU_state.hh"

#include "ED_screen.hh"
#include "ED_transform.hh"

#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "WM_api.hh"
#include "WM_types.hh"

/* For querying audio files. */
#ifdef WITH_AUDASPACE
#  include "BKE_sound.hh"
#  include <AUD_Sound.h>
#  include <AUD_Special.h>
#endif

/* Own include. */
#include "sequencer_intern.hh"
#include "sequencer_strips_batch.hh"

namespace blender::ed::vse {

struct SeqDropCoords {
  float start_frame, channel;
  int strip_len, channel_len;
  float playback_rate;
  float audio_length;
  bool only_audio = false;
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

/* While drag-and-drop in the sequencer, the internal drop-box implementation allows to have a drop
 * preview of the file dragged. This checks when drag-and-drop is done with a single file, and when
 * only a expected `file_handler` can be used, so internal drop-box can be used instead of the
 * `file_handler`. */
static bool test_single_file_handler_poll(const bContext *C, wmDrag *drag, StringRef file_handler)
{
  const auto paths = WM_drag_get_paths(drag);
  auto file_handlers = bke::file_handlers_poll_file_drop(C, paths);
  return paths.size() == 1 && file_handlers.size() == 1 &&
         file_handler == file_handlers[0]->idname;
}

static bool image_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (file_type == FILE_TYPE_IMAGE &&
        test_single_file_handler_poll(C, drag, "SEQUENCER_FH_image_strip"))
    {
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
    if (file_type == FILE_TYPE_MOVIE) {
      return true;
    }
  }
  if (WM_drag_is_ID_type(drag, ID_MC)) {
    return true;
  }
  return false;
}

static bool movie_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (is_movie(drag) && (drag->type != WM_DRAG_PATH ||
                         test_single_file_handler_poll(C, drag, "SEQUENCER_FH_movie_strip")))
  {
    generic_poll_operations(event, TH_SEQ_MOVIE);
    return true;
  }

  return false;
}

static bool is_sound(wmDrag *drag)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (file_type == FILE_TYPE_SOUND) {
      return true;
    }
  }
  if (WM_drag_is_ID_type(drag, ID_SO)) {
    return true;
  }
  return false;
}

static bool sound_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event)
{
  if (is_sound(drag) && (drag->type != WM_DRAG_PATH ||
                         test_single_file_handler_poll(C, drag, "SEQUENCER_FH_sound_strip")))
  {
    generic_poll_operations(event, TH_SEQ_AUDIO);
    return true;
  }

  return false;
}

static float update_overlay_strip_position_data(bContext *C, const int mval[2])
{
  SeqDropCoords *coords = &g_drop_coords;
  ARegion *region = CTX_wm_region(C);
  Scene *scene = CTX_data_sequencer_scene(C);
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
  else if (coords->only_audio) {
    float scene_playback_rate = float(scene->r.frs_sec) / scene->r.frs_sec_base;
    strip_len = coords->audio_length * scene_playback_rate;
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

    valid_snap = transform::snap_sequencer_to_closest_strip_calc(
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

  /* Check if there is a strip that would intersect with the new strip(s). */
  coords->is_intersecting = false;
  Strip dummy_strip{};
  seq::strip_channel_set(&dummy_strip, coords->channel);
  dummy_strip.start = coords->start_frame;
  dummy_strip.len = coords->strip_len;
  dummy_strip.speed_factor = 1.0f;
  dummy_strip.media_playback_rate = coords->playback_rate;
  dummy_strip.flag = SEQ_AUTO_PLAYBACK_RATE;
  Editing *ed = seq::editing_ensure(scene);

  for (int i = 0; i < coords->channel_len && !coords->is_intersecting; i++) {
    coords->is_intersecting = seq::transform_test_overlap(
        scene, ed->current_strips(), &dummy_strip);
    seq::strip_channel_set(&dummy_strip, dummy_strip.channel + 1);
  }

  return strip_len;
}

static void sequencer_drop_copy(bContext *C, wmDrag *drag, wmDropBox *drop)
{
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
    RNA_boolean_set(drop->ptr, "skip_locked_or_muted_channels", false);
  }
  else {
    /* We are dropped inside the preview region. Put the strip on top of the
     * current displayed frame. */
    Scene *scene = CTX_data_sequencer_scene(C);
    Editing *ed = seq::editing_ensure(scene);
    ListBase *seqbase = seq::active_seqbase_get(ed);
    ListBase *channels = seq::channels_displayed_get(ed);
    SpaceSeq *sseq = CTX_wm_space_seq(C);

    VectorSet strips = seq::query_rendered_strips(
        scene, channels, seqbase, scene->r.cfra, sseq->chanshown);

    /* Get the top most strip channel that is in view. */
    int max_channel = -1;
    for (Strip *strip : strips) {
      max_channel = max_ii(strip->channel, max_channel);
    }

    if (max_channel != -1) {
      RNA_int_set(drop->ptr, "channel", max_channel);
    }
  }

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

  const char *path = WM_drag_get_single_path(drag);
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
    BLI_path_abs(r_path, ID_BLEND_PATH_FROM_GLOBAL(id));
  }
  else {
    BLI_strncpy(r_path, WM_drag_get_single_path(drag), FILE_MAX);
  }
}

static void draw_strip_in_view(bContext *C, wmWindow * /*win*/, wmDrag *drag, const int xy[2])
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
  wmOrtho2_region_pixelspace(region);

  /* Sometimes the active theme is not the sequencer theme, e.g. when an operator invokes the
   * file browser. This makes sure we get the right color values for the theme. */
  bThemeState theme_state;
  UI_Theme_Store(&theme_state);
  UI_SetTheme(SPACE_SEQ, RGN_TYPE_WINDOW);

  if (coords->use_snapping) {
    transform::sequencer_snap_point(region, coords->snap_point_x);
  }

  /* Init GPU drawing. */
  GPU_blend(GPU_BLEND_ALPHA_PREMULT);

  /* Draw strips. The code here is taken from sequencer_draw. */
  float x1 = coords->start_frame;
  float x2 = coords->start_frame + floorf(strip_len);
  uchar strip_color[4];
  strip_color[3] = 255;
  uchar text_color[4] = {255, 255, 255, 255};
  float pixelx = BLI_rctf_size_x(&region->v2d.cur) / (BLI_rcti_size_x(&region->v2d.mask) + 1);
  float pixely = BLI_rctf_size_y(&region->v2d.cur) / (BLI_rcti_size_y(&region->v2d.mask) + 1);

  StripsDrawBatch batch(&region->v2d);

  for (int i = 0; i < coords->channel_len; i++) {
    float y1 = floorf(coords->channel) + i + STRIP_OFSBOTTOM;
    float y2 = floorf(coords->channel) + i + STRIP_OFSTOP;

    if (coords->type == TH_SEQ_MOVIE && i == 0 && coords->channel_len > 1) {
      /* Assume only video strips occupies two channels.
       * One for video and the other for audio.
       * The audio channel is added first.
       */
      UI_GetThemeColor3ubv(TH_SEQ_AUDIO, strip_color);
    }
    else {
      UI_GetThemeColor3ubv(coords->type, strip_color);
    }

    SeqStripDrawData &data = batch.add_strip(x1, x2, y2, y1, y2, x1, x2, 0, true);
    data.flags |= GPU_SEQ_FLAG_BACKGROUND | GPU_SEQ_FLAG_BORDER | GPU_SEQ_FLAG_SELECTED;
    data.col_background = color_pack(strip_color);

    if (coords->is_intersecting) {
      strip_color[0] = 255;
      strip_color[1] = strip_color[2] = 33;
    }
    else {
      if (coords->channel_len - 1 == i) {
        text_color[0] = text_color[1] = text_color[2] = 255;
        UI_GetThemeColor3ubv(TH_SEQ_ACTIVE, strip_color);
        data.flags |= GPU_SEQ_FLAG_ACTIVE;
      }
      else {
        text_color[0] = text_color[1] = text_color[2] = 10;
        UI_GetThemeColor3ubv(TH_SEQ_SELECTED, strip_color);
      }
    }
    strip_color[3] = 204;
    data.col_outline = color_pack(strip_color);

    /* Taken from strip_handle_draw_size_get(). */
    const float handle_size = pixelx * (5.0f * U.pixelsize);

    /* Calculate height needed for drawing text on strip. */
    float text_margin_y = y2 - min_ff(0.40f, 20 * UI_SCALE_FAC * pixely);
    float text_margin_x = 2.0f * handle_size;

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
      SNPRINTF_UTF8(strip_duration_text, "%d", int(x2 - x1));
      text_array[len_text_arr++] = text_sep;
      text_array[len_text_arr++] = strip_duration_text;
    }

    BLI_assert(len_text_arr <= ARRAY_SIZE(text_array));

    const size_t text_display_len = BLI_string_join_array(
        text_display, FILE_MAX, text_array, len_text_arr);

    UI_view2d_text_cache_add_rectf(
        &region->v2d, &rect, text_display, text_display_len, text_color);
  }
  batch.flush_batch();

  /* Clean after drawing up. */
  UI_Theme_Restore(&theme_state);
  GPU_matrix_pop();
  GPU_blend(GPU_BLEND_NONE);

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
};

static void prefetch_data_fn(void *custom_data, wmJobWorkerStatus * /*worker_status*/)
{
  DropJobData *job_data = (DropJobData *)custom_data;

  if (job_data->only_audio) {
#ifdef WITH_AUDASPACE
    /* Get the sound file length */
    AUD_Sound *sound = AUD_Sound_file(job_data->path);
    if (sound != nullptr) {

      AUD_SoundInfo info = AUD_getInfo(sound);
      if ((eSoundChannels)info.specs.channels != SOUND_CHANNELS_INVALID) {
        g_drop_coords.audio_length = info.length;
      }
      /* The playback rate is defined by the scene. This will be computed later in
       * #update_overlay_strip_position_data, when we know the scene from the context. So set it to
       * 0 for now. */
      g_drop_coords.playback_rate = 0.0f;
      AUD_Sound_free(sound);
      return;
    }
#endif
  }

  /* The movie reader is not used to access pixel data here, so avoid internal colorspace
   * conversions that ensures typical color pipeline in Blender as they might be expensive. */
  char colorspace[/*MAX_COLORSPACE_NAME*/ 64] = "\0";
  MovieReader *anim = openanim(job_data->path, IB_byte_data, 0, true, colorspace);

  if (anim != nullptr) {
    g_drop_coords.strip_len = MOV_get_duration_frames(anim, IMB_TC_NONE);
    g_drop_coords.playback_rate = MOV_get_fps(anim);
    MOV_close(anim);
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

  wmJob *wm_job = WM_jobs_get(wm,
                              nullptr,
                              nullptr,
                              "Loading previews...",
                              eWM_JobFlag(0),
                              WM_JOB_TYPE_SEQ_DRAG_DROP_PREVIEW);

  DropJobData *job_data = MEM_mallocN<DropJobData>("SeqDragDropPreviewData");
  get_drag_path(C, drag, job_data->path);

  job_data->only_audio = only_audio;
  g_drop_coords.only_audio = only_audio;

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

static void movie_drop_on_enter(wmDropBox *drop, wmDrag * /*drag*/)
{
  if (generic_drop_draw_handling(drop)) {
    return;
  }
}

static void sound_drop_on_enter(wmDropBox *drop, wmDrag * /*drag*/)
{
  if (generic_drop_draw_handling(drop)) {
    return;
  }
}

static void image_drop_on_enter(wmDropBox *drop, wmDrag * /*drag*/)
{
  if (generic_drop_draw_handling(drop)) {
    return;
  }

  SeqDropCoords *coords = static_cast<SeqDropCoords *>(drop->draw_data);
  coords->strip_len = DEFAULT_IMG_STRIP_LENGTH;
  coords->channel_len = 1;
}

static void sequencer_drop_on_exit(wmDropBox *drop, wmDrag * /*drag*/)
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
  drop->draw_in_view = draw_strip_in_view;
  drop->on_enter = image_drop_on_enter;
  drop->on_exit = sequencer_drop_on_exit;

  drop->on_drag_start = audio_prefetch;

  drop = WM_dropbox_add(
      lb, "SEQUENCER_OT_movie_strip_add", movie_drop_poll, sequencer_drop_copy, nullptr, nullptr);
  drop->draw_droptip = nop_draw_droptip_fn;
  drop->draw_in_view = draw_strip_in_view;
  drop->on_enter = movie_drop_on_enter;
  drop->on_exit = sequencer_drop_on_exit;

  drop->on_drag_start = video_prefetch;

  drop = WM_dropbox_add(
      lb, "SEQUENCER_OT_sound_strip_add", sound_drop_poll, sequencer_drop_copy, nullptr, nullptr);
  drop->draw_droptip = nop_draw_droptip_fn;
  drop->draw_in_view = draw_strip_in_view;
  drop->on_enter = sound_drop_on_enter;
  drop->on_exit = sequencer_drop_on_exit;
}

static bool image_drop_preview_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (file_type == FILE_TYPE_IMAGE) {
      return true;
    }
  }

  return WM_drag_is_ID_type(drag, ID_IM);
}

static bool movie_drop_preview_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (file_type == FILE_TYPE_MOVIE) {
      return true;
    }
  }

  return WM_drag_is_ID_type(drag, ID_MC);
}

static bool sound_drop_preview_poll(bContext * /*C*/, wmDrag *drag, const wmEvent * /*event*/)
{
  if (drag->type == WM_DRAG_PATH) {
    const eFileSel_File_Types file_type = eFileSel_File_Types(WM_drag_get_path_file_type(drag));
    if (file_type == FILE_TYPE_SOUND) {
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

}  // namespace blender::ed::vse

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation */

/** \file
 * \ingroup spseq
 */

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "ED_screen.h"

#include "BIF_glutil.h"

#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

#include "WM_api.h"
#include "WM_types.h"

#include "MEM_guardedalloc.h"

/* Own include. */
#include "sequencer_intern.h"

typedef struct ThumbnailDrawJob {
  SeqRenderData context;
  GHash *sequences_ghash;
  Scene *scene;
  rctf *view_area;
  float pixelx;
  float pixely;
  float thumb_height;
} ThumbnailDrawJob;

typedef struct ThumbDataItem {
  Sequence *seq_dupli;
  Scene *scene;
} ThumbDataItem;

static void thumbnail_hash_data_free(void *val)
{
  ThumbDataItem *item = val;
  SEQ_sequence_free(item->scene, item->seq_dupli);
  MEM_freeN(val);
}

static void thumbnail_freejob(void *data)
{
  ThumbnailDrawJob *tj = data;
  BLI_ghash_free(tj->sequences_ghash, NULL, thumbnail_hash_data_free);
  MEM_freeN(tj->view_area);
  MEM_freeN(tj);
}

static void thumbnail_endjob(void *data)
{
  ThumbnailDrawJob *tj = data;
  WM_main_add_notifier(NC_SCENE | ND_SEQUENCER, tj->scene);
}

static bool check_seq_need_thumbnails(const Scene *scene, Sequence *seq, rctf *view_area)
{
  if (!ELEM(seq->type, SEQ_TYPE_MOVIE, SEQ_TYPE_IMAGE)) {
    return false;
  }
  if (min_ii(SEQ_time_left_handle_frame_get(scene, seq), SEQ_time_start_frame_get(seq)) >
      view_area->xmax)
  {
    return false;
  }
  if (max_ii(SEQ_time_right_handle_frame_get(scene, seq),
             SEQ_time_content_end_frame_get(scene, seq)) < view_area->xmin)
  {
    return false;
  }
  if (seq->machine + 1.0f < view_area->ymin) {
    return false;
  }
  if (seq->machine > view_area->ymax) {
    return false;
  }

  /* Handle is moved, but not for this strip. */
  if ((G.moving & G_TRANSFORM_SEQ) != 0 && (seq->flag & (SEQ_LEFTSEL | SEQ_RIGHTSEL)) == 0) {
    return false;
  }

  return true;
}

static void seq_get_thumb_image_dimensions(Sequence *seq,
                                           float pixelx,
                                           float pixely,
                                           float *r_thumb_width,
                                           float thumb_height,
                                           float *r_image_width,
                                           float *r_image_height)
{
  float image_width = seq->strip->stripdata->orig_width;
  float image_height = seq->strip->stripdata->orig_height;

  /* Fix the dimensions to be max SEQ_RENDER_THUMB_SIZE (256) for x or y. */
  float aspect_ratio = (float)image_width / image_height;
  if (image_width > image_height) {
    image_width = SEQ_RENDER_THUMB_SIZE;
    image_height = round_fl_to_int(image_width / aspect_ratio);
  }
  else {
    image_height = SEQ_RENDER_THUMB_SIZE;
    image_width = round_fl_to_int(image_height * aspect_ratio);
  }

  /* Calculate thumb dimensions. */
  aspect_ratio = ((float)image_width) / image_height;
  float thumb_h_px = thumb_height / pixely;
  float thumb_width = aspect_ratio * thumb_h_px * pixelx;

  *r_thumb_width = thumb_width;
  if (r_image_width && r_image_height) {
    *r_image_width = image_width;
    *r_image_height = image_height;
  }
}

static void thumbnail_start_job(void *data,
                                bool *stop,
                                bool *UNUSED(do_update),
                                float *UNUSED(progress))
{
  ThumbnailDrawJob *tj = data;
  const Scene *scene = tj->scene;
  float frame_step;

  GHashIterator gh_iter;

  /* First pass: render visible images. */
  BLI_ghashIterator_init(&gh_iter, tj->sequences_ghash);
  while (!BLI_ghashIterator_done(&gh_iter) & !*stop) {
    Sequence *seq_orig = BLI_ghashIterator_getKey(&gh_iter);
    ThumbDataItem *val = BLI_ghash_lookup(tj->sequences_ghash, seq_orig);

    if (check_seq_need_thumbnails(scene, seq_orig, tj->view_area)) {
      seq_get_thumb_image_dimensions(
          val->seq_dupli, tj->pixelx, tj->pixely, &frame_step, tj->thumb_height, NULL, NULL);
      SEQ_render_thumbnails(
          &tj->context, val->seq_dupli, seq_orig, frame_step, tj->view_area, stop);
      SEQ_relations_sequence_free_anim(val->seq_dupli);
    }
    BLI_ghashIterator_step(&gh_iter);
  }

  /* Second pass: render "guaranteed" set of images. */
  BLI_ghashIterator_init(&gh_iter, tj->sequences_ghash);
  while (!BLI_ghashIterator_done(&gh_iter) & !*stop) {
    Sequence *seq_orig = BLI_ghashIterator_getKey(&gh_iter);
    ThumbDataItem *val = BLI_ghash_lookup(tj->sequences_ghash, seq_orig);

    if (check_seq_need_thumbnails(scene, seq_orig, tj->view_area)) {
      seq_get_thumb_image_dimensions(
          val->seq_dupli, tj->pixelx, tj->pixely, &frame_step, tj->thumb_height, NULL, NULL);
      SEQ_render_thumbnails_base_set(&tj->context, val->seq_dupli, seq_orig, tj->view_area, stop);
      SEQ_relations_sequence_free_anim(val->seq_dupli);
    }
    BLI_ghashIterator_step(&gh_iter);
  }
}

static SeqRenderData sequencer_thumbnail_context_init(const bContext *C)
{
  struct Main *bmain = CTX_data_main(C);
  struct Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  SeqRenderData context = {0};

  /* Taking rectx and recty as 0 as dimensions not known here, and context is used to calculate
   * hash key but not necessary as other variables of SeqRenderData are unique enough. */
  SEQ_render_new_render_data(bmain, depsgraph, scene, 0, 0, sseq->render_size, false, &context);
  context.view_id = BKE_scene_multiview_view_id_get(&scene->r, STEREO_LEFT_NAME);
  context.use_proxies = false;
  context.scene = scene;

  return context;
}

static GHash *sequencer_thumbnail_ghash_init(const bContext *C, View2D *v2d, Editing *ed)
{
  Scene *scene = CTX_data_scene(C);

  /* Set the data for thumbnail caching job. */
  GHash *thumb_data_hash = BLI_ghash_ptr_new("seq_duplicates_and_origs");

  LISTBASE_FOREACH (Sequence *, seq, ed->seqbasep) {
    ThumbDataItem *val_need_update = BLI_ghash_lookup(thumb_data_hash, seq);
    if (val_need_update == NULL && check_seq_need_thumbnails(scene, seq, &v2d->cur)) {
      ThumbDataItem *val = MEM_callocN(sizeof(ThumbDataItem), "Thumbnail Hash Values");
      val->seq_dupli = SEQ_sequence_dupli_recursive(scene, scene, NULL, seq, 0);
      val->scene = scene;
      BLI_ghash_insert(thumb_data_hash, seq, val);
    }
    else {
      if (val_need_update != NULL) {
        val_need_update->seq_dupli->start = seq->start;
        val_need_update->seq_dupli->startdisp = SEQ_time_left_handle_frame_get(scene, seq);
      }
    }
  }

  return thumb_data_hash;
}

static void sequencer_thumbnail_init_job(const bContext *C,
                                         View2D *v2d,
                                         Editing *ed,
                                         float thumb_height)
{
  wmJob *wm_job;
  ThumbnailDrawJob *tj = NULL;
  ScrArea *area = CTX_wm_area(C);
  wm_job = WM_jobs_get(CTX_wm_manager(C),
                       CTX_wm_window(C),
                       CTX_data_scene(C),
                       "Draw Thumbnails",
                       0,
                       WM_JOB_TYPE_SEQ_DRAW_THUMBNAIL);

  /* Get the thumbnail job if it exists. */
  tj = WM_jobs_customdata_get(wm_job);
  if (!tj) {
    tj = MEM_callocN(sizeof(ThumbnailDrawJob), "Thumbnail cache job");

    /* Duplicate value of v2d->cur and v2d->tot to have module separation. */
    rctf *view_area = MEM_callocN(sizeof(struct rctf), "viewport area");
    view_area->xmax = v2d->cur.xmax;
    view_area->xmin = v2d->cur.xmin;
    view_area->ymax = v2d->cur.ymax;
    view_area->ymin = v2d->cur.ymin;

    tj->scene = CTX_data_scene(C);
    tj->view_area = view_area;
    tj->context = sequencer_thumbnail_context_init(C);
    tj->sequences_ghash = sequencer_thumbnail_ghash_init(C, v2d, ed);
    tj->pixelx = BLI_rctf_size_x(&v2d->cur) / BLI_rcti_size_x(&v2d->mask);
    tj->pixely = BLI_rctf_size_y(&v2d->cur) / BLI_rcti_size_y(&v2d->mask);
    tj->thumb_height = thumb_height;
    WM_jobs_customdata_set(wm_job, tj, thumbnail_freejob);
    WM_jobs_timer(wm_job, 0.1, NC_SCENE | ND_SEQUENCER, NC_SCENE | ND_SEQUENCER);
    WM_jobs_callbacks(wm_job, thumbnail_start_job, NULL, NULL, thumbnail_endjob);
  }

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }
  else {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, NULL);
  }

  ED_area_tag_redraw(area);
}

static bool sequencer_thumbnail_v2d_is_navigating(const bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  View2D *v2d = &region->v2d;
  return (v2d->flag & V2D_IS_NAVIGATING) != 0;
}

static void sequencer_thumbnail_start_job_if_necessary(
    const bContext *C, Editing *ed, View2D *v2d, bool thumbnail_is_missing, float thumb_height)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);

  if (sequencer_thumbnail_v2d_is_navigating(C)) {
    WM_event_add_notifier(C, NC_SCENE | ND_SEQUENCER, NULL);
    return;
  }

  /* During rendering, cache is wiped, it doesn't make sense to render thumbnails. */
  if (G.is_rendering) {
    return;
  }

  /* Job start requested, but over area which has been processed. Unless `thumbnail_is_missing` is
   * true, ignore this request as all images are in view. */
  if (v2d->cur.xmax == sseq->runtime.last_thumbnail_area.xmax &&
      v2d->cur.ymax == sseq->runtime.last_thumbnail_area.ymax && !thumbnail_is_missing)
  {
    return;
  }

  /* Stop the job first as view has changed. Pointless to continue old job. */
  if (v2d->cur.xmax != sseq->runtime.last_thumbnail_area.xmax ||
      v2d->cur.ymax != sseq->runtime.last_thumbnail_area.ymax)
  {
    WM_jobs_stop(CTX_wm_manager(C), NULL, thumbnail_start_job);
  }

  sequencer_thumbnail_init_job(C, v2d, ed, thumb_height);
  sseq->runtime.last_thumbnail_area = v2d->cur;
}

void last_displayed_thumbnails_list_free(void *val)
{
  BLI_gset_free(val, NULL);
}

static GSet *last_displayed_thumbnails_list_ensure(const bContext *C, Sequence *seq)
{
  SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq->runtime.last_displayed_thumbnails == NULL) {
    sseq->runtime.last_displayed_thumbnails = BLI_ghash_ptr_new(__func__);
  }

  GSet *displayed_thumbnails = BLI_ghash_lookup(sseq->runtime.last_displayed_thumbnails, seq);
  if (displayed_thumbnails == NULL) {
    displayed_thumbnails = BLI_gset_int_new(__func__);
    BLI_ghash_insert(sseq->runtime.last_displayed_thumbnails, seq, displayed_thumbnails);
  }

  return displayed_thumbnails;
}

static void last_displayed_thumbnails_list_cleanup(GSet *previously_displayed,
                                                   float range_start,
                                                   float range_end)
{
  GSetIterator gset_iter;
  BLI_gsetIterator_init(&gset_iter, previously_displayed);
  while (!BLI_gsetIterator_done(&gset_iter)) {
    int frame = (float)POINTER_AS_INT(BLI_gsetIterator_getKey(&gset_iter));
    BLI_gsetIterator_step(&gset_iter);

    if (frame > range_start && frame < range_end) {
      BLI_gset_remove(previously_displayed, POINTER_FROM_INT(frame), NULL);
    }
  }
}

static int sequencer_thumbnail_closest_previous_frame_get(int timeline_frame,
                                                          GSet *previously_displayed)
{
  int best_diff = INT_MAX;
  int best_frame = timeline_frame;

  /* Previously displayed thumbnails. */
  GSetIterator gset_iter;
  BLI_gsetIterator_init(&gset_iter, previously_displayed);
  while (!BLI_gsetIterator_done(&gset_iter)) {
    int frame = POINTER_AS_INT(BLI_gsetIterator_getKey(&gset_iter));
    int diff = abs(frame - timeline_frame);
    if (diff < best_diff) {
      best_diff = diff;
      best_frame = frame;
    }
    BLI_gsetIterator_step(&gset_iter);
  }
  return best_frame;
}

static int sequencer_thumbnail_closest_guaranteed_frame_get(struct Scene *scene,
                                                            Sequence *seq,
                                                            int timeline_frame)
{
  if (timeline_frame <= SEQ_time_left_handle_frame_get(scene, seq)) {
    return SEQ_time_left_handle_frame_get(scene, seq);
  }

  /* Set of "guaranteed" thumbnails. */
  const int frame_index = timeline_frame - SEQ_time_left_handle_frame_get(scene, seq);
  const int frame_step = SEQ_render_thumbnails_guaranteed_set_frame_step_get(scene, seq);
  const int relative_base_frame = round_fl_to_int(frame_index / (float)frame_step) * frame_step;
  const int nearest_guaranted_absolute_frame = relative_base_frame +
                                               SEQ_time_left_handle_frame_get(scene, seq);
  return nearest_guaranted_absolute_frame;
}

static ImBuf *sequencer_thumbnail_closest_from_memory(const SeqRenderData *context,
                                                      Sequence *seq,
                                                      int timeline_frame,
                                                      GSet *previously_displayed,
                                                      rcti *crop,
                                                      bool clipped)
{
  int frame_previous = sequencer_thumbnail_closest_previous_frame_get(timeline_frame,
                                                                      previously_displayed);
  ImBuf *ibuf_previous = SEQ_get_thumbnail(context, seq, frame_previous, crop, clipped);

  int frame_guaranteed = sequencer_thumbnail_closest_guaranteed_frame_get(
      context->scene, seq, timeline_frame);
  ImBuf *ibuf_guaranteed = SEQ_get_thumbnail(context, seq, frame_guaranteed, crop, clipped);

  ImBuf *closest_in_memory = NULL;

  if (ibuf_previous && ibuf_guaranteed) {
    if (abs(frame_previous - timeline_frame) < abs(frame_guaranteed - timeline_frame)) {
      IMB_freeImBuf(ibuf_guaranteed);
      closest_in_memory = ibuf_previous;
    }
    else {
      IMB_freeImBuf(ibuf_previous);
      closest_in_memory = ibuf_guaranteed;
    }
  }

  if (ibuf_previous == NULL) {
    closest_in_memory = ibuf_guaranteed;
  }

  if (ibuf_guaranteed == NULL) {
    closest_in_memory = ibuf_previous;
  }

  return closest_in_memory;
}

void draw_seq_strip_thumbnail(View2D *v2d,
                              const bContext *C,
                              Scene *scene,
                              Sequence *seq,
                              float y1,
                              float y2,
                              float pixelx,
                              float pixely)
{
  bool clipped = false;
  float image_height, image_width, thumb_width;
  rcti crop;

  StripElem *se = seq->strip->stripdata;
  if (se->orig_height == 0 || se->orig_width == 0) {
    return;
  }

  /* If width of the strip too small ignore drawing thumbnails. */
  if ((y2 - y1) / pixely <= 20 * UI_SCALE_FAC) {
    return;
  }

  SeqRenderData context = sequencer_thumbnail_context_init(C);

  if ((seq->flag & SEQ_FLAG_SKIP_THUMBNAILS) != 0) {
    return;
  }

  const float thumb_height = y2 - y1;
  seq_get_thumb_image_dimensions(
      seq, pixelx, pixely, &thumb_width, thumb_height, &image_width, &image_height);

  float thumb_y_end = y1 + thumb_height;

  float cut_off = 0;
  float upper_thumb_bound = SEQ_time_has_right_still_frames(scene, seq) ?
                                (seq->start + seq->len) :
                                SEQ_time_right_handle_frame_get(scene, seq);
  if (seq->type == SEQ_TYPE_IMAGE) {
    upper_thumb_bound = SEQ_time_right_handle_frame_get(scene, seq);
  }

  float timeline_frame = SEQ_render_thumbnail_first_frame_get(scene, seq, thumb_width, &v2d->cur);
  float thumb_x_end;

  GSet *last_displayed_thumbnails = last_displayed_thumbnails_list_ensure(C, seq);
  /* Cleanup thumbnail list outside of rendered range, which is cleaned up one by one to prevent
   * flickering after zooming. */
  if (!sequencer_thumbnail_v2d_is_navigating(C)) {
    last_displayed_thumbnails_list_cleanup(last_displayed_thumbnails, -FLT_MAX, timeline_frame);
  }

  /* Start drawing. */
  while (timeline_frame < upper_thumb_bound) {
    thumb_x_end = timeline_frame + thumb_width;
    clipped = false;

    /* Checks to make sure that thumbs are loaded only when in view and within the confines of the
     * strip. Some may not be required but better to have conditions for safety as x1 here is
     * point to start caching from and not drawing. */
    if (timeline_frame > v2d->cur.xmax) {
      break;
    }

    /* Set the clipping bound to show the left handle moving over thumbs and not shift thumbs. */
    if (IN_RANGE_INCL(SEQ_time_left_handle_frame_get(scene, seq), timeline_frame, thumb_x_end)) {
      cut_off = SEQ_time_left_handle_frame_get(scene, seq) - timeline_frame;
      clipped = true;
    }

    /* Clip if full thumbnail cannot be displayed. */
    if (thumb_x_end > (upper_thumb_bound)) {
      thumb_x_end = upper_thumb_bound;
      clipped = true;
      if (thumb_x_end - timeline_frame < 1) {
        break;
      }
    }

    float zoom_x = thumb_width / image_width;
    float zoom_y = thumb_height / image_height;

    float cropx_min = (cut_off / pixelx) / (zoom_y / pixely);
    float cropx_max = ((thumb_x_end - timeline_frame) / pixelx) / (zoom_y / pixely);
    if (cropx_max == (thumb_x_end - timeline_frame)) {
      cropx_max = cropx_max + 1;
    }
    BLI_rcti_init(&crop, (int)(cropx_min), (int)cropx_max, 0, (int)(image_height)-1);

    /* Get the image. */
    ImBuf *ibuf = SEQ_get_thumbnail(&context, seq, timeline_frame, &crop, clipped);

    if (!ibuf) {
      sequencer_thumbnail_start_job_if_necessary(C, scene->ed, v2d, true, thumb_height);

      ibuf = sequencer_thumbnail_closest_from_memory(
          &context, seq, timeline_frame, last_displayed_thumbnails, &crop, clipped);
    }
    /* Store recently rendered frames, so they can be reused when zooming. */
    else if (!sequencer_thumbnail_v2d_is_navigating(C)) {
      /* Clear images in frame range occupied by new thumbnail. */
      last_displayed_thumbnails_list_cleanup(
          last_displayed_thumbnails, timeline_frame, thumb_x_end);
      /* Insert new thumbnail frame to list. */
      BLI_gset_add(last_displayed_thumbnails, POINTER_FROM_INT(timeline_frame));
    }

    /* If there is no image still, abort. */
    if (!ibuf) {
      break;
    }

    /* Transparency on overlap. */
    if (seq->flag & SEQ_OVERLAP) {
      GPU_blend(GPU_BLEND_ALPHA);
      if (ibuf->byte_buffer.data) {
        uchar *buf = ibuf->byte_buffer.data;
        for (int pixel = ibuf->x * ibuf->y; pixel--; buf += 4) {
          buf[3] = OVERLAP_ALPHA;
        }
      }
      else if (ibuf->float_buffer.data) {
        float *buf = ibuf->float_buffer.data;
        for (int pixel = ibuf->x * ibuf->y; pixel--; buf += ibuf->channels) {
          buf[3] = (OVERLAP_ALPHA / 255.0f);
        }
      }
    }

    ED_draw_imbuf_ctx_clipping(C,
                               ibuf,
                               timeline_frame + cut_off,
                               y1,
                               true,
                               timeline_frame + cut_off,
                               y1,
                               thumb_x_end,
                               thumb_y_end,
                               zoom_x,
                               zoom_y);
    IMB_freeImBuf(ibuf);
    GPU_blend(GPU_BLEND_NONE);
    cut_off = 0;
    timeline_frame = SEQ_render_thumbnail_next_frame_get(scene, seq, timeline_frame, thumb_width);
  }
  last_displayed_thumbnails_list_cleanup(last_displayed_thumbnails, timeline_frame, FLT_MAX);
}

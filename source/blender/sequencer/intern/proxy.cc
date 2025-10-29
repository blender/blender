/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#else
#  include <unistd.h>
#endif

#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "WM_types.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_read.hh"

#include "SEQ_proxy.hh"
#include "SEQ_relations.hh"
#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_time.hh"

#include "cache/intra_frame_cache.hh"
#include "multiview.hh"
#include "proxy.hh"
#include "render.hh"
#include "sequencer.hh"
#include "utils.hh"

namespace blender::seq {

struct IndexBuildContext {
  MovieProxyBuilder *proxy_builder;

  int tc_flags;
  int size_flags;
  int quality;
  bool overwrite;
  int view_id;

  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  Strip *strip, *orig_seq;
  SessionUID orig_seq_uid;
};

IMB_Proxy_Size rendersize_to_proxysize(eSpaceSeq_Proxy_RenderSize render_size)
{
  switch (render_size) {
    case SEQ_RENDER_SIZE_PROXY_25:
      return IMB_PROXY_25;
    case SEQ_RENDER_SIZE_PROXY_50:
      return IMB_PROXY_50;
    case SEQ_RENDER_SIZE_PROXY_75:
      return IMB_PROXY_75;
    case SEQ_RENDER_SIZE_PROXY_100:
      return IMB_PROXY_100;
    default:
      return IMB_PROXY_NONE;
  }
}

float rendersize_to_scale_factor(eSpaceSeq_Proxy_RenderSize render_size)
{
  switch (render_size) {
    case SEQ_RENDER_SIZE_PROXY_25:
      return 0.25f;
    case SEQ_RENDER_SIZE_PROXY_50:
      return 0.5f;
    case SEQ_RENDER_SIZE_PROXY_75:
      return 0.75f;
    default:
      return 1.0f;
  }
}

bool seq_proxy_get_custom_file_filepath(Strip *strip, char *filepath, const int view_id)
{
  /* Ideally this would be #PROXY_MAXFILE however BLI_path_abs clamps to #FILE_MAX. */
  char filepath_temp[FILE_MAX];
  char suffix[24];
  StripProxy *proxy = strip->data->proxy;

  if (proxy == nullptr) {
    return false;
  }

  BLI_path_join(filepath_temp, sizeof(filepath_temp), proxy->dirpath, proxy->filename);
  BLI_path_abs(filepath_temp, BKE_main_blendfile_path_from_global());

  if (view_id > 0) {
    SNPRINTF(suffix, "_%d", view_id);
    /* TODO(sergey): This will actually append suffix after extension
     * which is weird but how was originally coded in multi-view branch.
     */
    BLI_snprintf(filepath, PROXY_MAXFILE, "%s_%s", filepath_temp, suffix);
  }
  else {
    BLI_strncpy(filepath, filepath_temp, PROXY_MAXFILE);
  }

  return true;
}

static bool seq_proxy_get_filepath(Scene *scene,
                                   Strip *strip,
                                   int timeline_frame,
                                   eSpaceSeq_Proxy_RenderSize render_size,
                                   char *filepath,
                                   const int view_id)
{
  char dirpath[PROXY_MAXFILE];
  char suffix[24] = {'\0'};
  Editing *ed = editing_get(scene);
  StripProxy *proxy = strip->data->proxy;

  if (proxy == nullptr) {
    return false;
  }

  /* Multi-view suffix. */
  if (view_id > 0) {
    SNPRINTF(suffix, "_%d", view_id);
  }

  /* Per strip with Custom file situation is handled separately. */
  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE &&
      ed->proxy_storage != SEQ_EDIT_PROXY_DIR_STORAGE)
  {
    if (seq_proxy_get_custom_file_filepath(strip, filepath, view_id)) {
      return true;
    }
  }

  if (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE) {
    /* Per project default. */
    if (ed->proxy_dir[0] == 0) {
      STRNCPY(dirpath, "//BL_proxy");
    }
    else { /* Per project with custom dirpath. */
      STRNCPY(dirpath, ed->proxy_dir);
    }
    BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());
  }
  else {
    /* Pre strip with custom dir. */
    if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) {
      STRNCPY(dirpath, strip->data->proxy->dirpath);
    }
    else { /* Per strip default. */
      SNPRINTF(dirpath, "%s" SEP_STR "BL_proxy", strip->data->dirpath);
    }
  }

  /* Proxy size number to be used in path. */
  int proxy_size_number = rendersize_to_scale_factor(render_size) * 100;

  BLI_snprintf(filepath,
               PROXY_MAXFILE,
               "%s" SEP_STR "images" SEP_STR "%d" SEP_STR "%s_proxy%s.jpg",
               dirpath,
               proxy_size_number,
               render_give_stripelem(scene, strip, timeline_frame)->filename,
               suffix);
  BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());
  return true;
}

bool can_use_proxy(const RenderData *context, const Strip *strip, IMB_Proxy_Size psize)
{
  if (strip->data->proxy == nullptr || !context->use_proxies) {
    return false;
  }

  short size_flags = strip->data->proxy->build_size_flags;
  return (strip->flag & SEQ_USE_PROXY) != 0 && psize != IMB_PROXY_NONE &&
         (size_flags & psize) != 0;
}

ImBuf *seq_proxy_fetch(const RenderData *context, Strip *strip, int timeline_frame)
{
  char filepath[PROXY_MAXFILE];
  StripProxy *proxy = strip->data->proxy;
  const eSpaceSeq_Proxy_RenderSize psize = eSpaceSeq_Proxy_RenderSize(
      context->preview_render_size);
  StripAnim *sanim;

  /* only use proxies, if they are enabled (even if present!) */
  if (!can_use_proxy(context, strip, rendersize_to_proxysize(psize))) {
    return nullptr;
  }

  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    int frameno = round_fl_to_int(give_frame_index(context->scene, strip, timeline_frame)) +
                  strip->anim_startofs;
    if (proxy->anim == nullptr) {
      if (seq_proxy_get_filepath(
              context->scene, strip, timeline_frame, psize, filepath, context->view_id) == 0)
      {
        return nullptr;
      }

      /* Sequencer takes care of colorspace conversion of the result. The input is the best to be
       * kept unchanged for the performance reasons. */
      proxy->anim = openanim(
          filepath, IB_byte_data, 0, true, strip->data->colorspace_settings.name);
    }
    if (proxy->anim == nullptr) {
      return nullptr;
    }

    strip_open_anim_file(context->scene, strip, true);
    sanim = static_cast<StripAnim *>(strip->anims.first);

    frameno = MOV_calc_frame_index_with_timecode(
        sanim ? sanim->anim : nullptr, IMB_Timecode_Type(strip->data->proxy->tc), frameno);

    return MOV_decode_frame(proxy->anim, frameno, IMB_TC_NONE, IMB_PROXY_NONE);
  }

  if (seq_proxy_get_filepath(
          context->scene, strip, timeline_frame, psize, filepath, context->view_id) == 0)
  {
    return nullptr;
  }

  if (BLI_exists(filepath)) {
    ImBuf *ibuf = IMB_load_image_from_filepath(filepath, IB_byte_data | IB_metadata);

    if (ibuf) {
      seq_imbuf_assign_spaces(context->scene, ibuf);
    }

    return ibuf;
  }

  return nullptr;
}

static void seq_proxy_build_frame(const RenderData *context,
                                  SeqRenderState *state,
                                  Strip *strip,
                                  int timeline_frame,
                                  int proxy_render_size,
                                  const bool overwrite)
{
  char filepath[PROXY_MAXFILE];
  ImBuf *ibuf_tmp, *ibuf;
  Scene *scene = context->scene;

  if (!seq_proxy_get_filepath(scene,
                              strip,
                              timeline_frame,
                              eSpaceSeq_Proxy_RenderSize(proxy_render_size),
                              filepath,
                              context->view_id))
  {
    return;
  }

  if (!overwrite && BLI_exists(filepath)) {
    return;
  }

  ibuf_tmp = seq_render_strip(context, state, strip, timeline_frame);

  int rectx = (proxy_render_size * ibuf_tmp->x) / 100;
  int recty = (proxy_render_size * ibuf_tmp->y) / 100;

  if (ibuf_tmp->x != rectx || ibuf_tmp->y != recty) {
    ibuf = IMB_scale_into_new(ibuf_tmp, rectx, recty, IMBScaleFilter::Nearest, true);
    IMB_freeImBuf(ibuf_tmp);
  }
  else {
    ibuf = ibuf_tmp;
  }

  const int quality = strip->data->proxy->quality;
  const bool save_float = ibuf->float_buffer.data != nullptr;
  ibuf->foptions.quality = quality;
  if (save_float) {
    /* Float image: save as EXR with FP16 data and DWAA compression. */
    ibuf->ftype = IMB_FTYPE_OPENEXR;
    ibuf->foptions.flag = OPENEXR_HALF | R_IMF_EXR_CODEC_DWAA;
  }
  else {
    /* Byte image: save as JPG. */
    ibuf->ftype = IMB_FTYPE_JPG;
    if (ibuf->planes == 32) {
      ibuf->planes = 24; /* JPGs do not support alpha. */
    }
  }
  BLI_file_ensure_parent_dir_exists(filepath);

  const bool ok = IMB_save_image(ibuf, filepath, save_float ? IB_float_data : IB_byte_data);
  if (ok == false) {
    perror(filepath);
  }

  IMB_freeImBuf(ibuf);
}

/**
 * Cache the result of #BKE_scene_multiview_view_prefix_get.
 */
struct MultiViewPrefixVars {
  char prefix[FILE_MAX];
  const char *ext;
};

/**
 * Returns whether the file this context would read from even exist,
 * if not, don't create the context.
 *
 * \param prefix_vars: Stores prefix variables for reuse,
 * these variables are for internal use, the caller must not depend on them.
 *
 * \note This function must first a `view_id` of zero, to initialize `prefix_vars`
 * for use with other views.
 */
static bool seq_proxy_multiview_context_invalid(Strip *strip,
                                                Scene *scene,
                                                const int view_id,
                                                MultiViewPrefixVars *prefix_vars)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if ((strip->type == STRIP_TYPE_IMAGE) && (strip->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    if (view_id == 0) {
      /* Clear on first use. */
      prefix_vars->prefix[0] = '\0';
      prefix_vars->ext = nullptr;

      char filepath[FILE_MAX];
      BLI_path_join(
          filepath, sizeof(filepath), strip->data->dirpath, strip->data->stripdata->filename);
      BLI_path_abs(filepath, ID_BLEND_PATH_FROM_GLOBAL(&scene->id));
      BKE_scene_multiview_view_prefix_get(scene, filepath, prefix_vars->prefix, &prefix_vars->ext);
    }

    if (prefix_vars->prefix[0] == '\0') {
      return view_id != 0;
    }

    char filepath[FILE_MAX];
    seq_multiview_name(scene, view_id, prefix_vars->prefix, prefix_vars->ext, filepath, FILE_MAX);
    if (BLI_access(filepath, R_OK) == 0) {
      return false;
    }

    return view_id != 0;
  }
  return false;
}

/**
 * This returns the maximum possible number of required contexts
 */
static int seq_proxy_context_count(Strip *strip, Scene *scene)
{
  int num_views = 1;

  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return 1;
  }

  switch (strip->type) {
    case STRIP_TYPE_MOVIE: {
      num_views = BLI_listbase_count(&strip->anims);
      break;
    }
    case STRIP_TYPE_IMAGE: {
      switch (strip->views_format) {
        case R_IMF_VIEWS_INDIVIDUAL:
          num_views = BKE_scene_multiview_num_views_get(&scene->r);
          break;
        case R_IMF_VIEWS_STEREO_3D:
          num_views = 2;
          break;
        case R_IMF_VIEWS_MULTIVIEW:
        /* not supported at the moment */
        /* pass through */
        default:
          num_views = 1;
      }
      break;
    }
  }

  return num_views;
}

static bool seq_proxy_need_rebuild(Strip *strip, MovieReader *anim)
{
  if ((strip->data->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) == 0) {
    return true;
  }

  IMB_Proxy_Size required_proxies = IMB_Proxy_Size(strip->data->proxy->build_size_flags);
  int built_proxies = MOV_get_existing_proxies(anim);
  return (required_proxies & built_proxies) != required_proxies;
}

bool proxy_rebuild_context(Main *bmain,
                           Depsgraph *depsgraph,
                           Scene *scene,
                           Strip *strip,
                           Set<std::string> *processed_paths,
                           ListBase *queue,
                           bool build_only_on_bad_performance)
{
  IndexBuildContext *context;
  Strip *strip_new;
  LinkData *link;
  int num_files;
  int i;

  if (!strip->data || !strip->data->proxy) {
    return true;
  }

  if (!(strip->flag & SEQ_USE_PROXY)) {
    return true;
  }

  num_files = seq_proxy_context_count(strip, scene);

  MultiViewPrefixVars prefix_vars; /* Initialized by #seq_proxy_multiview_context_invalid. */
  for (i = 0; i < num_files; i++) {
    if (seq_proxy_multiview_context_invalid(strip, scene, i, &prefix_vars)) {
      continue;
    }

    /* Check if proxies are already built here, because actually opening anims takes a lot of
     * time. */
    strip_open_anim_file(scene, strip, false);
    StripAnim *sanim = static_cast<StripAnim *>(BLI_findlink(&strip->anims, i));
    if (sanim->anim && !seq_proxy_need_rebuild(strip, sanim->anim)) {
      continue;
    }

    relations_strip_free_anim(strip);

    context = MEM_callocN<IndexBuildContext>("strip proxy rebuild context");

    strip_new = strip_duplicate_recursive(
        bmain, scene, scene, nullptr, strip, StripDuplicate::Selected);

    context->tc_flags = strip_new->data->proxy->build_tc_flags;
    context->size_flags = strip_new->data->proxy->build_size_flags;
    context->quality = strip_new->data->proxy->quality;
    context->overwrite = (strip_new->data->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) == 0;

    context->bmain = bmain;
    context->depsgraph = depsgraph;
    context->scene = scene;
    context->orig_seq = strip;
    context->orig_seq_uid = strip->runtime.session_uid;
    context->strip = strip_new;

    context->view_id = i; /* only for images */

    if (strip_new->type == STRIP_TYPE_MOVIE) {
      strip_open_anim_file(scene, strip_new, true);
      sanim = static_cast<StripAnim *>(BLI_findlink(&strip_new->anims, i));

      if (sanim->anim) {
        context->proxy_builder = MOV_proxy_builder_start(sanim->anim,
                                                         IMB_Timecode_Type(context->tc_flags),
                                                         context->size_flags,
                                                         context->quality,
                                                         context->overwrite,
                                                         processed_paths,
                                                         build_only_on_bad_performance);
      }
      if (!context->proxy_builder) {
        MEM_freeN(context);
        return false;
      }
    }

    link = BLI_genericNodeN(context);
    BLI_addtail(queue, link);
  }

  return true;
}

void proxy_rebuild(IndexBuildContext *context, wmJobWorkerStatus *worker_status)
{
  const bool overwrite = context->overwrite;
  RenderData render_context;
  Strip *strip = context->strip;
  Scene *scene = context->scene;
  Main *bmain = context->bmain;
  int timeline_frame;

  if (strip->type == STRIP_TYPE_MOVIE) {
    if (context->proxy_builder) {
      MOV_proxy_builder_process(context->proxy_builder,
                                &worker_status->stop,
                                &worker_status->do_update,
                                &worker_status->progress);
    }

    return;
  }

  if (!(strip->flag & SEQ_USE_PROXY)) {
    return;
  }

  /* that's why it is called custom... */
  if (strip->data->proxy && strip->data->proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    return;
  }

  /* fail safe code */
  int width, height;
  BKE_render_resolution(&scene->r, false, &width, &height);

  render_new_render_data(bmain,
                         context->depsgraph,
                         context->scene,
                         width,
                         height,
                         SEQ_RENDER_SIZE_PROXY_100,
                         false,
                         &render_context);

  render_context.skip_cache = true;
  render_context.is_proxy_render = true;
  render_context.view_id = context->view_id;

  SeqRenderState state;

  for (timeline_frame = time_left_handle_frame_get(scene, strip);
       timeline_frame < time_right_handle_frame_get(scene, strip);
       timeline_frame++)
  {
    intra_frame_cache_set_cur_frame(render_context.scene,
                                    timeline_frame,
                                    render_context.view_id,
                                    render_context.rectx,
                                    render_context.recty);

    if (context->size_flags & IMB_PROXY_25) {
      seq_proxy_build_frame(&render_context, &state, strip, timeline_frame, 25, overwrite);
    }
    if (context->size_flags & IMB_PROXY_50) {
      seq_proxy_build_frame(&render_context, &state, strip, timeline_frame, 50, overwrite);
    }
    if (context->size_flags & IMB_PROXY_75) {
      seq_proxy_build_frame(&render_context, &state, strip, timeline_frame, 75, overwrite);
    }
    if (context->size_flags & IMB_PROXY_100) {
      seq_proxy_build_frame(&render_context, &state, strip, timeline_frame, 100, overwrite);
    }

    worker_status->progress = float(timeline_frame - time_left_handle_frame_get(scene, strip)) /
                              (time_right_handle_frame_get(scene, strip) -
                               time_left_handle_frame_get(scene, strip));
    worker_status->do_update = true;

    if (worker_status->stop || G.is_break) {
      break;
    }
  }
}

void proxy_rebuild_finish(IndexBuildContext *context, bool stop)
{
  if (context->proxy_builder) {
    LISTBASE_FOREACH (StripAnim *, sanim, &context->strip->anims) {
      MOV_close_proxies(sanim->anim);
    }

    MOV_proxy_builder_finish(context->proxy_builder, stop);
  }

  seq_free_strip_recurse(nullptr, context->strip, true);

  MEM_freeN(context);
}

void proxy_set(Strip *strip, bool value)
{
  if (value) {
    strip->flag |= SEQ_USE_PROXY;
    if (strip->data->proxy == nullptr) {
      strip->data->proxy = seq_strip_proxy_alloc();
    }
  }
  else {
    strip->flag &= ~SEQ_USE_PROXY;
  }
}

void seq_proxy_index_dir_set(MovieReader *anim, const char *base_dir)
{
  char dirname[FILE_MAX];
  char filename[FILE_MAXFILE];

  MOV_get_filename(anim, filename, FILE_MAXFILE);
  BLI_path_join(dirname, sizeof(dirname), base_dir, filename);
  MOV_set_custom_proxy_dir(anim, dirname);
}

void free_strip_proxy(Strip *strip)
{
  if (strip->data && strip->data->proxy && strip->data->proxy->anim) {
    MOV_close(strip->data->proxy->anim);
    strip->data->proxy->anim = nullptr;
  }
}

}  // namespace blender::seq

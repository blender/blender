/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2003-2009 Blender Authors
 * SPDX-FileCopyrightText: 2005-2006 Peter Schlaile <peter [at] schlaile [dot] de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_session_uuid.h"
#include "BLI_string.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#else
#  include <unistd.h>
#endif

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "SEQ_iterator.h"
#include "SEQ_proxy.h"
#include "SEQ_relations.h"
#include "SEQ_render.h"
#include "SEQ_sequencer.h"
#include "SEQ_time.h"

#include "multiview.h"
#include "proxy.h"
#include "render.h"
#include "sequencer.h"
#include "strip_time.h"
#include "utils.h"

struct SeqIndexBuildContext {
  IndexBuildContext *index_context;

  int tc_flags;
  int size_flags;
  int quality;
  bool overwrite;
  int view_id;

  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  Sequence *seq, *orig_seq;
  SessionUUID orig_seq_uuid;
};

int SEQ_rendersize_to_proxysize(int render_size)
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
  }
  return IMB_PROXY_NONE;
}

double SEQ_rendersize_to_scale_factor(int render_size)
{
  switch (render_size) {
    case SEQ_RENDER_SIZE_PROXY_25:
      return 0.25;
    case SEQ_RENDER_SIZE_PROXY_50:
      return 0.50;
    case SEQ_RENDER_SIZE_PROXY_75:
      return 0.75;
  }
  return 1.0;
}

bool seq_proxy_get_custom_file_filepath(Sequence *seq, char *filepath, const int view_id)
{
  /* Ideally this would be #PROXY_MAXFILE however BLI_path_abs clamps to #FILE_MAX. */
  char filepath_temp[FILE_MAX];
  char suffix[24];
  StripProxy *proxy = seq->strip->proxy;

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
                                   Sequence *seq,
                                   int timeline_frame,
                                   eSpaceSeq_Proxy_RenderSize render_size,
                                   char *filepath,
                                   const int view_id)
{
  char dirpath[PROXY_MAXFILE];
  char suffix[24] = {'\0'};
  Editing *ed = SEQ_editing_get(scene);
  StripProxy *proxy = seq->strip->proxy;

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
    if (seq_proxy_get_custom_file_filepath(seq, filepath, view_id)) {
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
      STRNCPY(dirpath, seq->strip->proxy->dirpath);
    }
    else { /* Per strip default. */
      SNPRINTF(dirpath, "%s" SEP_STR "BL_proxy", seq->strip->dirpath);
    }
  }

  /* Proxy size number to be used in path. */
  int proxy_size_number = SEQ_rendersize_to_scale_factor(render_size) * 100;

  BLI_snprintf(filepath,
               PROXY_MAXFILE,
               "%s" SEP_STR "images" SEP_STR "%d" SEP_STR "%s_proxy%s.jpg",
               dirpath,
               proxy_size_number,
               SEQ_render_give_stripelem(scene, seq, timeline_frame)->filename,
               suffix);
  BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());
  return true;
}

bool SEQ_can_use_proxy(const SeqRenderData *context, Sequence *seq, int psize)
{
  if (seq->strip->proxy == nullptr || !context->use_proxies) {
    return false;
  }

  short size_flags = seq->strip->proxy->build_size_flags;
  return (seq->flag & SEQ_USE_PROXY) != 0 && psize != IMB_PROXY_NONE && (size_flags & psize) != 0;
}

ImBuf *seq_proxy_fetch(const SeqRenderData *context, Sequence *seq, int timeline_frame)
{
  char filepath[PROXY_MAXFILE];
  StripProxy *proxy = seq->strip->proxy;
  const eSpaceSeq_Proxy_RenderSize psize = eSpaceSeq_Proxy_RenderSize(
      context->preview_render_size);
  StripAnim *sanim;

  /* only use proxies, if they are enabled (even if present!) */
  if (!SEQ_can_use_proxy(context, seq, SEQ_rendersize_to_proxysize(psize))) {
    return nullptr;
  }

  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    int frameno = int(SEQ_give_frame_index(context->scene, seq, timeline_frame)) +
                  seq->anim_startofs;
    if (proxy->anim == nullptr) {
      if (seq_proxy_get_filepath(
              context->scene, seq, timeline_frame, psize, filepath, context->view_id) == 0)
      {
        return nullptr;
      }

      proxy->anim = openanim(filepath, IB_rect, 0, seq->strip->colorspace_settings.name);
    }
    if (proxy->anim == nullptr) {
      return nullptr;
    }

    seq_open_anim_file(context->scene, seq, true);
    sanim = static_cast<StripAnim *>(seq->anims.first);

    frameno = IMB_anim_index_get_frame_index(
        sanim ? sanim->anim : nullptr, IMB_Timecode_Type(seq->strip->proxy->tc), frameno);

    return IMB_anim_absolute(proxy->anim, frameno, IMB_TC_NONE, IMB_PROXY_NONE);
  }

  if (seq_proxy_get_filepath(
          context->scene, seq, timeline_frame, psize, filepath, context->view_id) == 0)
  {
    return nullptr;
  }

  if (BLI_exists(filepath)) {
    ImBuf *ibuf = IMB_loadiffname(filepath, IB_rect, nullptr);

    if (ibuf) {
      seq_imbuf_assign_spaces(context->scene, ibuf);
    }

    return ibuf;
  }

  return nullptr;
}

static void seq_proxy_build_frame(const SeqRenderData *context,
                                  SeqRenderState *state,
                                  Sequence *seq,
                                  int timeline_frame,
                                  int proxy_render_size,
                                  const bool overwrite)
{
  char filepath[PROXY_MAXFILE];
  int quality;
  int rectx, recty;
  ImBuf *ibuf_tmp, *ibuf;
  Scene *scene = context->scene;

  if (!seq_proxy_get_filepath(scene,
                              seq,
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

  ibuf_tmp = seq_render_strip(context, state, seq, timeline_frame);

  rectx = (proxy_render_size * ibuf_tmp->x) / 100;
  recty = (proxy_render_size * ibuf_tmp->y) / 100;

  if (ibuf_tmp->x != rectx || ibuf_tmp->y != recty) {
    ibuf = IMB_dupImBuf(ibuf_tmp);
    IMB_metadata_copy(ibuf, ibuf_tmp);
    IMB_freeImBuf(ibuf_tmp);
    IMB_scalefastImBuf(ibuf, short(rectx), short(recty));
  }
  else {
    ibuf = ibuf_tmp;
  }

  /* depth = 32 is intentionally left in, otherwise ALPHA channels
   * won't work... */
  quality = seq->strip->proxy->quality;
  ibuf->ftype = IMB_FTYPE_JPG;
  ibuf->foptions.quality = quality;

  /* unsupported feature only confuses other s/w */
  if (ibuf->planes == 32) {
    ibuf->planes = 24;
  }

  BLI_file_ensure_parent_dir_exists(filepath);

  const bool ok = IMB_saveiff(ibuf, filepath, IB_rect);
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
static bool seq_proxy_multiview_context_invalid(Sequence *seq,
                                                Scene *scene,
                                                const int view_id,
                                                MultiViewPrefixVars *prefix_vars)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if ((seq->type == SEQ_TYPE_IMAGE) && (seq->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    if (view_id == 0) {
      /* Clear on first use. */
      prefix_vars->prefix[0] = '\0';
      prefix_vars->ext = nullptr;

      char filepath[FILE_MAX];
      BLI_path_join(
          filepath, sizeof(filepath), seq->strip->dirpath, seq->strip->stripdata->filename);
      BLI_path_abs(filepath, BKE_main_blendfile_path_from_global());
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
static int seq_proxy_context_count(Sequence *seq, Scene *scene)
{
  int num_views = 1;

  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return 1;
  }

  switch (seq->type) {
    case SEQ_TYPE_MOVIE: {
      num_views = BLI_listbase_count(&seq->anims);
      break;
    }
    case SEQ_TYPE_IMAGE: {
      switch (seq->views_format) {
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

static bool seq_proxy_need_rebuild(Sequence *seq, anim *anim)
{
  if ((seq->strip->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) == 0) {
    return true;
  }

  IMB_Proxy_Size required_proxies = IMB_Proxy_Size(seq->strip->proxy->build_size_flags);
  int built_proxies = IMB_anim_proxy_get_existing(anim);
  return (required_proxies & built_proxies) != required_proxies;
}

bool SEQ_proxy_rebuild_context(Main *bmain,
                               Depsgraph *depsgraph,
                               Scene *scene,
                               Sequence *seq,
                               GSet *file_list,
                               ListBase *queue,
                               bool build_only_on_bad_performance)
{
  SeqIndexBuildContext *context;
  Sequence *nseq;
  LinkData *link;
  int num_files;
  int i;

  if (!seq->strip || !seq->strip->proxy) {
    return true;
  }

  if (!(seq->flag & SEQ_USE_PROXY)) {
    return true;
  }

  num_files = seq_proxy_context_count(seq, scene);

  MultiViewPrefixVars prefix_vars; /* Initialized by #seq_proxy_multiview_context_invalid. */
  for (i = 0; i < num_files; i++) {
    if (seq_proxy_multiview_context_invalid(seq, scene, i, &prefix_vars)) {
      continue;
    }

    /* Check if proxies are already built here, because actually opening anims takes a lot of
     * time. */
    seq_open_anim_file(scene, seq, false);
    StripAnim *sanim = static_cast<StripAnim *>(BLI_findlink(&seq->anims, i));
    if (sanim->anim && !seq_proxy_need_rebuild(seq, sanim->anim)) {
      continue;
    }

    SEQ_relations_sequence_free_anim(seq);

    context = static_cast<SeqIndexBuildContext *>(
        MEM_callocN(sizeof(SeqIndexBuildContext), "seq proxy rebuild context"));

    nseq = SEQ_sequence_dupli_recursive(scene, scene, nullptr, seq, 0);

    context->tc_flags = nseq->strip->proxy->build_tc_flags;
    context->size_flags = nseq->strip->proxy->build_size_flags;
    context->quality = nseq->strip->proxy->quality;
    context->overwrite = (nseq->strip->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) == 0;

    context->bmain = bmain;
    context->depsgraph = depsgraph;
    context->scene = scene;
    context->orig_seq = seq;
    context->orig_seq_uuid = seq->runtime.session_uuid;
    context->seq = nseq;

    context->view_id = i; /* only for images */

    if (nseq->type == SEQ_TYPE_MOVIE) {
      seq_open_anim_file(scene, nseq, true);
      sanim = static_cast<StripAnim *>(BLI_findlink(&nseq->anims, i));

      if (sanim->anim) {
        context->index_context = IMB_anim_index_rebuild_context(
            sanim->anim,
            IMB_Timecode_Type(context->tc_flags),
            context->size_flags,
            context->quality,
            context->overwrite,
            file_list,
            build_only_on_bad_performance);
      }
      if (!context->index_context) {
        MEM_freeN(context);
        return false;
      }
    }

    link = BLI_genericNodeN(context);
    BLI_addtail(queue, link);
  }

  return true;
}

void SEQ_proxy_rebuild(SeqIndexBuildContext *context, bool *stop, bool *do_update, float *progress)
{
  const bool overwrite = context->overwrite;
  SeqRenderData render_context;
  Sequence *seq = context->seq;
  Scene *scene = context->scene;
  Main *bmain = context->bmain;
  int timeline_frame;

  if (seq->type == SEQ_TYPE_MOVIE) {
    if (context->index_context) {
      IMB_anim_index_rebuild(context->index_context, stop, do_update, progress);
    }

    return;
  }

  if (!(seq->flag & SEQ_USE_PROXY)) {
    return;
  }

  /* that's why it is called custom... */
  if (seq->strip->proxy && seq->strip->proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    return;
  }

  /* fail safe code */
  int width, height;
  BKE_render_resolution(&scene->r, false, &width, &height);

  SEQ_render_new_render_data(
      bmain, context->depsgraph, context->scene, width, height, 100, false, &render_context);

  render_context.skip_cache = true;
  render_context.is_proxy_render = true;
  render_context.view_id = context->view_id;

  SeqRenderState state;
  seq_render_state_init(&state);

  for (timeline_frame = SEQ_time_left_handle_frame_get(scene, seq);
       timeline_frame < SEQ_time_right_handle_frame_get(scene, seq);
       timeline_frame++)
  {
    if (context->size_flags & IMB_PROXY_25) {
      seq_proxy_build_frame(&render_context, &state, seq, timeline_frame, 25, overwrite);
    }
    if (context->size_flags & IMB_PROXY_50) {
      seq_proxy_build_frame(&render_context, &state, seq, timeline_frame, 50, overwrite);
    }
    if (context->size_flags & IMB_PROXY_75) {
      seq_proxy_build_frame(&render_context, &state, seq, timeline_frame, 75, overwrite);
    }
    if (context->size_flags & IMB_PROXY_100) {
      seq_proxy_build_frame(&render_context, &state, seq, timeline_frame, 100, overwrite);
    }

    *progress = float(timeline_frame - SEQ_time_left_handle_frame_get(scene, seq)) /
                (SEQ_time_right_handle_frame_get(scene, seq) -
                 SEQ_time_left_handle_frame_get(scene, seq));
    *do_update = true;

    if (*stop || G.is_break) {
      break;
    }
  }
}

void SEQ_proxy_rebuild_finish(SeqIndexBuildContext *context, bool stop)
{
  if (context->index_context) {
    LISTBASE_FOREACH (StripAnim *, sanim, &context->seq->anims) {
      IMB_close_anim_proxies(sanim->anim);
    }

    IMB_anim_index_rebuild_finish(context->index_context, stop);
  }

  seq_free_sequence_recurse(nullptr, context->seq, true);

  MEM_freeN(context);
}

void SEQ_proxy_set(Sequence *seq, bool value)
{
  if (value) {
    seq->flag |= SEQ_USE_PROXY;
    if (seq->strip->proxy == nullptr) {
      seq->strip->proxy = seq_strip_proxy_alloc();
    }
  }
  else {
    seq->flag &= ~SEQ_USE_PROXY;
  }
}

void seq_proxy_index_dir_set(anim *anim, const char *base_dir)
{
  char dirname[FILE_MAX];
  char filename[FILE_MAXFILE];

  IMB_anim_get_filename(anim, filename, FILE_MAXFILE);
  BLI_path_join(dirname, sizeof(dirname), base_dir, filename);
  IMB_anim_set_index_dir(anim, dirname);
}

void free_proxy_seq(Sequence *seq)
{
  if (seq->strip && seq->strip->proxy && seq->strip->proxy->anim) {
    IMB_free_anim(seq->strip->proxy->anim);
    seq->strip->proxy->anim = nullptr;
  }
}

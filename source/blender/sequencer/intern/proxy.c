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
 *
 * - Blender Foundation, 2003-2009
 * - Peter Schlaile <peter [at] schlaile [dot] de> 2005/2006
 */

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

#include "SEQ_sequencer.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_metadata.h"

#include "proxy.h"
#include "render.h"
#include "sequencer.h"

typedef struct SeqIndexBuildContext {
  struct IndexBuildContext *index_context;

  int tc_flags;
  int size_flags;
  int quality;
  bool overwrite;
  int view_id;

  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  Sequence *seq, *orig_seq;
} SeqIndexBuildContext;

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

bool seq_proxy_get_custom_file_fname(Sequence *seq, char *name, const int view_id)
{
  char fname[FILE_MAXFILE];
  char suffix[24];
  StripProxy *proxy = seq->strip->proxy;

  if (proxy == NULL) {
    return false;
  }

  BLI_join_dirfile(fname, PROXY_MAXFILE, proxy->dir, proxy->file);
  BLI_path_abs(fname, BKE_main_blendfile_path_from_global());

  if (view_id > 0) {
    BLI_snprintf(suffix, sizeof(suffix), "_%d", view_id);
    /* TODO(sergey): This will actually append suffix after extension
     * which is weird but how was originally coded in multi-view branch.
     */
    BLI_snprintf(name, PROXY_MAXFILE, "%s_%s", fname, suffix);
  }
  else {
    BLI_strncpy(name, fname, PROXY_MAXFILE);
  }

  return true;
}

static bool seq_proxy_get_fname(Editing *ed,
                                Sequence *seq,
                                int timeline_frame,
                                eSpaceSeq_Proxy_RenderSize render_size,
                                char *name,
                                const int view_id)
{
  char dir[PROXY_MAXFILE];
  char suffix[24] = {'\0'};
  StripProxy *proxy = seq->strip->proxy;

  if (proxy == NULL) {
    return false;
  }

  /* Multi-view suffix. */
  if (view_id > 0) {
    BLI_snprintf(suffix, sizeof(suffix), "_%d", view_id);
  }

  /* Per strip with Custom file situation is handled separately. */
  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE &&
      ed->proxy_storage != SEQ_EDIT_PROXY_DIR_STORAGE) {
    if (seq_proxy_get_custom_file_fname(seq, name, view_id)) {
      return true;
    }
  }

  if (ed->proxy_storage == SEQ_EDIT_PROXY_DIR_STORAGE) {
    /* Per project default. */
    if (ed->proxy_dir[0] == 0) {
      BLI_strncpy(dir, "//BL_proxy", sizeof(dir));
    }
    else { /* Per project with custom dir. */
      BLI_strncpy(dir, ed->proxy_dir, sizeof(dir));
    }
    BLI_path_abs(name, BKE_main_blendfile_path_from_global());
  }
  else {
    /* Pre strip with custom dir. */
    if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_DIR) {
      BLI_strncpy(dir, seq->strip->proxy->dir, sizeof(dir));
    }
    else { /* Per strip default. */
      BLI_snprintf(dir, PROXY_MAXFILE, "%s/BL_proxy", seq->strip->dir);
    }
  }

  /* Proxy size number to be used in path. */
  int proxy_size_number = SEQ_rendersize_to_scale_factor(render_size) * 100;

  BLI_snprintf(name,
               PROXY_MAXFILE,
               "%s/images/%d/%s_proxy%s",
               dir,
               proxy_size_number,
               SEQ_render_give_stripelem(seq, timeline_frame)->name,
               suffix);
  BLI_path_abs(name, BKE_main_blendfile_path_from_global());
  strcat(name, ".jpg");

  return true;
}

bool SEQ_can_use_proxy(Sequence *seq, int psize)
{
  if (seq->strip->proxy == NULL) {
    return false;
  }
  short size_flags = seq->strip->proxy->build_size_flags;
  return (seq->flag & SEQ_USE_PROXY) != 0 && psize != IMB_PROXY_NONE && (size_flags & psize) != 0;
}

ImBuf *seq_proxy_fetch(const SeqRenderData *context, Sequence *seq, int timeline_frame)
{
  char name[PROXY_MAXFILE];
  StripProxy *proxy = seq->strip->proxy;
  const eSpaceSeq_Proxy_RenderSize psize = context->preview_render_size;
  Editing *ed = context->scene->ed;
  StripAnim *sanim;

  /* only use proxies, if they are enabled (even if present!) */
  if (!SEQ_can_use_proxy(seq, SEQ_rendersize_to_proxysize(psize))) {
    return NULL;
  }

  if (proxy->storage & SEQ_STORAGE_PROXY_CUSTOM_FILE) {
    int frameno = (int)seq_give_frame_index(seq, timeline_frame) + seq->anim_startofs;
    if (proxy->anim == NULL) {
      if (seq_proxy_get_fname(ed, seq, timeline_frame, psize, name, context->view_id) == 0) {
        return NULL;
      }

      proxy->anim = openanim(name, IB_rect, 0, seq->strip->colorspace_settings.name);
    }
    if (proxy->anim == NULL) {
      return NULL;
    }

    seq_open_anim_file(context->scene, seq, true);
    sanim = seq->anims.first;

    frameno = IMB_anim_index_get_frame_index(
        sanim ? sanim->anim : NULL, seq->strip->proxy->tc, frameno);

    return IMB_anim_absolute(proxy->anim, frameno, IMB_TC_NONE, IMB_PROXY_NONE);
  }

  if (seq_proxy_get_fname(ed, seq, timeline_frame, psize, name, context->view_id) == 0) {
    return NULL;
  }

  if (BLI_exists(name)) {
    ImBuf *ibuf = IMB_loadiffname(name, IB_rect, NULL);

    if (ibuf) {
      seq_imbuf_assign_spaces(context->scene, ibuf);
    }

    return ibuf;
  }

  return NULL;
}

static void seq_proxy_build_frame(const SeqRenderData *context,
                                  SeqRenderState *state,
                                  Sequence *seq,
                                  int timeline_frame,
                                  int proxy_render_size,
                                  const bool overwrite)
{
  char name[PROXY_MAXFILE];
  int quality;
  int rectx, recty;
  int ok;
  ImBuf *ibuf_tmp, *ibuf;
  Editing *ed = context->scene->ed;

  if (!seq_proxy_get_fname(ed, seq, timeline_frame, proxy_render_size, name, context->view_id)) {
    return;
  }

  if (!overwrite && BLI_exists(name)) {
    return;
  }

  ibuf_tmp = seq_render_strip(context, state, seq, timeline_frame);

  rectx = (proxy_render_size * ibuf_tmp->x) / 100;
  recty = (proxy_render_size * ibuf_tmp->y) / 100;

  if (ibuf_tmp->x != rectx || ibuf_tmp->y != recty) {
    ibuf = IMB_dupImBuf(ibuf_tmp);
    IMB_metadata_copy(ibuf, ibuf_tmp);
    IMB_freeImBuf(ibuf_tmp);
    IMB_scalefastImBuf(ibuf, (short)rectx, (short)recty);
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

  BLI_make_existing_file(name);

  ok = IMB_saveiff(ibuf, name, IB_rect | IB_zbuf | IB_zbuffloat);
  if (ok == 0) {
    perror(name);
  }

  IMB_freeImBuf(ibuf);
}

/**
 * Returns whether the file this context would read from even exist,
 * if not, don't create the context
 */
static bool seq_proxy_multiview_context_invalid(Sequence *seq, Scene *scene, const int view_id)
{
  if ((scene->r.scemode & R_MULTIVIEW) == 0) {
    return false;
  }

  if ((seq->type == SEQ_TYPE_IMAGE) && (seq->views_format == R_IMF_VIEWS_INDIVIDUAL)) {
    static char prefix[FILE_MAX];
    static const char *ext = NULL;
    char str[FILE_MAX];

    if (view_id == 0) {
      char path[FILE_MAX];
      BLI_join_dirfile(path, sizeof(path), seq->strip->dir, seq->strip->stripdata->name);
      BLI_path_abs(path, BKE_main_blendfile_path_from_global());
      BKE_scene_multiview_view_prefix_get(scene, path, prefix, &ext);
    }
    else {
      prefix[0] = '\0';
    }

    if (prefix[0] == '\0') {
      return view_id != 0;
    }

    seq_multiview_name(scene, view_id, prefix, ext, str, FILE_MAX);

    if (BLI_access(str, R_OK) == 0) {
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

bool SEQ_proxy_rebuild_context(Main *bmain,
                               Depsgraph *depsgraph,
                               Scene *scene,
                               Sequence *seq,
                               struct GSet *file_list,
                               ListBase *queue)
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

  for (i = 0; i < num_files; i++) {
    if (seq_proxy_multiview_context_invalid(seq, scene, i)) {
      continue;
    }

    context = MEM_callocN(sizeof(SeqIndexBuildContext), "seq proxy rebuild context");

    nseq = BKE_sequence_dupli_recursive(scene, scene, NULL, seq, 0);

    context->tc_flags = nseq->strip->proxy->build_tc_flags;
    context->size_flags = nseq->strip->proxy->build_size_flags;
    context->quality = nseq->strip->proxy->quality;
    context->overwrite = (nseq->strip->proxy->build_flags & SEQ_PROXY_SKIP_EXISTING) == 0;

    context->bmain = bmain;
    context->depsgraph = depsgraph;
    context->scene = scene;
    context->orig_seq = seq;
    context->seq = nseq;

    context->view_id = i; /* only for images */

    if (nseq->type == SEQ_TYPE_MOVIE) {
      StripAnim *sanim;

      seq_open_anim_file(scene, nseq, true);
      sanim = BLI_findlink(&nseq->anims, i);

      if (sanim->anim) {
        context->index_context = IMB_anim_index_rebuild_context(sanim->anim,
                                                                context->tc_flags,
                                                                context->size_flags,
                                                                context->quality,
                                                                context->overwrite,
                                                                file_list);
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

void SEQ_proxy_rebuild(SeqIndexBuildContext *context,
                       short *stop,
                       short *do_update,
                       float *progress)
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

  SEQ_render_new_render_data(bmain,
                             context->depsgraph,
                             context->scene,
                             roundf((scene->r.size * (float)scene->r.xsch) / 100.0f),
                             roundf((scene->r.size * (float)scene->r.ysch) / 100.0f),
                             100,
                             false,
                             &render_context);

  render_context.skip_cache = true;
  render_context.is_proxy_render = true;
  render_context.view_id = context->view_id;

  SeqRenderState state;
  seq_render_state_init(&state);

  for (timeline_frame = seq->startdisp + seq->startstill;
       timeline_frame < seq->enddisp - seq->endstill;
       timeline_frame++) {
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

    *progress = (float)(timeline_frame - seq->startdisp - seq->startstill) /
                (seq->enddisp - seq->endstill - seq->startdisp - seq->startstill);
    *do_update = true;

    if (*stop || G.is_break) {
      break;
    }
  }
}

void SEQ_proxy_rebuild_finish(SeqIndexBuildContext *context, bool stop)
{
  if (context->index_context) {
    StripAnim *sanim;

    for (sanim = context->seq->anims.first; sanim; sanim = sanim->next) {
      IMB_close_anim_proxies(sanim->anim);
    }

    for (sanim = context->orig_seq->anims.first; sanim; sanim = sanim->next) {
      IMB_close_anim_proxies(sanim->anim);
    }

    IMB_anim_index_rebuild_finish(context->index_context, stop);
  }

  seq_free_sequence_recurse(NULL, context->seq, true);

  MEM_freeN(context);
}

void SEQ_proxy_set(struct Sequence *seq, bool value)
{
  if (value) {
    seq->flag |= SEQ_USE_PROXY;
    if (seq->strip->proxy == NULL) {
      seq->strip->proxy = MEM_callocN(sizeof(struct StripProxy), "StripProxy");
      seq->strip->proxy->quality = 90;
      seq->strip->proxy->build_tc_flags = SEQ_PROXY_TC_ALL;
      seq->strip->proxy->build_size_flags = SEQ_PROXY_IMAGE_SIZE_25;
    }
  }
  else {
    seq->flag &= ~SEQ_USE_PROXY;
  }
}

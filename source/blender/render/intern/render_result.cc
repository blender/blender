/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_hash_md5.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_listbase.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.h"
#include "BKE_camera.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_image_save.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_openexr.h"

#include "GPU_texture.h"

#include "RE_engine.h"

#include "render_result.h"
#include "render_types.h"

/********************************** Free *************************************/

static void render_result_views_free(RenderResult *rr)
{
  while (rr->views.first) {
    RenderView *rv = static_cast<RenderView *>(rr->views.first);
    BLI_remlink(&rr->views, rv);

    RE_RenderByteBuffer_data_free(&rv->byte_buffer);
    RE_RenderBuffer_data_free(&rv->combined_buffer);
    RE_RenderBuffer_data_free(&rv->z_buffer);

    MEM_freeN(rv);
  }

  rr->have_combined = false;
}

void render_result_free(RenderResult *rr)
{
  if (rr == nullptr) {
    return;
  }

  while (rr->layers.first) {
    RenderLayer *rl = static_cast<RenderLayer *>(rr->layers.first);

    while (rl->passes.first) {
      RenderPass *rpass = static_cast<RenderPass *>(rl->passes.first);

      RE_RenderBuffer_data_free(&rpass->buffer);

      BLI_freelinkN(&rl->passes, rpass);
    }
    BLI_remlink(&rr->layers, rl);
    MEM_freeN(rl);
  }

  render_result_views_free(rr);

  RE_RenderByteBuffer_data_free(&rr->byte_buffer);
  RE_RenderBuffer_data_free(&rr->combined_buffer);
  RE_RenderBuffer_data_free(&rr->z_buffer);

  if (rr->text) {
    MEM_freeN(rr->text);
  }
  if (rr->error) {
    MEM_freeN(rr->error);
  }

  BKE_stamp_data_free(rr->stamp_data);

  MEM_freeN(rr);
}

void render_result_free_list(ListBase *lb, RenderResult *rr)
{
  RenderResult *rrnext;

  for (; rr; rr = rrnext) {
    rrnext = rr->next;

    if (lb && lb->first) {
      BLI_remlink(lb, rr);
    }

    render_result_free(rr);
  }
}

void render_result_free_gpu_texture_caches(RenderResult *rr)
{
  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    LISTBASE_FOREACH (RenderPass *, rpass, &rl->passes) {
      if (rpass->buffer.gpu_texture) {
        GPU_texture_free(rpass->buffer.gpu_texture);
        rpass->buffer.gpu_texture = nullptr;
      }
    }
  }
}

/********************************* multiview *************************************/

void render_result_views_shallowcopy(RenderResult *dst, RenderResult *src)
{
  if (dst == nullptr || src == nullptr) {
    return;
  }

  LISTBASE_FOREACH (RenderView *, rview, &src->views) {
    RenderView *rv;

    rv = MEM_cnew<RenderView>("new render view");
    BLI_addtail(&dst->views, rv);

    STRNCPY(rv->name, rview->name);

    rv->combined_buffer = rview->combined_buffer;
    rv->z_buffer = rview->z_buffer;
    rv->byte_buffer = rview->byte_buffer;
  }
}

void render_result_views_shallowdelete(RenderResult *rr)
{
  if (rr == nullptr) {
    return;
  }

  while (rr->views.first) {
    RenderView *rv = static_cast<RenderView *>(rr->views.first);
    BLI_remlink(&rr->views, rv);
    MEM_freeN(rv);
  }
}

/********************************** New **************************************/

static void render_layer_allocate_pass(RenderResult *rr, RenderPass *rp)
{
  if (rp->buffer.data != nullptr) {
    return;
  }

  const size_t rectsize = size_t(rr->rectx) * rr->recty * rp->channels;
  float *buffer_data = MEM_cnew_array<float>(rectsize, rp->name);

  rp->buffer = RE_RenderBuffer_new(buffer_data);

  if (STREQ(rp->name, RE_PASSNAME_VECTOR)) {
    /* initialize to max speed */
    for (int x = rectsize - 1; x >= 0; x--) {
      buffer_data[x] = PASS_VECTOR_MAX;
    }
  }
  else if (STREQ(rp->name, RE_PASSNAME_Z)) {
    for (int x = rectsize - 1; x >= 0; x--) {
      buffer_data[x] = 10e10;
    }
  }
}

RenderPass *render_layer_add_pass(RenderResult *rr,
                                  RenderLayer *rl,
                                  int channels,
                                  const char *name,
                                  const char *viewname,
                                  const char *chan_id,
                                  const bool allocate)
{
  const int view_id = BLI_findstringindex(&rr->views, viewname, offsetof(RenderView, name));
  RenderPass *rpass = MEM_cnew<RenderPass>(name);

  rpass->channels = channels;
  rpass->rectx = rl->rectx;
  rpass->recty = rl->recty;
  rpass->view_id = view_id;

  STRNCPY(rpass->name, name);
  STRNCPY(rpass->chan_id, chan_id);
  STRNCPY(rpass->view, viewname);
  RE_render_result_full_channel_name(
      rpass->fullname, nullptr, rpass->name, rpass->view, rpass->chan_id, -1);

  if (rl->exrhandle) {
    int a;
    for (a = 0; a < channels; a++) {
      char passname[EXR_PASS_MAXNAME];
      RE_render_result_full_channel_name(
          passname, nullptr, rpass->name, nullptr, rpass->chan_id, a);
      IMB_exr_add_channel(rl->exrhandle, rl->name, passname, viewname, 0, 0, nullptr, false);
    }
  }

  BLI_addtail(&rl->passes, rpass);

  if (allocate) {
    render_layer_allocate_pass(rr, rpass);
  }
  else {
    /* The result contains non-allocated pass now, so tag it as such. */
    rr->passes_allocated = false;
  }

  return rpass;
}

RenderResult *render_result_new(Render *re,
                                rcti *partrct,
                                const char *layername,
                                const char *viewname)
{
  RenderResult *rr;
  RenderLayer *rl;
  int rectx, recty;

  rectx = BLI_rcti_size_x(partrct);
  recty = BLI_rcti_size_y(partrct);

  if (rectx <= 0 || recty <= 0) {
    return nullptr;
  }

  rr = MEM_cnew<RenderResult>("new render result");
  rr->rectx = rectx;
  rr->recty = recty;

  /* tilerect is relative coordinates within render disprect. do not subtract crop yet */
  rr->tilerect.xmin = partrct->xmin - re->disprect.xmin;
  rr->tilerect.xmax = partrct->xmax - re->disprect.xmin;
  rr->tilerect.ymin = partrct->ymin - re->disprect.ymin;
  rr->tilerect.ymax = partrct->ymax - re->disprect.ymin;

  rr->passes_allocated = false;

  render_result_views_new(rr, &re->r);

  /* Check render-data for amount of layers. */
  FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer) {
    if (layername && layername[0]) {
      if (!STREQ(view_layer->name, layername)) {
        continue;
      }
    }

    rl = MEM_cnew<RenderLayer>("new render layer");
    BLI_addtail(&rr->layers, rl);

    STRNCPY(rl->name, view_layer->name);
    rl->layflag = view_layer->layflag;

    rl->passflag = view_layer->passflag;

    rl->rectx = rectx;
    rl->recty = recty;

    LISTBASE_FOREACH (RenderView *, rv, &rr->views) {
      const char *view = rv->name;

      if (viewname && viewname[0]) {
        if (!STREQ(view, viewname)) {
          continue;
        }
      }

      /* A render-layer should always have a "Combined" pass. */
      render_layer_add_pass(rr, rl, 4, "Combined", view, "RGBA", false);
    }
  }
  FOREACH_VIEW_LAYER_TO_RENDER_END;

  /* Preview-render doesn't do layers, so we make a default one. */
  if (BLI_listbase_is_empty(&rr->layers) && !(layername && layername[0])) {
    rl = MEM_cnew<RenderLayer>("new render layer");
    BLI_addtail(&rr->layers, rl);

    rl->rectx = rectx;
    rl->recty = recty;

    LISTBASE_FOREACH (RenderView *, rv, &rr->views) {
      const char *view = rv->name;

      if (viewname && viewname[0]) {
        if (!STREQ(view, viewname)) {
          continue;
        }
      }

      /* A render-layer should always have a "Combined" pass. */
      render_layer_add_pass(rr, rl, 4, RE_PASSNAME_COMBINED, view, "RGBA", false);
    }

    /* NOTE: this has to be in sync with `scene.cc`. */
    rl->layflag = SCE_LAY_FLAG_DEFAULT;
    rl->passflag = SCE_PASS_COMBINED;

    re->single_view_layer[0] = '\0';
  }

  /* Border render; calculate offset for use in compositor. compo is centralized coords. */
  /* XXX(ton): obsolete? I now use it for drawing border render offset. */
  rr->xof = re->disprect.xmin + BLI_rcti_cent_x(&re->disprect) - (re->winx / 2);
  rr->yof = re->disprect.ymin + BLI_rcti_cent_y(&re->disprect) - (re->winy / 2);

  return rr;
}

void render_result_passes_allocated_ensure(RenderResult *rr)
{
  if (rr == nullptr) {
    /* Happens when the result was not yet allocated for the current scene or slot configuration.
     */
    return;
  }

  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    LISTBASE_FOREACH (RenderPass *, rp, &rl->passes) {
      if (rl->exrhandle != nullptr && !STREQ(rp->name, RE_PASSNAME_COMBINED)) {
        continue;
      }

      render_layer_allocate_pass(rr, rp);
    }
  }

  rr->passes_allocated = true;
}

void render_result_clone_passes(Render *re, RenderResult *rr, const char *viewname)
{
  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    RenderLayer *main_rl = RE_GetRenderLayer(re->result, rl->name);
    if (!main_rl) {
      continue;
    }

    LISTBASE_FOREACH (RenderPass *, main_rp, &main_rl->passes) {
      if (viewname && viewname[0] && !STREQ(main_rp->view, viewname)) {
        continue;
      }

      /* Compare fullname to make sure that the view also is equal. */
      RenderPass *rp = static_cast<RenderPass *>(
          BLI_findstring(&rl->passes, main_rp->fullname, offsetof(RenderPass, fullname)));
      if (!rp) {
        render_layer_add_pass(
            rr, rl, main_rp->channels, main_rp->name, main_rp->view, main_rp->chan_id, false);
      }
    }
  }
}

void RE_create_render_pass(RenderResult *rr,
                           const char *name,
                           int channels,
                           const char *chan_id,
                           const char *layername,
                           const char *viewname,
                           const bool allocate)
{
  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    if (layername && layername[0] && !STREQ(rl->name, layername)) {
      continue;
    }

    LISTBASE_FOREACH (RenderView *, rv, &rr->views) {
      const char *view = rv->name;

      if (viewname && viewname[0] && !STREQ(view, viewname)) {
        continue;
      }

      /* Ensure that the pass doesn't exist yet. */
      bool pass_exists = false;
      LISTBASE_FOREACH (RenderPass *, rp, &rl->passes) {
        if (STREQ(rp->name, name) && STREQ(rp->view, view)) {
          pass_exists = true;
          break;
        }
      }

      if (!pass_exists) {
        render_layer_add_pass(rr, rl, channels, name, view, chan_id, allocate);
      }
    }
  }
}

void RE_pass_set_buffer_data(RenderPass *pass, float *data)
{
  RE_RenderBuffer_assign_data(&pass->buffer, data);
}

GPUTexture *RE_pass_ensure_gpu_texture_cache(Render *re, RenderPass *rpass)
{
  if (rpass->buffer.gpu_texture) {
    return rpass->buffer.gpu_texture;
  }
  if (rpass->buffer.data == nullptr) {
    return nullptr;
  }

  const eGPUTextureFormat format = (rpass->channels == 1) ? GPU_R16F :
                                   (rpass->channels == 3) ? GPU_RGB16F :
                                                            GPU_RGBA16F;

  rpass->buffer.gpu_texture = GPU_texture_create_2d("RenderBuffer.gpu_texture",
                                                    rpass->rectx,
                                                    rpass->recty,
                                                    1,
                                                    format,
                                                    GPU_TEXTURE_USAGE_GENERAL,
                                                    nullptr);

  if (rpass->buffer.gpu_texture) {
    GPU_texture_update(rpass->buffer.gpu_texture, GPU_DATA_FLOAT, rpass->buffer.data);
    re->result_has_gpu_texture_caches = true;
  }

  return rpass->buffer.gpu_texture;
}

void RE_render_result_full_channel_name(char *fullname,
                                        const char *layname,
                                        const char *passname,
                                        const char *viewname,
                                        const char *chan_id,
                                        const int channel)
{
  /* OpenEXR compatible full channel name. */
  const char *strings[4];
  int strings_len = 0;

  if (layname && layname[0]) {
    strings[strings_len++] = layname;
  }
  if (passname && passname[0]) {
    strings[strings_len++] = passname;
  }
  if (viewname && viewname[0]) {
    strings[strings_len++] = viewname;
  }

  char token[2];
  if (channel >= 0) {
    ARRAY_SET_ITEMS(token, chan_id[channel], '\0');
    strings[strings_len++] = token;
  }

  BLI_string_join_array_by_sep_char(fullname, EXR_PASS_MAXNAME, '.', strings, strings_len);
}

static int passtype_from_name(const char *name)
{
  const char delim[] = {'.', '\0'};
  const char *sep, *suf;
  int len = BLI_str_partition(name, delim, &sep, &suf);

#define CHECK_PASS(NAME) \
  if (STREQLEN(name, RE_PASSNAME_##NAME, len)) { \
    return SCE_PASS_##NAME; \
  } \
  ((void)0)

  CHECK_PASS(COMBINED);
  CHECK_PASS(Z);
  CHECK_PASS(VECTOR);
  CHECK_PASS(NORMAL);
  CHECK_PASS(UV);
  CHECK_PASS(EMIT);
  CHECK_PASS(SHADOW);
  CHECK_PASS(AO);
  CHECK_PASS(ENVIRONMENT);
  CHECK_PASS(INDEXOB);
  CHECK_PASS(INDEXMA);
  CHECK_PASS(MIST);
  CHECK_PASS(DIFFUSE_DIRECT);
  CHECK_PASS(DIFFUSE_INDIRECT);
  CHECK_PASS(DIFFUSE_COLOR);
  CHECK_PASS(GLOSSY_DIRECT);
  CHECK_PASS(GLOSSY_INDIRECT);
  CHECK_PASS(GLOSSY_COLOR);
  CHECK_PASS(TRANSM_DIRECT);
  CHECK_PASS(TRANSM_INDIRECT);
  CHECK_PASS(TRANSM_COLOR);
  CHECK_PASS(SUBSURFACE_DIRECT);
  CHECK_PASS(SUBSURFACE_INDIRECT);
  CHECK_PASS(SUBSURFACE_COLOR);

#undef CHECK_PASS
  return 0;
}

/* callbacks for render_result_new_from_exr */
static void *ml_addlayer_cb(void *base, const char *str)
{
  RenderResult *rr = static_cast<RenderResult *>(base);

  RenderLayer *rl = MEM_cnew<RenderLayer>("new render layer");
  BLI_addtail(&rr->layers, rl);

  BLI_strncpy(rl->name, str, EXR_LAY_MAXNAME);
  return rl;
}

static void ml_addpass_cb(void *base,
                          void *lay,
                          const char *name,
                          float *rect,
                          int totchan,
                          const char *chan_id,
                          const char *view)
{
  RenderResult *rr = static_cast<RenderResult *>(base);
  RenderLayer *rl = static_cast<RenderLayer *>(lay);
  RenderPass *rpass = MEM_cnew<RenderPass>("loaded pass");

  BLI_addtail(&rl->passes, rpass);
  rpass->channels = totchan;
  rl->passflag |= passtype_from_name(name);

  /* channel id chars */
  STRNCPY(rpass->chan_id, chan_id);

  RE_pass_set_buffer_data(rpass, rect);

  STRNCPY(rpass->name, name);
  STRNCPY(rpass->view, view);
  RE_render_result_full_channel_name(rpass->fullname, nullptr, name, view, rpass->chan_id, -1);

  if (view[0] != '\0') {
    rpass->view_id = BLI_findstringindex(&rr->views, view, offsetof(RenderView, name));
  }
  else {
    rpass->view_id = 0;
  }
}

static void *ml_addview_cb(void *base, const char *str)
{
  RenderResult *rr = static_cast<RenderResult *>(base);

  RenderView *rv = MEM_cnew<RenderView>("new render view");
  STRNCPY(rv->name, str);

  /* For stereo drawing we need to ensure:
   * STEREO_LEFT_NAME  == STEREO_LEFT_ID and
   * STEREO_RIGHT_NAME == STEREO_RIGHT_ID */

  if (STREQ(str, STEREO_LEFT_NAME)) {
    BLI_addhead(&rr->views, rv);
  }
  else if (STREQ(str, STEREO_RIGHT_NAME)) {
    RenderView *left_rv = static_cast<RenderView *>(
        BLI_findstring(&rr->views, STEREO_LEFT_NAME, offsetof(RenderView, name)));

    if (left_rv == nullptr) {
      BLI_addhead(&rr->views, rv);
    }
    else {
      BLI_insertlinkafter(&rr->views, left_rv, rv);
    }
  }
  else {
    BLI_addtail(&rr->views, rv);
  }

  return rv;
}

static int order_render_passes(const void *a, const void *b)
{
  /* 1 if `a` is after `b`. */
  RenderPass *rpa = (RenderPass *)a;
  RenderPass *rpb = (RenderPass *)b;
  uint passtype_a = passtype_from_name(rpa->name);
  uint passtype_b = passtype_from_name(rpb->name);

  /* Render passes with default type always go first. */
  if (passtype_b && !passtype_a) {
    return 1;
  }
  if (passtype_a && !passtype_b) {
    return 0;
  }

  if (passtype_a && passtype_b) {
    if (passtype_a > passtype_b) {
      return 1;
    }
    if (passtype_a < passtype_b) {
      return 0;
    }
  }
  else {
    int cmp = strncmp(rpa->name, rpb->name, EXR_PASS_MAXNAME);
    if (cmp > 0) {
      return 1;
    }
    if (cmp < 0) {
      return 0;
    }
  }

  /* they have the same type */
  /* left first */
  if (STREQ(rpa->view, STEREO_LEFT_NAME)) {
    return 0;
  }
  if (STREQ(rpb->view, STEREO_LEFT_NAME)) {
    return 1;
  }

  /* right second */
  if (STREQ(rpa->view, STEREO_RIGHT_NAME)) {
    return 0;
  }
  if (STREQ(rpb->view, STEREO_RIGHT_NAME)) {
    return 1;
  }

  /* remaining in ascending id order */
  return (rpa->view_id < rpb->view_id);
}

RenderResult *render_result_new_from_exr(
    void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty)
{
  RenderResult *rr = MEM_cnew<RenderResult>(__func__);
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  rr->rectx = rectx;
  rr->recty = recty;

  IMB_exr_multilayer_convert(exrhandle, rr, ml_addview_cb, ml_addlayer_cb, ml_addpass_cb);

  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    rl->rectx = rectx;
    rl->recty = recty;

    BLI_listbase_sort(&rl->passes, order_render_passes);

    LISTBASE_FOREACH (RenderPass *, rpass, &rl->passes) {
      rpass->rectx = rectx;
      rpass->recty = recty;

      if (rpass->channels >= 3) {
        IMB_colormanagement_transform(rpass->buffer.data,
                                      rpass->rectx,
                                      rpass->recty,
                                      rpass->channels,
                                      colorspace,
                                      to_colorspace,
                                      predivide);
      }
    }
  }

  return rr;
}

void render_result_view_new(RenderResult *rr, const char *viewname)
{
  RenderView *rv = MEM_cnew<RenderView>("new render view");
  BLI_addtail(&rr->views, rv);
  STRNCPY(rv->name, viewname);
}

void render_result_views_new(RenderResult *rr, const RenderData *rd)
{
  /* clear previously existing views - for sequencer */
  render_result_views_free(rr);

  /* check renderdata for amount of views */
  if (rd->scemode & R_MULTIVIEW) {
    LISTBASE_FOREACH (SceneRenderView *, srv, &rd->views) {
      if (BKE_scene_multiview_is_render_view_active(rd, srv) == false) {
        continue;
      }
      render_result_view_new(rr, srv->name);
    }
  }

  /* we always need at least one view */
  if (BLI_listbase_count_at_most(&rr->views, 1) == 0) {
    render_result_view_new(rr, "");
  }
}

/*********************************** Merge ***********************************/

static void do_merge_tile(
    RenderResult *rr, RenderResult *rrpart, float *target, float *tile, int pixsize)
{
  int y, tilex, tiley;
  size_t ofs, copylen;

  copylen = tilex = rrpart->rectx;
  tiley = rrpart->recty;

  ofs = (size_t(rrpart->tilerect.ymin) * rr->rectx + rrpart->tilerect.xmin);
  target += pixsize * ofs;

  copylen *= sizeof(float) * pixsize;
  tilex *= pixsize;
  ofs = pixsize * rr->rectx;

  for (y = 0; y < tiley; y++) {
    memcpy(target, tile, copylen);
    target += ofs;
    tile += tilex;
  }
}

void render_result_merge(RenderResult *rr, RenderResult *rrpart)
{
  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    RenderLayer *rlp = RE_GetRenderLayer(rrpart, rl->name);

    if (rlp) {
      /* Passes are allocated in sync. */
      for (RenderPass *rpass = static_cast<RenderPass *>(rl->passes.first),
                      *rpassp = static_cast<RenderPass *>(rlp->passes.first);
           rpass && rpassp;
           rpass = rpass->next)
      {
        /* For save buffers, skip any passes that are only saved to disk. */
        if (rpass->buffer.data == nullptr || rpassp->buffer.data == nullptr) {
          continue;
        }
        /* Render-result have all passes, render-part only the active view's passes. */
        if (!STREQ(rpassp->fullname, rpass->fullname)) {
          continue;
        }

        do_merge_tile(rr, rrpart, rpass->buffer.data, rpassp->buffer.data, rpass->channels);

        /* manually get next render pass */
        rpassp = rpassp->next;
      }
    }
  }
}

/**************************** Single Layer Rendering *************************/

void render_result_single_layer_begin(Render *re)
{
  /* all layers except the active one get temporally pushed away */

  /* officially pushed result should be nullptr... error can happen with do_seq */
  RE_FreeRenderResult(re->pushedresult);

  re->pushedresult = re->result;
  re->result = nullptr;
}

void render_result_single_layer_end(Render *re)
{
  if (re->result == nullptr) {
    printf("pop render result error; no current result!\n");
    return;
  }

  if (!re->pushedresult) {
    return;
  }

  if (re->pushedresult->rectx == re->result->rectx && re->pushedresult->recty == re->result->recty)
  {
    /* find which layer in re->pushedresult should be replaced */
    RenderLayer *rl = static_cast<RenderLayer *>(re->result->layers.first);

    /* render result should be empty after this */
    BLI_remlink(&re->result->layers, rl);

    /* reconstruct render result layers */
    LISTBASE_FOREACH (ViewLayer *, view_layer, &re->scene->view_layers) {
      if (STREQ(view_layer->name, re->single_view_layer)) {
        BLI_addtail(&re->result->layers, rl);
      }
      else {
        RenderLayer *rlpush = RE_GetRenderLayer(re->pushedresult, view_layer->name);
        if (rlpush) {
          BLI_remlink(&re->pushedresult->layers, rlpush);
          BLI_addtail(&re->result->layers, rlpush);
        }
      }
    }
  }

  RE_FreeRenderResult(re->pushedresult);
  re->pushedresult = nullptr;
}

bool render_result_exr_file_read_path(RenderResult *rr,
                                      RenderLayer *rl_single,
                                      ReportList *reports,
                                      const char *filepath)
{
  void *exrhandle = IMB_exr_get_handle();
  int rectx, recty;

  if (!IMB_exr_begin_read(exrhandle, filepath, &rectx, &recty, false)) {
    IMB_exr_close(exrhandle);
    return false;
  }

  ListBase layers = (rr) ? rr->layers : ListBase{rl_single, rl_single};
  const int expected_rectx = (rr) ? rr->rectx : rl_single->rectx;
  const int expected_recty = (rr) ? rr->recty : rl_single->recty;
  bool found_channels = false;

  if (rectx != expected_rectx || recty != expected_recty) {
    BKE_reportf(reports,
                RPT_ERROR,
                "reading render result: dimensions don't match, expected %dx%d",
                expected_rectx,
                expected_recty);
    IMB_exr_close(exrhandle);
    return true;
  }

  LISTBASE_FOREACH (RenderLayer *, rl, &layers) {
    if (rl_single && rl_single != rl) {
      continue;
    }

    /* passes are allocated in sync */
    LISTBASE_FOREACH (RenderPass *, rpass, &rl->passes) {
      const int xstride = rpass->channels;
      const int ystride = xstride * rectx;
      int a;
      char fullname[EXR_PASS_MAXNAME];

      for (a = 0; a < xstride; a++) {
        RE_render_result_full_channel_name(
            fullname, nullptr, rpass->name, rpass->view, rpass->chan_id, a);

        if (IMB_exr_set_channel(
                exrhandle, rl->name, fullname, xstride, ystride, rpass->buffer.data + a)) {
          found_channels = true;
        }
        else if (rl_single) {
          if (IMB_exr_set_channel(
                  exrhandle, nullptr, fullname, xstride, ystride, rpass->buffer.data + a)) {
            found_channels = true;
          }
          else {
            BKE_reportf(nullptr,
                        RPT_WARNING,
                        "reading render result: expected channel \"%s.%s\" or \"%s\" not found",
                        rl->name,
                        fullname,
                        fullname);
          }
        }
        else {
          BKE_reportf(nullptr,
                      RPT_WARNING,
                      "reading render result: expected channel \"%s.%s\" not found",
                      rl->name,
                      fullname);
        }
      }

      RE_render_result_full_channel_name(
          rpass->fullname, nullptr, rpass->name, rpass->view, rpass->chan_id, -1);
    }
  }

  if (found_channels) {
    IMB_exr_read_channels(exrhandle);
  }

  IMB_exr_close(exrhandle);

  return true;
}

#define FILE_CACHE_MAX (FILE_MAXFILE + FILE_MAXFILE + MAX_ID_NAME + 100)

static void render_result_exr_file_cache_path(Scene *sce,
                                              const char *root,
                                              char r_path[FILE_CACHE_MAX])
{
  char filename_full[FILE_MAX + MAX_ID_NAME + 100], filename[FILE_MAXFILE], dirname[FILE_MAXDIR];
  char path_digest[16] = {0};
  char path_hexdigest[33];

  /* If root is relative, use either current .blend file dir, or temp one if not saved. */
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  if (blendfile_path[0] != '\0') {
    BLI_path_split_dir_file(blendfile_path, dirname, sizeof(dirname), filename, sizeof(filename));
    BLI_path_extension_strip(filename); /* Strip `.blend`. */
    BLI_hash_md5_buffer(blendfile_path, strlen(blendfile_path), path_digest);
  }
  else {
    STRNCPY(dirname, BKE_tempdir_base());
    STRNCPY(filename, "UNSAVED");
  }
  BLI_hash_md5_to_hexdigest(path_digest, path_hexdigest);

  /* Default to *non-volatile* temp dir. */
  if (*root == '\0') {
    root = BKE_tempdir_base();
  }

  SNPRINTF(filename_full, "cached_RR_%s_%s_%s.exr", filename, sce->id.name + 2, path_hexdigest);

  BLI_path_join(r_path, FILE_CACHE_MAX, root, filename_full);
  if (BLI_path_is_rel(r_path)) {
    BLI_path_abs(r_path, dirname);
  }
}

void render_result_exr_file_cache_write(Render *re)
{
  RenderResult *rr = re->result;
  char str[FILE_CACHE_MAX];
  char *root = U.render_cachedir;

  render_result_passes_allocated_ensure(rr);

  render_result_exr_file_cache_path(re->scene, root, str);
  printf("Caching exr file, %dx%d, %s\n", rr->rectx, rr->recty, str);

  BKE_image_render_write_exr(nullptr, rr, str, nullptr, true, nullptr, -1);
}

bool render_result_exr_file_cache_read(Render *re)
{
  /* File path to cache. */
  char filepath[FILE_CACHE_MAX] = "";
  char *root = U.render_cachedir;
  render_result_exr_file_cache_path(re->scene, root, filepath);

  printf("read exr cache file: %s\n", filepath);

  /* Try opening the file. */
  void *exrhandle = IMB_exr_get_handle();
  int rectx, recty;

  if (!IMB_exr_begin_read(exrhandle, filepath, &rectx, &recty, true)) {
    printf("cannot read: %s\n", filepath);
    IMB_exr_close(exrhandle);
    return false;
  }

  /* Read file contents into render result. */
  const char *colorspace = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_SCENE_LINEAR);
  RE_FreeRenderResult(re->result);

  IMB_exr_read_channels(exrhandle);
  re->result = render_result_new_from_exr(exrhandle, colorspace, false, rectx, recty);

  IMB_exr_close(exrhandle);

  return true;
}

/*************************** Combined Pixel Rect *****************************/

ImBuf *RE_render_result_rect_to_ibuf(RenderResult *rr,
                                     const ImageFormatData *imf,
                                     const float dither,
                                     const int view_id)
{
  ImBuf *ibuf = IMB_allocImBuf(rr->rectx, rr->recty, imf->planes, 0);
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  /* if not exists, BKE_imbuf_write makes one */
  IMB_assign_shared_byte_buffer(ibuf, rv->byte_buffer.data, rv->byte_buffer.sharing_info);
  IMB_assign_shared_float_buffer(ibuf, rv->combined_buffer.data, rv->combined_buffer.sharing_info);
  IMB_assign_shared_float_z_buffer(ibuf, rv->z_buffer.data, rv->z_buffer.sharing_info);

  /* float factor for random dither, imbuf takes care of it */
  ibuf->dither = dither;

  /* prepare to gamma correct to sRGB color space
   * note that sequence editor can generate 8bpc render buffers
   */
  if (ibuf->byte_buffer.data) {
    if (BKE_imtype_valid_depths(imf->imtype) &
        (R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_24 | R_IMF_CHAN_DEPTH_32))
    {
      if (imf->depth == R_IMF_CHAN_DEPTH_8) {
        /* Higher depth bits are supported but not needed for current file output. */
        IMB_assign_float_buffer(ibuf, nullptr, IB_DO_NOT_TAKE_OWNERSHIP);
      }
      else {
        IMB_float_from_rect(ibuf);
      }
    }
    else {
      /* ensure no float buffer remained from previous frame */
      IMB_assign_float_buffer(ibuf, nullptr, IB_DO_NOT_TAKE_OWNERSHIP);
    }
  }

  /* Color -> gray-scale. */
  /* editing directly would alter the render view */
  if (imf->planes == R_IMF_PLANES_BW && imf->imtype != R_IMF_IMTYPE_MULTILAYER) {
    ImBuf *ibuf_bw = IMB_dupImBuf(ibuf);
    IMB_color_to_bw(ibuf_bw);
    IMB_freeImBuf(ibuf);
    ibuf = ibuf_bw;
  }

  return ibuf;
}

void RE_render_result_rect_from_ibuf(RenderResult *rr, const ImBuf *ibuf, const int view_id)
{
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  if (ibuf->float_buffer.data) {
    rr->have_combined = true;

    if (!rv->combined_buffer.data) {
      float *data = MEM_cnew_array<float>(4 * rr->rectx * rr->recty, "render_seq rectf");
      RE_RenderBuffer_assign_data(&rv->combined_buffer, data);
    }

    memcpy(rv->combined_buffer.data,
           ibuf->float_buffer.data,
           sizeof(float[4]) * rr->rectx * rr->recty);

    /* TSK! Since sequence render doesn't free the *rr render result, the old rect32
     * can hang around when sequence render has rendered a 32 bits one before */
    RE_RenderByteBuffer_data_free(&rv->byte_buffer);
  }
  else if (ibuf->byte_buffer.data) {
    rr->have_combined = true;

    if (!rv->byte_buffer.data) {
      uint8_t *data = MEM_cnew_array<uint8_t>(4 * rr->rectx * rr->recty, "render_seq rect");
      RE_RenderByteBuffer_assign_data(&rv->byte_buffer, data);
    }

    memcpy(rv->byte_buffer.data, ibuf->byte_buffer.data, sizeof(int) * rr->rectx * rr->recty);

    /* Same things as above, old rectf can hang around from previous render. */
    RE_RenderBuffer_data_free(&rv->combined_buffer);
  }
}

void render_result_rect_fill_zero(RenderResult *rr, const int view_id)
{
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  if (rv->combined_buffer.data) {
    memset(rv->combined_buffer.data, 0, sizeof(float[4]) * rr->rectx * rr->recty);
  }
  else if (rv->byte_buffer.data) {
    memset(rv->byte_buffer.data, 0, 4 * rr->rectx * rr->recty);
  }
  else {
    uint8_t *data = MEM_cnew_array<uint8_t>(rr->rectx * rr->recty, "render_seq rect");
    RE_RenderByteBuffer_assign_data(&rv->byte_buffer, data);
  }
}

void render_result_rect_get_pixels(RenderResult *rr,
                                   uint *rect,
                                   int rectx,
                                   int recty,
                                   const ColorManagedViewSettings *view_settings,
                                   const ColorManagedDisplaySettings *display_settings,
                                   const int view_id)
{
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  if (rv && rv->byte_buffer.data) {
    memcpy(rect, rv->byte_buffer.data, sizeof(int) * rr->rectx * rr->recty);
  }
  else if (rv && rv->combined_buffer.data) {
    IMB_display_buffer_transform_apply((uchar *)rect,
                                       rv->combined_buffer.data,
                                       rr->rectx,
                                       rr->recty,
                                       4,
                                       view_settings,
                                       display_settings,
                                       true);
  }
  else {
    /* else fill with black */
    memset(rect, 0, sizeof(int) * rectx * recty);
  }
}

/*************************** multiview functions *****************************/

bool RE_HasCombinedLayer(const RenderResult *result)
{
  if (result == nullptr) {
    return false;
  }

  const RenderView *rv = static_cast<RenderView *>(result->views.first);
  if (rv == nullptr) {
    return false;
  }

  return (rv->byte_buffer.data || rv->combined_buffer.data);
}

bool RE_HasFloatPixels(const RenderResult *result)
{
  LISTBASE_FOREACH (const RenderView *, rview, &result->views) {
    if (rview->byte_buffer.data && !rview->combined_buffer.data) {
      return false;
    }
  }

  return true;
}

bool RE_RenderResult_is_stereo(const RenderResult *result)
{
  if (!BLI_findstring(&result->views, STEREO_LEFT_NAME, offsetof(RenderView, name))) {
    return false;
  }

  if (!BLI_findstring(&result->views, STEREO_RIGHT_NAME, offsetof(RenderView, name))) {
    return false;
  }

  return true;
}

RenderView *RE_RenderViewGetById(RenderResult *rr, const int view_id)
{
  RenderView *rv = static_cast<RenderView *>(BLI_findlink(&rr->views, view_id));
  BLI_assert(rr->views.first);
  return rv ? rv : static_cast<RenderView *>(rr->views.first);
}

RenderView *RE_RenderViewGetByName(RenderResult *rr, const char *viewname)
{
  RenderView *rv = static_cast<RenderView *>(
      BLI_findstring(&rr->views, viewname, offsetof(RenderView, name)));
  BLI_assert(rr->views.first);
  return rv ? rv : static_cast<RenderView *>(rr->views.first);
}

static RenderPass *duplicate_render_pass(RenderPass *rpass)
{
  RenderPass *new_rpass = MEM_cnew<RenderPass>("new render pass", *rpass);
  new_rpass->next = new_rpass->prev = nullptr;

  if (new_rpass->buffer.sharing_info != nullptr) {
    new_rpass->buffer.sharing_info->add_user();
  }

  return new_rpass;
}

static RenderLayer *duplicate_render_layer(RenderLayer *rl)
{
  RenderLayer *new_rl = MEM_cnew<RenderLayer>("new render layer", *rl);
  new_rl->next = new_rl->prev = nullptr;
  new_rl->passes.first = new_rl->passes.last = nullptr;
  new_rl->exrhandle = nullptr;
  LISTBASE_FOREACH (RenderPass *, rpass, &rl->passes) {
    RenderPass *new_rpass = duplicate_render_pass(rpass);
    BLI_addtail(&new_rl->passes, new_rpass);
  }
  return new_rl;
}

static RenderView *duplicate_render_view(RenderView *rview)
{
  RenderView *new_rview = MEM_cnew<RenderView>("new render view", *rview);

  /* Reset buffers, they are not supposed to be shallow-coped. */
  new_rview->combined_buffer = {};
  new_rview->z_buffer = {};
  new_rview->byte_buffer = {};

  if (rview->combined_buffer.data != nullptr) {
    RE_RenderBuffer_assign_data(&new_rview->combined_buffer,
                                static_cast<float *>(MEM_dupallocN(rview->combined_buffer.data)));
  }

  if (rview->z_buffer.data != nullptr) {
    RE_RenderBuffer_assign_data(&new_rview->z_buffer,
                                static_cast<float *>(MEM_dupallocN(rview->z_buffer.data)));
  }

  if (rview->byte_buffer.data != nullptr) {
    RE_RenderByteBuffer_assign_data(
        &new_rview->byte_buffer, static_cast<uint8_t *>(MEM_dupallocN(rview->byte_buffer.data)));
  }

  return new_rview;
}

RenderResult *RE_DuplicateRenderResult(RenderResult *rr)
{
  RenderResult *new_rr = MEM_cnew<RenderResult>("new duplicated render result", *rr);
  new_rr->next = new_rr->prev = nullptr;
  new_rr->layers.first = new_rr->layers.last = nullptr;
  new_rr->views.first = new_rr->views.last = nullptr;
  LISTBASE_FOREACH (RenderLayer *, rl, &rr->layers) {
    RenderLayer *new_rl = duplicate_render_layer(rl);
    BLI_addtail(&new_rr->layers, new_rl);
  }
  LISTBASE_FOREACH (RenderView *, rview, &rr->views) {
    RenderView *new_rview = duplicate_render_view(rview);
    BLI_addtail(&new_rr->views, new_rview);
  }

  /* Reset buffers, they are not supposed to be shallow-coped. */
  new_rr->combined_buffer = {};
  new_rr->z_buffer = {};
  new_rr->byte_buffer = {};

  if (rr->combined_buffer.data) {
    RE_RenderBuffer_assign_data(&new_rr->combined_buffer,
                                static_cast<float *>(MEM_dupallocN(rr->combined_buffer.data)));
  }
  if (rr->z_buffer.data) {
    RE_RenderBuffer_assign_data(&new_rr->z_buffer,
                                static_cast<float *>(MEM_dupallocN(rr->z_buffer.data)));
  }
  if (rr->byte_buffer.data) {
    RE_RenderByteBuffer_assign_data(&new_rr->byte_buffer,
                                    static_cast<uint8_t *>(MEM_dupallocN(rr->byte_buffer.data)));
  }

  new_rr->stamp_data = BKE_stamp_data_copy(new_rr->stamp_data);
  return new_rr;
}

/* --------------------------------------------------------------------
 * Render buffer.
 */

template<class BufferType> static BufferType render_buffer_new(decltype(BufferType::data) data)
{
  BufferType buffer;

  buffer.data = data;
  buffer.sharing_info = blender::implicit_sharing::info_for_mem_free(data);
  buffer.gpu_texture = nullptr;

  return buffer;
}

template<class BufferType> static void render_buffer_data_free(BufferType *render_buffer)
{
  if (!render_buffer->sharing_info) {
    MEM_SAFE_FREE(render_buffer->data);
    return;
  }

  blender::implicit_sharing::free_shared_data(&render_buffer->data, &render_buffer->sharing_info);

  if (render_buffer->gpu_texture) {
    GPU_texture_free(render_buffer->gpu_texture);
    render_buffer->gpu_texture = nullptr;
  }
}

template<class BufferType>
static void render_buffer_assign_data(BufferType *render_buffer, decltype(BufferType::data) data)
{
  render_buffer_data_free(render_buffer);

  if (!data) {
    render_buffer->data = nullptr;
    render_buffer->sharing_info = nullptr;
    return;
  }

  render_buffer->data = data;
  render_buffer->sharing_info = blender::implicit_sharing::info_for_mem_free(data);
}

template<class BufferType>
static void render_buffer_assign_shared(BufferType *lhs, const BufferType *rhs)
{
  render_buffer_data_free(lhs);

  if (rhs) {
    blender::implicit_sharing::copy_shared_pointer(
        rhs->data, rhs->sharing_info, &lhs->data, &lhs->sharing_info);
  }
}

RenderBuffer RE_RenderBuffer_new(float *data)
{
  return render_buffer_new<RenderBuffer>(data);
}

void RE_RenderBuffer_assign_data(RenderBuffer *render_buffer, float *data)
{
  return render_buffer_assign_data(render_buffer, data);
}

void RE_RenderBuffer_assign_shared(RenderBuffer *lhs, const RenderBuffer *rhs)
{
  render_buffer_assign_shared(lhs, rhs);
}

void RE_RenderBuffer_data_free(RenderBuffer *render_buffer)
{
  render_buffer_data_free(render_buffer);
}

RenderByteBuffer RE_RenderByteBuffer_new(uint8_t *data)
{
  return render_buffer_new<RenderByteBuffer>(data);
}

void RE_RenderByteBuffer_assign_data(RenderByteBuffer *render_buffer, uint8_t *data)
{
  return render_buffer_assign_data(render_buffer, data);
}

void RE_RenderByteBuffer_assign_shared(RenderByteBuffer *lhs, const RenderByteBuffer *rhs)
{
  render_buffer_assign_shared(lhs, rhs);
}

void RE_RenderByteBuffer_data_free(RenderByteBuffer *render_buffer)
{
  render_buffer_data_free(render_buffer);
}

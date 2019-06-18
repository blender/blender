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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup render
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_hash_md5.h"
#include "BLI_path_util.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_threads.h"

#include "BKE_appdir.h"
#include "BKE_camera.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_colormanagement.h"

#include "intern/openexr/openexr_multi.h"

#include "RE_engine.h"

#include "render_result.h"
#include "render_types.h"

/********************************** Free *************************************/

static void render_result_views_free(RenderResult *res)
{
  while (res->views.first) {
    RenderView *rv = res->views.first;
    BLI_remlink(&res->views, rv);

    if (rv->rect32) {
      MEM_freeN(rv->rect32);
    }

    if (rv->rectz) {
      MEM_freeN(rv->rectz);
    }

    if (rv->rectf) {
      MEM_freeN(rv->rectf);
    }

    MEM_freeN(rv);
  }

  res->have_combined = false;
}

void render_result_free(RenderResult *res)
{
  if (res == NULL) {
    return;
  }

  while (res->layers.first) {
    RenderLayer *rl = res->layers.first;

    while (rl->passes.first) {
      RenderPass *rpass = rl->passes.first;
      if (rpass->rect) {
        MEM_freeN(rpass->rect);
      }
      BLI_remlink(&rl->passes, rpass);
      MEM_freeN(rpass);
    }
    BLI_remlink(&res->layers, rl);
    MEM_freeN(rl);
  }

  render_result_views_free(res);

  if (res->rect32) {
    MEM_freeN(res->rect32);
  }
  if (res->rectz) {
    MEM_freeN(res->rectz);
  }
  if (res->rectf) {
    MEM_freeN(res->rectf);
  }
  if (res->text) {
    MEM_freeN(res->text);
  }
  if (res->error) {
    MEM_freeN(res->error);
  }

  BKE_stamp_data_free(res->stamp_data);

  MEM_freeN(res);
}

/* version that's compatible with fullsample buffers */
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

/********************************* multiview *************************************/

/* create a new views Listbase in rr without duplicating the memory pointers */
void render_result_views_shallowcopy(RenderResult *dst, RenderResult *src)
{
  RenderView *rview;

  if (dst == NULL || src == NULL) {
    return;
  }

  for (rview = src->views.first; rview; rview = rview->next) {
    RenderView *rv;

    rv = MEM_mallocN(sizeof(RenderView), "new render view");
    BLI_addtail(&dst->views, rv);

    BLI_strncpy(rv->name, rview->name, sizeof(rv->name));
    rv->rectf = rview->rectf;
    rv->rectz = rview->rectz;
    rv->rect32 = rview->rect32;
  }
}

/* free the views created temporarily */
void render_result_views_shallowdelete(RenderResult *rr)
{
  if (rr == NULL) {
    return;
  }

  while (rr->views.first) {
    RenderView *rv = rr->views.first;
    BLI_remlink(&rr->views, rv);
    MEM_freeN(rv);
  }
}

static char *set_pass_name(char *outname, const char *name, int channel, const char *chan_id)
{
  BLI_strncpy(outname, name, EXR_PASS_MAXNAME);
  if (channel >= 0) {
    char token[3] = {'.', chan_id[channel], '\0'};
    strncat(outname, token, EXR_PASS_MAXNAME);
  }
  return outname;
}

static void set_pass_full_name(
    char *fullname, const char *name, int channel, const char *view, const char *chan_id)
{
  BLI_strncpy(fullname, name, EXR_PASS_MAXNAME);
  if (view && view[0]) {
    strncat(fullname, ".", EXR_PASS_MAXNAME);
    strncat(fullname, view, EXR_PASS_MAXNAME);
  }
  if (channel >= 0) {
    char token[3] = {'.', chan_id[channel], '\0'};
    strncat(fullname, token, EXR_PASS_MAXNAME);
  }
}

/********************************** New **************************************/

static RenderPass *render_layer_add_pass(RenderResult *rr,
                                         RenderLayer *rl,
                                         int channels,
                                         const char *name,
                                         const char *viewname,
                                         const char *chan_id)
{
  const int view_id = BLI_findstringindex(&rr->views, viewname, offsetof(RenderView, name));
  RenderPass *rpass = MEM_callocN(sizeof(RenderPass), name);
  size_t rectsize = ((size_t)rr->rectx) * rr->recty * channels;

  rpass->channels = channels;
  rpass->rectx = rl->rectx;
  rpass->recty = rl->recty;
  rpass->view_id = view_id;

  BLI_strncpy(rpass->name, name, sizeof(rpass->name));
  BLI_strncpy(rpass->chan_id, chan_id, sizeof(rpass->chan_id));
  BLI_strncpy(rpass->view, viewname, sizeof(rpass->view));
  set_pass_full_name(rpass->fullname, rpass->name, -1, rpass->view, rpass->chan_id);

  if (rl->exrhandle) {
    int a;
    for (a = 0; a < channels; a++) {
      char passname[EXR_PASS_MAXNAME];
      IMB_exr_add_channel(rl->exrhandle,
                          rl->name,
                          set_pass_name(passname, rpass->name, a, rpass->chan_id),
                          viewname,
                          0,
                          0,
                          NULL,
                          false);
    }
  }

  /* Always allocate combined for display, in case of save buffers
   * other passes are not allocated and only saved to the EXR file. */
  if (rl->exrhandle == NULL || STREQ(rpass->name, RE_PASSNAME_COMBINED)) {
    float *rect;
    int x;

    rpass->rect = MEM_mapallocN(sizeof(float) * rectsize, name);
    if (rpass->rect == NULL) {
      MEM_freeN(rpass);
      return NULL;
    }

    if (STREQ(rpass->name, RE_PASSNAME_VECTOR)) {
      /* initialize to max speed */
      rect = rpass->rect;
      for (x = rectsize - 1; x >= 0; x--) {
        rect[x] = PASS_VECTOR_MAX;
      }
    }
    else if (STREQ(rpass->name, RE_PASSNAME_Z)) {
      rect = rpass->rect;
      for (x = rectsize - 1; x >= 0; x--) {
        rect[x] = 10e10;
      }
    }
  }

  BLI_addtail(&rl->passes, rpass);

  return rpass;
}
/* wrapper called from render_opengl */
RenderPass *gp_add_pass(
    RenderResult *rr, RenderLayer *rl, int channels, const char *name, const char *viewname)
{
  return render_layer_add_pass(rr, rl, channels, name, viewname, "RGBA");
}

/* called by main render as well for parts */
/* will read info from Render *re to define layers */
/* called in threads */
/* re->winx,winy is coordinate space of entire image, partrct the part within */
RenderResult *render_result_new(Render *re,
                                rcti *partrct,
                                int crop,
                                int savebuffers,
                                const char *layername,
                                const char *viewname)
{
  RenderResult *rr;
  RenderLayer *rl;
  RenderView *rv;
  int rectx, recty;

  rectx = BLI_rcti_size_x(partrct);
  recty = BLI_rcti_size_y(partrct);

  if (rectx <= 0 || recty <= 0) {
    return NULL;
  }

  rr = MEM_callocN(sizeof(RenderResult), "new render result");
  rr->rectx = rectx;
  rr->recty = recty;
  rr->renrect.xmin = 0;
  rr->renrect.xmax = rectx - 2 * crop;
  /* crop is one or two extra pixels rendered for filtering, is used for merging and display too */
  rr->crop = crop;

  /* tilerect is relative coordinates within render disprect. do not subtract crop yet */
  rr->tilerect.xmin = partrct->xmin - re->disprect.xmin;
  rr->tilerect.xmax = partrct->xmax - re->disprect.xmin;
  rr->tilerect.ymin = partrct->ymin - re->disprect.ymin;
  rr->tilerect.ymax = partrct->ymax - re->disprect.ymin;

  if (savebuffers) {
    rr->do_exr_tile = true;
  }

  render_result_views_new(rr, &re->r);

  /* check renderdata for amount of layers */
  FOREACH_VIEW_LAYER_TO_RENDER_BEGIN (re, view_layer) {
    if (layername && layername[0]) {
      if (!STREQ(view_layer->name, layername)) {
        continue;
      }
    }

    rl = MEM_callocN(sizeof(RenderLayer), "new render layer");
    BLI_addtail(&rr->layers, rl);

    BLI_strncpy(rl->name, view_layer->name, sizeof(rl->name));
    rl->layflag = view_layer->layflag;
    rl->passflag =
        view_layer->passflag; /* for debugging: view_layer->passflag | SCE_PASS_RAYHITS; */
    rl->rectx = rectx;
    rl->recty = recty;

    if (rr->do_exr_tile) {
      rl->exrhandle = IMB_exr_get_handle();
    }

    for (rv = rr->views.first; rv; rv = rv->next) {
      const char *view = rv->name;

      if (viewname && viewname[0]) {
        if (!STREQ(view, viewname)) {
          continue;
        }
      }

      if (rr->do_exr_tile) {
        IMB_exr_add_view(rl->exrhandle, view);
      }

#define RENDER_LAYER_ADD_PASS_SAFE(rr, rl, channels, name, viewname, chan_id) \
  do { \
    if (render_layer_add_pass(rr, rl, channels, name, viewname, chan_id) == NULL) { \
      render_result_free(rr); \
      return NULL; \
    } \
  } while (false)

      /* a renderlayer should always have a Combined pass*/
      render_layer_add_pass(rr, rl, 4, "Combined", view, "RGBA");

      if (view_layer->passflag & SCE_PASS_Z) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 1, RE_PASSNAME_Z, view, "Z");
      }
      if (view_layer->passflag & SCE_PASS_VECTOR) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 4, RE_PASSNAME_VECTOR, view, "XYZW");
      }
      if (view_layer->passflag & SCE_PASS_NORMAL) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_NORMAL, view, "XYZ");
      }
      if (view_layer->passflag & SCE_PASS_UV) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_UV, view, "UVA");
      }
      if (view_layer->passflag & SCE_PASS_EMIT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_EMIT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_AO) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_AO, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_ENVIRONMENT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_ENVIRONMENT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_SHADOW) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_SHADOW, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_INDEXOB) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 1, RE_PASSNAME_INDEXOB, view, "X");
      }
      if (view_layer->passflag & SCE_PASS_INDEXMA) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 1, RE_PASSNAME_INDEXMA, view, "X");
      }
      if (view_layer->passflag & SCE_PASS_MIST) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 1, RE_PASSNAME_MIST, view, "Z");
      }
      if (rl->passflag & SCE_PASS_RAYHITS) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 4, RE_PASSNAME_RAYHITS, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_DIFFUSE_DIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_DIFFUSE_DIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_DIFFUSE_INDIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_DIFFUSE_INDIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_DIFFUSE_COLOR) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_DIFFUSE_COLOR, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_GLOSSY_DIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_GLOSSY_DIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_GLOSSY_INDIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_GLOSSY_INDIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_GLOSSY_COLOR) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_GLOSSY_COLOR, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_TRANSM_DIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_TRANSM_DIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_TRANSM_INDIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_TRANSM_INDIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_TRANSM_COLOR) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_TRANSM_COLOR, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_SUBSURFACE_DIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_SUBSURFACE_DIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_SUBSURFACE_INDIRECT) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_SUBSURFACE_INDIRECT, view, "RGB");
      }
      if (view_layer->passflag & SCE_PASS_SUBSURFACE_COLOR) {
        RENDER_LAYER_ADD_PASS_SAFE(rr, rl, 3, RE_PASSNAME_SUBSURFACE_COLOR, view, "RGB");
      }
#undef RENDER_LAYER_ADD_PASS_SAFE
    }
  }
  FOREACH_VIEW_LAYER_TO_RENDER_END;

  /* previewrender doesn't do layers, so we make a default one */
  if (BLI_listbase_is_empty(&rr->layers) && !(layername && layername[0])) {
    rl = MEM_callocN(sizeof(RenderLayer), "new render layer");
    BLI_addtail(&rr->layers, rl);

    rl->rectx = rectx;
    rl->recty = recty;

    /* duplicate code... */
    if (rr->do_exr_tile) {
      rl->exrhandle = IMB_exr_get_handle();
    }

    for (rv = rr->views.first; rv; rv = rv->next) {
      const char *view = rv->name;

      if (viewname && viewname[0]) {
        if (strcmp(view, viewname) != 0) {
          continue;
        }
      }

      if (rr->do_exr_tile) {
        IMB_exr_add_view(rl->exrhandle, view);
      }

      /* a renderlayer should always have a Combined pass */
      render_layer_add_pass(rr, rl, 4, RE_PASSNAME_COMBINED, view, "RGBA");
    }

    /* note, this has to be in sync with scene.c */
    rl->layflag = 0x7FFF; /* solid ztra halo strand */
    rl->passflag = SCE_PASS_COMBINED;

    re->active_view_layer = 0;
  }

  /* border render; calculate offset for use in compositor. compo is centralized coords */
  /* XXX obsolete? I now use it for drawing border render offset (ton) */
  rr->xof = re->disprect.xmin + BLI_rcti_cent_x(&re->disprect) - (re->winx / 2);
  rr->yof = re->disprect.ymin + BLI_rcti_cent_y(&re->disprect) - (re->winy / 2);

  return rr;
}

void render_result_clone_passes(Render *re, RenderResult *rr, const char *viewname)
{
  RenderLayer *rl;
  RenderPass *main_rp;

  for (rl = rr->layers.first; rl; rl = rl->next) {
    RenderLayer *main_rl = BLI_findstring(
        &re->result->layers, rl->name, offsetof(RenderLayer, name));
    if (!main_rl) {
      continue;
    }

    for (main_rp = main_rl->passes.first; main_rp; main_rp = main_rp->next) {
      if (viewname && viewname[0] && !STREQ(main_rp->view, viewname)) {
        continue;
      }

      /* Compare fullname to make sure that the view also is equal. */
      RenderPass *rp = BLI_findstring(
          &rl->passes, main_rp->fullname, offsetof(RenderPass, fullname));
      if (!rp) {
        render_layer_add_pass(
            rr, rl, main_rp->channels, main_rp->name, main_rp->view, main_rp->chan_id);
      }
    }
  }
}

void render_result_add_pass(RenderResult *rr,
                            const char *name,
                            int channels,
                            const char *chan_id,
                            const char *layername,
                            const char *viewname)
{
  RenderLayer *rl;
  RenderPass *rp;
  RenderView *rv;

  for (rl = rr->layers.first; rl; rl = rl->next) {
    if (layername && layername[0] && !STREQ(rl->name, layername)) {
      continue;
    }

    for (rv = rr->views.first; rv; rv = rv->next) {
      const char *view = rv->name;

      if (viewname && viewname[0] && !STREQ(view, viewname)) {
        continue;
      }

      /* Ensure that the pass doesn't exist yet. */
      for (rp = rl->passes.first; rp; rp = rp->next) {
        if (!STREQ(rp->name, name)) {
          continue;
        }
        if (!STREQ(rp->view, view)) {
          continue;
        }
        break;
      }

      if (!rp) {
        render_layer_add_pass(rr, rl, channels, name, view, chan_id);
      }
    }
  }
}

static int passtype_from_name(const char *name)
{
  const char delim[] = {'.', '\0'};
  const char *sep, *suf;
  int len = BLI_str_partition(name, delim, &sep, &suf);

#define CHECK_PASS(NAME) \
  if (STREQLEN(name, RE_PASSNAME_##NAME, len)) \
  return SCE_PASS_##NAME

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
  CHECK_PASS(RAYHITS);
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
  RenderResult *rr = base;
  RenderLayer *rl;

  rl = MEM_callocN(sizeof(RenderLayer), "new render layer");
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
  RenderResult *rr = base;
  RenderLayer *rl = lay;
  RenderPass *rpass = MEM_callocN(sizeof(RenderPass), "loaded pass");

  BLI_addtail(&rl->passes, rpass);
  rpass->channels = totchan;
  rl->passflag |= passtype_from_name(name);

  /* channel id chars */
  BLI_strncpy(rpass->chan_id, chan_id, sizeof(rpass->chan_id));

  rpass->rect = rect;
  BLI_strncpy(rpass->name, name, EXR_PASS_MAXNAME);
  BLI_strncpy(rpass->view, view, sizeof(rpass->view));
  set_pass_full_name(rpass->fullname, name, -1, view, rpass->chan_id);

  if (view[0] != '\0') {
    rpass->view_id = BLI_findstringindex(&rr->views, view, offsetof(RenderView, name));
  }
  else {
    rpass->view_id = 0;
  }
}

static void *ml_addview_cb(void *base, const char *str)
{
  RenderResult *rr = base;
  RenderView *rv;

  rv = MEM_callocN(sizeof(RenderView), "new render view");
  BLI_strncpy(rv->name, str, EXR_VIEW_MAXNAME);

  /* For stereo drawing we need to ensure:
   * STEREO_LEFT_NAME  == STEREO_LEFT_ID and
   * STEREO_RIGHT_NAME == STEREO_RIGHT_ID */

  if (STREQ(str, STEREO_LEFT_NAME)) {
    BLI_addhead(&rr->views, rv);
  }
  else if (STREQ(str, STEREO_RIGHT_NAME)) {
    RenderView *left_rv = BLI_findstring(&rr->views, STEREO_LEFT_NAME, offsetof(RenderView, name));

    if (left_rv == NULL) {
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
  // 1 if a is after b
  RenderPass *rpa = (RenderPass *)a;
  RenderPass *rpb = (RenderPass *)b;
  unsigned int passtype_a = passtype_from_name(rpa->name);
  unsigned int passtype_b = passtype_from_name(rpb->name);

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
    else if (passtype_a < passtype_b) {
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
  else if (STREQ(rpb->view, STEREO_LEFT_NAME)) {
    return 1;
  }

  /* right second */
  if (STREQ(rpa->view, STEREO_RIGHT_NAME)) {
    return 0;
  }
  else if (STREQ(rpb->view, STEREO_RIGHT_NAME)) {
    return 1;
  }

  /* remaining in ascending id order */
  return (rpa->view_id < rpb->view_id);
}

/* From imbuf, if a handle was returned and
 * it's not a singlelayer multiview we convert this to render result. */
RenderResult *render_result_new_from_exr(
    void *exrhandle, const char *colorspace, bool predivide, int rectx, int recty)
{
  RenderResult *rr = MEM_callocN(sizeof(RenderResult), __func__);
  RenderLayer *rl;
  RenderPass *rpass;
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  rr->rectx = rectx;
  rr->recty = recty;

  IMB_exr_multilayer_convert(exrhandle, rr, ml_addview_cb, ml_addlayer_cb, ml_addpass_cb);

  for (rl = rr->layers.first; rl; rl = rl->next) {
    rl->rectx = rectx;
    rl->recty = recty;

    BLI_listbase_sort(&rl->passes, order_render_passes);

    for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
      rpass->rectx = rectx;
      rpass->recty = recty;

      if (rpass->channels >= 3) {
        IMB_colormanagement_transform(rpass->rect,
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
  RenderView *rv = MEM_callocN(sizeof(RenderView), "new render view");
  BLI_addtail(&rr->views, rv);
  BLI_strncpy(rv->name, viewname, sizeof(rv->name));
}

void render_result_views_new(RenderResult *rr, RenderData *rd)
{
  SceneRenderView *srv;

  /* clear previously existing views - for sequencer */
  render_result_views_free(rr);

  /* check renderdata for amount of views */
  if ((rd->scemode & R_MULTIVIEW)) {
    for (srv = rd->views.first; srv; srv = srv->next) {
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

bool render_result_has_views(RenderResult *rr)
{
  RenderView *rv = rr->views.first;
  return (rv && (rv->next || rv->name[0]));
}

/*********************************** Merge ***********************************/

static void do_merge_tile(
    RenderResult *rr, RenderResult *rrpart, float *target, float *tile, int pixsize)
{
  int y, tilex, tiley;
  size_t ofs, copylen;

  copylen = tilex = rrpart->rectx;
  tiley = rrpart->recty;

  if (rrpart->crop) { /* filters add pixel extra */
    tile += pixsize * (rrpart->crop + ((size_t)rrpart->crop) * tilex);

    copylen = tilex - 2 * rrpart->crop;
    tiley -= 2 * rrpart->crop;

    ofs = (((size_t)rrpart->tilerect.ymin) + rrpart->crop) * rr->rectx +
          (rrpart->tilerect.xmin + rrpart->crop);
    target += pixsize * ofs;
  }
  else {
    ofs = (((size_t)rrpart->tilerect.ymin) * rr->rectx + rrpart->tilerect.xmin);
    target += pixsize * ofs;
  }

  copylen *= sizeof(float) * pixsize;
  tilex *= pixsize;
  ofs = pixsize * rr->rectx;

  for (y = 0; y < tiley; y++) {
    memcpy(target, tile, copylen);
    target += ofs;
    tile += tilex;
  }
}

/* used when rendering to a full buffer, or when reading the exr part-layer-pass file */
/* no test happens here if it fits... we also assume layers are in sync */
/* is used within threads */
void render_result_merge(RenderResult *rr, RenderResult *rrpart)
{
  RenderLayer *rl, *rlp;
  RenderPass *rpass, *rpassp;

  for (rl = rr->layers.first; rl; rl = rl->next) {
    rlp = RE_GetRenderLayer(rrpart, rl->name);
    if (rlp) {
      /* Passes are allocated in sync. */
      for (rpass = rl->passes.first, rpassp = rlp->passes.first; rpass && rpassp;
           rpass = rpass->next) {
        /* For save buffers, skip any passes that are only saved to disk. */
        if (rpass->rect == NULL || rpassp->rect == NULL) {
          continue;
        }
        /* Renderresult have all passes, renderpart only the active view's passes. */
        if (strcmp(rpassp->fullname, rpass->fullname) != 0) {
          continue;
        }

        do_merge_tile(rr, rrpart, rpass->rect, rpassp->rect, rpass->channels);

        /* manually get next render pass */
        rpassp = rpassp->next;
      }
    }
  }
}

/* Called from the UI and render pipeline, to save multilayer and multiview
 * images, optionally isolating a specific, view, layer or RGBA/Z pass. */
bool RE_WriteRenderResult(ReportList *reports,
                          RenderResult *rr,
                          const char *filename,
                          ImageFormatData *imf,
                          const char *view,
                          int layer)
{
  void *exrhandle = IMB_exr_get_handle();
  const bool half_float = (imf && imf->depth == R_IMF_CHAN_DEPTH_16);
  const bool multi_layer = !(imf && imf->imtype == R_IMF_IMTYPE_OPENEXR);
  const bool write_z = !multi_layer && (imf && (imf->flag & R_IMF_FLAG_ZBUF));

  /* Write first layer if not multilayer and no layer was specified. */
  if (!multi_layer && layer == -1) {
    layer = 0;
  }

  /* First add views since IMB_exr_add_channel checks number of views. */
  if (render_result_has_views(rr)) {
    for (RenderView *rview = rr->views.first; rview; rview = rview->next) {
      if (!view || STREQ(view, rview->name)) {
        IMB_exr_add_view(exrhandle, rview->name);
      }
    }
  }

  /* Compositing result. */
  if (rr->have_combined) {
    for (RenderView *rview = rr->views.first; rview; rview = rview->next) {
      if (!rview->rectf) {
        continue;
      }

      const char *viewname = rview->name;
      if (view) {
        if (!STREQ(view, viewname)) {
          continue;
        }
        else {
          viewname = "";
        }
      }

      /* Skip compositing if only a single other layer is requested. */
      if (!multi_layer && layer != 0) {
        continue;
      }

      for (int a = 0; a < 4; a++) {
        char passname[EXR_PASS_MAXNAME];
        char layname[EXR_PASS_MAXNAME];
        const char *chan_id = "RGBA";

        if (multi_layer) {
          set_pass_name(passname, "Combined", a, chan_id);
          BLI_strncpy(layname, "Composite", sizeof(layname));
        }
        else {
          passname[0] = chan_id[a];
          passname[1] = '\0';
          layname[0] = '\0';
        }

        IMB_exr_add_channel(exrhandle,
                            layname,
                            passname,
                            viewname,
                            4,
                            4 * rr->rectx,
                            rview->rectf + a,
                            half_float);
      }

      if (write_z && rview->rectz) {
        const char *layname = (multi_layer) ? "Composite" : "";
        IMB_exr_add_channel(exrhandle, layname, "Z", viewname, 1, rr->rectx, rview->rectz, false);
      }
    }
  }

  /* Other render layers. */
  int nr = (rr->have_combined) ? 1 : 0;
  for (RenderLayer *rl = rr->layers.first; rl; rl = rl->next, nr++) {
    /* Skip other render layers if requested. */
    if (!multi_layer && nr != layer) {
      continue;
    }

    for (RenderPass *rp = rl->passes.first; rp; rp = rp->next) {
      /* Skip non-RGBA and Z passes if not using multi layer. */
      if (!multi_layer && !(STREQ(rp->name, RE_PASSNAME_COMBINED) || STREQ(rp->name, "") ||
                            (STREQ(rp->name, RE_PASSNAME_Z) && write_z))) {
        continue;
      }

      /* Skip pass if it does not match the requested view(s). */
      const char *viewname = rp->view;
      if (view) {
        if (!STREQ(view, viewname)) {
          continue;
        }
        else {
          viewname = "";
        }
      }

      /* We only store RGBA passes as half float, for
       * others precision loss can be problematic. */
      bool pass_half_float = half_float &&
                             (STREQ(rp->chan_id, "RGB") || STREQ(rp->chan_id, "RGBA") ||
                              STREQ(rp->chan_id, "R") || STREQ(rp->chan_id, "G") ||
                              STREQ(rp->chan_id, "B") || STREQ(rp->chan_id, "A"));

      for (int a = 0; a < rp->channels; a++) {
        /* Save Combined as RGBA if single layer save. */
        char passname[EXR_PASS_MAXNAME];
        char layname[EXR_PASS_MAXNAME];

        if (multi_layer) {
          set_pass_name(passname, rp->name, a, rp->chan_id);
          BLI_strncpy(layname, rl->name, sizeof(layname));
        }
        else {
          passname[0] = rp->chan_id[a];
          passname[1] = '\0';
          layname[0] = '\0';
        }

        IMB_exr_add_channel(exrhandle,
                            layname,
                            passname,
                            viewname,
                            rp->channels,
                            rp->channels * rr->rectx,
                            rp->rect + a,
                            pass_half_float);
      }
    }
  }

  errno = 0;

  BLI_make_existing_file(filename);

  int compress = (imf ? imf->exr_codec : 0);
  bool success = IMB_exr_begin_write(
      exrhandle, filename, rr->rectx, rr->recty, compress, rr->stamp_data);
  if (success) {
    IMB_exr_write_channels(exrhandle);
  }
  else {
    /* TODO, get the error from openexr's exception */
    BKE_reportf(
        reports, RPT_ERROR, "Error writing render result, %s (see console)", strerror(errno));
  }

  IMB_exr_close(exrhandle);
  return success;
}

/**************************** Single Layer Rendering *************************/

void render_result_single_layer_begin(Render *re)
{
  /* all layers except the active one get temporally pushed away */

  /* officially pushed result should be NULL... error can happen with do_seq */
  RE_FreeRenderResult(re->pushedresult);

  re->pushedresult = re->result;
  re->result = NULL;
}

/* if scemode is R_SINGLE_LAYER, at end of rendering, merge the both render results */
void render_result_single_layer_end(Render *re)
{
  ViewLayer *view_layer;
  RenderLayer *rlpush;
  RenderLayer *rl;
  int nr;

  if (re->result == NULL) {
    printf("pop render result error; no current result!\n");
    return;
  }

  if (!re->pushedresult) {
    return;
  }

  if (re->pushedresult->rectx == re->result->rectx &&
      re->pushedresult->recty == re->result->recty) {
    /* find which layer in re->pushedresult should be replaced */
    rl = re->result->layers.first;

    /* render result should be empty after this */
    BLI_remlink(&re->result->layers, rl);

    /* reconstruct render result layers */
    for (nr = 0, view_layer = re->view_layers.first; view_layer;
         view_layer = view_layer->next, nr++) {
      if (nr == re->active_view_layer) {
        BLI_addtail(&re->result->layers, rl);
      }
      else {
        rlpush = RE_GetRenderLayer(re->pushedresult, view_layer->name);
        if (rlpush) {
          BLI_remlink(&re->pushedresult->layers, rlpush);
          BLI_addtail(&re->result->layers, rlpush);
        }
      }
    }
  }

  RE_FreeRenderResult(re->pushedresult);
  re->pushedresult = NULL;
}

/************************* EXR Tile File Rendering ***************************/

static void save_render_result_tile(RenderResult *rr, RenderResult *rrpart, const char *viewname)
{
  RenderLayer *rlp, *rl;
  RenderPass *rpassp;
  int offs, partx, party;

  BLI_thread_lock(LOCK_IMAGE);

  for (rlp = rrpart->layers.first; rlp; rlp = rlp->next) {
    rl = RE_GetRenderLayer(rr, rlp->name);

    /* should never happen but prevents crash if it does */
    BLI_assert(rl);
    if (UNLIKELY(rl == NULL)) {
      continue;
    }

    if (rrpart->crop) { /* filters add pixel extra */
      offs = (rrpart->crop + rrpart->crop * rrpart->rectx);
    }
    else {
      offs = 0;
    }

    /* passes are allocated in sync */
    for (rpassp = rlp->passes.first; rpassp; rpassp = rpassp->next) {
      const int xstride = rpassp->channels;
      int a;
      char fullname[EXR_PASS_MAXNAME];

      for (a = 0; a < xstride; a++) {
        set_pass_full_name(fullname, rpassp->name, a, viewname, rpassp->chan_id);

        IMB_exr_set_channel(rl->exrhandle,
                            rlp->name,
                            fullname,
                            xstride,
                            xstride * rrpart->rectx,
                            rpassp->rect + a + xstride * offs);
      }
    }
  }

  party = rrpart->tilerect.ymin + rrpart->crop;
  partx = rrpart->tilerect.xmin + rrpart->crop;

  for (rlp = rrpart->layers.first; rlp; rlp = rlp->next) {
    rl = RE_GetRenderLayer(rr, rlp->name);

    /* should never happen but prevents crash if it does */
    BLI_assert(rl);
    if (UNLIKELY(rl == NULL)) {
      continue;
    }

    IMB_exrtile_write_channels(rl->exrhandle, partx, party, 0, viewname, false);
  }

  BLI_thread_unlock(LOCK_IMAGE);
}

void render_result_save_empty_result_tiles(Render *re)
{
  RenderResult *rr;
  RenderLayer *rl;

  for (rr = re->result; rr; rr = rr->next) {
    for (rl = rr->layers.first; rl; rl = rl->next) {
      GHashIterator pa_iter;
      GHASH_ITER (pa_iter, re->parts) {
        RenderPart *pa = BLI_ghashIterator_getValue(&pa_iter);
        if (pa->status != PART_STATUS_MERGED) {
          int party = pa->disprect.ymin - re->disprect.ymin;
          int partx = pa->disprect.xmin - re->disprect.xmin;
          IMB_exrtile_write_channels(rl->exrhandle, partx, party, 0, re->viewname, true);
        }
      }
    }
  }
}

/* Compute list of passes needed by render engine. */
static void templates_register_pass_cb(void *userdata,
                                       Scene *UNUSED(scene),
                                       ViewLayer *UNUSED(view_layer),
                                       const char *name,
                                       int channels,
                                       const char *chan_id,
                                       int UNUSED(type))
{
  ListBase *templates = userdata;
  RenderPass *pass = MEM_callocN(sizeof(RenderPass), "RenderPassTemplate");

  pass->channels = channels;
  BLI_strncpy(pass->name, name, sizeof(pass->name));
  BLI_strncpy(pass->chan_id, chan_id, sizeof(pass->chan_id));

  BLI_addtail(templates, pass);
}

static void render_result_get_pass_templates(RenderEngine *engine,
                                             Render *re,
                                             RenderLayer *rl,
                                             ListBase *templates)
{
  BLI_listbase_clear(templates);

  if (engine && engine->type->update_render_passes) {
    ViewLayer *view_layer = BLI_findstring(&re->view_layers, rl->name, offsetof(ViewLayer, name));
    if (view_layer) {
      RE_engine_update_render_passes(
          engine, re->scene, view_layer, templates_register_pass_cb, templates);
    }
  }
}

/* begin write of exr tile file */
void render_result_exr_file_begin(Render *re, RenderEngine *engine)
{
  char str[FILE_MAX];

  for (RenderResult *rr = re->result; rr; rr = rr->next) {
    for (RenderLayer *rl = rr->layers.first; rl; rl = rl->next) {
      /* Get passes needed by engine. Normally we would wait for the
       * engine to create them, but for EXR file we need to know in
       * advance. */
      ListBase templates;
      render_result_get_pass_templates(engine, re, rl, &templates);

      /* Create render passes requested by engine. Only this part is
       * mutex locked to avoid deadlock with Python GIL. */
      BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
      for (RenderPass *pass = templates.first; pass; pass = pass->next) {
        render_result_add_pass(
            re->result, pass->name, pass->channels, pass->chan_id, rl->name, NULL);
      }
      BLI_rw_mutex_unlock(&re->resultmutex);

      BLI_freelistN(&templates);

      /* Open EXR file for writing. */
      render_result_exr_file_path(re->scene, rl->name, rr->sample_nr, str);
      printf("write exr tmp file, %dx%d, %s\n", rr->rectx, rr->recty, str);
      IMB_exrtile_begin_write(rl->exrhandle, str, 0, rr->rectx, rr->recty, re->partx, re->party);
    }
  }
}

/* end write of exr tile file, read back first sample */
void render_result_exr_file_end(Render *re, RenderEngine *engine)
{
  /* Close EXR files. */
  for (RenderResult *rr = re->result; rr; rr = rr->next) {
    for (RenderLayer *rl = rr->layers.first; rl; rl = rl->next) {
      IMB_exr_close(rl->exrhandle);
      rl->exrhandle = NULL;
    }

    rr->do_exr_tile = false;
  }

  /* Create new render result in memory instead of on disk. */
  BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
  render_result_free_list(&re->fullresult, re->result);
  re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS, RR_ALL_VIEWS);
  BLI_rw_mutex_unlock(&re->resultmutex);

  for (RenderLayer *rl = re->result->layers.first; rl; rl = rl->next) {
    /* Get passes needed by engine. */
    ListBase templates;
    render_result_get_pass_templates(engine, re, rl, &templates);

    /* Create render passes requested by engine. Only this part is
     * mutex locked to avoid deadlock with Python GIL. */
    BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
    for (RenderPass *pass = templates.first; pass; pass = pass->next) {
      render_result_add_pass(
          re->result, pass->name, pass->channels, pass->chan_id, rl->name, NULL);
    }

    BLI_freelistN(&templates);

    /* Render passes contents from file. */
    char str[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100] = "";
    render_result_exr_file_path(re->scene, rl->name, 0, str);
    printf("read exr tmp file: %s\n", str);

    if (!render_result_exr_file_read_path(re->result, rl, str)) {
      printf("cannot read: %s\n", str);
    }
    BLI_rw_mutex_unlock(&re->resultmutex);
  }
}

/* save part into exr file */
void render_result_exr_file_merge(RenderResult *rr, RenderResult *rrpart, const char *viewname)
{
  for (; rr && rrpart; rr = rr->next, rrpart = rrpart->next) {
    save_render_result_tile(rr, rrpart, viewname);
  }
}

/* path to temporary exr file */
void render_result_exr_file_path(Scene *scene, const char *layname, int sample, char *filepath)
{
  char name[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100];
  const char *fi = BLI_path_basename(BKE_main_blendfile_path_from_global());

  if (sample == 0) {
    BLI_snprintf(name, sizeof(name), "%s_%s_%s.exr", fi, scene->id.name + 2, layname);
  }
  else {
    BLI_snprintf(name, sizeof(name), "%s_%s_%s%d.exr", fi, scene->id.name + 2, layname, sample);
  }

  /* Make name safe for paths, see T43275. */
  BLI_filename_make_safe(name);

  BLI_make_file_string("/", filepath, BKE_tempdir_session(), name);
}

/* called for reading temp files, and for external engines */
int render_result_exr_file_read_path(RenderResult *rr,
                                     RenderLayer *rl_single,
                                     const char *filepath)
{
  RenderLayer *rl;
  RenderPass *rpass;
  void *exrhandle = IMB_exr_get_handle();
  int rectx, recty;

  if (IMB_exr_begin_read(exrhandle, filepath, &rectx, &recty) == 0) {
    printf("failed being read %s\n", filepath);
    IMB_exr_close(exrhandle);
    return 0;
  }

  if (rr == NULL || rectx != rr->rectx || recty != rr->recty) {
    if (rr) {
      printf("error in reading render result: dimensions don't match\n");
    }
    else {
      printf("error in reading render result: NULL result pointer\n");
    }
    IMB_exr_close(exrhandle);
    return 0;
  }

  for (rl = rr->layers.first; rl; rl = rl->next) {
    if (rl_single && rl_single != rl) {
      continue;
    }

    /* passes are allocated in sync */
    for (rpass = rl->passes.first; rpass; rpass = rpass->next) {
      const int xstride = rpass->channels;
      int a;
      char fullname[EXR_PASS_MAXNAME];

      for (a = 0; a < xstride; a++) {
        set_pass_full_name(fullname, rpass->name, a, rpass->view, rpass->chan_id);
        IMB_exr_set_channel(
            exrhandle, rl->name, fullname, xstride, xstride * rectx, rpass->rect + a);
      }

      set_pass_full_name(rpass->fullname, rpass->name, -1, rpass->view, rpass->chan_id);
    }
  }

  IMB_exr_read_channels(exrhandle);
  IMB_exr_close(exrhandle);

  return 1;
}

static void render_result_exr_file_cache_path(Scene *sce, const char *root, char *r_path)
{
  char filename_full[FILE_MAX + MAX_ID_NAME + 100], filename[FILE_MAXFILE], dirname[FILE_MAXDIR];
  char path_digest[16] = {0};
  char path_hexdigest[33];

  /* If root is relative, use either current .blend file dir, or temp one if not saved. */
  const char *blendfile_path = BKE_main_blendfile_path_from_global();
  if (blendfile_path[0] != '\0') {
    BLI_split_dirfile(blendfile_path, dirname, filename, sizeof(dirname), sizeof(filename));
    BLI_path_extension_replace(filename, sizeof(filename), ""); /* strip '.blend' */
    BLI_hash_md5_buffer(blendfile_path, strlen(blendfile_path), path_digest);
  }
  else {
    BLI_strncpy(dirname, BKE_tempdir_base(), sizeof(dirname));
    BLI_strncpy(filename, "UNSAVED", sizeof(filename));
  }
  BLI_hash_md5_to_hexdigest(path_digest, path_hexdigest);

  /* Default to *non-volatile* tmp dir. */
  if (*root == '\0') {
    root = BKE_tempdir_base();
  }

  BLI_snprintf(filename_full,
               sizeof(filename_full),
               "cached_RR_%s_%s_%s.exr",
               filename,
               sce->id.name + 2,
               path_hexdigest);
  BLI_make_file_string(dirname, r_path, root, filename_full);
}

void render_result_exr_file_cache_write(Render *re)
{
  RenderResult *rr = re->result;
  char str[FILE_MAXFILE + FILE_MAXFILE + MAX_ID_NAME + 100];
  char *root = U.render_cachedir;

  render_result_exr_file_cache_path(re->scene, root, str);
  printf("Caching exr file, %dx%d, %s\n", rr->rectx, rr->recty, str);

  RE_WriteRenderResult(NULL, rr, str, NULL, NULL, -1);
}

/* For cache, makes exact copy of render result */
bool render_result_exr_file_cache_read(Render *re)
{
  char str[FILE_MAXFILE + MAX_ID_NAME + MAX_ID_NAME + 100] = "";
  char *root = U.render_cachedir;

  RE_FreeRenderResult(re->result);
  re->result = render_result_new(re, &re->disprect, 0, RR_USE_MEM, RR_ALL_LAYERS, RR_ALL_VIEWS);

  /* First try cache. */
  render_result_exr_file_cache_path(re->scene, root, str);

  printf("read exr cache file: %s\n", str);
  if (!render_result_exr_file_read_path(re->result, NULL, str)) {
    printf("cannot read: %s\n", str);
    return false;
  }
  return true;
}

/*************************** Combined Pixel Rect *****************************/

ImBuf *render_result_rect_to_ibuf(RenderResult *rr, RenderData *rd, const int view_id)
{
  ImBuf *ibuf = IMB_allocImBuf(rr->rectx, rr->recty, rd->im_format.planes, 0);
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  /* if not exists, BKE_imbuf_write makes one */
  ibuf->rect = (unsigned int *)rv->rect32;
  ibuf->rect_float = rv->rectf;
  ibuf->zbuf_float = rv->rectz;

  /* float factor for random dither, imbuf takes care of it */
  ibuf->dither = rd->dither_intensity;

  /* prepare to gamma correct to sRGB color space
   * note that sequence editor can generate 8bpc render buffers
   */
  if (ibuf->rect) {
    if (BKE_imtype_valid_depths(rd->im_format.imtype) &
        (R_IMF_CHAN_DEPTH_12 | R_IMF_CHAN_DEPTH_16 | R_IMF_CHAN_DEPTH_24 | R_IMF_CHAN_DEPTH_32)) {
      if (rd->im_format.depth == R_IMF_CHAN_DEPTH_8) {
        /* Higher depth bits are supported but not needed for current file output. */
        ibuf->rect_float = NULL;
      }
      else {
        IMB_float_from_rect(ibuf);
      }
    }
    else {
      /* ensure no float buffer remained from previous frame */
      ibuf->rect_float = NULL;
    }
  }

  /* color -> grayscale */
  /* editing directly would alter the render view */
  if (rd->im_format.planes == R_IMF_PLANES_BW) {
    ImBuf *ibuf_bw = IMB_dupImBuf(ibuf);
    IMB_color_to_bw(ibuf_bw);
    IMB_freeImBuf(ibuf);
    ibuf = ibuf_bw;
  }

  return ibuf;
}

void RE_render_result_rect_from_ibuf(RenderResult *rr,
                                     RenderData *UNUSED(rd),
                                     ImBuf *ibuf,
                                     const int view_id)
{
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  if (ibuf->rect_float) {
    rr->have_combined = true;

    if (!rv->rectf) {
      rv->rectf = MEM_mallocN(4 * sizeof(float) * rr->rectx * rr->recty, "render_seq rectf");
    }

    memcpy(rv->rectf, ibuf->rect_float, 4 * sizeof(float) * rr->rectx * rr->recty);

    /* TSK! Since sequence render doesn't free the *rr render result, the old rect32
     * can hang around when sequence render has rendered a 32 bits one before */
    MEM_SAFE_FREE(rv->rect32);
  }
  else if (ibuf->rect) {
    rr->have_combined = true;

    if (!rv->rect32) {
      rv->rect32 = MEM_mallocN(sizeof(int) * rr->rectx * rr->recty, "render_seq rect");
    }

    memcpy(rv->rect32, ibuf->rect, 4 * rr->rectx * rr->recty);

    /* Same things as above, old rectf can hang around from previous render. */
    MEM_SAFE_FREE(rv->rectf);
  }
}

void render_result_rect_fill_zero(RenderResult *rr, const int view_id)
{
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  if (rv->rectf) {
    memset(rv->rectf, 0, 4 * sizeof(float) * rr->rectx * rr->recty);
  }
  else if (rv->rect32) {
    memset(rv->rect32, 0, 4 * rr->rectx * rr->recty);
  }
  else {
    rv->rect32 = MEM_callocN(sizeof(int) * rr->rectx * rr->recty, "render_seq rect");
  }
}

void render_result_rect_get_pixels(RenderResult *rr,
                                   unsigned int *rect,
                                   int rectx,
                                   int recty,
                                   const ColorManagedViewSettings *view_settings,
                                   const ColorManagedDisplaySettings *display_settings,
                                   const int view_id)
{
  RenderView *rv = RE_RenderViewGetById(rr, view_id);

  if (rv->rect32) {
    memcpy(rect, rv->rect32, sizeof(int) * rr->rectx * rr->recty);
  }
  else if (rv->rectf) {
    IMB_display_buffer_transform_apply((unsigned char *)rect,
                                       rv->rectf,
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

bool RE_HasCombinedLayer(RenderResult *res)
{
  RenderView *rv;

  if (res == NULL) {
    return false;
  }

  rv = res->views.first;
  if (rv == NULL) {
    return false;
  }

  return (rv->rect32 || rv->rectf);
}

bool RE_HasFloatPixels(RenderResult *res)
{
  RenderView *rview;

  for (rview = res->views.first; rview; rview = rview->next) {
    if (rview->rect32 && !rview->rectf) {
      return false;
    }
  }

  return true;
}

bool RE_RenderResult_is_stereo(RenderResult *res)
{
  if (!BLI_findstring(&res->views, STEREO_LEFT_NAME, offsetof(RenderView, name))) {
    return false;
  }

  if (!BLI_findstring(&res->views, STEREO_RIGHT_NAME, offsetof(RenderView, name))) {
    return false;
  }

  return true;
}

RenderView *RE_RenderViewGetById(RenderResult *res, const int view_id)
{
  RenderView *rv = BLI_findlink(&res->views, view_id);
  BLI_assert(res->views.first);
  return rv ? rv : res->views.first;
}

RenderView *RE_RenderViewGetByName(RenderResult *res, const char *viewname)
{
  RenderView *rv = BLI_findstring(&res->views, viewname, offsetof(RenderView, name));
  BLI_assert(res->views.first);
  return rv ? rv : res->views.first;
}

static RenderPass *duplicate_render_pass(RenderPass *rpass)
{
  RenderPass *new_rpass = MEM_mallocN(sizeof(RenderPass), "new render pass");
  *new_rpass = *rpass;
  new_rpass->next = new_rpass->prev = NULL;
  if (new_rpass->rect != NULL) {
    new_rpass->rect = MEM_dupallocN(new_rpass->rect);
  }
  return new_rpass;
}

static RenderLayer *duplicate_render_layer(RenderLayer *rl)
{
  RenderLayer *new_rl = MEM_mallocN(sizeof(RenderLayer), "new render layer");
  *new_rl = *rl;
  new_rl->next = new_rl->prev = NULL;
  new_rl->passes.first = new_rl->passes.last = NULL;
  new_rl->exrhandle = NULL;
  for (RenderPass *rpass = rl->passes.first; rpass != NULL; rpass = rpass->next) {
    RenderPass *new_rpass = duplicate_render_pass(rpass);
    BLI_addtail(&new_rl->passes, new_rpass);
  }
  return new_rl;
}

static RenderView *duplicate_render_view(RenderView *rview)
{
  RenderView *new_rview = MEM_mallocN(sizeof(RenderView), "new render view");
  *new_rview = *rview;
  if (new_rview->rectf != NULL) {
    new_rview->rectf = MEM_dupallocN(new_rview->rectf);
  }
  if (new_rview->rectz != NULL) {
    new_rview->rectz = MEM_dupallocN(new_rview->rectz);
  }
  if (new_rview->rect32 != NULL) {
    new_rview->rect32 = MEM_dupallocN(new_rview->rect32);
  }
  return new_rview;
}

RenderResult *RE_DuplicateRenderResult(RenderResult *rr)
{
  RenderResult *new_rr = MEM_mallocN(sizeof(RenderResult), "new duplicated render result");
  *new_rr = *rr;
  new_rr->next = new_rr->prev = NULL;
  new_rr->layers.first = new_rr->layers.last = NULL;
  new_rr->views.first = new_rr->views.last = NULL;
  for (RenderLayer *rl = rr->layers.first; rl != NULL; rl = rl->next) {
    RenderLayer *new_rl = duplicate_render_layer(rl);
    BLI_addtail(&new_rr->layers, new_rl);
  }
  for (RenderView *rview = rr->views.first; rview != NULL; rview = rview->next) {
    RenderView *new_rview = duplicate_render_view(rview);
    BLI_addtail(&new_rr->views, new_rview);
  }
  if (new_rr->rect32 != NULL) {
    new_rr->rect32 = MEM_dupallocN(new_rr->rect32);
  }
  if (new_rr->rectf != NULL) {
    new_rr->rectf = MEM_dupallocN(new_rr->rectf);
  }
  if (new_rr->rectz != NULL) {
    new_rr->rectz = MEM_dupallocN(new_rr->rectz);
  }
  new_rr->stamp_data = BKE_stamp_data_copy(new_rr->stamp_data);
  return new_rr;
}

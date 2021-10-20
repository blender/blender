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
 * \ingroup edutil
 */

#include "MEM_guardedalloc.h"

#include "BLI_rect.h"

#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "ED_image.h"
#include "ED_screen.h"
#include "ED_space_api.h"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "SEQ_render.h"
#include "SEQ_sequencer.h"

#include "UI_view2d.h"

#include "WM_api.h"
#include "WM_types.h"

#include "sequencer_intern.h"

/* Own define. */
#include "ED_util_imbuf.h"

/* -------------------------------------------------------------------- */
/** \name Image Pixel Sample Struct (Operator Custom Data)
 * \{ */

typedef struct ImageSampleInfo {
  ARegionType *art;
  void *draw_handle;
  int x, y;
  int channels;

  int width, height;
  int sample_size;

  unsigned char col[4];
  float colf[4];
  float linearcol[4];
  int z;
  float zf;

  unsigned char *colp;
  const float *colfp;
  int *zp;
  float *zfp;

  bool draw;
  bool color_manage;
  int use_default_view;
} ImageSampleInfo;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Pixel Sample
 * \{ */

static void image_sample_pixel_color_ubyte(const ImBuf *ibuf,
                                           const int coord[2],
                                           uchar r_col[4],
                                           float r_col_linear[4])
{
  const uchar *cp = (unsigned char *)(ibuf->rect + coord[1] * ibuf->x + coord[0]);
  copy_v4_v4_uchar(r_col, cp);
  rgba_uchar_to_float(r_col_linear, r_col);
  IMB_colormanagement_colorspace_to_scene_linear_v4(r_col_linear, false, ibuf->rect_colorspace);
}

static void image_sample_pixel_color_float(ImBuf *ibuf, const int coord[2], float r_col[4])
{
  const float *cp = ibuf->rect_float + (ibuf->channels) * (coord[1] * ibuf->x + coord[0]);
  copy_v4_v4(r_col, cp);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Pixel Region Sample
 * \{ */

static void image_sample_rect_color_ubyte(const ImBuf *ibuf,
                                          const rcti *rect,
                                          uchar r_col[4],
                                          float r_col_linear[4])
{
  uint col_accum_ub[4] = {0, 0, 0, 0};
  zero_v4(r_col_linear);
  int col_tot = 0;
  int coord[2];
  for (coord[0] = rect->xmin; coord[0] <= rect->xmax; coord[0]++) {
    for (coord[1] = rect->ymin; coord[1] <= rect->ymax; coord[1]++) {
      float col_temp_fl[4];
      uchar col_temp_ub[4];
      image_sample_pixel_color_ubyte(ibuf, coord, col_temp_ub, col_temp_fl);
      add_v4_v4(r_col_linear, col_temp_fl);
      col_accum_ub[0] += (uint)col_temp_ub[0];
      col_accum_ub[1] += (uint)col_temp_ub[1];
      col_accum_ub[2] += (uint)col_temp_ub[2];
      col_accum_ub[3] += (uint)col_temp_ub[3];
      col_tot += 1;
    }
  }
  mul_v4_fl(r_col_linear, 1.0 / (float)col_tot);

  r_col[0] = MIN2(col_accum_ub[0] / col_tot, 255);
  r_col[1] = MIN2(col_accum_ub[1] / col_tot, 255);
  r_col[2] = MIN2(col_accum_ub[2] / col_tot, 255);
  r_col[3] = MIN2(col_accum_ub[3] / col_tot, 255);
}

static void image_sample_rect_color_float(ImBuf *ibuf, const rcti *rect, float r_col[4])
{
  zero_v4(r_col);
  int col_tot = 0;
  int coord[2];
  for (coord[0] = rect->xmin; coord[0] <= rect->xmax; coord[0]++) {
    for (coord[1] = rect->ymin; coord[1] <= rect->ymax; coord[1]++) {
      float col_temp_fl[4];
      image_sample_pixel_color_float(ibuf, coord, col_temp_fl);
      add_v4_v4(r_col, col_temp_fl);
      col_tot += 1;
    }
  }
  mul_v4_fl(r_col, 1.0 / (float)col_tot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Pixel Sample (Internal Utilities)
 * \{ */

static void image_sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  ARegion *region = CTX_wm_region(C);
  Image *image = ED_space_image(sima);

  float uv[2];
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &uv[0], &uv[1]);
  int tile = BKE_image_get_tile_from_pos(sima->image, uv, uv, NULL);

  void *lock;
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, tile);
  ImageSampleInfo *info = op->customdata;
  Scene *scene = CTX_data_scene(C);
  CurveMapping *curve_mapping = scene->view_settings.curve_mapping;

  if (ibuf == NULL) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    info->draw = false;
    return;
  }

  if (uv[0] >= 0.0f && uv[1] >= 0.0f && uv[0] < 1.0f && uv[1] < 1.0f) {
    int x = (int)(uv[0] * ibuf->x), y = (int)(uv[1] * ibuf->y);

    CLAMP(x, 0, ibuf->x - 1);
    CLAMP(y, 0, ibuf->y - 1);

    info->width = ibuf->x;
    info->height = ibuf->y;
    info->x = x;
    info->y = y;

    info->draw = true;
    info->channels = ibuf->channels;

    info->colp = NULL;
    info->colfp = NULL;
    info->zp = NULL;
    info->zfp = NULL;

    info->use_default_view = (image->flag & IMA_VIEW_AS_RENDER) ? false : true;

    rcti sample_rect;
    sample_rect.xmin = max_ii(0, x - info->sample_size / 2);
    sample_rect.ymin = max_ii(0, y - info->sample_size / 2);
    sample_rect.xmax = min_ii(ibuf->x, sample_rect.xmin + info->sample_size) - 1;
    sample_rect.ymax = min_ii(ibuf->y, sample_rect.ymin + info->sample_size) - 1;

    if (ibuf->rect) {
      image_sample_rect_color_ubyte(ibuf, &sample_rect, info->col, info->linearcol);
      rgba_uchar_to_float(info->colf, info->col);

      info->colp = info->col;
      info->colfp = info->colf;
      info->color_manage = true;
    }
    if (ibuf->rect_float) {
      image_sample_rect_color_float(ibuf, &sample_rect, info->colf);

      if (ibuf->channels == 4) {
        /* pass */
      }
      else if (ibuf->channels == 3) {
        info->colf[3] = 1.0f;
      }
      else {
        info->colf[1] = info->colf[0];
        info->colf[2] = info->colf[0];
        info->colf[3] = 1.0f;
      }
      info->colfp = info->colf;

      copy_v4_v4(info->linearcol, info->colf);

      info->color_manage = true;
    }

    if (ibuf->zbuf) {
      /* TODO: blend depth (not urgent). */
      info->z = ibuf->zbuf[y * ibuf->x + x];
      info->zp = &info->z;
      if (ibuf->zbuf == (int *)ibuf->rect) {
        info->colp = NULL;
      }
    }
    if (ibuf->zbuf_float) {
      /* TODO: blend depth (not urgent). */
      info->zf = ibuf->zbuf_float[y * ibuf->x + x];
      info->zfp = &info->zf;
      if (ibuf->zbuf_float == ibuf->rect_float) {
        info->colfp = NULL;
      }
    }

    if (curve_mapping && ibuf->channels == 4) {
      /* we reuse this callback for set curves point operators */
      if (RNA_struct_find_property(op->ptr, "point")) {
        int point = RNA_enum_get(op->ptr, "point");

        if (point == 1) {
          BKE_curvemapping_set_black_white(curve_mapping, NULL, info->linearcol);
        }
        else if (point == 0) {
          BKE_curvemapping_set_black_white(curve_mapping, info->linearcol, NULL);
        }
        WM_event_add_notifier(C, NC_WINDOW, NULL);
      }
    }

    /* XXX node curve integration. */
#if 0
    {
      ScrArea *sa, *cur = curarea;

      node_curvemap_sample(fp); /* sends global to node editor */
      for (sa = G.curscreen->areabase.first; sa; sa = sa->next) {
        if (sa->spacetype == SPACE_NODE) {
          areawinset(sa->win);
          scrarea_do_windraw(sa);
        }
      }
      node_curvemap_sample(NULL); /* clears global in node editor */
      curarea = cur;
    }
#endif
  }
  else {
    info->draw = 0;
  }

  ED_space_image_release_buffer(sima, ibuf, lock);
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void sequencer_sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  struct Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  SpaceSeq *sseq = (SpaceSeq *)CTX_wm_space_data(C);
  ARegion *region = CTX_wm_region(C);
  ImBuf *ibuf = sequencer_ibuf_get(bmain, region, depsgraph, scene, sseq, CFRA, 0, NULL);
  ImageSampleInfo *info = op->customdata;
  float fx, fy;

  if (ibuf == NULL) {
    info->draw = 0;
    return;
  }

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fx, &fy);

  fx /= scene->r.xasp / scene->r.yasp;

  fx += (float)scene->r.xsch / 2.0f;
  fy += (float)scene->r.ysch / 2.0f;
  fx *= (float)ibuf->x / (float)scene->r.xsch;
  fy *= (float)ibuf->y / (float)scene->r.ysch;

  if (fx >= 0.0f && fy >= 0.0f && fx < ibuf->x && fy < ibuf->y) {
    const float *fp;
    unsigned char *cp;
    int x = (int)fx, y = (int)fy;

    info->x = x;
    info->y = y;
    info->draw = 1;
    info->channels = ibuf->channels;

    info->colp = NULL;
    info->colfp = NULL;

    if (ibuf->rect) {
      cp = (unsigned char *)(ibuf->rect + y * ibuf->x + x);

      info->col[0] = cp[0];
      info->col[1] = cp[1];
      info->col[2] = cp[2];
      info->col[3] = cp[3];
      info->colp = info->col;

      info->colf[0] = (float)cp[0] / 255.0f;
      info->colf[1] = (float)cp[1] / 255.0f;
      info->colf[2] = (float)cp[2] / 255.0f;
      info->colf[3] = (float)cp[3] / 255.0f;
      info->colfp = info->colf;

      copy_v4_v4(info->linearcol, info->colf);
      IMB_colormanagement_colorspace_to_scene_linear_v4(
          info->linearcol, false, ibuf->rect_colorspace);

      info->color_manage = true;
    }
    if (ibuf->rect_float) {
      fp = (ibuf->rect_float + (ibuf->channels) * (y * ibuf->x + x));

      info->colf[0] = fp[0];
      info->colf[1] = fp[1];
      info->colf[2] = fp[2];
      info->colf[3] = fp[3];
      info->colfp = info->colf;

      /* sequencer's image buffers are in non-linear space, need to make them linear */
      copy_v4_v4(info->linearcol, info->colf);
      SEQ_render_pixel_from_sequencer_space_v4(scene, info->linearcol);

      info->color_manage = true;
    }
  }
  else {
    info->draw = 0;
  }

  IMB_freeImBuf(ibuf);
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void ed_imbuf_sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *sa = CTX_wm_area(C);
  if (sa == NULL) {
    return;
  }

  switch (sa->spacetype) {
    case SPACE_IMAGE: {
      image_sample_apply(C, op, event);
      break;
    }
    case SPACE_SEQ: {
      sequencer_sample_apply(C, op, event);
      break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Pixel Sample (Public Operator Callback)
 *
 * Callbacks for the sample operator, used by sequencer and image spaces.
 * \{ */

void ED_imbuf_sample_draw(const bContext *C, ARegion *region, void *arg_info)
{
  ImageSampleInfo *info = arg_info;
  if (!info->draw) {
    return;
  }

  Scene *scene = CTX_data_scene(C);
  ED_image_draw_info(scene,
                     region,
                     info->color_manage,
                     info->use_default_view,
                     info->channels,
                     info->x,
                     info->y,
                     info->colp,
                     info->colfp,
                     info->linearcol,
                     info->zp,
                     info->zfp);

  if (info->sample_size > 1) {
    ScrArea *sa = CTX_wm_area(C);

    if (sa && sa->spacetype == SPACE_IMAGE) {

      const wmWindow *win = CTX_wm_window(C);
      const wmEvent *event = win->eventstate;

      SpaceImage *sima = CTX_wm_space_image(C);
      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      const float color[3] = {1, 1, 1};
      immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);
      immUniformColor3fv(color);

      /* TODO(campbell): lock to pixels. */
      rctf sample_rect_fl;
      BLI_rctf_init_pt_radius(
          &sample_rect_fl,
          (float[2]){event->xy[0] - region->winrct.xmin, event->xy[1] - region->winrct.ymin},
          (float)(info->sample_size / 2.0f) * sima->zoom);

      GPU_logic_op_xor_set(true);

      GPU_line_width(1.0f);
      imm_draw_box_wire_2d(pos,
                           (float)sample_rect_fl.xmin,
                           (float)sample_rect_fl.ymin,
                           (float)sample_rect_fl.xmax,
                           (float)sample_rect_fl.ymax);

      GPU_logic_op_xor_set(false);

      immUnbindProgram();
    }
  }
}

void ED_imbuf_sample_exit(bContext *C, wmOperator *op)
{
  ImageSampleInfo *info = op->customdata;

  ED_region_draw_cb_exit(info->art, info->draw_handle);
  ED_area_tag_redraw(CTX_wm_area(C));
  MEM_freeN(info);
}

int ED_imbuf_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  ScrArea *sa = CTX_wm_area(C);
  if (sa) {
    switch (sa->spacetype) {
      case SPACE_IMAGE: {
        SpaceImage *sima = sa->spacedata.first;
        if (region->regiontype == RGN_TYPE_WINDOW) {
          if (ED_space_image_show_cache_and_mval_over(sima, region, event->mval)) {
            return OPERATOR_PASS_THROUGH;
          }
        }
        if (!ED_space_image_has_buffer(sima)) {
          return OPERATOR_CANCELLED;
        }
        break;
      }
      case SPACE_SEQ: {
        /* Sequencer checks could be added. */
        break;
      }
    }
  }

  ImageSampleInfo *info = MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo");

  info->art = region->type;
  info->draw_handle = ED_region_draw_cb_activate(
      region->type, ED_imbuf_sample_draw, info, REGION_DRAW_POST_PIXEL);
  info->sample_size = RNA_int_get(op->ptr, "size");
  op->customdata = info;

  ed_imbuf_sample_apply(C, op, event);

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

int ED_imbuf_sample_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
    case RIGHTMOUSE: /* XXX hardcoded */
      if (event->val == KM_RELEASE) {
        ED_imbuf_sample_exit(C, op);
        return OPERATOR_CANCELLED;
      }
      break;
    case MOUSEMOVE:
      ed_imbuf_sample_apply(C, op, event);
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

void ED_imbuf_sample_cancel(bContext *C, wmOperator *op)
{
  ED_imbuf_sample_exit(C, op);
}

bool ED_imbuf_sample_poll(bContext *C)
{
  ScrArea *area = CTX_wm_area(C);
  if (area == NULL) {
    return false;
  }

  switch (area->spacetype) {
    case SPACE_IMAGE: {
      SpaceImage *sima = area->spacedata.first;
      Object *obedit = CTX_data_edit_object(C);
      if (obedit) {
        /* Disable when UV editing so it doesn't swallow all click events
         * (use for setting cursor). */
        if (ED_space_image_show_uvedit(sima, obedit)) {
          return false;
        }
      }
      else if (sima->mode != SI_MODE_VIEW) {
        return false;
      }
      return true;
    }
    case SPACE_SEQ: {
      SpaceSeq *sseq = area->spacedata.first;

      if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
        return false;
      }
      if (SEQ_editing_get(CTX_data_scene(C)) == NULL) {
        return false;
      }
      ARegion *region = CTX_wm_region(C);
      if (!(region && (region->regiontype == RGN_TYPE_PREVIEW))) {
        return false;
      }
      return true;
    }
  }

  return false;
}

/** \} */

/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_math_vector_types.hh"
#include "BLI_rect.h"

#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_image.h"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "ED_image.hh"
#include "ED_screen.hh"
#include "ED_space_api.hh"

#include "GPU_immediate.h"
#include "GPU_state.h"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "SEQ_render.hh"
#include "SEQ_sequencer.hh"

#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "sequencer_intern.hh"

/* Own define. */
#include "ED_util_imbuf.hh"

/* -------------------------------------------------------------------- */
/** \name Image Pixel Sample Struct (Operator Custom Data)
 * \{ */

struct ImageSampleInfo {
  ARegionType *art;
  void *draw_handle;
  int x, y;
  int channels;

  int width, height;
  int sample_size;

  uchar col[4];
  float colf[4];
  float linearcol[4];
  int z;
  float zf;

  uchar *colp;
  const float *colfp;
  int *zp;
  float *zfp;

  bool draw;
  bool color_manage;
  int use_default_view;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Pixel Sample
 * \{ */

static void image_sample_pixel_color_ubyte(const ImBuf *ibuf,
                                           const int coord[2],
                                           uchar r_col[4],
                                           float r_col_linear[4])
{
  const uchar *cp = ibuf->byte_buffer.data + 4 * (coord[1] * ibuf->x + coord[0]);
  copy_v4_v4_uchar(r_col, cp);
  rgba_uchar_to_float(r_col_linear, r_col);
  IMB_colormanagement_colorspace_to_scene_linear_v4(
      r_col_linear, false, ibuf->byte_buffer.colorspace);
}

static void image_sample_pixel_color_float(ImBuf *ibuf, const int coord[2], float r_col[4])
{
  const float *cp = ibuf->float_buffer.data + (ibuf->channels) * (coord[1] * ibuf->x + coord[0]);
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
      col_accum_ub[0] += uint(col_temp_ub[0]);
      col_accum_ub[1] += uint(col_temp_ub[1]);
      col_accum_ub[2] += uint(col_temp_ub[2]);
      col_accum_ub[3] += uint(col_temp_ub[3]);
      col_tot += 1;
    }
  }
  mul_v4_fl(r_col_linear, 1.0 / float(col_tot));

  r_col[0] = std::min<uchar>(col_accum_ub[0] / col_tot, 255);
  r_col[1] = std::min<uchar>(col_accum_ub[1] / col_tot, 255);
  r_col[2] = std::min<uchar>(col_accum_ub[2] / col_tot, 255);
  r_col[3] = std::min<uchar>(col_accum_ub[3] / col_tot, 255);
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
  mul_v4_fl(r_col, 1.0 / float(col_tot));
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
  int tile = BKE_image_get_tile_from_pos(sima->image, uv, uv, nullptr);

  void *lock;
  ImBuf *ibuf = ED_space_image_acquire_buffer(sima, &lock, tile);
  ImageSampleInfo *info = static_cast<ImageSampleInfo *>(op->customdata);
  Scene *scene = CTX_data_scene(C);
  CurveMapping *curve_mapping = scene->view_settings.curve_mapping;

  if (ibuf == nullptr) {
    ED_space_image_release_buffer(sima, ibuf, lock);
    info->draw = false;
    return;
  }

  if (uv[0] >= 0.0f && uv[1] >= 0.0f && uv[0] < 1.0f && uv[1] < 1.0f) {
    int x = int(uv[0] * ibuf->x), y = int(uv[1] * ibuf->y);

    CLAMP(x, 0, ibuf->x - 1);
    CLAMP(y, 0, ibuf->y - 1);

    info->width = ibuf->x;
    info->height = ibuf->y;
    info->x = x;
    info->y = y;

    info->draw = true;
    info->channels = ibuf->channels;

    info->colp = nullptr;
    info->colfp = nullptr;
    info->zp = nullptr;
    info->zfp = nullptr;

    info->use_default_view = (image->flag & IMA_VIEW_AS_RENDER) ? false : true;

    rcti sample_rect;
    sample_rect.xmin = max_ii(0, x - info->sample_size / 2);
    sample_rect.ymin = max_ii(0, y - info->sample_size / 2);
    sample_rect.xmax = min_ii(ibuf->x, sample_rect.xmin + info->sample_size) - 1;
    sample_rect.ymax = min_ii(ibuf->y, sample_rect.ymin + info->sample_size) - 1;

    if (ibuf->byte_buffer.data) {
      image_sample_rect_color_ubyte(ibuf, &sample_rect, info->col, info->linearcol);
      rgba_uchar_to_float(info->colf, info->col);

      info->colp = info->col;
      info->colfp = info->colf;
      info->color_manage = true;
    }
    if (ibuf->float_buffer.data) {
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

    if (curve_mapping && ibuf->channels == 4) {
      /* we reuse this callback for set curves point operators */
      if (RNA_struct_find_property(op->ptr, "point")) {
        int point = RNA_enum_get(op->ptr, "point");

        if (point == 1) {
          BKE_curvemapping_set_black_white(curve_mapping, nullptr, info->linearcol);
        }
        else if (point == 0) {
          BKE_curvemapping_set_black_white(curve_mapping, info->linearcol, nullptr);
        }
        WM_event_add_notifier(C, NC_WINDOW, nullptr);
      }
    }

/* XXX node curve integration. */
#if 0
    {
      ScrArea *area, *cur = curarea;

      node_curvemap_sample(fp); /* sends global to node editor */
      for (area = G.curscreen->areabase.first; area; area = area->next) {
        if (area->spacetype == SPACE_NODE) {
          areawinset(area->win);
          scrarea_do_windraw(area);
        }
      }
      node_curvemap_sample(nullptr); /* clears global in node editor */
      curarea = cur;
    }
#endif
  }
  else {
    info->draw = false;
  }

  ED_space_image_release_buffer(sima, ibuf, lock);
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void sequencer_sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  SpaceSeq *sseq = (SpaceSeq *)CTX_wm_space_data(C);
  ARegion *region = CTX_wm_region(C);
  ImBuf *ibuf = sequencer_ibuf_get(
      bmain, region, depsgraph, scene, sseq, scene->r.cfra, 0, nullptr);
  ImageSampleInfo *info = static_cast<ImageSampleInfo *>(op->customdata);
  float fx, fy;

  if (ibuf == nullptr) {
    info->draw = false;
    return;
  }

  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &fx, &fy);

  fx /= scene->r.xasp / scene->r.yasp;

  fx += float(scene->r.xsch) / 2.0f;
  fy += float(scene->r.ysch) / 2.0f;
  fx *= float(ibuf->x) / float(scene->r.xsch);
  fy *= float(ibuf->y) / float(scene->r.ysch);

  if (fx >= 0.0f && fy >= 0.0f && fx < ibuf->x && fy < ibuf->y) {
    const float *fp;
    uchar *cp;
    int x = int(fx), y = int(fy);

    info->x = x;
    info->y = y;
    info->draw = true;
    info->channels = ibuf->channels;

    info->colp = nullptr;
    info->colfp = nullptr;

    if (ibuf->byte_buffer.data) {
      cp = ibuf->byte_buffer.data + 4 * (y * ibuf->x + x);

      info->col[0] = cp[0];
      info->col[1] = cp[1];
      info->col[2] = cp[2];
      info->col[3] = cp[3];
      info->colp = info->col;

      info->colf[0] = float(cp[0]) / 255.0f;
      info->colf[1] = float(cp[1]) / 255.0f;
      info->colf[2] = float(cp[2]) / 255.0f;
      info->colf[3] = float(cp[3]) / 255.0f;
      info->colfp = info->colf;

      copy_v4_v4(info->linearcol, info->colf);
      IMB_colormanagement_colorspace_to_scene_linear_v4(
          info->linearcol, false, ibuf->byte_buffer.colorspace);

      info->color_manage = true;
    }
    if (ibuf->float_buffer.data) {
      fp = (ibuf->float_buffer.data + (ibuf->channels) * (y * ibuf->x + x));

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
    info->draw = false;
  }

  IMB_freeImBuf(ibuf);
  ED_area_tag_redraw(CTX_wm_area(C));
}

static void ed_imbuf_sample_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  ScrArea *area = CTX_wm_area(C);
  if (area == nullptr) {
    return;
  }

  switch (area->spacetype) {
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
  ImageSampleInfo *info = static_cast<ImageSampleInfo *>(arg_info);
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
                     info->linearcol);

  if (info->sample_size > 1) {
    ScrArea *area = CTX_wm_area(C);

    if (area && area->spacetype == SPACE_IMAGE) {

      const wmWindow *win = CTX_wm_window(C);
      const wmEvent *event = win->eventstate;

      SpaceImage *sima = CTX_wm_space_image(C);
      GPUVertFormat *format = immVertexFormat();
      uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      const float color[3] = {1, 1, 1};
      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformColor3fv(color);

      /* TODO(@ideasman42): lock to pixels. */
      rctf sample_rect_fl;
      BLI_rctf_init_pt_radius(&sample_rect_fl,
                              blender::float2{float(event->xy[0] - region->winrct.xmin),
                                              float(event->xy[1] - region->winrct.ymin)},
                              float(info->sample_size / 2.0f) * sima->zoom);

      GPU_logic_op_xor_set(true);

      GPU_line_width(1.0f);
      imm_draw_box_wire_2d(
          pos, sample_rect_fl.xmin, sample_rect_fl.ymin, sample_rect_fl.xmax, sample_rect_fl.ymax);

      GPU_logic_op_xor_set(false);

      immUnbindProgram();
    }
  }
}

void ED_imbuf_sample_exit(bContext *C, wmOperator *op)
{
  ImageSampleInfo *info = static_cast<ImageSampleInfo *>(op->customdata);

  ED_region_draw_cb_exit(info->art, info->draw_handle);
  ED_area_tag_redraw(CTX_wm_area(C));
  MEM_freeN(info);
}

int ED_imbuf_sample_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  ScrArea *area = CTX_wm_area(C);
  if (area) {
    switch (area->spacetype) {
      case SPACE_IMAGE: {
        SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
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

  ImageSampleInfo *info = static_cast<ImageSampleInfo *>(
      MEM_callocN(sizeof(ImageSampleInfo), "ImageSampleInfo"));

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
  if (area == nullptr) {
    return false;
  }

  switch (area->spacetype) {
    case SPACE_IMAGE: {
      SpaceImage *sima = static_cast<SpaceImage *>(area->spacedata.first);
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
      SpaceSeq *sseq = static_cast<SpaceSeq *>(area->spacedata.first);

      if (sseq->mainb != SEQ_DRAW_IMG_IMBUF) {
        return false;
      }
      if (SEQ_editing_get(CTX_data_scene(C)) == nullptr) {
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

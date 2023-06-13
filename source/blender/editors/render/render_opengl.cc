/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <cmath>
#include <cstddef>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "DNA_camera_types.h"

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim_data.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_image_format.h"
#include "BKE_image_save.h"
#include "BKE_lib_query.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_writeavi.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DRW_engine.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_gpencil_legacy.h"
#include "ED_screen.h"
#include "ED_view3d.h"
#include "ED_view3d_offscreen.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "SEQ_render.h"

#include "GPU_framebuffer.h"
#include "GPU_matrix.h"

#include "render_intern.hh"

/* Define this to get timing information. */
// #define DEBUG_TIME

#ifdef DEBUG_TIME
#  include "PIL_time.h"
#endif

/* TODO(sergey): Find better approximation of the scheduled frames.
 * For really high-resolution renders it might fail still. */
#define MAX_SCHEDULED_FRAMES 8

struct OGLRender {
  Main *bmain;
  Render *re;
  Scene *scene;
  WorkSpace *workspace;
  ViewLayer *view_layer;
  Depsgraph *depsgraph;

  View3D *v3d;
  RegionView3D *rv3d;
  ARegion *region;

  ScrArea *prevsa;
  ARegion *prevar;

  int views_len; /* multi-view views */

  bool is_sequencer;
  SpaceSeq *sseq;
  struct {
    ImBuf **ibufs_arr;
  } seq_data;

  Image *ima;
  ImageUser iuser;

  GPUOffScreen *ofs;
  int sizex, sizey;
  int write_still;

  ReportList *reports;
  bMovieHandle *mh;
  int cfrao, nfra;

  int totvideos;

  /* For only rendering frames that have a key in animation data. */
  BLI_bitmap *render_frames;

  /* quick lookup */
  int view_id;

  /* wm vars for timer and progress cursor */
  wmWindowManager *wm;
  wmWindow *win;

  /** Use to check if running modal or not (invoked or executed). */
  wmTimer *timer;

  void **movie_ctx_arr;

  TaskPool *task_pool;
  bool pool_ok;
  bool is_animation;

  eImageFormatDepth color_depth;
  SpinLock reports_lock;
  uint num_scheduled_frames;
  ThreadMutex task_mutex;
  ThreadCondition task_condition;

#ifdef DEBUG_TIME
  double time_start;
#endif
};

static bool screen_opengl_is_multiview(OGLRender *oglrender)
{
  View3D *v3d = oglrender->v3d;
  RegionView3D *rv3d = oglrender->rv3d;
  RenderData *rd = &oglrender->scene->r;

  if ((rd == nullptr) || ((v3d != nullptr) && (rv3d == nullptr))) {
    return false;
  }

  return (rd->scemode & R_MULTIVIEW) &&
         ((v3d == nullptr) || (rv3d->persp == RV3D_CAMOB && v3d->camera));
}

static void screen_opengl_views_setup(OGLRender *oglrender)
{
  RenderResult *rr;
  RenderView *rv;
  SceneRenderView *srv;
  bool is_multiview;
  Object *camera;
  View3D *v3d = oglrender->v3d;

  RenderData *rd = &oglrender->scene->r;

  rr = RE_AcquireResultWrite(oglrender->re);

  is_multiview = screen_opengl_is_multiview(oglrender);

  if (!is_multiview) {
    /* we only have one view when multiview is off */
    rv = static_cast<RenderView *>(rr->views.first);

    if (rv == nullptr) {
      rv = MEM_cnew<RenderView>("new opengl render view");
      BLI_addtail(&rr->views, rv);
    }

    while (rv->next) {
      RenderView *rv_del = rv->next;
      BLI_remlink(&rr->views, rv_del);

      RE_RenderBuffer_data_free(&rv_del->combined_buffer);
      RE_RenderBuffer_data_free(&rv_del->z_buffer);
      RE_RenderByteBuffer_data_free(&rv_del->byte_buffer);

      MEM_freeN(rv_del);
    }
  }
  else {
    if (v3d) {
      RE_SetOverrideCamera(oglrender->re, V3D_CAMERA_SCENE(oglrender->scene, v3d));
    }

    /* remove all the views that are not needed */
    rv = static_cast<RenderView *>(rr->views.last);
    while (rv) {
      srv = static_cast<SceneRenderView *>(
          BLI_findstring(&rd->views, rv->name, offsetof(SceneRenderView, name)));
      if (BKE_scene_multiview_is_render_view_active(rd, srv)) {
        rv = rv->prev;
      }
      else {
        RenderView *rv_del = rv;
        rv = rv_del->prev;

        BLI_remlink(&rr->views, rv_del);

        RE_RenderBuffer_data_free(&rv_del->combined_buffer);
        RE_RenderBuffer_data_free(&rv_del->z_buffer);
        RE_RenderByteBuffer_data_free(&rv_del->byte_buffer);

        MEM_freeN(rv_del);
      }
    }

    /* create all the views that are needed */
    for (srv = static_cast<SceneRenderView *>(rd->views.first); srv; srv = srv->next) {
      if (BKE_scene_multiview_is_render_view_active(rd, srv) == false) {
        continue;
      }

      rv = static_cast<RenderView *>(
          BLI_findstring(&rr->views, srv->name, offsetof(SceneRenderView, name)));

      if (rv == nullptr) {
        rv = MEM_cnew<RenderView>("new opengl render view");
        STRNCPY(rv->name, srv->name);
        BLI_addtail(&rr->views, rv);
      }
    }
  }

  if (!(is_multiview && BKE_scene_multiview_is_stereo3d(rd))) {
    oglrender->iuser.flag &= ~IMA_SHOW_STEREO;
  }

  /* will only work for non multiview correctly */
  if (v3d) {
    camera = BKE_camera_multiview_render(oglrender->scene, v3d->camera, "new opengl render view");
    BKE_render_result_stamp_info(oglrender->scene, camera, rr, false);
  }
  else {
    BKE_render_result_stamp_info(oglrender->scene, oglrender->scene->camera, rr, false);
  }

  RE_ReleaseResult(oglrender->re);
}

static void screen_opengl_render_doit(const bContext *C, OGLRender *oglrender, RenderResult *rr)
{
  Scene *scene = oglrender->scene;
  Object *camera = nullptr;
  int sizex = oglrender->sizex;
  int sizey = oglrender->sizey;
  ImBuf *ibuf_result = nullptr;

  if (oglrender->is_sequencer) {
    SpaceSeq *sseq = oglrender->sseq;
    bGPdata *gpd = (sseq && (sseq->flag & SEQ_PREVIEW_SHOW_GPENCIL)) ? sseq->gpd : nullptr;

    /* use pre-calculated ImBuf (avoids deadlock), see: */
    ImBuf *ibuf = oglrender->seq_data.ibufs_arr[oglrender->view_id];

    if (ibuf) {
      ibuf_result = IMB_dupImBuf(ibuf);
      IMB_freeImBuf(ibuf);
      /* OpenGL render is considered to be preview and should be
       * as fast as possible. So currently we're making sure sequencer
       * result is always byte to simplify color management pipeline.
       *
       * TODO(sergey): In the case of output to float container (EXR)
       * it actually makes sense to keep float buffer instead.
       */
      if (ibuf_result->float_buffer.data != nullptr) {
        IMB_rect_from_float(ibuf_result);
        imb_freerectfloatImBuf(ibuf_result);
      }
      BLI_assert((sizex == ibuf->x) && (sizey == ibuf->y));
    }
    else if (gpd) {
      /* If there are no strips, Grease Pencil still needs a buffer to draw on */
      ibuf_result = IMB_allocImBuf(sizex, sizey, 32, IB_rect);
    }

    if (gpd) {
      int i;
      uchar *gp_rect;
      uchar *render_rect = ibuf_result->byte_buffer.data;

      DRW_gpu_context_enable();
      GPU_offscreen_bind(oglrender->ofs, true);

      GPU_clear_color(0.0f, 0.0f, 0.0f, 0.0f);
      GPU_clear_depth(1.0f);

      GPU_matrix_reset();
      wmOrtho2(0, scene->r.xsch, 0, scene->r.ysch);
      GPU_matrix_translate_2f(scene->r.xsch / 2, scene->r.ysch / 2);

      G.f |= G_FLAG_RENDER_VIEWPORT;
      ED_annotation_draw_ex(scene, gpd, sizex, sizey, scene->r.cfra, SPACE_SEQ);
      G.f &= ~G_FLAG_RENDER_VIEWPORT;

      gp_rect = static_cast<uchar *>(
          MEM_mallocN(sizeof(uchar[4]) * sizex * sizey, "offscreen rect"));
      GPU_offscreen_read_color(oglrender->ofs, GPU_DATA_UBYTE, gp_rect);

      for (i = 0; i < sizex * sizey * 4; i += 4) {
        blend_color_mix_byte(&render_rect[i], &render_rect[i], &gp_rect[i]);
      }
      GPU_offscreen_unbind(oglrender->ofs, true);
      DRW_gpu_context_disable();

      MEM_freeN(gp_rect);
    }
  }
  else {
    /* shouldn't suddenly give errors mid-render but possible */
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    char err_out[256] = "unknown";
    ImBuf *ibuf_view;
    bool draw_sky = (scene->r.alphamode == R_ADDSKY);
    const int alpha_mode = (draw_sky) ? R_ADDSKY : R_ALPHAPREMUL;
    const char *viewname = RE_GetActiveRenderView(oglrender->re);
    View3D *v3d = oglrender->v3d;

    if (v3d != nullptr) {
      ARegion *region = oglrender->region;
      ibuf_view = ED_view3d_draw_offscreen_imbuf(depsgraph,
                                                 scene,
                                                 static_cast<eDrawType>(v3d->shading.type),
                                                 v3d,
                                                 region,
                                                 sizex,
                                                 sizey,
                                                 IB_rectfloat,
                                                 alpha_mode,
                                                 viewname,
                                                 true,
                                                 oglrender->ofs,
                                                 err_out);

      /* for stamp only */
      if (oglrender->rv3d->persp == RV3D_CAMOB && v3d->camera) {
        camera = BKE_camera_multiview_render(oglrender->scene, v3d->camera, viewname);
      }
    }
    else {
      ibuf_view = ED_view3d_draw_offscreen_imbuf_simple(depsgraph,
                                                        scene,
                                                        nullptr,
                                                        OB_SOLID,
                                                        scene->camera,
                                                        sizex,
                                                        sizey,
                                                        IB_rectfloat,
                                                        V3D_OFSDRAW_SHOW_ANNOTATION,
                                                        alpha_mode,
                                                        viewname,
                                                        oglrender->ofs,
                                                        err_out);
      camera = scene->camera;
    }

    if (ibuf_view) {
      ibuf_result = ibuf_view;
    }
    else {
      fprintf(stderr, "%s: failed to get buffer, %s\n", __func__, err_out);
    }
  }

  if (ibuf_result != nullptr) {
    if ((scene->r.stamp & R_STAMP_ALL) && (scene->r.stamp & R_STAMP_DRAW)) {
      float *rectf = nullptr;
      uchar *rect = nullptr;
      if (ibuf_result->float_buffer.data) {
        rectf = ibuf_result->float_buffer.data;
      }
      else {
        rect = ibuf_result->byte_buffer.data;
      }
      BKE_image_stamp_buf(scene, camera, nullptr, rect, rectf, rr->rectx, rr->recty, 4);
    }
    RE_render_result_rect_from_ibuf(rr, ibuf_result, oglrender->view_id);
    IMB_freeImBuf(ibuf_result);
  }
}

static void screen_opengl_render_write(OGLRender *oglrender)
{
  Scene *scene = oglrender->scene;
  RenderResult *rr;
  bool ok;
  char filepath[FILE_MAX];

  rr = RE_AcquireResultRead(oglrender->re);

  BKE_image_path_from_imformat(filepath,
                               scene->r.pic,
                               BKE_main_blendfile_path(oglrender->bmain),
                               scene->r.cfra,
                               &scene->r.im_format,
                               (scene->r.scemode & R_EXTENSION) != 0,
                               false,
                               nullptr);

  /* write images as individual images or stereo */
  BKE_render_result_stamp_info(scene, scene->camera, rr, false);
  ok = BKE_image_render_write(oglrender->reports, rr, scene, false, filepath);

  RE_ReleaseResultImage(oglrender->re);

  if (ok) {
    printf("OpenGL Render written to '%s'\n", filepath);
  }
  else {
    printf("OpenGL Render failed to write '%s'\n", filepath);
  }
}

static void UNUSED_FUNCTION(addAlphaOverFloat)(float dest[4], const float source[4])
{
  /* `d = s + (1-alpha_s)d` */
  float mul;

  mul = 1.0f - source[3];

  dest[0] = (mul * dest[0]) + source[0];
  dest[1] = (mul * dest[1]) + source[1];
  dest[2] = (mul * dest[2]) + source[2];
  dest[3] = (mul * dest[3]) + source[3];
}

static void screen_opengl_render_apply(const bContext *C, OGLRender *oglrender)
{
  RenderResult *rr;
  RenderView *rv;
  int view_id;
  ImBuf *ibuf;
  void *lock;

  if (oglrender->is_sequencer) {
    Scene *scene = oglrender->scene;

    SeqRenderData context;
    SpaceSeq *sseq = oglrender->sseq;
    int chanshown = sseq ? sseq->chanshown : 0;

    SEQ_render_new_render_data(oglrender->bmain,
                               oglrender->depsgraph,
                               scene,
                               oglrender->sizex,
                               oglrender->sizey,
                               SEQ_RENDER_SIZE_SCENE,
                               false,
                               &context);

    for (view_id = 0; view_id < oglrender->views_len; view_id++) {
      context.view_id = view_id;
      context.gpu_offscreen = oglrender->ofs;
      oglrender->seq_data.ibufs_arr[view_id] = SEQ_render_give_ibuf(
          &context, scene->r.cfra, chanshown);
    }
  }

  rr = RE_AcquireResultRead(oglrender->re);
  for (rv = static_cast<RenderView *>(rr->views.first), view_id = 0; rv; rv = rv->next, view_id++)
  {
    BLI_assert(view_id < oglrender->views_len);
    RE_SetActiveRenderView(oglrender->re, rv->name);
    oglrender->view_id = view_id;
    /* render composite */
    screen_opengl_render_doit(C, oglrender, rr);
  }

  RE_ReleaseResult(oglrender->re);

  ibuf = BKE_image_acquire_ibuf(oglrender->ima, &oglrender->iuser, &lock);
  if (ibuf) {
    ibuf->userflags |= IB_DISPLAY_BUFFER_INVALID;
  }
  BKE_image_release_ibuf(oglrender->ima, ibuf, lock);
  BKE_image_partial_update_mark_full_update(oglrender->ima);

  if (oglrender->write_still) {
    screen_opengl_render_write(oglrender);
  }
}

static void gather_frames_to_render_for_adt(const OGLRender *oglrender, const AnimData *adt)
{
  if (adt == nullptr || adt->action == nullptr) {
    return;
  }

  Scene *scene = oglrender->scene;
  int frame_start = PSFRA;
  int frame_end = PEFRA;

  LISTBASE_FOREACH (FCurve *, fcu, &adt->action->curves) {
    if (fcu->driver != nullptr || fcu->fpt != nullptr) {
      /* Drivers have values for any point in time, so to get "the keyed frames" they are
       * useless. Same for baked FCurves, they also have keys for every frame, which is not
       * useful for rendering the keyed subset of the frames. */
      continue;
    }

    bool found = false; /* Not interesting, we just want a starting point for the for-loop. */
    int key_index = BKE_fcurve_bezt_binarysearch_index(
        fcu->bezt, frame_start, fcu->totvert, &found);
    for (; key_index < fcu->totvert; key_index++) {
      BezTriple *bezt = &fcu->bezt[key_index];
      /* The frame range to render uses integer frame numbers, and the frame
       * step is also an integer, so we always render on the frame. */
      int frame_nr = round_fl_to_int(bezt->vec[1][0]);

      /* (frame_nr < frame_start) cannot happen because of the binary search above. */
      BLI_assert(frame_nr >= frame_start);
      if (frame_nr > frame_end) {
        break;
      }
      BLI_BITMAP_ENABLE(oglrender->render_frames, frame_nr - frame_start);
    }
  }
}

static void gather_frames_to_render_for_grease_pencil(const OGLRender *oglrender,
                                                      const bGPdata *gp)
{
  if (gp == nullptr) {
    return;
  }

  Scene *scene = oglrender->scene;
  int frame_start = PSFRA;
  int frame_end = PEFRA;

  LISTBASE_FOREACH (const bGPDlayer *, gp_layer, &gp->layers) {
    LISTBASE_FOREACH (const bGPDframe *, gp_frame, &gp_layer->frames) {
      if (gp_frame->framenum < frame_start || gp_frame->framenum > frame_end) {
        continue;
      }
      BLI_BITMAP_ENABLE(oglrender->render_frames, gp_frame->framenum - frame_start);
    }
  }
}

static int gather_frames_to_render_for_id(LibraryIDLinkCallbackData *cb_data)
{
  ID **id_p = cb_data->id_pointer;
  if (*id_p == nullptr) {
    return IDWALK_RET_NOP;
  }
  ID *id = *id_p;

  ID *self_id = cb_data->self_id;
  const int cb_flag = cb_data->cb_flag;
  if (cb_flag == IDWALK_CB_LOOPBACK || id == self_id) {
    /* IDs may end up referencing themselves one way or the other, and those
     * (the self_id ones) have always already been processed. */
    return IDWALK_RET_STOP_RECURSION;
  }

  OGLRender *oglrender = static_cast<OGLRender *>(cb_data->user_data);

  /* Whitelist of datablocks to follow pointers into. */
  const ID_Type id_type = GS(id->name);
  switch (id_type) {
    /* Whitelist: */
    case ID_ME:        /* Mesh */
    case ID_CU_LEGACY: /* Curve */
    case ID_MB:        /* MetaBall */
    case ID_MA:        /* Material */
    case ID_TE:        /* Tex (Texture) */
    case ID_IM:        /* Image */
    case ID_LT:        /* Lattice */
    case ID_LA:        /* Light */
    case ID_CA:        /* Camera */
    case ID_KE:        /* Key (shape key) */
    case ID_VF:        /* VFont (Vector Font) */
    case ID_TXT:       /* Text */
    case ID_SPK:       /* Speaker */
    case ID_SO:        /* Sound */
    case ID_AR:        /* bArmature */
    case ID_NT:        /* bNodeTree */
    case ID_PA:        /* ParticleSettings */
    case ID_MC:        /* MovieClip */
    case ID_MSK:       /* Mask */
    case ID_LP:        /* LightProbe */
    case ID_CV:        /* Curves */
    case ID_PT:        /* PointCloud */
    case ID_VO:        /* Volume */
    case ID_SIM:       /* Simulation */
      break;

      /* Blacklist: */
    case ID_SCE: /* Scene */
    case ID_LI:  /* Library */
    case ID_OB:  /* Object */
    case ID_IP:  /* Ipo (depreciated, replaced by FCurves) */
    case ID_WO:  /* World */
    case ID_SCR: /* Screen */
    case ID_GR:  /* Group */
    case ID_AC:  /* bAction */
    case ID_BR:  /* Brush */
    case ID_WM:  /* WindowManager */
    case ID_LS:  /* FreestyleLineStyle */
    case ID_PAL: /* Palette */
    case ID_PC:  /* PaintCurve */
    case ID_CF:  /* CacheFile */
    case ID_WS:  /* WorkSpace */
      /* Only follow pointers to specific datablocks, to avoid ending up in
       * unrelated datablocks and exploding the number of blocks we follow. If the
       * frames of the animation of certain objects should be taken into account,
       * they should have been selected by the user. */
      return IDWALK_RET_STOP_RECURSION;

    /* Special cases: */
    case ID_GD_LEGACY: /* bGPdata, (Grease Pencil) */
      /* In addition to regular ID's animdata, GreasePencil uses a specific frame-based animation
       * system that requires specific handling here. */
      gather_frames_to_render_for_grease_pencil(oglrender, (bGPdata *)id);
      break;
    case ID_GP:
      /* TODO: gather frames. */
      break;
  }

  AnimData *adt = BKE_animdata_from_id(id);
  gather_frames_to_render_for_adt(oglrender, adt);

  return IDWALK_RET_NOP;
}

/**
 * Collect the frame numbers for which selected objects have keys in the animation data.
 * The frames ares stored in #OGLRender.render_frames.
 *
 * Note that this follows all pointers to ID blocks, only filtering on ID type,
 * so it will pick up keys from pointers in custom properties as well.
 */
static void gather_frames_to_render(bContext *C, OGLRender *oglrender)
{
  Scene *scene = oglrender->scene;
  int frame_start = PSFRA;
  int frame_end = PEFRA;

  /* Will be freed in screen_opengl_render_end(). */
  oglrender->render_frames = BLI_BITMAP_NEW(frame_end - frame_start + 1,
                                            "OGLRender::render_frames");

  /* The first frame should always be rendered, otherwise there is nothing to write to file. */
  BLI_BITMAP_ENABLE(oglrender->render_frames, 0);

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    ID *id = &ob->id;

    /* Gather the frames from the object animation data. */
    AnimData *adt = BKE_animdata_from_id(id);
    gather_frames_to_render_for_adt(oglrender, adt);

    /* Gather the frames from linked data-blocks (materials, shape-keys, etc.). */
    BKE_library_foreach_ID_link(
        nullptr, id, gather_frames_to_render_for_id, oglrender, IDWALK_RECURSE);
  }
  CTX_DATA_END;
}

static bool screen_opengl_render_init(bContext *C, wmOperator *op)
{
  /* new render clears all callbacks */
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  WorkSpace *workspace = CTX_wm_workspace(C);

  Scene *scene = CTX_data_scene(C);
  ScrArea *prevsa = CTX_wm_area(C);
  ARegion *prevar = CTX_wm_region(C);
  GPUOffScreen *ofs;
  OGLRender *oglrender;
  int sizex, sizey;
  bool is_view_context = RNA_boolean_get(op->ptr, "view_context");
  const bool is_animation = RNA_boolean_get(op->ptr, "animation");
  const bool is_render_keyed_only = RNA_boolean_get(op->ptr, "render_keyed_only");
  const bool is_sequencer = RNA_boolean_get(op->ptr, "sequencer");
  const bool is_write_still = RNA_boolean_get(op->ptr, "write_still");
  const eImageFormatDepth color_depth = static_cast<eImageFormatDepth>(
      (is_animation) ? (eImageFormatDepth)scene->r.im_format.depth : R_IMF_CHAN_DEPTH_32);
  char err_out[256] = "unknown";

  if (G.background) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot use OpenGL render in background mode (no opengl context)");
    return false;
  }

  /* only one render job at a time */
  if (WM_jobs_test(wm, scene, WM_JOB_TYPE_RENDER)) {
    return false;
  }

  if (is_sequencer) {
    is_view_context = false;
  }
  else {
    /* ensure we have a 3d view */
    if (!ED_view3d_context_activate(C)) {
      RNA_boolean_set(op->ptr, "view_context", false);
      is_view_context = false;
    }

    if (!is_view_context && scene->camera == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "Scene has no camera");
      return false;
    }
  }

  if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
    return false;
  }

  /* stop all running jobs, except screen one. currently previews frustrate Render */
  WM_jobs_kill_all_except(wm, CTX_wm_screen(C));

  /* create offscreen buffer */
  BKE_render_resolution(&scene->r, false, &sizex, &sizey);

  /* corrects render size with actual size, not every card supports non-power-of-two dimensions */
  DRW_gpu_context_enable(); /* Off-screen creation needs to be done in DRW context. */
  ofs = GPU_offscreen_create(sizex,
                             sizey,
                             true,
                             GPU_RGBA16F,
                             GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_HOST_READ,
                             err_out);
  DRW_gpu_context_disable();

  if (!ofs) {
    BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL off-screen buffer, %s", err_out);
    return false;
  }

  /* allocate opengl render */
  oglrender = MEM_new<OGLRender>("OGLRender");
  op->customdata = oglrender;

  oglrender->ofs = ofs;
  oglrender->sizex = sizex;
  oglrender->sizey = sizey;
  oglrender->bmain = CTX_data_main(C);
  oglrender->scene = scene;
  oglrender->workspace = workspace;
  oglrender->view_layer = CTX_data_view_layer(C);
  /* NOTE: The depsgraph is not only used to update scene for a new frames, but also to initialize
   * output video handles, which does need evaluated scene. */
  oglrender->depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  oglrender->cfrao = scene->r.cfra;

  oglrender->write_still = is_write_still && !is_animation;
  oglrender->is_animation = is_animation;
  oglrender->color_depth = color_depth;

  oglrender->views_len = BKE_scene_multiview_num_views_get(&scene->r);

  oglrender->is_sequencer = is_sequencer;
  if (is_sequencer) {
    oglrender->sseq = CTX_wm_space_seq(C);
    ImBuf **ibufs_arr = static_cast<ImBuf **>(
        MEM_callocN(sizeof(*ibufs_arr) * oglrender->views_len, __func__));
    oglrender->seq_data.ibufs_arr = ibufs_arr;
  }

  oglrender->prevsa = prevsa;
  oglrender->prevar = prevar;

  if (is_view_context) {
    /* so quad view renders camera */
    ED_view3d_context_user_region(C, &oglrender->v3d, &oglrender->region);

    oglrender->rv3d = static_cast<RegionView3D *>(oglrender->region->regiondata);

    /* MUST be cleared on exit */
    memset(&oglrender->scene->customdata_mask_modal,
           0,
           sizeof(oglrender->scene->customdata_mask_modal));
    ED_view3d_datamask(oglrender->scene,
                       oglrender->view_layer,
                       oglrender->v3d,
                       &oglrender->scene->customdata_mask_modal);

    /* apply immediately in case we're rendering from a script,
     * running notifiers again will overwrite */
    CustomData_MeshMasks_update(&oglrender->scene->customdata_mask,
                                &oglrender->scene->customdata_mask_modal);
  }

  /* create render */
  oglrender->re = RE_NewSceneRender(scene);

  /* create image and image user */
  oglrender->ima = BKE_image_ensure_viewer(oglrender->bmain, IMA_TYPE_R_RESULT, "Render Result");
  BKE_image_signal(oglrender->bmain, oglrender->ima, nullptr, IMA_SIGNAL_FREE);
  BKE_image_backup_render(oglrender->scene, oglrender->ima, true);

  oglrender->iuser.scene = scene;

  /* create render result */
  RE_InitState(
      oglrender->re, nullptr, &scene->r, &scene->view_layers, nullptr, sizex, sizey, nullptr);

  /* create render views */
  screen_opengl_views_setup(oglrender);

  /* wm vars */
  oglrender->wm = wm;
  oglrender->win = win;

  oglrender->totvideos = 0;
  oglrender->mh = nullptr;
  oglrender->movie_ctx_arr = nullptr;

  if (is_animation) {
    if (is_render_keyed_only) {
      gather_frames_to_render(C, oglrender);
    }

    if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
      oglrender->task_pool = BLI_task_pool_create_background_serial(oglrender, TASK_PRIORITY_HIGH);
    }
    else {
      oglrender->task_pool = BLI_task_pool_create(oglrender, TASK_PRIORITY_HIGH);
    }
    oglrender->pool_ok = true;
    BLI_spin_init(&oglrender->reports_lock);
  }
  else {
    oglrender->task_pool = nullptr;
  }
  oglrender->num_scheduled_frames = 0;
  BLI_mutex_init(&oglrender->task_mutex);
  BLI_condition_init(&oglrender->task_condition);

#ifdef DEBUG_TIME
  oglrender->time_start = PIL_check_seconds_timer();
#endif

  return true;
}

static void screen_opengl_render_end(bContext *C, OGLRender *oglrender)
{
  Scene *scene = oglrender->scene;
  int i;

  if (oglrender->is_animation) {
    /* Trickery part for movie output:
     *
     * We MUST write frames in an exact order, so we only let background
     * thread to work on that, and main thread is simply waits for that
     * thread to do all the dirty work.
     *
     * After this loop is done work_and_wait() will have nothing to do,
     * so we don't run into wrong order of frames written to the stream.
     */
    if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
      BLI_mutex_lock(&oglrender->task_mutex);
      while (oglrender->num_scheduled_frames > 0) {
        BLI_condition_wait(&oglrender->task_condition, &oglrender->task_mutex);
      }
      BLI_mutex_unlock(&oglrender->task_mutex);
    }
    BLI_task_pool_work_and_wait(oglrender->task_pool);
    BLI_task_pool_free(oglrender->task_pool);
    BLI_spin_end(&oglrender->reports_lock);
  }
  BLI_mutex_end(&oglrender->task_mutex);
  BLI_condition_end(&oglrender->task_condition);

#ifdef DEBUG_TIME
  printf("Total render time: %f\n", PIL_check_seconds_timer() - oglrender->time_start);
#endif

  MEM_SAFE_FREE(oglrender->render_frames);

  if (oglrender->mh) {
    if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
      for (i = 0; i < oglrender->totvideos; i++) {
        oglrender->mh->end_movie(oglrender->movie_ctx_arr[i]);
        oglrender->mh->context_free(oglrender->movie_ctx_arr[i]);
      }
    }

    if (oglrender->movie_ctx_arr) {
      MEM_freeN(oglrender->movie_ctx_arr);
    }
  }

  if (oglrender->timer) { /* exec will not have a timer */
    Depsgraph *depsgraph = oglrender->depsgraph;
    scene->r.cfra = oglrender->cfrao;
    BKE_scene_graph_update_for_newframe(depsgraph);

    WM_event_remove_timer(oglrender->wm, oglrender->win, oglrender->timer);
  }

  WM_cursor_modal_restore(oglrender->win);

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);

  DRW_gpu_context_enable();
  GPU_offscreen_free(oglrender->ofs);
  DRW_gpu_context_disable();

  if (oglrender->is_sequencer) {
    MEM_freeN(oglrender->seq_data.ibufs_arr);
  }

  memset(&oglrender->scene->customdata_mask_modal,
         0,
         sizeof(oglrender->scene->customdata_mask_modal));

  CTX_wm_area_set(C, oglrender->prevsa);
  CTX_wm_region_set(C, oglrender->prevar);

  MEM_delete(oglrender);
}

static void screen_opengl_render_cancel(bContext *C, wmOperator *op)
{
  screen_opengl_render_end(C, static_cast<OGLRender *>(op->customdata));
}

/* share between invoke and exec */
static bool screen_opengl_render_anim_init(bContext *C, wmOperator *op)
{
  /* initialize animation */
  OGLRender *oglrender;
  Scene *scene;

  oglrender = static_cast<OGLRender *>(op->customdata);
  scene = oglrender->scene;
  oglrender->totvideos = BKE_scene_multiview_num_videos_get(&scene->r);

  oglrender->reports = op->reports;

  if (BKE_imtype_is_movie(scene->r.im_format.imtype)) {
    size_t width, height;
    int i;

    BKE_scene_multiview_videos_dimensions_get(
        &scene->r, oglrender->sizex, oglrender->sizey, &width, &height);
    oglrender->mh = BKE_movie_handle_get(scene->r.im_format.imtype);

    if (oglrender->mh == nullptr) {
      BKE_report(oglrender->reports, RPT_ERROR, "Movie format unsupported");
      screen_opengl_render_end(C, oglrender);
      return false;
    }

    oglrender->movie_ctx_arr = static_cast<void **>(
        MEM_mallocN(sizeof(void *) * oglrender->totvideos, "Movies"));

    for (i = 0; i < oglrender->totvideos; i++) {
      Scene *scene_eval = DEG_get_evaluated_scene(oglrender->depsgraph);
      const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, i);

      oglrender->movie_ctx_arr[i] = oglrender->mh->context_create();
      if (!oglrender->mh->start_movie(oglrender->movie_ctx_arr[i],
                                      scene_eval,
                                      &scene->r,
                                      oglrender->sizex,
                                      oglrender->sizey,
                                      oglrender->reports,
                                      PRVRANGEON != 0,
                                      suffix))
      {
        screen_opengl_render_end(C, oglrender);
        return false;
      }
    }
  }

  oglrender->cfrao = scene->r.cfra;
  oglrender->nfra = PSFRA;
  scene->r.cfra = PSFRA;

  return true;
}

struct WriteTaskData {
  RenderResult *rr;
  Scene tmp_scene;
};

static void write_result(TaskPool *__restrict pool, WriteTaskData *task_data)
{
  OGLRender *oglrender = (OGLRender *)BLI_task_pool_user_data(pool);
  Scene *scene = &task_data->tmp_scene;
  RenderResult *rr = task_data->rr;
  const bool is_movie = BKE_imtype_is_movie(scene->r.im_format.imtype);
  const int cfra = scene->r.cfra;
  bool ok;
  /* Don't attempt to write if we've got an error. */
  if (!oglrender->pool_ok) {
    RE_FreeRenderResult(rr);
    BLI_mutex_lock(&oglrender->task_mutex);
    oglrender->num_scheduled_frames--;
    BLI_condition_notify_all(&oglrender->task_condition);
    BLI_mutex_unlock(&oglrender->task_mutex);
    return;
  }
  /* Construct local thread0safe copy of reports structure which we can
   * safely pass to the underlying functions.
   */
  ReportList reports;
  BKE_reports_init(&reports, oglrender->reports->flag & ~RPT_PRINT);
  /* Do actual save logic here, depending on the file format.
   *
   * NOTE: We have to construct temporary scene with proper scene->r.cfra.
   * This is because underlying calls do not use r.cfra but use scene
   * for that.
   */
  if (is_movie) {
    ok = RE_WriteRenderViewsMovie(&reports,
                                  rr,
                                  scene,
                                  &scene->r,
                                  oglrender->mh,
                                  oglrender->movie_ctx_arr,
                                  oglrender->totvideos,
                                  PRVRANGEON != 0);
  }
  else {
    /* TODO(sergey): We can in theory save some CPU ticks here because we
     * calculate file name again here.
     */
    char filepath[FILE_MAX];
    BKE_image_path_from_imformat(filepath,
                                 scene->r.pic,
                                 BKE_main_blendfile_path(oglrender->bmain),
                                 cfra,
                                 &scene->r.im_format,
                                 (scene->r.scemode & R_EXTENSION) != 0,
                                 true,
                                 nullptr);

    BKE_render_result_stamp_info(scene, scene->camera, rr, false);
    ok = BKE_image_render_write(nullptr, rr, scene, true, filepath);
    if (!ok) {
      BKE_reportf(&reports, RPT_ERROR, "Write error: cannot save %s", filepath);
    }
  }
  if (reports.list.first != nullptr) {
    BLI_spin_lock(&oglrender->reports_lock);
    for (Report *report = static_cast<Report *>(reports.list.first); report != nullptr;
         report = report->next)
    {
      BKE_report(oglrender->reports, static_cast<eReportType>(report->type), report->message);
    }
    BLI_spin_unlock(&oglrender->reports_lock);
  }
  if (!ok) {
    oglrender->pool_ok = false;
  }
  RE_FreeRenderResult(rr);
  BLI_mutex_lock(&oglrender->task_mutex);
  oglrender->num_scheduled_frames--;
  BLI_condition_notify_all(&oglrender->task_condition);
  BLI_mutex_unlock(&oglrender->task_mutex);
}

static void write_result_func(TaskPool *__restrict pool, void *task_data_v)
{
  /* Isolate task so that multithreaded image operations don't cause this thread to start
   * writing another frame. If that happens we may reach the MAX_SCHEDULED_FRAMES limit,
   * and cause the render thread and writing threads to deadlock waiting for each other. */
  WriteTaskData *task_data = (WriteTaskData *)task_data_v;
  blender::threading::isolate_task([&] { write_result(pool, task_data); });
}

static bool schedule_write_result(OGLRender *oglrender, RenderResult *rr)
{
  if (!oglrender->pool_ok) {
    RE_FreeRenderResult(rr);
    return false;
  }
  Scene *scene = oglrender->scene;
  WriteTaskData *task_data = MEM_new<WriteTaskData>("write task data");
  task_data->rr = rr;
  memcpy(&task_data->tmp_scene, scene, sizeof(task_data->tmp_scene));
  BLI_mutex_lock(&oglrender->task_mutex);
  oglrender->num_scheduled_frames++;
  if (oglrender->num_scheduled_frames > MAX_SCHEDULED_FRAMES) {
    BLI_condition_wait(&oglrender->task_condition, &oglrender->task_mutex);
  }
  BLI_mutex_unlock(&oglrender->task_mutex);
  BLI_task_pool_push(oglrender->task_pool, write_result_func, task_data, true, nullptr);
  return true;
}

static bool screen_opengl_render_anim_step(bContext *C, wmOperator *op)
{
  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);
  Scene *scene = oglrender->scene;
  Depsgraph *depsgraph = oglrender->depsgraph;
  char filepath[FILE_MAX];
  bool ok = false;
  const bool view_context = (oglrender->v3d != nullptr);
  bool is_movie;
  RenderResult *rr;

  /* go to next frame */
  if (scene->r.cfra < oglrender->nfra) {
    scene->r.cfra++;
  }
  while (scene->r.cfra < oglrender->nfra) {
    BKE_scene_graph_update_for_newframe(depsgraph);
    scene->r.cfra++;
  }

  is_movie = BKE_imtype_is_movie(scene->r.im_format.imtype);

  if (!is_movie) {
    BKE_image_path_from_imformat(filepath,
                                 scene->r.pic,
                                 BKE_main_blendfile_path(oglrender->bmain),
                                 scene->r.cfra,
                                 &scene->r.im_format,
                                 (scene->r.scemode & R_EXTENSION) != 0,
                                 true,
                                 nullptr);

    if ((scene->r.mode & R_NO_OVERWRITE) && BLI_exists(filepath)) {
      BLI_spin_lock(&oglrender->reports_lock);
      BKE_reportf(op->reports, RPT_INFO, "Skipping existing frame \"%s\"", filepath);
      BLI_spin_unlock(&oglrender->reports_lock);
      ok = true;
      goto finally;
    }
  }

  WM_cursor_time(oglrender->win, scene->r.cfra);

  BKE_scene_graph_update_for_newframe(depsgraph);

  if (view_context) {
    if (oglrender->rv3d->persp == RV3D_CAMOB && oglrender->v3d->camera &&
        oglrender->v3d->scenelock) {
      /* since BKE_scene_graph_update_for_newframe() is used rather
       * then ED_update_for_newframe() the camera needs to be set */
      if (BKE_scene_camera_switch_update(scene)) {
        oglrender->v3d->camera = scene->camera;
      }
    }
  }
  else {
    BKE_scene_camera_switch_update(scene);
  }

  if (oglrender->render_frames == nullptr ||
      BLI_BITMAP_TEST_BOOL(oglrender->render_frames, scene->r.cfra - PSFRA))
  {
    /* render into offscreen buffer */
    screen_opengl_render_apply(C, oglrender);
  }

  /* save to disk */
  rr = RE_AcquireResultRead(oglrender->re);
  {
    RenderResult *new_rr = RE_DuplicateRenderResult(rr);
    RE_ReleaseResult(oglrender->re);

    ok = schedule_write_result(oglrender, new_rr);
  }

finally: /* Step the frame and bail early if needed */

  /* go to next frame */
  oglrender->nfra += scene->r.frame_step;

  /* stop at the end or on error */
  if (scene->r.cfra >= PEFRA || !ok) {
    screen_opengl_render_end(C, static_cast<OGLRender *>(op->customdata));
    return false;
  }

  return true;
}

static int screen_opengl_render_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);
  const bool anim = RNA_boolean_get(op->ptr, "animation");
  bool ret;

  switch (event->type) {
    case EVT_ESCKEY:
      /* cancel */
      oglrender->pool_ok = false; /* Flag pool for cancel. */
      screen_opengl_render_end(C, static_cast<OGLRender *>(op->customdata));
      return OPERATOR_FINISHED;
    case TIMER:
      /* render frame? */
      if (oglrender->timer == event->customdata) {
        break;
      }
      ATTR_FALLTHROUGH;
    default:
      /* nothing to do */
      return OPERATOR_RUNNING_MODAL;
  }

  /* run first because screen_opengl_render_anim_step can free oglrender */
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, oglrender->scene);

  if (anim == 0) {
    screen_opengl_render_apply(C, static_cast<OGLRender *>(op->customdata));
    screen_opengl_render_end(C, static_cast<OGLRender *>(op->customdata));
    return OPERATOR_FINISHED;
  }

  ret = screen_opengl_render_anim_step(C, op);

  /* stop at the end or on error */
  if (ret == false) {
    return OPERATOR_FINISHED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static int screen_opengl_render_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  OGLRender *oglrender;
  const bool anim = RNA_boolean_get(op->ptr, "animation");

  if (!screen_opengl_render_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (anim) {
    if (!screen_opengl_render_anim_init(C, op)) {
      return OPERATOR_CANCELLED;
    }
  }

  oglrender = static_cast<OGLRender *>(op->customdata);
  render_view_open(C, event->xy[0], event->xy[1], op->reports);

  /* View may be changed above #USER_RENDER_DISPLAY_WINDOW. */
  oglrender->win = CTX_wm_window(C);

  WM_event_add_modal_handler(C, op);
  oglrender->timer = WM_event_add_timer(oglrender->wm, oglrender->win, TIMER, 0.01f);

  return OPERATOR_RUNNING_MODAL;
}

/* executes blocking render */
static int screen_opengl_render_exec(bContext *C, wmOperator *op)
{
  const bool is_animation = RNA_boolean_get(op->ptr, "animation");

  if (!screen_opengl_render_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (!is_animation) { /* same as invoke */
    /* render image */
    screen_opengl_render_apply(C, static_cast<OGLRender *>(op->customdata));
    screen_opengl_render_end(C, static_cast<OGLRender *>(op->customdata));

    return OPERATOR_FINISHED;
  }

  bool ret = true;

  if (!screen_opengl_render_anim_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  while (ret) {
    ret = screen_opengl_render_anim_step(C, op);
  }

  /* no redraw needed, we leave state as we entered it */
  //  ED_update_for_newframe(C);
  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, CTX_data_scene(C));

  return OPERATOR_FINISHED;
}

static char *screen_opengl_render_description(bContext * /*C*/,
                                              wmOperatorType * /*ot*/,
                                              PointerRNA *ptr)
{
  if (!RNA_boolean_get(ptr, "animation")) {
    return nullptr;
  }

  if (RNA_boolean_get(ptr, "render_keyed_only")) {
    return BLI_strdup(TIP_(
        "Render the viewport for the animation range of this scene, but only render keyframes of "
        "selected objects"));
  }

  return BLI_strdup(TIP_("Render the viewport for the animation range of this scene"));
}

void RENDER_OT_opengl(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Viewport Render";
  ot->description = "Take a snapshot of the active viewport";
  ot->idname = "RENDER_OT_opengl";

  /* api callbacks */
  ot->get_description = screen_opengl_render_description;
  ot->invoke = screen_opengl_render_invoke;
  ot->exec = screen_opengl_render_exec; /* blocking */
  ot->modal = screen_opengl_render_modal;
  ot->cancel = screen_opengl_render_cancel;

  ot->poll = ED_operator_screenactive;

  prop = RNA_def_boolean(ot->srna,
                         "animation",
                         false,
                         "Animation",
                         "Render files from the animation range of this scene");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "render_keyed_only",
                         false,
                         "Render Keyframes Only",
                         "Render only those frames where selected objects have a key in their "
                         "animation data. Only used when rendering animation");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(
      ot->srna, "sequencer", false, "Sequencer", "Render using the sequencer's OpenGL display");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(
      ot->srna,
      "write_still",
      false,
      "Write Image",
      "Save rendered the image to the output path (used only when animation is disabled)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "view_context",
                         true,
                         "View Context",
                         "Use the current 3D view for rendering, else use scene settings");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

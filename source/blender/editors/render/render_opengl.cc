/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup render
 */

#include <condition_variable>
#include <cstddef>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_color_blend.h"
#include "BLI_mutex.hh"
#include "BLI_string_utf8.h"
#include "BLI_task.h"
#include "BLI_task.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_anim_data.hh"
#include "BKE_camera.h"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_fcurve.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_image_format.hh"
#include "BKE_image_save.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "DRW_engine.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"
#include "ED_view3d.hh"
#include "ED_view3d_offscreen.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "MOV_write.hh"

#include "RE_pipeline.h"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "SEQ_render.hh"

#include "ANIM_action_legacy.hh"

#include "GPU_context.hh"
#include "GPU_framebuffer.hh"
#include "GPU_matrix.hh"
#include "GPU_state.hh"
#include "GPU_viewport.hh"

#include "CLG_log.h"

#include "render_intern.hh"

namespace path_templates = blender::bke::path_templates;

static CLG_LogRef LOG = {"render"};

/* TODO(sergey): Find better approximation of the scheduled frames.
 * For really high-resolution renders it might fail still. */
#define MAX_SCHEDULED_FRAMES 8

struct OGLRender : public RenderJobBase {
  Main *bmain = nullptr;
  Render *re = nullptr;
  WorkSpace *workspace = nullptr;
  ViewLayer *view_layer = nullptr;
  Depsgraph *depsgraph = nullptr;

  View3D *v3d = nullptr;
  RegionView3D *rv3d = nullptr;
  ARegion *region = nullptr;

  int views_len = 0; /* multi-view views */

  bool is_sequencer = false;
  SpaceSeq *sseq = nullptr;
  struct {
    ImBuf **ibufs_arr = nullptr;
  } seq_data;

  Image *ima = nullptr;
  ImageUser iuser = {};

  GPUOffScreen *ofs = nullptr;
  int sizex = 0;
  int sizey = 0;
  int write_still = false;

  GPUViewport *viewport = nullptr;

  blender::Mutex reports_mutex;
  ReportList *reports = nullptr;

  int cfrao = 0;
  int nfra = 0;

  int totvideos = 0;

  /* For only rendering frames that have a key in animation data. */
  BLI_bitmap *render_frames = nullptr;

  /* quick lookup */
  int view_id = 0;

  /* wm vars for timer and progress cursor */
  wmWindowManager *wm = nullptr;
  wmWindow *win = nullptr;

  blender::Vector<MovieWriter *> movie_writers;

  TaskPool *task_pool = nullptr;
  bool pool_ok = true;
  bool is_animation = false;

  eImageFormatDepth color_depth = R_IMF_CHAN_DEPTH_32;
  uint num_scheduled_frames = 0;
  std::mutex task_mutex;
  std::condition_variable task_condition;

  wmJob *wm_job = nullptr;

  bool ended = false;
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
      rv = MEM_callocN<RenderView>("new opengl render view");
      BLI_addtail(&rr->views, rv);
    }

    while (rv->next) {
      RenderView *rv_del = rv->next;
      BLI_remlink(&rr->views, rv_del);

      IMB_freeImBuf(rv_del->ibuf);

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

        IMB_freeImBuf(rv_del->ibuf);

        MEM_freeN(rv_del);
      }
    }

    /* create all the views that are needed */
    LISTBASE_FOREACH (SceneRenderView *, srv, &rd->views) {
      if (BKE_scene_multiview_is_render_view_active(rd, srv) == false) {
        continue;
      }

      rv = static_cast<RenderView *>(
          BLI_findstring(&rr->views, srv->name, offsetof(SceneRenderView, name)));

      if (rv == nullptr) {
        rv = MEM_callocN<RenderView>("new opengl render view");
        STRNCPY_UTF8(rv->name, srv->name);
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

static void screen_opengl_render_doit(OGLRender *oglrender, RenderResult *rr)
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
        IMB_byte_from_float(ibuf_result);
        IMB_free_float_pixels(ibuf_result);
      }
      BLI_assert((sizex == ibuf->x) && (sizey == ibuf->y));
    }
    else if (gpd) {
      /* If there are no strips, Grease Pencil still needs a buffer to draw on */
      ibuf_result = IMB_allocImBuf(sizex, sizey, 32, IB_byte_data);
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
    Depsgraph *depsgraph = oglrender->depsgraph;
    char err_out[256] = "unknown";
    ImBuf *ibuf_view;
    bool draw_sky = (scene->r.alphamode == R_ADDSKY);
    const int alpha_mode = (draw_sky) ? R_ADDSKY : R_ALPHAPREMUL;
    const char *viewname = RE_GetActiveRenderView(oglrender->re);
    View3D *v3d = oglrender->v3d;

    BKE_scene_graph_evaluated_ensure(depsgraph, oglrender->bmain);

    if (v3d != nullptr) {
      ARegion *region = oglrender->region;
      ibuf_view = ED_view3d_draw_offscreen_imbuf(depsgraph,
                                                 scene,
                                                 static_cast<eDrawType>(v3d->shading.type),
                                                 v3d,
                                                 region,
                                                 sizex,
                                                 sizey,
                                                 IB_float_data,
                                                 alpha_mode,
                                                 viewname,
                                                 true,
                                                 oglrender->ofs,
                                                 oglrender->viewport,
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
                                                        IB_float_data,
                                                        V3D_OFSDRAW_SHOW_ANNOTATION,
                                                        alpha_mode,
                                                        viewname,
                                                        oglrender->ofs,
                                                        oglrender->viewport,
                                                        err_out);
      camera = scene->camera;
    }

    if (ibuf_view) {
      ibuf_result = ibuf_view;
    }
    else {
      CLOG_ERROR(&LOG, "%s: failed to get buffer, %s", __func__, err_out);
    }
  }

  if (ibuf_result != nullptr) {
    if ((scene->r.stamp & R_STAMP_ALL) && (scene->r.stamp & R_STAMP_DRAW)) {
      BKE_image_stamp_buf(scene, camera, nullptr, ibuf_result);
    }
    RE_render_result_rect_from_ibuf(rr, ibuf_result, oglrender->view_id);
    IMB_freeImBuf(ibuf_result);
  }

  /* Perform render step between renders to allow
   * flushing of freed GPUBackend resources. */
  DRW_gpu_context_enable();
  if (GPU_backend_get_type() == GPU_BACKEND_METAL) {
    GPU_flush();
  }
  GPU_render_step(true);
  DRW_gpu_context_disable();
}

static void screen_opengl_render_write(OGLRender *oglrender)
{
  Scene *scene = oglrender->scene;
  RenderResult *rr;
  bool ok;
  char filepath[FILE_MAX];

  rr = RE_AcquireResultRead(oglrender->re);

  path_templates::VariableMap template_variables;
  BKE_add_template_variables_general(template_variables, &scene->id);
  BKE_add_template_variables_for_render_path(template_variables, *scene);

  const char *relbase = BKE_main_blendfile_path(oglrender->bmain);
  const blender::Vector<path_templates::Error> errors = BKE_image_path_from_imformat(
      filepath,
      scene->r.pic,
      relbase,
      &template_variables,
      scene->r.cfra,
      &scene->r.im_format,
      (scene->r.scemode & R_EXTENSION) != 0,
      false,
      nullptr);

  if (!errors.is_empty()) {
    std::unique_lock lock(oglrender->reports_mutex);
    BKE_report_path_template_errors(oglrender->reports, RPT_ERROR, scene->r.pic, errors);
    ok = false;
  }
  else {
    /* write images as individual images or stereo */
    BKE_render_result_stamp_info(scene, scene->camera, rr, false);
    ok = BKE_image_render_write(oglrender->reports, rr, scene, false, filepath);

    RE_ReleaseResultImage(oglrender->re);
  }

  if (ok) {
    CLOG_INFO_NOCHECK(&LOG, "OpenGL Render written to '%s'", filepath);
  }
  else {
    CLOG_ERROR(&LOG, "OpenGL Render failed to write '%s'", filepath);
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

static void screen_opengl_render_apply(OGLRender *oglrender)
{
  RenderResult *rr;
  RenderView *rv;
  int view_id;
  ImBuf *ibuf;
  void *lock;

  if (oglrender->is_sequencer) {
    Scene *scene = oglrender->scene;

    blender::seq::RenderData context;
    SpaceSeq *sseq = oglrender->sseq;
    int chanshown = sseq ? sseq->chanshown : 0;

    blender::seq::render_new_render_data(oglrender->bmain,
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
      context.gpu_viewport = oglrender->viewport;
      oglrender->seq_data.ibufs_arr[view_id] = blender::seq::render_give_ibuf(
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
    screen_opengl_render_doit(oglrender, rr);
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

  for (const FCurve *fcu : blender::animrig::legacy::fcurves_for_assigned_action(adt)) {
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
  const LibraryForeachIDCallbackFlag cb_flag = cb_data->cb_flag;
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
      break;

      /* Blacklist: */
    case ID_SCE: /* Scene */
    case ID_LI:  /* Library */
    case ID_OB:  /* Object */
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

  const bool is_sequencer = RNA_boolean_get(op->ptr, "sequencer");

  Scene *scene = !is_sequencer ? CTX_data_scene(C) : CTX_data_sequencer_scene(C);
  if (!scene) {
    return false;
  }
  ScrArea *prev_area = CTX_wm_area(C);
  ARegion *prev_region = CTX_wm_region(C);
  GPUOffScreen *ofs;
  OGLRender *oglrender;
  int sizex, sizey;
  bool is_view_context = RNA_boolean_get(op->ptr, "view_context");
  const bool is_animation = RNA_boolean_get(op->ptr, "animation");
  const bool is_render_keyed_only = RNA_boolean_get(op->ptr, "render_keyed_only");
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

  if (!is_animation && is_write_still && BKE_imtype_is_movie(scene->r.im_format.imtype)) {
    BKE_report(
        op->reports, RPT_ERROR, "Cannot write a single file with an animation format selected");
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

  /* stop all running jobs, except screen one. currently previews frustrate Render */
  WM_jobs_kill_all_except(wm, CTX_wm_screen(C));

  /* create offscreen buffer */
  BKE_render_resolution(&scene->r, false, &sizex, &sizey);

  /* corrects render size with actual size, not every card supports non-power-of-two dimensions */
  DRW_gpu_context_enable(); /* Off-screen creation needs to be done in DRW context. */
  ofs = GPU_offscreen_create(sizex,
                             sizey,
                             true,
                             blender::gpu::TextureFormat::SFLOAT_16_16_16_16,
                             GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_HOST_READ,
                             false,
                             err_out);
  DRW_gpu_context_disable();

  if (!ofs) {
    BKE_reportf(op->reports, RPT_ERROR, "Failed to create OpenGL off-screen buffer, %s", err_out);
    CTX_wm_area_set(C, prev_area);
    CTX_wm_region_set(C, prev_region);
    return false;
  }

  /* allocate opengl render */
  oglrender = MEM_new<OGLRender>("OGLRender");
  op->customdata = oglrender;

  oglrender->ofs = ofs;
  oglrender->sizex = sizex;
  oglrender->sizey = sizey;
  oglrender->viewport = GPU_viewport_create();
  oglrender->bmain = CTX_data_main(C);
  oglrender->scene = scene;
  oglrender->current_scene = scene;
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

  if (is_view_context) {
    /* Prefer rendering camera in quad view if possible. */
    if (!ED_view3d_context_user_region(C, &oglrender->v3d, &oglrender->region)) {
      /* If not get region activated by ED_view3d_context_activate earlier. */
      oglrender->v3d = CTX_wm_view3d(C);
      oglrender->region = CTX_wm_region(C);
    }

    oglrender->rv3d = static_cast<RegionView3D *>(oglrender->region->regiondata);

    /* MUST be cleared on exit */
    oglrender->scene->customdata_mask_modal = CustomData_MeshMasks{};
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
  }

  CTX_wm_area_set(C, prev_area);
  CTX_wm_region_set(C, prev_region);

  return true;
}

static void screen_opengl_render_end(OGLRender *oglrender)
{
  /* Ensure we don't call this both from the job and operator callbacks. */
  if (oglrender->ended) {
    return;
  }

  if (oglrender->task_pool) {
    /* Trickery part for movie output:
     *
     * We MUST write frames in an exact order, so we only let background
     * thread to work on that, and main thread is simply waits for that
     * thread to do all the dirty work.
     *
     * After this loop is done work_and_wait() will have nothing to do,
     * so we don't run into wrong order of frames written to the stream.
     */
    if (BKE_imtype_is_movie(oglrender->scene->r.im_format.imtype)) {
      std::unique_lock lock(oglrender->task_mutex);
      while (oglrender->num_scheduled_frames > 0) {
        oglrender->task_condition.wait(lock);
      }
    }

    BLI_task_pool_work_and_wait(oglrender->task_pool);
    BLI_task_pool_free(oglrender->task_pool);
    oglrender->task_pool = nullptr;
  }

  MEM_SAFE_FREE(oglrender->render_frames);

  if (!oglrender->movie_writers.is_empty()) {
    if (BKE_imtype_is_movie(oglrender->scene->r.im_format.imtype)) {
      for (MovieWriter *writer : oglrender->movie_writers) {
        MOV_write_end(writer);
      }
    }
    oglrender->movie_writers.clear_and_shrink();
  }

  if (oglrender->ofs || oglrender->viewport) {
    DRW_gpu_context_enable();
    GPU_offscreen_free(oglrender->ofs);
    GPU_viewport_free(oglrender->viewport);
    DRW_gpu_context_disable();

    oglrender->ofs = nullptr;
    oglrender->viewport = nullptr;
  }

  MEM_SAFE_FREE(oglrender->seq_data.ibufs_arr);

  oglrender->scene->customdata_mask_modal = CustomData_MeshMasks{};

  if (oglrender->wm_job) { /* exec will not have a job */
    Depsgraph *depsgraph = oglrender->depsgraph;
    oglrender->scene->r.cfra = oglrender->cfrao;
    BKE_scene_graph_update_for_newframe(depsgraph);
  }
  else if (oglrender->win) {
    WM_cursor_modal_restore(oglrender->win);
  }

  WM_main_add_notifier(NC_SCENE | ND_RENDER_RESULT, oglrender->scene);
  G.is_rendering = false;
  oglrender->ended = true;
}

static void screen_opengl_render_cancel(bContext *C, wmOperator *op)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);

  if (oglrender->is_animation) {
    WM_jobs_kill_type(wm, oglrender->scene, WM_JOB_TYPE_RENDER);
  }

  screen_opengl_render_end(oglrender);
  MEM_delete(oglrender);
}

/* share between invoke and exec */
static bool screen_opengl_render_anim_init(wmOperator *op)
{
  /* initialize animation */
  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);
  Scene *scene = oglrender->scene;

  ImageFormatData image_format;
  BKE_image_format_init_for_write(&image_format, scene, nullptr, true);

  oglrender->totvideos = BKE_scene_multiview_num_videos_get(&scene->r, &image_format);
  oglrender->reports = op->reports;

  if (BKE_imtype_is_movie(image_format.imtype)) {
    size_t width, height;
    int i;

    BKE_scene_multiview_videos_dimensions_get(
        &scene->r, &image_format, oglrender->sizex, oglrender->sizey, &width, &height);
    oglrender->movie_writers.reserve(oglrender->totvideos);

    for (i = 0; i < oglrender->totvideos; i++) {
      Scene *scene_eval = DEG_get_evaluated_scene(oglrender->depsgraph);
      const char *suffix = BKE_scene_multiview_view_id_suffix_get(&scene->r, i);
      MovieWriter *writer = MOV_write_begin(scene_eval,
                                            &scene->r,
                                            &image_format,
                                            oglrender->sizex,
                                            oglrender->sizey,
                                            oglrender->reports,
                                            PRVRANGEON != 0,
                                            suffix);
      if (writer == nullptr) {
        BKE_image_format_free(&image_format);
        screen_opengl_render_end(oglrender);
        MEM_delete(oglrender);
        return false;
      }
      oglrender->movie_writers.append(writer);
    }
  }

  BKE_image_format_free(&image_format);

  G.is_rendering = true;
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
    std::scoped_lock lock(oglrender->task_mutex);
    oglrender->num_scheduled_frames--;
    oglrender->task_condition.notify_all();
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
                                  oglrender->movie_writers.data(),
                                  oglrender->totvideos,
                                  PRVRANGEON != 0);
  }
  else {
    /* TODO(sergey): We can in theory save some CPU ticks here because we
     * calculate file name again here.
     */
    char filepath[FILE_MAX];
    path_templates::VariableMap template_variables;
    BKE_add_template_variables_general(template_variables, &scene->id);
    BKE_add_template_variables_for_render_path(template_variables, *scene);

    const char *relbase = BKE_main_blendfile_path(oglrender->bmain);
    const blender::Vector<path_templates::Error> errors = BKE_image_path_from_imformat(
        filepath,
        scene->r.pic,
        relbase,
        &template_variables,
        cfra,
        &scene->r.im_format,
        (scene->r.scemode & R_EXTENSION) != 0,
        true,
        nullptr);

    if (!errors.is_empty()) {
      BKE_report_path_template_errors(&reports, RPT_ERROR, scene->r.pic, errors);
      ok = false;
    }
    else {
      BKE_render_result_stamp_info(scene, scene->camera, rr, false);
      ok = BKE_image_render_write(nullptr, rr, scene, true, filepath);
    }

    if (!ok) {
      BKE_reportf(&reports, RPT_ERROR, "Write error: cannot save %s", filepath);
    }
  }
  if (reports.list.first != nullptr) {
    /* TODO: Should rather use new #BKE_reports_move_to_reports ? */
    std::unique_lock lock(oglrender->reports_mutex);
    for (Report *report = static_cast<Report *>(reports.list.first); report != nullptr;
         report = report->next)
    {
      BKE_report(oglrender->reports, static_cast<eReportType>(report->type), report->message);
    }
  }
  BKE_reports_free(&reports);
  if (!ok) {
    oglrender->pool_ok = false;
  }
  RE_FreeRenderResult(rr);

  {
    std::unique_lock lock(oglrender->task_mutex);
    oglrender->num_scheduled_frames--;
    oglrender->task_condition.notify_all();
  }
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
  WriteTaskData *task_data = MEM_callocN<WriteTaskData>("write task data");
  task_data->rr = rr;
  memcpy(&task_data->tmp_scene, scene, sizeof(task_data->tmp_scene));
  {
    std::unique_lock lock(oglrender->task_mutex);
    oglrender->num_scheduled_frames++;
    if (oglrender->num_scheduled_frames > MAX_SCHEDULED_FRAMES) {
      oglrender->task_condition.wait(lock);
    }
  }
  BLI_task_pool_push(oglrender->task_pool, write_result_func, task_data, true, nullptr);
  return true;
}

static bool screen_opengl_render_anim_step(OGLRender *oglrender)
{
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
    path_templates::VariableMap template_variables;
    BKE_add_template_variables_general(template_variables, &scene->id);
    BKE_add_template_variables_for_render_path(template_variables, *scene);

    const char *relbase = BKE_main_blendfile_path(oglrender->bmain);
    const blender::Vector<path_templates::Error> errors = BKE_image_path_from_imformat(
        filepath,
        scene->r.pic,
        relbase,
        &template_variables,
        scene->r.cfra,
        &scene->r.im_format,
        (scene->r.scemode & R_EXTENSION) != 0,
        true,
        nullptr);

    if (!errors.is_empty()) {
      std::unique_lock lock(oglrender->reports_mutex);
      BKE_report_path_template_errors(oglrender->reports, RPT_ERROR, scene->r.pic, errors);
      ok = false;
    }
    else if ((scene->r.mode & R_NO_OVERWRITE) && BLI_exists(filepath)) {
      {
        std::unique_lock lock(oglrender->reports_mutex);
        BKE_reportf(oglrender->reports, RPT_INFO, "Skipping existing frame \"%s\"", filepath);
      }
      ok = true;
      goto finally;
    }
  }

  if (!oglrender->wm_job && oglrender->win) {
    /* When doing blocking animation render without a job from a Python script, show time cursor so
     * Blender doesn't appear frozen. */
    WM_cursor_time(oglrender->win, scene->r.cfra);
  }

  BKE_scene_graph_update_for_newframe(depsgraph);

  if (view_context) {
    if (oglrender->rv3d->persp == RV3D_CAMOB && oglrender->v3d->camera &&
        oglrender->v3d->scenelock)
    {
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
    screen_opengl_render_apply(oglrender);
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
    return false;
  }

  return true;
}

static wmOperatorStatus screen_opengl_render_modal(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);

  /* Still render completes immediately, but still modal to show some feedback
   * in case render initialization takes a while. */
  if (!oglrender->is_animation) {
    screen_opengl_render_apply(oglrender);
    screen_opengl_render_end(oglrender);
    MEM_delete(oglrender);
    return OPERATOR_FINISHED;
  }

  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), oglrender->scene, WM_JOB_TYPE_RENDER)) {
    screen_opengl_render_end(oglrender);
    MEM_delete(oglrender);
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* catch escape key. */
  return (event->type == EVT_ESCKEY) ? OPERATOR_RUNNING_MODAL : OPERATOR_PASS_THROUGH;
}

static void opengl_render_startjob(void *customdata, wmJobWorkerStatus *worker_status)
{
  OGLRender *oglrender = static_cast<OGLRender *>(customdata);
  Scene *scene = oglrender->scene;

  bool canceled = false;
  bool finished = false;

  while (!finished && !canceled) {
    /* Render while blocking main thread, since we use 3D viewport resources. */
    WM_job_main_thread_lock_acquire(oglrender->wm_job);

    if (worker_status->stop || G.is_break) {
      canceled = true;
    }
    else {
      finished = !screen_opengl_render_anim_step(oglrender);
      worker_status->progress = float(scene->r.cfra - PSFRA + 1) / float(PEFRA - PSFRA + 1);
      worker_status->do_update = true;
    }

    WM_job_main_thread_lock_release(oglrender->wm_job);

    if (worker_status->stop || G.is_break) {
      canceled = true;
    }
  }

  if (canceled) {
    /* Cancel task pool writing images asynchronously. */
    oglrender->pool_ok = false;
  }
}

static void opengl_render_freejob(void *customdata)
{
  /* End the render here, as the modal handler might be called with the window out of focus. */
  OGLRender *oglrender = static_cast<OGLRender *>(customdata);
  screen_opengl_render_end(oglrender);
}

static wmOperatorStatus screen_opengl_render_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  const bool anim = RNA_boolean_get(op->ptr, "animation");

  if (!screen_opengl_render_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (anim) {
    if (!screen_opengl_render_anim_init(op)) {
      return OPERATOR_CANCELLED;
    }
  }

  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);
  render_view_open(C, event->xy[0], event->xy[1], op->reports);

  /* View may be changed above #USER_RENDER_DISPLAY_WINDOW. */
  oglrender->win = CTX_wm_window(C);

  /* Setup animation job. */
  if (anim) {
    G.is_break = false;

    wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                                CTX_wm_window(C),
                                oglrender->scene,
                                "Rendering viewport...",
                                WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS,
                                WM_JOB_TYPE_RENDER);

    oglrender->wm_job = wm_job;

    WM_jobs_customdata_set(wm_job, oglrender, opengl_render_freejob);
    WM_jobs_timer(wm_job, 0.01f, NC_SCENE | ND_RENDER_RESULT, 0);
    WM_jobs_callbacks(wm_job, opengl_render_startjob, nullptr, nullptr, nullptr);
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* executes blocking render */
static wmOperatorStatus screen_opengl_render_exec(bContext *C, wmOperator *op)
{
  if (!screen_opengl_render_init(C, op)) {
    return OPERATOR_CANCELLED;
  }

  OGLRender *oglrender = static_cast<OGLRender *>(op->customdata);

  if (!oglrender->is_animation) { /* same as invoke */
    screen_opengl_render_apply(oglrender);
    screen_opengl_render_end(oglrender);
    MEM_delete(oglrender);

    return OPERATOR_FINISHED;
  }

  bool ret = true;

  if (!screen_opengl_render_anim_init(op)) {
    return OPERATOR_CANCELLED;
  }

  while (ret) {
    ret = screen_opengl_render_anim_step(oglrender);
  }

  screen_opengl_render_end(oglrender);
  MEM_delete(oglrender);

  return OPERATOR_FINISHED;
}

static std::string screen_opengl_render_get_description(bContext * /*C*/,
                                                        wmOperatorType * /*ot*/,
                                                        PointerRNA *ptr)
{
  if (!RNA_boolean_get(ptr, "animation")) {
    return "";
  }

  if (RNA_boolean_get(ptr, "render_keyed_only")) {
    return TIP_(
        "Render the viewport for the animation range of this scene, but only render keyframes of "
        "selected objects");
  }

  return TIP_("Render the viewport for the animation range of this scene");
}

void RENDER_OT_opengl(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Viewport Render";
  ot->description = "Take a snapshot of the active viewport";
  ot->idname = "RENDER_OT_opengl";

  /* API callbacks. */
  ot->get_description = screen_opengl_render_get_description;
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
      "Save the rendered image to the output path (used only when animation is disabled)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "view_context",
                         true,
                         "View Context",
                         "Use the current 3D view for rendering, else use scene settings");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

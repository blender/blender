/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"

#include "BKE_DerivedMesh.hh"
#include "BKE_attribute.hh"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_image.h"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "RE_multires_bake.h"
#include "RE_pipeline.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_uvedit.hh"

#include "object_intern.h"

static Image *bake_object_image_get(Object *ob, int mat_nr)
{
  Image *image = nullptr;
  ED_object_get_active_image(ob, mat_nr + 1, &image, nullptr, nullptr, nullptr);
  return image;
}

static Image **bake_object_image_get_array(Object *ob)
{
  Image **image_array = static_cast<Image **>(
      MEM_mallocN(sizeof(Material *) * ob->totcol, __func__));
  for (int i = 0; i < ob->totcol; i++) {
    image_array[i] = bake_object_image_get(ob, i);
  }
  return image_array;
}

/* ****************** multires BAKING ********************** */

/* holder of per-object data needed for bake job
 * needed to make job totally thread-safe */
struct MultiresBakerJobData {
  MultiresBakerJobData *next, *prev;
  /* material aligned image array (for per-face bake image) */
  struct {
    Image **array;
    int len;
  } ob_image;
  DerivedMesh *lores_dm, *hires_dm;
  int lvl, tot_lvl;
  ListBase images;
};

/* data passing to multires-baker job */
struct MultiresBakeJob {
  Scene *scene;
  ListBase data;
  /** Clear the images before baking */
  bool bake_clear;
  /** Margin size in pixels. */
  int bake_margin;
  /** margin type */
  char bake_margin_type;
  /** mode of baking (displacement, normals, AO) */
  short mode;
  /** Use low-resolution mesh when baking displacement maps */
  bool use_lores_mesh;
  /** Number of rays to be cast when doing AO baking */
  int number_of_rays;
  /** Bias between object and start ray point when doing AO baking */
  float bias;
  /** Number of threads to be used for baking */
  int threads;
  /** User scale used to scale displacement when baking derivative map. */
  float user_scale;
};

static bool multiresbake_check(bContext *C, wmOperator *op)
{
  using namespace blender;
  Scene *scene = CTX_data_scene(C);
  Object *ob;
  Mesh *mesh;
  MultiresModifierData *mmd;
  bool ok = true;
  int a;

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    ob = base->object;

    if (ob->type != OB_MESH) {
      BKE_report(
          op->reports, RPT_ERROR, "Baking of multires data only works with an active mesh object");

      ok = false;
      break;
    }

    mesh = (Mesh *)ob->data;
    mmd = get_multires_modifier(scene, ob, false);

    /* Multi-resolution should be and be last in the stack */
    if (ok && mmd) {
      ModifierData *md;

      ok = mmd->totlvl > 0;

      for (md = (ModifierData *)mmd->modifier.next; md && ok; md = md->next) {
        if (BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
          ok = false;
        }
      }
    }
    else {
      ok = false;
    }

    if (!ok) {
      BKE_report(op->reports, RPT_ERROR, "Multires data baking requires multi-resolution object");

      break;
    }

    if (!CustomData_has_layer(&mesh->corner_data, CD_PROP_FLOAT2)) {
      BKE_report(op->reports, RPT_ERROR, "Mesh should be unwrapped before multires data baking");

      ok = false;
    }
    else {
      const bke::AttributeAccessor attributes = mesh->attributes();
      const VArraySpan material_indices = *attributes.lookup<int>("material_index",
                                                                  bke::AttrDomain::Face);
      a = mesh->faces_num;
      while (ok && a--) {
        Image *ima = bake_object_image_get(ob,
                                           material_indices.is_empty() ? 0 : material_indices[a]);

        if (!ima) {
          BKE_report(
              op->reports, RPT_ERROR, "You should have active texture to use multires baker");

          ok = false;
        }
        else {
          LISTBASE_FOREACH (ImageTile *, tile, &ima->tiles) {
            ImageUser iuser;
            BKE_imageuser_default(&iuser);
            iuser.tile = tile->tile_number;

            ImBuf *ibuf = BKE_image_acquire_ibuf(ima, &iuser, nullptr);

            if (!ibuf) {
              BKE_report(
                  op->reports, RPT_ERROR, "Baking should happen to image with image buffer");

              ok = false;
            }
            else {
              if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
                ok = false;
              }

              if (ibuf->float_buffer.data && !ELEM(ibuf->channels, 0, 4)) {
                ok = false;
              }

              if (!ok) {
                BKE_report(op->reports, RPT_ERROR, "Baking to unsupported image type");
              }
            }

            BKE_image_release_ibuf(ima, ibuf, nullptr);
          }
        }
      }
    }

    if (!ok) {
      break;
    }
  }
  CTX_DATA_END;

  return ok;
}

static DerivedMesh *multiresbake_create_loresdm(Scene *scene, Object *ob, int *lvl)
{
  DerivedMesh *dm;
  MultiresModifierData *mmd = get_multires_modifier(scene, ob, false);
  Mesh *mesh = (Mesh *)ob->data;
  MultiresModifierData tmp_mmd = blender::dna::shallow_copy(*mmd);

  *lvl = mmd->lvl;

  if (mmd->lvl == 0) {
    DerivedMesh *cddm = CDDM_from_mesh(mesh);
    DM_set_only_copy(cddm, &CD_MASK_BAREMESH);
    return cddm;
  }

  DerivedMesh *cddm = CDDM_from_mesh(mesh);
  DM_set_only_copy(cddm, &CD_MASK_BAREMESH);
  tmp_mmd.lvl = mmd->lvl;
  tmp_mmd.sculptlvl = mmd->lvl;
  dm = multires_make_derived_from_derived(cddm, &tmp_mmd, scene, ob, MultiresFlags(0));

  cddm->release(cddm);

  return dm;
}

static DerivedMesh *multiresbake_create_hiresdm(Scene *scene, Object *ob, int *lvl)
{
  Mesh *mesh = (Mesh *)ob->data;
  MultiresModifierData *mmd = get_multires_modifier(scene, ob, false);
  MultiresModifierData tmp_mmd = blender::dna::shallow_copy(*mmd);
  DerivedMesh *cddm = CDDM_from_mesh(mesh);
  DerivedMesh *dm;

  DM_set_only_copy(cddm, &CD_MASK_BAREMESH);

  /* TODO: DM_set_only_copy wouldn't set mask for loop and poly data,
   *       but we really need BAREMESH only to save lots of memory
   */
  CustomData_set_only_copy(&cddm->loopData, CD_MASK_BAREMESH.lmask);
  CustomData_set_only_copy(&cddm->polyData, CD_MASK_BAREMESH.pmask);

  *lvl = mmd->totlvl;

  tmp_mmd.lvl = mmd->totlvl;
  tmp_mmd.sculptlvl = mmd->totlvl;
  dm = multires_make_derived_from_derived(cddm, &tmp_mmd, scene, ob, MultiresFlags(0));
  cddm->release(cddm);

  return dm;
}

enum ClearFlag {
  CLEAR_TANGENT_NORMAL = 1,
  CLEAR_DISPLACEMENT = 2,
};

static void clear_single_image(Image *image, ClearFlag flag)
{
  const float vec_alpha[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  const float vec_solid[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  const float nor_alpha[4] = {0.5f, 0.5f, 1.0f, 0.0f};
  const float nor_solid[4] = {0.5f, 0.5f, 1.0f, 1.0f};
  const float disp_alpha[4] = {0.5f, 0.5f, 0.5f, 0.0f};
  const float disp_solid[4] = {0.5f, 0.5f, 0.5f, 1.0f};

  if ((image->id.tag & LIB_TAG_DOIT) == 0) {
    LISTBASE_FOREACH (ImageTile *, tile, &image->tiles) {
      ImageUser iuser;
      BKE_imageuser_default(&iuser);
      iuser.tile = tile->tile_number;

      ImBuf *ibuf = BKE_image_acquire_ibuf(image, &iuser, nullptr);

      if (flag == CLEAR_TANGENT_NORMAL) {
        IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? nor_alpha : nor_solid);
      }
      else if (flag == CLEAR_DISPLACEMENT) {
        IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? disp_alpha : disp_solid);
      }
      else {
        IMB_rectfill(ibuf, (ibuf->planes == R_IMF_PLANES_RGBA) ? vec_alpha : vec_solid);
      }

      image->id.tag |= LIB_TAG_DOIT;

      BKE_image_release_ibuf(image, ibuf, nullptr);
    }
  }
}

static void clear_images_poly(Image **ob_image_array, int ob_image_array_len, ClearFlag flag)
{
  for (int i = 0; i < ob_image_array_len; i++) {
    Image *image = ob_image_array[i];
    if (image) {
      image->id.tag &= ~LIB_TAG_DOIT;
    }
  }

  for (int i = 0; i < ob_image_array_len; i++) {
    Image *image = ob_image_array[i];
    if (image) {
      clear_single_image(image, flag);
    }
  }

  for (int i = 0; i < ob_image_array_len; i++) {
    Image *image = ob_image_array[i];
    if (image) {
      image->id.tag &= ~LIB_TAG_DOIT;
    }
  }
}

static int multiresbake_image_exec_locked(bContext *C, wmOperator *op)
{
  Object *ob;
  Scene *scene = CTX_data_scene(C);
  int objects_baked = 0;

  if (!multiresbake_check(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (scene->r.bake_flag & R_BAKE_CLEAR) { /* clear images */
    CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
      ClearFlag clear_flag = ClearFlag(0);

      ob = base->object;
      // mesh = (Mesh *)ob->data;

      if (scene->r.bake_mode == RE_BAKE_NORMALS) {
        clear_flag = CLEAR_TANGENT_NORMAL;
      }
      else if (scene->r.bake_mode == RE_BAKE_DISPLACEMENT) {
        clear_flag = CLEAR_DISPLACEMENT;
      }

      {
        Image **ob_image_array = bake_object_image_get_array(ob);
        clear_images_poly(ob_image_array, ob->totcol, clear_flag);
        MEM_freeN(ob_image_array);
      }
    }
    CTX_DATA_END;
  }

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    MultiresBakeRender bkr = {nullptr};

    ob = base->object;

    multires_flush_sculpt_updates(ob);

    /* copy data stored in job descriptor */
    bkr.scene = scene;
    bkr.bake_margin = scene->r.bake_margin;
    if (scene->r.bake_mode == RE_BAKE_NORMALS) {
      bkr.bake_margin_type = R_BAKE_EXTEND;
    }
    else {
      bkr.bake_margin_type = scene->r.bake_margin_type;
    }
    bkr.mode = scene->r.bake_mode;
    bkr.use_lores_mesh = scene->r.bake_flag & R_BAKE_LORES_MESH;
    bkr.bias = scene->r.bake_biasdist;
    bkr.number_of_rays = scene->r.bake_samples;
    bkr.threads = BKE_scene_num_threads(scene);
    bkr.user_scale = (scene->r.bake_flag & R_BAKE_USERSCALE) ? scene->r.bake_user_scale : -1.0f;
    // bkr.reports= op->reports;

    /* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
    bkr.ob_image.array = bake_object_image_get_array(ob);
    bkr.ob_image.len = ob->totcol;

    bkr.hires_dm = multiresbake_create_hiresdm(scene, ob, &bkr.tot_lvl);
    bkr.lores_dm = multiresbake_create_loresdm(scene, ob, &bkr.lvl);

    RE_multires_bake_images(&bkr);

    MEM_freeN(bkr.ob_image.array);

    BLI_freelistN(&bkr.image);

    bkr.lores_dm->release(bkr.lores_dm);
    bkr.hires_dm->release(bkr.hires_dm);

    objects_baked++;
  }
  CTX_DATA_END;

  if (!objects_baked) {
    BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");
  }

  return OPERATOR_FINISHED;
}

/**
 * Multi-resolution-bake adopted for job-system executing.
 */
static void init_multiresbake_job(bContext *C, MultiresBakeJob *bkj)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob;

  /* backup scene settings, so their changing in UI would take no effect on baker */
  bkj->scene = scene;
  bkj->bake_margin = scene->r.bake_margin;
  if (scene->r.bake_mode == RE_BAKE_NORMALS) {
    bkj->bake_margin_type = R_BAKE_EXTEND;
  }
  else {
    bkj->bake_margin_type = scene->r.bake_margin_type;
  }
  bkj->mode = scene->r.bake_mode;
  bkj->use_lores_mesh = scene->r.bake_flag & R_BAKE_LORES_MESH;
  bkj->bake_clear = scene->r.bake_flag & R_BAKE_CLEAR;
  bkj->bias = scene->r.bake_biasdist;
  bkj->number_of_rays = scene->r.bake_samples;
  bkj->threads = BKE_scene_num_threads(scene);
  bkj->user_scale = (scene->r.bake_flag & R_BAKE_USERSCALE) ? scene->r.bake_user_scale : -1.0f;
  // bkj->reports = op->reports;

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    int lvl;

    ob = base->object;

    multires_flush_sculpt_updates(ob);

    MultiresBakerJobData *data = MEM_cnew<MultiresBakerJobData>(__func__);

    data->ob_image.array = bake_object_image_get_array(ob);
    data->ob_image.len = ob->totcol;

    /* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
    data->hires_dm = multiresbake_create_hiresdm(scene, ob, &data->tot_lvl);
    data->lores_dm = multiresbake_create_loresdm(scene, ob, &lvl);
    data->lvl = lvl;

    BLI_addtail(&bkj->data, data);
  }
  CTX_DATA_END;
}

static void multiresbake_startjob(void *bkv, wmJobWorkerStatus *worker_status)
{
  MultiresBakeJob *bkj = static_cast<MultiresBakeJob *>(bkv);
  int baked_objects = 0, tot_obj;

  tot_obj = BLI_listbase_count(&bkj->data);

  if (bkj->bake_clear) { /* clear images */
    LISTBASE_FOREACH (MultiresBakerJobData *, data, &bkj->data) {
      ClearFlag clear_flag = ClearFlag(0);

      if (bkj->mode == RE_BAKE_NORMALS) {
        clear_flag = CLEAR_TANGENT_NORMAL;
      }
      else if (bkj->mode == RE_BAKE_DISPLACEMENT) {
        clear_flag = CLEAR_DISPLACEMENT;
      }

      clear_images_poly(data->ob_image.array, data->ob_image.len, clear_flag);
    }
  }

  LISTBASE_FOREACH (MultiresBakerJobData *, data, &bkj->data) {
    MultiresBakeRender bkr = {nullptr};

    /* copy data stored in job descriptor */
    bkr.scene = bkj->scene;
    bkr.bake_margin = bkj->bake_margin;
    bkr.bake_margin_type = bkj->bake_margin_type;
    bkr.mode = bkj->mode;
    bkr.use_lores_mesh = bkj->use_lores_mesh;
    bkr.user_scale = bkj->user_scale;
    // bkr.reports = bkj->reports;
    bkr.ob_image.array = data->ob_image.array;
    bkr.ob_image.len = data->ob_image.len;

    /* create low-resolution DM (to bake to) and hi-resolution DM (to bake from) */
    bkr.lores_dm = data->lores_dm;
    bkr.hires_dm = data->hires_dm;
    bkr.tot_lvl = data->tot_lvl;
    bkr.lvl = data->lvl;

    /* needed for proper progress bar */
    bkr.tot_obj = tot_obj;
    bkr.baked_objects = baked_objects;

    bkr.stop = &worker_status->stop;
    bkr.do_update = &worker_status->do_update;
    bkr.progress = &worker_status->progress;

    bkr.bias = bkj->bias;
    bkr.number_of_rays = bkj->number_of_rays;
    bkr.threads = bkj->threads;

    RE_multires_bake_images(&bkr);

    data->images = bkr.image;

    baked_objects++;
  }
}

static void multiresbake_freejob(void *bkv)
{
  MultiresBakeJob *bkj = static_cast<MultiresBakeJob *>(bkv);
  MultiresBakerJobData *data, *next;

  data = static_cast<MultiresBakerJobData *>(bkj->data.first);
  while (data) {
    next = data->next;
    data->lores_dm->release(data->lores_dm);
    data->hires_dm->release(data->hires_dm);

    /* delete here, since this delete will be called from main thread */
    LISTBASE_FOREACH (LinkData *, link, &data->images) {
      Image *ima = (Image *)link->data;
      BKE_image_partial_update_mark_full_update(ima);
    }

    MEM_freeN(data->ob_image.array);

    BLI_freelistN(&data->images);

    MEM_freeN(data);
    data = next;
  }

  MEM_freeN(bkj);
}

static int multiresbake_image_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  if (!multiresbake_check(C, op)) {
    return OPERATOR_CANCELLED;
  }

  MultiresBakeJob *bkr = MEM_cnew<MultiresBakeJob>(__func__);
  init_multiresbake_job(C, bkr);

  if (!bkr->data.first) {
    BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");
    return OPERATOR_CANCELLED;
  }

  /* setup job */
  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Multires Bake",
                              WM_JOB_EXCL_RENDER | WM_JOB_PRIORITY | WM_JOB_PROGRESS,
                              WM_JOB_TYPE_OBJECT_BAKE_TEXTURE);
  WM_jobs_customdata_set(wm_job, bkr, multiresbake_freejob);
  WM_jobs_timer(wm_job, 0.5, NC_IMAGE, 0); /* TODO: only draw bake image, can we enforce this. */
  WM_jobs_callbacks(wm_job, multiresbake_startjob, nullptr, nullptr, nullptr);

  G.is_break = false;

  WM_jobs_start(CTX_wm_manager(C), wm_job);
  WM_cursor_wait(false);

  /* add modal handler for ESC */
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

/* ****************** render BAKING ********************** */

/** Catch escape key to cancel. */
static int objects_bake_render_modal(bContext *C, wmOperator * /*op*/, const wmEvent *event)
{
  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_OBJECT_BAKE_TEXTURE)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

static bool is_multires_bake(Scene *scene)
{
  if (ELEM(scene->r.bake_mode, RE_BAKE_NORMALS, RE_BAKE_DISPLACEMENT, RE_BAKE_AO)) {
    return scene->r.bake_flag & R_BAKE_MULTIRES;
  }

  return false;
}

static int objects_bake_render_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_scene(C);
  int result = OPERATOR_CANCELLED;

  result = multiresbake_image_exec(C, op);

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  return result;
}

static int bake_image_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int result = OPERATOR_CANCELLED;

  if (!is_multires_bake(scene)) {
    BLI_assert(0);
    return result;
  }

  result = multiresbake_image_exec_locked(C, op);

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  return result;
}

void OBJECT_OT_bake_image(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Bake";
  ot->description = "Bake image textures of selected objects";
  ot->idname = "OBJECT_OT_bake_image";

  /* api callbacks */
  ot->exec = bake_image_exec;
  ot->invoke = objects_bake_render_invoke;
  ot->modal = objects_bake_render_modal;
  ot->poll = ED_operator_object_active;
}

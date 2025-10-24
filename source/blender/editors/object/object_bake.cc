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

#include "BLI_listbase.h"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_global.hh"
#include "BKE_image.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_subdiv.hh"

#include "RE_multires_bake.h"
#include "RE_pipeline.h"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_uvedit.hh"

#include "object_intern.hh"

namespace blender::ed::object {

static Image *bake_object_image_get(Object &object, const int mat_nr)
{
  Image *image = nullptr;
  ED_object_get_active_image(&object, mat_nr + 1, &image, nullptr, nullptr, nullptr);
  return image;
}

static Vector<Image *> bake_object_image_get_array(Object &object)
{
  Vector<Image *> images;
  images.reserve(object.totcol);
  for (int i = 0; i < object.totcol; i++) {
    images.append(bake_object_image_get(object, i));
  }
  return images;
}

/* ****************** multires BAKING ********************** */

/* holder of per-object data needed for bake job
 * needed to make job totally thread-safe */
struct MultiresBakerJobData {
  MultiresBakerJobData *next = nullptr, *prev = nullptr;

  /* Material aligned image array (for per-face bake image). */
  Vector<Image *> ob_image;

  /* Base mesh at the input of the multiresolution modifier. */
  Mesh *base_mesh = nullptr;

  /* Multi-resolution modifier which is being baked. */
  MultiresModifierData *multires_modifier = nullptr;

  Set<Image *> images;
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
  eBakeType type;
  eBakeSpace displacement_space;
  /** Use low-resolution mesh when baking displacement maps */
  bool use_low_resolution_mesh;
};

static bool multiresbake_check(bContext *C, wmOperator *op)
{
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

    if (mesh->uv_map_names().is_empty()) {
      BKE_report(op->reports, RPT_ERROR, "Mesh should be unwrapped before multires data baking");

      ok = false;
    }
    else {
      const bke::AttributeAccessor attributes = mesh->attributes();
      const VArraySpan material_indices = *attributes.lookup<int>("material_index",
                                                                  bke::AttrDomain::Face);
      a = mesh->faces_num;
      while (ok && a--) {
        Image *ima = bake_object_image_get(*ob,
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

  if ((image->id.tag & ID_TAG_DOIT) == 0) {
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

      image->id.tag |= ID_TAG_DOIT;

      BKE_image_release_ibuf(image, ibuf, nullptr);
    }
  }
}

static void clear_images_poly(const Span<Image *> ob_image_array, const ClearFlag flag)
{
  for (Image *image : ob_image_array) {
    if (image) {
      image->id.tag &= ~ID_TAG_DOIT;
    }
  }

  for (Image *image : ob_image_array) {
    if (image) {
      clear_single_image(image, flag);
    }
  }

  for (Image *image : ob_image_array) {
    if (image) {
      image->id.tag &= ~ID_TAG_DOIT;
    }
  }
}

static wmOperatorStatus multiresbake_image_exec_locked(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  int objects_baked = 0;

  if (!multiresbake_check(C, op)) {
    return OPERATOR_CANCELLED;
  }

  if (scene->r.bake.flag & R_BAKE_CLEAR) { /* clear images */
    CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
      Object &object = *base->object;
      BLI_assert(object.type == OB_MESH);

      ClearFlag clear_flag = ClearFlag(0);

      if (scene->r.bake.type == R_BAKE_NORMALS) {
        clear_flag = CLEAR_TANGENT_NORMAL;
      }
      else if (scene->r.bake.type == R_BAKE_DISPLACEMENT) {
        clear_flag = CLEAR_DISPLACEMENT;
      }

      {
        const Vector<Image *> ob_image_array = bake_object_image_get_array(object);
        clear_images_poly(ob_image_array, clear_flag);
      }
    }
    CTX_DATA_END;
  }

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    Object &object = *base->object;
    BLI_assert(object.type == OB_MESH);

    MultiresBakeRender bake;

    multires_flush_sculpt_updates(&object);

    /* Copy data stored in job descriptor. */
    bake.bake_margin = scene->r.bake.margin;
    if (scene->r.bake.type == R_BAKE_NORMALS) {
      bake.bake_margin_type = R_BAKE_EXTEND;
    }
    else {
      bake.bake_margin_type = eBakeMarginType(scene->r.bake.margin_type);
    }
    bake.type = eBakeType(scene->r.bake.type);
    bake.displacement_space = eBakeSpace(scene->r.bake.displacement_space);
    bake.use_low_resolution_mesh = scene->r.bake.flag & R_BAKE_LORES_MESH;

    bake.ob_image = bake_object_image_get_array(object);

    bake.base_mesh = static_cast<Mesh *>(object.data);
    bake.multires_modifier = get_multires_modifier(scene, &object, false);

    RE_multires_bake_images(bake);

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

  /* backup scene settings, so their changing in UI would take no effect on baker */
  bkj->scene = scene;
  bkj->bake_margin = scene->r.bake.margin;
  if (scene->r.bake.type == R_BAKE_NORMALS) {
    bkj->bake_margin_type = R_BAKE_EXTEND;
  }
  else {
    bkj->bake_margin_type = eBakeMarginType(scene->r.bake.margin_type);
  }
  bkj->type = eBakeType(scene->r.bake.type);
  bkj->displacement_space = eBakeSpace(scene->r.bake.displacement_space);
  bkj->use_low_resolution_mesh = scene->r.bake.flag & R_BAKE_LORES_MESH;
  bkj->bake_clear = scene->r.bake.flag & R_BAKE_CLEAR;

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    Object &object = *base->object;
    BLI_assert(object.type == OB_MESH);

    multires_flush_sculpt_updates(&object);

    MultiresBakerJobData *data = MEM_new<MultiresBakerJobData>(__func__);

    data->ob_image = bake_object_image_get_array(object);

    data->base_mesh = static_cast<Mesh *>(object.data);
    data->multires_modifier = get_multires_modifier(scene, &object, false);

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

      if (bkj->type == R_BAKE_NORMALS) {
        clear_flag = CLEAR_TANGENT_NORMAL;
      }
      else if (bkj->type == R_BAKE_DISPLACEMENT) {
        clear_flag = CLEAR_DISPLACEMENT;
      }

      clear_images_poly(data->ob_image, clear_flag);
    }
  }

  LISTBASE_FOREACH (MultiresBakerJobData *, data, &bkj->data) {
    MultiresBakeRender bake;

    /* copy data stored in job descriptor */
    bake.bake_margin = bkj->bake_margin;
    bake.bake_margin_type = eBakeMarginType(bkj->bake_margin_type);
    bake.type = bkj->type;
    bake.displacement_space = bkj->displacement_space;
    bake.use_low_resolution_mesh = bkj->use_low_resolution_mesh;
    bake.ob_image = data->ob_image;

    bake.base_mesh = data->base_mesh;
    bake.multires_modifier = data->multires_modifier;

    /* needed for proper progress bar */
    bake.num_total_objects = tot_obj;
    bake.num_baked_objects = baked_objects;

    bake.stop = &worker_status->stop;
    bake.do_update = &worker_status->do_update;
    bake.progress = &worker_status->progress;

    RE_multires_bake_images(bake);

    data->images = bake.images;

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

    /* delete here, since this delete will be called from main thread */
    for (Image *image : data->images) {
      BKE_image_partial_update_mark_full_update(image);
    }

    MEM_delete(data);
    data = next;
  }

  MEM_freeN(bkj);
}

static wmOperatorStatus multiresbake_image_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  if (!multiresbake_check(C, op)) {
    return OPERATOR_CANCELLED;
  }

  MultiresBakeJob *bkr = MEM_callocN<MultiresBakeJob>(__func__);
  init_multiresbake_job(C, bkr);

  if (!bkr->data.first) {
    BKE_report(op->reports, RPT_ERROR, "No objects found to bake from");
    MEM_freeN(bkr);
    return OPERATOR_CANCELLED;
  }

  /* setup job */
  wmJob *wm_job = WM_jobs_get(CTX_wm_manager(C),
                              CTX_wm_window(C),
                              scene,
                              "Baking Multires...",
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
static wmOperatorStatus objects_bake_render_modal(bContext *C,
                                                  wmOperator * /*op*/,
                                                  const wmEvent *event)
{
  /* no running blender, remove handler and pass through */
  if (0 == WM_jobs_test(CTX_wm_manager(C), CTX_data_scene(C), WM_JOB_TYPE_OBJECT_BAKE_TEXTURE)) {
    return OPERATOR_FINISHED | OPERATOR_PASS_THROUGH;
  }

  /* running render */
  switch (event->type) {
    case EVT_ESCKEY:
      return OPERATOR_RUNNING_MODAL;
    default: {
      break;
    }
  }
  return OPERATOR_PASS_THROUGH;
}

static bool is_multires_bake(Scene *scene)
{
  if (ELEM(scene->r.bake.type,
           R_BAKE_NORMALS,
           R_BAKE_DISPLACEMENT,
           R_BAKE_VECTOR_DISPLACEMENT,
           R_BAKE_AO))
  {
    return scene->r.bake.flag & R_BAKE_MULTIRES;
  }

  return false;
}

static wmOperatorStatus objects_bake_render_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent * /*event*/)
{
  Scene *scene = CTX_data_scene(C);
  wmOperatorStatus result = OPERATOR_CANCELLED;

  result = multiresbake_image_exec(C, op);

  WM_event_add_notifier(C, NC_SCENE | ND_RENDER_RESULT, scene);

  return result;
}

static wmOperatorStatus bake_image_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  wmOperatorStatus result = OPERATOR_CANCELLED;

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

  /* API callbacks. */
  ot->exec = bake_image_exec;
  ot->invoke = objects_bake_render_invoke;
  ot->modal = objects_bake_render_modal;
  ot->poll = ED_operator_object_active;
}

}  // namespace blender::ed::object

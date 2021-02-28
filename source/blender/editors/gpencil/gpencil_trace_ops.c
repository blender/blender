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
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup edgpencil
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"
#include "BKE_image.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf_types.h"

#include "ED_gpencil.h"
#include "ED_object.h"

#include "gpencil_intern.h"
#include "gpencil_trace.h"

typedef struct TraceJob {
  /* from wmJob */
  struct Object *owner;
  short *stop, *do_update;
  float *progress;

  bContext *C;
  wmWindowManager *wm;
  Main *bmain;
  Scene *scene;
  View3D *v3d;
  Base *base_active;
  Object *ob_active;
  Image *image;
  Object *ob_gpencil;
  bGPdata *gpd;
  bGPDlayer *gpl;

  bool was_ob_created;
  bool use_current_frame;

  int32_t frame_target;
  float threshold;
  float scale;
  float sample;
  int32_t resolution;
  int32_t thickness;
  int32_t turnpolicy;
  int32_t mode;

  bool success;
  bool was_canceled;
} TraceJob;

/**
 * Trace a image.
 * \param ibuf: Image buffer.
 * \param gpf: Destination frame.
 */
static bool gpencil_trace_image(TraceJob *trace_job, ImBuf *ibuf, bGPDframe *gpf)
{
  potrace_bitmap_t *bm = NULL;
  potrace_param_t *param = NULL;
  potrace_state_t *st = NULL;

  /* Create an empty BW bitmap. */
  bm = ED_gpencil_trace_bitmap_new(ibuf->x, ibuf->y);
  if (!bm) {
    return false;
  }

  /* Set tracing parameters, starting from defaults */
  param = potrace_param_default();
  if (!param) {
    return false;
  }
  param->turdsize = 0;
  param->turnpolicy = trace_job->turnpolicy;

  /* Load BW bitmap with image. */
  ED_gpencil_trace_image_to_bitmap(ibuf, bm, trace_job->threshold);

  /* Trace the bitmap. */
  st = potrace_trace(param, bm);
  if (!st || st->status != POTRACE_STATUS_OK) {
    ED_gpencil_trace_bitmap_free(bm);
    if (st) {
      potrace_state_free(st);
    }
    potrace_param_free(param);
    return false;
  }
  /* Free BW bitmap. */
  ED_gpencil_trace_bitmap_free(bm);

  /* Convert the trace to strokes. */
  int32_t offset[2];
  offset[0] = ibuf->x / 2;
  offset[1] = ibuf->y / 2;

  /* Scale correction for Potrace.
   * Really, there isn't documented in Potrace about how the scale is calculated,
   * but after doing a lot of tests, it looks is using a VGA resolution (640) as a base.
   * Maybe there are others ways to get the right scale conversion, but this solution works. */
  float scale_potrace = trace_job->scale * (640.0f / (float)ibuf->x) *
                        ((float)ibuf->x / (float)ibuf->y);
  if (ibuf->x > ibuf->y) {
    scale_potrace *= (float)ibuf->y / (float)ibuf->x;
  }

  ED_gpencil_trace_data_to_strokes(trace_job->bmain,
                                   st,
                                   trace_job->ob_gpencil,
                                   gpf,
                                   offset,
                                   scale_potrace,
                                   trace_job->sample,
                                   trace_job->resolution,
                                   trace_job->thickness);

  /* Free memory. */
  potrace_state_free(st);
  potrace_param_free(param);

  return true;
}

/* Trace Image to Grease Pencil. */
static bool gpencil_trace_image_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == NULL) || (ob->type != OB_EMPTY) || (ob->data == NULL)) {
    CTX_wm_operator_poll_msg_set(C, "No image empty selected");
    return false;
  }

  Image *image = (Image *)ob->data;
  if (!ELEM(image->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    CTX_wm_operator_poll_msg_set(C, "No valid image format selected");
    return false;
  }

  return true;
}

static void trace_initialize_job_data(TraceJob *trace_job)
{
  /* Create a new grease pencil object. */
  if (trace_job->ob_gpencil == NULL) {
    ushort local_view_bits = (trace_job->v3d && trace_job->v3d->localvd) ?
                                 trace_job->v3d->local_view_uuid :
                                 0;
    trace_job->ob_gpencil = ED_gpencil_add_object(
        trace_job->C, trace_job->ob_active->loc, local_view_bits);
    /* Apply image rotation. */
    copy_v3_v3(trace_job->ob_gpencil->rot, trace_job->ob_active->rot);
    /* Grease pencil is rotated 90 degrees in X axis by default. */
    trace_job->ob_gpencil->rot[0] -= DEG2RADF(90.0f);
    trace_job->was_ob_created = true;
    /* Apply image Scale. */
    copy_v3_v3(trace_job->ob_gpencil->scale, trace_job->ob_active->scale);
    /* The default display size of the image is 5.0 and this is used as scale = 1.0. */
    mul_v3_fl(trace_job->ob_gpencil->scale, trace_job->ob_active->empty_drawsize / 5.0f);
  }

  /* Create Layer. */
  trace_job->gpd = (bGPdata *)trace_job->ob_gpencil->data;
  trace_job->gpl = BKE_gpencil_layer_active_get(trace_job->gpd);
  if (trace_job->gpl == NULL) {
    trace_job->gpl = BKE_gpencil_layer_addnew(trace_job->gpd, DATA_("Trace"), true);
  }
}

static void trace_start_job(void *customdata, short *stop, short *do_update, float *progress)
{
  TraceJob *trace_job = customdata;

  trace_job->stop = stop;
  trace_job->do_update = do_update;
  trace_job->progress = progress;
  trace_job->was_canceled = false;
  const int init_frame = max_ii((trace_job->use_current_frame) ? trace_job->frame_target : 0, 0);

  G.is_break = false;

  /* Single Image. */
  if ((trace_job->image->source == IMA_SRC_FILE) ||
      (trace_job->mode == GPENCIL_TRACE_MODE_SINGLE)) {
    void *lock;
    ImageUser *iuser = trace_job->ob_active->iuser;
    iuser->framenr = init_frame;
    ImBuf *ibuf = BKE_image_acquire_ibuf(trace_job->image, iuser, &lock);
    if (ibuf) {
      /* Create frame. */
      bGPDframe *gpf = BKE_gpencil_layer_frame_get(
          trace_job->gpl, trace_job->frame_target, GP_GETFRAME_ADD_NEW);
      gpencil_trace_image(trace_job, ibuf, gpf);
      BKE_image_release_ibuf(trace_job->image, ibuf, lock);
      *(trace_job->progress) = 1.0f;
    }
  }
  /* Image sequence. */
  else if (trace_job->image->type == IMA_TYPE_IMAGE) {
    ImageUser *iuser = trace_job->ob_active->iuser;
    for (int i = init_frame; i < iuser->frames; i++) {
      if (G.is_break) {
        trace_job->was_canceled = true;
        break;
      }

      *(trace_job->progress) = (float)i / (float)iuser->frames;
      *do_update = true;

      iuser->framenr = i + 1;

      void *lock;
      ImBuf *ibuf = BKE_image_acquire_ibuf(trace_job->image, iuser, &lock);
      if (ibuf) {
        /* Create frame. */
        bGPDframe *gpf = BKE_gpencil_layer_frame_get(
            trace_job->gpl, trace_job->frame_target + i, GP_GETFRAME_ADD_NEW);
        gpencil_trace_image(trace_job, ibuf, gpf);

        BKE_image_release_ibuf(trace_job->image, ibuf, lock);
      }
    }
  }

  trace_job->success = !trace_job->was_canceled;
  *do_update = true;
  *stop = 0;
}

static void trace_end_job(void *customdata)
{
  TraceJob *trace_job = customdata;

  /* If canceled, delete all previously created object and data-block. */
  if ((trace_job->was_canceled) && (trace_job->was_ob_created) && (trace_job->ob_gpencil)) {
    bGPdata *gpd = trace_job->ob_gpencil->data;
    BKE_id_delete(trace_job->bmain, &trace_job->ob_gpencil->id);
    BKE_id_delete(trace_job->bmain, &gpd->id);
  }

  if (trace_job->success) {
    DEG_relations_tag_update(trace_job->bmain);

    DEG_id_tag_update(&trace_job->scene->id, ID_RECALC_SELECT);
    DEG_id_tag_update(&trace_job->gpd->id, ID_RECALC_GEOMETRY | ID_RECALC_COPY_ON_WRITE);

    WM_main_add_notifier(NC_OBJECT | NA_ADDED, NULL);
    WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, trace_job->scene);
  }
}

static void trace_free_job(void *customdata)
{
  TraceJob *tj = customdata;
  MEM_freeN(tj);
}

static int gpencil_trace_image_exec(bContext *C, wmOperator *op)
{
  TraceJob *job = MEM_mallocN(sizeof(TraceJob), "TraceJob");
  job->C = C;
  job->owner = CTX_data_active_object(C);
  job->wm = CTX_wm_manager(C);
  job->bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  job->scene = scene;
  job->v3d = CTX_wm_view3d(C);
  job->base_active = CTX_data_active_base(C);
  job->ob_active = job->base_active->object;
  job->image = (Image *)job->ob_active->data;
  job->frame_target = CFRA;
  job->use_current_frame = RNA_boolean_get(op->ptr, "use_current_frame");

  /* Create a new grease pencil object or reuse selected. */
  eGP_TargetObjectMode target = RNA_enum_get(op->ptr, "target");
  job->ob_gpencil = (target == GP_TARGET_OB_SELECTED) ? BKE_view_layer_non_active_selected_object(
                                                            CTX_data_view_layer(C), job->v3d) :
                                                        NULL;

  if (job->ob_gpencil != NULL) {
    if (job->ob_gpencil->type != OB_GPENCIL) {
      BKE_report(op->reports, RPT_WARNING, "Target object not a grease pencil, ignoring!");
      job->ob_gpencil = NULL;
    }
    else if (BKE_object_obdata_is_libdata(job->ob_gpencil)) {
      BKE_report(op->reports, RPT_WARNING, "Target object library-data, ignoring!");
      job->ob_gpencil = NULL;
    }
  }

  job->was_ob_created = false;

  job->threshold = RNA_float_get(op->ptr, "threshold");
  job->scale = RNA_float_get(op->ptr, "scale");
  job->sample = RNA_float_get(op->ptr, "sample");
  job->resolution = RNA_int_get(op->ptr, "resolution");
  job->thickness = RNA_int_get(op->ptr, "thickness");
  job->turnpolicy = RNA_enum_get(op->ptr, "turnpolicy");
  job->mode = RNA_enum_get(op->ptr, "mode");

  trace_initialize_job_data(job);

  /* Back to active base. */
  ED_object_base_activate(job->C, job->base_active);

  if (job->image->source == IMA_SRC_FILE) {
    short stop = 0, do_update = true;
    float progress;
    trace_start_job(job, &stop, &do_update, &progress);
    trace_end_job(job);
    trace_free_job(job);
  }
  else {
    wmJob *wm_job = WM_jobs_get(job->wm,
                                CTX_wm_window(C),
                                job->scene,
                                "Trace Image",
                                WM_JOB_PROGRESS,
                                WM_JOB_TYPE_TRACE_IMAGE);

    WM_jobs_customdata_set(wm_job, job, trace_free_job);
    WM_jobs_timer(wm_job, 0.1, NC_GEOM | ND_DATA, NC_GEOM | ND_DATA);
    WM_jobs_callbacks(wm_job, trace_start_job, NULL, NULL, trace_end_job);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  return OPERATOR_FINISHED;
}

static int gpencil_trace_image_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* Show popup dialog to allow editing. */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(C, op, 250);
}

void GPENCIL_OT_trace_image(wmOperatorType *ot)
{
  static const EnumPropertyItem turnpolicy_type[] = {
      {POTRACE_TURNPOLICY_BLACK,
       "BLACK",
       0,
       "Black",
       "Prefers to connect black (foreground) components"},
      {POTRACE_TURNPOLICY_WHITE,
       "WHITE",
       0,
       "White",
       "Prefers to connect white (background) components"},
      {POTRACE_TURNPOLICY_LEFT, "LEFT", 0, "Left", "Always take a left turn"},
      {POTRACE_TURNPOLICY_RIGHT, "RIGHT", 0, "Right", "Always take a right turn"},
      {POTRACE_TURNPOLICY_MINORITY,
       "MINORITY",
       0,
       "Minority",
       "Prefers to connect the color (black or white) that occurs least frequently in the local "
       "neighborhood of the current position"},
      {POTRACE_TURNPOLICY_MAJORITY,
       "MAJORITY",
       0,
       "Majority",
       "Prefers to connect the color (black or white) that occurs most frequently in the local "
       "neighborhood of the current position"},
      {POTRACE_TURNPOLICY_RANDOM, "RANDOM", 0, "Random", "Choose pseudo-randomly"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem trace_modes[] = {
      {GPENCIL_TRACE_MODE_SINGLE, "SINGLE", 0, "Single", "Trace the current frame of the image"},
      {GPENCIL_TRACE_MODE_SEQUENCE, "SEQUENCE", 0, "Sequence", "Trace full sequence"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem target_object_modes[] = {
      {GP_TARGET_OB_NEW, "NEW", 0, "New Object", ""},
      {GP_TARGET_OB_SELECTED, "SELECTED", 0, "Selected Object", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Trace Image to Grease Pencil";
  ot->idname = "GPENCIL_OT_trace_image";
  ot->description = "Extract Grease Pencil strokes from image";

  /* callbacks */
  ot->invoke = gpencil_trace_image_invoke;
  ot->exec = gpencil_trace_image_exec;
  ot->poll = gpencil_trace_image_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "target",
                          target_object_modes,
                          GP_TARGET_OB_NEW,
                          "Target Object",
                          "Target grease pencil");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  RNA_def_int(ot->srna, "thickness", 10, 1, 1000, "Thickness", "", 1, 1000);
  RNA_def_int(
      ot->srna, "resolution", 5, 1, 20, "Resolution", "Resolution of the generated curves", 1, 20);

  RNA_def_float(ot->srna,
                "scale",
                1.0f,
                0.001f,
                100.0f,
                "Scale",
                "Scale of the final stroke",
                0.001f,
                100.0f);
  RNA_def_float(ot->srna,
                "sample",
                0.0f,
                0.0f,
                100.0f,
                "Sample",
                "Distance to sample points, zero to disable",
                0.0f,
                100.0f);
  RNA_def_float_factor(ot->srna,
                       "threshold",
                       0.5f,
                       0.0f,
                       1.0f,
                       "Color Threshold",
                       "Determine the lightness threshold above which strokes are generated",
                       0.0f,
                       1.0f);
  RNA_def_enum(ot->srna,
               "turnpolicy",
               turnpolicy_type,
               POTRACE_TURNPOLICY_MINORITY,
               "Turn Policy",
               "Determines how to resolve ambiguities during decomposition of bitmaps into paths");
  RNA_def_enum(ot->srna,
               "mode",
               trace_modes,
               GPENCIL_TRACE_MODE_SINGLE,
               "Mode",
               "Determines if trace simple image or full sequence");
  RNA_def_boolean(ot->srna,
                  "use_current_frame",
                  true,
                  "Start At Current Frame",
                  "Trace Image starting in current image frame");
}

/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_global.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_image.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.h"
#include "BKE_object.hh"
#include "BKE_report.hh"

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "BLI_task.hh"
#include "BLT_translation.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "IMB_imbuf_types.hh"

#include "ED_grease_pencil.hh"
#include "ED_numinput.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "grease_pencil_trace_util.hh"

namespace blender::ed::sculpt_paint::greasepencil {

/* -------------------------------------------------------------------- */
/** \name Trace Image Operator
 * \{ */

using image_trace::TurnPolicy;

/* Target object modes. */
enum class TargetObjectMode : int8_t {
  New = 0,
  Selected = 1,
};

enum class TraceMode : int8_t {
  Single = 0,
  Sequence = 1,
};

#ifdef WITH_POTRACE

struct TraceJob {
  /* from wmJob */
  Object *owner;
  bool *stop, *do_update;
  float *progress;

  bContext *C;
  wmWindowManager *wm;
  Main *bmain;
  Scene *scene;
  View3D *v3d;
  Base *base_active;
  Object *ob_active;
  Image *image;
  Object *ob_grease_pencil;
  bke::greasepencil::Layer *layer;

  Array<bke::CurvesGeometry> traced_curves;

  bool was_ob_created;
  bool use_current_frame;

  /* Frame number where the output frame is generated. */
  int frame_target;
  float threshold;
  float radius;
  TurnPolicy turnpolicy;
  TraceMode mode;
  /* Custom source frame, allows overriding the default scene frame. */
  int frame_number;

  bool success;
  bool was_canceled;

  void ensure_output_object();
};

void TraceJob::ensure_output_object()
{
  using namespace blender::bke::greasepencil;

  /* Create a new grease pencil object. */
  if (this->ob_grease_pencil == nullptr) {
    const ushort local_view_bits = (this->v3d && this->v3d->localvd) ? this->v3d->local_view_uid :
                                                                       0;

    /* Copy transform from the active object. */
    this->ob_grease_pencil = ed::object::add_type(this->C,
                                                  OB_GREASE_PENCIL,
                                                  nullptr,
                                                  this->ob_active->loc,
                                                  this->ob_active->rot,
                                                  false,
                                                  local_view_bits);
    copy_v3_v3(this->ob_grease_pencil->scale, this->ob_active->scale);
    this->was_ob_created = true;
  }

  /* Create Layer. */
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(this->ob_grease_pencil->data);
  this->layer = grease_pencil.get_active_layer();
  if (this->layer == nullptr) {
    Layer &new_layer = grease_pencil.add_layer(DATA_("Trace"));
    grease_pencil.set_active_layer(&new_layer);
    this->layer = &new_layer;
  }
}

static float4x4 pixel_to_object_transform(const Object &image_object,
                                          const ImBuf &ibuf,
                                          const float2 pixel_center = float2(0.5f))
{
  const float3 pixel_center_3d = float3(pixel_center.x, pixel_center.y, 0);
  const float3 pixel_size_3d = math::safe_rcp(float3(ibuf.x, ibuf.y, 0));
  const float3 image_offset_3d = float3(image_object.ima_ofs[0], image_object.ima_ofs[1], 0);
  const float max_image_scale = image_object.empty_drawsize;
  const float3 image_aspect_3d = (ibuf.x > ibuf.y ? float3(1, float(ibuf.y) / float(ibuf.x), 1) :
                                                    float3(float(ibuf.x) / float(ibuf.y), 1, 1));

  const float4x4 to_object = math::translate(
      math::from_scale<float4x4>(image_aspect_3d * max_image_scale), image_offset_3d);
  const float4x4 to_normalized = math::translate(math::scale(to_object, pixel_size_3d),
                                                 pixel_center_3d);
  return to_normalized;
}

static int ensure_foreground_material(Main *bmain, Object *ob, const StringRefNull name)
{
  int index = BKE_grease_pencil_object_material_index_get_by_name(ob, name.c_str());
  if (index == -1) {
    Material &ma = *BKE_grease_pencil_object_material_new(bmain, ob, name.c_str(), &index);
    copy_v4_v4(ma.gp_style->stroke_rgba, float4(0, 0, 0, 1));
    ma.gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    ma.gp_style->flag |= GP_MATERIAL_FILL_SHOW;
  }
  return index;
}

static int ensure_background_material(Main *bmain, Object *ob, const StringRefNull name)
{
  int index = BKE_grease_pencil_object_material_index_get_by_name(ob, name.c_str());
  if (index == -1) {
    Material &ma = *BKE_grease_pencil_object_material_new(bmain, ob, name.c_str(), &index);
    copy_v4_v4(ma.gp_style->stroke_rgba, float4(0, 0, 0, 1));
    copy_v4_v4(ma.gp_style->fill_rgba, float4(0, 0, 0, 1));
    ma.gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
    ma.gp_style->flag |= GP_MATERIAL_FILL_SHOW;
    ma.gp_style->flag |= GP_MATERIAL_IS_STROKE_HOLDOUT;
    ma.gp_style->flag |= GP_MATERIAL_IS_FILL_HOLDOUT;
  }
  return index;
}

static bke::CurvesGeometry grease_pencil_trace_image(TraceJob &trace_job, const ImBuf &ibuf)
{
  /* Trace the image. */
  potrace_bitmap_t *bm = image_trace::image_to_bitmap(ibuf, [&](const ColorGeometry4f &color) {
    return math::average(float3(color.r, color.g, color.b)) * color.a > trace_job.threshold;
  });

  image_trace::TraceParams params;
  params.size_threshold = 0;
  params.turn_policy = trace_job.turnpolicy;
  image_trace::Trace *trace = image_trace::trace_bitmap(params, *bm);
  image_trace::free_bitmap(bm);

  /* Attribute ID for which curves are "holes" with a negative trace sign. */
  const StringRef hole_attribute_id = "is_hole";

  /* Transform from bitmap index space to local image object space. */
  const float4x4 transform = pixel_to_object_transform(*trace_job.ob_active, ibuf);
  bke::CurvesGeometry trace_curves = image_trace::trace_to_curves(
      *trace, hole_attribute_id, transform);
  image_trace::free_trace(trace);

  /* Assign different materials to foreground curves and hole curves. */
  bke::MutableAttributeAccessor attributes = trace_curves.attributes_for_write();
  const int material_fg = ensure_foreground_material(
      trace_job.bmain, trace_job.ob_grease_pencil, "Stroke");
  const int material_bg = ensure_background_material(
      trace_job.bmain, trace_job.ob_grease_pencil, "Holdout");
  const VArraySpan<bool> holes = *attributes.lookup<bool>(hole_attribute_id);
  bke::SpanAttributeWriter<int> material_indices = attributes.lookup_or_add_for_write_span<int>(
      "material_index", bke::AttrDomain::Curve);
  threading::parallel_for(trace_curves.curves_range(), 4096, [&](const IndexRange range) {
    for (const int curve_i : range) {
      const bool is_hole = holes[curve_i];
      material_indices.span[curve_i] = (is_hole ? material_bg : material_fg);
    }
  });
  material_indices.finish();
  /* Remove hole attribute */
  attributes.remove(hole_attribute_id);

  /* Uniform radius for all trace curves. */
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", bke::AttrDomain::Point);
  radii.span.fill(trace_job.radius);
  radii.finish();

  return trace_curves;
}

static void trace_start_job(void *customdata, wmJobWorkerStatus *worker_status)
{
  TraceJob &trace_job = *static_cast<TraceJob *>(customdata);

  trace_job.stop = &worker_status->stop;
  trace_job.do_update = &worker_status->do_update;
  trace_job.progress = &worker_status->progress;
  trace_job.was_canceled = false;
  const int init_frame = std::max((trace_job.use_current_frame) ? trace_job.frame_target : 0, 0);

  G.is_break = false;

  /* Single Image. */
  if (trace_job.image->source == IMA_SRC_FILE || trace_job.mode == TraceMode::Single) {
    ImageUser &iuser = *trace_job.ob_active->iuser;
    trace_job.traced_curves.reinitialize(1);

    iuser.framenr = ((trace_job.frame_number == 0) || (trace_job.frame_number > iuser.frames)) ?
                        init_frame :
                        trace_job.frame_number;
    void *lock;
    ImBuf *ibuf = BKE_image_acquire_ibuf(trace_job.image, &iuser, &lock);
    if (ibuf) {
      trace_job.traced_curves.first() = grease_pencil_trace_image(trace_job, *ibuf);
      BKE_image_release_ibuf(trace_job.image, ibuf, lock);
      *(trace_job.progress) = 1.0f;
    }
  }
  /* Image sequence. */
  else if (trace_job.image->type == IMA_TYPE_IMAGE) {
    ImageUser &iuser = *trace_job.ob_active->iuser;
    const int num_frames = iuser.frames - init_frame + 1;
    trace_job.traced_curves.reinitialize(num_frames);
    for (const int i : IndexRange(num_frames)) {
      if (G.is_break) {
        trace_job.was_canceled = true;
        break;
      }

      const int frame_number = init_frame + i;
      *(trace_job.progress) = float(frame_number) / float(iuser.frames);
      worker_status->do_update = true;

      iuser.framenr = frame_number;

      void *lock;
      ImBuf *ibuf = BKE_image_acquire_ibuf(trace_job.image, &iuser, &lock);
      if (ibuf) {
        trace_job.traced_curves[i] = grease_pencil_trace_image(trace_job, *ibuf);
        BKE_image_release_ibuf(trace_job.image, ibuf, lock);
      }
    }
  }

  trace_job.success = !trace_job.was_canceled;
  worker_status->do_update = true;
  worker_status->stop = false;
}

static void trace_end_job(void *customdata)
{
  TraceJob &trace_job = *static_cast<TraceJob *>(customdata);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(trace_job.ob_grease_pencil->data);

  /* Update all the drawings once the job is done and we're executing in the main thread again.
   * Changing drawing array or updating the drawing geometry is not thread-safe. */
  switch (trace_job.mode) {
    case TraceMode::Single: {
      BLI_assert(trace_job.traced_curves.size() == 1);
      bke::greasepencil::Drawing *drawing = grease_pencil.get_drawing_at(*trace_job.layer,
                                                                         trace_job.frame_target);
      if (drawing == nullptr) {
        drawing = grease_pencil.insert_frame(*trace_job.layer, trace_job.frame_target);
      }
      BLI_assert(drawing != nullptr);
      drawing->strokes_for_write() = trace_job.traced_curves.first();
      drawing->tag_topology_changed();
      break;
    }
    case TraceMode::Sequence: {
      const ImageUser &iuser = *trace_job.ob_active->iuser;
      const int init_frame = std::max((trace_job.use_current_frame) ? trace_job.frame_target : 0,
                                      0);
      const int num_frames = iuser.frames - init_frame + 1;
      BLI_assert(trace_job.traced_curves.size() == num_frames);
      for (const int i : IndexRange(num_frames)) {
        const int frame_number = init_frame + i;
        bke::greasepencil::Drawing *drawing = grease_pencil.get_drawing_at(*trace_job.layer,
                                                                           frame_number);
        if (drawing == nullptr) {
          drawing = grease_pencil.insert_frame(*trace_job.layer, frame_number);
        }
        BLI_assert(drawing != nullptr);
        drawing->strokes_for_write() = trace_job.traced_curves[i];
        drawing->tag_topology_changed();
      }
      break;
    }
  }

  /* If canceled, delete all previously created object and data-block. */
  if ((trace_job.was_canceled) && (trace_job.was_ob_created) && (trace_job.ob_grease_pencil)) {
    BKE_id_delete(trace_job.bmain, &trace_job.ob_grease_pencil->id);
    BKE_id_delete(trace_job.bmain, &grease_pencil.id);
  }

  if (trace_job.success) {
    DEG_relations_tag_update(trace_job.bmain);

    DEG_id_tag_update(&trace_job.scene->id, ID_RECALC_SELECT);
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);

    WM_main_add_notifier(NC_OBJECT | NA_ADDED, nullptr);
    WM_main_add_notifier(NC_SCENE | ND_OB_ACTIVE, trace_job.scene);
  }
}

static void trace_free_job(void *customdata)
{
  TraceJob *tj = static_cast<TraceJob *>(customdata);
  MEM_delete(tj);
}

/* Trace Image to Grease Pencil. */
static bool grease_pencil_trace_image_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_EMPTY) || (ob->data == nullptr)) {
    CTX_wm_operator_poll_msg_set(C, "No image empty selected");
    return false;
  }

  Image *image = static_cast<Image *>(ob->data);
  if (!ELEM(image->source, IMA_SRC_FILE, IMA_SRC_SEQUENCE, IMA_SRC_MOVIE)) {
    CTX_wm_operator_poll_msg_set(C, "No valid image format selected");
    return false;
  }

  return true;
}

static int grease_pencil_trace_image_exec(bContext *C, wmOperator *op)
{
  TraceJob *job = MEM_new<TraceJob>("TraceJob");
  job->C = C;
  job->owner = CTX_data_active_object(C);
  job->wm = CTX_wm_manager(C);
  job->bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  job->scene = scene;
  job->v3d = CTX_wm_view3d(C);
  job->base_active = CTX_data_active_base(C);
  job->ob_active = job->base_active->object;
  job->image = static_cast<Image *>(job->ob_active->data);
  job->frame_target = scene->r.cfra;
  job->use_current_frame = RNA_boolean_get(op->ptr, "use_current_frame");

  /* Create a new grease pencil object or reuse selected. */
  const TargetObjectMode target = TargetObjectMode(RNA_enum_get(op->ptr, "target"));
  job->ob_grease_pencil = (target == TargetObjectMode::Selected) ?
                              BKE_view_layer_non_active_selected_object(
                                  scene, CTX_data_view_layer(C), job->v3d) :
                              nullptr;

  if (job->ob_grease_pencil != nullptr) {
    if (job->ob_grease_pencil->type != OB_GREASE_PENCIL) {
      BKE_report(op->reports, RPT_WARNING, "Target object not a Grease Pencil, ignoring!");
      job->ob_grease_pencil = nullptr;
    }
    else if (BKE_object_obdata_is_libdata(job->ob_grease_pencil)) {
      BKE_report(op->reports, RPT_WARNING, "Target object library-data, ignoring!");
      job->ob_grease_pencil = nullptr;
    }
  }

  job->was_ob_created = false;

  job->threshold = RNA_float_get(op->ptr, "threshold");
  job->radius = RNA_float_get(op->ptr, "radius");
  job->turnpolicy = TurnPolicy(RNA_enum_get(op->ptr, "turnpolicy"));
  job->mode = TraceMode(RNA_enum_get(op->ptr, "mode"));
  job->frame_number = RNA_int_get(op->ptr, "frame_number");

  job->ensure_output_object();

  /* Back to active base. */
  blender::ed::object::base_activate(job->C, job->base_active);

  if ((job->image->source == IMA_SRC_FILE) || (job->frame_number > 0)) {
    wmJobWorkerStatus worker_status = {};
    trace_start_job(job, &worker_status);
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
    WM_jobs_callbacks(wm_job, trace_start_job, nullptr, nullptr, trace_end_job);

    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  return OPERATOR_FINISHED;
}

static int grease_pencil_trace_image_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* Show popup dialog to allow editing. */
  /* FIXME: hard-coded dimensions here are just arbitrary. */
  return WM_operator_props_dialog_popup(C, op, 250);
}

static void GREASE_PENCIL_OT_trace_image(wmOperatorType *ot)
{
  PropertyRNA *prop;

  static const EnumPropertyItem turnpolicy_type[] = {
      {int(TurnPolicy::Foreground),
       "FOREGROUND",
       0,
       "Foreground",
       "Prefers to connect foreground components"},
      {int(TurnPolicy::Background),
       "BACKGROUND",
       0,
       "Background",
       "Prefers to connect background components"},
      {int(TurnPolicy::Left), "LEFT", 0, "Left", "Always take a left turn"},
      {int(TurnPolicy::Right), "RIGHT", 0, "Right", "Always take a right turn"},
      {int(TurnPolicy::Minority),
       "MINORITY",
       0,
       "Minority",
       "Prefers to connect the color that occurs least frequently in the local "
       "neighborhood of the current position"},
      {int(TurnPolicy::Majority),
       "MAJORITY",
       0,
       "Majority",
       "Prefers to connect the color that occurs most frequently in the local "
       "neighborhood of the current position"},
      {int(TurnPolicy::Random), "RANDOM", 0, "Random", "Choose pseudo-randomly"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem trace_modes[] = {
      {int(TraceMode::Single), "SINGLE", 0, "Single", "Trace the current frame of the image"},
      {int(TraceMode::Sequence), "SEQUENCE", 0, "Sequence", "Trace full sequence"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem target_object_modes[] = {
      {int(TargetObjectMode::New), "NEW", 0, "New Object", ""},
      {int(TargetObjectMode::Selected), "SELECTED", 0, "Selected Object", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Trace Image to Grease Pencil";
  ot->idname = "GREASE_PENCIL_OT_trace_image";
  ot->description = "Extract Grease Pencil strokes from image";

  /* callbacks */
  ot->invoke = grease_pencil_trace_image_invoke;
  ot->exec = grease_pencil_trace_image_exec;
  ot->poll = grease_pencil_trace_image_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna,
                          "target",
                          target_object_modes,
                          int(TargetObjectMode::New),
                          "Target Object",
                          "Target Grease Pencil");
  RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

  RNA_def_float(ot->srna, "radius", 0.01f, 0.001f, 1.0f, "Radius", "", 0.001, 1.0f);

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
               int(TurnPolicy::Minority),
               "Turn Policy",
               "Determines how to resolve ambiguities during decomposition of bitmaps into paths");
  RNA_def_enum(ot->srna,
               "mode",
               trace_modes,
               int(TraceMode::Single),
               "Mode",
               "Determines if trace simple image or full sequence");
  RNA_def_boolean(ot->srna,
                  "use_current_frame",
                  true,
                  "Start At Current Frame",
                  "Trace Image starting in current image frame");
  prop = RNA_def_int(
      ot->srna,
      "frame_number",
      0,
      0,
      9999,
      "Trace Frame",
      "Used to trace only one frame of the image sequence, set to zero to trace all",
      0,
      9999);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

#endif

/** \} */

}  // namespace blender::ed::sculpt_paint::greasepencil

/* -------------------------------------------------------------------- */
/** \name Registration
 * \{ */

void ED_operatortypes_grease_pencil_trace()
{
  using namespace blender::ed::sculpt_paint::greasepencil;

#ifdef WITH_POTRACE
  WM_operatortype_append(GREASE_PENCIL_OT_trace_image);
#endif
}

/** \} */

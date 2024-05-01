/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgrease_pencil
 */

#include "BLI_string.h"
#include "BLI_task.hh"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_undo_system.hh"

#include "CLG_log.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_grease_pencil.hh"
#include "ED_undo.hh"

#include "MEM_guardedalloc.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include <iostream>
#include <ostream>

static CLG_LogRef LOG = {"ed.undo.greasepencil"};

namespace blender::ed::greasepencil::undo {

/* -------------------------------------------------------------------- */
/** \name Implements ED Undo System
 *
 * \note This is similar for all edit-mode types.
 * \{ */

/**
 * Store all drawings, layers and layers data, in each undo step.
 *
 * Each drawing type has its own array in the undo #StepObject data.
 *
 * NOTE: Storing Reference drawings is also needed, since drawings can be added or removed, data
 * from Reference ones also needs to be stored.
 */

/* Store contextual data and status info during undo step encoding or decoding. */
struct StepEncodeStatus {};

struct StepDecodeStatus {
  /**
   * In case some reference drawing needs to be re-created, the GreasePencil ID gets a new
   * relation to another GreasePencil ID.
   */
  bool needs_relationships_update = false;
};

class StepDrawingGeometryBase {
 protected:
  /* Index of this drawing in the original combined array of all drawings in GreasePencil ID. */
  int index_;

  /* Data from #GreasePencilDrawingBase that needs to be saved in undo steps. */
  uint32_t flag_;

 protected:
  /**
   * Ensures that the drawing from the given array at the current index exists,
   * and has the proposer type.
   *
   * Non-existing drawings can happen after extending the drawings array.
   *
   * Mismatch in drawing types can happen when some drawings have been deleted between the undo
   * step storage, and the current state of the GreasePencil data.
   */
  void decode_valid_drawingtype_at_index_ensure(MutableSpan<GreasePencilDrawingBase *> drawings,
                                                const GreasePencilDrawingType drawing_type) const
  {
    /* TODO: Maybe that code should rather be part of GreasePencil:: API, together with
     * `add_empty_drawings` and such? */
    GreasePencilDrawingBase *drawing = drawings[index_];
    if (drawing != nullptr) {
      if (drawing->type == drawing_type) {
        return;
      }
      switch (drawing->type) {
        case GP_DRAWING:
          MEM_delete(&reinterpret_cast<GreasePencilDrawing *>(drawing)->wrap());
          break;
        case GP_DRAWING_REFERENCE:
          MEM_delete(&reinterpret_cast<GreasePencilDrawingReference *>(drawing)->wrap());
          break;
      }
      drawing = nullptr;
    }
    if (drawing == nullptr) {
      switch (drawing_type) {
        case GP_DRAWING:
          drawings[index_] = reinterpret_cast<GreasePencilDrawingBase *>(
              MEM_new<bke::greasepencil::Drawing>(__func__));
          break;
        case GP_DRAWING_REFERENCE:
          drawings[index_] = reinterpret_cast<GreasePencilDrawingBase *>(
              MEM_new<bke::greasepencil::DrawingReference>(__func__));
          break;
      }
    }
  }
};

class StepDrawingGeometry : public StepDrawingGeometryBase {
  bke::CurvesGeometry geometry_;

 public:
  void encode(const GreasePencilDrawing &drawing_geometry,
              const int64_t drawing_index,
              StepEncodeStatus & /* encode_status */)
  {
    BLI_assert(drawing_index >= 0 && drawing_index < INT32_MAX);
    index_ = int(drawing_index);

    flag_ = drawing_geometry.base.flag;
    geometry_ = drawing_geometry.geometry.wrap();
  }

  void decode(GreasePencil &grease_pencil, StepDecodeStatus & /*decode_status*/) const
  {
    MutableSpan<GreasePencilDrawingBase *> drawings = grease_pencil.drawings();
    this->decode_valid_drawingtype_at_index_ensure(drawings, GP_DRAWING);
    BLI_assert(drawings[index_]->type == GP_DRAWING);

    GreasePencilDrawing &drawing_geometry = *reinterpret_cast<GreasePencilDrawing *>(
        drawings[index_]);

    drawing_geometry.base.flag = flag_;
    drawing_geometry.geometry.wrap() = geometry_;

    /* TODO: Check if there is a way to tell if both stored and current geometry are still the
     * same, to avoid recomputing the cache all the time for all drawings? */
    drawing_geometry.runtime->triangles_cache.tag_dirty();
  }
};

class StepDrawingReference : public StepDrawingGeometryBase {
  UndoRefID_GreasePencil grease_pencil_ref_ = {};

 public:
  void encode(const GreasePencilDrawingReference &drawing_reference,
              const int64_t drawing_index,
              StepEncodeStatus & /* encode_status */)
  {
    BLI_assert(drawing_index >= 0 && drawing_index < INT32_MAX);
    index_ = int(drawing_index);

    flag_ = drawing_reference.base.flag;
    grease_pencil_ref_.ptr = drawing_reference.id_reference;
  }

  void decode(GreasePencil &grease_pencil, StepDecodeStatus &decode_status) const
  {
    MutableSpan<GreasePencilDrawingBase *> drawings = grease_pencil.drawings();
    this->decode_valid_drawingtype_at_index_ensure(drawings, GP_DRAWING_REFERENCE);
    BLI_assert(drawings[index_]->type == GP_DRAWING_REFERENCE);

    GreasePencilDrawingReference &drawing_reference =
        *reinterpret_cast<GreasePencilDrawingReference *>(drawings[index_]);
    drawing_reference.base.flag = flag_;

    if (drawing_reference.id_reference != grease_pencil_ref_.ptr) {
      id_us_min(reinterpret_cast<ID *>(drawing_reference.id_reference));
      drawing_reference.id_reference = grease_pencil_ref_.ptr;
      id_us_plus(reinterpret_cast<ID *>(drawing_reference.id_reference));
      decode_status.needs_relationships_update = true;
    }
  }

  void foreach_id_ref(UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
  {
    foreach_ID_ref_fn(user_data, reinterpret_cast<UndoRefID *>(&grease_pencil_ref_));
  }
};

class StepObject {
 public:
  UndoRefID_Object obedit_ref = {};

 private:
  Array<StepDrawingGeometry> drawings_geometry_;
  Array<StepDrawingReference> drawings_reference_;

  int layers_num_ = 0;
  bke::greasepencil::LayerGroup root_group_;
  std::string active_layer_name_;
  CustomData layers_data_ = {};

 private:
  void encode_drawings(const GreasePencil &grease_pencil, StepEncodeStatus &encode_status)
  {
    const Span<const GreasePencilDrawingBase *> drawings = grease_pencil.drawings();

    int64_t drawings_geometry_num = 0;
    int64_t drawings_reference_num = 0;
    for (const int64_t idx : drawings.index_range()) {
      const GreasePencilDrawingBase &drawing = *drawings[idx];
      switch (drawing.type) {
        case GP_DRAWING:
          drawings_geometry_num++;
          break;
        case GP_DRAWING_REFERENCE:
          drawings_reference_num++;
          break;
      }
    }

    drawings_geometry_.reinitialize(drawings_geometry_num);
    drawings_reference_.reinitialize(drawings_reference_num);

    int drawings_geometry_idx = 0;
    int drawings_reference_idx = 0;
    for (const int64_t idx : drawings.index_range()) {
      const GreasePencilDrawingBase &drawing = *drawings[idx];
      switch (drawing.type) {
        case GP_DRAWING:
          drawings_geometry_[drawings_geometry_idx++].encode(
              reinterpret_cast<const GreasePencilDrawing &>(drawing), idx, encode_status);
          break;
        case GP_DRAWING_REFERENCE:
          drawings_reference_[drawings_reference_idx++].encode(
              reinterpret_cast<const GreasePencilDrawingReference &>(drawing), idx, encode_status);
          break;
      }
    }
  }

  void decode_drawings(GreasePencil &grease_pencil, StepDecodeStatus &decode_status) const
  {
    const int drawing_array_num = int(drawings_geometry_.size() + drawings_reference_.size());
    grease_pencil.resize_drawings(drawing_array_num);

    for (const StepDrawingGeometry &drawing : drawings_geometry_) {
      drawing.decode(grease_pencil, decode_status);
    }
    for (const StepDrawingReference &drawing : drawings_reference_) {
      drawing.decode(grease_pencil, decode_status);
    }
  }

  void encode_layers(const GreasePencil &grease_pencil, StepEncodeStatus & /*encode_status*/)
  {
    layers_num_ = int(grease_pencil.layers().size());

    CustomData_copy(
        &grease_pencil.layers_data, &layers_data_, eCustomDataMask(CD_MASK_ALL), layers_num_);

    if (grease_pencil.has_active_layer()) {
      active_layer_name_ = grease_pencil.get_active_layer()->name();
    }

    root_group_ = grease_pencil.root_group();
  }

  void decode_layers(GreasePencil &grease_pencil, StepDecodeStatus & /*decode_status*/) const
  {
    if (grease_pencil.root_group_ptr) {
      MEM_delete(&grease_pencil.root_group());
    }

    grease_pencil.root_group_ptr = MEM_new<bke::greasepencil::LayerGroup>(__func__, root_group_);
    BLI_assert(layers_num_ == grease_pencil.layers().size());

    if (!active_layer_name_.empty()) {
      const bke::greasepencil::TreeNode *active_node =
          grease_pencil.root_group().find_node_by_name(active_layer_name_);
      if (active_node && active_node->is_layer()) {
        grease_pencil.set_active_layer(&active_node->as_layer());
      }
    }

    CustomData_copy(
        &layers_data_, &grease_pencil.layers_data, eCustomDataMask(CD_MASK_ALL), layers_num_);
  }

 public:
  ~StepObject()
  {
    CustomData_free(&layers_data_, layers_num_);
  }

  void encode(Object *ob, StepEncodeStatus &encode_status)
  {
    const GreasePencil &grease_pencil = *static_cast<GreasePencil *>(ob->data);
    this->obedit_ref.ptr = ob;

    this->encode_drawings(grease_pencil, encode_status);
    this->encode_layers(grease_pencil, encode_status);
  }

  void decode(StepDecodeStatus &decode_status) const
  {
    GreasePencil &grease_pencil = *static_cast<GreasePencil *>(this->obedit_ref.ptr->data);

    this->decode_drawings(grease_pencil, decode_status);
    this->decode_layers(grease_pencil, decode_status);

    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  }

  void foreach_id_ref(UndoTypeForEachIDRefFn foreach_ID_ref_fn, void *user_data)
  {
    foreach_ID_ref_fn(user_data, reinterpret_cast<UndoRefID *>(&this->obedit_ref));
    for (StepDrawingReference &drawing_ref : drawings_reference_) {
      drawing_ref.foreach_id_ref(foreach_ID_ref_fn, user_data);
    }
  }
};

struct GreasePencilUndoStep {
  UndoStep step;
  /** See #ED_undo_object_editmode_validate_scene_from_windows code comment for details. */
  UndoRefID_Scene scene_ref = {};
  Array<StepObject> objects;
};

static bool step_encode(bContext *C, Main *bmain, UndoStep *us_p)
{
  GreasePencilUndoStep *us = reinterpret_cast<GreasePencilUndoStep *>(us_p);
  StepEncodeStatus encode_status;

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Vector<Object *> objects = ED_undo_editmode_objects_from_view_layer(scene, view_layer);

  us->scene_ref.ptr = scene;
  new (&us->objects) Array<StepObject>(objects.size());

  threading::parallel_for(us->objects.index_range(), 8, [&](const IndexRange range) {
    for (const int64_t i : range) {
      Object *ob = objects[i];
      us->objects[i].encode(ob, encode_status);
    }
  });

  bmain->is_memfile_undo_flush_needed = true;

  return true;
}

static void step_decode(
    bContext *C, Main *bmain, UndoStep *us_p, const eUndoStepDir /*dir*/, bool /*is_final*/)
{
  GreasePencilUndoStep *us = reinterpret_cast<GreasePencilUndoStep *>(us_p);
  StepDecodeStatus decode_status;

  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ED_undo_object_editmode_validate_scene_from_windows(
      CTX_wm_manager(C), us->scene_ref.ptr, &scene, &view_layer);
  ED_undo_object_editmode_restore_helper(scene,
                                         view_layer,
                                         &us->objects.first().obedit_ref.ptr,
                                         uint(us->objects.size()),
                                         sizeof(decltype(us->objects)::value_type));

  BLI_assert(BKE_object_is_in_editmode(us->objects.first().obedit_ref.ptr));

  for (const StepObject &step_object : us->objects) {
    step_object.decode(decode_status);
  }

  if (decode_status.needs_relationships_update) {
    DEG_relations_tag_update(bmain);
  }

  ED_undo_object_set_active_or_warn(
      scene, view_layer, us->objects.first().obedit_ref.ptr, us_p->name, &LOG);

  bmain->is_memfile_undo_flush_needed = true;

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
}

static void step_free(UndoStep *us_p)
{
  GreasePencilUndoStep *us = reinterpret_cast<GreasePencilUndoStep *>(us_p);
  us->objects.~Array();
}

static void foreach_ID_ref(UndoStep *us_p,
                           UndoTypeForEachIDRefFn foreach_ID_ref_fn,
                           void *user_data)
{
  GreasePencilUndoStep *us = reinterpret_cast<GreasePencilUndoStep *>(us_p);

  foreach_ID_ref_fn(user_data, reinterpret_cast<UndoRefID *>(&us->scene_ref));
  for (StepObject &object : us->objects) {
    object.foreach_id_ref(foreach_ID_ref_fn, user_data);
  }
}

/** \} */

}  // namespace blender::ed::greasepencil::undo

void ED_undosys_type_grease_pencil(UndoType *ut)
{
  using namespace blender::ed;

  ut->name = "Edit GreasePencil";
  ut->poll = greasepencil::editable_grease_pencil_poll;
  ut->step_encode = greasepencil::undo::step_encode;
  ut->step_decode = greasepencil::undo::step_decode;
  ut->step_free = greasepencil::undo::step_free;

  ut->step_foreach_ID_ref = greasepencil::undo::foreach_ID_ref;

  ut->flags = UNDOTYPE_FLAG_NEED_CONTEXT_FOR_ENCODE;

  ut->step_size = sizeof(greasepencil::undo::GreasePencilUndoStep);
}

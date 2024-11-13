/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 */

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "BLI_enumerable_thread_specific.hh"
#include "BLI_offset_indices.hh"
#include "BLI_task.hh"

#include "DNA_object_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"
#include "ED_grease_pencil.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"

namespace blender::ed::greasepencil {

/* -------------------------------------------------------------------- */
/** \name Selection Utility Functions
 * \{ */

inline int clamp_range(const IndexRange range, const int index)
{
  BLI_assert(!range.is_empty());
  return std::clamp(index, int(range.first()), int(range.last()));
}

/**
 * Callback for each segment. Each segment can have two point ranges, one of them may be empty.
 * Returns the total number of segments, or zero if the curve is cyclic and can be regarded as a
 * single contiguous range.
 *
 * void fn(int segment_index, IndexRange point_range1, IndexRange point_range2);
 */
template<typename Fn>
static int foreach_curve_segment(const CurveSegmentsData &segment_data,
                                 const int curve_index,
                                 const IndexRange points,
                                 Fn &&fn)
{
  if (points.is_empty()) {
    return 0;
  }

  const OffsetIndices segments_by_curve = OffsetIndices<int>(segment_data.segment_offsets);
  const IndexRange segments = segments_by_curve[curve_index];

  for (const int segment_i : segments) {
    const int segment_point_i = segment_data.segment_start_points[segment_i];
    const float segment_fraction = segment_data.segment_start_fractions[segment_i];

    if (segment_i < segments.last()) {
      const int next_segment_i = segment_i + 1;
      const int next_segment_point_i = segment_data.segment_start_points[next_segment_i];
      const float next_segment_fraction = segment_data.segment_start_fractions[next_segment_i];

      /* Start point with zero fraction is included. */
      const int first_point_i = (segment_fraction == 0.0f ?
                                     segment_point_i :
                                     clamp_range(points, segment_point_i + 1));
      const int next_first_point_i = (next_segment_fraction == 0.0f ?
                                          next_segment_point_i :
                                          clamp_range(points, next_segment_point_i + 1));
      const IndexRange points_range = IndexRange::from_begin_end(first_point_i,
                                                                 next_first_point_i);
      fn(segment_i, points_range, IndexRange());
    }
    else {
      const int first_segment_point_i = segment_data.segment_start_points[segments.first()];
      const float first_segment_fraction = segment_data.segment_start_fractions[segments.first()];
      /* Start point with zero fraction is included. */
      const int first_point_i = (segment_fraction == 0.0f ?
                                     segment_point_i :
                                     clamp_range(points, segment_point_i + 1));
      /* End point with zero fraction is excluded. */
      const int next_first_point_i = (first_segment_fraction == 0.0f ?
                                          first_segment_point_i :
                                          clamp_range(points, first_segment_point_i + 1));
      const IndexRange points_range1 = IndexRange::from_begin_end(points.first(),
                                                                  next_first_point_i);
      const IndexRange points_range2 = IndexRange::from_begin_end_inclusive(first_point_i,
                                                                            points.last());

      fn(segment_i, points_range1, points_range2);
    }
  }
  return segments.size();
}

bool apply_mask_as_selection(bke::CurvesGeometry &curves,
                             const IndexMask &selection_mask,
                             const bke::AttrDomain selection_domain,
                             const StringRef attribute_name,
                             const GrainSize grain_size,
                             const eSelectOp sel_op)
{
  if (selection_mask.is_empty()) {
    return false;
  }

  const eCustomDataType create_type = CD_PROP_BOOL;
  bke::GSpanAttributeWriter writer = ed::curves::ensure_selection_attribute(
      curves, selection_domain, create_type, attribute_name);

  selection_mask.foreach_index(grain_size, [&](const int64_t element_i) {
    ed::curves::apply_selection_operation_at_index(writer.span, element_i, sel_op);
  });

  writer.finish();

  return true;
}

bool apply_mask_as_segment_selection(bke::CurvesGeometry &curves,
                                     const IndexMask &point_selection_mask,
                                     const StringRef attribute_name,
                                     const Curves2DBVHTree &tree_data,
                                     const IndexRange tree_data_range,
                                     const GrainSize grain_size,
                                     const eSelectOp sel_op)
{
  /* Use regular selection for anything other than the ".selection" attribute. */
  if (attribute_name != ".selection") {
    return apply_mask_as_selection(
        curves, point_selection_mask, bke::AttrDomain::Point, attribute_name, grain_size, sel_op);
  }

  if (point_selection_mask.is_empty()) {
    return false;
  }
  IndexMaskMemory memory;

  const IndexMask changed_curve_mask = ed::curves::curve_mask_from_points(
      curves, point_selection_mask, GrainSize(512), memory);

  const OffsetIndices points_by_curve = curves.points_by_curve();
  const Span<float2> screen_space_positions = tree_data.start_positions.as_span().slice(
      tree_data_range);

  const CurveSegmentsData segment_data = ed::greasepencil::find_curve_segments(
      curves, changed_curve_mask, screen_space_positions, tree_data, tree_data_range);

  const OffsetIndices<int> segments_by_curve = OffsetIndices<int>(segment_data.segment_offsets);
  const eCustomDataType create_type = CD_PROP_BOOL;
  bke::GSpanAttributeWriter attribute_writer = ed::curves::ensure_selection_attribute(
      curves, bke::AttrDomain::Point, create_type, attribute_name);

  /* Find all segments that have changed points and fill them. */
  Array<bool> changed_points(curves.points_num());
  point_selection_mask.to_bools(changed_points);

  auto test_points_range = [&](const IndexRange range) -> bool {
    for (const int point_i : range) {
      if (changed_points[point_i]) {
        return true;
      }
    }
    return false;
  };
  auto update_points_range = [&](const IndexRange range) {
    for (const int point_i : range) {
      ed::curves::apply_selection_operation_at_index(attribute_writer.span, point_i, sel_op);
    }
  };

  threading::parallel_for(
      segments_by_curve.index_range(), grain_size.value, [&](const IndexRange range) {
        for (const int curve_i : range) {
          const IndexRange points = points_by_curve[curve_i];

          const int num_segments = foreach_curve_segment(
              segment_data,
              curve_i,
              points,
              [&](const int /*segment_i*/, const IndexRange points1, const IndexRange points2) {
                if (test_points_range(points1) || test_points_range(points2)) {
                  update_points_range(points1);
                  update_points_range(points2);
                }
              });
          if (num_segments == 0 && test_points_range(points)) {
            /* Cyclic curve without cuts, select all. */
            update_points_range(points);
          }
        }
      });

  attribute_writer.finish();
  return true;
}

bool selection_update(const ViewContext *vc,
                      const eSelectOp sel_op,
                      SelectionUpdateFunc select_operation)
{
  using namespace blender;

  Object *object = (vc->obedit ? vc->obedit : vc->obact);
  const Object *ob_eval = DEG_get_evaluated_object(vc->depsgraph, object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  /* Get selection domain from tool settings. */
  const bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(
      vc->scene->toolsettings, object);
  const bool use_segment_selection = ED_grease_pencil_segment_selection_enabled(
      vc->scene->toolsettings, object);

  bool changed = false;
  const Array<Vector<ed::greasepencil::MutableDrawingInfo>> drawings_by_frame =
      ed::greasepencil::retrieve_editable_drawings_grouped_per_frame(*vc->scene, grease_pencil);

  for (const Span<ed::greasepencil::MutableDrawingInfo> drawings : drawings_by_frame) {
    if (drawings.is_empty()) {
      continue;
    }
    const int frame_number = drawings.first().frame_number;

    /* Construct BVH tree for all drawings on the same frame. */
    ed::greasepencil::Curves2DBVHTree tree_data;
    BLI_SCOPED_DEFER([&]() { ed::greasepencil::free_curves_2d_bvh_data(tree_data); });
    if (use_segment_selection) {
      tree_data = ed::greasepencil::build_curves_2d_bvh_from_visible(
          *vc, *ob_eval, grease_pencil, drawings, frame_number);
    }
    OffsetIndices tree_data_by_drawing = OffsetIndices<int>(tree_data.drawing_offsets);

    for (const int i_drawing : drawings.index_range()) {
      // TODO optimize by lazy-initializing the tree data ONLY IF the changed_element_mask is not
      // empty.

      const ed::greasepencil::MutableDrawingInfo &info = drawings[i_drawing];
      bke::CurvesGeometry &curves = info.drawing.strokes_for_write();
      const Span<StringRef> selection_attribute_names =
          ed::curves::get_curves_selection_attribute_names(curves);

      IndexMaskMemory memory;
      const IndexMask elements = ed::greasepencil::retrieve_editable_elements(
          *object, info, selection_domain, memory);
      if (elements.is_empty()) {
        continue;
      }

      for (const StringRef attribute_name : selection_attribute_names) {
        const IndexMask changed_element_mask = select_operation(
            info, elements, attribute_name, memory);

        /* Modes that un-set all elements not in the mask. */
        if (ELEM(sel_op, SEL_OP_SET, SEL_OP_AND)) {
          if (bke::GSpanAttributeWriter selection =
                  curves.attributes_for_write().lookup_for_write_span(attribute_name))
          {
            ed::curves::fill_selection_false(selection.span);
            selection.finish();
          }
        }

        if (use_segment_selection) {
          /* Range of points in tree data matching this curve, for re-using screen space
           * positions. */
          const IndexRange tree_data_range = tree_data_by_drawing[i_drawing];
          changed |= ed::greasepencil::apply_mask_as_segment_selection(curves,
                                                                       changed_element_mask,
                                                                       attribute_name,
                                                                       tree_data,
                                                                       tree_data_range,
                                                                       GrainSize(4096),
                                                                       sel_op);
        }
        else {
          changed |= ed::greasepencil::apply_mask_as_selection(curves,
                                                               changed_element_mask,
                                                               selection_domain,
                                                               attribute_name,
                                                               GrainSize(4096),
                                                               sel_op);
        }
      }
    }
  }

  if (changed) {
    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a
     * generic attribute for now. */
    DEG_id_tag_update(static_cast<ID *>(object->data), ID_RECALC_GEOMETRY);
    WM_event_add_notifier(vc->C, NC_GEOM | ND_DATA, object->data);
  }

  return changed;
}

/** \} */

static int select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(scene->toolsettings,
                                                                           object);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selectable_elements = retrieve_editable_elements(
        *object, info, selection_domain, memory);
    if (selectable_elements.is_empty()) {
      return;
    }
    if (action == SEL_TOGGLE) {
      action = blender::ed::curves::has_anything_selected(info.drawing.strokes(),
                                                          selection_domain) ?
                   SEL_DESELECT :
                   SEL_SELECT;
    }
    blender::ed::curves::select_all(
        info.drawing.strokes_for_write(), selectable_elements, selection_domain, action);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All Strokes";
  ot->idname = "GREASE_PENCIL_OT_select_all";
  ot->description = "(De)select all visible strokes";

  ot->exec = select_all_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static int select_more_exec(bContext *C, wmOperator * /*op*/)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const ViewContext vc = ED_view3d_viewcontext_init(C, CTX_data_depsgraph_pointer(C));

  ed::greasepencil::selection_update(&vc,
                                     SEL_OP_ADD,
                                     [&](const ed::greasepencil::MutableDrawingInfo &info,
                                         const IndexMask & /*universe*/,
                                         StringRef attribute_name,
                                         IndexMaskMemory &memory) {
                                       return blender::ed::curves::select_adjacent_mask(
                                           info.drawing.strokes(), attribute_name, false, memory);
                                     });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->idname = "GREASE_PENCIL_OT_select_more";
  ot->description = "Grow the selection by one point";

  ot->exec = select_more_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_less_exec(bContext *C, wmOperator * /*op*/)
{
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const ViewContext vc = ED_view3d_viewcontext_init(C, CTX_data_depsgraph_pointer(C));

  ed::greasepencil::selection_update(&vc,
                                     SEL_OP_SUB,
                                     [&](const ed::greasepencil::MutableDrawingInfo &info,
                                         const IndexMask & /*universe*/,
                                         StringRef attribute_name,
                                         IndexMaskMemory &memory) {
                                       return blender::ed::curves::select_adjacent_mask(
                                           info.drawing.strokes(), attribute_name, true, memory);
                                     });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->idname = "GREASE_PENCIL_OT_select_less";
  ot->description = "Shrink the selection by one point";

  ot->exec = select_less_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    IndexMaskMemory memory;
    const IndexMask selectable_strokes = ed::greasepencil::retrieve_editable_strokes(
        *object, info.drawing, info.layer_index, memory);
    if (selectable_strokes.is_empty()) {
      return;
    }
    blender::ed::curves::select_linked(info.drawing.strokes_for_write(), selectable_strokes);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked";
  ot->idname = "GREASE_PENCIL_OT_select_linked";
  ot->description = "Select all points in curves with any point selection";

  ot->exec = select_linked_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int select_random_exec(bContext *C, wmOperator *op)
{
  using namespace blender;
  const float ratio = RNA_float_get(op->ptr, "ratio");
  const int seed = WM_operator_properties_select_random_seed_increment_get(op);
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(scene->toolsettings,
                                                                           object);
  const ViewContext vc = ED_view3d_viewcontext_init(C, CTX_data_depsgraph_pointer(C));

  /* Note: For segment selection this doesn't work very well, because it is based on random point
   * selection. A segment has a high probability of getting at least one selected point and be
   * itself selected.
   * For better distribution the random value must be generated per segment and possibly weighted
   * by segment length.
   */
  ed::greasepencil::selection_update(
      &vc,
      SEL_OP_SET,
      [&](const ed::greasepencil::MutableDrawingInfo &info,
          const IndexMask & /*universe*/,
          StringRef /*attribute_name*/,
          IndexMaskMemory &memory) -> IndexMask {
        const IndexMask selectable_elements = retrieve_editable_elements(
            *object, info, selection_domain, memory);
        if (selectable_elements.is_empty()) {
          return {};
        }
        return ed::curves::random_mask(info.drawing.strokes(),
                                       selectable_elements,
                                       selection_domain,
                                       blender::get_default_hash<int>(seed, info.layer_index),
                                       ratio,
                                       memory);
      });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_random(wmOperatorType *ot)
{
  ot->name = "Select Random";
  ot->idname = "GREASE_PENCIL_OT_select_random";
  ot->description = "Selects random points from the current strokes selection";

  ot->exec = select_random_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_random(ot);
}

static int select_alternate_exec(bContext *C, wmOperator *op)
{
  const bool deselect_ends = RNA_boolean_get(op->ptr, "deselect_ends");
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    blender::ed::curves::select_alternate(info.drawing.strokes_for_write(), deselect_ends);
  });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_alternate(wmOperatorType *ot)
{
  ot->name = "Select Alternate";
  ot->idname = "GREASE_PENCIL_OT_select_alternate";
  ot->description = "Select alternated points in strokes with already selected points";

  ot->exec = select_alternate_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "deselect_ends",
                  false,
                  "Deselect Ends",
                  "(De)select the first and last point of each stroke");
}

enum class SelectSimilarMode {
  LAYER,
  MATERIAL,
  VERTEX_COLOR,
  RADIUS,
  OPACITY,
};

static const EnumPropertyItem select_similar_mode_items[] = {
    {int(SelectSimilarMode::LAYER), "LAYER", 0, "Layer", ""},
    {int(SelectSimilarMode::MATERIAL), "MATERIAL", 0, "Material", ""},
    {int(SelectSimilarMode::VERTEX_COLOR), "VERTEX_COLOR", 0, "Vertex Color", ""},
    {int(SelectSimilarMode::RADIUS), "RADIUS", 0, "Radius", ""},
    {int(SelectSimilarMode::OPACITY), "OPACITY", 0, "Opacity", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

template<typename T>
void insert_selected_values(const bke::CurvesGeometry &curves,
                            const bke::AttrDomain domain,
                            const StringRef attribute_id,
                            blender::Set<T> &r_value_set)
{
  T default_value;
  CPPType::get<T>().default_construct(&default_value);

  const bke::AttributeAccessor attributes = curves.attributes();
  const VArraySpan<bool> selection = *attributes.lookup_or_default<bool>(
      ".selection", domain, true);
  const VArraySpan<T> values = *attributes.lookup_or_default<T>(
      attribute_id, domain, default_value);

  threading::EnumerableThreadSpecific<Set<T>> value_set_by_thread;
  threading::parallel_for(
      IndexRange(attributes.domain_size(domain)), 1024, [&](const IndexRange range) {
        Set<T> &local_value_set = value_set_by_thread.local();
        for (const int i : range) {
          if (selection[i]) {
            local_value_set.add(values[i]);
          }
        }
      });

  for (const Set<T> &local_value_set : value_set_by_thread) {
    /* TODO is there a union function that can do this more efficiently? */
    for (const T &key : local_value_set) {
      r_value_set.add(key);
    }
  }
}

template<typename T, typename DistanceFn>
static void select_similar_by_value(Scene *scene,
                                    Object *object,
                                    GreasePencil &grease_pencil,
                                    const bke::AttrDomain domain,
                                    const StringRef attribute_id,
                                    float threshold,
                                    DistanceFn distance_fn)
{
  using namespace blender::ed::greasepencil;

  T default_value;
  CPPType::get<T>().default_construct(&default_value);

  const blender::Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene,
                                                                                  grease_pencil);

  blender::Set<T> selected_values;
  for (const MutableDrawingInfo &info : drawings) {
    insert_selected_values(info.drawing.strokes(), domain, attribute_id, selected_values);
  }

  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::MutableAttributeAccessor attributes =
        info.drawing.strokes_for_write().attributes_for_write();
    const int domain_size = attributes.domain_size(domain);
    bke::SpanAttributeWriter<bool> selection_writer =
        attributes.lookup_or_add_for_write_span<bool>(
            ".selection",
            domain,
            bke::AttributeInitVArray(VArray<bool>::ForSingle(true, domain_size)));
    const VArraySpan<T> values = *attributes.lookup_or_default<T>(
        attribute_id, domain, default_value);

    IndexMaskMemory memory;
    const IndexMask mask = ed::greasepencil::retrieve_editable_points(
        *object, info.drawing, info.layer_index, memory);

    mask.foreach_index(GrainSize(1024), [&](const int index) {
      if (selection_writer.span[index]) {
        return;
      }
      for (const T &test_value : selected_values) {
        if (distance_fn(values[index], test_value) <= threshold) {
          selection_writer.span[index] = true;
        }
      }
    });

    selection_writer.finish();
  });
}

static void select_similar_by_layer(Scene *scene,
                                    Object *object,
                                    GreasePencil &grease_pencil,
                                    bke::AttrDomain domain)
{
  const blender::Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene,
                                                                                  grease_pencil);

  blender::Set<int> selected_layers;
  /* Layer is selected if any point is selected. */
  for (const MutableDrawingInfo &info : drawings) {
    const VArraySpan<bool> selection =
        *info.drawing.strokes().attributes().lookup_or_default<bool>(".selection", domain, true);
    for (const int i : selection.index_range()) {
      if (selection[i]) {
        selected_layers.add(info.layer_index);
        break;
      }
    }
  }

  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    if (!selected_layers.contains(info.layer_index)) {
      return;
    }

    IndexMaskMemory memory;
    const IndexMask editable_elements = retrieve_editable_elements(*object, info, domain, memory);
    if (editable_elements.is_empty()) {
      return;
    }
    ed::curves::select_all(
        info.drawing.strokes_for_write(), editable_elements, domain, SEL_SELECT);
  });
}

static int select_similar_exec(bContext *C, wmOperator *op)
{
  const SelectSimilarMode mode = SelectSimilarMode(RNA_enum_get(op->ptr, "mode"));
  const float threshold = RNA_float_get(op->ptr, "threshold");
  Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  bke::AttrDomain selection_domain = ED_grease_pencil_selection_domain_get(scene->toolsettings,
                                                                           object);

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);

  switch (mode) {
    case SelectSimilarMode::LAYER:
      select_similar_by_layer(scene, object, grease_pencil, selection_domain);
      break;
    case SelectSimilarMode::MATERIAL:
      select_similar_by_value<int>(
          scene,
          object,
          grease_pencil,
          selection_domain,
          "material_index",
          threshold,
          [](const int a, const int b) -> float { return float(math::distance(a, b)); });
      break;
    case SelectSimilarMode::VERTEX_COLOR:
      select_similar_by_value<ColorGeometry4f>(
          scene,
          object,
          grease_pencil,
          selection_domain,
          "vertex_color",
          threshold,
          [](const ColorGeometry4f &a, const ColorGeometry4f &b) -> float {
            return math::distance(float4(a), float4(b));
          });
      break;
    case SelectSimilarMode::RADIUS:
      select_similar_by_value<float>(
          scene,
          object,
          grease_pencil,
          selection_domain,
          "radius",
          threshold,
          [](const float a, const float b) -> float { return math::distance(a, b); });
      break;
    case SelectSimilarMode::OPACITY:
      select_similar_by_value<float>(
          scene,
          object,
          grease_pencil,
          selection_domain,
          "opacity",
          threshold,
          [](const float a, const float b) -> float { return math::distance(a, b); });
      break;
  }

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_similar(wmOperatorType *ot)
{
  ot->name = "Select Similar";
  ot->idname = "GREASE_PENCIL_OT_select_similar";
  ot->description = "Select all strokes with similar characteristics";

  ot->invoke = WM_menu_invoke;
  ot->exec = select_similar_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "mode", select_similar_mode_items, int(SelectSimilarMode::LAYER), "Mode", "");

  RNA_def_float(ot->srna, "threshold", 0.1f, 0.0f, FLT_MAX, "Threshold", "", 0.0f, 10.0f);
}

static int select_ends_exec(bContext *C, wmOperator *op)
{
  const int amount_start = RNA_int_get(op->ptr, "amount_start");
  const int amount_end = RNA_int_get(op->ptr, "amount_end");
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const ViewContext vc = ED_view3d_viewcontext_init(C, CTX_data_depsgraph_pointer(C));

  ed::greasepencil::selection_update(
      &vc,
      SEL_OP_SET,
      [&](const ed::greasepencil::MutableDrawingInfo &info,
          const IndexMask & /*universe*/,
          StringRef /*attribute_name*/,
          IndexMaskMemory &memory) {
        const IndexMask selectable_strokes = ed::greasepencil::retrieve_editable_strokes(
            *object, info.drawing, info.layer_index, memory);
        return ed::curves::end_points(
            info.drawing.strokes(), selectable_strokes, amount_start, amount_end, false, memory);
      });

  /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
   * attribute for now. */
  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_select_ends(wmOperatorType *ot)
{
  ot->name = "Select Ends";
  ot->idname = "GREASE_PENCIL_OT_select_ends";
  ot->description = "Select end points of strokes";

  ot->exec = select_ends_exec;
  ot->poll = editable_grease_pencil_point_selection_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "amount_start",
              0,
              0,
              INT32_MAX,
              "Amount Start",
              "Number of points to select from the start",
              0,
              INT32_MAX);
  RNA_def_int(ot->srna,
              "amount_end",
              1,
              0,
              INT32_MAX,
              "Amount End",
              "Number of points to select from the end",
              0,
              INT32_MAX);
}

static int select_set_mode_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::greasepencil;

  /* Set new selection mode. */
  const int mode_new = RNA_enum_get(op->ptr, "mode");
  ToolSettings *ts = CTX_data_tool_settings(C);

  bool changed = (mode_new != ts->gpencil_selectmode_edit);
  ts->gpencil_selectmode_edit = mode_new;

  /* Convert all drawings of the active GP to the new selection domain. */
  Object *object = CTX_data_active_object(C);
  const bke::AttrDomain domain = ED_grease_pencil_selection_domain_get(ts, object);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  Span<GreasePencilDrawingBase *> drawings = grease_pencil.drawings();

  for (const int index : drawings.index_range()) {
    GreasePencilDrawingBase *drawing_base = drawings[index];
    if (drawing_base->type != GP_DRAWING) {
      continue;
    }

    GreasePencilDrawing *drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base);
    bke::CurvesGeometry &curves = drawing->wrap().strokes_for_write();
    if (curves.is_empty()) {
      continue;
    }

    /* Skip curve when the selection domain already matches, or when there is no selection
     * at all. */
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    const std::optional<bke::AttributeMetaData> meta_data = attributes.lookup_meta_data(
        ".selection");
    if ((!meta_data) || (meta_data->domain == domain)) {
      continue;
    }

    /* When the new selection domain is 'curve', ensure all curves with a point selection
     * are selected. */
    if (domain == bke::AttrDomain::Curve) {
      blender::ed::curves::select_linked(curves);
    }

    /* Convert selection domain. */
    const GVArray src = *attributes.lookup(".selection", domain);
    if (src) {
      const CPPType &type = src.type();
      void *dst = MEM_malloc_arrayN(attributes.domain_size(domain), type.size(), __func__);
      src.materialize(dst);

      attributes.remove(".selection");
      if (!attributes.add(".selection",
                          domain,
                          bke::cpp_type_to_custom_data_type(type),
                          bke::AttributeInitMoveArray(dst)))
      {
        MEM_freeN(dst);
      }

      changed = true;

      /* TODO: expand point selection to segments when in 'segment' mode. */
    }
  }

  if (changed) {
    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &grease_pencil);

    WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_set_selection_mode(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Select Mode";
  ot->idname = __func__;
  ot->description = "Change the selection mode for Grease Pencil strokes";

  ot->exec = select_set_mode_exec;
  ot->poll = editable_grease_pencil_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_grease_pencil_selectmode_items, 0, "Mode", "");
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));
}

static int grease_pencil_material_select_exec(bContext *C, wmOperator *op)
{
  const Scene *scene = CTX_data_scene(C);
  Object *object = CTX_data_active_object(C);
  GreasePencil &grease_pencil = *static_cast<GreasePencil *>(object->data);
  const bool select = !RNA_boolean_get(op->ptr, "deselect");
  const int material_index = object->actcol - 1;

  if (material_index == -1) {
    return OPERATOR_CANCELLED;
  }

  const Vector<MutableDrawingInfo> drawings = retrieve_editable_drawings(*scene, grease_pencil);
  threading::parallel_for_each(drawings, [&](const MutableDrawingInfo &info) {
    bke::CurvesGeometry &curves = info.drawing.strokes_for_write();

    IndexMaskMemory memory;
    const IndexMask strokes = retrieve_editable_strokes_by_material(
        *object, info.drawing, material_index, memory);
    if (strokes.is_empty()) {
      return;
    }
    bke::GSpanAttributeWriter selection = ed::curves::ensure_selection_attribute(
        curves, bke::AttrDomain::Curve, CD_PROP_BOOL);
    index_mask::masked_fill(selection.span.typed<bool>(), select, strokes);
    selection.finish();
  });

  DEG_id_tag_update(&grease_pencil.id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA | NA_EDITED, &grease_pencil);

  return OPERATOR_FINISHED;
}

static void GREASE_PENCIL_OT_material_select(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Material";
  ot->idname = "GREASE_PENCIL_OT_material_select";
  ot->description = "Select/Deselect all Grease Pencil strokes using current material";

  /* callbacks. */
  ot->exec = grease_pencil_material_select_exec;
  ot->poll = editable_grease_pencil_poll;

  /* flags. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ot->prop = RNA_def_boolean(ot->srna, "deselect", false, "Deselect", "Unselect strokes");
  RNA_def_property_flag(ot->prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

}  // namespace blender::ed::greasepencil

blender::bke::AttrDomain ED_grease_pencil_edit_selection_domain_get(
    const ToolSettings *tool_settings)
{
  switch (tool_settings->gpencil_selectmode_edit) {
    case GP_SELECTMODE_POINT:
      return blender::bke::AttrDomain::Point;
    case GP_SELECTMODE_STROKE:
      return blender::bke::AttrDomain::Curve;
    case GP_SELECTMODE_SEGMENT:
      return blender::bke::AttrDomain::Point;
  }
  return blender::bke::AttrDomain::Point;
}

blender::bke::AttrDomain ED_grease_pencil_sculpt_selection_domain_get(
    const ToolSettings *tool_settings)
{
  const int selectmode = tool_settings->gpencil_selectmode_sculpt;
  if (selectmode & (GP_SCULPT_MASK_SELECTMODE_POINT | GP_SCULPT_MASK_SELECTMODE_SEGMENT)) {
    return blender::bke::AttrDomain::Point;
  }
  if (selectmode & (GP_SCULPT_MASK_SELECTMODE_STROKE)) {
    return blender::bke::AttrDomain::Curve;
  }
  return blender::bke::AttrDomain::Point;
}

blender::bke::AttrDomain ED_grease_pencil_vertex_selection_domain_get(
    const ToolSettings *tool_settings)
{
  const int selectmode = tool_settings->gpencil_selectmode_vertex;
  if (selectmode & (GP_VERTEX_MASK_SELECTMODE_POINT | GP_VERTEX_MASK_SELECTMODE_SEGMENT)) {
    return blender::bke::AttrDomain::Point;
  }
  if (selectmode & (GP_VERTEX_MASK_SELECTMODE_STROKE)) {
    return blender::bke::AttrDomain::Curve;
  }
  return blender::bke::AttrDomain::Point;
}

blender::bke::AttrDomain ED_grease_pencil_selection_domain_get(const ToolSettings *tool_settings,
                                                               const Object *object)
{
  if (object->mode & OB_MODE_EDIT) {
    return ED_grease_pencil_edit_selection_domain_get(tool_settings);
  }
  if (object->mode & OB_MODE_SCULPT_GREASE_PENCIL) {
    return ED_grease_pencil_sculpt_selection_domain_get(tool_settings);
  }
  if (object->mode & OB_MODE_VERTEX_GREASE_PENCIL) {
    return ED_grease_pencil_vertex_selection_domain_get(tool_settings);
  }
  return blender::bke::AttrDomain::Point;
}

bool ED_grease_pencil_edit_segment_selection_enabled(const ToolSettings *tool_settings)
{
  return tool_settings->gpencil_selectmode_edit == GP_SELECTMODE_SEGMENT;
}

bool ED_grease_pencil_sculpt_segment_selection_enabled(const ToolSettings *tool_settings)
{
  return tool_settings->gpencil_selectmode_sculpt & GP_SCULPT_MASK_SELECTMODE_SEGMENT;
}

bool ED_grease_pencil_vertex_segment_selection_enabled(const ToolSettings *tool_settings)
{
  return tool_settings->gpencil_selectmode_vertex & GP_VERTEX_MASK_SELECTMODE_SEGMENT;
}

bool ED_grease_pencil_segment_selection_enabled(const ToolSettings *tool_settings,
                                                const Object *object)
{
  if (object->mode & OB_MODE_EDIT) {
    return ED_grease_pencil_edit_segment_selection_enabled(tool_settings);
  }
  if (object->mode & OB_MODE_SCULPT_GREASE_PENCIL) {
    return ED_grease_pencil_sculpt_segment_selection_enabled(tool_settings);
  }
  if (object->mode & OB_MODE_VERTEX_GREASE_PENCIL) {
    return ED_grease_pencil_vertex_segment_selection_enabled(tool_settings);
  }
  return false;
}

void ED_operatortypes_grease_pencil_select()
{
  using namespace blender::ed::greasepencil;
  WM_operatortype_append(GREASE_PENCIL_OT_select_all);
  WM_operatortype_append(GREASE_PENCIL_OT_select_more);
  WM_operatortype_append(GREASE_PENCIL_OT_select_less);
  WM_operatortype_append(GREASE_PENCIL_OT_select_linked);
  WM_operatortype_append(GREASE_PENCIL_OT_select_random);
  WM_operatortype_append(GREASE_PENCIL_OT_select_alternate);
  WM_operatortype_append(GREASE_PENCIL_OT_select_similar);
  WM_operatortype_append(GREASE_PENCIL_OT_select_ends);
  WM_operatortype_append(GREASE_PENCIL_OT_set_selection_mode);
  WM_operatortype_append(GREASE_PENCIL_OT_material_select);
}

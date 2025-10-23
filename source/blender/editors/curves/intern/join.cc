/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_instances.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_curves.hh"
#include "ED_object.hh"

#include "GEO_realize_instances.hh"

namespace blender::ed::curves {

wmOperatorStatus join_objects_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *active_object = CTX_data_active_object(C);
  BLI_assert(active_object);
  BLI_assert(active_object->type == OB_CURVES);
  Curves &active_curves = *static_cast<Curves *>(active_object->data);
  const float4x4 &world_to_active = active_object->world_to_object();

  Vector<Object *> objects{active_object};
  bool active_object_selected = false;
  CTX_DATA_BEGIN (C, Object *, object, selected_editable_objects) {
    if (object == active_object) {
      active_object_selected = true;
      continue;
    }
    if (object->type != OB_CURVES) {
      continue;
    }
    objects.append(object);
  }
  CTX_DATA_END;

  if (!active_object_selected) {
    BKE_report(op->reports, RPT_WARNING, "Active object is not a selected curves object");
    return OPERATOR_CANCELLED;
  }

  bke::Instances instances;
  instances.resize(objects.size());
  MutableSpan<float4x4> transforms = instances.transforms_for_write();
  MutableSpan<int> references = instances.reference_handles_for_write();
  Map<const Curves *, int> reference_by_orig_curves;
  for (const int i : objects.index_range()) {
    transforms[i] = world_to_active * objects[i]->object_to_world();
    const Curves *orig_curves = static_cast<const Curves *>(objects[i]->data);
    references[i] = reference_by_orig_curves.lookup_or_add_cb(orig_curves, [&]() {
      auto geometry = bke::GeometrySet::from_curves(BKE_curves_copy_for_eval(orig_curves));
      return instances.add_new_reference(std::move(geometry));
    });
  }

  bke::GeometrySet realized_geometry = geometry::realize_instances(
                                           bke::GeometrySet::from_instances(
                                               &instances, bke::GeometryOwnershipType::ReadOnly),
                                           geometry::RealizeInstancesOptions())
                                           .geometry;

  if (!realized_geometry.has_curves()) {
    BKE_report(op->reports, RPT_WARNING, "No curves data to join");
    return OPERATOR_CANCELLED;
  }

  Curves *realized_curves = realized_geometry.get_curves_for_write();
  active_curves.geometry.wrap() = std::move(realized_curves->geometry.wrap());

  for (Object *object : objects.as_span().drop_front(1)) {
    object::base_free_and_unlink(bmain, scene, object);
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&active_object->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

}  // namespace blender::ed::curves

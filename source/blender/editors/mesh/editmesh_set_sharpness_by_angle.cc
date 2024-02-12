/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "BLI_math_angle_types.hh"
#include "BLI_math_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph.hh"

#include "WM_api.hh"

#include "ED_screen.hh"

#include "mesh_intern.hh"

namespace blender::ed::mesh {

static int set_sharpness_by_angle_exec(bContext *C, wmOperator *op)
{
  const float angle_limit_cos = std::cos(RNA_float_get(op->ptr, "angle"));
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      CTX_data_scene(C), CTX_data_view_layer(C), CTX_wm_view3d(C));

  for (Object *object : objects) {
    Mesh &mesh = *static_cast<Mesh *>(object->data);
    BMEditMesh *em = mesh.edit_mesh;

    bool changed = false;
    BMIter iter;
    BMEdge *e;
    BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
      if (!BM_elem_flag_test(e, BM_ELEM_SELECT)) {
        continue;
      }
      const bool prev_sharp = !BM_elem_flag_test(e, BM_ELEM_SMOOTH);
      if (extend && prev_sharp) {
        continue;
      }
      BMLoop *l1, *l2;
      if (!BM_edge_loop_pair(e, &l1, &l2)) {
        continue;
      }
      const float angle_cos = math::dot(float3(l1->f->no), float3(l2->f->no));
      const bool sharp = angle_cos <= angle_limit_cos;
      BM_elem_flag_set(e, BM_ELEM_SMOOTH, !sharp);
      changed = changed || sharp != prev_sharp;
    }

    if (changed) {
      BKE_editmesh_lnorspace_update(em);
      DEG_id_tag_update(&mesh.id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, &mesh.id);
    }
  }

  return OPERATOR_FINISHED;
}

void MESH_OT_set_sharpness_by_angle(wmOperatorType *ot)
{
  ot->name = "Set Sharpness by Angle";
  ot->description = "Set edge sharpness based on the angle between neighboring faces";
  ot->idname = "MESH_OT_set_sharpness_by_angle";

  ot->exec = set_sharpness_by_angle_exec;
  ot->poll = ED_operator_editmesh;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_float_rotation(ot->srna,
                                             "angle",
                                             0,
                                             nullptr,
                                             math::AngleRadian::from_degree(0.01f).radian(),
                                             math::AngleRadian::from_degree(180.0f).radian(),
                                             "Angle",
                                             "",
                                             math::AngleRadian::from_degree(1.0f).radian(),
                                             math::AngleRadian::from_degree(180.0f).radian());
  RNA_def_property_float_default(prop, math::AngleRadian::from_degree(30.0f).radian());

  RNA_def_boolean(ot->srna,
                  "extend",
                  false,
                  "Extend",
                  "Add new sharp edges without clearing existing sharp edges");
}

}  // namespace blender::ed::mesh

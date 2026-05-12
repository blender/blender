/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"

#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "BLT_translation.hh"

#include "ED_mesh.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "mesh_intern.hh" /* own include */

namespace blender {

static const EnumPropertyItem prop_method_items[] = {
    {FLATTEN_BEST_FIT, "BEST_FIT", 0, "Best Fit", "Calculate a best fitting plane"},
    {FLATTEN_NORMAL, "NORMAL", 0, "Normal", "Derive plane by averaging face normals"},
    {FLATTEN_VIEW, "VIEW", 0, "View", "Flatten on a plane perpendicular to the viewing angle"},
    {0, nullptr},
};

static wmOperatorStatus edbm_flatten_exec(bContext *C, wmOperator *op)
{
  const Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(
      *bmain, scene, view_layer, CTX_wm_view3d(C));

  const float factor = RNA_float_get(op->ptr, "factor");
  const int method = RNA_enum_get(op->ptr, "method");
  bool lock[3];
  RNA_boolean_get_array(op->ptr, "lock", lock);
  bool changed = false;
  RegionView3D *rv3d = method == FLATTEN_VIEW ? CTX_wm_region_view3d(C) : nullptr;

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    float view_normal[3] = {0.0f, 0.0f, 1.0f};
    if (rv3d) {
      float3 vn = math::normalize(
          math::transform_direction(obedit->world_to_object(), float3(rv3d->viewinv[2])));
      copy_v3_v3(view_normal, vn);
    }

    if (!EDBM_op_callf(
            em,
            op,
            "flatten geom=%hvef factor=%f method=%i view_normal=%v lock_x=%b lock_y=%b lock_z=%b",
            BM_ELEM_SELECT,
            factor,
            method,
            view_normal,
            lock[0],
            lock[1],
            lock[2]))
    {
      continue;
    }

    changed = true;
    EDBMUpdate_Params params{};
    params.calc_looptris = true;
    params.calc_normals = true;
    EDBM_update(id_cast<Mesh *>(obedit->data), &params);
  }

  return changed ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static void edbm_flatten_ui(bContext * /*C*/, wmOperator *op)
{
  ui::Layout &layout = *op->layout;
  layout.use_property_split_set(true);

  layout.prop(op->ptr, "factor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout.prop(op->ptr, "method", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  ui::Layout &lock_row = layout.row(true, IFACE_("Lock"));
  PropertyRNA *lock_prop = RNA_struct_find_property(op->ptr, "lock");
  lock_row.prop(op->ptr, lock_prop, 0, 0, ui::ITEM_R_TOGGLE, "X", ICON_NONE);
  lock_row.prop(op->ptr, lock_prop, 1, 0, ui::ITEM_R_TOGGLE, "Y", ICON_NONE);
  lock_row.prop(op->ptr, lock_prop, 2, 0, ui::ITEM_R_TOGGLE, "Z", ICON_NONE);
}

void MESH_OT_flatten(wmOperatorType *ot)
{
  ot->name = "Flatten";
  ot->description = "Flatten vertices on a best-fitting plane";
  ot->idname = "MESH_OT_flatten";

  ot->exec = edbm_flatten_exec;
  ot->poll = ED_operator_editmesh;
  ot->ui = edbm_flatten_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_float_factor(
      ot->srna, "factor", 1.0f, 0.0f, 1.0f, "Factor", "Force of the tool", 0.0f, 1.0f);
  RNA_def_enum(
      ot->srna, "method", prop_method_items, 0, "Method", "Plane on which vertices are flattened");
  RNA_def_boolean_array(ot->srna, "lock", 3, nullptr, "Lock", "Lock editing of the axis");
}

}  // namespace blender

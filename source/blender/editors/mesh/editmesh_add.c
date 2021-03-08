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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edmesh
 */

#include "BLI_math.h"
#include "BLI_sys_types.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "mesh_intern.h" /* own include */

#define MESH_ADD_VERTS_MAXI 10000000

/* ********* add primitive operators ************* */

typedef struct MakePrimitiveData {
  float mat[4][4];
  bool was_editmode;
} MakePrimitiveData;

static Object *make_prim_init(bContext *C,
                              const char *idname,
                              const float loc[3],
                              const float rot[3],
                              const float scale[3],
                              ushort local_view_bits,
                              MakePrimitiveData *r_creation_data)
{
  struct Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);

  r_creation_data->was_editmode = false;
  if (obedit == NULL || obedit->type != OB_MESH) {
    obedit = ED_object_add_type(C, OB_MESH, idname, loc, rot, false, local_view_bits);
    ED_object_editmode_enter_ex(bmain, scene, obedit, 0);

    r_creation_data->was_editmode = true;
  }

  ED_object_new_primitive_matrix(C, obedit, loc, rot, r_creation_data->mat);

  if (scale) {
    rescale_m4(r_creation_data->mat, scale);
  }

  return obedit;
}

static void make_prim_finish(bContext *C,
                             Object *obedit,
                             const MakePrimitiveData *creation_data,
                             int enter_editmode)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool exit_editmode = ((creation_data->was_editmode == true) && (enter_editmode == false));

  /* Primitive has all verts selected, use vert select flush
   * to push this up to edges & faces. */
  EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

  /* only recalc editmode tessface if we are staying in editmode */
  EDBM_update_generic(obedit->data, !exit_editmode, true);

  /* userdef */
  if (exit_editmode) {
    ED_object_editmode_exit_ex(CTX_data_main(C), CTX_data_scene(C), obedit, EM_FREEDATA);
  }
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);
}

static int add_primitive_plane_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Plane"),
                          loc,
                          rot,
                          NULL,
                          local_view_bits,
                          &creation_data);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_grid x_segments=%i y_segments=%i size=%f matrix=%m4 calc_uvs=%b",
          1,
          1,
          RNA_float_get(op->ptr, "size") / 2.0f,
          creation_data.mat,
          calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_plane_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Plane";
  ot->description = "Construct a filled planar mesh with 4 vertices";
  ot->idname = "MESH_OT_primitive_plane_add";

  /* api callbacks */
  ot->exec = add_primitive_plane_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_unit_props_size(ot);
  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_cube_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cube"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_cube matrix=%m4 size=%f calc_uvs=%b",
                                creation_data.mat,
                                RNA_float_get(op->ptr, "size"),
                                calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  /* BMESH_TODO make plane side this: M_SQRT2 - plane (diameter of 1.41 makes it unit size) */
  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cube_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cube";
  ot->description = "Construct a cube mesh";
  ot->idname = "MESH_OT_primitive_cube_add";

  /* api callbacks */
  ot->exec = add_primitive_cube_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_unit_props_size(ot);
  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static const EnumPropertyItem fill_type_items[] = {
    {0, "NOTHING", 0, "Nothing", "Don't fill at all"},
    {1, "NGON", 0, "N-Gon", "Use n-gons"},
    {2, "TRIFAN", 0, "Triangle Fan", "Use triangle fans"},
    {0, NULL, 0, NULL, NULL},
};

static int add_primitive_circle_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  int cap_end, cap_tri;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  cap_end = RNA_enum_get(op->ptr, "fill_type");
  cap_tri = (cap_end == 2);

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Circle"),
                          loc,
                          rot,
                          NULL,
                          local_view_bits,
                          &creation_data);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_circle segments=%i radius=%f cap_ends=%b cap_tris=%b matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "vertices"),
          RNA_float_get(op->ptr, "radius"),
          cap_end,
          cap_tri,
          creation_data.mat,
          calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_circle_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Circle";
  ot->description = "Construct a circle mesh";
  ot->idname = "MESH_OT_primitive_circle_add";

  /* api callbacks */
  ot->exec = add_primitive_circle_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "vertices", 32, 3, MESH_ADD_VERTS_MAXI, "Vertices", "", 3, 500);
  ED_object_add_unit_props_radius(ot);
  RNA_def_enum(ot->srna, "fill_type", fill_type_items, 0, "Fill Type", "");

  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_cylinder_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const int end_fill_type = RNA_enum_get(op->ptr, "end_fill_type");
  const bool cap_end = (end_fill_type != 0);
  const bool cap_tri = (end_fill_type == 2);
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cylinder"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b "
                                "cap_tris=%b depth=%f matrix=%m4 calc_uvs=%b",
                                RNA_int_get(op->ptr, "vertices"),
                                RNA_float_get(op->ptr, "radius"),
                                RNA_float_get(op->ptr, "radius"),
                                cap_end,
                                cap_tri,
                                RNA_float_get(op->ptr, "depth"),
                                creation_data.mat,
                                calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cylinder_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cylinder";
  ot->description = "Construct a cylinder mesh";
  ot->idname = "MESH_OT_primitive_cylinder_add";

  /* api callbacks */
  ot->exec = add_primitive_cylinder_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "vertices", 32, 3, MESH_ADD_VERTS_MAXI, "Vertices", "", 3, 500);
  ED_object_add_unit_props_radius(ot);
  RNA_def_float_distance(
      ot->srna, "depth", 2.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Depth", "", 0.001, 100.00);
  RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Cap Fill Type", "");

  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_cone_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const int end_fill_type = RNA_enum_get(op->ptr, "end_fill_type");
  const bool cap_end = (end_fill_type != 0);
  const bool cap_tri = (end_fill_type == 2);
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cone"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_cone segments=%i diameter1=%f diameter2=%f cap_ends=%b "
                                "cap_tris=%b depth=%f matrix=%m4 calc_uvs=%b",
                                RNA_int_get(op->ptr, "vertices"),
                                RNA_float_get(op->ptr, "radius1"),
                                RNA_float_get(op->ptr, "radius2"),
                                cap_end,
                                cap_tri,
                                RNA_float_get(op->ptr, "depth"),
                                creation_data.mat,
                                calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cone_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cone";
  ot->description = "Construct a conic mesh";
  ot->idname = "MESH_OT_primitive_cone_add";

  /* api callbacks */
  ot->exec = add_primitive_cone_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "vertices", 32, 3, MESH_ADD_VERTS_MAXI, "Vertices", "", 3, 500);
  RNA_def_float_distance(
      ot->srna, "radius1", 1.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius 1", "", 0.001, 100.00);
  RNA_def_float_distance(
      ot->srna, "radius2", 0.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius 2", "", 0.0, 100.00);
  RNA_def_float_distance(
      ot->srna, "depth", 2.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Depth", "", 0.001, 100.00);
  RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Base Fill Type", "");

  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_grid_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Grid"),
                          loc,
                          rot,
                          NULL,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_grid x_segments=%i y_segments=%i size=%f matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "x_subdivisions"),
          RNA_int_get(op->ptr, "y_subdivisions"),
          RNA_float_get(op->ptr, "size") / 2.0f,
          creation_data.mat,
          calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_grid_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Grid";
  ot->description = "Construct a grid mesh";
  ot->idname = "MESH_OT_primitive_grid_add";

  /* api callbacks */
  ot->exec = add_primitive_grid_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* Note that if you use MESH_ADD_VERTS_MAXI for both x and y at the same time
   * you will still reach impossible values (10^12 vertices or so...). */
  RNA_def_int(
      ot->srna, "x_subdivisions", 10, 2, MESH_ADD_VERTS_MAXI, "X Subdivisions", "", 2, 1000);
  RNA_def_int(
      ot->srna, "y_subdivisions", 10, 2, MESH_ADD_VERTS_MAXI, "Y Subdivisions", "", 2, 1000);

  ED_object_add_unit_props_size(ot);
  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_monkey_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  float dia;
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Y', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL);

  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Suzanne"),
                          loc,
                          rot,
                          NULL,
                          local_view_bits,
                          &creation_data);
  dia = RNA_float_get(op->ptr, "size") / 2.0f;
  mul_mat3_m4_fl(creation_data.mat, dia);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_monkey matrix=%m4 calc_uvs=%b",
                                creation_data.mat,
                                calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_monkey_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Monkey";
  ot->description = "Construct a Suzanne mesh";
  ot->idname = "MESH_OT_primitive_monkey_add";

  /* api callbacks */
  ot->exec = add_primitive_monkey_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  ED_object_add_unit_props_size(ot);
  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_uvsphere_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Sphere"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_uvsphere u_segments=%i v_segments=%i diameter=%f matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "segments"),
          RNA_int_get(op->ptr, "ring_count"),
          RNA_float_get(op->ptr, "radius"),
          creation_data.mat,
          calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_uv_sphere_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add UV Sphere";
  ot->description = "Construct a UV sphere mesh";
  ot->idname = "MESH_OT_primitive_uv_sphere_add";

  /* api callbacks */
  ot->exec = add_primitive_uvsphere_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "segments", 32, 3, MESH_ADD_VERTS_MAXI / 100, "Segments", "", 3, 500);
  RNA_def_int(ot->srna, "ring_count", 16, 3, MESH_ADD_VERTS_MAXI / 100, "Rings", "", 3, 500);

  ED_object_add_unit_props_radius(ot);
  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

static int add_primitive_icosphere_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  ED_object_add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, NULL);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Icosphere"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_texture_ensure(obedit->data, NULL);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_icosphere subdivisions=%i diameter=%f matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "subdivisions"),
          RNA_float_get(op->ptr, "radius"),
          creation_data.mat,
          calc_uvs)) {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_ico_sphere_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Ico Sphere";
  ot->description = "Construct an Icosphere mesh";
  ot->idname = "MESH_OT_primitive_ico_sphere_add";

  /* api callbacks */
  ot->exec = add_primitive_icosphere_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "subdivisions", 2, 1, 10, "Subdivisions", "", 1, 8);

  ED_object_add_unit_props_radius(ot);
  ED_object_add_mesh_props(ot);
  ED_object_add_generic_props(ot, true);
}

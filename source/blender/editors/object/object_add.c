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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edobj
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_fluidsim_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_action.h"
#include "BKE_anim_data.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_displist.h"
#include "BKE_duplilist.h"
#include "BKE_effect.h"
#include "BKE_font.h"
#include "BKE_gpencil_curve.h"
#include "BKE_gpencil_geom.h"
#include "BKE_hair.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_light.h"
#include "BKE_lightprobe.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
#include "BKE_nla.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcloud.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_speaker.h"
#include "BKE_volume.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_gpencil.h"
#include "ED_mball.h"
#include "ED_mesh.h"
#include "ED_node.h"
#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_physics.h"
#include "ED_render.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "UI_resources.h"

#include "object_intern.h"

/* -------------------------------------------------------------------- */
/** \name Local Enum Declarations
 * \{ */

/* this is an exact copy of the define in rna_light.c
 * kept here because of linking order.
 * Icons are only defined here */
const EnumPropertyItem rna_enum_light_type_items[] = {
    {LA_LOCAL, "POINT", ICON_LIGHT_POINT, "Point", "Omnidirectional point light source"},
    {LA_SUN, "SUN", ICON_LIGHT_SUN, "Sun", "Constant direction parallel ray light source"},
    {LA_SPOT, "SPOT", ICON_LIGHT_SPOT, "Spot", "Directional cone light source"},
    {LA_AREA, "AREA", ICON_LIGHT_AREA, "Area", "Directional area light source"},
    {0, NULL, 0, NULL, NULL},
};

/* copy from rna_object_force.c */
static const EnumPropertyItem field_type_items[] = {
    {PFIELD_FORCE, "FORCE", ICON_FORCE_FORCE, "Force", ""},
    {PFIELD_WIND, "WIND", ICON_FORCE_WIND, "Wind", ""},
    {PFIELD_VORTEX, "VORTEX", ICON_FORCE_VORTEX, "Vortex", ""},
    {PFIELD_MAGNET, "MAGNET", ICON_FORCE_MAGNETIC, "Magnetic", ""},
    {PFIELD_HARMONIC, "HARMONIC", ICON_FORCE_HARMONIC, "Harmonic", ""},
    {PFIELD_CHARGE, "CHARGE", ICON_FORCE_CHARGE, "Charge", ""},
    {PFIELD_LENNARDJ, "LENNARDJ", ICON_FORCE_LENNARDJONES, "Lennard-Jones", ""},
    {PFIELD_TEXTURE, "TEXTURE", ICON_FORCE_TEXTURE, "Texture", ""},
    {PFIELD_GUIDE, "GUIDE", ICON_FORCE_CURVE, "Curve Guide", ""},
    {PFIELD_BOID, "BOID", ICON_FORCE_BOID, "Boid", ""},
    {PFIELD_TURBULENCE, "TURBULENCE", ICON_FORCE_TURBULENCE, "Turbulence", ""},
    {PFIELD_DRAG, "DRAG", ICON_FORCE_DRAG, "Drag", ""},
    {PFIELD_FLUIDFLOW, "FLUID", ICON_FORCE_FLUIDFLOW, "Fluid Flow", ""},
    {0, NULL, 0, NULL, NULL},
};

static EnumPropertyItem lightprobe_type_items[] = {
    {LIGHTPROBE_TYPE_CUBE,
     "CUBEMAP",
     ICON_LIGHTPROBE_CUBEMAP,
     "Reflection Cubemap",
     "Reflection probe with spherical or cubic attenuation"},
    {LIGHTPROBE_TYPE_PLANAR,
     "PLANAR",
     ICON_LIGHTPROBE_PLANAR,
     "Reflection Plane",
     "Planar reflection probe"},
    {LIGHTPROBE_TYPE_GRID,
     "GRID",
     ICON_LIGHTPROBE_GRID,
     "Irradiance Volume",
     "Irradiance probe to capture diffuse indirect lighting"},
    {0, NULL, 0, NULL, NULL},
};

enum {
  ALIGN_WORLD = 0,
  ALIGN_VIEW,
  ALIGN_CURSOR,
};

static const EnumPropertyItem align_options[] = {
    {ALIGN_WORLD, "WORLD", 0, "World", "Align the new object to the world"},
    {ALIGN_VIEW, "VIEW", 0, "View", "Align the new object to the view"},
    {ALIGN_CURSOR, "CURSOR", 0, "3D Cursor", "Use the 3D cursor orientation for the new object"},
    {0, NULL, 0, NULL, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Add Object API
 *
 * \{ */

void ED_object_location_from_view(bContext *C, float loc[3])
{
  const Scene *scene = CTX_data_scene(C);
  copy_v3_v3(loc, scene->cursor.location);
}

void ED_object_rotation_from_quat(float rot[3], const float viewquat[4], const char align_axis)
{
  BLI_assert(align_axis >= 'X' && align_axis <= 'Z');

  switch (align_axis) {
    case 'X': {
      /* Same as 'rv3d->viewinv[1]' */
      float axis_y[4] = {0.0f, 1.0f, 0.0f};
      float quat_y[4], quat[4];
      axis_angle_to_quat(quat_y, axis_y, M_PI_2);
      mul_qt_qtqt(quat, viewquat, quat_y);
      quat_to_eul(rot, quat);
      break;
    }
    case 'Y': {
      quat_to_eul(rot, viewquat);
      rot[0] -= (float)M_PI_2;
      break;
    }
    case 'Z': {
      quat_to_eul(rot, viewquat);
      break;
    }
  }
}

void ED_object_rotation_from_view(bContext *C, float rot[3], const char align_axis)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BLI_assert(align_axis >= 'X' && align_axis <= 'Z');
  if (rv3d) {
    float viewquat[4];
    copy_qt_qt(viewquat, rv3d->viewquat);
    viewquat[0] *= -1.0f;
    ED_object_rotation_from_quat(rot, viewquat, align_axis);
  }
  else {
    zero_v3(rot);
  }
}

void ED_object_base_init_transform_on_add(Object *object, const float loc[3], const float rot[3])
{
  if (loc) {
    copy_v3_v3(object->loc, loc);
  }

  if (rot) {
    copy_v3_v3(object->rot, rot);
  }

  BKE_object_to_mat4(object, object->obmat);
}

/* Uses context to figure out transform for primitive.
 * Returns standard diameter. */
float ED_object_new_primitive_matrix(
    bContext *C, Object *obedit, const float loc[3], const float rot[3], float primmat[4][4])
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  float mat[3][3], rmat[3][3], cmat[3][3], imat[3][3];

  unit_m4(primmat);

  eul_to_mat3(rmat, rot);
  invert_m3(rmat);

  /* inverse transform for initial rotation and object */
  copy_m3_m4(mat, obedit->obmat);
  mul_m3_m3m3(cmat, rmat, mat);
  invert_m3_m3(imat, cmat);
  copy_m4_m3(primmat, imat);

  /* center */
  copy_v3_v3(primmat[3], loc);
  sub_v3_v3v3(primmat[3], primmat[3], obedit->obmat[3]);
  invert_m3_m3(imat, mat);
  mul_m3_v3(imat, primmat[3]);

  {
    const float dia = v3d ? ED_view3d_grid_scale(scene, v3d, NULL) :
                            ED_scene_grid_scale(scene, NULL);
    return dia;
  }

  // return 1.0f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Object Operator
 * \{ */

static void view_align_update(struct Main *UNUSED(main),
                              struct Scene *UNUSED(scene),
                              struct PointerRNA *ptr)
{
  RNA_struct_idprops_unset(ptr, "rotation");
}

void ED_object_add_unit_props_size(wmOperatorType *ot)
{
  RNA_def_float_distance(
      ot->srna, "size", 2.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Size", "", 0.001, 100.00);
}

void ED_object_add_unit_props_radius_ex(wmOperatorType *ot, float default_value)
{
  RNA_def_float_distance(
      ot->srna, "radius", default_value, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius", "", 0.001, 100.00);
}

void ED_object_add_unit_props_radius(wmOperatorType *ot)
{
  ED_object_add_unit_props_radius_ex(ot, 1.0f);
}

void ED_object_add_generic_props(wmOperatorType *ot, bool do_editmode)
{
  PropertyRNA *prop;

  if (do_editmode) {
    prop = RNA_def_boolean(
        ot->srna, "enter_editmode", 0, "Enter Editmode", "Enter editmode when adding this object");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }
  /* note: this property gets hidden for add-camera operator */
  prop = RNA_def_enum(
      ot->srna, "align", align_options, ALIGN_WORLD, "Align", "The alignment of the new object");
  RNA_def_property_update_runtime(prop, view_align_update);

  prop = RNA_def_float_vector_xyz(ot->srna,
                                  "location",
                                  3,
                                  NULL,
                                  -OBJECT_ADD_SIZE_MAXF,
                                  OBJECT_ADD_SIZE_MAXF,
                                  "Location",
                                  "Location for the newly added object",
                                  -1000.0f,
                                  1000.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  prop = RNA_def_float_rotation(ot->srna,
                                "rotation",
                                3,
                                NULL,
                                -OBJECT_ADD_SIZE_MAXF,
                                OBJECT_ADD_SIZE_MAXF,
                                "Rotation",
                                "Rotation for the newly added object",
                                DEG2RADF(-360.0f),
                                DEG2RADF(360.0f));
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_float_vector_xyz(ot->srna,
                                  "scale",
                                  3,
                                  NULL,
                                  -OBJECT_ADD_SIZE_MAXF,
                                  OBJECT_ADD_SIZE_MAXF,
                                  "Scale",
                                  "Scale for the newly added object",
                                  -1000.0f,
                                  1000.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void ED_object_add_mesh_props(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna, "calc_uvs", true, "Generate UVs", "Generate a default UV map");
}

bool ED_object_add_generic_get_opts(bContext *C,
                                    wmOperator *op,
                                    const char view_align_axis,
                                    float loc[3],
                                    float rot[3],
                                    float scale[3],
                                    bool *enter_editmode,
                                    ushort *local_view_bits,
                                    bool *is_view_aligned)
{
  PropertyRNA *prop;

  /* Switch to Edit mode? optional prop */
  if ((prop = RNA_struct_find_property(op->ptr, "enter_editmode"))) {
    bool _enter_editmode;
    if (!enter_editmode) {
      enter_editmode = &_enter_editmode;
    }

    if (RNA_property_is_set(op->ptr, prop) && enter_editmode) {
      *enter_editmode = RNA_property_boolean_get(op->ptr, prop);
    }
    else {
      *enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
      RNA_property_boolean_set(op->ptr, prop, *enter_editmode);
    }
  }

  if (local_view_bits) {
    View3D *v3d = CTX_wm_view3d(C);
    if (v3d && v3d->localvd) {
      *local_view_bits = v3d->local_view_uuid;
    }
  }

  /* Location! */
  {
    float _loc[3];
    if (!loc) {
      loc = _loc;
    }

    if (RNA_struct_property_is_set(op->ptr, "location")) {
      RNA_float_get_array(op->ptr, "location", loc);
    }
    else {
      ED_object_location_from_view(C, loc);
      RNA_float_set_array(op->ptr, "location", loc);
    }
  }

  /* Rotation! */
  {
    bool _is_view_aligned;
    float _rot[3];
    if (!is_view_aligned) {
      is_view_aligned = &_is_view_aligned;
    }
    if (!rot) {
      rot = _rot;
    }

    if (RNA_struct_property_is_set(op->ptr, "rotation")) {
      /* If rotation is set, always use it. Alignment (and corresponding user preference)
       * can be ignored since this is in world space anyways.
       * To not confuse (e.g. on redo), dont set it to ALIGN_WORLD in the op UI though. */
      *is_view_aligned = false;
      RNA_float_get_array(op->ptr, "rotation", rot);
    }
    else {
      int alignment = ALIGN_WORLD;
      prop = RNA_struct_find_property(op->ptr, "align");

      if (RNA_property_is_set(op->ptr, prop)) {
        /* If alignment is set, always use it. */
        *is_view_aligned = alignment == ALIGN_VIEW;
        alignment = RNA_property_enum_get(op->ptr, prop);
      }
      else {
        /* If alignment is not set, use User Preferences. */
        *is_view_aligned = (U.flag & USER_ADD_VIEWALIGNED) != 0;
        if (*is_view_aligned) {
          RNA_property_enum_set(op->ptr, prop, ALIGN_VIEW);
          alignment = ALIGN_VIEW;
        }
        else if ((U.flag & USER_ADD_CURSORALIGNED) != 0) {
          RNA_property_enum_set(op->ptr, prop, ALIGN_CURSOR);
          alignment = ALIGN_CURSOR;
        }
        else {
          RNA_property_enum_set(op->ptr, prop, ALIGN_WORLD);
          alignment = ALIGN_WORLD;
        }
      }
      switch (alignment) {
        case ALIGN_WORLD:
          RNA_float_get_array(op->ptr, "rotation", rot);
          break;
        case ALIGN_VIEW:
          ED_object_rotation_from_view(C, rot, view_align_axis);
          RNA_float_set_array(op->ptr, "rotation", rot);
          break;
        case ALIGN_CURSOR: {
          const Scene *scene = CTX_data_scene(C);
          float tmat[3][3];
          BKE_scene_cursor_rot_to_mat3(&scene->cursor, tmat);
          mat3_normalized_to_eul(rot, tmat);
          RNA_float_set_array(op->ptr, "rotation", rot);
          break;
        }
      }
    }
  }

  /* Scale! */
  {
    float _scale[3];
    if (!scale) {
      scale = _scale;
    }

    /* For now this is optional, we can make it always use. */
    copy_v3_fl(scale, 1.0f);
    if ((prop = RNA_struct_find_property(op->ptr, "scale"))) {
      if (RNA_property_is_set(op->ptr, prop)) {
        RNA_property_float_get_array(op->ptr, prop, scale);
      }
      else {
        copy_v3_fl(scale, 1.0f);
        RNA_property_float_set_array(op->ptr, prop, scale);
      }
    }
  }

  return true;
}

/* For object add primitive operators.
 * Do not call undo push in this function (users of this function have to). */
Object *ED_object_add_type(bContext *C,
                           int type,
                           const char *name,
                           const float loc[3],
                           const float rot[3],
                           bool enter_editmode,
                           ushort local_view_bits)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob;

  /* for as long scene has editmode... */
  if (CTX_data_edit_object(C)) {
    ED_object_editmode_exit(C, EM_FREEDATA);
  }

  /* deselects all, sets active object */
  ob = BKE_object_add(bmain, scene, view_layer, type, name);
  BASACT(view_layer)->local_view_bits = local_view_bits;
  /* editor level activate, notifiers */
  ED_object_base_activate(C, view_layer->basact);

  /* more editor stuff */
  ED_object_base_init_transform_on_add(ob, loc, rot);

  /* TODO(sergey): This is weird to manually tag objects for update, better to
   * use DEG_id_tag_update here perhaps.
   */
  DEG_id_type_tag(bmain, ID_OB);
  DEG_relations_tag_update(bmain);
  if (ob->data != NULL) {
    DEG_id_tag_update_ex(bmain, (ID *)ob->data, ID_RECALC_EDITORS);
  }

  if (enter_editmode) {
    ED_object_editmode_enter_ex(bmain, scene, ob, 0);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  /* TODO(sergey): Use proper flag for tagging here. */
  DEG_id_tag_update(&scene->id, 0);

  ED_outliner_select_sync_from_object_tag(C);

  return ob;
}

/* for object add operator */
static int object_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3], radius;

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  radius = RNA_float_get(op->ptr, "radius");
  ob = ED_object_add_type(
      C, RNA_enum_get(op->ptr, "type"), NULL, loc, rot, enter_editmode, local_view_bits);

  if (ob->type == OB_LATTICE) {
    /* lattice is a special case!
     * we never want to scale the obdata since that is the rest-state */
    copy_v3_fl(ob->scale, radius);
  }
  else {
    BKE_object_obdata_size_init(ob, radius);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Object";
  ot->description = "Add an object to the scene";
  ot->idname = "OBJECT_OT_add";

  /* api callbacks */
  ot->exec = object_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ED_object_add_unit_props_radius(ot);
  PropertyRNA *prop = RNA_def_enum(ot->srna, "type", rna_enum_object_type_items, 0, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Probe Operator
 * \{ */

/* for object add operator */
static const char *get_lightprobe_defname(int type)
{
  switch (type) {
    case LIGHTPROBE_TYPE_GRID:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "IrradianceVolume");
    case LIGHTPROBE_TYPE_PLANAR:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "ReflectionPlane");
    case LIGHTPROBE_TYPE_CUBE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "ReflectionCubemap");
    default:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "LightProbe");
  }
}

static int lightprobe_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  LightProbe *probe;
  int type;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];
  float radius;

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  type = RNA_enum_get(op->ptr, "type");
  radius = RNA_float_get(op->ptr, "radius");

  ob = ED_object_add_type(
      C, OB_LIGHTPROBE, get_lightprobe_defname(type), loc, rot, false, local_view_bits);
  copy_v3_fl(ob->scale, radius);

  probe = (LightProbe *)ob->data;

  BKE_lightprobe_type_set(probe, type);

  DEG_relations_tag_update(CTX_data_main(C));

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lightprobe_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Light Probe";
  ot->description = "Add a light probe object";
  ot->idname = "OBJECT_OT_lightprobe_add";

  /* api callbacks */
  ot->exec = lightprobe_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", lightprobe_type_items, 0, "Type", "");

  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Effector Operator
 * \{ */

/* for object add operator */
static int effector_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  int type;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];
  float mat[4][4];
  float dia;

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  type = RNA_enum_get(op->ptr, "type");
  dia = RNA_float_get(op->ptr, "radius");

  if (type == PFIELD_GUIDE) {
    Curve *cu;
    const char *name = CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "CurveGuide");
    ob = ED_object_add_type(C, OB_CURVE, name, loc, rot, false, local_view_bits);

    cu = ob->data;
    cu->flag |= CU_PATH | CU_3D;
    ED_object_editmode_enter(C, 0);
    ED_object_new_primitive_matrix(C, ob, loc, rot, mat);
    BLI_addtail(&cu->editnurb->nurbs,
                ED_curve_add_nurbs_primitive(C, ob, mat, CU_NURBS | CU_PRIM_PATH, dia));
    if (!enter_editmode) {
      ED_object_editmode_exit(C, EM_FREEDATA);
    }
  }
  else {
    const char *name = CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Field");
    ob = ED_object_add_type(C, OB_EMPTY, name, loc, rot, false, local_view_bits);
    BKE_object_obdata_size_init(ob, dia);
    if (ELEM(type, PFIELD_WIND, PFIELD_VORTEX)) {
      ob->empty_drawtype = OB_SINGLE_ARROW;
    }
  }

  ob->pd = BKE_partdeflect_new(type);

  DEG_relations_tag_update(CTX_data_main(C));

  return OPERATOR_FINISHED;
}

void OBJECT_OT_effector_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Effector";
  ot->description = "Add an empty object with a physics effector to the scene";
  ot->idname = "OBJECT_OT_effector_add";

  /* api callbacks */
  ot->exec = effector_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", field_type_items, 0, "Type", "");

  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Camera Operator
 * \{ */

static int object_camera_add_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob;
  Camera *cam;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];

  /* force view align for cameras */
  RNA_enum_set(op->ptr, "align", ALIGN_VIEW);

  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  ob = ED_object_add_type(C, OB_CAMERA, NULL, loc, rot, false, local_view_bits);

  if (v3d) {
    if (v3d->camera == NULL) {
      v3d->camera = ob;
    }
    if (v3d->scenelock && scene->camera == NULL) {
      scene->camera = ob;
    }
  }

  cam = ob->data;
  cam->drawsize = v3d ? ED_view3d_grid_scale(scene, v3d, NULL) : ED_scene_grid_scale(scene, NULL);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_camera_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Camera";
  ot->description = "Add a camera object to the scene";
  ot->idname = "OBJECT_OT_camera_add";

  /* api callbacks */
  ot->exec = object_camera_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_generic_props(ot, true);

  /* hide this for cameras, default */
  prop = RNA_struct_type_find_property(ot->srna, "align");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Metaball Operator
 * \{ */

static int object_metaball_add_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  bool newob = false;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];
  float mat[4][4];
  float dia;

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  if (obedit == NULL || obedit->type != OB_MBALL) {
    obedit = ED_object_add_type(C, OB_MBALL, NULL, loc, rot, true, local_view_bits);
    newob = true;
  }
  else {
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  ED_object_new_primitive_matrix(C, obedit, loc, rot, mat);
  /* Halving here is done to account for constant values from #BKE_mball_element_add.
   * While the default radius of the resulting meta element is 2,
   * we want to pass in 1 so other values such as resolution are scaled by 1.0. */
  dia = RNA_float_get(op->ptr, "radius") / 2;

  ED_mball_add_primitive(C, obedit, mat, dia, RNA_enum_get(op->ptr, "type"));

  /* userdef */
  if (newob && !enter_editmode) {
    ED_object_editmode_exit(C, EM_FREEDATA);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_metaball_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Metaball";
  ot->description = "Add an metaball object to the scene";
  ot->idname = "OBJECT_OT_metaball_add";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_metaball_add_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_metaelem_type_items, 0, "Primitive", "");

  ED_object_add_unit_props_radius_ex(ot, 2.0f);
  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Text Operator
 * \{ */

static int object_add_text_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  if (obedit && obedit->type == OB_FONT) {
    return OPERATOR_CANCELLED;
  }

  obedit = ED_object_add_type(C, OB_FONT, NULL, loc, rot, enter_editmode, local_view_bits);
  BKE_object_obdata_size_init(obedit, RNA_float_get(op->ptr, "radius"));

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_text_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Text";
  ot->description = "Add a text object to the scene";
  ot->idname = "OBJECT_OT_text_add";

  /* api callbacks */
  ot->exec = object_add_text_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Armature Operator
 * \{ */

static int object_armature_add_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  bool newob = false;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3], dia;
  bool view_aligned = rv3d && (U.flag & USER_ADD_VIEWALIGNED);

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(
          C, op, 'Z', loc, rot, NULL, &enter_editmode, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  if ((obedit == NULL) || (obedit->type != OB_ARMATURE)) {
    obedit = ED_object_add_type(C, OB_ARMATURE, NULL, loc, rot, true, local_view_bits);
    ED_object_editmode_enter(C, 0);
    newob = true;
  }
  else {
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  if (obedit == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Cannot create editmode armature");
    return OPERATOR_CANCELLED;
  }

  dia = RNA_float_get(op->ptr, "radius");
  ED_armature_ebone_add_primitive(obedit, dia, view_aligned);

  /* userdef */
  if (newob && !enter_editmode) {
    ED_object_editmode_exit(C, EM_FREEDATA);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_armature_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Armature";
  ot->description = "Add an armature object to the scene";
  ot->idname = "OBJECT_OT_armature_add";

  /* api callbacks */
  ot->exec = object_armature_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Empty Operator
 * \{ */

static int object_empty_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  int type = RNA_enum_get(op->ptr, "type");
  ushort local_view_bits;
  float loc[3], rot[3];

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  ob = ED_object_add_type(C, OB_EMPTY, NULL, loc, rot, false, local_view_bits);

  BKE_object_empty_draw_type_set(ob, type);
  BKE_object_obdata_size_init(ob, RNA_float_get(op->ptr, "radius"));

  return OPERATOR_FINISHED;
}

void OBJECT_OT_empty_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Empty";
  ot->description = "Add an empty object to the scene";
  ot->idname = "OBJECT_OT_empty_add";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_empty_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_empty_drawtype_items, 0, "Type", "");

  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, false);
}

static int empty_drop_named_image_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Scene *scene = CTX_data_scene(C);

  Image *ima = NULL;

  ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
  if (!ima) {
    return OPERATOR_CANCELLED;
  }
  /* handled below */
  id_us_min(&ima->id);

  Object *ob = NULL;
  Object *ob_cursor = ED_view3d_give_object_under_cursor(C, event->mval);

  /* either change empty under cursor or create a new empty */
  if (ob_cursor && ob_cursor->type == OB_EMPTY) {
    WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
    DEG_id_tag_update((ID *)ob_cursor, ID_RECALC_TRANSFORM);
    ob = ob_cursor;
  }
  else {
    /* add new empty */
    ushort local_view_bits;
    float rot[3];

    if (!ED_object_add_generic_get_opts(
            C, op, 'Z', NULL, rot, NULL, NULL, &local_view_bits, NULL)) {
      return OPERATOR_CANCELLED;
    }
    ob = ED_object_add_type(C, OB_EMPTY, NULL, NULL, rot, false, local_view_bits);

    ED_object_location_from_view(C, ob->loc);
    ED_view3d_cursor3d_position(C, event->mval, false, ob->loc);
    ED_object_rotation_from_view(C, ob->rot, 'Z');
    ob->empty_drawsize = 5.0f;
  }

  BKE_object_empty_draw_type_set(ob, OB_EMPTY_IMAGE);

  id_us_min(ob->data);
  ob->data = ima;
  id_us_plus(ob->data);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_drop_named_image(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Empty Image/Drop Image to Empty";
  ot->description = "Add an empty image type to scene with data";
  ot->idname = "OBJECT_OT_drop_named_image";

  /* api callbacks */
  ot->invoke = empty_drop_named_image_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_string(ot->srna, "filepath", NULL, FILE_MAX, "Filepath", "Path to image file");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  RNA_def_boolean(ot->srna,
                  "relative_path",
                  true,
                  "Relative Path",
                  "Select the file relative to the blend file");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_string(ot->srna, "name", NULL, MAX_ID_NAME - 2, "Name", "Image name to assign");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  ED_object_add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Gpencil Operator
 * \{ */

static bool object_gpencil_add_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Object *obact = CTX_data_active_object(C);

  if ((scene == NULL) || (ID_IS_LINKED(scene))) {
    return false;
  }

  if (obact && obact->type == OB_GPENCIL) {
    if (obact->mode != OB_MODE_OBJECT) {
      return false;
    }
  }

  return true;
}

static int object_gpencil_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (ob && (ob->type == OB_GPENCIL)) ? ob->data : NULL;

  const int type = RNA_enum_get(op->ptr, "type");

  ushort local_view_bits;
  float loc[3], rot[3];
  bool newob = false;

  /* Note: We use 'Y' here (not 'Z'), as */
  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(C, op, 'Y', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  /* add new object if not currently editing a GP object,
   * or if "empty" was chosen (i.e. user wants a blank GP canvas)
   */
  if ((gpd == NULL) || (GPENCIL_ANY_MODE(gpd) == false) || (type == GP_EMPTY)) {
    const char *ob_name = NULL;
    switch (type) {
      case GP_MONKEY: {
        ob_name = "Suzanne";
        break;
      }
      case GP_STROKE: {
        ob_name = "Stroke";
        break;
      }
      default: {
        break;
      }
    }

    ob = ED_object_add_type(C, OB_GPENCIL, ob_name, loc, rot, true, local_view_bits);
    gpd = ob->data;
    newob = true;
  }
  else {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_ADDED, NULL);
  }

  /* create relevant geometry */
  switch (type) {
    case GP_STROKE: {
      float radius = RNA_float_get(op->ptr, "radius");
      float mat[4][4];

      ED_object_new_primitive_matrix(C, ob, loc, rot, mat);
      mul_v3_fl(mat[0], radius);
      mul_v3_fl(mat[1], radius);
      mul_v3_fl(mat[2], radius);

      ED_gpencil_create_stroke(C, ob, mat);
      break;
    }
    case GP_MONKEY: {
      float radius = RNA_float_get(op->ptr, "radius");
      float mat[4][4];

      ED_object_new_primitive_matrix(C, ob, loc, rot, mat);
      mul_v3_fl(mat[0], radius);
      mul_v3_fl(mat[1], radius);
      mul_v3_fl(mat[2], radius);

      ED_gpencil_create_monkey(C, ob, mat);
      break;
    }
    case GP_EMPTY:
      /* do nothing */
      break;

    default:
      BKE_report(op->reports, RPT_WARNING, "Not implemented");
      break;
  }

  /* if this is a new object, initialise default stuff (colors, etc.) */
  if (newob) {
    /* set default viewport color to black */
    copy_v3_fl(ob->color, 0.0f);

    ED_gpencil_add_defaults(C, ob);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_gpencil_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Grease Pencil";
  ot->description = "Add a Grease Pencil object to the scene";
  ot->idname = "OBJECT_OT_gpencil_add";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_gpencil_add_exec;
  ot->poll = object_gpencil_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, false);

  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_gpencil_type_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Light Operator
 * \{ */

static const char *get_light_defname(int type)
{
  switch (type) {
    case LA_LOCAL:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Point");
    case LA_SUN:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Sun");
    case LA_SPOT:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Spot");
    case LA_AREA:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Area");
    default:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Light");
  }
}

static int object_light_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  Light *la;
  int type = RNA_enum_get(op->ptr, "type");
  ushort local_view_bits;
  float loc[3], rot[3];

  WM_operator_view3d_unit_defaults(C, op);
  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  ob = ED_object_add_type(C, OB_LAMP, get_light_defname(type), loc, rot, false, local_view_bits);

  float size = RNA_float_get(op->ptr, "radius");
  /* Better defaults for light size. */
  switch (type) {
    case LA_LOCAL:
    case LA_SPOT:
      break;
    case LA_AREA:
      size *= 4.0f;
      break;
    default:
      size *= 0.5f;
      break;
  }
  BKE_object_obdata_size_init(ob, size);

  la = (Light *)ob->data;
  la->type = type;

  if (type == LA_SUN) {
    la->energy = 1.0f;
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_light_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Light";
  ot->description = "Add a light object to the scene";
  ot->idname = "OBJECT_OT_light_add";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_light_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_light_type_items, 0, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_LIGHT);

  ED_object_add_unit_props_radius(ot);
  ED_object_add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Collection Instance Operator
 * \{ */

static int collection_instance_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Collection *collection;
  ushort local_view_bits;
  float loc[3], rot[3];

  if (RNA_struct_property_is_set(op->ptr, "name")) {
    char name[MAX_ID_NAME - 2];

    RNA_string_get(op->ptr, "name", name);
    collection = (Collection *)BKE_libblock_find_name(bmain, ID_GR, name);

    if (0 == RNA_struct_property_is_set(op->ptr, "location")) {
      const wmEvent *event = CTX_wm_window(C)->eventstate;
      ARegion *region = CTX_wm_region(C);
      const int mval[2] = {event->x - region->winrct.xmin, event->y - region->winrct.ymin};
      ED_object_location_from_view(C, loc);
      ED_view3d_cursor3d_position(C, mval, false, loc);
      RNA_float_set_array(op->ptr, "location", loc);
    }
  }
  else {
    collection = BLI_findlink(&bmain->collections, RNA_enum_get(op->ptr, "collection"));
  }

  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  if (collection) {
    Scene *scene = CTX_data_scene(C);
    ViewLayer *view_layer = CTX_data_view_layer(C);

    /* Avoid dependency cycles. */
    LayerCollection *active_lc = BKE_layer_collection_get_active(view_layer);
    while (BKE_collection_find_cycle(active_lc->collection, collection)) {
      active_lc = BKE_layer_collection_activate_parent(view_layer, active_lc);
    }

    Object *ob = ED_object_add_type(
        C, OB_EMPTY, collection->id.name + 2, loc, rot, false, local_view_bits);
    ob->instance_collection = collection;
    ob->empty_drawsize = U.collection_instance_empty_size;
    ob->transflag |= OB_DUPLICOLLECTION;
    id_us_plus(&collection->id);

    /* works without this except if you try render right after, see: 22027 */
    DEG_relations_tag_update(bmain);
    DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
    WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);

    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

/* only used as menu */
void OBJECT_OT_collection_instance_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Collection Instance";
  ot->description = "Add a collection instance";
  ot->idname = "OBJECT_OT_collection_instance_add";

  /* api callbacks */
  ot->invoke = WM_enum_search_invoke;
  ot->exec = collection_instance_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(
      ot->srna, "name", "Collection", MAX_ID_NAME - 2, "Name", "Collection name to add");
  prop = RNA_def_enum(ot->srna, "collection", DummyRNA_NULL_items, 0, "Collection", "");
  RNA_def_enum_funcs(prop, RNA_collection_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
  ED_object_add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Speaker Operator
 * \{ */

static int object_speaker_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob;
  ushort local_view_bits;
  float loc[3], rot[3];
  Scene *scene = CTX_data_scene(C);

  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  ob = ED_object_add_type(C, OB_SPEAKER, NULL, loc, rot, false, local_view_bits);

  /* to make it easier to start using this immediately in NLA, a default sound clip is created
   * ready to be moved around to retime the sound and/or make new sound clips
   */
  {
    /* create new data for NLA hierarchy */
    AnimData *adt = BKE_animdata_add_id(&ob->id);
    NlaTrack *nlt = BKE_nlatrack_add(adt, NULL);
    NlaStrip *strip = BKE_nla_add_soundstrip(bmain, scene, ob->data);
    strip->start = CFRA;
    strip->end += strip->start;

    /* hook them up */
    BKE_nlatrack_add_strip(nlt, strip);

    /* auto-name the strip, and give the track an interesting name  */
    BLI_strncpy(nlt->name, DATA_("SoundTrack"), sizeof(nlt->name));
    BKE_nlastrip_validate_name(adt, strip);

    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_EDITED, NULL);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_speaker_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Speaker";
  ot->description = "Add a speaker object to the scene";
  ot->idname = "OBJECT_OT_speaker_add";

  /* api callbacks */
  ot->exec = object_speaker_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Hair Operator
 * \{ */

static int object_hair_add_exec(bContext *C, wmOperator *op)
{
  ushort local_view_bits;
  float loc[3], rot[3];

  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  Object *object = ED_object_add_type(C, OB_HAIR, NULL, loc, rot, false, local_view_bits);
  object->dtx |= OB_DRAWBOUNDOX; /* TODO: remove once there is actual drawing. */

  return OPERATOR_FINISHED;
}

void OBJECT_OT_hair_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Hair";
  ot->description = "Add a hair object to the scene";
  ot->idname = "OBJECT_OT_hair_add";

  /* api callbacks */
  ot->exec = object_hair_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Point Cloud Operator
 * \{ */

static int object_pointcloud_add_exec(bContext *C, wmOperator *op)
{
  ushort local_view_bits;
  float loc[3], rot[3];

  if (!ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, NULL, NULL, &local_view_bits, NULL)) {
    return OPERATOR_CANCELLED;
  }
  Object *object = ED_object_add_type(C, OB_POINTCLOUD, NULL, loc, rot, false, local_view_bits);
  object->dtx |= OB_DRAWBOUNDOX; /* TODO: remove once there is actual drawing. */

  return OPERATOR_FINISHED;
}

void OBJECT_OT_pointcloud_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Point Cloud";
  ot->description = "Add a point cloud object to the scene";
  ot->idname = "OBJECT_OT_pointcloud_add";

  /* api callbacks */
  ot->exec = object_pointcloud_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ED_object_add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Object Operator
 * \{ */
/* remove base from a specific scene */
/* note: now unlinks constraints as well */
void ED_object_base_free_and_unlink(Main *bmain, Scene *scene, Object *ob)
{
  if (BKE_library_ID_is_indirectly_used(bmain, ob) && ID_REAL_USERS(ob) <= 1 &&
      ID_EXTRA_USERS(ob) == 0) {
    /* We cannot delete indirectly used object... */
    printf(
        "WARNING, undeletable object '%s', should have been catched before reaching this "
        "function!",
        ob->id.name + 2);
    return;
  }

  DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_BASE_FLAGS);

  BKE_scene_collections_object_remove(bmain, scene, ob, true);
}

static int object_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win;
  const bool use_global = RNA_boolean_get(op->ptr, "use_global");
  uint changed_count = 0;

  if (CTX_data_edit_object(C)) {
    return OPERATOR_CANCELLED;
  }

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    const bool is_indirectly_used = BKE_library_ID_is_indirectly_used(bmain, ob);
    if (ob->id.tag & LIB_TAG_INDIRECT) {
      /* Can this case ever happen? */
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Cannot delete indirectly linked object '%s'",
                  ob->id.name + 2);
      continue;
    }
    else if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Cannot delete object '%s' from scene '%s', indirectly used objects need at "
                  "least one user",
                  ob->id.name + 2,
                  scene->id.name + 2);
      continue;
    }

    /* if grease pencil object, set cache as dirty */
    if (ob->type == OB_GPENCIL) {
      bGPdata *gpd = (bGPdata *)ob->data;
      DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    }

    /* This is sort of a quick hack to address T51243 -
     * Proper thing to do here would be to nuke most of all this custom scene/object/base handling,
     * and use generic lib remap/query for that.
     * But this is for later (aka 2.8, once layers & co are settled and working).
     */
    if (use_global && ob->id.lib == NULL) {
      /* We want to nuke the object, let's nuke it the easy way (not for linked data though)... */
      BKE_id_delete(bmain, &ob->id);
      changed_count += 1;
      continue;
    }

    /* remove from Grease Pencil parent */
    /* XXX This is likely not correct?
     *     Will also remove parent from grease pencil from other scenes,
     *     even when use_global is false... */
    for (bGPdata *gpd = bmain->gpencils.first; gpd; gpd = gpd->id.next) {
      LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
        if (gpl->parent != NULL) {
          if (gpl->parent == ob) {
            gpl->parent = NULL;
          }
        }
      }
    }

    /* remove from current scene only */
    ED_object_base_free_and_unlink(bmain, scene, ob);
    changed_count += 1;

    if (use_global) {
      Scene *scene_iter;
      for (scene_iter = bmain->scenes.first; scene_iter; scene_iter = scene_iter->id.next) {
        if (scene_iter != scene && !ID_IS_LINKED(scene_iter)) {
          if (is_indirectly_used && ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0) {
            BKE_reportf(op->reports,
                        RPT_WARNING,
                        "Cannot delete object '%s' from scene '%s', indirectly used objects need "
                        "at least one user",
                        ob->id.name + 2,
                        scene_iter->id.name + 2);
            break;
          }
          ED_object_base_free_and_unlink(bmain, scene_iter, ob);
        }
      }
    }
    /* end global */
  }
  CTX_DATA_END;

  BKE_reportf(op->reports, RPT_INFO, "Deleted %u object(s)", changed_count);

  if (changed_count == 0) {
    return OPERATOR_CANCELLED;
  }

  /* delete has to handle all open scenes */
  BKE_main_id_tag_listbase(&bmain->scenes, LIB_TAG_DOIT, true);
  for (win = wm->windows.first; win; win = win->next) {
    scene = WM_window_get_active_scene(win);

    if (scene->id.tag & LIB_TAG_DOIT) {
      scene->id.tag &= ~LIB_TAG_DOIT;

      DEG_relations_tag_update(bmain);

      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
      WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
    }
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete selected objects";
  ot->idname = "OBJECT_OT_delete";

  /* api callbacks */
  ot->invoke = WM_operator_confirm_or_exec;
  ot->exec = object_delete_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_boolean(
      ot->srna, "use_global", 0, "Delete Globally", "Remove object from all scenes");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  WM_operator_properties_confirm_or_exec(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy Object Utilities
 * \{ */

/* after copying objects, copied data should get new pointers */
static void copy_object_set_idnew(bContext *C)
{
  Main *bmain = CTX_data_main(C);

  CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects) {
    BKE_libblock_relink_to_newid(&ob->id);
  }
  CTX_DATA_END;

  BKE_main_id_clear_newpoins(bmain);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Instanced Objects Real Operator
 * \{ */

/* XXX TODO That whole hierarchy handling based on persistent_id tricks is
 * very confusing and convoluted, and it will fail in many cases besides basic ones.
 * Think this should be replaced by a proper tree-like representation of the instantiations,
 * should help a lot in both readability, and precise consistent rebuilding of hierarchy.
 */

/**
 * \note regarding hashing dupli-objects which come from OB_DUPLICOLLECTION,
 * skip the first member of #DupliObject.persistent_id
 * since its a unique index and we only want to know if the group objects are from the same
 * dupli-group instance.
 *
 * \note regarding hashing dupli-objects which come from non-OB_DUPLICOLLECTION,
 * include the first member of #DupliObject.persistent_id
 * since its the index of the vertex/face the object is instantiated on and we want to identify
 * objects on the same vertex/face.
 * In other words, we consider each group of objects from a same item as being
 * the 'local group' where to check for parents.
 */
static uint dupliobject_hash(const void *ptr)
{
  const DupliObject *dob = ptr;
  uint hash = BLI_ghashutil_ptrhash(dob->ob);

  if (dob->type == OB_DUPLICOLLECTION) {
    for (int i = 1; (i < MAX_DUPLI_RECUR) && dob->persistent_id[i] != INT_MAX; i++) {
      hash ^= (dob->persistent_id[i] ^ i);
    }
  }
  else {
    hash ^= (dob->persistent_id[0] ^ 0);
  }
  return hash;
}

/**
 * \note regarding hashing dupli-objects when using OB_DUPLICOLLECTION,
 * skip the first member of #DupliObject.persistent_id
 * since its a unique index and we only want to know if the group objects are from the same
 * dupli-group instance.
 */
static uint dupliobject_instancer_hash(const void *ptr)
{
  const DupliObject *dob = ptr;
  uint hash = BLI_ghashutil_inthash(dob->persistent_id[0]);
  for (int i = 1; (i < MAX_DUPLI_RECUR) && dob->persistent_id[i] != INT_MAX; i++) {
    hash ^= (dob->persistent_id[i] ^ i);
  }
  return hash;
}

/* Compare function that matches dupliobject_hash */
static bool dupliobject_cmp(const void *a_, const void *b_)
{
  const DupliObject *a = a_;
  const DupliObject *b = b_;

  if (a->ob != b->ob) {
    return true;
  }

  if (a->type != b->type) {
    return true;
  }

  if (a->type == OB_DUPLICOLLECTION) {
    for (int i = 1; (i < MAX_DUPLI_RECUR); i++) {
      if (a->persistent_id[i] != b->persistent_id[i]) {
        return true;
      }
      else if (a->persistent_id[i] == INT_MAX) {
        break;
      }
    }
  }
  else {
    if (a->persistent_id[0] != b->persistent_id[0]) {
      return true;
    }
  }

  /* matching */
  return false;
}

/* Compare function that matches dupliobject_instancer_hash. */
static bool dupliobject_instancer_cmp(const void *a_, const void *b_)
{
  const DupliObject *a = a_;
  const DupliObject *b = b_;

  for (int i = 0; (i < MAX_DUPLI_RECUR); i++) {
    if (a->persistent_id[i] != b->persistent_id[i]) {
      return true;
    }
    else if (a->persistent_id[i] == INT_MAX) {
      break;
    }
  }

  /* matching */
  return false;
}

static void make_object_duplilist_real(bContext *C,
                                       Depsgraph *depsgraph,
                                       Scene *scene,
                                       Base *base,
                                       const bool use_base_parent,
                                       const bool use_hierarchy)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  ListBase *lb_duplis;
  DupliObject *dob;
  GHash *dupli_gh, *parent_gh = NULL, *instancer_gh = NULL;

  if (!(base->object->transflag & OB_DUPLI)) {
    return;
  }

  Object *object_eval = DEG_get_evaluated_object(depsgraph, base->object);
  lb_duplis = object_duplilist(depsgraph, scene, object_eval);

  dupli_gh = BLI_ghash_ptr_new(__func__);
  if (use_hierarchy) {
    parent_gh = BLI_ghash_new(dupliobject_hash, dupliobject_cmp, __func__);

    if (use_base_parent) {
      instancer_gh = BLI_ghash_new(
          dupliobject_instancer_hash, dupliobject_instancer_cmp, __func__);
    }
  }

  for (dob = lb_duplis->first; dob; dob = dob->next) {
    Object *ob_src = DEG_get_original_object(dob->ob);
    Object *ob_dst = ID_NEW_SET(ob_src, BKE_object_copy(bmain, ob_src));
    Base *base_dst;

    /* font duplis can have a totcol without material, we get them from parent
     * should be implemented better...
     */
    if (ob_dst->mat == NULL) {
      ob_dst->totcol = 0;
    }

    BKE_collection_object_add_from(bmain, scene, base->object, ob_dst);
    base_dst = BKE_view_layer_base_find(view_layer, ob_dst);
    BLI_assert(base_dst != NULL);

    ED_object_base_select(base_dst, BA_SELECT);
    DEG_id_tag_update(&ob_dst->id, ID_RECALC_SELECT);

    BKE_scene_object_base_flag_sync_from_base(base_dst);

    /* make sure apply works */
    BKE_animdata_free(&ob_dst->id, true);
    ob_dst->adt = NULL;

    /* Proxies are not to be copied. */
    ob_dst->proxy_from = NULL;
    ob_dst->proxy_group = NULL;
    ob_dst->proxy = NULL;

    ob_dst->parent = NULL;
    BKE_constraints_free(&ob_dst->constraints);
    ob_dst->runtime.curve_cache = NULL;
    const bool is_dupli_instancer = (ob_dst->transflag & OB_DUPLI) != 0;
    ob_dst->transflag &= ~OB_DUPLI;
    /* Remove instantiated collection, it's annoying to keep it here
     * (and get potentially a lot of usages of it then...). */
    id_us_min((ID *)ob_dst->instance_collection);
    ob_dst->instance_collection = NULL;

    copy_m4_m4(ob_dst->obmat, dob->mat);
    BKE_object_apply_mat4(ob_dst, ob_dst->obmat, false, false);

    BLI_ghash_insert(dupli_gh, dob, ob_dst);
    if (parent_gh) {
      void **val;
      /* Due to nature of hash/comparison of this ghash, a lot of duplis may be considered as
       * 'the same', this avoids trying to insert same key several time and
       * raise asserts in debug builds... */
      if (!BLI_ghash_ensure_p(parent_gh, dob, &val)) {
        *val = ob_dst;
      }

      if (is_dupli_instancer && instancer_gh) {
        /* Same as above, we may have several 'hits'. */
        if (!BLI_ghash_ensure_p(instancer_gh, dob, &val)) {
          *val = ob_dst;
        }
      }
    }
  }

  for (dob = lb_duplis->first; dob; dob = dob->next) {
    Object *ob_src = dob->ob;
    Object *ob_dst = BLI_ghash_lookup(dupli_gh, dob);

    /* Remap new object to itself, and clear again newid pointer of orig object. */
    BKE_libblock_relink_to_newid(&ob_dst->id);

    DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY);

    if (use_hierarchy) {
      /* original parents */
      Object *ob_src_par = ob_src->parent;
      Object *ob_dst_par = NULL;

      /* find parent that was also made real */
      if (ob_src_par) {
        /* OK to keep most of the members uninitialized,
         * they won't be read, this is simply for a hash lookup. */
        DupliObject dob_key;
        dob_key.ob = ob_src_par;
        dob_key.type = dob->type;
        if (dob->type == OB_DUPLICOLLECTION) {
          memcpy(&dob_key.persistent_id[1],
                 &dob->persistent_id[1],
                 sizeof(dob->persistent_id[1]) * (MAX_DUPLI_RECUR - 1));
        }
        else {
          dob_key.persistent_id[0] = dob->persistent_id[0];
        }
        ob_dst_par = BLI_ghash_lookup(parent_gh, &dob_key);
      }

      if (ob_dst_par) {
        /* allow for all possible parent types */
        ob_dst->partype = ob_src->partype;
        BLI_strncpy(ob_dst->parsubstr, ob_src->parsubstr, sizeof(ob_dst->parsubstr));
        ob_dst->par1 = ob_src->par1;
        ob_dst->par2 = ob_src->par2;
        ob_dst->par3 = ob_src->par3;

        copy_m4_m4(ob_dst->parentinv, ob_src->parentinv);

        ob_dst->parent = ob_dst_par;
      }
    }
    if (use_base_parent && ob_dst->parent == NULL) {
      Object *ob_dst_par = NULL;

      if (instancer_gh != NULL) {
        /* OK to keep most of the members uninitialized,
         * they won't be read, this is simply for a hash lookup. */
        DupliObject dob_key;
        /* We are looking one step upper in hierarchy, so we need to 'shift' the persitent_id,
         * ignoring the first item.
         * We only check on persistent_id here, since we have no idea what object it might be. */
        memcpy(&dob_key.persistent_id[0],
               &dob->persistent_id[1],
               sizeof(dob_key.persistent_id[0]) * (MAX_DUPLI_RECUR - 1));
        ob_dst_par = BLI_ghash_lookup(instancer_gh, &dob_key);
      }

      if (ob_dst_par == NULL) {
        /* Default to parenting to root object...
         * Always the case when use_hierarchy is false. */
        ob_dst_par = base->object;
      }

      ob_dst->parent = ob_dst_par;
      ob_dst->partype = PAROBJECT;
    }

    if (ob_dst->parent) {
      /* note, this may be the parent of other objects, but it should
       * still work out ok */
      BKE_object_apply_mat4(ob_dst, dob->mat, false, true);

      /* to set ob_dst->orig and in case there's any other discrepancies */
      DEG_id_tag_update(&ob_dst->id, ID_RECALC_TRANSFORM);
    }
  }

  if (base->object->transflag & OB_DUPLICOLLECTION && base->object->instance_collection) {
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->proxy_group == base->object) {
        ob->proxy = NULL;
        ob->proxy_from = NULL;
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
      }
    }
    base->object->instance_collection = NULL;
  }

  ED_object_base_select(base, BA_DESELECT);
  DEG_id_tag_update(&base->object->id, ID_RECALC_SELECT);

  BLI_ghash_free(dupli_gh, NULL, NULL);
  if (parent_gh) {
    BLI_ghash_free(parent_gh, NULL, NULL);
  }
  if (instancer_gh) {
    BLI_ghash_free(instancer_gh, NULL, NULL);
  }

  free_object_duplilist(lb_duplis);

  BKE_main_id_clear_newpoins(bmain);

  base->object->transflag &= ~OB_DUPLI;
  DEG_id_tag_update(&base->object->id, ID_RECALC_COPY_ON_WRITE);
}

static int object_duplicates_make_real_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);

  const bool use_base_parent = RNA_boolean_get(op->ptr, "use_base_parent");
  const bool use_hierarchy = RNA_boolean_get(op->ptr, "use_hierarchy");

  BKE_main_id_clear_newpoins(bmain);

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    make_object_duplilist_real(C, depsgraph, scene, base, use_base_parent, use_hierarchy);

    /* dependencies were changed */
    WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, base->object);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE, scene);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, NULL);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicates_make_real(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Instances Real";
  ot->description = "Make instanced objects attached to this object real";
  ot->idname = "OBJECT_OT_duplicates_make_real";

  /* api callbacks */
  ot->exec = object_duplicates_make_real_exec;

  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "use_base_parent",
                  0,
                  "Parent",
                  "Parent newly created objects to the original duplicator");
  RNA_def_boolean(
      ot->srna, "use_hierarchy", 0, "Keep Hierarchy", "Maintain parent child relationships");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Convert Operator
 * \{ */

static const EnumPropertyItem convert_target_items[] = {
    {OB_CURVE, "CURVE", ICON_OUTLINER_OB_CURVE, "Curve from Mesh/Text", ""},
    {OB_MESH, "MESH", ICON_OUTLINER_OB_MESH, "Mesh from Curve/Meta/Surf/Text", ""},
    {OB_GPENCIL, "GPENCIL", ICON_OUTLINER_OB_GREASEPENCIL, "Grease Pencil from Curve/Mesh", ""},
    {0, NULL, 0, NULL, NULL},
};

static void object_data_convert_ensure_curve_cache(Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  if (ob->runtime.curve_cache == NULL) {
    /* Force creation. This is normally not needed but on operator
     * redo we might end up with an object which isn't evaluated yet.
     * Also happens in case we are working on a copy of the object
     * (all its caches have been nuked then).
     */
    if (ELEM(ob->type, OB_SURF, OB_CURVE, OB_FONT)) {
      /* We need 'for render' ON here, to enable computing bevel dipslist if needed.
       * Also makes sense anyway, we would not want e.g. to loose hidden parts etc. */
      BKE_displist_make_curveTypes(depsgraph, scene, ob, true, false);
    }
    else if (ob->type == OB_MBALL) {
      BKE_displist_make_mball(depsgraph, scene, ob);
    }
  }
}

static void object_data_convert_curve_to_mesh(Main *bmain, Depsgraph *depsgraph, Object *ob)
{
  Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
  Curve *curve = ob->data;

  Mesh *mesh = BKE_mesh_new_from_object_to_bmain(bmain, depsgraph, object_eval, true);
  if (mesh == NULL) {
    /* Unable to convert the curve to a mesh. */
    return;
  }

  BKE_object_free_modifiers(ob, 0);
  /* Replace curve used by the object itself. */
  ob->data = mesh;
  ob->type = OB_MESH;
  id_us_min(&curve->id);
  id_us_plus(&mesh->id);
  /* Change objects which are using same curve.
   * A bit annoying, but:
   * - It's possible to have multiple curve objects selected which are sharing the same curve
   *   datablock. We don't want mesh to be created for every of those objects.
   * - This is how conversion worked for a long long time. */
  LISTBASE_FOREACH (Object *, other_object, &bmain->objects) {
    if (other_object->data == curve) {
      other_object->type = OB_MESH;

      id_us_min((ID *)other_object->data);
      other_object->data = ob->data;
      id_us_plus((ID *)other_object->data);
    }
  }
}

static bool object_convert_poll(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  Base *base_act = CTX_data_active_base(C);
  Object *obact = base_act ? base_act->object : NULL;

  return (!ID_IS_LINKED(scene) && obact && (BKE_object_is_in_editmode(obact) == false) &&
          (base_act->flag & BASE_SELECTED) && !ID_IS_LINKED(obact));
}

/* Helper for object_convert_exec */
static Base *duplibase_for_convert(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, ViewLayer *view_layer, Base *base, Object *ob)
{
  Object *obn;
  Base *basen;

  if (ob == NULL) {
    ob = base->object;
  }

  obn = BKE_object_copy(bmain, ob);
  DEG_id_tag_update(&obn->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  BKE_collection_object_add_from(bmain, scene, ob, obn);

  basen = BKE_view_layer_base_find(view_layer, obn);
  ED_object_base_select(basen, BA_SELECT);
  ED_object_base_select(base, BA_DESELECT);

  /* XXX An ugly hack needed because if we re-run depsgraph with some new MBall objects
   * having same 'family name' as orig ones, they will affect end result of MBall computation...
   * For until we get rid of that name-based thingy in MBalls, that should do the trick
   * (this is weak, but other solution (to change name of obn) is even worse imho).
   * See T65996. */
  const bool is_meta_ball = (obn->type == OB_MBALL);
  void *obdata = obn->data;
  if (is_meta_ball) {
    obn->type = OB_EMPTY;
    obn->data = NULL;
  }

  /* XXX Doing that here is stupid, it means we update and re-evaluate the whole depsgraph every
   * time we need to duplicate an object to convert it. Even worse, this is not 100% correct, since
   * we do not yet have duplicated obdata.
   * However, that is a safe solution for now. Proper, longer-term solution is to refactor
   * object_convert_exec to:
   *  - duplicate all data it needs to in a first loop.
   *  - do a single update.
   *  - convert data in a second loop. */
  DEG_graph_tag_relations_update(depsgraph);
  CustomData_MeshMasks customdata_mask_prev = scene->customdata_mask;
  CustomData_MeshMasks_update(&scene->customdata_mask, &CD_MASK_MESH);
  BKE_scene_graph_update_tagged(depsgraph, bmain);
  scene->customdata_mask = customdata_mask_prev;

  if (is_meta_ball) {
    obn->type = OB_MBALL;
    obn->data = obdata;
  }

  return basen;
}

static int object_convert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Base *basen = NULL, *basact = NULL;
  Object *ob1, *obact = CTX_data_active_object(C);
  Curve *cu;
  Nurb *nu;
  MetaBall *mb;
  Mesh *me;
  Object *ob_gpencil = NULL;
  const short target = RNA_enum_get(op->ptr, "target");
  bool keep_original = RNA_boolean_get(op->ptr, "keep_original");

  const float angle = RNA_float_get(op->ptr, "angle");
  const int thickness = RNA_int_get(op->ptr, "thickness");
  const bool use_seams = RNA_boolean_get(op->ptr, "seams");
  const bool use_faces = RNA_boolean_get(op->ptr, "faces");
  const float offset = RNA_float_get(op->ptr, "offset");

  int a, mballConverted = 0;
  bool gpencilConverted = false;

  /* don't forget multiple users! */

  {
    FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
      ob->flag &= ~OB_DONE;

      /* flag data that's not been edited (only needed for !keep_original) */
      if (ob->data) {
        ((ID *)ob->data)->tag |= LIB_TAG_DOIT;
      }

      /* possible metaball basis is not in this scene */
      if (ob->type == OB_MBALL && target == OB_MESH) {
        if (BKE_mball_is_basis(ob) == false) {
          Object *ob_basis;
          ob_basis = BKE_mball_basis_find(scene, ob);
          if (ob_basis) {
            ob_basis->flag &= ~OB_DONE;
          }
        }
      }
    }
    FOREACH_SCENE_OBJECT_END;
  }

  ListBase selected_editable_bases;
  CTX_data_selected_editable_bases(C, &selected_editable_bases);

  /* Ensure we get all meshes calculated with a sufficient data-mask,
   * needed since re-evaluating single modifiers causes bugs if they depend
   * on other objects data masks too, see: T50950. */
  {
    LISTBASE_FOREACH (CollectionPointerLink *, link, &selected_editable_bases) {
      Base *base = link->ptr.data;
      Object *ob = base->object;

      /* The way object type conversion works currently (enforcing conversion of *all* objects
       * using converted object-data, even some un-selected/hidden/another scene ones,
       * sounds totally bad to me.
       * However, changing this is more design than bug-fix, not to mention convoluted code below,
       * so that will be for later.
       * But at the very least, do not do that with linked IDs! */
      if ((ID_IS_LINKED(ob) || (ob->data && ID_IS_LINKED(ob->data))) && !keep_original) {
        keep_original = true;
        BKE_report(
            op->reports,
            RPT_INFO,
            "Converting some linked object/object data, enforcing 'Keep Original' option to True");
      }

      DEG_id_tag_update(&base->object->id, ID_RECALC_GEOMETRY);
    }

    CustomData_MeshMasks customdata_mask_prev = scene->customdata_mask;
    CustomData_MeshMasks_update(&scene->customdata_mask, &CD_MASK_MESH);
    BKE_scene_graph_update_tagged(depsgraph, bmain);
    scene->customdata_mask = customdata_mask_prev;
  }

  LISTBASE_FOREACH (CollectionPointerLink *, link, &selected_editable_bases) {
    Object *newob = NULL;
    Base *base = link->ptr.data;
    Object *ob = base->object;

    if (ob->flag & OB_DONE || !IS_TAGGED(ob->data)) {
      if (ob->type != target) {
        base->flag &= ~SELECT;
        ob->flag &= ~SELECT;
      }

      /* obdata already modified */
      if (!IS_TAGGED(ob->data)) {
        /* When 2 objects with linked data are selected, converting both
         * would keep modifiers on all but the converted object [#26003] */
        if (ob->type == OB_MESH) {
          BKE_object_free_modifiers(ob, 0); /* after derivedmesh calls! */
        }
        if (ob->type == OB_GPENCIL) {
          BKE_object_free_modifiers(ob, 0); /* after derivedmesh calls! */
          BKE_object_free_shaderfx(ob, 0);
        }
      }
    }
    else if (ob->type == OB_MESH && target == OB_CURVE) {
      ob->flag |= OB_DONE;

      if (keep_original) {
        basen = duplibase_for_convert(bmain, depsgraph, scene, view_layer, base, NULL);
        newob = basen->object;

        /* decrement original mesh's usage count  */
        me = newob->data;
        id_us_min(&me->id);

        /* make a new copy of the mesh */
        newob->data = BKE_mesh_copy(bmain, me);
      }
      else {
        newob = ob;
      }

      BKE_mesh_to_curve(bmain, depsgraph, scene, newob);

      if (newob->type == OB_CURVE) {
        BKE_object_free_modifiers(newob, 0); /* after derivedmesh calls! */
        ED_rigidbody_object_remove(bmain, scene, newob);
      }
    }
    else if (ob->type == OB_MESH && target == OB_GPENCIL) {
      ob->flag |= OB_DONE;

      /* Create a new grease pencil object and copy transformations. */
      ushort local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uuid : 0;
      float loc[3], size[3], rot[3][3], eul[3];
      float matrix[4][4];
      mat4_to_loc_rot_size(loc, rot, size, ob->obmat);
      mat3_to_eul(eul, rot);

      ob_gpencil = ED_gpencil_add_object(C, loc, local_view_bits);
      copy_v3_v3(ob_gpencil->loc, loc);
      copy_v3_v3(ob_gpencil->rot, eul);
      copy_v3_v3(ob_gpencil->scale, size);
      unit_m4(matrix);
      /* Set object in 3D mode. */
      bGPdata *gpd = (bGPdata *)ob_gpencil->data;
      gpd->draw_mode = GP_DRAWMODE_3D;

      BKE_gpencil_convert_mesh(bmain,
                               depsgraph,
                               scene,
                               ob_gpencil,
                               ob,
                               angle,
                               thickness,
                               offset,
                               matrix,
                               0,
                               use_seams,
                               use_faces);
      gpencilConverted = true;

      /* Remove unused materials. */
      int actcol = ob_gpencil->actcol;
      for (int slot = 1; slot <= ob_gpencil->totcol; slot++) {
        while (slot <= ob_gpencil->totcol &&
               !BKE_object_material_slot_used(ob_gpencil->data, slot)) {
          ob_gpencil->actcol = slot;
          BKE_object_material_slot_remove(CTX_data_main(C), ob_gpencil);

          if (actcol >= slot) {
            actcol--;
          }
        }
      }
      ob_gpencil->actcol = actcol;
    }
    else if (ob->type == OB_MESH) {
      ob->flag |= OB_DONE;

      if (keep_original) {
        basen = duplibase_for_convert(bmain, depsgraph, scene, view_layer, base, NULL);
        newob = basen->object;

        /* decrement original mesh's usage count  */
        me = newob->data;
        id_us_min(&me->id);

        /* make a new copy of the mesh */
        newob->data = BKE_mesh_copy(bmain, me);
      }
      else {
        newob = ob;
        DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
      }

      /* make new mesh data from the original copy */
      /* note: get the mesh from the original, not from the copy in some
       * cases this doesn't give correct results (when MDEF is used for eg)
       */
      Scene *scene_eval = (Scene *)DEG_get_evaluated_id(depsgraph, &scene->id);
      Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
      Mesh *me_eval = mesh_get_eval_final(depsgraph, scene_eval, ob_eval, &CD_MASK_MESH);
      me_eval = BKE_mesh_copy_for_eval(me_eval, false);
      BKE_mesh_nomain_to_mesh(me_eval, newob->data, newob, &CD_MASK_MESH, true);
      BKE_object_free_modifiers(newob, 0); /* after derivedmesh calls! */
    }
    else if (ob->type == OB_FONT) {
      ob->flag |= OB_DONE;

      if (keep_original) {
        basen = duplibase_for_convert(bmain, depsgraph, scene, view_layer, base, NULL);
        newob = basen->object;

        /* decrement original curve's usage count  */
        id_us_min(&((Curve *)newob->data)->id);

        /* make a new copy of the curve */
        newob->data = BKE_curve_copy(bmain, ob->data);
      }
      else {
        newob = ob;
      }

      cu = newob->data;

      Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);
      BKE_vfont_to_curve_ex(ob_eval, ob_eval->data, FO_EDIT, &cu->nurb, NULL, NULL, NULL, NULL);

      newob->type = OB_CURVE;
      cu->type = OB_CURVE;

      if (cu->vfont) {
        id_us_min(&cu->vfont->id);
        cu->vfont = NULL;
      }
      if (cu->vfontb) {
        id_us_min(&cu->vfontb->id);
        cu->vfontb = NULL;
      }
      if (cu->vfonti) {
        id_us_min(&cu->vfonti->id);
        cu->vfonti = NULL;
      }
      if (cu->vfontbi) {
        id_us_min(&cu->vfontbi->id);
        cu->vfontbi = NULL;
      }

      if (!keep_original) {
        /* other users */
        if (ID_REAL_USERS(&cu->id) > 1) {
          for (ob1 = bmain->objects.first; ob1; ob1 = ob1->id.next) {
            if (ob1->data == ob->data) {
              ob1->type = OB_CURVE;
              DEG_id_tag_update(&ob1->id,
                                ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
            }
          }
        }
      }

      for (nu = cu->nurb.first; nu; nu = nu->next) {
        nu->charidx = 0;
      }

      cu->flag &= ~CU_3D;
      BKE_curve_curve_dimension_update(cu);

      if (target == OB_MESH) {
        /* No assumption should be made that the resulting objects is a mesh, as conversion can
         * fail. */
        object_data_convert_curve_to_mesh(bmain, depsgraph, newob);
        /* meshes doesn't use displist */
        BKE_object_free_curve_cache(newob);
      }
    }
    else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
      ob->flag |= OB_DONE;

      if (target == OB_MESH) {
        if (keep_original) {
          basen = duplibase_for_convert(bmain, depsgraph, scene, view_layer, base, NULL);
          newob = basen->object;

          /* decrement original curve's usage count  */
          id_us_min(&((Curve *)newob->data)->id);

          /* make a new copy of the curve */
          newob->data = BKE_curve_copy(bmain, ob->data);
        }
        else {
          newob = ob;
        }

        /* No assumption should be made that the resulting objects is a mesh, as conversion can
         * fail. */
        object_data_convert_curve_to_mesh(bmain, depsgraph, newob);
        /* meshes doesn't use displist */
        BKE_object_free_curve_cache(newob);
      }
      else if (target == OB_GPENCIL) {
        if (ob->type != OB_CURVE) {
          ob->flag &= ~OB_DONE;
          BKE_report(op->reports, RPT_ERROR, "Convert Surfaces to Grease Pencil is not supported");
        }
        else {
          /* Create a new grease pencil object and copy transformations.
           * Nurbs Surface are not supported.
           */
          ushort local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uuid : 0;
          ob_gpencil = ED_gpencil_add_object(C, ob->loc, local_view_bits);
          copy_v3_v3(ob_gpencil->rot, ob->rot);
          copy_v3_v3(ob_gpencil->scale, ob->scale);
          BKE_gpencil_convert_curve(bmain, scene, ob_gpencil, ob, false, false, true);
          gpencilConverted = true;
        }
      }
    }
    else if (ob->type == OB_MBALL && target == OB_MESH) {
      Object *baseob;

      base->flag &= ~BASE_SELECTED;
      ob->base_flag &= ~BASE_SELECTED;

      baseob = BKE_mball_basis_find(scene, ob);

      if (ob != baseob) {
        /* if motherball is converting it would be marked as done later */
        ob->flag |= OB_DONE;
      }

      if (!(baseob->flag & OB_DONE)) {
        basen = duplibase_for_convert(bmain, depsgraph, scene, view_layer, base, baseob);
        newob = basen->object;

        mb = newob->data;
        id_us_min(&mb->id);

        newob->data = BKE_mesh_add(bmain, "Mesh");
        newob->type = OB_MESH;

        me = newob->data;
        me->totcol = mb->totcol;
        if (newob->totcol) {
          me->mat = MEM_dupallocN(mb->mat);
          for (a = 0; a < newob->totcol; a++) {
            id_us_plus((ID *)me->mat[a]);
          }
        }

        object_data_convert_ensure_curve_cache(depsgraph, scene, baseob);
        BKE_mesh_from_metaball(&baseob->runtime.curve_cache->disp, newob->data);

        if (obact->type == OB_MBALL) {
          basact = basen;
        }

        baseob->flag |= OB_DONE;
        mballConverted = 1;
      }
    }
    else {
      continue;
    }

    /* Ensure new object has consistent material data with its new obdata. */
    if (newob) {
      BKE_object_materials_test(bmain, newob, newob->data);
    }

    /* tag obdata if it was been changed */

    /* If the original object is active then make this object active */
    if (basen) {
      if (ob == obact) {
        /* store new active base to update BASACT */
        basact = basen;
      }

      basen = NULL;
    }

    if (!keep_original && (ob->flag & OB_DONE)) {
      /* NOTE: Tag transform for update because object parenting to curve with path is handled
       * differently from all other cases. Converting curve to mesh and mesh to curve will likely
       * affect the way children are evaluated.
       * It is not enough to tag only geometry and rely on the curve parenting relations because
       * this relation is lost when curve is converted to mesh. */
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
      ((ID *)ob->data)->tag &= ~LIB_TAG_DOIT; /* flag not to convert this datablock again */
    }
  }
  BLI_freelistN(&selected_editable_bases);

  if (!keep_original) {
    if (mballConverted) {
      /* We need to remove non-basis MBalls first, otherwise we won't be able to detect them if
       * their basis happens to be removed first. */
      FOREACH_SCENE_OBJECT_BEGIN (scene, ob_mball) {
        if (ob_mball->type == OB_MBALL) {
          Object *ob_basis = NULL;
          if (!BKE_mball_is_basis(ob_mball) &&
              ((ob_basis = BKE_mball_basis_find(scene, ob_mball)) && (ob_basis->flag & OB_DONE))) {
            ED_object_base_free_and_unlink(bmain, scene, ob_mball);
          }
        }
      }
      FOREACH_SCENE_OBJECT_END;
      FOREACH_SCENE_OBJECT_BEGIN (scene, ob_mball) {
        if (ob_mball->type == OB_MBALL) {
          if (ob_mball->flag & OB_DONE) {
            if (BKE_mball_is_basis(ob_mball)) {
              ED_object_base_free_and_unlink(bmain, scene, ob_mball);
            }
          }
        }
      }
      FOREACH_SCENE_OBJECT_END;
    }
    /* Remove curves and meshes converted to Grease Pencil object. */
    if (gpencilConverted) {
      FOREACH_SCENE_OBJECT_BEGIN (scene, ob_delete) {
        if ((ob_delete->type == OB_CURVE) || (ob_delete->type == OB_MESH)) {
          if (ob_delete->flag & OB_DONE) {
            ED_object_base_free_and_unlink(bmain, scene, ob_delete);
          }
        }
      }
      FOREACH_SCENE_OBJECT_END;
    }
  }

  // XXX  ED_object_editmode_enter(C, 0);
  // XXX  exit_editmode(C, EM_FREEDATA|); /* freedata, but no undo */

  if (basact) {
    /* active base was changed */
    ED_object_base_activate(C, basact);
    BASACT(view_layer) = basact;
  }
  else if (BASACT(view_layer)->object->flag & OB_DONE) {
    WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, BASACT(view_layer)->object);
    WM_event_add_notifier(C, NC_OBJECT | ND_DATA, BASACT(view_layer)->object);
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

static void object_convert_ui(bContext *UNUSED(C), wmOperator *op)
{
  uiLayout *layout = op->layout;
  PointerRNA ptr;

  RNA_pointer_create(NULL, op->type->srna, op->properties, &ptr);
  uiItemR(layout, &ptr, "target", 0, NULL, ICON_NONE);
  uiItemR(layout, &ptr, "keep_original", 0, NULL, ICON_NONE);

  if (RNA_enum_get(&ptr, "target") == OB_GPENCIL) {
    uiItemR(layout, &ptr, "thickness", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "angle", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "offset", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "seams", 0, NULL, ICON_NONE);
    uiItemR(layout, &ptr, "faces", 0, NULL, ICON_NONE);
  }
}

void OBJECT_OT_convert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Convert to";
  ot->description = "Convert selected objects to another type";
  ot->idname = "OBJECT_OT_convert";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_convert_exec;
  ot->poll = object_convert_poll;
  ot->ui = object_convert_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(
      ot->srna, "target", convert_target_items, OB_MESH, "Target", "Type of object to convert to");
  RNA_def_boolean(ot->srna,
                  "keep_original",
                  0,
                  "Keep Original",
                  "Keep original objects instead of replacing them");

  prop = RNA_def_float_rotation(ot->srna,
                                "angle",
                                0,
                                NULL,
                                DEG2RADF(0.0f),
                                DEG2RADF(180.0f),
                                "Threshold Angle",
                                "Threshold to determine ends of the strokes",
                                DEG2RADF(0.0f),
                                DEG2RADF(180.0f));
  RNA_def_property_float_default(prop, DEG2RADF(70.0f));

  RNA_def_int(ot->srna, "thickness", 5, 1, 100, "Thickness", "", 1, 100);
  RNA_def_boolean(ot->srna, "seams", 0, "Only Seam Edges", "Convert only seam edges");
  RNA_def_boolean(ot->srna, "faces", 1, "Export Faces", "Export faces as filled strokes");
  RNA_def_float_distance(ot->srna,
                         "offset",
                         0.01f,
                         0.0,
                         OBJECT_ADD_SIZE_MAXF,
                         "Stroke Offset",
                         "Offset strokes from fill",
                         0.0,
                         100.00);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Duplicate Object Operator
 * \{ */

/*
 * dupflag: a flag made from constants declared in DNA_userdef_types.h
 * The flag tells adduplicate() whether to copy data linked to the object,
 * or to reference the existing data.
 * U.dupflag for default operations or you can construct a flag as python does
 * if the dupflag is 0 then no data will be copied (linked duplicate). */

/* used below, assumes id.new is correct */
/* leaves selection of base/object unaltered */
/* Does set ID->newid pointers. */
static Base *object_add_duplicate_internal(
    Main *bmain, Scene *scene, ViewLayer *view_layer, Object *ob, const eDupli_ID_Flags dupflag)
{
  Base *base, *basen = NULL;
  Object *obn;

  if (ob->mode & OB_MODE_POSE) {
    /* nothing? */
  }
  else {
    obn = ID_NEW_SET(ob, BKE_object_duplicate(bmain, ob, dupflag));
    DEG_id_tag_update(&obn->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

    base = BKE_view_layer_base_find(view_layer, ob);
    if ((base != NULL) && (base->flag & BASE_VISIBLE_DEPSGRAPH)) {
      BKE_collection_object_add_from(bmain, scene, ob, obn);
    }
    else {
      LayerCollection *layer_collection = BKE_layer_collection_get_active(view_layer);
      BKE_collection_object_add(bmain, layer_collection->collection, obn);
    }

    basen = BKE_view_layer_base_find(view_layer, obn);
    if (base != NULL) {
      basen->local_view_bits = base->local_view_bits;
    }

    /* 1) duplis should end up in same collection as the original
     * 2) Rigid Body sim participants MUST always be part of a collection...
     */
    // XXX: is 2) really a good measure here?
    if (ob->rigidbody_object || ob->rigidbody_constraint) {
      Collection *collection;
      for (collection = bmain->collections.first; collection; collection = collection->id.next) {
        if (BKE_collection_has_object(collection, ob)) {
          BKE_collection_object_add(bmain, collection, obn);
        }
      }
    }
  }
  return basen;
}

/* single object duplicate, if dupflag==0, fully linked, else it uses the flags given */
/* leaves selection of base/object unaltered.
 * note: don't call this within a loop since clear_* funcs loop over the entire database.
 * note: caller must do DAG_relations_tag_update(bmain);
 *       this is not done automatic since we may duplicate many objects in a batch */
Base *ED_object_add_duplicate(
    Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base, const eDupli_ID_Flags dupflag)
{
  Base *basen;
  Object *ob;

  basen = object_add_duplicate_internal(bmain, scene, view_layer, base->object, dupflag);
  if (basen == NULL) {
    return NULL;
  }

  ob = basen->object;

  /* link own references to the newly duplicated data [#26816] */
  BKE_libblock_relink_to_newid(&ob->id);

  /* DAG_relations_tag_update(bmain); */ /* caller must do */

  if (ob->data != NULL) {
    DEG_id_tag_update_ex(bmain, (ID *)ob->data, ID_RECALC_EDITORS);
  }

  BKE_main_id_clear_newpoins(bmain);

  return basen;
}

/* contextual operator dupli */
static int duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool linked = RNA_boolean_get(op->ptr, "linked");
  const eDupli_ID_Flags dupflag = (linked) ? 0 : (eDupli_ID_Flags)U.dupflag;

  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    Base *basen = object_add_duplicate_internal(bmain, scene, view_layer, base->object, dupflag);

    /* note that this is safe to do with this context iterator,
     * the list is made in advance */
    ED_object_base_select(base, BA_DESELECT);
    ED_object_base_select(basen, BA_SELECT);

    if (basen == NULL) {
      continue;
    }

    /* new object becomes active */
    if (BASACT(view_layer) == base) {
      ED_object_base_activate(C, basen);
    }

    if (basen->object->data) {
      DEG_id_tag_update(basen->object->data, 0);
    }
  }
  CTX_DATA_END;

  copy_object_set_idnew(C);

  ED_outliner_select_sync_from_object_tag(C);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_COPY_ON_WRITE | ID_RECALC_SELECT);

  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Duplicate Objects";
  ot->description = "Duplicate selected objects";
  ot->idname = "OBJECT_OT_duplicate";

  /* api callbacks */
  ot->exec = duplicate_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  prop = RNA_def_boolean(ot->srna,
                         "linked",
                         0,
                         "Linked",
                         "Duplicate object but not object data, linking to the original data");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(
      ot->srna, "mode", rna_enum_transform_mode_types, TFM_TRANSLATION, "Mode", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Named Object Operator
 *
 * Use for drag & drop.
 * \{ */

static int object_add_named_exec(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);
  const wmEvent *event = win ? win->eventstate : NULL;
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Base *basen;
  Object *ob;
  const bool linked = RNA_boolean_get(op->ptr, "linked");
  const eDupli_ID_Flags dupflag = (linked) ? 0 : (eDupli_ID_Flags)U.dupflag;
  char name[MAX_ID_NAME - 2];

  /* find object, create fake base */
  RNA_string_get(op->ptr, "name", name);
  ob = (Object *)BKE_libblock_find_name(bmain, ID_OB, name);

  if (ob == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Object not found");
    return OPERATOR_CANCELLED;
  }

  /* prepare dupli */
  basen = object_add_duplicate_internal(bmain, scene, view_layer, ob, dupflag);

  if (basen == NULL) {
    BKE_report(op->reports, RPT_ERROR, "Object could not be duplicated");
    return OPERATOR_CANCELLED;
  }

  basen->object->restrictflag &= ~OB_RESTRICT_VIEWPORT;

  if (event) {
    ARegion *region = CTX_wm_region(C);
    const int mval[2] = {event->x - region->winrct.xmin, event->y - region->winrct.ymin};
    ED_object_location_from_view(C, basen->object->loc);
    ED_view3d_cursor3d_position(C, mval, false, basen->object->loc);
  }

  ED_object_base_select(basen, BA_SELECT);
  ED_object_base_activate(C, basen);

  copy_object_set_idnew(C);

  /* TODO(sergey): Only update relations for the current scene. */
  DEG_relations_tag_update(bmain);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_add_named(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Named Object";
  ot->description = "Add named object";
  ot->idname = "OBJECT_OT_add_named";

  /* api callbacks */
  ot->exec = object_add_named_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna,
                  "linked",
                  0,
                  "Linked",
                  "Duplicate object but not object data, linking to the original data");
  RNA_def_string(ot->srna, "name", NULL, MAX_ID_NAME - 2, "Name", "Object name to add");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join Object Operator
 *
 * \{ */

static bool object_join_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (!ob || ID_IS_LINKED(ob)) {
    return 0;
  }

  if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_ARMATURE, OB_GPENCIL)) {
    return ED_operator_screenactive(C);
  }
  else {
    return 0;
  }
}

static int object_join_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);

  if (ob->mode & OB_MODE_EDIT) {
    BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
    return OPERATOR_CANCELLED;
  }
  else if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot edit external library data");
    return OPERATOR_CANCELLED;
  }
  else if (ob->type == OB_GPENCIL) {
    bGPdata *gpd = (bGPdata *)ob->data;
    if ((!gpd) || GPENCIL_ANY_MODE(gpd)) {
      BKE_report(op->reports, RPT_ERROR, "This data does not support joining in this mode");
      return OPERATOR_CANCELLED;
    }
  }

  if (ob->type == OB_MESH) {
    return ED_mesh_join_objects_exec(C, op);
  }
  else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
    return ED_curve_join_objects_exec(C, op);
  }
  else if (ob->type == OB_ARMATURE) {
    return ED_armature_join_objects_exec(C, op);
  }
  else if (ob->type == OB_GPENCIL) {
    return ED_gpencil_join_objects_exec(C, op);
  }

  return OPERATOR_CANCELLED;
}

void OBJECT_OT_join(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join";
  ot->description = "Join selected objects into active object";
  ot->idname = "OBJECT_OT_join";

  /* api callbacks */
  ot->exec = object_join_exec;
  ot->poll = object_join_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join as Shape Key Operator
 * \{ */

static bool join_shapes_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (!ob || ID_IS_LINKED(ob)) {
    return 0;
  }

  /* only meshes supported at the moment */
  if (ob->type == OB_MESH) {
    return ED_operator_screenactive(C);
  }
  else {
    return 0;
  }
}

static int join_shapes_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);

  if (ob->mode & OB_MODE_EDIT) {
    BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
    return OPERATOR_CANCELLED;
  }
  else if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot edit external library data");
    return OPERATOR_CANCELLED;
  }

  if (ob->type == OB_MESH) {
    return ED_mesh_shapes_join_objects_exec(C, op);
  }

  return OPERATOR_CANCELLED;
}

void OBJECT_OT_join_shapes(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join as Shapes";
  ot->description = "Copy the current resulting shape of another selected object to this one";
  ot->idname = "OBJECT_OT_join_shapes";

  /* api callbacks */
  ot->exec = join_shapes_exec;
  ot->poll = join_shapes_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_collection_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_light_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"
#include "DNA_vfont_types.h"

#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector_types.hh"
#include "BLI_rand.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_action.hh"
#include "BKE_anim_data.hh"
#include "BKE_anonymous_attribute_id.hh"
#include "BKE_armature.hh"
#include "BKE_attribute.h"
#include "BKE_camera.h"
#include "BKE_collection.hh"
#include "BKE_constraint.h"
#include "BKE_context.hh"
#include "BKE_curve.hh"
#include "BKE_curve_legacy_convert.hh"
#include "BKE_curve_to_mesh.hh"
#include "BKE_curves.h"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_displist.h"
#include "BKE_duplilist.hh"
#include "BKE_effect.h"
#include "BKE_geometry_set.hh"
#include "BKE_geometry_set_instances.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_key.hh"
#include "BKE_lattice.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_lib_query.hh"
#include "BKE_lib_remap.hh"
#include "BKE_library.hh"
#include "BKE_light.h"
#include "BKE_lightprobe.h"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_mball.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.hh"
#include "BKE_nla.hh"
#include "BKE_node.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_particle.h"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"
#include "BKE_vfont.hh"
#include "BKE_volume.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "GEO_join_geometries.hh"
#include "GEO_mesh_to_curve.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "UI_interface_layout.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_armature.hh"
#include "ED_curve.hh"
#include "ED_curves.hh"
#include "ED_gpencil_legacy.hh"
#include "ED_grease_pencil.hh"
#include "ED_mball.hh"
#include "ED_mesh.hh"
#include "ED_node.hh"
#include "ED_object.hh"
#include "ED_outliner.hh"
#include "ED_physics.hh"
#include "ED_pointcloud.hh"
#include "ED_render.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_transform.hh"
#include "ED_view3d.hh"

#include "ANIM_bone_collections.hh"

#include "UI_resources.hh"

#include "object_intern.hh"

const EnumPropertyItem rna_enum_light_type_items[] = {
    {LA_LOCAL, "POINT", ICON_LIGHT_POINT, "Point", "Omnidirectional point light source"},
    {LA_SUN, "SUN", ICON_LIGHT_SUN, "Sun", "Constant direction parallel ray light source"},
    {LA_SPOT, "SPOT", ICON_LIGHT_SPOT, "Spot", "Directional cone light source"},
    {LA_AREA, "AREA", ICON_LIGHT_AREA, "Area", "Directional area light source"},
    {0, nullptr, 0, nullptr, nullptr},
};

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name Local Enum Declarations
 * \{ */

/* This is an exact copy of the define in `rna_light.cc`
 * kept here because of linking order.
 * Icons are only defined here. */

/* copy from rna_object_force.cc */
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
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem lightprobe_type_items[] = {
    {LIGHTPROBE_TYPE_SPHERE,
     "SPHERE",
     ICON_LIGHTPROBE_SPHERE,
     "Sphere",
     "Light probe that captures precise lighting from all directions at a single point in space"},
    {LIGHTPROBE_TYPE_PLANE,
     "PLANE",
     ICON_LIGHTPROBE_PLANE,
     "Plane",
     "Light probe that captures incoming light from a single direction on a plane"},
    {LIGHTPROBE_TYPE_VOLUME,
     "VOLUME",
     ICON_LIGHTPROBE_VOLUME,
     "Volume",
     "Light probe that captures low frequency lighting inside a volume"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Local Helpers
 * \{ */

/**
 * Operator properties for creating an object under a screen space (2D) coordinate.
 * Used for object dropping like behavior (drag object and drop into 3D View).
 */
static void object_add_drop_xy_props(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_int(ot->srna,
                     "drop_x",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "Drop X",
                     "X-coordinate (screen space) to place the new object under",
                     INT_MIN,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_int(ot->srna,
                     "drop_y",
                     0,
                     INT_MIN,
                     INT_MAX,
                     "Drop Y",
                     "Y-coordinate (screen space) to place the new object under",
                     INT_MIN,
                     INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static bool object_add_drop_xy_is_set(const wmOperator *op)
{
  return RNA_struct_property_is_set(op->ptr, "drop_x") &&
         RNA_struct_property_is_set(op->ptr, "drop_y");
}

/**
 * Query the currently set X- and Y-coordinate to position the new object under.
 * \param r_mval: Returned pointer to the coordinate in region-space.
 */
static bool object_add_drop_xy_get(bContext *C, wmOperator *op, int (*r_mval)[2])
{
  if (!object_add_drop_xy_is_set(op)) {
    (*r_mval)[0] = 0.0f;
    (*r_mval)[1] = 0.0f;
    return false;
  }

  const ARegion *region = CTX_wm_region(C);
  (*r_mval)[0] = RNA_int_get(op->ptr, "drop_x") - region->winrct.xmin;
  (*r_mval)[1] = RNA_int_get(op->ptr, "drop_y") - region->winrct.ymin;

  return true;
}

/**
 * Set the drop coordinate to the mouse position (if not already set) and call the operator's
 * `exec()` callback.
 */
static wmOperatorStatus object_add_drop_xy_generic_invoke(bContext *C,
                                                          wmOperator *op,
                                                          const wmEvent *event)
{
  if (!object_add_drop_xy_is_set(op)) {
    RNA_int_set(op->ptr, "drop_x", event->xy[0]);
    RNA_int_set(op->ptr, "drop_y", event->xy[1]);
  }
  return op->type->exec(C, op);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Add Object API
 * \{ */

void location_from_view(bContext *C, float loc[3])
{
  const Scene *scene = CTX_data_scene(C);
  copy_v3_v3(loc, scene->cursor.location);
}

void rotation_from_quat(float rot[3], const float viewquat[4], const char align_axis)
{
  BLI_assert(align_axis >= 'X' && align_axis <= 'Z');

  switch (align_axis) {
    case 'X': {
      /* Same as 'rv3d->viewinv[1]' */
      const float axis_y[4] = {0.0f, 1.0f, 0.0f};
      float quat_y[4], quat[4];
      axis_angle_to_quat(quat_y, axis_y, M_PI_2);
      mul_qt_qtqt(quat, viewquat, quat_y);
      quat_to_eul(rot, quat);
      break;
    }
    case 'Y': {
      quat_to_eul(rot, viewquat);
      rot[0] -= float(M_PI_2);
      break;
    }
    case 'Z': {
      quat_to_eul(rot, viewquat);
      break;
    }
  }
}

void rotation_from_view(bContext *C, float rot[3], const char align_axis)
{
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  BLI_assert(align_axis >= 'X' && align_axis <= 'Z');
  if (rv3d) {
    float viewquat[4];
    copy_qt_qt(viewquat, rv3d->viewquat);
    viewquat[0] *= -1.0f;
    rotation_from_quat(rot, viewquat, align_axis);
  }
  else {
    zero_v3(rot);
  }
}

void init_transform_on_add(Object *object, const float loc[3], const float rot[3])
{
  if (loc) {
    copy_v3_v3(object->loc, loc);
  }

  if (rot) {
    copy_v3_v3(object->rot, rot);
  }

  BKE_object_to_mat4(object, object->runtime->object_to_world.ptr());
}

float new_primitive_matrix(bContext *C,
                           Object *obedit,
                           const float loc[3],
                           const float rot[3],
                           const float scale[3],
                           float r_primmat[4][4])
{
  Scene *scene = CTX_data_scene(C);
  View3D *v3d = CTX_wm_view3d(C);
  float mat[3][3], rmat[3][3], cmat[3][3], imat[3][3];

  unit_m4(r_primmat);

  eul_to_mat3(rmat, rot);
  invert_m3(rmat);

  /* inverse transform for initial rotation and object */
  copy_m3_m4(mat, obedit->object_to_world().ptr());
  mul_m3_m3m3(cmat, rmat, mat);
  invert_m3_m3(imat, cmat);
  copy_m4_m3(r_primmat, imat);

  /* center */
  copy_v3_v3(r_primmat[3], loc);
  sub_v3_v3v3(r_primmat[3], r_primmat[3], obedit->object_to_world().location());
  invert_m3_m3(imat, mat);
  mul_m3_v3(imat, r_primmat[3]);

  if (scale != nullptr) {
    rescale_m4(r_primmat, scale);
  }

  {
    const float dia = v3d ? ED_view3d_grid_scale(scene, v3d, nullptr) :
                            ED_scene_grid_scale(scene, nullptr);
    return dia;
  }

  // return 1.0f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Object Operator
 * \{ */

static void view_align_update(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  RNA_struct_system_idprops_unset(ptr, "rotation");
}

void add_unit_props_size(wmOperatorType *ot)
{
  RNA_def_float_distance(
      ot->srna, "size", 2.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Size", "", 0.001, 100.00);
}

void add_unit_props_radius_ex(wmOperatorType *ot, float default_value)
{
  RNA_def_float_distance(
      ot->srna, "radius", default_value, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius", "", 0.001, 100.00);
}

void add_unit_props_radius(wmOperatorType *ot)
{
  add_unit_props_radius_ex(ot, 1.0f);
}

void add_generic_props(wmOperatorType *ot, bool do_editmode)
{
  PropertyRNA *prop;

  if (do_editmode) {
    prop = RNA_def_boolean(ot->srna,
                           "enter_editmode",
                           false,
                           "Enter Edit Mode",
                           "Enter edit mode when adding this object");
    RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  }
  /* NOTE: this property gets hidden for add-camera operator. */
  prop = RNA_def_enum(
      ot->srna, "align", align_options, ALIGN_WORLD, "Align", "The alignment of the new object");
  RNA_def_property_update_runtime(prop, view_align_update);

  prop = RNA_def_float_vector_xyz(ot->srna,
                                  "location",
                                  3,
                                  nullptr,
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
                                nullptr,
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
                                  nullptr,
                                  -OBJECT_ADD_SIZE_MAXF,
                                  OBJECT_ADD_SIZE_MAXF,
                                  "Scale",
                                  "Scale for the newly added object",
                                  -1000.0f,
                                  1000.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

void add_mesh_props(wmOperatorType *ot)
{
  RNA_def_boolean(ot->srna, "calc_uvs", true, "Generate UVs", "Generate a default UV map");
}

void add_generic_get_opts(bContext *C,
                          wmOperator *op,
                          const char view_align_axis,
                          float r_loc[3],
                          float r_rot[3],
                          float r_scale[3],
                          bool *r_enter_editmode,
                          ushort *r_local_view_bits,
                          bool *r_is_view_aligned)
{
  /* Edit Mode! (optional) */
  {
    bool _enter_editmode;
    if (!r_enter_editmode) {
      r_enter_editmode = &_enter_editmode;
    }
    /* Only to ensure the value is _always_ set.
     * Typically the property will exist when the argument is non-null. */
    *r_enter_editmode = false;

    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "enter_editmode");
    if (prop != nullptr) {
      if (RNA_property_is_set(op->ptr, prop) && r_enter_editmode) {
        *r_enter_editmode = RNA_property_boolean_get(op->ptr, prop);
      }
      else {
        *r_enter_editmode = (U.flag & USER_ADD_EDITMODE) != 0;
        RNA_property_boolean_set(op->ptr, prop, *r_enter_editmode);
      }
    }
  }

  if (r_local_view_bits) {
    View3D *v3d = CTX_wm_view3d(C);
    *r_local_view_bits = (v3d && v3d->localvd) ? v3d->local_view_uid : 0;
  }

  /* Location! */
  {
    float _loc[3];
    if (!r_loc) {
      r_loc = _loc;
    }

    if (RNA_struct_property_is_set(op->ptr, "location")) {
      RNA_float_get_array(op->ptr, "location", r_loc);
    }
    else {
      location_from_view(C, r_loc);
      RNA_float_set_array(op->ptr, "location", r_loc);
    }
  }

  /* Rotation! */
  {
    bool _is_view_aligned;
    float _rot[3];
    if (!r_is_view_aligned) {
      r_is_view_aligned = &_is_view_aligned;
    }
    if (!r_rot) {
      r_rot = _rot;
    }

    if (RNA_struct_property_is_set(op->ptr, "rotation")) {
      /* If rotation is set, always use it. Alignment (and corresponding user preference)
       * can be ignored since this is in world space anyways.
       * To not confuse (e.g. on redo), don't set it to #ALIGN_WORLD in the op UI though. */
      *r_is_view_aligned = false;
      RNA_float_get_array(op->ptr, "rotation", r_rot);
    }
    else {
      int alignment = ALIGN_WORLD;
      PropertyRNA *prop = RNA_struct_find_property(op->ptr, "align");

      if (RNA_property_is_set(op->ptr, prop)) {
        /* If alignment is set, always use it. */
        *r_is_view_aligned = alignment == ALIGN_VIEW;
        alignment = RNA_property_enum_get(op->ptr, prop);
      }
      else {
        /* If alignment is not set, use User Preferences. */
        *r_is_view_aligned = (U.flag & USER_ADD_VIEWALIGNED) != 0;
        if (*r_is_view_aligned) {
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
          RNA_float_get_array(op->ptr, "rotation", r_rot);
          break;
        case ALIGN_VIEW:
          rotation_from_view(C, r_rot, view_align_axis);
          RNA_float_set_array(op->ptr, "rotation", r_rot);
          break;
        case ALIGN_CURSOR: {
          const Scene *scene = CTX_data_scene(C);
          const float3x3 tmat = scene->cursor.matrix<float3x3>();
          mat3_normalized_to_eul(r_rot, tmat.ptr());
          RNA_float_set_array(op->ptr, "rotation", r_rot);
          break;
        }
      }
    }
  }

  /* Scale! */
  {
    float _scale[3];
    if (!r_scale) {
      r_scale = _scale;
    }

    /* For now this is optional, we can make it always use. */
    copy_v3_fl(r_scale, 1.0f);

    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "scale");
    if (prop != nullptr) {
      if (RNA_property_is_set(op->ptr, prop)) {
        RNA_property_float_get_array(op->ptr, prop, r_scale);
      }
      else {
        copy_v3_fl(r_scale, 1.0f);
        RNA_property_float_set_array(op->ptr, prop, r_scale);
      }
    }
  }
}

Object *add_type_with_obdata(bContext *C,
                             const int type,
                             const char *name,
                             const float loc[3],
                             const float rot[3],
                             const bool enter_editmode,
                             const ushort local_view_bits,
                             ID *obdata)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  {
    BKE_view_layer_synced_ensure(scene, view_layer);
    Object *obedit = BKE_view_layer_edit_object_get(view_layer);
    if (obedit != nullptr) {
      editmode_exit_ex(bmain, scene, obedit, EM_FREEDATA);
    }
  }

  /* deselects all, sets active object */
  Object *ob;
  if (obdata != nullptr) {
    BLI_assert(type == BKE_object_obdata_to_type(obdata));
    ob = BKE_object_add_for_data(bmain, scene, view_layer, type, name, obdata, true);
    const short *materials_len_p = BKE_id_material_len_p(obdata);
    if (materials_len_p && *materials_len_p > 0) {
      BKE_object_materials_sync_length(bmain, ob, static_cast<ID *>(ob->data));
    }
  }
  else {
    ob = BKE_object_add(bmain, scene, view_layer, type, name);
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *ob_base_act = BKE_view_layer_active_base_get(view_layer);
  /* While not getting a valid base is not a good thing, it can happen in convoluted corner cases,
   * better not crash on it in releases. */
  BLI_assert(ob_base_act != nullptr);
  if (ob_base_act != nullptr) {
    ob_base_act->local_view_bits = local_view_bits;
    /* editor level activate, notifiers */
    base_activate(C, ob_base_act);
  }

  /* more editor stuff */
  init_transform_on_add(ob, loc, rot);

  /* TODO(sergey): This is weird to manually tag objects for update, better to
   * use DEG_id_tag_update here perhaps.
   */
  DEG_id_type_tag(bmain, ID_OB);
  DEG_relations_tag_update(bmain);
  if (ob->data != nullptr) {
    DEG_id_tag_update_ex(bmain, (ID *)ob->data, ID_RECALC_EDITORS);
  }

  if (enter_editmode) {
    editmode_enter_ex(bmain, scene, ob, 0);
  }

  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  ED_outliner_select_sync_from_object_tag(C);

  return ob;
}

Object *add_type(bContext *C,
                 const int type,
                 const char *name,
                 const float loc[3],
                 const float rot[3],
                 const bool enter_editmode,
                 const ushort local_view_bits)
{
  return add_type_with_obdata(C, type, name, loc, rot, enter_editmode, local_view_bits, nullptr);
}

static bool object_can_have_lattice_modifier(const Object *ob)
{
  return ELEM(ob->type,
              OB_MESH,
              OB_CURVES_LEGACY,
              OB_SURF,
              OB_FONT,
              OB_CURVES,
              OB_GREASE_PENCIL,
              OB_LATTICE);
}

/* for object add operator */
static wmOperatorStatus object_add_exec(bContext *C, wmOperator *op)
{
  ushort local_view_bits;
  bool enter_editmode;
  float loc[3], rot[3], radius;
  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  radius = RNA_float_get(op->ptr, "radius");
  Object *ob = add_type(
      C, RNA_enum_get(op->ptr, "type"), nullptr, loc, rot, enter_editmode, local_view_bits);

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

  /* API callbacks. */
  ot->exec = object_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  add_unit_props_radius(ot);
  PropertyRNA *prop = RNA_def_enum(ot->srna, "type", rna_enum_object_type_items, 0, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  add_generic_props(ot, true);
}

/* -------------------------------------------------------------------- */
/** \name Add Lattice Deformation to Selected Operator
 * \{ */

static std::optional<Bounds<float3>> lattice_add_to_selected_collect_targets_and_calc_bounds(
    bContext *C, const float orientation_matrix[3][3], Vector<Object *> &r_targets)
{
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = CTX_wm_view3d(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  Bounds<float3> local_bounds;
  local_bounds.min = float3(FLT_MAX);
  local_bounds.max = float3(-FLT_MAX);
  bool has_bounds = false;

  float inverse_orientation_matrix[3][3];
  invert_m3_m3_safe_ortho(inverse_orientation_matrix, orientation_matrix);

  LISTBASE_FOREACH (Base *, base, &view_layer->object_bases) {
    if (!BASE_SELECTED_EDITABLE(v3d, base) || !object_can_have_lattice_modifier(base->object)) {
      continue;
    }

    r_targets.append(base->object);
    const Object *object_eval = DEG_get_evaluated(depsgraph, base->object);
    if (object_eval && DEG_object_transform_is_evaluated(*object_eval)) {
      if (std::optional<Bounds<float3>> object_bounds = BKE_object_boundbox_get(object_eval)) {
        const float (*object_to_world_matrix)[4] = object_eval->object_to_world().ptr();
        /* Generate all 8 corners of the bounding box. */
        std::array<float3, 8> corners = bounds::corners(*object_bounds);
        for (float3 &corner : corners) {
          mul_m4_v3(object_to_world_matrix, corner);
          mul_m3_v3(inverse_orientation_matrix, corner);
          local_bounds.min = math::min(local_bounds.min, corner);
          local_bounds.max = math::max(local_bounds.max, corner);
        }
        has_bounds = true;
      }
    }
  }

  if (has_bounds) {
    return local_bounds;
  }
  return std::nullopt;
}

static wmOperatorStatus lattice_add_to_selected_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob_active = CTX_data_active_object(C);
  ushort local_view_bits;
  bool enter_editmode;
  float location[3], rotation_euler[3];
  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(
      C, op, 'Z', location, rotation_euler, nullptr, &enter_editmode, &local_view_bits, nullptr);

  const float margin = RNA_float_get(op->ptr, "margin");
  const bool add_modifiers = RNA_boolean_get(op->ptr, "add_modifiers");
  const int resolution_u = RNA_int_get(op->ptr, "resolution_u");
  const int resolution_v = RNA_int_get(op->ptr, "resolution_v");
  const int resolution_w = RNA_int_get(op->ptr, "resolution_w");
  CTX_data_ensure_evaluated_depsgraph(C);
  float orientation_matrix[3][3];

  if (ob_active) {
    copy_m3_m4(orientation_matrix, ob_active->object_to_world().ptr());
    normalize_m3(orientation_matrix);
  }
  else {
    unit_m3(orientation_matrix);
  }

  Vector<Object *> targets;
  std::optional<Bounds<float3>> bounds_opt =
      lattice_add_to_selected_collect_targets_and_calc_bounds(C, orientation_matrix, targets);

  /* Disable fit to selected when there are no valid targets
   * (either nothing is selected or meshes with no geometry). */
  if (targets.is_empty() || !bounds_opt.has_value()) {
    RNA_boolean_set(op->ptr, "fit_to_selected", false);
  }
  const bool fit_to_selected = RNA_boolean_get(op->ptr, "fit_to_selected");

  Object *ob_lattice = add_type(
      C, OB_LATTICE, nullptr, location, rotation_euler, enter_editmode, local_view_bits);
  Lattice *lt = (Lattice *)ob_lattice->data;

  if (fit_to_selected && bounds_opt.has_value()) {
    /* Calculate the center and size of this combined bounding box. */
    const float3 center_local = bounds_opt->center();
    const float3 size_local = bounds_opt->size() + float3(margin * 2);

    /* Orient lattice center and apply rotation. */
    float3 center_world = center_local;
    mul_m3_v3(orientation_matrix, center_world);
    BKE_object_mat3_to_rot(ob_lattice, orientation_matrix, false);

    copy_v3_v3(ob_lattice->loc, center_world);
    copy_v3_v3(ob_lattice->scale, size_local);

    /* Prevent invalid or zero lattice size, fallback to 1.0f. */
    for (int i = 0; i < 3; i++) {
      if (!isfinite(ob_lattice->scale[i]) || ob_lattice->scale[i] <= FLT_EPSILON) {
        ob_lattice->scale[i] = 1.0f;
      }
    }
  }
  else {
    /* Fallback when fit to selected is off. */
    copy_v3_fl(ob_lattice->scale, RNA_float_get(op->ptr, "radius"));

    /* Apply user specified Euler rotation instead of cached quat. */
    ob_lattice->rotmode = ROT_MODE_EUL;
    copy_v3_v3(ob_lattice->rot, rotation_euler);
  }

  if (add_modifiers) {
    for (Object *ob : targets) {
      BLI_assert(ob != ob_lattice);
      BLI_assert(object_can_have_lattice_modifier(ob));

      LatticeModifierData *lmd = (LatticeModifierData *)modifier_add(
          op->reports, bmain, scene, ob, nullptr, eModifierType_Lattice);
      if (UNLIKELY(lmd == nullptr)) {
        continue;
      }

      lmd->object = ob_lattice;
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);
    }
  }

  BKE_lattice_resize(
      lt, max_ii(1, resolution_u), max_ii(1, resolution_v), max_ii(1, resolution_w), ob_lattice);

  DEG_id_tag_update(&ob_lattice->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
  return OPERATOR_FINISHED;
}

static bool object_add_to_selected_poll_property(const bContext * /*C*/,
                                                 wmOperator *op,
                                                 const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);

  /* Shows only relevant redo properties.
   * If `fit_to_selected` is:
   * - true: location & rotation are ignored.
   * - false: margin is ignored since it only applies to the "fit".
   */
  if (RNA_boolean_get(op->ptr, "fit_to_selected")) {
    if (STR_ELEM(prop_id, "radius", "align", "location", "rotation")) {
      return false;
    }
  }
  else {
    if (STREQ(prop_id, "margin")) {
      return false;
    }
  }
  return true;
}

void OBJECT_OT_lattice_add_to_selected(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Lattice Deformer";
  ot->description = "Add a lattice and use it to deform selected objects";
  ot->idname = "OBJECT_OT_lattice_add_to_selected";

  /* API callbacks. */
  ot->exec = lattice_add_to_selected_exec;
  ot->poll = ED_operator_objectmode;
  ot->poll_property = object_add_to_selected_poll_property;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;

  prop = RNA_def_boolean(ot->srna,
                         "fit_to_selected",
                         true,
                         "Fit to Selected",
                         "Resize lattice to fit selected deformable objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  add_unit_props_radius(ot);
  prop = RNA_def_float(ot->srna,
                       "margin",
                       0.0f,
                       0.0f,
                       FLT_MAX,
                       "Margin",
                       "Add margin to lattice dimensions",
                       0.0f,
                       10.0f);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_boolean(ot->srna,
                         "add_modifiers",
                         true,
                         "Add Modifiers",
                         "Automatically add lattice modifiers to selected objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int(ot->srna,
                     "resolution_u",
                     2,
                     1,
                     64,
                     "Resolution U",
                     "Lattice resolution in U direction",
                     1,
                     64);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int(
      ot->srna, "resolution_v", 2, 1, 64, "V", "Lattice resolution in V direction", 1, 64);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_int(
      ot->srna, "resolution_w", 2, 1, 64, "W", "Lattice resolution in W direction", 1, 64);
  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Probe Operator
 * \{ */

/* for object add operator */
static const char *get_lightprobe_defname(int type)
{
  switch (type) {
    case LIGHTPROBE_TYPE_VOLUME:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Volume");
    case LIGHTPROBE_TYPE_PLANE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Plane");
    case LIGHTPROBE_TYPE_SPHERE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "Sphere");
    default:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_LIGHT, "LightProbe");
  }
}

static wmOperatorStatus lightprobe_add_exec(bContext *C, wmOperator *op)
{
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];
  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  int type = RNA_enum_get(op->ptr, "type");
  float radius = RNA_float_get(op->ptr, "radius");

  Object *ob = add_type(
      C, OB_LIGHTPROBE, get_lightprobe_defname(type), loc, rot, false, local_view_bits);
  copy_v3_fl(ob->scale, radius);

  LightProbe *probe = (LightProbe *)ob->data;

  BKE_lightprobe_type_set(probe, type);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lightprobe_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Light Probe";
  ot->description = "Add a light probe object";
  ot->idname = "OBJECT_OT_lightprobe_add";

  /* API callbacks. */
  ot->exec = lightprobe_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", lightprobe_type_items, 0, "Type", "");

  add_unit_props_radius(ot);
  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Effector Operator
 * \{ */

/* for object add operator */

static const char *get_effector_defname(ePFieldType type)
{
  switch (type) {
    case PFIELD_FORCE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Force");
    case PFIELD_VORTEX:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Vortex");
    case PFIELD_MAGNET:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Magnet");
    case PFIELD_WIND:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Wind");
    case PFIELD_GUIDE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "CurveGuide");
    case PFIELD_TEXTURE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "TextureField");
    case PFIELD_HARMONIC:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Harmonic");
    case PFIELD_CHARGE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Charge");
    case PFIELD_LENNARDJ:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Lennard-Jones");
    case PFIELD_BOID:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Boid");
    case PFIELD_TURBULENCE:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Turbulence");
    case PFIELD_DRAG:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Drag");
    case PFIELD_FLUIDFLOW:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "FluidField");
    case PFIELD_NULL:
      return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Field");
    case NUM_PFIELD_TYPES:
      break;
  }

  BLI_assert(false);
  return CTX_DATA_(BLT_I18NCONTEXT_ID_OBJECT, "Field");
}

static wmOperatorStatus effector_add_exec(bContext *C, wmOperator *op)
{
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];
  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  const ePFieldType type = static_cast<ePFieldType>(RNA_enum_get(op->ptr, "type"));
  float dia = RNA_float_get(op->ptr, "radius");

  Object *ob;
  if (type == PFIELD_GUIDE) {
    Main *bmain = CTX_data_main(C);
    Scene *scene = CTX_data_scene(C);
    ob = add_type(
        C, OB_CURVES_LEGACY, get_effector_defname(type), loc, rot, false, local_view_bits);

    Curve *cu = static_cast<Curve *>(ob->data);
    cu->flag |= CU_PATH | CU_3D;
    editmode_enter_ex(bmain, scene, ob, 0);

    float mat[4][4];
    new_primitive_matrix(C, ob, loc, rot, nullptr, mat);
    mul_mat3_m4_fl(mat, dia);
    BLI_addtail(&cu->editnurb->nurbs,
                ED_curve_add_nurbs_primitive(C, ob, mat, CU_NURBS | CU_PRIM_PATH, 1));
    if (!enter_editmode) {
      editmode_exit_ex(bmain, scene, ob, EM_FREEDATA);
    }
  }
  else {
    ob = add_type(C, OB_EMPTY, get_effector_defname(type), loc, rot, false, local_view_bits);
    BKE_object_obdata_size_init(ob, dia);
    if (ELEM(type, PFIELD_WIND, PFIELD_VORTEX)) {
      ob->empty_drawtype = OB_SINGLE_ARROW;
    }
  }

  ob->pd = BKE_partdeflect_new(type);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_effector_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Effector";
  ot->description = "Add an empty object with a physics effector to the scene";
  ot->idname = "OBJECT_OT_effector_add";

  /* API callbacks. */
  ot->exec = effector_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", field_type_items, 0, "Type", "");

  add_unit_props_radius(ot);
  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Camera Operator
 * \{ */

static wmOperatorStatus object_camera_add_exec(bContext *C, wmOperator *op)
{
  View3D *v3d = CTX_wm_view3d(C);
  Scene *scene = CTX_data_scene(C);

  /* force view align for cameras */
  RNA_enum_set(op->ptr, "align", ALIGN_VIEW);

  ushort local_view_bits;
  bool enter_editmode;
  float loc[3], rot[3];
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  Object *ob = add_type(C, OB_CAMERA, nullptr, loc, rot, false, local_view_bits);

  if (v3d) {
    if (v3d->camera == nullptr) {
      v3d->camera = ob;
    }
    if (v3d->scenelock && scene->camera == nullptr) {
      scene->camera = ob;
    }
  }

  Camera *cam = static_cast<Camera *>(ob->data);
  cam->drawsize = v3d ? ED_view3d_grid_scale(scene, v3d, nullptr) :
                        ED_scene_grid_scale(scene, nullptr);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_camera_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Camera";
  ot->description = "Add a camera object to the scene";
  ot->idname = "OBJECT_OT_camera_add";

  /* API callbacks. */
  ot->exec = object_camera_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  add_generic_props(ot, true);

  /* hide this for cameras, default */
  prop = RNA_struct_type_find_property(ot->srna, "align");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Metaball Operator
 * \{ */

static wmOperatorStatus object_metaball_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  ushort local_view_bits;
  bool enter_editmode;
  float loc[3], rot[3];
  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  bool newob = false;
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);
  if (obedit == nullptr || obedit->type != OB_MBALL) {
    obedit = add_type(C, OB_MBALL, nullptr, loc, rot, true, local_view_bits);
    newob = true;
  }
  else {
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  float mat[4][4];
  new_primitive_matrix(C, obedit, loc, rot, nullptr, mat);
  /* Halving here is done to account for constant values from #BKE_mball_element_add.
   * While the default radius of the resulting meta element is 2,
   * we want to pass in 1 so other values such as resolution are scaled by 1.0. */
  float dia = RNA_float_get(op->ptr, "radius") / 2;

  ED_mball_add_primitive(C, obedit, newob, mat, dia, RNA_enum_get(op->ptr, "type"));

  /* userdef */
  if (newob && !enter_editmode) {
    editmode_exit_ex(bmain, scene, obedit, EM_FREEDATA);
  }
  else {
    /* Only needed in edit-mode (#add_type normally handles this). */
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_metaball_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Metaball";
  ot->description = "Add an metaball object to the scene";
  ot->idname = "OBJECT_OT_metaball_add";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_metaball_add_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_metaelem_type_items, 0, "Primitive", "");

  add_unit_props_radius_ex(ot, 2.0f);
  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Text Operator
 * \{ */

static wmOperatorStatus object_add_text_exec(bContext *C, wmOperator *op)
{
  Object *obedit = CTX_data_edit_object(C);
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3];

  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  if (obedit && obedit->type == OB_FONT) {
    return OPERATOR_CANCELLED;
  }

  obedit = add_type(C, OB_FONT, nullptr, loc, rot, enter_editmode, local_view_bits);
  BKE_object_obdata_size_init(obedit, RNA_float_get(op->ptr, "radius"));

  return OPERATOR_FINISHED;
}

void OBJECT_OT_text_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Text";
  ot->description = "Add a text object to the scene";
  ot->idname = "OBJECT_OT_text_add";

  /* API callbacks. */
  ot->exec = object_add_text_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  add_unit_props_radius(ot);
  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Armature Operator
 * \{ */

static wmOperatorStatus object_armature_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Object *obedit = BKE_view_layer_edit_object_get(view_layer);

  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  bool newob = false;
  bool enter_editmode;
  ushort local_view_bits;
  float loc[3], rot[3], dia;
  bool view_aligned = rv3d && (U.flag & USER_ADD_VIEWALIGNED);

  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  if ((obedit == nullptr) || (obedit->type != OB_ARMATURE)) {
    obedit = add_type(C, OB_ARMATURE, nullptr, loc, rot, true, local_view_bits);
    editmode_enter_ex(bmain, scene, obedit, 0);
    newob = true;
  }
  else {
    DEG_id_tag_update(&obedit->id, ID_RECALC_GEOMETRY);
  }

  if (obedit == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Cannot create editmode armature");
    return OPERATOR_CANCELLED;
  }

  /* Give the Armature its default bone collection. */
  bArmature *armature = static_cast<bArmature *>(obedit->data);
  BoneCollection *default_bonecoll = ANIM_armature_bonecoll_new(armature, "");
  ANIM_armature_bonecoll_active_set(armature, default_bonecoll);

  dia = RNA_float_get(op->ptr, "radius");
  ED_armature_ebone_add_primitive(obedit, dia, view_aligned);

  /* userdef */
  if (newob && !enter_editmode) {
    editmode_exit_ex(bmain, scene, obedit, EM_FREEDATA);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_armature_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Armature";
  ot->description = "Add an armature object to the scene";
  ot->idname = "OBJECT_OT_armature_add";

  /* API callbacks. */
  ot->exec = object_armature_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  add_unit_props_radius(ot);
  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Empty Operator
 * \{ */

static wmOperatorStatus object_empty_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  int type = RNA_enum_get(op->ptr, "type");
  ushort local_view_bits;
  float loc[3], rot[3];

  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  ob = add_type(C, OB_EMPTY, nullptr, loc, rot, false, local_view_bits);

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

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_empty_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_empty_drawtype_items, 0, "Type", "");

  add_unit_props_radius(ot);
  add_generic_props(ot, false);
}

static wmOperatorStatus object_image_add_exec(bContext *C, wmOperator *op)
{
  Image *ima = nullptr;

  ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
  if (!ima) {
    return OPERATOR_CANCELLED;
  }

  if (!ED_operator_objectmode(C)) {
    BKE_report(op->reports, RPT_ERROR, "Image objects can only be added in Object Mode");
    return OPERATOR_CANCELLED;
  }

  /* Add new empty. */
  ushort local_view_bits;
  float loc[3], rot[3];

  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  Object *ob = add_type(C, OB_EMPTY, nullptr, loc, rot, false, local_view_bits);
  ob->empty_drawsize = 5.0f;

  if (RNA_boolean_get(op->ptr, "background")) {
    /* When "background" has been set to "true", set image to render in the background. */
    ob->empty_image_depth = OB_EMPTY_IMAGE_DEPTH_BACK;
    ob->empty_image_visibility_flag = OB_EMPTY_IMAGE_HIDE_BACK;

    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d->persp != RV3D_PERSP) {
      ob->empty_image_visibility_flag |= OB_EMPTY_IMAGE_HIDE_PERSPECTIVE;
    }
  }

  BKE_object_empty_draw_type_set(ob, OB_EMPTY_IMAGE);

  ob->data = ima;

  return OPERATOR_FINISHED;
}

static wmOperatorStatus object_image_add_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!RNA_struct_property_is_set(op->ptr, "align")) {
    /* Default to Aligned unless something else was explicitly passed. */
    RNA_enum_set(op->ptr, "align", ALIGN_VIEW);
  }

  /* Check if the user has not specified the image to load.
   * If they have not, assume this is a drag an drop operation. */
  if (!RNA_struct_property_is_set(op->ptr, "filepath") &&
      !WM_operator_properties_id_lookup_is_set(op->ptr))
  {
    WM_event_add_fileselect(C, op);
    return OPERATOR_RUNNING_MODAL;
  }

  if (!RNA_struct_property_is_set(op->ptr, "background")) {
    /* Check if we should switch to "background" mode. */
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d->persp != RV3D_PERSP) {
      RNA_boolean_set(op->ptr, "background", true);
    }
  }

  float loc[3];
  location_from_view(C, loc);
  ED_view3d_cursor3d_position(C, event->mval, false, loc);
  RNA_float_set_array(op->ptr, "location", loc);

  Object *ob_cursor = ED_view3d_give_object_under_cursor(C, event->mval);

  /* Either change empty under cursor or create a new empty. */
  if (!ob_cursor || ob_cursor->type != OB_EMPTY) {
    return object_image_add_exec(C, op);
  }
  /* User dropped an image on an existing image. */
  Image *ima = nullptr;

  ima = (Image *)WM_operator_drop_load_path(C, op, ID_IM);
  if (!ima) {
    return OPERATOR_CANCELLED;
  }
  /* Handled below. */
  id_us_min(&ima->id);

  Scene *scene = CTX_data_scene(C);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  DEG_id_tag_update((ID *)ob_cursor, ID_RECALC_TRANSFORM);

  BKE_object_empty_draw_type_set(ob_cursor, OB_EMPTY_IMAGE);

  id_us_min(static_cast<ID *>(ob_cursor->data));
  ob_cursor->data = ima;
  id_us_plus(static_cast<ID *>(ob_cursor->data));
  return OPERATOR_FINISHED;
}

static bool object_image_add_poll(bContext *C)
{
  return CTX_wm_region_view3d(C);
}

void OBJECT_OT_empty_image_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Empty Image/Drop Image to Empty";
  ot->description = "Add an empty image type to scene with data";
  ot->idname = "OBJECT_OT_empty_image_add";

  /* API callbacks. */
  ot->invoke = object_image_add_invoke;
  ot->exec = object_image_add_exec;
  ot->poll = object_image_add_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_IMAGE | FILE_TYPE_MOVIE,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH | WM_FILESEL_RELPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);

  WM_operator_properties_id_lookup(ot, true);
  add_generic_props(ot, false);
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "background",
                         false,
                         "Put in Background",
                         "Make the image render behind all objects");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  /* Hide the filepath and relative path prop */
  prop = RNA_struct_type_find_property(ot->srna, "filepath");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_PRESET);
  prop = RNA_struct_type_find_property(ot->srna, "relative_path");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Grease Pencil Operator
 * \{ */

static EnumPropertyItem rna_enum_gpencil_add_stroke_depth_order_items[] = {
    {GP_DRAWMODE_2D,
     "2D",
     0,
     "2D Layers",
     "Display strokes using Grease Pencil layers to define order"},
    {GP_DRAWMODE_3D, "3D", 0, "3D Location", "Display strokes using real 3D position in 3D space"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus object_grease_pencil_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *original_active_object = CTX_data_active_object(C);
  /* TODO: For now, only support adding the 'Stroke' type. */
  const int type = RNA_enum_get(op->ptr, "type");

  ushort local_view_bits;
  float loc[3], rot[3];

  /* NOTE: We use 'Y' here (not 'Z'), as. */
  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Y', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  const char *ob_name = nullptr;
  switch (type) {
    case GP_EMPTY: {
      ob_name = CTX_DATA_(BLT_I18NCONTEXT_ID_GPENCIL, "GPencil");
      break;
    }
    case GP_STROKE: {
      ob_name = CTX_DATA_(BLT_I18NCONTEXT_ID_GPENCIL, "Stroke");
      break;
    }
    case GP_MONKEY: {
      ob_name = CTX_DATA_(BLT_I18NCONTEXT_ID_GPENCIL, "Suzanne");
      break;
    }
    case GREASE_PENCIL_LINEART_OBJECT:
    case GREASE_PENCIL_LINEART_SCENE:
    case GREASE_PENCIL_LINEART_COLLECTION: {
      ob_name = CTX_DATA_(BLT_I18NCONTEXT_ID_GPENCIL, "LineArt");
      break;
    }
    default: {
      break;
    }
  }

  Object *object = add_type(C, OB_GREASE_PENCIL, ob_name, loc, rot, false, local_view_bits);
  GreasePencil &grease_pencil_id = *static_cast<GreasePencil *>(object->data);
  const bool use_in_front = RNA_boolean_get(op->ptr, "use_in_front");
  const bool use_lights = RNA_boolean_get(op->ptr, "use_lights");

  switch (type) {
    case GP_EMPTY: {
      greasepencil::create_blank(*bmain, *object, scene->r.cfra);
      break;
    }
    case GP_STROKE: {
      const float radius = RNA_float_get(op->ptr, "radius");
      const float3 scale(radius);

      float4x4 mat;
      new_primitive_matrix(C, object, loc, rot, scale, mat.ptr());

      greasepencil::create_stroke(*bmain, *object, mat, scene->r.cfra);
      break;
    }
    case GP_MONKEY: {
      const float radius = RNA_float_get(op->ptr, "radius");
      const float3 scale(radius);

      float4x4 mat;
      new_primitive_matrix(C, object, loc, rot, scale, mat.ptr());

      greasepencil::create_suzanne(*bmain, *object, mat, scene->r.cfra);
      break;
    }
    case GREASE_PENCIL_LINEART_OBJECT:
    case GREASE_PENCIL_LINEART_SCENE:
    case GREASE_PENCIL_LINEART_COLLECTION: {
      const int type = RNA_enum_get(op->ptr, "type");
      const int stroke_depth_order = RNA_enum_get(op->ptr, "stroke_depth_order");
      const float stroke_depth_offset = RNA_float_get(op->ptr, "stroke_depth_offset");

      greasepencil::create_blank(*bmain, *object, scene->r.cfra);

      auto *grease_pencil = reinterpret_cast<GreasePencil *>(object->data);
      auto *new_md = BKE_modifier_new(eModifierType_GreasePencilLineart);
      auto *md = reinterpret_cast<GreasePencilLineartModifierData *>(new_md);

      BLI_addtail(&object->modifiers, md);
      BKE_modifier_unique_name(&object->modifiers, new_md);
      BKE_modifiers_persistent_uid_init(*object, *new_md);

      if (type == GREASE_PENCIL_LINEART_COLLECTION) {
        md->source_type = LINEART_SOURCE_COLLECTION;
        md->source_collection = CTX_data_collection(C);
      }
      else if (type == GREASE_PENCIL_LINEART_OBJECT) {
        md->source_type = LINEART_SOURCE_OBJECT;
        md->source_object = original_active_object;
      }
      else {
        /* Whole scene. */
        md->source_type = LINEART_SOURCE_SCENE;
      }
      /* Only created one layer and one material. */
      STRNCPY_UTF8(md->target_layer, grease_pencil->get_active_layer()->name().c_str());
      md->target_material = BKE_object_material_get(object, 0);
      if (md->target_material) {
        id_us_plus(&md->target_material->id);
      }

      if (!use_in_front) {
        if (stroke_depth_order == GP_DRAWMODE_3D) {
          grease_pencil->flag |= GREASE_PENCIL_STROKE_ORDER_3D;
        }
        md->stroke_depth_offset = stroke_depth_offset;
      }

      break;
    }
  }

  SET_FLAG_FROM_TEST(object->dtx, use_in_front, OB_DRAW_IN_FRONT);
  SET_FLAG_FROM_TEST(object->dtx, use_lights, OB_USE_GPENCIL_LIGHTS);

  for (blender::bke::greasepencil::Layer *layer : grease_pencil_id.layers_for_write()) {
    SET_FLAG_FROM_TEST(layer->as_node().flag, use_lights, GP_LAYER_TREE_NODE_USE_LIGHTS);
  }

  DEG_id_tag_update(&grease_pencil_id.id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, &grease_pencil_id.id);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus object_grease_pencil_add_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent * /*event*/)
{
  const int type = RNA_enum_get(op->ptr, "type");

  /* Only disable "use_in_front" if it's one of the non-LineArt types */
  if (ELEM(type, GP_EMPTY, GP_STROKE, GP_MONKEY)) {
    RNA_boolean_set(op->ptr, "use_in_front", false);
  }

  return object_grease_pencil_add_exec(C, op);
}

void OBJECT_OT_grease_pencil_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Grease Pencil";
  ot->description = "Add a Grease Pencil object to the scene";
  ot->idname = "OBJECT_OT_grease_pencil_add";

  /* API callbacks. */
  ot->exec = object_grease_pencil_add_exec;
  ot->invoke = object_grease_pencil_add_invoke;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_object_gpencil_type_items, 0, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_boolean(ot->srna,
                  "use_in_front",
                  true,
                  "Show In Front",
                  "Show Line Art Grease Pencil in front of everything");
  RNA_def_float(ot->srna,
                "stroke_depth_offset",
                0.05f,
                0.0f,
                FLT_MAX,
                "Stroke Offset",
                "Stroke offset for the Line Art modifier",
                0.0f,
                0.5f);
  RNA_def_boolean(
      ot->srna, "use_lights", true, "Use Lights", "Use lights for this Grease Pencil object");
  RNA_def_enum(
      ot->srna,
      "stroke_depth_order",
      rna_enum_gpencil_add_stroke_depth_order_items,
      GP_DRAWMODE_3D,
      "Stroke Depth Order",
      "Defines how the strokes are ordered in 3D space (for objects not displayed 'In Front')");

  add_unit_props_radius(ot);
  add_generic_props(ot, false);
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

static wmOperatorStatus object_light_add_exec(bContext *C, wmOperator *op)
{
  Object *ob;
  Light *la;
  int type = RNA_enum_get(op->ptr, "type");
  ushort local_view_bits;
  float loc[3], rot[3];

  WM_operator_view3d_unit_defaults(C, op);
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  ob = add_type(C, OB_LAMP, get_light_defname(type), loc, rot, false, local_view_bits);

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

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_light_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "type", rna_enum_light_type_items, 0, "Type", "");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_ID_LIGHT);

  add_unit_props_radius(ot);
  add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Collection Instance Operator
 * \{ */

struct CollectionAddInfo {
  /* The collection that is supposed to be added, determined through operator properties. */
  Collection *collection;
  /* The local-view bits (if any) the object should have set to become visible in current context.
   */
  ushort local_view_bits;
  /* The transform that should be applied to the collection, determined through operator properties
   * if set (e.g. to place the collection under the cursor), otherwise through context (e.g. 3D
   * cursor location). */
  float loc[3], rot[3], scale[3];
};

static std::optional<CollectionAddInfo> collection_add_info_get_from_op(bContext *C,
                                                                        wmOperator *op)
{
  CollectionAddInfo add_info{};

  Main *bmain = CTX_data_main(C);

  PropertyRNA *prop_location = RNA_struct_find_property(op->ptr, "location");

  add_info.collection = reinterpret_cast<Collection *>(
      WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_GR));

  bool update_location_if_necessary = false;
  if (add_info.collection) {
    update_location_if_necessary = true;
  }
  else {
    add_info.collection = static_cast<Collection *>(
        BLI_findlink(&bmain->collections, RNA_enum_get(op->ptr, "collection")));
  }

  if (update_location_if_necessary && CTX_wm_region_view3d(C)) {
    int mval[2];
    if (!RNA_property_is_set(op->ptr, prop_location) && object_add_drop_xy_get(C, op, &mval)) {
      location_from_view(C, add_info.loc);
      ED_view3d_cursor3d_position(C, mval, false, add_info.loc);
      RNA_property_float_set_array(op->ptr, prop_location, add_info.loc);
    }
  }

  if (add_info.collection == nullptr) {
    return std::nullopt;
  }

  add_generic_get_opts(C,
                       op,
                       'Z',
                       add_info.loc,
                       add_info.rot,
                       add_info.scale,
                       nullptr,
                       &add_info.local_view_bits,
                       nullptr);

  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* Avoid dependency cycles. */
  LayerCollection *active_lc = BKE_layer_collection_get_active(view_layer);
  while (BKE_collection_cycle_find(active_lc->collection, add_info.collection)) {
    active_lc = BKE_layer_collection_activate_parent(view_layer, active_lc);
  }

  return add_info;
}

static wmOperatorStatus collection_instance_add_exec(bContext *C, wmOperator *op)
{
  std::optional<CollectionAddInfo> add_info = collection_add_info_get_from_op(C, op);
  if (!add_info) {
    return OPERATOR_CANCELLED;
  }

  Object *ob = add_type(C,
                        OB_EMPTY,
                        add_info->collection->id.name + 2,
                        add_info->loc,
                        add_info->rot,
                        false,
                        add_info->local_view_bits);
  /* `add_type()` does not have scale argument so copy that value separately. */
  copy_v3_v3(ob->scale, add_info->scale);
  ob->instance_collection = add_info->collection;
  ob->empty_drawsize = U.collection_instance_empty_size;
  ob->transflag |= OB_DUPLICOLLECTION;
  id_us_plus(&add_info->collection->id);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus object_instance_add_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  if (!object_add_drop_xy_is_set(op)) {
    RNA_int_set(op->ptr, "drop_x", event->xy[0]);
    RNA_int_set(op->ptr, "drop_y", event->xy[1]);
  }

  if (!WM_operator_properties_id_lookup_is_set(op->ptr)) {
    return WM_enum_search_invoke(C, op, event);
  }
  return op->type->exec(C, op);
}

void OBJECT_OT_collection_instance_add(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Add Collection Instance";
  ot->description = "Add a collection instance";
  ot->idname = "OBJECT_OT_collection_instance_add";

  /* API callbacks. */
  ot->invoke = object_instance_add_invoke;
  ot->exec = collection_instance_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(
      ot->srna, "name", "Collection", MAX_ID_NAME - 2, "Name", "Collection name to add");
  prop = RNA_def_enum(ot->srna, "collection", rna_enum_dummy_NULL_items, 0, "Collection", "");
  RNA_def_enum_funcs(prop, RNA_collection_itemf);
  RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
  add_generic_props(ot, false);

  WM_operator_properties_id_lookup(ot, false);

  object_add_drop_xy_props(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Collection Drop Operator
 *
 * Internal operator for collection dropping.
 *
 * \warning This is tied closely together to the drop-box callbacks, so it shouldn't be used on its
 *          own.
 *
 * The drop-box callback imports the collection, links it into the view-layer, selects all imported
 * objects (which may include peripheral objects like parents or boolean-objects of an object in
 * the collection) and activates one. Only the callback has enough info to do this reliably. Based
 * on the instancing operator option, this operator then does one of two things:
 * - Instancing enabled: Unlink the collection again, and instead add a collection instance empty
 *   at the drop position.
 * - Instancing disabled: Transform the objects to the drop position, keeping all relative
 *   transforms of the objects to each other as is.
 *
 * \{ */

static wmOperatorStatus collection_drop_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  LayerCollection *active_collection = CTX_data_layer_collection(C);
  std::optional<CollectionAddInfo> add_info = collection_add_info_get_from_op(C, op);
  if (!add_info) {
    return OPERATOR_CANCELLED;
  }

  if (RNA_boolean_get(op->ptr, "use_instance")) {
    BKE_collection_child_remove(bmain, active_collection->collection, add_info->collection);
    DEG_id_tag_update(&active_collection->collection->id, ID_RECALC_SYNC_TO_EVAL);
    DEG_relations_tag_update(bmain);

    Object *ob = add_type(C,
                          OB_EMPTY,
                          add_info->collection->id.name + 2,
                          add_info->loc,
                          add_info->rot,
                          false,
                          add_info->local_view_bits);
    ob->instance_collection = add_info->collection;
    ob->empty_drawsize = U.collection_instance_empty_size;
    ob->transflag |= OB_DUPLICOLLECTION;
    id_us_plus(&add_info->collection->id);
  }
  else if (ID_IS_EDITABLE(&add_info->collection->id)) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    float delta_mat[4][4];
    unit_m4(delta_mat);

    const float scale[3] = {1.0f, 1.0f, 1.0f};
    loc_eul_size_to_mat4(delta_mat, add_info->loc, add_info->rot, scale);

    float offset[3];
    /* Reverse apply the instance offset, so toggling the Instance option doesn't cause the
     * collection to jump. */
    negate_v3_v3(offset, add_info->collection->instance_offset);
    translate_m4(delta_mat, UNPACK3(offset));

    ObjectsInViewLayerParams params = {0};
    Vector<Object *> objects = BKE_view_layer_array_selected_objects_params(
        view_layer, nullptr, &params);
    object_xform_array_m4(objects.data(), objects.size(), delta_mat);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_collection_external_asset_drop(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  /* Name should only be displayed in the drag tooltip. */
  ot->name = "Add Collection";
  ot->description = "Add the dragged collection to the scene";
  ot->idname = "OBJECT_OT_collection_external_asset_drop";

  /* API callbacks. */
  ot->invoke = object_instance_add_invoke;
  ot->exec = collection_drop_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  /* properties */
  WM_operator_properties_id_lookup(ot, false);

  add_generic_props(ot, false);

  prop = RNA_def_boolean(ot->srna,
                         "use_instance",
                         true,
                         "Instance",
                         "Add the dropped collection as collection instance");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  object_add_drop_xy_props(ot);

  prop = RNA_def_enum(ot->srna, "collection", rna_enum_dummy_NULL_items, 0, "Collection", "");
  RNA_def_enum_funcs(prop, RNA_collection_itemf);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN | PROP_ENUM_NO_TRANSLATE);
  ot->prop = prop;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Data Instance Operator
 *
 * Use for dropping ID's from the outliner.
 * \{ */

static wmOperatorStatus object_data_instance_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  ID *id = nullptr;
  ushort local_view_bits;
  float loc[3], rot[3];

  PropertyRNA *prop_type = RNA_struct_find_property(op->ptr, "type");
  PropertyRNA *prop_location = RNA_struct_find_property(op->ptr, "location");

  const short id_type = RNA_property_enum_get(op->ptr, prop_type);
  id = WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, (ID_Type)id_type);
  if (id == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const int object_type = BKE_object_obdata_to_type(id);
  if (object_type == -1) {
    return OPERATOR_CANCELLED;
  }

  if (CTX_wm_region_view3d(C)) {
    int mval[2];
    if (!RNA_property_is_set(op->ptr, prop_location) && object_add_drop_xy_get(C, op, &mval)) {
      location_from_view(C, loc);
      ED_view3d_cursor3d_position(C, mval, false, loc);
      RNA_property_float_set_array(op->ptr, prop_location, loc);
    }
  }

  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  add_type_with_obdata(C, object_type, id->name + 2, loc, rot, false, local_view_bits, id);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_data_instance_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Object Data Instance";
  ot->description = "Add an object data instance";
  ot->idname = "OBJECT_OT_data_instance_add";

  /* API callbacks. */
  ot->invoke = object_add_drop_xy_generic_invoke;
  ot->exec = object_data_instance_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_id_lookup(ot, true);
  PropertyRNA *prop = RNA_def_enum(ot->srna, "type", rna_enum_id_type_items, 0, "Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  add_generic_props(ot, false);

  object_add_drop_xy_props(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Speaker Operator
 * \{ */

static wmOperatorStatus object_speaker_add_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  ushort local_view_bits;
  float loc[3], rot[3];
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  Object *ob = add_type(C, OB_SPEAKER, nullptr, loc, rot, false, local_view_bits);
  const bool is_liboverride = ID_IS_OVERRIDE_LIBRARY(ob);

  /* To make it easier to start using this immediately in NLA, a default sound clip is created
   * ready to be moved around to re-time the sound and/or make new sound clips. */
  {
    /* create new data for NLA hierarchy */
    AnimData *adt = BKE_animdata_ensure_id(&ob->id);
    NlaTrack *nlt = BKE_nlatrack_new_tail(&adt->nla_tracks, is_liboverride);
    BKE_nlatrack_set_active(&adt->nla_tracks, nlt);
    NlaStrip *strip = BKE_nla_add_soundstrip(bmain, scene, static_cast<Speaker *>(ob->data));
    strip->start = scene->r.cfra;
    strip->end += strip->start;

    /* hook them up */
    BKE_nlatrack_add_strip(nlt, strip, is_liboverride);

    /* Auto-name the strip, and give the track an interesting name. */
    STRNCPY_UTF8(nlt->name, DATA_("SoundTrack"));
    BKE_nlastrip_validate_name(adt, strip);

    WM_event_add_notifier(C, NC_ANIMATION | ND_NLA | NA_ADDED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_speaker_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Speaker";
  ot->description = "Add a speaker object to the scene";
  ot->idname = "OBJECT_OT_speaker_add";

  /* API callbacks. */
  ot->exec = object_speaker_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  add_generic_props(ot, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Curves Operator
 * \{ */

static wmOperatorStatus object_curves_random_add_exec(bContext *C, wmOperator *op)
{
  ushort local_view_bits;
  float loc[3], rot[3];
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  Object *object = add_type(C, OB_CURVES, nullptr, loc, rot, false, local_view_bits);

  Curves *curves_id = static_cast<Curves *>(object->data);
  curves_id->geometry.wrap() = ed::curves::primitive_random_sphere(500, 8);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_curves_random_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Random Curves";
  ot->description = "Add a curves object with random curves to the scene";
  ot->idname = "OBJECT_OT_curves_random_add";

  /* API callbacks. */
  ot->exec = object_curves_random_add_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  add_generic_props(ot, false);
}

static wmOperatorStatus object_curves_empty_hair_add_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);

  ushort local_view_bits;
  add_generic_get_opts(C, op, 'Z', nullptr, nullptr, nullptr, nullptr, &local_view_bits, nullptr);

  Object *surface_ob = CTX_data_active_object(C);
  BLI_assert(surface_ob != nullptr);

  Object *curves_ob = add_type(C, OB_CURVES, nullptr, nullptr, nullptr, false, local_view_bits);
  BKE_object_apply_mat4(curves_ob, surface_ob->object_to_world().ptr(), false, false);

  /* Set surface object. */
  Curves *curves_id = static_cast<Curves *>(curves_ob->data);
  curves_id->surface = surface_ob;

  /* Parent to surface object. */
  parent_set(op->reports, C, scene, curves_ob, surface_ob, PAR_OBJECT, false, true, nullptr);

  /* Decide which UV map to use for attachment. */
  Mesh *surface_mesh = static_cast<Mesh *>(surface_ob->data);
  const StringRef uv_name = surface_mesh->active_uv_map_name();
  if (!uv_name.is_empty()) {
    curves_id->surface_uv_map = BLI_strdupn(uv_name.data(), uv_name.size());
  }

  /* Add deformation modifier. */
  ed::curves::ensure_surface_deformation_node_exists(*C, *curves_ob);

  /* Make sure the surface object has a rest position attribute which is necessary for
   * deformations. */
  surface_ob->modifier_flag |= OB_MODIFIER_FLAG_ADD_REST_POSITION;

  return OPERATOR_FINISHED;
}

static bool object_curves_empty_hair_add_poll(bContext *C)
{
  if (!ED_operator_objectmode(C)) {
    return false;
  }
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_MESH) {
    CTX_wm_operator_poll_msg_set(C, "No active mesh object");
    return false;
  }
  return true;
}

void OBJECT_OT_curves_empty_hair_add(wmOperatorType *ot)
{
  ot->name = "Add Empty Curves";
  ot->description = "Add an empty curve object to the scene with the selected mesh as surface";
  ot->idname = "OBJECT_OT_curves_empty_hair_add";

  ot->exec = object_curves_empty_hair_add_exec;
  ot->poll = object_curves_empty_hair_add_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Point Cloud Operator
 * \{ */

static wmOperatorStatus object_pointcloud_add_exec(bContext *C, wmOperator *op)
{
  ushort local_view_bits;
  float loc[3], rot[3];
  add_generic_get_opts(C, op, 'Z', loc, rot, nullptr, nullptr, &local_view_bits, nullptr);

  Object *object = add_type(C, OB_POINTCLOUD, nullptr, loc, rot, false, local_view_bits);
  PointCloud &pointcloud = *static_cast<PointCloud *>(object->data);
  pointcloud.totpoint = 400;

  bke::MutableAttributeAccessor attributes = pointcloud.attributes_for_write();
  bke::SpanAttributeWriter<float3> position = attributes.lookup_or_add_for_write_only_span<float3>(
      "position", bke::AttrDomain::Point);
  bke::SpanAttributeWriter<float> radii = attributes.lookup_or_add_for_write_only_span<float>(
      "radius", bke::AttrDomain::Point);

  RandomNumberGenerator rng(0);
  for (const int i : position.span.index_range()) {
    position.span[i] = float3(rng.get_float(), rng.get_float(), rng.get_float()) * 2.0f - 1.0f;
    radii.span[i] = 0.05f * rng.get_float();
  }

  position.finish();
  radii.finish();

  return OPERATOR_FINISHED;
}

void OBJECT_OT_pointcloud_random_add(wmOperatorType *ot)
{
  ot->name = "Add Point Cloud";
  ot->description = "Add a point cloud object to the scene";
  ot->idname = "OBJECT_OT_pointcloud_random_add";

  ot->exec = object_pointcloud_add_exec;
  ot->poll = ED_operator_objectmode;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  add_generic_props(ot, false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Delete Object Operator
 * \{ */

void base_free_and_unlink(Main *bmain, Scene *scene, Object *ob)
{
  if (ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0 &&
      BKE_library_ID_is_indirectly_used(bmain, ob))
  {
    /* We cannot delete indirectly used object... */
    printf(
        "WARNING, undeletable object '%s', should have been caught before reaching this "
        "function!",
        ob->id.name + 2);
    return;
  }
  if (!BKE_lib_override_library_id_is_user_deletable(bmain, &ob->id)) {
    /* Do not delete objects used by overrides of collections. */
    return;
  }

  DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_BASE_FLAGS);

  BKE_scene_collections_object_remove(bmain, scene, ob, true);
}

void base_free_and_unlink_no_indirect_check(Main *bmain, Scene *scene, Object *ob)
{
  BLI_assert(!BKE_library_ID_is_indirectly_used(bmain, ob));
  DEG_id_tag_update_ex(bmain, &ob->id, ID_RECALC_BASE_FLAGS);
  BKE_scene_collections_object_remove(bmain, scene, ob, true);
}

static wmOperatorStatus object_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  wmWindowManager *wm = CTX_wm_manager(C);
  const bool use_global = RNA_boolean_get(op->ptr, "use_global");
  const bool confirm = op->flag & OP_IS_INVOKE;
  uint changed_count = 0;
  uint tagged_count = 0;

  if (CTX_data_edit_object(C)) {
    return OPERATOR_CANCELLED;
  }

  BKE_main_id_tag_all(bmain, ID_TAG_DOIT, false);

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (ob->id.tag & ID_TAG_INDIRECT) {
      /* Can this case ever happen? */
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Cannot delete indirectly linked object '%s'",
                  ob->id.name + 2);
      continue;
    }

    if (!BKE_lib_override_library_id_is_user_deletable(bmain, &ob->id)) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Cannot delete object '%s' as it is used by override collections",
                  ob->id.name + 2);
      continue;
    }

    if (ID_REAL_USERS(ob) <= 1 && ID_EXTRA_USERS(ob) == 0 &&
        BKE_library_ID_is_indirectly_used(bmain, ob))
    {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Cannot delete object '%s' from scene '%s', indirectly used objects need at "
                  "least one user",
                  ob->id.name + 2,
                  scene->id.name + 2);
      continue;
    }

    /* Use multi tagged delete if `use_global=True`, or the object is used only in one scene. */
    if (use_global || ID_REAL_USERS(ob) <= 1) {
      ob->id.tag |= ID_TAG_DOIT;
      tagged_count += 1;
    }
    else {
      /* Object is used in multiple scenes. Delete the object from the current scene only. */
      base_free_and_unlink_no_indirect_check(bmain, scene, ob);
      changed_count += 1;
    }
  }
  CTX_DATA_END;

  if ((changed_count + tagged_count) == 0) {
    return OPERATOR_CANCELLED;
  }

  if (tagged_count > 0) {
    BKE_id_multi_tagged_delete(bmain);
  }

  if (confirm) {
    BKE_reportf(op->reports, RPT_INFO, "Deleted %u object(s)", (changed_count + tagged_count));
  }

  /* delete has to handle all open scenes */
  BKE_main_id_tag_listbase(&bmain->scenes, ID_TAG_DOIT, true);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    scene = WM_window_get_active_scene(win);

    if (scene->id.tag & ID_TAG_DOIT) {
      scene->id.tag &= ~ID_TAG_DOIT;

      DEG_relations_tag_update(bmain);

      DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
      WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
      WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus object_delete_invoke(bContext *C,
                                             wmOperator *op,
                                             const wmEvent * /*event*/)
{
  if (RNA_boolean_get(op->ptr, "confirm")) {
    return WM_operator_confirm_ex(C,
                                  op,
                                  IFACE_("Delete selected objects?"),
                                  nullptr,
                                  IFACE_("Delete"),
                                  ALERT_ICON_NONE,
                                  false);
  }
  return object_delete_exec(C, op);
}

void OBJECT_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete selected objects";
  ot->idname = "OBJECT_OT_delete";

  /* API callbacks. */
  ot->invoke = object_delete_invoke;
  ot->exec = object_delete_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_boolean(
      ot->srna, "use_global", false, "Delete Globally", "Remove object from all scenes");
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
    BKE_libblock_relink_to_newid(bmain, &ob->id, ID_REMAP_SKIP_USER_CLEAR);
  }
  CTX_DATA_END;

#ifndef NDEBUG
  /* Call to `BKE_libblock_relink_to_newid` above is supposed to have cleared all those flags. */
  ID *id_iter;
  FOREACH_MAIN_ID_BEGIN (bmain, id_iter) {
    if (GS(id_iter->name) == ID_OB) {
      /* Not all duplicated objects would be used by other newly duplicated data, so their flag
       * will not always be cleared. */
      continue;
    }
    BLI_assert((id_iter->tag & ID_TAG_NEW) == 0);
  }
  FOREACH_MAIN_ID_END;
#endif

  BKE_main_id_newptr_and_tag_clear(bmain);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Make Instanced Objects Real Operator
 * \{ */

/* XXX TODO: That whole hierarchy handling based on persistent_id tricks is
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
  const DupliObject *dob = static_cast<const DupliObject *>(ptr);
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
  const DupliObject *dob = static_cast<const DupliObject *>(ptr);
  uint hash = BLI_ghashutil_inthash(dob->persistent_id[0]);
  for (int i = 1; (i < MAX_DUPLI_RECUR) && dob->persistent_id[i] != INT_MAX; i++) {
    hash ^= (dob->persistent_id[i] ^ i);
  }
  return hash;
}

/**
 * Compare function that matches #dupliobject_hash.
 */
static bool dupliobject_cmp(const void *a_, const void *b_)
{
  const DupliObject *a = static_cast<const DupliObject *>(a_);
  const DupliObject *b = static_cast<const DupliObject *>(b_);

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
      if (a->persistent_id[i] == INT_MAX) {
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
  const DupliObject *a = static_cast<const DupliObject *>(a_);
  const DupliObject *b = static_cast<const DupliObject *>(b_);

  for (int i = 0; (i < MAX_DUPLI_RECUR); i++) {
    if (a->persistent_id[i] != b->persistent_id[i]) {
      return true;
    }
    if (a->persistent_id[i] == INT_MAX) {
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
  GHash *parent_gh = nullptr, *instancer_gh = nullptr;

  Object *object_eval = DEG_get_evaluated(depsgraph, base->object);

  if (!(base->object->transflag & OB_DUPLI) &&
      !bke::object_has_geometry_set_instances(*object_eval))
  {
    return;
  }

  DupliList duplilist;
  object_duplilist(depsgraph, scene, object_eval, nullptr, duplilist);

  if (duplilist.is_empty()) {
    return;
  }

  blender::Map<const DupliObject *, Object *> dupli_map;
  if (use_hierarchy) {
    parent_gh = BLI_ghash_new(dupliobject_hash, dupliobject_cmp, __func__);

    if (use_base_parent) {
      instancer_gh = BLI_ghash_new(
          dupliobject_instancer_hash, dupliobject_instancer_cmp, __func__);
    }
  }

  for (DupliObject &dob : duplilist) {
    Object *ob_src = DEG_get_original(dob.ob);
    Object *ob_dst = static_cast<Object *>(ID_NEW_SET(ob_src, BKE_id_copy(bmain, &ob_src->id)));
    id_us_min(&ob_dst->id);

    /* font duplis can have a totcol without material, we get them from parent
     * should be implemented better...
     */
    if (ob_dst->mat == nullptr) {
      ob_dst->totcol = 0;
    }

    BKE_collection_object_add_from(bmain, scene, base->object, ob_dst);
    BKE_view_layer_synced_ensure(scene, view_layer);
    Base *base_dst = BKE_view_layer_base_find(view_layer, ob_dst);
    BLI_assert(base_dst != nullptr);

    base_select(base_dst, BA_SELECT);
    DEG_id_tag_update(&ob_dst->id, ID_RECALC_SELECT);

    BKE_scene_object_base_flag_sync_from_base(base_dst);

    /* make sure apply works */
    BKE_animdata_free(&ob_dst->id, true);
    ob_dst->adt = nullptr;

    ob_dst->parent = nullptr;
    BKE_constraints_free(&ob_dst->constraints);
    ob_dst->runtime->curve_cache = nullptr;
    const bool is_dupli_instancer = (ob_dst->transflag & OB_DUPLI) != 0;
    ob_dst->transflag &= ~OB_DUPLI;
    /* Remove instantiated collection, it's annoying to keep it here
     * (and get potentially a lot of usages of it then...). */
    id_us_min((ID *)ob_dst->instance_collection);
    ob_dst->instance_collection = nullptr;

    copy_m4_m4(ob_dst->runtime->object_to_world.ptr(), dob.mat);
    BKE_object_apply_mat4(ob_dst, ob_dst->object_to_world().ptr(), false, false);

    dupli_map.add(&dob, ob_dst);

    if (parent_gh) {
      void **val;
      /* Due to nature of hash/comparison of this ghash, a lot of duplis may be considered as
       * 'the same', this avoids trying to insert same key several time and
       * raise asserts in debug builds... */
      if (!BLI_ghash_ensure_p(parent_gh, &dob, &val)) {
        *val = ob_dst;
      }

      if (is_dupli_instancer && instancer_gh) {
        /* Same as above, we may have several 'hits'. */
        if (!BLI_ghash_ensure_p(instancer_gh, &dob, &val)) {
          *val = ob_dst;
        }
      }
    }
  }

  for (DupliObject &dob : duplilist) {
    Object *ob_src = dob.ob;
    Object *ob_dst = dupli_map.lookup(&dob);

    /* Remap new object to itself, and clear again newid pointer of orig object. */
    BKE_libblock_relink_to_newid(bmain, &ob_dst->id, 0);

    DEG_id_tag_update(&ob_dst->id, ID_RECALC_GEOMETRY);

    if (use_hierarchy) {
      /* original parents */
      Object *ob_src_par = ob_src->parent;
      Object *ob_dst_par = nullptr;

      /* find parent that was also made real */
      if (ob_src_par) {
        /* OK to keep most of the members uninitialized,
         * they won't be read, this is simply for a hash lookup. */
        DupliObject dob_key;
        dob_key.ob = ob_src_par;
        dob_key.type = dob.type;
        if (dob.type == OB_DUPLICOLLECTION) {
          memcpy(&dob_key.persistent_id[1],
                 &dob.persistent_id[1],
                 sizeof(dob.persistent_id[1]) * (MAX_DUPLI_RECUR - 1));
        }
        else {
          dob_key.persistent_id[0] = dob.persistent_id[0];
        }
        ob_dst_par = static_cast<Object *>(BLI_ghash_lookup(parent_gh, &dob_key));
      }

      if (ob_dst_par) {
        /* allow for all possible parent types */
        ob_dst->partype = ob_src->partype;
        STRNCPY_UTF8(ob_dst->parsubstr, ob_src->parsubstr);
        ob_dst->par1 = ob_src->par1;
        ob_dst->par2 = ob_src->par2;
        ob_dst->par3 = ob_src->par3;

        copy_m4_m4(ob_dst->parentinv, ob_src->parentinv);

        ob_dst->parent = ob_dst_par;
      }
    }
    if (use_base_parent && ob_dst->parent == nullptr) {
      Object *ob_dst_par = nullptr;

      if (instancer_gh != nullptr) {
        /* OK to keep most of the members uninitialized,
         * they won't be read, this is simply for a hash lookup. */
        DupliObject dob_key;
        /* We are looking one step upper in hierarchy, so we need to 'shift' the `persistent_id`,
         * ignoring the first item.
         * We only check on persistent_id here, since we have no idea what object it might be. */
        memcpy(&dob_key.persistent_id[0],
               &dob.persistent_id[1],
               sizeof(dob_key.persistent_id[0]) * (MAX_DUPLI_RECUR - 1));
        ob_dst_par = static_cast<Object *>(BLI_ghash_lookup(instancer_gh, &dob_key));
      }

      if (ob_dst_par == nullptr) {
        /* Default to parenting to root object...
         * Always the case when use_hierarchy is false. */
        ob_dst_par = base->object;
      }

      ob_dst->parent = ob_dst_par;
      ob_dst->partype = PAROBJECT;
    }

    if (ob_dst->parent) {
      /* NOTE: this may be the parent of other objects, but it should
       * still work out ok */
      BKE_object_apply_mat4(ob_dst, dob.mat, false, true);

      /* to set ob_dst->orig and in case there's any other discrepancies */
      DEG_id_tag_update(&ob_dst->id, ID_RECALC_TRANSFORM);
    }
  }

  if (base->object->transflag & OB_DUPLICOLLECTION && base->object->instance_collection) {
    base->object->instance_collection = nullptr;
  }

  base_select(base, BA_DESELECT);
  DEG_id_tag_update(&base->object->id, ID_RECALC_SELECT);

  if (parent_gh) {
    BLI_ghash_free(parent_gh, nullptr, nullptr);
  }
  if (instancer_gh) {
    BLI_ghash_free(instancer_gh, nullptr, nullptr);
  }

  BKE_main_id_newptr_and_tag_clear(bmain);

  base->object->transflag &= ~OB_DUPLI;
  DEG_id_tag_update(&base->object->id, ID_RECALC_SYNC_TO_EVAL);
}

static wmOperatorStatus object_duplicates_make_real_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);

  const bool use_base_parent = RNA_boolean_get(op->ptr, "use_base_parent");
  const bool use_hierarchy = RNA_boolean_get(op->ptr, "use_hierarchy");

  BKE_main_id_newptr_and_tag_clear(bmain);

  CTX_DATA_BEGIN (C, Base *, base, selected_editable_bases) {
    make_object_duplilist_real(C, depsgraph, scene, base, use_base_parent, use_hierarchy);

    /* dependencies were changed */
    WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, base->object);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE, scene);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);
  ED_outliner_select_sync_from_object_tag(C);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicates_make_real(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Instances Real";
  ot->description = "Make instanced objects attached to this object real";
  ot->idname = "OBJECT_OT_duplicates_make_real";

  /* API callbacks. */
  ot->exec = object_duplicates_make_real_exec;

  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_boolean(ot->srna,
                             "use_base_parent",
                             false,
                             "Parent",
                             "Parent newly created objects to the original instancer");
  RNA_def_property_translation_context(ot->prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  RNA_def_boolean(
      ot->srna, "use_hierarchy", false, "Keep Hierarchy", "Maintain parent child relationships");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Data Convert Operator
 * \{ */

static const EnumPropertyItem convert_target_items[] = {
    {OB_CURVES_LEGACY,
     "CURVE",
     ICON_OUTLINER_OB_CURVE,
     "Curve",
     "Curve from Mesh or Text objects"},
    {OB_MESH,
     "MESH",
     ICON_OUTLINER_OB_MESH,
     "Mesh",
     "Mesh from Curve, Surface, Metaball, Text, or Point Cloud objects"},
    {OB_POINTCLOUD,
     "POINTCLOUD",
     ICON_OUTLINER_OB_POINTCLOUD,
     "Point Cloud",
     "Point Cloud from Mesh objects"},
    {OB_CURVES, "CURVES", ICON_OUTLINER_OB_CURVES, "Curves", "Curves from evaluated curve data"},
    {OB_GREASE_PENCIL,
     "GREASEPENCIL",
     ICON_OUTLINER_OB_GREASEPENCIL,
     "Grease Pencil",
     "Grease Pencil from Curve or Mesh objects"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem *convert_target_itemf(bContext *C,
                                                    PointerRNA * /*ptr*/,
                                                    PropertyRNA * /*prop*/,
                                                    bool *r_free)
{
  if (!C) { /* needed for docs */
    return convert_target_items;
  }

  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  RNA_enum_items_add_value(&item, &totitem, convert_target_items, OB_MESH);
  RNA_enum_items_add_value(&item, &totitem, convert_target_items, OB_CURVES_LEGACY);
  RNA_enum_items_add_value(&item, &totitem, convert_target_items, OB_CURVES);
  RNA_enum_items_add_value(&item, &totitem, convert_target_items, OB_POINTCLOUD);
  RNA_enum_items_add_value(&item, &totitem, convert_target_items, OB_GREASE_PENCIL);

  RNA_enum_item_end(&item, &totitem);

  *r_free = true;

  return item;
}

static void object_data_convert_curve_to_mesh(Main *bmain, Depsgraph *depsgraph, Object *ob)
{
  Object *object_eval = DEG_get_evaluated(depsgraph, ob);
  Curve *curve = static_cast<Curve *>(ob->data);

  Mesh *mesh = BKE_mesh_new_from_object_to_bmain(bmain, depsgraph, object_eval, true);
  if (mesh == nullptr) {
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
   *   data-block. We don't want mesh to be created for every of those objects.
   * - This is how conversion worked for a long time. */
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
  if (!ID_IS_EDITABLE(scene)) {
    return false;
  }
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  /* Don't use `active_object` in the context, it's important this value
   * is from the view-layer as it's used to check if Blender is in object mode. */
  Object *obact = BKE_view_layer_active_object_get(view_layer);
  if (obact && obact->mode != OB_MODE_OBJECT) {
    return false;
  }

  /* Note that `obact` may not be editable,
   * only check the active object to ensure Blender is in object mode. */
  return true;
}

/* Helper for object_convert_exec */
static Base *duplibase_for_convert(
    Main *bmain, Depsgraph *depsgraph, Scene *scene, ViewLayer *view_layer, Base *base, Object *ob)
{
  if (ob == nullptr) {
    ob = base->object;
  }

  Object *obn = (Object *)BKE_id_copy(bmain, &ob->id);
  id_us_min(&obn->id);
  DEG_id_tag_update(&obn->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  BKE_collection_object_add_from(bmain, scene, ob, obn);

  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *basen = BKE_view_layer_base_find(view_layer, obn);
  base_select(basen, BA_SELECT);
  base_select(base, BA_DESELECT);

  /* XXX: An ugly hack needed because if we re-run depsgraph with some new meta-ball objects
   * having same 'family name' as orig ones, they will affect end result of meta-ball computation.
   * For until we get rid of that name-based thingy in meta-balls, that should do the trick
   * (this is weak, but other solution (to change name of `obn`) is even worse IMHO).
   * See #65996. */
  const bool is_meta_ball = (obn->type == OB_MBALL);
  void *obdata = obn->data;
  if (is_meta_ball) {
    obn->type = OB_EMPTY;
    obn->data = nullptr;
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

struct ObjectConversionInfo {
  Main *bmain;
  Depsgraph *depsgraph;
  Scene *scene;
  ViewLayer *view_layer;
  /**
   * Note that this is not used for conversion operation,
   * only to ensure the active-object doesn't change from a user perspective.
   */
  Object *obact;
  bool keep_original;
  bool do_merge_customdata;
  PointerRNA *op_props;
  ReportList *reports;
};

static Object *get_object_for_conversion(Base &base, ObjectConversionInfo &info, Base **r_new_base)
{
  if (info.keep_original) {
    *r_new_base = duplibase_for_convert(
        info.bmain, info.depsgraph, info.scene, info.view_layer, &base, nullptr);
    Object *newob = (*r_new_base)->object;

    /* Decrement original object data usage count. */
    ID *original_object_data = static_cast<ID *>(newob->data);
    id_us_min(original_object_data);

    /* Make a copy of the object data. */
    newob->data = BKE_id_copy(info.bmain, original_object_data);

    return newob;
  }
  *r_new_base = nullptr;
  return base.object;
}

static Object *convert_mesh_to_curves_legacy(Base &base,
                                             ObjectConversionInfo &info,
                                             Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  BKE_mesh_to_curve(info.bmain, info.depsgraph, info.scene, newob);

  if (newob->type == OB_CURVES_LEGACY) {
    BKE_object_free_modifiers(newob, 0); /* after derivedmesh calls! */
    if (newob->rigidbody_object != nullptr) {
      ED_rigidbody_object_remove(info.bmain, info.scene, newob);
    }
  }

  return newob;
}

static Object *convert_curves_component_to_curves(Base &base,
                                                  ObjectConversionInfo &info,
                                                  Base **r_new_base)
{
  Object *ob = base.object, *newob = nullptr;
  ob->flag |= OB_DONE;

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  bke::GeometrySet geometry;
  if (ob_eval->runtime->geometry_set_eval != nullptr) {
    geometry = *ob_eval->runtime->geometry_set_eval;
  }

  if (geometry.has_curves()) {
    newob = get_object_for_conversion(base, info, r_new_base);

    const Curves *curves_eval = geometry.get_curves();
    Curves *new_curves = BKE_id_new<Curves>(info.bmain, newob->id.name + 2);

    newob->data = new_curves;
    newob->type = OB_CURVES;

    new_curves->geometry.wrap() = curves_eval->geometry.wrap();
    BKE_object_material_from_eval_data(info.bmain, newob, &curves_eval->id);

    BKE_object_free_derived_caches(newob);
    BKE_object_free_modifiers(newob, 0);
  }
  else {
    BKE_reportf(info.reports,
                RPT_WARNING,
                "Object '%s' has no evaluated Curve or Grease Pencil data",
                ob->id.name + 2);
  }

  return newob;
}

static Object *convert_grease_pencil_component_to_curves(Base &base,
                                                         ObjectConversionInfo &info,
                                                         Base **r_new_base)
{
  Object *ob = base.object, *newob = nullptr;
  ob->flag |= OB_DONE;

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  bke::GeometrySet geometry;
  if (ob_eval->runtime->geometry_set_eval != nullptr) {
    geometry = *ob_eval->runtime->geometry_set_eval;
  }

  if (geometry.has_grease_pencil()) {
    newob = get_object_for_conversion(base, info, r_new_base);

    Curves *new_curves = BKE_id_new<Curves>(info.bmain, newob->id.name + 2);
    newob->data = new_curves;
    newob->type = OB_CURVES;

    if (const Curves *curves_eval = geometry.get_curves()) {
      new_curves->geometry.wrap() = curves_eval->geometry.wrap();
      BKE_object_material_from_eval_data(info.bmain, newob, &curves_eval->id);
    }
    else if (const GreasePencil *grease_pencil = geometry.get_grease_pencil()) {
      const Vector<ed::greasepencil::DrawingInfo> drawings =
          ed::greasepencil::retrieve_visible_drawings(*info.scene, *grease_pencil, false);
      if (drawings.size() > 0) {
        Array<bke::GeometrySet> geometries(drawings.size());
        for (const int i : drawings.index_range()) {
          Curves *curves_id = BKE_id_new_nomain<Curves>(nullptr);
          curves_id->geometry.wrap() = drawings[i].drawing.strokes();
          geometries[i] = bke::GeometrySet::from_curves(curves_id);
        }
        bke::GeometrySet joined_curves = geometry::join_geometries(geometries, {});

        new_curves->geometry.wrap() = joined_curves.get_curves()->geometry.wrap();
        new_curves->geometry.wrap().tag_topology_changed();
        BKE_object_material_from_eval_data(info.bmain, newob, &joined_curves.get_curves()->id);
      }
    }

    BKE_object_free_derived_caches(newob);
    BKE_object_free_modifiers(newob, 0);
  }
  else {
    BKE_reportf(info.reports,
                RPT_WARNING,
                "Object '%s' has no evaluated Curve or Grease Pencil data",
                ob->id.name + 2);
  }

  return newob;
}

static Object *convert_mesh_to_curves(Base &base, ObjectConversionInfo &info, Base **r_new_base)
{
  Object *newob = convert_curves_component_to_curves(base, info, r_new_base);
  if (newob) {
    return newob;
  }
  return convert_grease_pencil_component_to_curves(base, info, r_new_base);
}

static Object *convert_mesh_to_pointcloud(Base &base,
                                          ObjectConversionInfo &info,
                                          Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  BKE_mesh_to_pointcloud(info.bmain, info.depsgraph, info.scene, newob);

  if (newob->type == OB_POINTCLOUD) {
    BKE_object_free_modifiers(newob, 0); /* after derivedmesh calls! */
    ED_rigidbody_object_remove(info.bmain, info.scene, newob);
  }

  return newob;
}

static Object *convert_mesh_to_mesh(Base &base, ObjectConversionInfo &info, Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  /* make new mesh data from the original copy */
  /* NOTE: get the mesh from the original, not from the copy in some
   * cases this doesn't give correct results (when MDEF is used for eg)
   */
  const Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  Mesh *new_mesh = mesh_eval ? BKE_mesh_copy_for_eval(*mesh_eval) :
                               BKE_mesh_new_nomain(0, 0, 0, 0);
  BKE_object_material_from_eval_data(info.bmain, newob, &new_mesh->id);
  /* Anonymous attributes shouldn't be available on the applied geometry. */
  new_mesh->attributes_for_write().remove_anonymous();
  if (info.do_merge_customdata) {
    BKE_mesh_merge_customdata_for_apply_modifier(new_mesh);
  }

  Mesh *ob_data_mesh = (Mesh *)newob->data;

  if (ob_data_mesh->key) {
    /* NOTE(@ideasman42): Clearing the shape-key is needed when the
     * number of vertices remains unchanged. Otherwise using this operator
     * to "Apply Visual Geometry" will evaluate using the existing shape-key
     * which doesn't have the "evaluated" coordinates from `new_mesh`.
     * See #128839 for details.
     *
     * While shape-keys could be supported, this is more of a feature to consider.
     * As there is already a `MESH_OT_blend_from_shape` operator,
     * it's not clear this is especially useful or needed. */
    if (!CustomData_has_layer(&new_mesh->vert_data, CD_SHAPEKEY)) {
      id_us_min(&ob_data_mesh->key->id);
      ob_data_mesh->key = nullptr;
    }
  }
  BKE_mesh_nomain_to_mesh(new_mesh, ob_data_mesh, newob);

  BKE_object_free_modifiers(newob, 0); /* after derivedmesh calls! */

  if (!info.keep_original) {
    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
  }

  return newob;
}

static int mesh_to_grease_pencil_add_material(Main &bmain,
                                              Object &ob_grease_pencil,
                                              const StringRefNull name,
                                              const std::optional<float4> &stroke_color,
                                              const std::optional<float4> &fill_color)
{
  int index;
  Material *ma = BKE_grease_pencil_object_material_ensure_by_name(
      &bmain, &ob_grease_pencil, DATA_(name.c_str()), &index);

  if (stroke_color.has_value()) {
    copy_v4_v4(ma->gp_style->stroke_rgba, stroke_color.value());
    srgb_to_linearrgb_v4(ma->gp_style->stroke_rgba, ma->gp_style->stroke_rgba);
  }

  if (fill_color.has_value()) {
    copy_v4_v4(ma->gp_style->fill_rgba, fill_color.value());
    srgb_to_linearrgb_v4(ma->gp_style->fill_rgba, ma->gp_style->fill_rgba);
  }

  SET_FLAG_FROM_TEST(ma->gp_style->flag, stroke_color.has_value(), GP_MATERIAL_STROKE_SHOW);
  SET_FLAG_FROM_TEST(ma->gp_style->flag, fill_color.has_value(), GP_MATERIAL_FILL_SHOW);

  return index;
}

class FillColorRecord {
 public:
  float4 color;
  StringRefNull name;
  bool operator==(const FillColorRecord &other) const
  {
    return other.color == color && other.name == name;
  }
  uint64_t hash() const
  {
    return hash_string(name);
  }
};

static VectorSet<FillColorRecord> mesh_to_grease_pencil_get_material_list(
    Object &ob_mesh, const Mesh &mesh, Array<int> &material_remap)
{
  const short num_materials = mesh.totcol;
  const FillColorRecord empty_fill = {float4(1.0f), DATA_("Empty Fill")};

  /* This function will only be called when we want to create fills out of mesh faces, so always
   * ensure that fills would have at least one material to be assigned to. */
  if (num_materials == 0) {
    VectorSet<FillColorRecord> fill_colors;
    fill_colors.add(empty_fill);
    material_remap.reinitialize(1);
    material_remap[0] = 0;
    return fill_colors;
  }

  VectorSet<FillColorRecord> fill_colors;

  material_remap.reinitialize(num_materials);

  for (const int material_i : IndexRange(num_materials)) {
    const Material *mesh_material = BKE_object_material_get(&ob_mesh, material_i + 1);
    if (!mesh_material) {
      material_remap[material_i] = fill_colors.index_of_or_add(empty_fill);
      continue;
    }
    const float4 fill_color = float4(
        mesh_material->r, mesh_material->g, mesh_material->b, mesh_material->a);
    const StringRefNull material_name = BKE_id_name(mesh_material->id);
    const FillColorRecord record = {fill_color, material_name};
    material_remap[material_i] = fill_colors.index_of_or_add(record);
  }

  return fill_colors;
}
static void mesh_data_to_grease_pencil(const Mesh &mesh_eval,
                                       GreasePencil &grease_pencil,
                                       const int current_frame,
                                       const bool generate_faces,
                                       const float stroke_radius,
                                       const float offset,
                                       const Array<int> &material_remap)
{
  grease_pencil.flag |= GREASE_PENCIL_STROKE_ORDER_3D;

  if (mesh_eval.edges_num <= 0) {
    return;
  }

  bke::greasepencil::Layer &layer_line = grease_pencil.add_layer(DATA_("Lines"));
  bke::greasepencil::Drawing *drawing_line = grease_pencil.insert_frame(layer_line, current_frame);

  const Span<float3> mesh_positions = mesh_eval.vert_positions();
  const OffsetIndices<int> faces = mesh_eval.faces();
  Span<int> faces_span = faces.data();
  const Span<int> corner_verts = mesh_eval.corner_verts();

  if (generate_faces && !faces.is_empty()) {
    bke::greasepencil::Layer &layer_fill = grease_pencil.add_layer(DATA_("Fills"));
    bke::greasepencil::Drawing *drawing_fill = grease_pencil.insert_frame(layer_fill,
                                                                          current_frame);
    const int fills_num = faces.size();
    const int fills_points_num = corner_verts.size();

    drawing_fill->strokes_for_write().resize(fills_points_num, fills_num);
    bke::CurvesGeometry &curves_fill = drawing_fill->strokes_for_write();
    MutableSpan<float3> positions_fill = curves_fill.positions_for_write();
    MutableSpan<int> offsets_fill = curves_fill.offsets_for_write();
    MutableSpan<bool> cyclic_fill = curves_fill.cyclic_for_write();
    bke::SpanAttributeWriter<int> stroke_materials_fill =
        curves_fill.attributes_for_write().lookup_or_add_for_write_span<int>(
            "material_index", bke::AttrDomain::Curve);
    bke::AttributeAccessor mesh_attributes = mesh_eval.attributes();
    VArray<int> mesh_materials = *mesh_attributes.lookup_or_default(
        "material_index", bke::AttrDomain::Face, 0);

    curves_fill.fill_curve_types(CURVE_TYPE_POLY);
    array_utils::gather(mesh_positions, corner_verts, positions_fill);
    array_utils::copy(faces_span, offsets_fill);
    cyclic_fill.fill(true);

    MutableSpan<int> material_span = stroke_materials_fill.span;
    for (const int face_i : material_span.index_range()) {
      /* Increase material index by 1 to accommodate the stroke material. */
      material_span[face_i] = material_remap[mesh_materials[face_i]] + 1;
    }
    stroke_materials_fill.finish();
  }

  Mesh *mesh_copied = BKE_mesh_copy_for_eval(mesh_eval);
  const Span<float3> normals = mesh_copied->vert_normals();

  std::string unique_attribute_id = BKE_attribute_calc_unique_name(
      AttributeOwner::from_id(&mesh_copied->id), "vertex_normal_for_conversion");

  mesh_copied->attributes_for_write().add(
      unique_attribute_id,
      bke::AttrDomain::Point,
      bke::AttrType::Float3,
      bke::AttributeInitVArray(VArray<float3>::from_span(normals)));

  const int edges_num = mesh_copied->edges_num;
  bke::CurvesGeometry curves = geometry::mesh_edges_to_curves_convert(
      *mesh_copied, IndexRange(edges_num), {});

  MutableSpan<float3> curve_positions = curves.positions_for_write();
  const VArraySpan<float3> point_normals = *curves.attributes().lookup<float3>(
      unique_attribute_id);

  threading::parallel_for(curve_positions.index_range(), 8192, [&](const IndexRange range) {
    for (const int point_i : range) {
      curve_positions[point_i] += offset * point_normals[point_i];
    }
  });

  BKE_defgroup_copy_list(&grease_pencil.vertex_group_names, &mesh_copied->vertex_group_names);

  curves.radius_for_write().fill(stroke_radius);

  drawing_line->strokes_for_write() = std::move(curves);
  drawing_line->tag_topology_changed();

  BKE_id_free(nullptr, mesh_copied);
}

static Object *convert_mesh_to_grease_pencil(Base &base,
                                             ObjectConversionInfo &info,
                                             Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  const bool generate_faces = RNA_boolean_get(info.op_props, "faces");
  const int thickness = RNA_int_get(info.op_props, "thickness");
  const float offset = RNA_float_get(info.op_props, "offset");

  /* To be compatible with the thickness value of legacy Grease Pencil. */
  const float stroke_radius = float(thickness) / 2 *
                              bke::greasepencil::LEGACY_RADIUS_CONVERSION_FACTOR;

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  const Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);

  VectorSet<FillColorRecord> fill_colors;
  Array<int> material_remap;
  if (generate_faces) {
    fill_colors = mesh_to_grease_pencil_get_material_list(*ob_eval, *mesh_eval, material_remap);
  }

  BKE_object_free_derived_caches(newob);
  BKE_object_free_modifiers(newob, 0);

  GreasePencil *grease_pencil = BKE_grease_pencil_add(info.bmain, BKE_id_name(mesh_eval->id));
  newob->data = grease_pencil;
  newob->type = OB_GREASE_PENCIL;

  /* Reset object material array and count since currently the generic / grease pencil material
   * functions still depend on this value being coherent (The same value as
   * `GreasePencil::material_array_num`).
   */
  BKE_object_material_resize(info.bmain, newob, 0, true);

  mesh_to_grease_pencil_add_material(
      *info.bmain, *newob, DATA_("Stroke"), float4(0.0f, 0.0f, 0.0f, 1.0f), {});

  if (generate_faces) {
    for (const int fill_i : fill_colors.index_range()) {
      const FillColorRecord &record = fill_colors[fill_i];
      mesh_to_grease_pencil_add_material(*info.bmain, *newob, record.name, {}, record.color);
    }
  }

  mesh_data_to_grease_pencil(*mesh_eval,
                             *grease_pencil,
                             info.scene->r.cfra,
                             generate_faces,
                             stroke_radius,
                             offset,
                             material_remap);

  return newob;
}

static Object *convert_mesh(Base &base,
                            const ObjectType target,
                            ObjectConversionInfo &info,
                            Base **r_new_base)
{
  switch (target) {
    case OB_CURVES_LEGACY:
      return convert_mesh_to_curves_legacy(base, info, r_new_base);
    case OB_CURVES:
      return convert_mesh_to_curves(base, info, r_new_base);
    case OB_POINTCLOUD:
      return convert_mesh_to_pointcloud(base, info, r_new_base);
    case OB_MESH:
      return convert_mesh_to_mesh(base, info, r_new_base);
    case OB_GREASE_PENCIL:
      return convert_mesh_to_grease_pencil(base, info, r_new_base);
    default:
      /* Current logic does convert mesh to mesh for any other target types. This would change
       * after other types of conversion are designed and implemented. */
      return convert_mesh_to_mesh(base, info, r_new_base);
  }
}

static Object *convert_curves_to_mesh(Base &base, ObjectConversionInfo &info, Base **r_new_base)
{
  Object *ob = base.object, *newob = nullptr;
  ob->flag |= OB_DONE;

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  bke::GeometrySet geometry;
  if (ob_eval->runtime->geometry_set_eval != nullptr) {
    geometry = *ob_eval->runtime->geometry_set_eval;
  }

  const Mesh *mesh_eval = geometry.get_mesh();
  const Curves *curves_eval = geometry.get_curves();
  Mesh *new_mesh = nullptr;

  if (mesh_eval || curves_eval) {
    newob = get_object_for_conversion(base, info, r_new_base);
    new_mesh = BKE_id_new<Mesh>(info.bmain, newob->id.name + 2);
    newob->data = new_mesh;
    newob->type = OB_MESH;
  }
  else {
    BKE_reportf(info.reports,
                RPT_WARNING,
                "Object '%s' has no evaluated mesh or curves data",
                ob->id.name + 2);
    return nullptr;
  }

  if (mesh_eval) {
    BKE_mesh_nomain_to_mesh(BKE_mesh_copy_for_eval(*mesh_eval), new_mesh, newob);
    BKE_object_material_from_eval_data(info.bmain, newob, &mesh_eval->id);
    new_mesh->attributes_for_write().remove_anonymous();
  }
  else if (curves_eval) {
    Mesh *mesh = bke::curve_to_wire_mesh(curves_eval->geometry.wrap(),
                                         bke::ProcessAllAttributeExceptAnonymous{});
    if (!mesh) {
      mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
    }
    BKE_mesh_nomain_to_mesh(mesh, new_mesh, newob);
    BKE_object_material_from_eval_data(info.bmain, newob, &curves_eval->id);
  }

  BKE_object_free_derived_caches(newob);
  BKE_object_free_modifiers(newob, 0);

  return newob;
}

static Object *convert_curves_to_grease_pencil(Base &base,
                                               ObjectConversionInfo &info,
                                               Base **r_new_base)
{
  Object *ob = base.object, *newob = nullptr;
  ob->flag |= OB_DONE;

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  bke::GeometrySet geometry;
  if (ob_eval->runtime->geometry_set_eval != nullptr) {
    geometry = *ob_eval->runtime->geometry_set_eval;
  }

  const GreasePencil *grease_pencil_eval = geometry.get_grease_pencil();
  const Curves *curves_eval = geometry.get_curves();
  GreasePencil *new_grease_pencil = nullptr;

  if (grease_pencil_eval || curves_eval) {
    newob = get_object_for_conversion(base, info, r_new_base);
    new_grease_pencil = BKE_id_new<GreasePencil>(info.bmain, newob->id.name + 2);
    newob->data = new_grease_pencil;
    newob->type = OB_GREASE_PENCIL;
  }
  else {
    BKE_reportf(info.reports,
                RPT_WARNING,
                "Object '%s' has no evaluated Grease Pencil or Curves data",
                ob->id.name + 2);
    return nullptr;
  }

  if (grease_pencil_eval) {
    BKE_grease_pencil_nomain_to_grease_pencil(BKE_grease_pencil_copy_for_eval(grease_pencil_eval),
                                              new_grease_pencil);
    BKE_object_material_from_eval_data(info.bmain, newob, &grease_pencil_eval->id);
    new_grease_pencil->attributes_for_write().remove_anonymous();
  }
  else if (curves_eval) {
    GreasePencil *grease_pencil = BKE_grease_pencil_new_nomain();
    /* Insert a default layer and place the drawing on frame 1. */
    const std::string layer_name = "Layer";
    const int frame_number = 1;
    bke::greasepencil::Layer &layer = grease_pencil->add_layer(layer_name);
    bke::greasepencil::Drawing *drawing = grease_pencil->insert_frame(layer, frame_number);
    BLI_assert(drawing != nullptr);
    drawing->strokes_for_write() = curves_eval->geometry.wrap();
    /* Default radius (1.0 unit) is too thick for converted strokes. */
    drawing->radii_for_write().fill(0.01f);

    BKE_grease_pencil_nomain_to_grease_pencil(grease_pencil, new_grease_pencil);
    BKE_object_material_from_eval_data(info.bmain, newob, &curves_eval->id);
  }

  BKE_object_free_derived_caches(newob);
  BKE_object_free_modifiers(newob, 0);

  return newob;
}

static Object *convert_curves(Base &base,
                              const ObjectType target,
                              ObjectConversionInfo &info,
                              Base **r_new_base)
{
  switch (target) {
    case OB_MESH:
      return convert_curves_to_mesh(base, info, r_new_base);
    case OB_GREASE_PENCIL:
      return convert_curves_to_grease_pencil(base, info, r_new_base);
    default:
      return convert_curves_component_to_curves(base, info, r_new_base);
  }
}

static Object *convert_grease_pencil_to_mesh(Base &base,
                                             ObjectConversionInfo &info,
                                             Base **r_new_base)
{
  Object *ob = base.object, *newob = nullptr;
  ob->flag |= OB_DONE;

  /* Mostly same as converting to OB_CURVES, the mesh will be converted from Curves afterwards. */

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  bke::GeometrySet geometry;
  if (ob_eval->runtime->geometry_set_eval != nullptr) {
    geometry = *ob_eval->runtime->geometry_set_eval;
  }

  if (geometry.has_curves()) {
    newob = get_object_for_conversion(base, info, r_new_base);

    const Curves *curves_eval = geometry.get_curves();
    Curves *new_curves = BKE_id_new<Curves>(info.bmain, newob->id.name + 2);

    newob->data = new_curves;
    newob->type = OB_CURVES;

    new_curves->geometry.wrap() = curves_eval->geometry.wrap();
    BKE_object_material_from_eval_data(info.bmain, newob, &curves_eval->id);

    BKE_object_free_derived_caches(newob);
    BKE_object_free_modifiers(newob, 0);
  }
  else if (geometry.has_grease_pencil()) {
    newob = get_object_for_conversion(base, info, r_new_base);

    /* Do not link `new_curves` to `bmain` since it's temporary. */
    Curves *new_curves = BKE_id_new_nomain<Curves>(newob->id.name + 2);

    newob->data = new_curves;
    newob->type = OB_CURVES;

    if (const Curves *curves_eval = geometry.get_curves()) {
      new_curves->geometry.wrap() = curves_eval->geometry.wrap();
      BKE_object_material_from_eval_data(info.bmain, newob, &curves_eval->id);
    }
    else if (const GreasePencil *grease_pencil = geometry.get_grease_pencil()) {
      const Vector<ed::greasepencil::DrawingInfo> drawings =
          ed::greasepencil::retrieve_visible_drawings(*info.scene, *grease_pencil, false);
      Array<bke::GeometrySet> geometries(drawings.size());
      for (const int i : drawings.index_range()) {
        Curves *curves_id = BKE_id_new_nomain<Curves>(nullptr);
        curves_id->geometry.wrap() = drawings[i].drawing.strokes();
        const int layer_index = drawings[i].layer_index;
        const bke::greasepencil::Layer *layer = grease_pencil->layers()[layer_index];
        blender::float4x4 to_object = layer->to_object_space(*ob);
        bke::CurvesGeometry &new_curves = curves_id->geometry.wrap();
        math::transform_points(to_object, new_curves.positions_for_write());
        geometries[i] = bke::GeometrySet::from_curves(curves_id);
      }
      if (geometries.size() > 0) {
        bke::GeometrySet joined_curves = geometry::join_geometries(geometries, {});

        new_curves->geometry.wrap() = joined_curves.get_curves()->geometry.wrap();
        new_curves->geometry.wrap().tag_topology_changed();
        BKE_object_material_from_eval_data(info.bmain, newob, &joined_curves.get_curves()->id);
      }
    }

    Mesh *new_mesh = BKE_id_new<Mesh>(info.bmain, newob->id.name + 2);
    newob->data = new_mesh;
    newob->type = OB_MESH;

    Mesh *mesh = bke::curve_to_wire_mesh(new_curves->geometry.wrap(), {});
    if (!mesh) {
      mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
    }
    BKE_mesh_nomain_to_mesh(mesh, new_mesh, newob);
    BKE_object_material_from_eval_data(info.bmain, newob, &new_curves->id);

    /* Free `new_curves` because it is just an intermediate. */
    BKE_id_free(nullptr, new_curves);

    BKE_object_free_derived_caches(newob);
    BKE_object_free_modifiers(newob, 0);
  }
  else {
    BKE_reportf(
        info.reports, RPT_WARNING, "Object '%s' has no evaluated curves data", ob->id.name + 2);
  }

  return newob;
}

static Object *convert_grease_pencil(Base &base,
                                     const ObjectType target,
                                     ObjectConversionInfo &info,
                                     Base **r_new_base)
{
  switch (target) {
    case OB_CURVES:
      return convert_grease_pencil_component_to_curves(base, info, r_new_base);
    case OB_MESH:
      return convert_grease_pencil_to_mesh(base, info, r_new_base);
    default:
      return nullptr;
  }
  return nullptr;
}

static Object *convert_font_to_curve_legacy_generic(Object *ob,
                                                    Object *newob,
                                                    ObjectConversionInfo &info)
{
  Curve *cu = static_cast<Curve *>(newob->data);

  Object *ob_eval = DEG_get_evaluated(info.depsgraph, ob);
  BKE_vfont_to_curve_ex(ob_eval,
                        *static_cast<const Curve *>(ob_eval->data),
                        FO_EDIT,
                        &cu->nurb,
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr,
                        nullptr);

  newob->type = OB_CURVES_LEGACY;
  cu->ob_type = OB_CURVES_LEGACY;

#define CURVE_VFONT_CLEAR(vfont_member) \
  if (cu->vfont_member) { \
    id_us_min(&cu->vfont_member->id); \
    cu->vfont_member = nullptr; \
  } \
  ((void)0)

  CURVE_VFONT_CLEAR(vfont);
  CURVE_VFONT_CLEAR(vfontb);
  CURVE_VFONT_CLEAR(vfonti);
  CURVE_VFONT_CLEAR(vfontbi);

#undef CURVE_VFONT_CLEAR

  if (!info.keep_original) {
    /* other users */
    Object *ob1 = nullptr;
    if (ID_REAL_USERS(&cu->id) > 1) {
      for (ob1 = static_cast<Object *>(info.bmain->objects.first); ob1;
           ob1 = static_cast<Object *>(ob1->id.next))
      {
        if (ob1->data == ob->data && ob1 != ob) {
          ob1->type = OB_CURVES_LEGACY;
          DEG_id_tag_update(&ob1->id,
                            ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION);
        }
      }
    }
  }

  LISTBASE_FOREACH (Nurb *, nu, &cu->nurb) {
    nu->charidx = 0;
  }

  cu->flag &= ~CU_3D;
  BKE_curve_dimension_update(cu);

  return newob;
}

static Object *convert_font_to_curves_legacy(Base &base,
                                             ObjectConversionInfo &info,
                                             Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  return convert_font_to_curve_legacy_generic(ob, newob, info);
}

static Object *convert_font_to_curves(Base &base, ObjectConversionInfo &info, Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);
  Object *curve_ob = convert_font_to_curve_legacy_generic(ob, newob, info);
  BLI_assert(curve_ob->type == OB_CURVES_LEGACY);

  Curve *legacy_curve_id = static_cast<Curve *>(curve_ob->data);
  Curves *curves_nomain = bke::curve_legacy_to_curves(*legacy_curve_id);

  Curves *curves_id = BKE_curves_add(info.bmain, BKE_id_name(legacy_curve_id->id));
  curves_id->geometry.wrap() = curves_nomain->geometry.wrap();

  blender::bke::curves_copy_parameters(*curves_nomain, *curves_id);

  curve_ob->data = curves_id;
  curve_ob->type = OB_CURVES;

  BKE_id_free(nullptr, curves_nomain);

  return curve_ob;
}

/* Currently neither Grease Pencil nor legacy curves supports per-stroke/curve fill attribute, thus
 * the #fill argument applies on all strokes that are converted. */
static void add_grease_pencil_materials_for_conversion(Main &bmain,
                                                       ID &from_id,
                                                       Object &gp_object,
                                                       const bool use_fill)
{
  short *len_p = BKE_id_material_len_p(&from_id);
  if (!len_p || *len_p == 0) {
    return;
  }
  Material ***materials = BKE_id_material_array_p(&from_id);
  if (!materials || !(*materials)) {
    return;
  }
  for (short i = 0; i < *len_p; i++) {
    const Material *orig_material = (*materials)[i];
    const char *name = orig_material ? BKE_id_name(orig_material->id) : IFACE_("Empty Material");

    Material *gp_material = BKE_grease_pencil_object_material_new(
        &bmain, &gp_object, name, nullptr);

    /* If the original object has this material slot but didn't assign any material, then we don't
     * have anything to copy color information from. In those cases we still added an empty
     * material to keep the material index matching. */
    if (!orig_material) {
      continue;
    }

    copy_v4_v4(gp_material->gp_style->fill_rgba, &orig_material->r);

    SET_FLAG_FROM_TEST(gp_material->gp_style->flag, !use_fill, GP_MATERIAL_STROKE_SHOW);
    SET_FLAG_FROM_TEST(gp_material->gp_style->flag, use_fill, GP_MATERIAL_FILL_SHOW);
  }
}

static Object *convert_font_to_grease_pencil(Base &base,
                                             ObjectConversionInfo &info,
                                             Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);
  Object *curve_ob = convert_font_to_curve_legacy_generic(ob, newob, info);
  BLI_assert(curve_ob->type == OB_CURVES_LEGACY);

  Curve *legacy_curve_id = static_cast<Curve *>(curve_ob->data);
  Curves *curves_nomain = bke::curve_legacy_to_curves(*legacy_curve_id);

  GreasePencil *grease_pencil = BKE_grease_pencil_add(info.bmain,
                                                      BKE_id_name(legacy_curve_id->id));
  bke::greasepencil::Layer &layer = grease_pencil->add_layer(DATA_("Converted Layer"));

  const int current_frame = info.scene->r.cfra;

  bke::greasepencil::Drawing *drawing = grease_pencil->insert_frame(layer, current_frame);

  blender::bke::CurvesGeometry &curves = curves_nomain->geometry.wrap();

  drawing->strokes_for_write() = std::move(curves);
  /* Default radius (1.0 unit) is too thick for converted strokes. */
  drawing->radii_for_write().fill(0.01f);
  drawing->tag_positions_changed();

  curve_ob->data = grease_pencil;
  curve_ob->type = OB_GREASE_PENCIL;
  curve_ob->totcol = grease_pencil->material_array_num;

  const bool use_fill = (legacy_curve_id->flag & (CU_FRONT | CU_BACK)) != 0;
  add_grease_pencil_materials_for_conversion(*info.bmain, legacy_curve_id->id, *newob, use_fill);

  /* We don't need the intermediate font/curve data ID any more. */
  BKE_id_delete(info.bmain, legacy_curve_id);

  /* For some reason this must be called, otherwise evaluated id_cow will still be the original
   * curves id (and that seems to only happen if "Keep Original" is enabled, and only with this
   * specific conversion combination), not sure why. Ref: #138793 / #146252 */
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);

  BKE_id_free(nullptr, curves_nomain);

  return curve_ob;
}

static Object *convert_font_to_mesh(Base &base, ObjectConversionInfo &info, Base **r_new_base)
{
  Object *newob = convert_font_to_curves_legacy(base, info, r_new_base);

  /* No assumption should be made that the resulting objects is a mesh, as conversion can
   * fail. */
  object_data_convert_curve_to_mesh(info.bmain, info.depsgraph, newob);

  /* Meshes doesn't use the "curve cache". */
  BKE_object_free_derived_caches(newob);

  return newob;
}

static Object *convert_font(Base &base,
                            const short target,
                            ObjectConversionInfo &info,
                            Base **r_new_base)
{
  switch (target) {
    case OB_MESH:
      return convert_font_to_mesh(base, info, r_new_base);
    case OB_CURVES_LEGACY:
      return convert_font_to_curves_legacy(base, info, r_new_base);
    case OB_CURVES:
      return convert_font_to_curves(base, info, r_new_base);
    case OB_GREASE_PENCIL:
      return convert_font_to_grease_pencil(base, info, r_new_base);
    default:
      return nullptr;
  }
  return nullptr;
}

static Object *convert_curves_legacy_to_mesh(Base &base,
                                             ObjectConversionInfo &info,
                                             Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  /* No assumption should be made that the resulting objects is a mesh, as conversion can
   * fail. */
  object_data_convert_curve_to_mesh(info.bmain, info.depsgraph, newob);

  /* Meshes doesn't use the "curve cache". */
  BKE_object_free_derived_caches(newob);

  return newob;
}

static Object *convert_curves_legacy_to_curves(Base &base,
                                               ObjectConversionInfo &info,
                                               Base **r_new_base)
{
  Object *newob = convert_curves_component_to_curves(base, info, r_new_base);
  if (newob) {
    return newob;
  }
  return convert_grease_pencil_component_to_curves(base, info, r_new_base);
}

static Object *convert_curves_legacy_to_grease_pencil(Base &base,
                                                      ObjectConversionInfo &info,
                                                      Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);
  BLI_assert(newob->type == OB_CURVES_LEGACY);

  Curve *legacy_curve_id = static_cast<Curve *>(newob->data);
  Curves *curves_nomain = bke::curve_legacy_to_curves(*legacy_curve_id);

  GreasePencil *grease_pencil = BKE_grease_pencil_add(info.bmain,
                                                      BKE_id_name(legacy_curve_id->id));
  bke::greasepencil::Layer &layer = grease_pencil->add_layer(DATA_("Converted Layer"));

  const int current_frame = info.scene->r.cfra;

  bke::greasepencil::Drawing *drawing = grease_pencil->insert_frame(layer, current_frame);

  blender::bke::CurvesGeometry &curves = curves_nomain->geometry.wrap();

  drawing->strokes_for_write() = std::move(curves);
  /* Default radius (1.0 unit) is too thick for converted strokes. */
  drawing->radii_for_write().fill(0.01f);
  drawing->tag_positions_changed();

  newob->data = grease_pencil;
  newob->type = OB_GREASE_PENCIL;

  /* Some functions like #BKE_id_material_len_p still uses Object::totcol so this value must be in
   * sync. */
  newob->totcol = grease_pencil->material_array_num;

  const bool use_fill = (legacy_curve_id->flag & (CU_FRONT | CU_BACK)) != 0;
  add_grease_pencil_materials_for_conversion(*info.bmain, legacy_curve_id->id, *newob, use_fill);

  /* For some reason this must be called, otherwise evaluated id_cow will still be the original
   * curves id (and that seems to only happen if "Keep Original" is enabled, and only with this
   * specific conversion combination), not sure why. Ref: #138793 / #146252 */
  DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);

  BKE_id_free(nullptr, curves_nomain);

  return newob;
}

static Object *convert_curves_legacy(Base &base,
                                     const ObjectType target,
                                     ObjectConversionInfo &info,
                                     Base **r_new_base)
{
  switch (target) {
    case OB_MESH:
      return convert_curves_legacy_to_mesh(base, info, r_new_base);
    case OB_CURVES:
      return convert_curves_legacy_to_curves(base, info, r_new_base);
    case OB_GREASE_PENCIL:
      return convert_curves_legacy_to_grease_pencil(base, info, r_new_base);
    default:
      return nullptr;
  }
}

static Object *convert_mball_to_mesh(Base &base,
                                     ObjectConversionInfo &info,
                                     bool &r_mball_converted,
                                     Base **r_new_base,
                                     Base **r_act_base)
{
  Object *ob = base.object;
  Object *newob = nullptr;
  Object *baseob = nullptr;

  base.flag &= ~BASE_SELECTED;
  base.object->base_flag &= ~BASE_SELECTED;

  baseob = BKE_mball_basis_find(info.scene, ob);

  if (ob != baseob) {
    /* If mother-ball is converting it would be marked as done later. */
    ob->flag |= OB_DONE;
  }

  if (!(baseob->flag & OB_DONE)) {
    *r_new_base = duplibase_for_convert(
        info.bmain, info.depsgraph, info.scene, info.view_layer, &base, baseob);
    newob = (*r_new_base)->object;

    MetaBall *mb = static_cast<MetaBall *>(newob->data);
    id_us_min(&mb->id);

    /* Find the evaluated mesh of the basis metaball object. */
    Object *object_eval = DEG_get_evaluated(info.depsgraph, baseob);
    Mesh *mesh = BKE_mesh_new_from_object_to_bmain(info.bmain, info.depsgraph, object_eval, true);

    id_us_plus(&mesh->id);
    newob->data = mesh;
    newob->type = OB_MESH;

    if (info.obact && (info.obact->type == OB_MBALL)) {
      *r_act_base = *r_new_base;
    }

    baseob->flag |= OB_DONE;
    r_mball_converted = true;
  }

  return newob;
}

static Object *convert_mball(Base &base,
                             const ObjectType target,
                             ObjectConversionInfo &info,
                             bool &r_mball_converted,
                             Base **r_new_base,
                             Base **r_act_base)
{
  switch (target) {
    case OB_MESH:
      return convert_mball_to_mesh(base, info, r_mball_converted, r_new_base, r_act_base);
    default:
      return nullptr;
  }
}

static Object *convert_pointcloud_to_mesh(Base &base,
                                          ObjectConversionInfo &info,
                                          Base **r_new_base)
{
  Object *ob = base.object;
  ob->flag |= OB_DONE;
  Object *newob = get_object_for_conversion(base, info, r_new_base);

  BKE_pointcloud_to_mesh(info.bmain, info.depsgraph, info.scene, newob);

  if (newob->type == OB_MESH) {
    BKE_object_free_modifiers(newob, 0); /* after derivedmesh calls! */
    ED_rigidbody_object_remove(info.bmain, info.scene, newob);
  }

  return newob;
}

static Object *convert_pointcloud(Base &base,
                                  const ObjectType target,
                                  ObjectConversionInfo &info,
                                  Base **r_new_base)
{
  switch (target) {
    case OB_MESH:
      return convert_pointcloud_to_mesh(base, info, r_new_base);
    default:
      return nullptr;
  }
}

static wmOperatorStatus object_convert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const short target = RNA_enum_get(op->ptr, "target");
  bool keep_original = RNA_boolean_get(op->ptr, "keep_original");
  const bool do_merge_customdata = RNA_boolean_get(op->ptr, "merge_customdata");

  Vector<PointerRNA> selected_editable_bases;
  CTX_data_selected_editable_bases(C, &selected_editable_bases);

  /* Too expensive to detect on poll(). */
  if (selected_editable_bases.is_empty()) {
    BKE_report(op->reports, RPT_INFO, "No editable objects to convert");
    return OPERATOR_CANCELLED;
  }

  /* Disallow conversion if any selected editable object is in Edit Mode.
   * This could be supported in the future, but it's a rare corner case
   * typically triggered only by Python scripts, see #147387. */
  for (const PointerRNA &ptr : selected_editable_bases) {
    const Object *ob = ((const Base *)ptr.data)->object;
    if (ob->mode & OB_MODE_EDIT) {
      BKE_report(
          op->reports, RPT_ERROR, "Cannot convert selected objects while they are in edit mode");
      return OPERATOR_CANCELLED;
    }
  }

  /* don't forget multiple users! */

  {
    FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
      ob->flag &= ~OB_DONE;

      /* flag data that's not been edited (only needed for !keep_original) */
      if (ob->data) {
        ((ID *)ob->data)->tag |= ID_TAG_DOIT;
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

  ObjectConversionInfo info;
  info.bmain = bmain;
  info.depsgraph = depsgraph;
  info.scene = scene;
  info.view_layer = view_layer;
  info.obact = BKE_view_layer_active_object_get(view_layer);
  info.keep_original = keep_original;
  info.do_merge_customdata = do_merge_customdata;
  info.op_props = op->ptr;
  info.reports = op->reports;

  Base *act_base = nullptr;

  /* Ensure we get all meshes calculated with a sufficient data-mask,
   * needed since re-evaluating single modifiers causes bugs if they depend
   * on other objects data masks too, see: #50950. */
  {
    for (const PointerRNA &ptr : selected_editable_bases) {
      Base *base = static_cast<Base *>(ptr.data);
      Object *ob = base->object;

      /* The way object type conversion works currently (enforcing conversion of *all* objects
       * using converted object-data, even some un-selected/hidden/another scene ones,
       * sounds totally bad to me.
       * However, changing this is more design than bug-fix, not to mention convoluted code below,
       * so that will be for later.
       * But at the very least, do not do that with linked IDs! */
      if ((!BKE_id_is_editable(bmain, &ob->id) ||
           (ob->data && !BKE_id_is_editable(bmain, static_cast<ID *>(ob->data)))) &&
          !keep_original)
      {
        keep_original = true;
        BKE_report(op->reports,
                   RPT_INFO,
                   "Converting some non-editable object/object data, enforcing 'Keep Original' "
                   "option to True");
      }

      DEG_id_tag_update(&base->object->id, ID_RECALC_GEOMETRY);
    }

    CustomData_MeshMasks customdata_mask_prev = scene->customdata_mask;
    CustomData_MeshMasks_update(&scene->customdata_mask, &CD_MASK_MESH);
    BKE_scene_graph_update_tagged(depsgraph, bmain);
    scene->customdata_mask = customdata_mask_prev;
  }

  bool mball_converted = false;
  int incompatible_count = 0;

  for (const PointerRNA &ptr : selected_editable_bases) {
    Object *newob = nullptr;
    Base *base = static_cast<Base *>(ptr.data), *new_base = nullptr;
    Object *ob = base->object;

    if (ob->flag & OB_DONE || !IS_TAGGED(ob->data)) {
      if (ob->type != target) {
        base->flag &= ~SELECT;
        ob->flag &= ~SELECT;
      }

      /* obdata already modified */
      if (!IS_TAGGED(ob->data)) {
        /* When 2 objects with linked data are selected, converting both
         * would keep modifiers on all but the converted object #26003. */
        if (ob->type == OB_MESH) {
          BKE_object_free_modifiers(ob, 0); /* after derivedmesh calls! */
        }
      }
    }
    else {
      const ObjectType target_type = ObjectType(target);
      switch (ob->type) {
        case OB_MESH:
          newob = convert_mesh(*base, target_type, info, &new_base);
          break;
        case OB_CURVES:
          newob = convert_curves(*base, target_type, info, &new_base);
          break;
        case OB_CURVES_LEGACY:
          ATTR_FALLTHROUGH;
        case OB_SURF:
          newob = convert_curves_legacy(*base, target_type, info, &new_base);
          break;
        case OB_FONT:
          newob = convert_font(*base, target_type, info, &new_base);
          break;
        case OB_GREASE_PENCIL:
          newob = convert_grease_pencil(*base, target_type, info, &new_base);
          break;
        case OB_MBALL:
          newob = convert_mball(*base, target_type, info, mball_converted, &new_base, &act_base);
          break;
        case OB_POINTCLOUD:
          newob = convert_pointcloud(*base, target_type, info, &new_base);
          break;
        default:
          incompatible_count++;
          continue;
      }
    }

    /* Ensure new object has consistent material data with its new obdata. */
    if (newob) {
      BKE_object_materials_sync_length(bmain, newob, static_cast<ID *>(newob->data));
    }

    /* tag obdata if it was been changed */

    /* If the original object is active then make this object active */
    if (new_base) {
      if (info.obact && (info.obact == ob)) {
        /* Store new active base to update view layer. */
        act_base = new_base;
      }
      new_base = nullptr;
    }

    if (!keep_original && (ob->flag & OB_DONE)) {
      /* NOTE: Tag transform for update because object parenting to curve with path is handled
       * differently from all other cases. Converting curve to mesh and mesh to curve will likely
       * affect the way children are evaluated.
       * It is not enough to tag only geometry and rely on the curve parenting relations because
       * this relation is lost when curve is converted to mesh. */
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY | ID_RECALC_TRANSFORM);
      ((ID *)ob->data)->tag &= ~ID_TAG_DOIT; /* flag not to convert this datablock again */
    }
  }

  if (!keep_original) {
    if (mball_converted) {
      /* We need to remove non-basis MBalls first, otherwise we won't be able to detect them if
       * their basis happens to be removed first. */
      FOREACH_SCENE_OBJECT_BEGIN (scene, ob_mball) {
        if (ob_mball->type == OB_MBALL) {
          Object *ob_basis = nullptr;
          if (!BKE_mball_is_basis(ob_mball) &&
              ((ob_basis = BKE_mball_basis_find(scene, ob_mball)) && (ob_basis->flag & OB_DONE)))
          {
            base_free_and_unlink(bmain, scene, ob_mball);
          }
        }
      }
      FOREACH_SCENE_OBJECT_END;
      FOREACH_SCENE_OBJECT_BEGIN (scene, ob_mball) {
        if (ob_mball->type == OB_MBALL) {
          if (ob_mball->flag & OB_DONE) {
            if (BKE_mball_is_basis(ob_mball)) {
              base_free_and_unlink(bmain, scene, ob_mball);
            }
          }
        }
      }
      FOREACH_SCENE_OBJECT_END;
    }
  }

  // XXX: editmode_enter(C, 0);
  // XXX: exit_editmode(C, EM_FREEDATA|); /* free data, but no undo. */

  if (act_base) {
    /* active base was changed */
    base_activate(C, act_base);
    view_layer->basact = act_base;
  }
  else {
    BKE_view_layer_synced_ensure(scene, view_layer);
    if (Object *object = BKE_view_layer_active_object_get(view_layer)) {
      if (object->flag & OB_DONE) {
        WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, object);
        WM_event_add_notifier(C, NC_OBJECT | ND_DATA, object);
      }
    }
  }

  if (incompatible_count != 0) {
    const char *target_type_name = "";
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, "target");
    BLI_assert(prop != nullptr);
    RNA_property_enum_name(C, op->ptr, prop, target, &target_type_name);
    if (incompatible_count == selected_editable_bases.size()) {
      BKE_reportf(op->reports,
                  RPT_INFO,
                  "None of the objects are compatible with a conversion to \"%s\"",
                  RPT_(target_type_name));
    }
    else {
      BKE_reportf(
          op->reports,
          RPT_INFO,
          "The selection included %d object type(s) which do not support conversion to \"%s\"",
          incompatible_count,
          RPT_(target_type_name));
    }
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

static void object_convert_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  layout->use_property_split_set(true);

  layout->prop(op->ptr, "target", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "keep_original", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  const int target = RNA_enum_get(op->ptr, "target");
  if (target == OB_MESH) {
    layout->prop(op->ptr, "merge_customdata", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (target == OB_GREASE_PENCIL) {
    layout->prop(op->ptr, "thickness", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "faces", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

void OBJECT_OT_convert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Convert To";
  ot->description = "Convert selected objects to another type";
  ot->idname = "OBJECT_OT_convert";

  /* API callbacks. */
  ot->invoke = WM_menu_invoke;
  ot->exec = object_convert_exec;
  ot->poll = object_convert_poll;
  ot->ui = object_convert_ui;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = prop = RNA_def_enum(
      ot->srna, "target", convert_target_items, OB_MESH, "Target", "Type of object to convert to");
  RNA_def_enum_funcs(prop, convert_target_itemf);

  prop = RNA_def_boolean(ot->srna,
                         "keep_original",
                         false,
                         "Keep Original",
                         "Keep original objects instead of replacing them");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_OBJECT);

  RNA_def_boolean(
      ot->srna,
      "merge_customdata",
      true,
      "Merge UVs",
      "Merge UV coordinates that share a vertex to account for imprecision in some modifiers");

  RNA_def_int(ot->srna, "thickness", 5, 1, 100, "Thickness", "", 1, 100);
  RNA_def_boolean(ot->srna, "faces", true, "Export Faces", "Export faces as filled strokes");
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

static void object_add_sync_base_collection(
    Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base_src, Object *object_new)
{
  if ((base_src != nullptr) && (base_src->flag & BASE_ENABLED_AND_MAYBE_VISIBLE_IN_VIEWPORT)) {
    BKE_collection_object_add_from(bmain, scene, base_src->object, object_new);
  }
  else {
    LayerCollection *layer_collection = BKE_layer_collection_get_active(view_layer);
    BKE_collection_object_add(bmain, layer_collection->collection, object_new);
  }
}

static void object_add_sync_local_view(Base *base_src, Base *base_new)
{
  base_new->local_view_bits = base_src->local_view_bits;
}

static void object_add_sync_rigid_body(Main *bmain, Object *object_src, Object *object_new)
{
  /* 1) duplis should end up in same collection as the original
   * 2) Rigid Body sim participants MUST always be part of a collection...
   */
  /* XXX: is 2) really a good measure here? */
  if (object_src->rigidbody_object || object_src->rigidbody_constraint) {
    LISTBASE_FOREACH (Collection *, collection, &bmain->collections) {
      if (BKE_collection_has_object(collection, object_src)) {
        BKE_collection_object_add(bmain, collection, object_new);
      }
    }
  }
}

/**
 * - Assumes `id.new` is correct.
 * - Leaves selection of base/object unaltered.
 * - Sets #ID.newid pointers.
 */
static void object_add_duplicate_internal(Main *bmain,
                                          Object *ob,
                                          const eDupli_ID_Flags dupflag,
                                          const eLibIDDuplicateFlags duplicate_options,
                                          Object **r_ob_new)
{
  if (ob->mode & OB_MODE_POSE) {
    return;
  }

  Object *obn = BKE_object_duplicate(bmain, ob, dupflag, duplicate_options);
  if (r_ob_new) {
    *r_ob_new = obn;
  }
  DEG_id_tag_update(&obn->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
}

static Base *object_add_duplicate_internal(Main *bmain,
                                           Scene *scene,
                                           ViewLayer *view_layer,
                                           Object *ob,
                                           const eDupli_ID_Flags dupflag,
                                           const eLibIDDuplicateFlags duplicate_options,
                                           Object **r_ob_new)
{
  Object *object_new = nullptr;
  object_add_duplicate_internal(bmain, ob, dupflag, duplicate_options, &object_new);
  if (r_ob_new) {
    *r_ob_new = object_new;
  }
  if (object_new == nullptr) {
    return nullptr;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base_src = BKE_view_layer_base_find(view_layer, ob);
  object_add_sync_base_collection(bmain, scene, view_layer, base_src, object_new);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base_new = BKE_view_layer_base_find(view_layer, object_new);
  if (base_src && base_new) {
    object_add_sync_local_view(base_src, base_new);
  }
  object_add_sync_rigid_body(bmain, ob, object_new);
  return base_new;
}

Base *add_duplicate(
    Main *bmain, Scene *scene, ViewLayer *view_layer, Base *base, const eDupli_ID_Flags dupflag)
{
  Base *basen;
  Object *ob;

  basen = object_add_duplicate_internal(bmain,
                                        scene,
                                        view_layer,
                                        base->object,
                                        dupflag,
                                        LIB_ID_DUPLICATE_IS_SUBPROCESS |
                                            LIB_ID_DUPLICATE_IS_ROOT_ID,
                                        nullptr);
  if (basen == nullptr) {
    return nullptr;
  }

  ob = basen->object;

  /* Link own references to the newly duplicated data #26816.
   * Note that this function can be called from edit-mode code, in which case we may have to
   * enforce remapping obdata (by default this is forbidden in edit mode). */
  const int remap_flag = BKE_object_is_in_editmode(ob) ? ID_REMAP_FORCE_OBDATA_IN_EDITMODE : 0;
  BKE_libblock_relink_to_newid(bmain, &ob->id, remap_flag);

  /* Correct but the caller must do this. */
  // DAG_relations_tag_update(bmain);

  if (ob->data != nullptr) {
    DEG_id_tag_update_ex(bmain, (ID *)ob->data, ID_RECALC_EDITORS);
  }

  BKE_main_id_newptr_and_tag_clear(bmain);

  return basen;
}

/* contextual operator dupli */
static wmOperatorStatus duplicate_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool linked = RNA_boolean_get(op->ptr, "linked");
  const eDupli_ID_Flags dupflag = (linked) ? (eDupli_ID_Flags)0 : (eDupli_ID_Flags)U.dupflag;

  /* We need to handle that here ourselves, because we may duplicate several objects, in which case
   * we also want to remap pointers between those... */
  BKE_main_id_newptr_and_tag_clear(bmain);

  /* Duplicate the selected objects, remember data needed to process
   * after the sync. */
  struct DuplicateObjectLink {
    Base *base_src = nullptr;
    Object *object_new = nullptr;

    DuplicateObjectLink(Base *base_src) : base_src(base_src) {}
  };

  Vector<DuplicateObjectLink> object_base_links;
  CTX_DATA_BEGIN (C, Base *, base, selected_bases) {
    object_base_links.append(DuplicateObjectLink(base));
  }
  CTX_DATA_END;

  bool new_objects_created = false;
  for (DuplicateObjectLink &link : object_base_links) {
    object_add_duplicate_internal(bmain,
                                  link.base_src->object,
                                  dupflag,
                                  LIB_ID_DUPLICATE_IS_SUBPROCESS | LIB_ID_DUPLICATE_IS_ROOT_ID,
                                  &link.object_new);
    if (link.object_new) {
      new_objects_created = true;
    }
  }

  if (!new_objects_created) {
    return OPERATOR_CANCELLED;
  }

  /* Sync that could tag the view_layer out of sync. */
  for (DuplicateObjectLink &link : object_base_links) {
    /* note that this is safe to do with this context iterator,
     * the list is made in advance */
    base_select(link.base_src, BA_DESELECT);
    if (link.object_new) {
      object_add_sync_base_collection(bmain, scene, view_layer, link.base_src, link.object_new);
      object_add_sync_rigid_body(bmain, link.base_src->object, link.object_new);
    }
  }

  /* Sync the view layer. Everything else should not tag the view_layer out of sync. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *active_base = BKE_view_layer_active_base_get(view_layer);
  for (DuplicateObjectLink &link : object_base_links) {
    if (!link.object_new) {
      continue;
    }

    Base *base_new = BKE_view_layer_base_find(view_layer, link.object_new);
    BLI_assert(base_new);
    base_select(base_new, BA_SELECT);
    if (active_base == link.base_src) {
      base_activate(C, base_new);
    }

    if (link.object_new->data) {
      DEG_id_tag_update(static_cast<ID *>(link.object_new->data), 0);
    }

    object_add_sync_local_view(link.base_src, base_new);
  }

  /* Note that this will also clear newid pointers and tags. */
  copy_object_set_idnew(C);

  ED_outliner_select_sync_from_object_tag(C);

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL | ID_RECALC_SELECT);

  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_duplicate(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Duplicate Objects";
  ot->description = "Duplicate selected objects";
  ot->idname = "OBJECT_OT_duplicate";

  /* API callbacks. */
  ot->exec = duplicate_exec;
  ot->poll = ED_operator_objectmode;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* to give to transform */
  prop = RNA_def_boolean(ot->srna,
                         "linked",
                         false,
                         "Linked",
                         "Duplicate object but not object data, linking to the original data");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_enum(ot->srna,
                      "mode",
                      rna_enum_transform_mode_type_items,
                      blender::ed::transform::TFM_TRANSLATION,
                      "Mode",
                      "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Add Named Object Operator
 *
 * Use for drag & drop.
 * \{ */

static wmOperatorStatus object_add_named_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool linked = RNA_boolean_get(op->ptr, "linked");
  const eDupli_ID_Flags dupflag = (linked) ? (eDupli_ID_Flags)0 : (eDupli_ID_Flags)U.dupflag;

  /* Find object, create fake base. */

  Object *ob = reinterpret_cast<Object *>(
      WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_OB));

  if (ob == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Object not found");
    return OPERATOR_CANCELLED;
  }

  /* prepare dupli */
  Base *basen = object_add_duplicate_internal(
      bmain,
      scene,
      view_layer,
      ob,
      dupflag,
      /* Sub-process flag because the new-ID remapping (#BKE_libblock_relink_to_newid()) in this
       * function will only work if the object is already linked in the view layer, which is not
       * the case here. So we have to do the new-ID relinking ourselves
       * (#copy_object_set_idnew()).
       */
      LIB_ID_DUPLICATE_IS_SUBPROCESS | LIB_ID_DUPLICATE_IS_ROOT_ID,
      nullptr);

  if (basen == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Object could not be duplicated");
    return OPERATOR_CANCELLED;
  }

  basen->object->visibility_flag &= ~OB_HIDE_VIEWPORT;
  /* Do immediately, as #copy_object_set_idnew() below operates on visible objects. */
  BKE_base_eval_flags(basen);

  /* #object_add_duplicate_internal() doesn't deselect other objects,
   * unlike #object_add_common() or #BKE_view_layer_base_deselect_all(). */
  base_deselect_all(scene, view_layer, nullptr, SEL_DESELECT);
  base_select(basen, BA_SELECT);
  base_activate(C, basen);

  copy_object_set_idnew(C);

  /* TODO(sergey): Only update relations for the current scene. */
  DEG_relations_tag_update(bmain);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);
  ED_outliner_select_sync_from_object_tag(C);

  PropertyRNA *prop_matrix = RNA_struct_find_property(op->ptr, "matrix");
  if (RNA_property_is_set(op->ptr, prop_matrix)) {
    Object *ob_add = basen->object;
    RNA_property_float_get_array(
        op->ptr, prop_matrix, ob_add->runtime->object_to_world.base_ptr());
    BKE_object_apply_mat4(ob_add, ob_add->object_to_world().ptr(), true, true);

    DEG_id_tag_update(&ob_add->id, ID_RECALC_TRANSFORM);
  }
  else if (CTX_wm_region_view3d(C)) {
    int mval[2];
    if (object_add_drop_xy_get(C, op, &mval)) {
      location_from_view(C, basen->object->loc);
      ED_view3d_cursor3d_position(C, mval, false, basen->object->loc);
    }
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_add_named(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Object";
  ot->description = "Add named object";
  ot->idname = "OBJECT_OT_add_named";

  /* API callbacks. */
  ot->invoke = object_add_drop_xy_generic_invoke;
  ot->exec = object_add_named_exec;
  ot->poll = ED_operator_objectmode_poll_msg;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  RNA_def_boolean(ot->srna,
                  "linked",
                  false,
                  "Linked",
                  "Duplicate object but not object data, linking to the original data");

  WM_operator_properties_id_lookup(ot, true);

  prop = RNA_def_float_matrix(
      ot->srna, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "Matrix", "", 0.0f, 0.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  object_add_drop_xy_props(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Object to Mouse Operator
 * \{ */

/**
 * Alternate behavior for dropping an asset that positions the appended object(s).
 */
static wmOperatorStatus object_transform_to_mouse_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  Object *ob = reinterpret_cast<Object *>(
      WM_operator_properties_id_lookup_from_name_or_session_uid(bmain, op->ptr, ID_OB));

  if (!ob) {
    BKE_view_layer_synced_ensure(scene, view_layer);
    ob = BKE_view_layer_active_object_get(view_layer);
  }

  if (ob == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Object not found");
    return OPERATOR_CANCELLED;
  }

  /* Don't transform a linked object. There's just nothing to do here in this case, so return
   * #OPERATOR_FINISHED. */
  if (!BKE_id_is_editable(bmain, &ob->id)) {
    return OPERATOR_FINISHED;
  }

  /* Ensure the locations are updated so snap reads the evaluated active location. */
  CTX_data_ensure_evaluated_depsgraph(C);

  PropertyRNA *prop_matrix = RNA_struct_find_property(op->ptr, "matrix");
  if (RNA_property_is_set(op->ptr, prop_matrix)) {
    ObjectsInViewLayerParams params = {0};
    Vector<Object *> objects = BKE_view_layer_array_selected_objects_params(
        view_layer, nullptr, &params);

    float matrix[4][4];
    RNA_property_float_get_array(op->ptr, prop_matrix, &matrix[0][0]);

    float mat_src_unit[4][4];
    float mat_dst_unit[4][4];
    float final_delta[4][4];

    normalize_m4_m4(mat_src_unit, ob->object_to_world().ptr());
    normalize_m4_m4(mat_dst_unit, matrix);
    invert_m4(mat_src_unit);
    mul_m4_m4m4(final_delta, mat_dst_unit, mat_src_unit);

    object_xform_array_m4(objects.data(), objects.size(), final_delta);
  }
  else if (CTX_wm_region_view3d(C)) {
    int mval[2];
    if (object_add_drop_xy_get(C, op, &mval)) {
      float cursor[3];
      location_from_view(C, cursor);
      ED_view3d_cursor3d_position(C, mval, false, cursor);

      /* Use the active objects location since this is the ID which the user selected to drop.
       *
       * This transforms all selected objects, so that dropping a single object which links in
       * other objects will have their relative transformation preserved.
       * For example a child/parent relationship or other objects used with a boolean modifier.
       *
       * The caller is responsible for ensuring the selection state gives useful results.
       * Link/append does this using #FILE_AUTOSELECT. */
      ED_view3d_snap_selected_to_location(C, op, cursor, V3D_AROUND_ACTIVE);
    }
  }

  return OPERATOR_FINISHED;
}

void OBJECT_OT_transform_to_mouse(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Place Object Under Mouse";
  ot->description = "Snap selected item(s) to the mouse location";
  ot->idname = "OBJECT_OT_transform_to_mouse";

  /* API callbacks. */
  ot->invoke = object_add_drop_xy_generic_invoke;
  ot->exec = object_transform_to_mouse_exec;
  ot->poll = ED_operator_objectmode_poll_msg;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_string(
      ot->srna,
      "name",
      nullptr,
      MAX_ID_NAME - 2,
      "Name",
      "Object name to place (uses the active object when this and 'session_uid' are unset)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);
  prop = RNA_def_int(ot->srna,
                     "session_uid",
                     0,
                     INT32_MIN,
                     INT32_MAX,
                     "Session UUID",
                     "Session UUID of the object to place (uses the active object when this and "
                     "'name' are unset)",
                     INT32_MIN,
                     INT32_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_HIDDEN);

  prop = RNA_def_float_matrix(
      ot->srna, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "Matrix", "", 0.0f, 0.0f);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  object_add_drop_xy_props(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join Object Operator
 * \{ */

static bool object_join_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (ob == nullptr || ob->data == nullptr || !ID_IS_EDITABLE(ob) || ID_IS_OVERRIDE_LIBRARY(ob) ||
      ID_IS_OVERRIDE_LIBRARY(ob->data))
  {
    return false;
  }

  if (ELEM(ob->type,
           OB_MESH,
           OB_CURVES_LEGACY,
           OB_SURF,
           OB_ARMATURE,
           OB_CURVES,
           OB_GREASE_PENCIL,
           OB_POINTCLOUD))
  {
    return true;
  }
  return false;
}

static wmOperatorStatus object_join_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);

  if (ob->mode & OB_MODE_EDIT) {
    BKE_report(op->reports, RPT_ERROR, "This data does not support joining in edit mode");
    return OPERATOR_CANCELLED;
  }
  if (BKE_object_obdata_is_libdata(ob)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot edit external library data");
    return OPERATOR_CANCELLED;
  }
  if (!BKE_lib_override_library_id_is_user_deletable(bmain, &ob->id)) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Cannot edit object '%s' as it is used by override collections",
                ob->id.name + 2);
    return OPERATOR_CANCELLED;
  }

  wmOperatorStatus ret = OPERATOR_CANCELLED;
  if (ob->type == OB_MESH) {
    ret = mesh::join_objects_exec(C, op);
  }
  else if (ELEM(ob->type, OB_CURVES_LEGACY, OB_SURF)) {
    ret = ED_curve_join_objects_exec(C, op);
  }
  else if (ob->type == OB_ARMATURE) {
    ret = ED_armature_join_objects_exec(C, op);
  }
  else if (ob->type == OB_POINTCLOUD) {
    ret = pointcloud::join_objects_exec(C, op);
  }
  else if (ob->type == OB_CURVES) {
    ret = curves::join_objects_exec(C, op);
  }
  else if (ob->type == OB_GREASE_PENCIL) {
    ret = ED_grease_pencil_join_objects_exec(C, op);
  }

  if (ret & OPERATOR_FINISHED) {
    /* Even though internally failure to invert is accounted for with a fallback,
     * show a warning since the result may not be what the user expects. See #80077.
     *
     * Failure to invert the matrix is typically caused by zero scaled axes
     * (which can be caused by constraints, even if the input scale isn't zero).
     *
     * Internally the join functions use #invert_m4_m4_safe_ortho which creates
     * an inevitable matrix from one that has one or more degenerate axes.
     *
     * In most cases we don't worry about special handling for non-inevitable matrices however for
     * joining objects there may be flat 2D objects where it's not obvious the scale is zero.
     * In this case, using #invert_m4_m4_safe_ortho works as well as we can expect,
     * joining the contents, flattening on the axis that's zero scaled.
     * If the zero scale is removed, the data on this axis remains un-scaled
     * (something that wouldn't work for #invert_m4_m4_safe). */
    float imat_test[4][4];
    if (!invert_m4_m4(imat_test, ob->object_to_world().ptr())) {
      BKE_report(op->reports,
                 RPT_WARNING,
                 "Active object final transform has one or more zero scaled axes");
    }
  }

  return ret;
}

void OBJECT_OT_join(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join";
  ot->description = "Join selected objects into active object";
  ot->idname = "OBJECT_OT_join";

  /* API callbacks. */
  ot->exec = object_join_exec;
  ot->poll = object_join_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Join Key Data Operators
 * \{ */

static bool active_shape_key_editable_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (!ob) {
    return false;
  }
  if (ob->type != OB_MESH) {
    return false;
  }

  if (ob->mode & OB_MODE_EDIT) {
    CTX_wm_operator_poll_msg_set(C, "This operation is not supported in edit mode");
    return false;
  }
  if (BKE_object_obdata_is_libdata(ob)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit external library data");
    return false;
  }
  Main &bmain = *CTX_data_main(C);
  if (!BKE_lib_override_library_id_is_user_deletable(&bmain, &ob->id)) {
    CTX_wm_operator_poll_msg_set(C, "Cannot edit object used by override collections");
    return false;
  }
  return true;
}

static wmOperatorStatus join_shapes_exec(bContext *C, wmOperator *op)
{
  return ED_mesh_shapes_join_objects_exec(
      C, true, RNA_boolean_get(op->ptr, "use_mirror"), op->reports);
}

void OBJECT_OT_join_shapes(wmOperatorType *ot)
{
  ot->name = "Join as Shapes";
  ot->description =
      "Add the vertex positions of selected objects as shape keys or update existing shape keys "
      "with matching names";
  ot->idname = "OBJECT_OT_join_shapes";

  ot->exec = join_shapes_exec;
  ot->poll = active_shape_key_editable_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "use_mirror", false, "Mirror", "Mirror the new shape key values");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus update_all_shape_keys_exec(bContext *C, wmOperator *op)
{
  return ED_mesh_shapes_join_objects_exec(
      C, false, RNA_boolean_get(op->ptr, "use_mirror"), op->reports);
}

static bool object_update_shapes_poll(bContext *C)
{
  if (!active_shape_key_editable_poll(C)) {
    return false;
  }

  Object *ob = CTX_data_active_object(C);
  const Key *key = BKE_key_from_object(ob);
  if (!key || BLI_listbase_is_empty(&key->block)) {
    return false;
  }
  return true;
}

void OBJECT_OT_update_shapes(wmOperatorType *ot)
{
  ot->name = "Update from Objects";
  ot->description =
      "Update existing shape keys with the vertex positions of selected objects with matching "
      "names";
  ot->idname = "OBJECT_OT_update_shapes";

  ot->exec = update_all_shape_keys_exec;
  ot->poll = object_update_shapes_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "use_mirror", false, "Mirror", "Mirror the new shape key values");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** \} */

}  // namespace blender::ed::object

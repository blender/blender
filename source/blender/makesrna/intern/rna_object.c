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
 */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>

#include "DNA_action_types.h"
#include "DNA_brush_types.h"
#include "DNA_collection_types.h"
#include "DNA_customdata_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_force_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_shader_fx_types.h"
#include "DNA_workspace_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_camera.h"
#include "BKE_collection.h"
#include "BKE_editlattice.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_object_deform.h"
#include "BKE_object_facemap.h"
#include "BKE_paint.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "BLI_sys_types.h" /* needed for intptr_t used in ED_mesh.h */
#include "ED_mesh.h"

#include "WM_api.h"
#include "WM_types.h"

const EnumPropertyItem rna_enum_object_mode_items[] = {
    {OB_MODE_OBJECT, "OBJECT", ICON_OBJECT_DATAMODE, "Object Mode", ""},
    {OB_MODE_EDIT, "EDIT", ICON_EDITMODE_HLT, "Edit Mode", ""},
    {OB_MODE_POSE, "POSE", ICON_POSE_HLT, "Pose Mode", ""},
    {OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt Mode", ""},
    {OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
    {OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
    {OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
    {OB_MODE_PARTICLE_EDIT, "PARTICLE_EDIT", ICON_PARTICLEMODE, "Particle Edit", ""},
    {OB_MODE_EDIT_GPENCIL,
     "EDIT_GPENCIL",
     ICON_EDITMODE_HLT,
     "Edit Mode",
     "Edit Grease Pencil Strokes"},
    {OB_MODE_SCULPT_GPENCIL,
     "SCULPT_GPENCIL",
     ICON_SCULPTMODE_HLT,
     "Sculpt Mode",
     "Sculpt Grease Pencil Strokes"},
    {OB_MODE_PAINT_GPENCIL,
     "PAINT_GPENCIL",
     ICON_GREASEPENCIL,
     "Draw",
     "Paint Grease Pencil Strokes"},
    {OB_MODE_VERTEX_GPENCIL,
     "VERTEX_GPENCIL",
     ICON_VPAINT_HLT,
     "Vertex Paint",
     "Grease Pencil Vertex Paint Strokes"},
    {OB_MODE_WEIGHT_GPENCIL,
     "WEIGHT_GPENCIL",
     ICON_WPAINT_HLT,
     "Weight Paint",
     "Grease Pencil Weight Paint Strokes"},
    {0, NULL, 0, NULL, NULL},
};

/* Same as above, but with names that distinguish grease pencil. */
const EnumPropertyItem rna_enum_workspace_object_mode_items[] = {
    {OB_MODE_OBJECT, "OBJECT", ICON_OBJECT_DATAMODE, "Object Mode", ""},
    {OB_MODE_EDIT, "EDIT", ICON_EDITMODE_HLT, "Edit Mode", ""},
    {OB_MODE_POSE, "POSE", ICON_POSE_HLT, "Pose Mode", ""},
    {OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt Mode", ""},
    {OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
    {OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
    {OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
    {OB_MODE_PARTICLE_EDIT, "PARTICLE_EDIT", ICON_PARTICLEMODE, "Particle Edit", ""},
    {OB_MODE_EDIT_GPENCIL,
     "EDIT_GPENCIL",
     ICON_EDITMODE_HLT,
     "Grease Pencil Edit Mode",
     "Edit Grease Pencil Strokes"},
    {OB_MODE_SCULPT_GPENCIL,
     "SCULPT_GPENCIL",
     ICON_SCULPTMODE_HLT,
     "Grease Pencil Sculpt Mode",
     "Sculpt Grease Pencil Strokes"},
    {OB_MODE_PAINT_GPENCIL,
     "PAINT_GPENCIL",
     ICON_GREASEPENCIL,
     "Grease Pencil Draw",
     "Paint Grease Pencil Strokes"},
    {OB_MODE_VERTEX_GPENCIL,
     "VERTEX_GPENCIL",
     ICON_VPAINT_HLT,
     "Grease Pencil Vertex Paint",
     "Grease Pencil Vertex Paint Strokes"},
    {OB_MODE_WEIGHT_GPENCIL,
     "WEIGHT_GPENCIL",
     ICON_WPAINT_HLT,
     "Grease Pencil Weight Paint",
     "Grease Pencil Weight Paint Strokes"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_object_empty_drawtype_items[] = {
    {OB_PLAINAXES, "PLAIN_AXES", ICON_EMPTY_AXIS, "Plain Axes", ""},
    {OB_ARROWS, "ARROWS", ICON_EMPTY_ARROWS, "Arrows", ""},
    {OB_SINGLE_ARROW, "SINGLE_ARROW", ICON_EMPTY_SINGLE_ARROW, "Single Arrow", ""},
    {OB_CIRCLE, "CIRCLE", ICON_MESH_CIRCLE, "Circle", ""},
    {OB_CUBE, "CUBE", ICON_CUBE, "Cube", ""},
    {OB_EMPTY_SPHERE, "SPHERE", ICON_SPHERE, "Sphere", ""},
    {OB_EMPTY_CONE, "CONE", ICON_CONE, "Cone", ""},
    {OB_EMPTY_IMAGE, "IMAGE", ICON_FILE_IMAGE, "Image", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_object_empty_image_depth_items[] = {
    {OB_EMPTY_IMAGE_DEPTH_DEFAULT, "DEFAULT", 0, "Default", ""},
    {OB_EMPTY_IMAGE_DEPTH_FRONT, "FRONT", 0, "Front", ""},
    {OB_EMPTY_IMAGE_DEPTH_BACK, "BACK", 0, "Back", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_object_gpencil_type_items[] = {
    {GP_EMPTY, "EMPTY", ICON_EMPTY_AXIS, "Blank", "Create an empty grease pencil object"},
    {GP_STROKE, "STROKE", ICON_STROKE, "Stroke", "Create a simple stroke with basic colors"},
    {GP_MONKEY, "MONKEY", ICON_MONKEY, "Monkey", "Construct a Suzanne grease pencil object"},
    {0, NULL, 0, NULL, NULL}};

static const EnumPropertyItem parent_type_items[] = {
    {PAROBJECT, "OBJECT", 0, "Object", "The object is parented to an object"},
    {PARSKEL, "ARMATURE", 0, "Armature", ""},
    /* PARSKEL reuse will give issues. */
    {PARSKEL, "LATTICE", 0, "Lattice", "The object is parented to a lattice"},
    {PARVERT1, "VERTEX", 0, "Vertex", "The object is parented to a vertex"},
    {PARVERT3, "VERTEX_3", 0, "3 Vertices", ""},
    {PARBONE, "BONE", 0, "Bone", "The object is parented to a bone"},
    {0, NULL, 0, NULL, NULL},
};

#define INSTANCE_ITEMS_SHARED \
  {0, "NONE", 0, "None", ""}, \
      {OB_DUPLIVERTS, "VERTS", 0, "Verts", "Instantiate child objects on all vertices"}, \
  { \
    OB_DUPLIFACES, "FACES", 0, "Faces", "Instantiate child objects on all faces" \
  }

#define INSTANCE_ITEM_COLLECTION \
  { \
    OB_DUPLICOLLECTION, "COLLECTION", 0, "Collection", "Enable collection instancing" \
  }
static const EnumPropertyItem instance_items[] = {
    INSTANCE_ITEMS_SHARED,
    INSTANCE_ITEM_COLLECTION,
    {0, NULL, 0, NULL, NULL},
};
#ifdef RNA_RUNTIME
static EnumPropertyItem instance_items_nogroup[] = {
    INSTANCE_ITEMS_SHARED,
    {0, NULL, 0, NULL, NULL},
};
#endif
#undef INSTANCE_ITEMS_SHARED
#undef INSTANCE_ITEM_COLLECTION

const EnumPropertyItem rna_enum_metaelem_type_items[] = {
    {MB_BALL, "BALL", ICON_META_BALL, "Ball", ""},
    {MB_TUBE, "CAPSULE", ICON_META_CAPSULE, "Capsule", ""},
    {MB_PLANE, "PLANE", ICON_META_PLANE, "Plane", ""},
    /* NOTE: typo at original definition! */
    {MB_ELIPSOID, "ELLIPSOID", ICON_META_ELLIPSOID, "Ellipsoid", ""},
    {MB_CUBE, "CUBE", ICON_META_CUBE, "Cube", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_lightprobes_type_items[] = {
    {LIGHTPROBE_TYPE_CUBE, "CUBE", ICON_LIGHTPROBE_CUBEMAP, "Cube", ""},
    {LIGHTPROBE_TYPE_PLANAR, "PLANAR", ICON_LIGHTPROBE_PLANAR, "Planar", ""},
    {LIGHTPROBE_TYPE_GRID, "GRID", ICON_LIGHTPROBE_GRID, "Grid", ""},
    {0, NULL, 0, NULL, NULL},
};

/* used for 2 enums */
#define OBTYPE_CU_CURVE \
  { \
    OB_CURVE, "CURVE", 0, "Curve", "" \
  }
#define OBTYPE_CU_SURF \
  { \
    OB_SURF, "SURFACE", 0, "Surface", "" \
  }
#define OBTYPE_CU_FONT \
  { \
    OB_FONT, "FONT", 0, "Font", "" \
  }

const EnumPropertyItem rna_enum_object_type_items[] = {
    {OB_MESH, "MESH", 0, "Mesh", ""},
    OBTYPE_CU_CURVE,
    OBTYPE_CU_SURF,
    {OB_MBALL, "META", 0, "Meta", ""},
    OBTYPE_CU_FONT,
    {OB_HAIR, "HAIR", 0, "Hair", ""},
    {OB_POINTCLOUD, "POINTCLOUD", 0, "PointCloud", ""},
    {OB_VOLUME, "VOLUME", 0, "Volume", ""},
    {0, "", 0, NULL, NULL},
    {OB_ARMATURE, "ARMATURE", 0, "Armature", ""},
    {OB_LATTICE, "LATTICE", 0, "Lattice", ""},
    {OB_EMPTY, "EMPTY", 0, "Empty", ""},
    {OB_GPENCIL, "GPENCIL", 0, "GPencil", ""},
    {0, "", 0, NULL, NULL},
    {OB_CAMERA, "CAMERA", 0, "Camera", ""},
    {OB_LAMP, "LIGHT", 0, "Light", ""},
    {OB_SPEAKER, "SPEAKER", 0, "Speaker", ""},
    {OB_LIGHTPROBE, "LIGHT_PROBE", 0, "Probe", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_object_type_curve_items[] = {
    OBTYPE_CU_CURVE,
    OBTYPE_CU_SURF,
    OBTYPE_CU_FONT,
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_object_rotation_mode_items[] = {
    {ROT_MODE_QUAT, "QUATERNION", 0, "Quaternion (WXYZ)", "No Gimbal Lock"},
    {ROT_MODE_XYZ, "XYZ", 0, "XYZ Euler", "XYZ Rotation Order - prone to Gimbal Lock (default)"},
    {ROT_MODE_XZY, "XZY", 0, "XZY Euler", "XZY Rotation Order - prone to Gimbal Lock"},
    {ROT_MODE_YXZ, "YXZ", 0, "YXZ Euler", "YXZ Rotation Order - prone to Gimbal Lock"},
    {ROT_MODE_YZX, "YZX", 0, "YZX Euler", "YZX Rotation Order - prone to Gimbal Lock"},
    {ROT_MODE_ZXY, "ZXY", 0, "ZXY Euler", "ZXY Rotation Order - prone to Gimbal Lock"},
    {ROT_MODE_ZYX, "ZYX", 0, "ZYX Euler", "ZYX Rotation Order - prone to Gimbal Lock"},
    {ROT_MODE_AXISANGLE,
     "AXIS_ANGLE",
     0,
     "Axis Angle",
     "Axis Angle (W+XYZ), defines a rotation around some axis defined by 3D-Vector"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_object_axis_items[] = {
    {OB_POSX, "POS_X", 0, "+X", ""},
    {OB_POSY, "POS_Y", 0, "+Y", ""},
    {OB_POSZ, "POS_Z", 0, "+Z", ""},
    {OB_NEGX, "NEG_X", 0, "-X", ""},
    {OB_NEGY, "NEG_Y", 0, "-Y", ""},
    {OB_NEGZ, "NEG_Z", 0, "-Z", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_math.h"

#  include "DNA_ID.h"
#  include "DNA_constraint_types.h"
#  include "DNA_gpencil_types.h"
#  include "DNA_key_types.h"
#  include "DNA_lattice_types.h"
#  include "DNA_node_types.h"

#  include "BKE_armature.h"
#  include "BKE_brush.h"
#  include "BKE_constraint.h"
#  include "BKE_context.h"
#  include "BKE_curve.h"
#  include "BKE_deform.h"
#  include "BKE_effect.h"
#  include "BKE_global.h"
#  include "BKE_key.h"
#  include "BKE_material.h"
#  include "BKE_mesh.h"
#  include "BKE_modifier.h"
#  include "BKE_object.h"
#  include "BKE_particle.h"
#  include "BKE_scene.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "ED_curve.h"
#  include "ED_lattice.h"
#  include "ED_object.h"
#  include "ED_particle.h"

static void rna_Object_internal_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_TRANSFORM);
}

static void rna_Object_internal_update_draw(Main *UNUSED(bmain),
                                            Scene *UNUSED(scene),
                                            PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_SHADING);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ptr->owner_id);
}

static void rna_Object_matrix_world_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* don't use compat so we get predictable rotation */
  BKE_object_apply_mat4((Object *)ptr->owner_id, ((Object *)ptr->owner_id)->obmat, false, true);
  rna_Object_internal_update(bmain, scene, ptr);
}

static void rna_Object_hide_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  BKE_main_collection_sync_remap(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &ob->id);
}

static void rna_Object_duplicator_visibility_flag_update(Main *UNUSED(bmain),
                                                         Scene *UNUSED(scene),
                                                         PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

static void rna_MaterialIndex_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  if (ob && ob->type == OB_GPENCIL) {
    /* notifying material property in topbar */
    WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }
}

static void rna_GPencil_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  if (ob && ob->type == OB_GPENCIL) {
    bGPdata *gpd = (bGPdata *)ob->data;
    DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
  }
}

static void rna_Object_matrix_local_get(PointerRNA *ptr, float values[16])
{
  Object *ob = (Object *)ptr->owner_id;
  BKE_object_matrix_local_get(ob, (float(*)[4])values);
}

static void rna_Object_matrix_local_set(PointerRNA *ptr, const float values[16])
{
  Object *ob = (Object *)ptr->owner_id;
  float local_mat[4][4];

  /* Localspace matrix is truly relative to the parent,
   * but parameters stored in object are relative to parentinv matrix.
   * Undo the parent inverse part before applying it as local matrix. */
  if (ob->parent) {
    float invmat[4][4];
    invert_m4_m4(invmat, ob->parentinv);
    mul_m4_m4m4(local_mat, invmat, (float(*)[4])values);
  }
  else {
    copy_m4_m4(local_mat, (float(*)[4])values);
  }

  /* Don't use compat so we get predictable rotation, and do not use parenting either,
   * because it's a local matrix! */
  BKE_object_apply_mat4(ob, local_mat, false, false);
}

static void rna_Object_matrix_basis_get(PointerRNA *ptr, float values[16])
{
  Object *ob = (Object *)ptr->owner_id;
  BKE_object_to_mat4(ob, (float(*)[4])values);
}

static void rna_Object_matrix_basis_set(PointerRNA *ptr, const float values[16])
{
  Object *ob = (Object *)ptr->owner_id;
  BKE_object_apply_mat4(ob, (float(*)[4])values, false, false);
}

void rna_Object_internal_update_data_impl(PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ptr->owner_id);
}

void rna_Object_internal_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  rna_Object_internal_update_data_impl(ptr);
}

void rna_Object_internal_update_data_dependency(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);
  rna_Object_internal_update_data_impl(ptr);
}

static void rna_Object_active_shape_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  if (BKE_object_is_in_editmode(ob)) {
    /* exit/enter editmode to get new shape */
    switch (ob->type) {
      case OB_MESH: {
        Mesh *me = ob->data;
        BMEditMesh *em = me->edit_mesh;
        int select_mode = em->selectmode;
        EDBM_mesh_load(bmain, ob);
        EDBM_mesh_make(ob, select_mode, true);
        em = me->edit_mesh;

        DEG_id_tag_update(&me->id, 0);

        EDBM_mesh_normals_update(em);
        BKE_editmesh_looptri_calc(em);
        break;
      }
      case OB_CURVE:
      case OB_SURF:
        ED_curve_editnurb_load(bmain, ob);
        ED_curve_editnurb_make(ob);
        break;
      case OB_LATTICE:
        BKE_editlattice_load(ob);
        BKE_editlattice_make(ob);
        break;
    }
  }

  rna_Object_internal_update_data_impl(ptr);
}

static void rna_Object_dependency_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_TRANSFORM);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_PARENT, ptr->owner_id);
}

static void rna_Object_data_set(PointerRNA *ptr, PointerRNA value, struct ReportList *reports)
{
  Object *ob = (Object *)ptr->data;
  ID *id = value.data;

  if (ob->mode & OB_MODE_EDIT) {
    return;
  }

  /* assigning NULL only for empties */
  if ((id == NULL) && (ob->type != OB_EMPTY)) {
    return;
  }

  if (id && ((id->tag & LIB_TAG_NO_MAIN) != (ob->id.tag & LIB_TAG_NO_MAIN))) {
    BKE_report(reports,
               RPT_ERROR,
               "Can only assign evaluated data to evaluated object, or original data to "
               "original object");
    return;
  }

  if (ob->type == OB_EMPTY) {
    if (ob->data) {
      id_us_min((ID *)ob->data);
      ob->data = NULL;
    }

    if (!id || GS(id->name) == ID_IM) {
      id_us_plus(id);
      ob->data = id;
    }
  }
  else if (ob->type == OB_MESH) {
    BKE_mesh_assign_object(G_MAIN, ob, (Mesh *)id);
  }
  else {
    if (ob->data) {
      id_us_min((ID *)ob->data);
    }

    /* no need to type-check here ID. this is done in the _typef() function */
    BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));
    id_us_plus(id);

    ob->data = id;
    BKE_object_materials_test(G_MAIN, ob, id);

    if (GS(id->name) == ID_CU) {
      BKE_curve_type_test(ob);
    }
    else if (ob->type == OB_ARMATURE) {
      BKE_pose_rebuild(G_MAIN, ob, ob->data, true);
    }
  }
}

static StructRNA *rna_Object_data_typef(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->data;

  /* keep in sync with OB_DATA_SUPPORT_ID() macro */
  switch (ob->type) {
    case OB_EMPTY:
      return &RNA_Image;
    case OB_MESH:
      return &RNA_Mesh;
    case OB_CURVE:
      return &RNA_Curve;
    case OB_SURF:
      return &RNA_Curve;
    case OB_FONT:
      return &RNA_Curve;
    case OB_MBALL:
      return &RNA_MetaBall;
    case OB_LAMP:
      return &RNA_Light;
    case OB_CAMERA:
      return &RNA_Camera;
    case OB_LATTICE:
      return &RNA_Lattice;
    case OB_ARMATURE:
      return &RNA_Armature;
    case OB_SPEAKER:
      return &RNA_Speaker;
    case OB_LIGHTPROBE:
      return &RNA_LightProbe;
    case OB_GPENCIL:
      return &RNA_GreasePencil;
    case OB_HAIR:
      return &RNA_Hair;
    case OB_POINTCLOUD:
      return &RNA_PointCloud;
    case OB_VOLUME:
      return &RNA_Volume;
    default:
      return &RNA_ID;
  }
}

static bool rna_Object_data_poll(PointerRNA *ptr, const PointerRNA value)
{
  Object *ob = (Object *)ptr->data;

  if (ob->type == OB_GPENCIL) {
    /* GP Object - Don't allow using "Annotation" GP datablocks here */
    bGPdata *gpd = value.data;
    return (gpd->flag & GP_DATA_ANNOTATIONS) == 0;
  }

  return true;
}

static void rna_Object_parent_set(PointerRNA *ptr,
                                  PointerRNA value,
                                  struct ReportList *UNUSED(reports))
{
  Object *ob = (Object *)ptr->data;
  Object *par = (Object *)value.data;

  {
    ED_object_parent(ob, par, ob->partype, ob->parsubstr);
  }
}

static bool rna_Object_parent_override_apply(Main *UNUSED(bmain),
                                             PointerRNA *ptr_dst,
                                             PointerRNA *ptr_src,
                                             PointerRNA *ptr_storage,
                                             PropertyRNA *prop_dst,
                                             PropertyRNA *prop_src,
                                             PropertyRNA *UNUSED(prop_storage),
                                             const int len_dst,
                                             const int len_src,
                                             const int len_storage,
                                             PointerRNA *UNUSED(ptr_item_dst),
                                             PointerRNA *UNUSED(ptr_item_src),
                                             PointerRNA *UNUSED(ptr_item_storage),
                                             IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert(opop->operation == IDOVERRIDE_LIBRARY_OP_REPLACE &&
             "Unsupported RNA override operation on object parent pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* We need a special handling here because setting parent resets invert parent matrix,
   * which is evil in our case. */
  Object *ob = (Object *)ptr_dst->data;
  Object *parent_dst = RNA_property_pointer_get(ptr_dst, prop_dst).data;
  Object *parent_src = RNA_property_pointer_get(ptr_src, prop_src).data;

  if (parent_src == parent_dst) {
    return false;
  }

  if (parent_src == NULL) {
    /* The only case where we do want default behavior (with matrix reset). */
    ED_object_parent(ob, parent_src, ob->partype, ob->parsubstr);
  }
  else {
    ob->parent = parent_src;
  }
  return true;
}

static void rna_Object_parent_type_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->data;

  ED_object_parent(ob, ob->parent, value, ob->parsubstr);
}

static const EnumPropertyItem *rna_Object_parent_type_itemf(bContext *UNUSED(C),
                                                            PointerRNA *ptr,
                                                            PropertyRNA *UNUSED(prop),
                                                            bool *r_free)
{
  Object *ob = (Object *)ptr->data;
  EnumPropertyItem *item = NULL;
  int totitem = 0;

  RNA_enum_items_add_value(&item, &totitem, parent_type_items, PAROBJECT);

  if (ob->parent) {
    Object *par = ob->parent;

    if (par->type == OB_LATTICE) {
      /* special hack: prevents this overriding others */
      RNA_enum_items_add_value(&item, &totitem, &parent_type_items[2], PARSKEL);
    }
    else if (par->type == OB_ARMATURE) {
      /* special hack: prevents this being overridden */
      RNA_enum_items_add_value(&item, &totitem, &parent_type_items[1], PARSKEL);
      RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARBONE);
    }

    if (OB_TYPE_SUPPORT_PARVERT(par->type)) {
      RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARVERT1);
      RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARVERT3);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_Object_empty_display_type_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->data;

  BKE_object_empty_draw_type_set(ob, value);
}

static void rna_Object_parent_bone_set(PointerRNA *ptr, const char *value)
{
  Object *ob = (Object *)ptr->data;

  ED_object_parent(ob, ob->parent, ob->partype, value);
}

static const EnumPropertyItem *rna_Object_instance_type_itemf(bContext *UNUSED(C),
                                                              PointerRNA *ptr,
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *UNUSED(r_free))
{
  Object *ob = (Object *)ptr->data;
  const EnumPropertyItem *item;

  if (ob->type == OB_EMPTY) {
    item = instance_items;
  }
  else {
    item = instance_items_nogroup;
  }

  return item;
}

static void rna_Object_dup_collection_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          struct ReportList *UNUSED(reports))
{
  Object *ob = (Object *)ptr->data;
  Collection *grp = (Collection *)value.data;

  /* must not let this be set if the object belongs in this group already,
   * thus causing a cycle/infinite-recursion leading to crashes on load [#25298]
   */
  if (BKE_collection_has_object_recursive(grp, ob) == 0) {
    if (ob->type == OB_EMPTY) {
      id_us_min(&ob->instance_collection->id);
      ob->instance_collection = grp;
      id_us_plus(&ob->instance_collection->id);
    }
    else {
      BKE_report(NULL, RPT_ERROR, "Only empty objects support collection instances");
    }
  }
  else {
    BKE_report(NULL,
               RPT_ERROR,
               "Cannot set instance-collection as object belongs in group being instanced, thus "
               "causing a cycle");
  }
}

static void rna_VertexGroup_name_set(PointerRNA *ptr, const char *value)
{
  Object *ob = (Object *)ptr->owner_id;
  bDeformGroup *dg = (bDeformGroup *)ptr->data;
  BLI_strncpy_utf8(dg->name, value, sizeof(dg->name));
  BKE_object_defgroup_unique_name(dg, ob);
}

static int rna_VertexGroup_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  return BLI_findindex(&ob->defbase, ptr->data);
}

static PointerRNA rna_Object_active_vertex_group_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return rna_pointer_inherit_refine(
      ptr, &RNA_VertexGroup, BLI_findlink(&ob->defbase, ob->actdef - 1));
}

static void rna_Object_active_vertex_group_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               struct ReportList *reports)
{
  Object *ob = (Object *)ptr->owner_id;
  int index = BLI_findindex(&ob->defbase, value.data);
  if (index == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "VertexGroup '%s' not found in object '%s'",
                ((bDeformGroup *)value.data)->name,
                ob->id.name + 2);
    return;
  }

  ob->actdef = index + 1;
}

static int rna_Object_active_vertex_group_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return ob->actdef - 1;
}

static void rna_Object_active_vertex_group_index_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->owner_id;
  ob->actdef = value + 1;
}

static void rna_Object_active_vertex_group_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Object *ob = (Object *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&ob->defbase) - 1);
}

void rna_object_vgroup_name_index_get(PointerRNA *ptr, char *value, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  bDeformGroup *dg;

  dg = BLI_findlink(&ob->defbase, index - 1);

  if (dg) {
    BLI_strncpy(value, dg->name, sizeof(dg->name));
  }
  else {
    value[0] = '\0';
  }
}

int rna_object_vgroup_name_index_length(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  bDeformGroup *dg;

  dg = BLI_findlink(&ob->defbase, index - 1);
  return (dg) ? strlen(dg->name) : 0;
}

void rna_object_vgroup_name_index_set(PointerRNA *ptr, const char *value, short *index)
{
  Object *ob = (Object *)ptr->owner_id;
  *index = BKE_object_defgroup_name_index(ob, value) + 1;
}

void rna_object_vgroup_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
  Object *ob = (Object *)ptr->owner_id;
  bDeformGroup *dg = BKE_object_defgroup_find_name(ob, value);
  if (dg) {
    /* No need for BLI_strncpy_utf8, since this matches an existing group. */
    BLI_strncpy(result, value, maxlen);
    return;
  }

  result[0] = '\0';
}

static void rna_FaceMap_name_set(PointerRNA *ptr, const char *value)
{
  Object *ob = (Object *)ptr->owner_id;
  bFaceMap *fmap = (bFaceMap *)ptr->data;
  BLI_strncpy_utf8(fmap->name, value, sizeof(fmap->name));
  BKE_object_facemap_unique_name(ob, fmap);
}

static int rna_FaceMap_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  return BLI_findindex(&ob->fmaps, ptr->data);
}

static PointerRNA rna_Object_active_face_map_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return rna_pointer_inherit_refine(ptr, &RNA_FaceMap, BLI_findlink(&ob->fmaps, ob->actfmap - 1));
}

static int rna_Object_active_face_map_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return ob->actfmap - 1;
}

static void rna_Object_active_face_map_index_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->owner_id;
  ob->actfmap = value + 1;
}

static void rna_Object_active_face_map_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Object *ob = (Object *)ptr->owner_id;

  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&ob->fmaps) - 1);
}

void rna_object_BKE_object_facemap_name_index_get(PointerRNA *ptr, char *value, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  bFaceMap *fmap;

  fmap = BLI_findlink(&ob->fmaps, index - 1);

  if (fmap) {
    BLI_strncpy(value, fmap->name, sizeof(fmap->name));
  }
  else {
    value[0] = '\0';
  }
}

int rna_object_BKE_object_facemap_name_index_length(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->owner_id;
  bFaceMap *fmap;

  fmap = BLI_findlink(&ob->fmaps, index - 1);
  return (fmap) ? strlen(fmap->name) : 0;
}

void rna_object_BKE_object_facemap_name_index_set(PointerRNA *ptr, const char *value, short *index)
{
  Object *ob = (Object *)ptr->owner_id;
  *index = BKE_object_facemap_name_index(ob, value) + 1;
}

void rna_object_fmap_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
  Object *ob = (Object *)ptr->owner_id;
  bFaceMap *fmap = BKE_object_facemap_find_name(ob, value);
  if (fmap) {
    /* No need for BLI_strncpy_utf8, since this matches an existing group. */
    BLI_strncpy(result, value, maxlen);
    return;
  }

  result[0] = '\0';
}

void rna_object_uvlayer_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
  Object *ob = (Object *)ptr->owner_id;
  Mesh *me;
  CustomDataLayer *layer;
  int a;

  if (ob->type == OB_MESH && ob->data) {
    me = (Mesh *)ob->data;

    for (a = 0; a < me->ldata.totlayer; a++) {
      layer = &me->ldata.layers[a];

      if (layer->type == CD_MLOOPUV && STREQ(layer->name, value)) {
        BLI_strncpy(result, value, maxlen);
        return;
      }
    }
  }

  result[0] = '\0';
}

void rna_object_vcollayer_name_set(PointerRNA *ptr, const char *value, char *result, int maxlen)
{
  Object *ob = (Object *)ptr->owner_id;
  Mesh *me;
  CustomDataLayer *layer;
  int a;

  if (ob->type == OB_MESH && ob->data) {
    me = (Mesh *)ob->data;

    for (a = 0; a < me->fdata.totlayer; a++) {
      layer = &me->fdata.layers[a];

      if (layer->type == CD_MCOL && STREQ(layer->name, value)) {
        BLI_strncpy(result, value, maxlen);
        return;
      }
    }
  }

  result[0] = '\0';
}

static int rna_Object_active_material_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return MAX2(ob->actcol - 1, 0);
}

static void rna_Object_active_material_index_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->owner_id;
  ob->actcol = value + 1;

  if (ob->type == OB_MESH) {
    Mesh *me = ob->data;

    if (me->edit_mesh) {
      me->edit_mesh->mat_nr = value;
    }
  }
}

static void rna_Object_active_material_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Object *ob = (Object *)ptr->owner_id;
  *min = 0;
  *max = max_ii(ob->totcol - 1, 0);
}

/* returns active base material */
static PointerRNA rna_Object_active_material_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma;

  ma = (ob->totcol) ? BKE_object_material_get(ob, ob->actcol) : NULL;
  return rna_pointer_inherit_refine(ptr, &RNA_Material, ma);
}

static void rna_Object_active_material_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           struct ReportList *UNUSED(reports))
{
  Object *ob = (Object *)ptr->owner_id;

  DEG_id_tag_update(value.data, 0);
  BLI_assert(BKE_id_is_in_global_main(&ob->id));
  BLI_assert(BKE_id_is_in_global_main(value.data));
  BKE_object_material_assign(G_MAIN, ob, value.data, ob->actcol, BKE_MAT_ASSIGN_EXISTING);

  if (ob && ob->type == OB_GPENCIL) {
    /* notifying material property in topbar */
    WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, NULL);
  }
}

static int rna_Object_active_material_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  Object *ob = (Object *)ptr->owner_id;
  bool is_editable;

  if ((ob->matbits == NULL) || (ob->actcol == 0) || ob->matbits[ob->actcol - 1]) {
    is_editable = !ID_IS_LINKED(ob);
  }
  else {
    is_editable = ob->data ? !ID_IS_LINKED(ob->data) : false;
  }

  return is_editable ? PROP_EDITABLE : 0;
}

static void rna_Object_active_particle_system_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Object *ob = (Object *)ptr->owner_id;
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&ob->particlesystem) - 1);
}

static int rna_Object_active_particle_system_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  return psys_get_current_num(ob);
}

static void rna_Object_active_particle_system_index_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->owner_id;
  psys_set_current_num(ob, value);
}

static void rna_Object_particle_update(Main *UNUSED(bmain), Scene *scene, PointerRNA *ptr)
{
  /* TODO: Disabled for now, because bContext is not available. */
#  if 0
  Object *ob = (Object *)ptr->owner_id;
  PE_current_changed(NULL, scene, ob);
#  else
  (void)scene;
  (void)ptr;
#  endif
}

/* rotation - axis-angle */
static void rna_Object_rotation_axis_angle_get(PointerRNA *ptr, float *value)
{
  Object *ob = ptr->data;

  /* for now, assume that rotation mode is axis-angle */
  value[0] = ob->rotAngle;
  copy_v3_v3(&value[1], ob->rotAxis);
}

/* rotation - axis-angle */
static void rna_Object_rotation_axis_angle_set(PointerRNA *ptr, const float *value)
{
  Object *ob = ptr->data;

  /* for now, assume that rotation mode is axis-angle */
  ob->rotAngle = value[0];
  copy_v3_v3(ob->rotAxis, &value[1]);

  /* TODO: validate axis? */
}

static void rna_Object_rotation_mode_set(PointerRNA *ptr, int value)
{
  Object *ob = ptr->data;

  /* use API Method for conversions... */
  BKE_rotMode_change_values(
      ob->quat, ob->rot, ob->rotAxis, &ob->rotAngle, ob->rotmode, (short)value);

  /* finally, set the new rotation type */
  ob->rotmode = value;
}

static void rna_Object_dimensions_get(PointerRNA *ptr, float *value)
{
  Object *ob = ptr->data;
  BKE_object_dimensions_get(ob, value);
}

static void rna_Object_dimensions_set(PointerRNA *ptr, const float *value)
{
  Object *ob = ptr->data;
  BKE_object_dimensions_set(ob, value, 0);
}

static int rna_Object_location_editable(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->data;

  /* only if the axis in question is locked, not editable... */
  if ((index == 0) && (ob->protectflag & OB_LOCK_LOCX)) {
    return 0;
  }
  else if ((index == 1) && (ob->protectflag & OB_LOCK_LOCY)) {
    return 0;
  }
  else if ((index == 2) && (ob->protectflag & OB_LOCK_LOCZ)) {
    return 0;
  }
  else {
    return PROP_EDITABLE;
  }
}

static int rna_Object_scale_editable(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->data;

  /* only if the axis in question is locked, not editable... */
  if ((index == 0) && (ob->protectflag & OB_LOCK_SCALEX)) {
    return 0;
  }
  else if ((index == 1) && (ob->protectflag & OB_LOCK_SCALEY)) {
    return 0;
  }
  else if ((index == 2) && (ob->protectflag & OB_LOCK_SCALEZ)) {
    return 0;
  }
  else {
    return PROP_EDITABLE;
  }
}

static int rna_Object_rotation_euler_editable(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->data;

  /* only if the axis in question is locked, not editable... */
  if ((index == 0) && (ob->protectflag & OB_LOCK_ROTX)) {
    return 0;
  }
  else if ((index == 1) && (ob->protectflag & OB_LOCK_ROTY)) {
    return 0;
  }
  else if ((index == 2) && (ob->protectflag & OB_LOCK_ROTZ)) {
    return 0;
  }
  else {
    return PROP_EDITABLE;
  }
}

static int rna_Object_rotation_4d_editable(PointerRNA *ptr, int index)
{
  Object *ob = (Object *)ptr->data;

  /* only consider locks if locking components individually... */
  if (ob->protectflag & OB_LOCK_ROT4D) {
    /* only if the axis in question is locked, not editable... */
    if ((index == 0) && (ob->protectflag & OB_LOCK_ROTW)) {
      return 0;
    }
    else if ((index == 1) && (ob->protectflag & OB_LOCK_ROTX)) {
      return 0;
    }
    else if ((index == 2) && (ob->protectflag & OB_LOCK_ROTY)) {
      return 0;
    }
    else if ((index == 3) && (ob->protectflag & OB_LOCK_ROTZ)) {
      return 0;
    }
  }

  return PROP_EDITABLE;
}

static int rna_MaterialSlot_material_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
  Object *ob = (Object *)ptr->owner_id;
  const int index = (Material **)ptr->data - ob->mat;
  bool is_editable;

  if ((ob->matbits == NULL) || ob->matbits[index]) {
    is_editable = !ID_IS_LINKED(ob);
  }
  else {
    is_editable = ob->data ? !ID_IS_LINKED(ob->data) : false;
  }

  return is_editable ? PROP_EDITABLE : 0;
}

static PointerRNA rna_MaterialSlot_material_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma;
  const int index = (Material **)ptr->data - ob->mat;

  ma = BKE_object_material_get(ob, index + 1);
  return rna_pointer_inherit_refine(ptr, &RNA_Material, ma);
}

static void rna_MaterialSlot_material_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          struct ReportList *UNUSED(reports))
{
  Object *ob = (Object *)ptr->owner_id;
  int index = (Material **)ptr->data - ob->mat;

  BLI_assert(BKE_id_is_in_global_main(&ob->id));
  BLI_assert(BKE_id_is_in_global_main(value.data));
  BKE_object_material_assign(G_MAIN, ob, value.data, index + 1, BKE_MAT_ASSIGN_EXISTING);
}

static bool rna_MaterialSlot_material_poll(PointerRNA *ptr, PointerRNA value)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma = (Material *)value.data;

  if (ob->type == OB_GPENCIL) {
    /* GP Materials only */
    return (ma->gp_style != NULL);
  }
  else {
    /* Everything except GP materials */
    return (ma->gp_style == NULL);
  }
}

static int rna_MaterialSlot_link_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  int index = (Material **)ptr->data - ob->mat;

  return ob->matbits[index] != 0;
}

static void rna_MaterialSlot_link_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->owner_id;
  int index = (Material **)ptr->data - ob->mat;

  if (value) {
    ob->matbits[index] = 1;
    /* ob->colbits |= (1 << index); */ /* DEPRECATED */
  }
  else {
    ob->matbits[index] = 0;
    /* ob->colbits &= ~(1 << index); */ /* DEPRECATED */
  }
}

static int rna_MaterialSlot_name_length(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma;
  int index = (Material **)ptr->data - ob->mat;

  ma = BKE_object_material_get(ob, index + 1);

  if (ma) {
    return strlen(ma->id.name + 2);
  }

  return 0;
}

static void rna_MaterialSlot_name_get(PointerRNA *ptr, char *str)
{
  Object *ob = (Object *)ptr->owner_id;
  Material *ma;
  int index = (Material **)ptr->data - ob->mat;

  ma = BKE_object_material_get(ob, index + 1);

  if (ma) {
    strcpy(str, ma->id.name + 2);
  }
  else {
    str[0] = '\0';
  }
}

static void rna_MaterialSlot_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Object_internal_update(bmain, scene, ptr);

  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, ptr->owner_id);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, NULL);
  DEG_relations_tag_update(bmain);
}

static char *rna_MaterialSlot_path(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  int index = (Material **)ptr->data - ob->mat;

  return BLI_sprintfN("material_slots[%d]", index);
}

static PointerRNA rna_Object_display_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_ObjectDisplay, ptr->data);
}

static char *rna_ObjectDisplay_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("display");
}

static PointerRNA rna_Object_active_particle_system_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  ParticleSystem *psys = psys_get_current(ob);
  return rna_pointer_inherit_refine(ptr, &RNA_ParticleSystem, psys);
}

static void rna_Object_active_shape_key_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  Object *ob = (Object *)ptr->owner_id;
  Key *key = BKE_key_from_object(ob);

  *min = 0;
  if (key) {
    *max = BLI_listbase_count(&key->block) - 1;
    if (*max < 0) {
      *max = 0;
    }
  }
  else {
    *max = 0;
  }
}

static int rna_Object_active_shape_key_index_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  return MAX2(ob->shapenr - 1, 0);
}

static void rna_Object_active_shape_key_index_set(PointerRNA *ptr, int value)
{
  Object *ob = (Object *)ptr->owner_id;

  ob->shapenr = value + 1;
}

static PointerRNA rna_Object_active_shape_key_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  Key *key = BKE_key_from_object(ob);
  KeyBlock *kb;
  PointerRNA keyptr;

  if (key == NULL) {
    return PointerRNA_NULL;
  }

  kb = BLI_findlink(&key->block, ob->shapenr - 1);
  RNA_pointer_create((ID *)key, &RNA_ShapeKey, kb, &keyptr);
  return keyptr;
}

static PointerRNA rna_Object_field_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  /* weak */
  if (!ob->pd) {
    ob->pd = BKE_partdeflect_new(0);
  }

  return rna_pointer_inherit_refine(ptr, &RNA_FieldSettings, ob->pd);
}

static PointerRNA rna_Object_collision_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;

  if (ob->type != OB_MESH) {
    return PointerRNA_NULL;
  }

  /* weak */
  if (!ob->pd) {
    ob->pd = BKE_partdeflect_new(0);
  }

  return rna_pointer_inherit_refine(ptr, &RNA_CollisionSettings, ob->pd);
}

static PointerRNA rna_Object_active_constraint_get(PointerRNA *ptr)
{
  Object *ob = (Object *)ptr->owner_id;
  bConstraint *con = BKE_constraints_active_get(&ob->constraints);
  return rna_pointer_inherit_refine(ptr, &RNA_Constraint, con);
}

static void rna_Object_active_constraint_set(PointerRNA *ptr,
                                             PointerRNA value,
                                             struct ReportList *UNUSED(reports))
{
  Object *ob = (Object *)ptr->owner_id;
  BKE_constraints_active_set(&ob->constraints, (bConstraint *)value.data);
}

static bConstraint *rna_Object_constraints_new(Object *object, Main *bmain, int type)
{
  bConstraint *new_con = BKE_constraint_add_for_object(object, NULL, type);

  ED_object_constraint_tag_update(bmain, object, new_con);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, object);

  return new_con;
}

static void rna_Object_constraints_remove(Object *object,
                                          Main *bmain,
                                          ReportList *reports,
                                          PointerRNA *con_ptr)
{
  bConstraint *con = con_ptr->data;
  if (BLI_findindex(&object->constraints, con) == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Constraint '%s' not found in object '%s'",
                con->name,
                object->id.name + 2);
    return;
  }

  BKE_constraint_remove(&object->constraints, con);
  RNA_POINTER_INVALIDATE(con_ptr);

  ED_object_constraint_update(bmain, object);
  ED_object_constraint_active_set(object, NULL);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, object);
}

static void rna_Object_constraints_clear(Object *object, Main *bmain)
{
  BKE_constraints_free(&object->constraints);

  ED_object_constraint_update(bmain, object);
  ED_object_constraint_active_set(object, NULL);

  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, object);
}

static void rna_Object_constraints_move(
    Object *object, Main *bmain, ReportList *reports, int from, int to)
{
  if (from == to) {
    return;
  }

  if (!BLI_listbase_move_index(&object->constraints, from, to)) {
    BKE_reportf(reports, RPT_ERROR, "Could not move constraint from index '%d' to '%d'", from, to);
    return;
  }

  ED_object_constraint_tag_update(bmain, object, NULL);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT, object);
}

static bConstraint *rna_Object_constraints_copy(Object *object, Main *bmain, PointerRNA *con_ptr)
{
  bConstraint *con = con_ptr->data;
  bConstraint *new_con = BKE_constraint_copy_for_object(object, con);

  ED_object_constraint_tag_update(bmain, object, new_con);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, object);

  return new_con;
}

bool rna_Object_constraints_override_apply(Main *UNUSED(bmain),
                                           PointerRNA *ptr_dst,
                                           PointerRNA *ptr_src,
                                           PointerRNA *UNUSED(ptr_storage),
                                           PropertyRNA *UNUSED(prop_dst),
                                           PropertyRNA *UNUSED(prop_src),
                                           PropertyRNA *UNUSED(prop_storage),
                                           const int UNUSED(len_dst),
                                           const int UNUSED(len_src),
                                           const int UNUSED(len_storage),
                                           PointerRNA *UNUSED(ptr_item_dst),
                                           PointerRNA *UNUSED(ptr_item_src),
                                           PointerRNA *UNUSED(ptr_item_storage),
                                           IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(opop->operation == IDOVERRIDE_LIBRARY_OP_INSERT_AFTER &&
             "Unsupported RNA override operation on constraints collection");

  Object *ob_dst = (Object *)ptr_dst->owner_id;
  Object *ob_src = (Object *)ptr_src->owner_id;

  /* Remember that insertion operations are defined and stored in correct order, which means that
   * even if we insert several items in a row, we always insert first one, then second one, etc.
   * So we should always find 'anchor' constraint in both _src *and* _dst. */
  bConstraint *con_anchor = NULL;
  if (opop->subitem_local_name && opop->subitem_local_name[0]) {
    con_anchor = BLI_findstring(
        &ob_dst->constraints, opop->subitem_local_name, offsetof(bConstraint, name));
  }
  if (con_anchor == NULL && opop->subitem_local_index >= 0) {
    con_anchor = BLI_findlink(&ob_dst->constraints, opop->subitem_local_index);
  }
  /* Otherwise we just insert in first position. */

  bConstraint *con_src = NULL;
  if (opop->subitem_local_name && opop->subitem_local_name[0]) {
    con_src = BLI_findstring(
        &ob_src->constraints, opop->subitem_local_name, offsetof(bConstraint, name));
  }
  if (con_src == NULL && opop->subitem_local_index >= 0) {
    con_src = BLI_findlink(&ob_src->constraints, opop->subitem_local_index);
  }
  con_src = con_src ? con_src->next : ob_src->constraints.first;

  BLI_assert(con_src != NULL);

  bConstraint *con_dst = BKE_constraint_duplicate_ex(con_src, 0, true);

  /* This handles NULL anchor as expected by adding at head of list. */
  BLI_insertlinkafter(&ob_dst->constraints, con_anchor, con_dst);

  /* This should actually *not* be needed in typical cases.
   * However, if overridden source was edited, we *may* have some new conflicting names. */
  BKE_constraint_unique_name(con_dst, &ob_dst->constraints);

  //  printf("%s: We inserted a constraint...\n", __func__);
  return true;
}

static ModifierData *rna_Object_modifier_new(
    Object *object, bContext *C, ReportList *reports, const char *name, int type)
{
  return ED_object_modifier_add(reports, CTX_data_main(C), CTX_data_scene(C), object, name, type);
}

static void rna_Object_modifier_remove(Object *object,
                                       bContext *C,
                                       ReportList *reports,
                                       PointerRNA *md_ptr)
{
  ModifierData *md = md_ptr->data;
  if (ED_object_modifier_remove(reports, CTX_data_main(C), CTX_data_scene(C), object, md) ==
      false) {
    /* error is already set */
    return;
  }

  RNA_POINTER_INVALIDATE(md_ptr);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_modifier_clear(Object *object, bContext *C)
{
  ED_object_modifier_clear(CTX_data_main(C), CTX_data_scene(C), object);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

bool rna_Object_modifiers_override_apply(Main *bmain,
                                         PointerRNA *ptr_dst,
                                         PointerRNA *ptr_src,
                                         PointerRNA *UNUSED(ptr_storage),
                                         PropertyRNA *UNUSED(prop_dst),
                                         PropertyRNA *UNUSED(prop_src),
                                         PropertyRNA *UNUSED(prop_storage),
                                         const int UNUSED(len_dst),
                                         const int UNUSED(len_src),
                                         const int UNUSED(len_storage),
                                         PointerRNA *UNUSED(ptr_item_dst),
                                         PointerRNA *UNUSED(ptr_item_src),
                                         PointerRNA *UNUSED(ptr_item_storage),
                                         IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(opop->operation == IDOVERRIDE_LIBRARY_OP_INSERT_AFTER &&
             "Unsupported RNA override operation on modifiers collection");

  Object *ob_dst = (Object *)ptr_dst->owner_id;
  Object *ob_src = (Object *)ptr_src->owner_id;

  /* Remember that insertion operations are defined and stored in correct order, which means that
   * even if we insert several items in a row, we always insert first one, then second one, etc.
   * So we should always find 'anchor' modifier in both _src *and* _dst. */
  ModifierData *mod_anchor = NULL;
  if (opop->subitem_local_name && opop->subitem_local_name[0]) {
    mod_anchor = BLI_findstring(
        &ob_dst->modifiers, opop->subitem_local_name, offsetof(ModifierData, name));
  }
  if (mod_anchor == NULL && opop->subitem_local_index >= 0) {
    mod_anchor = BLI_findlink(&ob_dst->modifiers, opop->subitem_local_index);
  }
  /* Otherwise we just insert in first position. */

  ModifierData *mod_src = NULL;
  if (opop->subitem_local_name && opop->subitem_local_name[0]) {
    mod_src = BLI_findstring(
        &ob_src->modifiers, opop->subitem_local_name, offsetof(ModifierData, name));
  }
  if (mod_src == NULL && opop->subitem_local_index >= 0) {
    mod_src = BLI_findlink(&ob_src->modifiers, opop->subitem_local_index);
  }
  mod_src = mod_src ? mod_src->next : ob_src->modifiers.first;

  if (mod_src == NULL) {
    BLI_assert(mod_src != NULL);
    return false;
  }

  /* While it would be nicer to use lower-level BKE_modifier_new() here, this one is lacking
   * special-cases handling (particles and other physics modifiers mostly), so using the ED version
   * instead, to avoid duplicating code. */
  ModifierData *mod_dst = ED_object_modifier_add(
      NULL, bmain, NULL, ob_dst, mod_src->name, mod_src->type);

  /* XXX Current handling of 'copy' from particle-system modifier is *very* bad (it keeps same psys
   * pointer as source, then calling code copies psys of object separately and do some magic
   * remapping of pointers...), unfortunately several pieces of code in Object editing area rely on
   * this behavior. So for now, hacking around it to get it doing what we want it to do, as getting
   * a proper behavior would be everything but trivial, and this whole particle thingy is
   * end-of-life. */
  ParticleSystem *psys_dst = (mod_dst->type == eModifierType_ParticleSystem) ?
                                 ((ParticleSystemModifierData *)mod_dst)->psys :
                                 NULL;
  BKE_modifier_copydata(mod_src, mod_dst);
  if (mod_dst->type == eModifierType_ParticleSystem) {
    psys_dst->flag &= ~PSYS_DELETE;
    ((ParticleSystemModifierData *)mod_dst)->psys = psys_dst;
  }

  BLI_remlink(&ob_dst->modifiers, mod_dst);
  /* This handles NULL anchor as expected by adding at head of list. */
  BLI_insertlinkafter(&ob_dst->modifiers, mod_anchor, mod_dst);

  //  printf("%s: We inserted a modifier '%s'...\n", __func__, mod_dst->name);
  return true;
}

static GpencilModifierData *rna_Object_greasepencil_modifier_new(
    Object *object, bContext *C, ReportList *reports, const char *name, int type)
{
  return ED_object_gpencil_modifier_add(
      reports, CTX_data_main(C), CTX_data_scene(C), object, name, type);
}

static void rna_Object_greasepencil_modifier_remove(Object *object,
                                                    bContext *C,
                                                    ReportList *reports,
                                                    PointerRNA *gmd_ptr)
{
  GpencilModifierData *gmd = gmd_ptr->data;
  if (ED_object_gpencil_modifier_remove(reports, CTX_data_main(C), object, gmd) == false) {
    /* error is already set */
    return;
  }

  RNA_POINTER_INVALIDATE(gmd_ptr);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_greasepencil_modifier_clear(Object *object, bContext *C)
{
  ED_object_gpencil_modifier_clear(CTX_data_main(C), object);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

bool rna_Object_greasepencil_modifiers_override_apply(Main *bmain,
                                                      PointerRNA *ptr_dst,
                                                      PointerRNA *ptr_src,
                                                      PointerRNA *UNUSED(ptr_storage),
                                                      PropertyRNA *UNUSED(prop_dst),
                                                      PropertyRNA *UNUSED(prop_src),
                                                      PropertyRNA *UNUSED(prop_storage),
                                                      const int UNUSED(len_dst),
                                                      const int UNUSED(len_src),
                                                      const int UNUSED(len_storage),
                                                      PointerRNA *UNUSED(ptr_item_dst),
                                                      PointerRNA *UNUSED(ptr_item_src),
                                                      PointerRNA *UNUSED(ptr_item_storage),
                                                      IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(opop->operation == IDOVERRIDE_LIBRARY_OP_INSERT_AFTER &&
             "Unsupported RNA override operation on modifiers collection");

  Object *ob_dst = (Object *)ptr_dst->owner_id;
  Object *ob_src = (Object *)ptr_src->owner_id;

  /* Remember that insertion operations are defined and stored in correct order, which means that
   * even if we insert several items in a row, we always insert first one, then second one, etc.
   * So we should always find 'anchor' modifier in both _src *and* _dst. */
  GpencilModifierData *mod_anchor = NULL;
  if (opop->subitem_local_name && opop->subitem_local_name[0]) {
    mod_anchor = BLI_findstring(
        &ob_dst->greasepencil_modifiers, opop->subitem_local_name, offsetof(ModifierData, name));
  }
  if (mod_anchor == NULL && opop->subitem_local_index >= 0) {
    mod_anchor = BLI_findlink(&ob_dst->greasepencil_modifiers, opop->subitem_local_index);
  }
  /* Otherwise we just insert in first position. */

  GpencilModifierData *mod_src = NULL;
  if (opop->subitem_local_name && opop->subitem_local_name[0]) {
    mod_src = BLI_findstring(
        &ob_src->greasepencil_modifiers, opop->subitem_local_name, offsetof(ModifierData, name));
  }
  if (mod_src == NULL && opop->subitem_local_index >= 0) {
    mod_src = BLI_findlink(&ob_src->greasepencil_modifiers, opop->subitem_local_index);
  }
  mod_src = mod_src ? mod_src->next : ob_src->greasepencil_modifiers.first;

  if (mod_src == NULL) {
    BLI_assert(mod_src != NULL);
    return false;
  }

  /* While it would be nicer to use lower-level BKE_modifier_new() here, this one is lacking
   * special-cases handling (particles and other physics modifiers mostly), so using the ED version
   * instead, to avoid duplicating code. */
  GpencilModifierData *mod_dst = ED_object_gpencil_modifier_add(
      NULL, bmain, NULL, ob_dst, mod_src->name, mod_src->type);

  BLI_remlink(&ob_dst->modifiers, mod_dst);
  /* This handles NULL anchor as expected by adding at head of list. */
  BLI_insertlinkafter(&ob_dst->greasepencil_modifiers, mod_anchor, mod_dst);

  //  printf("%s: We inserted a gpencil modifier '%s'...\n", __func__, mod_dst->name);
  return true;
}

/* shader fx */
static ShaderFxData *rna_Object_shaderfx_new(
    Object *object, bContext *C, ReportList *reports, const char *name, int type)
{
  return ED_object_shaderfx_add(reports, CTX_data_main(C), CTX_data_scene(C), object, name, type);
}

static void rna_Object_shaderfx_remove(Object *object,
                                       bContext *C,
                                       ReportList *reports,
                                       PointerRNA *gmd_ptr)
{
  ShaderFxData *gmd = gmd_ptr->data;
  if (ED_object_shaderfx_remove(reports, CTX_data_main(C), object, gmd) == false) {
    /* error is already set */
    return;
  }

  RNA_POINTER_INVALIDATE(gmd_ptr);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_shaderfx_clear(Object *object, bContext *C)
{
  ED_object_shaderfx_clear(CTX_data_main(C), object);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_boundbox_get(PointerRNA *ptr, float *values)
{
  Object *ob = (Object *)ptr->owner_id;
  BoundBox *bb = BKE_object_boundbox_get(ob);
  if (bb) {
    memcpy(values, bb->vec, sizeof(bb->vec));
  }
  else {
    copy_vn_fl(values, sizeof(bb->vec) / sizeof(float), 0.0f);
  }
}

static bDeformGroup *rna_Object_vgroup_new(Object *ob, Main *bmain, const char *name)
{
  bDeformGroup *defgroup = BKE_object_defgroup_add_name(ob, name);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);

  return defgroup;
}

static void rna_Object_vgroup_remove(Object *ob,
                                     Main *bmain,
                                     ReportList *reports,
                                     PointerRNA *defgroup_ptr)
{
  bDeformGroup *defgroup = defgroup_ptr->data;
  if (BLI_findindex(&ob->defbase, defgroup) == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "DeformGroup '%s' not in object '%s'",
                defgroup->name,
                ob->id.name + 2);
    return;
  }

  BKE_object_defgroup_remove(ob, defgroup);
  RNA_POINTER_INVALIDATE(defgroup_ptr);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_Object_vgroup_clear(Object *ob, Main *bmain)
{
  BKE_object_defgroup_remove_all(ob);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_VertexGroup_vertex_add(ID *id,
                                       bDeformGroup *def,
                                       ReportList *reports,
                                       int index_len,
                                       int *index,
                                       float weight,
                                       int assignmode)
{
  Object *ob = (Object *)id;

  if (BKE_object_is_in_editmode_vgroup(ob)) {
    BKE_report(
        reports, RPT_ERROR, "VertexGroup.add(): cannot be called while object is in edit mode");
    return;
  }

  while (index_len--) {
    ED_vgroup_vert_add(
        ob, def, *index++, weight, assignmode); /* XXX, not efficient calling within loop*/
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, (ID *)ob->data);
}

static void rna_VertexGroup_vertex_remove(
    ID *id, bDeformGroup *dg, ReportList *reports, int index_len, int *index)
{
  Object *ob = (Object *)id;

  if (BKE_object_is_in_editmode_vgroup(ob)) {
    BKE_report(
        reports, RPT_ERROR, "VertexGroup.remove(): cannot be called while object is in edit mode");
    return;
  }

  while (index_len--) {
    ED_vgroup_vert_remove(ob, dg, *index++);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, (ID *)ob->data);
}

static float rna_VertexGroup_weight(ID *id, bDeformGroup *dg, ReportList *reports, int index)
{
  float weight = ED_vgroup_vert_weight((Object *)id, dg, index);

  if (weight < 0) {
    BKE_report(reports, RPT_ERROR, "Vertex not in group");
  }
  return weight;
}

static bFaceMap *rna_Object_fmap_new(Object *ob, const char *name)
{
  bFaceMap *fmap = BKE_object_facemap_add_name(ob, name);

  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);

  return fmap;
}

static void rna_Object_fmap_remove(Object *ob, ReportList *reports, PointerRNA *fmap_ptr)
{
  bFaceMap *fmap = fmap_ptr->data;
  if (BLI_findindex(&ob->fmaps, fmap) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "FaceMap '%s' not in object '%s'", fmap->name, ob->id.name + 2);
    return;
  }

  BKE_object_facemap_remove(ob, fmap);
  RNA_POINTER_INVALIDATE(fmap_ptr);

  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_Object_fmap_clear(Object *ob)
{
  BKE_object_facemap_clear(ob);

  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_FaceMap_face_add(
    ID *id, bFaceMap *fmap, ReportList *reports, int index_len, int *index)
{
  Object *ob = (Object *)id;

  if (BKE_object_is_in_editmode(ob)) {
    BKE_report(reports, RPT_ERROR, "FaceMap.add(): cannot be called while object is in edit mode");
    return;
  }

  while (index_len--) {
    ED_object_facemap_face_add(ob, fmap, *index++);
  }

  WM_main_add_notifier(NC_GEOM | ND_DATA, (ID *)ob->data);
}

static void rna_FaceMap_face_remove(
    ID *id, bFaceMap *fmap, ReportList *reports, int index_len, int *index)
{
  Object *ob = (Object *)id;

  if (BKE_object_is_in_editmode(ob)) {
    BKE_report(reports, RPT_ERROR, "FaceMap.add(): cannot be called while object is in edit mode");
    return;
  }

  while (index_len--) {
    ED_object_facemap_face_remove(ob, fmap, *index++);
  }

  WM_main_add_notifier(NC_GEOM | ND_DATA, (ID *)ob->data);
}

/* generic poll functions */
bool rna_Lattice_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_LATTICE;
}

bool rna_Curve_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_CURVE;
}

bool rna_Armature_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_ARMATURE;
}

bool rna_Mesh_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_MESH;
}

bool rna_Camera_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_CAMERA;
}

bool rna_Light_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_LAMP;
}

bool rna_GPencil_object_poll(PointerRNA *UNUSED(ptr), PointerRNA value)
{
  return ((Object *)value.owner_id)->type == OB_GPENCIL;
}

int rna_Object_use_dynamic_topology_sculpting_get(PointerRNA *ptr)
{
  SculptSession *ss = ((Object *)ptr->owner_id)->sculpt;
  return (ss && ss->bm);
}

#else

static void rna_def_vertex_group(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem assign_mode_items[] = {
      {WEIGHT_REPLACE, "REPLACE", 0, "Replace", "Replace"},
      {WEIGHT_ADD, "ADD", 0, "Add", "Add"},
      {WEIGHT_SUBTRACT, "SUBTRACT", 0, "Subtract", "Subtract"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "VertexGroup", NULL);
  RNA_def_struct_sdna(srna, "bDeformGroup");
  RNA_def_struct_ui_text(
      srna, "Vertex Group", "Group of vertices, used for armature deform and other purposes");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Vertex group name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_VertexGroup_name_set");
  /* update data because modifiers may use [#24761] */
  RNA_def_property_update(
      prop, NC_GEOM | ND_DATA | NA_RENAME, "rna_Object_internal_update_data_dependency");

  prop = RNA_def_property(srna, "lock_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "", "Maintain the relative weights for the group");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", 0);
  /* update data because modifiers may use [#24761] */
  RNA_def_property_update(prop, NC_GEOM | ND_DATA | NA_RENAME, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_VertexGroup_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index number of the vertex group");

  func = RNA_def_function(srna, "add", "rna_VertexGroup_vertex_add");
  RNA_def_function_ui_description(func, "Add vertices to the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  /* TODO, see how array size of 0 works, this shouldnt be used */
  parm = RNA_def_int_array(func, "index", 1, NULL, 0, 0, "", "Index List", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);
  parm = RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight", 0.0f, 1.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", assign_mode_items, 0, "", "Vertex assign mode");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "remove", "rna_VertexGroup_vertex_remove");
  RNA_def_function_ui_description(func, "Remove a vertex from the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  /* TODO, see how array size of 0 works, this shouldnt be used */
  parm = RNA_def_int_array(func, "index", 1, NULL, 0, 0, "", "Index List", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);

  func = RNA_def_function(srna, "weight", "rna_VertexGroup_weight");
  RNA_def_function_ui_description(func, "Get a vertex weight from the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "The index of the vertex", 0, INT_MAX);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight", 0.0f, 1.0f);
  RNA_def_function_return(func, parm);
}

static void rna_def_face_map(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "FaceMap", NULL);
  RNA_def_struct_sdna(srna, "bFaceMap");
  RNA_def_struct_ui_text(
      srna, "Face Map", "Group of faces, each face can only be part of one map");
  RNA_def_struct_ui_icon(srna, ICON_MOD_TRIANGULATE);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Face map name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_FaceMap_name_set");
  /* update data because modifiers may use [#24761] */
  RNA_def_property_update(prop, NC_GEOM | ND_DATA | NA_RENAME, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
  RNA_def_property_ui_text(prop, "Select", "Face-map selection state (for tools to use)");
  /* important not to use a notifier here, creates a feedback loop! */

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_FaceMap_index_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Index", "Index number of the face map");

  func = RNA_def_function(srna, "add", "rna_FaceMap_face_add");
  RNA_def_function_ui_description(func, "Add vertices to the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  /* TODO, see how array size of 0 works, this shouldnt be used */
  parm = RNA_def_int_array(func, "index", 1, NULL, 0, 0, "", "Index List", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);

  func = RNA_def_function(srna, "remove", "rna_FaceMap_face_remove");
  RNA_def_function_ui_description(func, "Remove a vertex from the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  /* TODO, see how array size of 0 works, this shouldnt be used */
  parm = RNA_def_int_array(func, "index", 1, NULL, 0, 0, "", "Index List", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);
}

static void rna_def_material_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem link_items[] = {
      {1, "OBJECT", 0, "Object", ""},
      {0, "DATA", 0, "Data", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* NOTE: there is no MaterialSlot equivalent in DNA, so the internal
   * pointer data points to ob->mat + index, and we manually implement
   * get/set for the properties. */

  srna = RNA_def_struct(brna, "MaterialSlot", NULL);
  RNA_def_struct_ui_text(srna, "Material Slot", "Material slot in an object");
  RNA_def_struct_ui_icon(srna, ICON_MATERIAL_DATA);

  RNA_define_lib_overridable(true);

  /* WARNING! Order is crucial for override to work properly here... :/
   * 'link' must come before material pointer,
   * since it defines where (in object or obdata) that one is set! */
  prop = RNA_def_property(srna, "link", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, link_items);
  RNA_def_property_enum_funcs(
      prop, "rna_MaterialSlot_link_get", "rna_MaterialSlot_link_set", NULL);
  RNA_def_property_ui_text(prop, "Link", "Link material to object or the object's data");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_MaterialSlot_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_MaterialSlot_material_editable");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_MaterialSlot_material_get",
                                 "rna_MaterialSlot_material_set",
                                 NULL,
                                 "rna_MaterialSlot_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material data-block used by this material slot");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_MaterialSlot_update");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_MaterialSlot_name_get", "rna_MaterialSlot_name_length", NULL);
  RNA_def_property_ui_text(prop, "Name", "Material slot name");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_struct_name_property(srna, prop);

  RNA_define_lib_overridable(false);

  RNA_def_struct_path_func(srna, "rna_MaterialSlot_path");
}

static void rna_def_object_constraints(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ObjectConstraints");
  srna = RNA_def_struct(brna, "ObjectConstraints", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Object Constraints", "Collection of object constraints");

  /* Collection active property */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Constraint");
  RNA_def_property_pointer_funcs(
      prop, "rna_Object_active_constraint_get", "rna_Object_active_constraint_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Constraint", "Active Object constraint");

  /* Constraint collection */
  func = RNA_def_function(srna, "new", "rna_Object_constraints_new");
  RNA_def_function_ui_description(func, "Add a new constraint to this object");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  /* object to add */
  parm = RNA_def_enum(
      func, "type", rna_enum_constraint_type_items, 1, "", "Constraint type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "constraint", "Constraint", "", "New constraint");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Object_constraints_remove");
  RNA_def_function_ui_description(func, "Remove a constraint from this object");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  /* constraint to remove */
  parm = RNA_def_pointer(func, "constraint", "Constraint", "", "Removed constraint");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "clear", "rna_Object_constraints_clear");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove all constraint from this object");

  func = RNA_def_function(srna, "move", "rna_Object_constraints_move");
  RNA_def_function_ui_description(func, "Move a constraint to a different position");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, INT_MIN, INT_MAX, "From Index", "Index to move", 0, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_int(func, "to_index", -1, INT_MIN, INT_MAX, "To Index", "Target index", 0, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "copy", "rna_Object_constraints_copy");
  RNA_def_function_ui_description(func, "Add a new constraint that is a copy of the given one");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  /* constraint to copy */
  parm = RNA_def_pointer(func,
                         "constraint",
                         "Constraint",
                         "",
                         "Constraint to copy - may belong to a different object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
  /* return type */
  parm = RNA_def_pointer(func, "new_constraint", "Constraint", "", "New constraint");
  RNA_def_function_return(func, parm);
}

/* object.modifiers */
static void rna_def_object_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ObjectModifiers");
  srna = RNA_def_struct(brna, "ObjectModifiers", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Object Modifiers", "Collection of object modifiers");

#  if 0
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EditBone");
  RNA_def_property_pointer_sdna(prop, NULL, "act_edbone");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active EditBone", "Armatures active edit bone");
  /*RNA_def_property_update(prop, 0, "rna_Armature_act_editbone_update"); */
  RNA_def_property_pointer_funcs(prop, NULL, "rna_Armature_act_edit_bone_set", NULL, NULL);

  /* todo, redraw */
/*      RNA_def_property_collection_active(prop, prop_act); */
#  endif

  /* add modifier */
  func = RNA_def_function(srna, "new", "rna_Object_modifier_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new modifier");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* modifier to add */
  parm = RNA_def_enum(
      func, "type", rna_enum_object_modifier_type_items, 1, "", "Modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "modifier", "Modifier", "", "Newly created modifier");
  RNA_def_function_return(func, parm);

  /* remove modifier */
  func = RNA_def_function(srna, "remove", "rna_Object_modifier_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an existing modifier from the object");
  /* modifier to remove */
  parm = RNA_def_pointer(func, "modifier", "Modifier", "", "Modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear", "rna_Object_modifier_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove all modifiers from the object");
}

/* object.grease_pencil_modifiers */
static void rna_def_object_grease_pencil_modifiers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ObjectGpencilModifiers");
  srna = RNA_def_struct(brna, "ObjectGpencilModifiers", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(
      srna, "Object Grease Pencil Modifiers", "Collection of object grease pencil modifiers");

  /* add greasepencil modifier */
  func = RNA_def_function(srna, "new", "rna_Object_greasepencil_modifier_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new greasepencil_modifier");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the greasepencil_modifier");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* greasepencil_modifier to add */
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_object_greasepencil_modifier_type_items,
                      1,
                      "",
                      "Modifier type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(
      func, "greasepencil_modifier", "GpencilModifier", "", "Newly created modifier");
  RNA_def_function_return(func, parm);

  /* remove greasepencil_modifier */
  func = RNA_def_function(srna, "remove", "rna_Object_greasepencil_modifier_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func,
                                  "Remove an existing greasepencil_modifier from the object");
  /* greasepencil_modifier to remove */
  parm = RNA_def_pointer(
      func, "greasepencil_modifier", "GpencilModifier", "", "Modifier to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  /* clear all greasepencil modifiers */
  func = RNA_def_function(srna, "clear", "rna_Object_greasepencil_modifier_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove all grease pencil modifiers from the object");
}

/* object.shaderfxs */
static void rna_def_object_shaderfxs(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ObjectShaderFx");
  srna = RNA_def_struct(brna, "ObjectShaderFx", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Object Shader Effects", "Collection of object effects");

  /* add shader_fx */
  func = RNA_def_function(srna, "new", "rna_Object_shaderfx_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new shader fx");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the effect");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* shader to add */
  parm = RNA_def_enum(
      func, "type", rna_enum_object_shaderfx_type_items, 1, "", "Effect type to add");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "shader_fx", "ShaderFx", "", "Newly created effect");
  RNA_def_function_return(func, parm);

  /* remove shader_fx */
  func = RNA_def_function(srna, "remove", "rna_Object_shaderfx_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an existing effect from the object");
  /* shader to remove */
  parm = RNA_def_pointer(func, "shader_fx", "ShaderFx", "", "Effect to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  /* clear all shader fx */
  func = RNA_def_function(srna, "clear", "rna_Object_shaderfx_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove all effects from the object");
}

/* object.particle_systems */
static void rna_def_object_particle_systems(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  PropertyRNA *prop;

  /* FunctionRNA *func; */
  /* PropertyRNA *parm; */

  RNA_def_property_srna(cprop, "ParticleSystems");
  srna = RNA_def_struct(brna, "ParticleSystems", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Particle Systems", "Collection of particle systems");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_pointer_funcs(prop, "rna_Object_active_particle_system_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Active Particle System", "Active particle system being displayed");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_particle_system_index_get",
                             "rna_Object_active_particle_system_index_set",
                             "rna_Object_active_particle_system_index_range");
  RNA_def_property_ui_text(
      prop, "Active Particle System Index", "Index of active particle system slot");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_particle_update");
}

/* object.vertex_groups */
static void rna_def_object_vertex_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "VertexGroups");
  srna = RNA_def_struct(brna, "VertexGroups", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Vertex Groups", "Collection of vertex groups");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "VertexGroup");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Object_active_vertex_group_get",
                                 "rna_Object_active_vertex_group_set",
                                 NULL,
                                 NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Vertex Group", "Vertex groups of the object");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "actdef");
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_vertex_group_index_get",
                             "rna_Object_active_vertex_group_index_set",
                             "rna_Object_active_vertex_group_index_range");
  RNA_def_property_ui_text(
      prop, "Active Vertex Group Index", "Active index in vertex group array");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_internal_update_data");

  /* vertex groups */ /* add_vertex_group */
  func = RNA_def_function(srna, "new", "rna_Object_vgroup_new");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Add vertex group to object");
  RNA_def_string(func, "name", "Group", 0, "", "Vertex group name"); /* optional */
  parm = RNA_def_pointer(func, "group", "VertexGroup", "", "New vertex group");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Object_vgroup_remove");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete vertex group from object");
  parm = RNA_def_pointer(func, "group", "VertexGroup", "", "Vertex group to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

  func = RNA_def_function(srna, "clear", "rna_Object_vgroup_clear");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Delete all vertex groups from object");
}

/* object.face_maps */
static void rna_def_object_face_maps(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "FaceMaps");
  srna = RNA_def_struct(brna, "FaceMaps", NULL);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Face Maps", "Collection of face maps");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FaceMap");
  RNA_def_property_pointer_funcs(
      prop, "rna_Object_active_face_map_get", "rna_Object_active_face_map_set", NULL, NULL);
  RNA_def_property_ui_text(prop, "Active Face Map", "Face maps of the object");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_sdna(prop, NULL, "actfmap");
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_face_map_index_get",
                             "rna_Object_active_face_map_index_set",
                             "rna_Object_active_face_map_index_range");
  RNA_def_property_ui_text(prop, "Active Face Map Index", "Active index in face map array");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_internal_update_data");

  /* face maps */ /* add_face_map */
  func = RNA_def_function(srna, "new", "rna_Object_fmap_new");
  RNA_def_function_ui_description(func, "Add face map to object");
  RNA_def_string(func, "name", "Map", 0, "", "face map name"); /* optional */
  parm = RNA_def_pointer(func, "fmap", "FaceMap", "", "New face map");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Object_fmap_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete vertex group from object");
  parm = RNA_def_pointer(func, "group", "FaceMap", "", "Face map to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_property_clear_flag(parm, PROP_THICK_WRAP);

  func = RNA_def_function(srna, "clear", "rna_Object_fmap_clear");
  RNA_def_function_ui_description(func, "Delete all vertex groups from object");
}

static void rna_def_object_display(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ObjectDisplay", NULL);
  RNA_def_struct_ui_text(srna, "Object Display", "Object display settings for 3d viewport");
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_nested(brna, srna, "Object");
  RNA_def_struct_path_func(srna, "rna_ObjectDisplay_path");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "show_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "dtx", OB_DRAW_NO_SHADOW_CAST);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Shadow", "Object cast shadows in the 3d viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  RNA_define_lib_overridable(false);
}

static void rna_def_object(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem up_items[] = {
      {OB_POSX, "X", 0, "X", ""},
      {OB_POSY, "Y", 0, "Y", ""},
      {OB_POSZ, "Z", 0, "Z", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem drawtype_items[] = {
      {OB_BOUNDBOX, "BOUNDS", 0, "Bounds", "Display the bounds of the object"},
      {OB_WIRE, "WIRE", 0, "Wire", "Display the object as a wireframe"},
      {OB_SOLID,
       "SOLID",
       0,
       "Solid",
       "Display the object as a solid (if solid drawing is enabled in the viewport)"},
      {OB_TEXTURE,
       "TEXTURED",
       0,
       "Textured",
       "Display the object with textures (if textures are enabled in the viewport)"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem boundtype_items[] = {
      {OB_BOUND_BOX, "BOX", 0, "Box", "Display bounds as box"},
      {OB_BOUND_SPHERE, "SPHERE", 0, "Sphere", "Display bounds as sphere"},
      {OB_BOUND_CYLINDER, "CYLINDER", 0, "Cylinder", "Display bounds as cylinder"},
      {OB_BOUND_CONE, "CONE", 0, "Cone", "Display bounds as cone"},
      {OB_BOUND_CAPSULE, "CAPSULE", 0, "Capsule", "Display bounds as capsule"},
      {0, NULL, 0, NULL, NULL},
  };

  static int boundbox_dimsize[] = {8, 3};

  srna = RNA_def_struct(brna, "Object", "ID");
  RNA_def_struct_ui_text(srna, "Object", "Object data-block defining an object in a scene");
  RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);
  RNA_def_struct_ui_icon(srna, ICON_OBJECT_DATA);

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_Object_data_set", "rna_Object_data_typef", "rna_Object_data_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Data", "Object data");
  RNA_def_property_update(prop, 0, "rna_Object_internal_update_data_dependency");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, rna_enum_object_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Type", "Type of Object");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_object_mode_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mode", "Object interaction mode");

  /* for data access */
  prop = RNA_def_property(srna, "bound_box", PROP_FLOAT, PROP_NONE);
  RNA_def_property_multi_array(prop, 2, boundbox_dimsize);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_float_funcs(prop, "rna_Object_boundbox_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop,
      "Bounding Box",
      "Object's bounding box in object-space coordinates, all values are -1.0 when "
      "not available");

  /* parent */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_Object_parent_set", NULL, NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_Object_parent_override_apply");
  RNA_def_property_ui_text(prop, "Parent", "Parent Object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "parent_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "partype");
  RNA_def_property_enum_items(prop, parent_type_items);
  RNA_def_property_enum_funcs(
      prop, NULL, "rna_Object_parent_type_set", "rna_Object_parent_type_itemf");
  RNA_def_property_ui_text(prop, "Parent Type", "Type of parent relation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "parent_vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "par1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Parent Vertices", "Indices of vertices in case of a vertex parenting relation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "parsubstr");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_Object_parent_bone_set");
  RNA_def_property_ui_text(
      prop, "Parent Bone", "Name of parent bone in case of a bone parenting relation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  /* Track and Up flags */
  /* XXX: these have been saved here for a bit longer (after old track was removed),
   *      since some other tools still refer to this */
  prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "trackflag");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(
      prop,
      "Track Axis",
      "Axis that points in 'forward' direction (applies to InstanceFrame when "
      "parent 'Follow' is enabled)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "upflag");
  RNA_def_property_enum_items(prop, up_items);
  RNA_def_property_ui_text(
      prop,
      "Up Axis",
      "Axis that points in the upward direction (applies to InstanceFrame when "
      "parent 'Follow' is enabled)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  /* proxy */
  prop = RNA_def_property(srna, "proxy", PROP_POINTER, PROP_NONE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Proxy", "Library object this proxy object controls");

  prop = RNA_def_property(srna, "proxy_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "proxy_group");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop, "Proxy Collection", "Library collection duplicator object this proxy object controls");

  /* materials */
  prop = RNA_def_property(srna, "material_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "mat", "totcol");
  RNA_def_property_struct_type(prop, "MaterialSlot");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_PROP_NAME);
  /* don't dereference pointer! */
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, "rna_iterator_array_get", NULL, NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Material Slots", "Material slots in the object");

  prop = RNA_def_property(srna, "active_material", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Object_active_material_get",
                                 "rna_Object_active_material_set",
                                 NULL,
                                 "rna_MaterialSlot_material_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_Object_active_material_editable");
  RNA_def_property_ui_text(prop, "Active Material", "Active material being displayed");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_MaterialSlot_update");

  prop = RNA_def_property(srna, "active_material_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "actcol");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_material_index_get",
                             "rna_Object_active_material_index_set",
                             "rna_Object_active_material_index_range");
  RNA_def_property_ui_text(prop, "Active Material Index", "Index of active material slot");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, "rna_MaterialIndex_update");

  /* transform */
  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "loc");
  RNA_def_property_editable_array_func(prop, "rna_Object_location_editable");
  RNA_def_property_ui_text(prop, "Location", "Location of the object");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, NULL, "quat");
  RNA_def_property_editable_array_func(prop, "rna_Object_rotation_4d_editable");
  RNA_def_property_ui_text(prop, "Quaternion Rotation", "Rotation in Quaternions");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* XXX: for axis-angle, it would have been nice to have 2 separate fields for UI purposes, but
   * having a single one is better for Keyframing and other property-management situations...
   */
  prop = RNA_def_property(srna, "rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(
      prop, "rna_Object_rotation_axis_angle_get", "rna_Object_rotation_axis_angle_set", NULL);
  RNA_def_property_editable_array_func(prop, "rna_Object_rotation_4d_editable");
  RNA_def_property_float_array_default(prop, rna_default_axis_angle);
  RNA_def_property_ui_text(
      prop, "Axis-Angle Rotation", "Angle of Rotation for Axis-Angle rotation representation");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, NULL, "rot");
  RNA_def_property_editable_array_func(prop, "rna_Object_rotation_euler_editable");
  RNA_def_property_ui_text(prop, "Euler Rotation", "Rotation in Eulers");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "rotmode");
  RNA_def_property_enum_items(prop, rna_enum_object_rotation_mode_items);
  RNA_def_property_enum_funcs(prop, NULL, "rna_Object_rotation_mode_set", NULL);
  RNA_def_property_ui_text(prop, "Rotation Mode", "");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_editable_array_func(prop, "rna_Object_scale_editable");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Scale", "Scaling of the object");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "dimensions", PROP_FLOAT, PROP_XYZ_LENGTH);
  RNA_def_property_array(prop, 3);
  /* Only as convenient helper for py API, and conflicts with animating scale. */
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_float_funcs(
      prop, "rna_Object_dimensions_get", "rna_Object_dimensions_set", NULL);
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(
      prop,
      "Dimensions",
      "Absolute bounding box dimensions of the object (WARNING: assigning to it or "
      "its members multiple consecutive times will not work correctly, "
      "as this needs up-to-date evaluated data)");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* delta transforms */
  prop = RNA_def_property(srna, "delta_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, NULL, "dloc");
  RNA_def_property_ui_text(
      prop, "Delta Location", "Extra translation added to the location of the object");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "delta_rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, NULL, "drot");
  RNA_def_property_ui_text(
      prop,
      "Delta Rotation (Euler)",
      "Extra rotation added to the rotation of the object (when using Euler rotations)");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "delta_rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, NULL, "dquat");
  RNA_def_property_ui_text(
      prop,
      "Delta Rotation (Quaternion)",
      "Extra rotation added to the rotation of the object (when using Quaternion rotations)");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

#  if 0 /* XXX not supported well yet... */
  prop = RNA_def_property(srna, "delta_rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
  /* FIXME: this is not a single field any more! (drotAxis and drotAngle) */
  RNA_def_property_float_sdna(prop, NULL, "dquat");
  RNA_def_property_float_array_default(prop, rna_default_axis_angle);
  RNA_def_property_ui_text(
      prop,
      "Delta Rotation (Axis Angle)",
      "Extra rotation added to the rotation of the object (when using Axis-Angle rotations)");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");
#  endif

  prop = RNA_def_property(srna, "delta_scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, NULL, "dscale");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Delta Scale", "Extra scaling added to the scale of the object");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* transform locks */
  prop = RNA_def_property(srna, "lock_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_LOCX);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Lock Location", "Lock editing of location when transforming");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROTX);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Lock Rotation", "Lock editing of rotation when transforming");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* XXX this is sub-optimal - it really should be included above,
   *     but due to technical reasons we can't do this! */
  prop = RNA_def_property(srna, "lock_rotation_w", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROTW);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop,
      "Lock Rotation (4D Angle)",
      "Lock editing of 'angle' component of four-component rotations when transforming");
  /* XXX this needs a better name */
  prop = RNA_def_property(srna, "lock_rotations_4d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_ROT4D);
  RNA_def_property_ui_text(
      prop,
      "Lock Rotations (4D)",
      "Lock editing of four component rotations by components (instead of as Eulers)");

  prop = RNA_def_property(srna, "lock_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "protectflag", OB_LOCK_SCALEX);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Lock Scale", "Lock editing of scale when transforming");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* matrix */
  prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "obmat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Matrix World", "Worldspace transformation matrix");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_matrix_world_update");

  prop = RNA_def_property(srna, "matrix_local", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(
      prop,
      "Local Matrix",
      "Parent relative transformation matrix - "
      "WARNING: Only takes into account 'Object' parenting, so e.g. in case of bone parenting "
      "you get a matrix relative to the Armature object, not to the actual parent bone");
  RNA_def_property_float_funcs(
      prop, "rna_Object_matrix_local_get", "rna_Object_matrix_local_set", NULL);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "matrix_basis", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop,
                           "Input Matrix",
                           "Matrix access to location, rotation and scale (including deltas), "
                           "before constraints and parenting are applied");
  RNA_def_property_float_funcs(
      prop, "rna_Object_matrix_basis_get", "rna_Object_matrix_basis_set", NULL);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /*parent_inverse*/
  prop = RNA_def_property(srna, "matrix_parent_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "parentinv");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Parent Inverse Matrix", "Inverse of object's parent matrix at time of parenting");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* modifiers */
  prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Modifier");
  RNA_def_property_ui_text(
      prop, "Modifiers", "Modifiers affecting the geometric data of the object");
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_Object_modifiers_override_apply");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_LIBRARY_INSERTION);
  rna_def_object_modifiers(brna, prop);

  /* Grease Pencil modifiers. */
  prop = RNA_def_property(srna, "grease_pencil_modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "greasepencil_modifiers", NULL);
  RNA_def_property_struct_type(prop, "GpencilModifier");
  RNA_def_property_ui_text(
      prop, "Grease Pencil Modifiers", "Modifiers affecting the data of the grease pencil object");
  RNA_def_property_override_funcs(
      prop, NULL, NULL, "rna_Object_greasepencil_modifiers_override_apply");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_LIBRARY_INSERTION);
  rna_def_object_grease_pencil_modifiers(brna, prop);

  /* Shader FX. */
  prop = RNA_def_property(srna, "shader_effects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "shader_fx", NULL);
  RNA_def_property_struct_type(prop, "ShaderFx");
  RNA_def_property_ui_text(prop, "Shader Effects", "Effects affecting display of object");
  RNA_define_lib_overridable(false);
  rna_def_object_shaderfxs(brna, prop);
  RNA_define_lib_overridable(true);

  /* constraints */
  prop = RNA_def_property(srna, "constraints", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Constraint");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_LIBRARY_INSERTION);
  RNA_def_property_ui_text(
      prop, "Constraints", "Constraints affecting the transformation of the object");
  RNA_def_property_override_funcs(prop, NULL, NULL, "rna_Object_constraints_override_apply");
#  if 0
  RNA_def_property_collection_funcs(
      prop, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "constraints__add", "constraints__remove");
#  endif
  rna_def_object_constraints(brna, prop);

  /* vertex groups */
  prop = RNA_def_property(srna, "vertex_groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "defbase", NULL);
  RNA_def_property_struct_type(prop, "VertexGroup");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Vertex Groups", "Vertex groups of the object");
  rna_def_object_vertex_groups(brna, prop);

  /* face maps */
  prop = RNA_def_property(srna, "face_maps", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "fmaps", NULL);
  RNA_def_property_struct_type(prop, "FaceMap");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Face Maps", "Maps of faces of the object");
  rna_def_object_face_maps(brna, prop);

  /* empty */
  prop = RNA_def_property(srna, "empty_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "empty_drawtype");
  RNA_def_property_enum_items(prop, rna_enum_object_empty_drawtype_items);
  RNA_def_property_enum_funcs(prop, NULL, "rna_Object_empty_display_type_set", NULL);
  RNA_def_property_ui_text(prop, "Empty Display Type", "Viewport display style for empties");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "empty_display_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, NULL, "empty_drawsize");
  RNA_def_property_range(prop, 0.0001f, 1000.0f);
  RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
  RNA_def_property_ui_text(
      prop, "Empty Display Size", "Size of display for empties in the viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "empty_image_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "ima_ofs");
  RNA_def_property_ui_text(prop, "Origin Offset", "Origin offset distance");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 0.1f, 2);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "iuser");
  RNA_def_property_ui_text(
      prop,
      "Image User",
      "Parameters defining which layer, pass and frame of the image is displayed");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "empty_image_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_object_empty_image_depth_items);
  RNA_def_property_ui_text(
      prop, "Empty Image Depth", "Determine which other objects will occlude the image");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_empty_image_perspective", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "empty_image_visibility_flag", OB_EMPTY_IMAGE_HIDE_PERSPECTIVE);
  RNA_def_property_ui_text(
      prop, "Display in Perspective Mode", "Display image in perspective mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_empty_image_orthographic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "empty_image_visibility_flag", OB_EMPTY_IMAGE_HIDE_ORTHOGRAPHIC);
  RNA_def_property_ui_text(
      prop, "Display in Orthographic Mode", "Display image in orthographic mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_empty_image_only_axis_aligned", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "empty_image_visibility_flag", OB_EMPTY_IMAGE_HIDE_NON_AXIS_ALIGNED);
  RNA_def_property_ui_text(prop,
                           "Display Only Axis Aligned",
                           "Only display the image when it is aligned with the view axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "use_empty_image_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "empty_image_flag", OB_EMPTY_IMAGE_USE_ALPHA_BLEND);
  RNA_def_property_ui_text(
      prop,
      "Use Alpha",
      "Use alpha blending instead of alpha test (can produce sorting artifacts)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  static EnumPropertyItem prop_empty_image_side_items[] = {
      {0, "DOUBLE_SIDED", 0, "Both", ""},
      {OB_EMPTY_IMAGE_HIDE_BACK, "FRONT", 0, "Front", ""},
      {OB_EMPTY_IMAGE_HIDE_FRONT, "BACK", 0, "Back", ""},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(srna, "empty_image_side", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "empty_image_visibility_flag");
  RNA_def_property_enum_items(prop, prop_empty_image_side_items);
  RNA_def_property_ui_text(prop, "Empty Image Side", "Show front/back side");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  /* render */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "index");
  RNA_def_property_ui_text(
      prop, "Pass Index", "Index number for the \"Object Index\" render pass");
  RNA_def_property_update(prop, NC_OBJECT, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_ui_text(
      prop, "Color", "Object color and alpha, used when faces have the ObColor mode enabled");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  /* physics */
  prop = RNA_def_property(srna, "field", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "pd");
  RNA_def_property_struct_type(prop, "FieldSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Object_field_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Field Settings", "Settings for using the object as a field in physics simulation");

  prop = RNA_def_property(srna, "collision", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "pd");
  RNA_def_property_struct_type(prop, "CollisionSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Object_collision_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop,
                           "Collision Settings",
                           "Settings for using the object as a collider in physics simulation");

  prop = RNA_def_property(srna, "soft_body", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "soft");
  RNA_def_property_struct_type(prop, "SoftBodySettings");
  RNA_def_property_ui_text(prop, "Soft Body Settings", "Settings for soft body simulation");

  prop = RNA_def_property(srna, "particle_systems", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "particlesystem", NULL);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_ui_text(prop, "Particle Systems", "Particle systems emitted from the object");
  rna_def_object_particle_systems(brna, prop);

  prop = RNA_def_property(srna, "rigid_body", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "rigidbody_object");
  RNA_def_property_struct_type(prop, "RigidBodyObject");
  RNA_def_property_ui_text(prop, "Rigid Body Settings", "Settings for rigid body simulation");

  prop = RNA_def_property(srna, "rigid_body_constraint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "rigidbody_constraint");
  RNA_def_property_struct_type(prop, "RigidBodyConstraint");
  RNA_def_property_ui_text(prop, "Rigid Body Constraint", "Constraint constraining rigid bodies");

  /* restrict */
  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", OB_RESTRICT_VIEWPORT);
  RNA_def_property_ui_text(prop, "Disable in Viewports", "Globally disable in viewports");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, -1);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", OB_RESTRICT_SELECT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Disable Selection", "Disable selection in viewport");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, -1);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", OB_RESTRICT_RENDER);
  RNA_def_property_ui_text(prop, "Disable in Renders", "Globally disable in renders");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, -1);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "show_instancer_for_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "duplicator_visibility_flag", OB_DUPLI_FLAG_RENDER);
  RNA_def_property_ui_text(prop, "Render Instancer", "Make instancer visible when rendering");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_DRAW, "rna_Object_duplicator_visibility_flag_update");

  prop = RNA_def_property(srna, "show_instancer_for_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "duplicator_visibility_flag", OB_DUPLI_FLAG_VIEWPORT);
  RNA_def_property_ui_text(prop, "Display Instancer", "Make instancer visible in the viewport");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_DRAW, "rna_Object_duplicator_visibility_flag_update");

  /* instancing */
  prop = RNA_def_property(srna, "instance_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "transflag");
  RNA_def_property_enum_items(prop, instance_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_Object_instance_type_itemf");
  RNA_def_property_ui_text(prop, "Instance Type", "If not None, object instancing method to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "use_instance_vertices_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transflag", OB_DUPLIROT);
  RNA_def_property_ui_text(
      prop, "Orient with Normals", "Rotate instance according to vertex normal");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "use_instance_faces_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transflag", OB_DUPLIFACES_SCALE);
  RNA_def_property_ui_text(prop, "Scale to Face Sizes", "Scale instance based on face size");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "instance_faces_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "instance_faces_scale");
  RNA_def_property_range(prop, 0.001f, 10000.0f);
  RNA_def_property_ui_text(prop, "Instance Faces Scale", "Scale the face instance objects");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "instance_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_pointer_sdna(prop, NULL, "instance_collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_Object_dup_collection_set", NULL, NULL);
  RNA_def_property_ui_text(prop, "Instance Collection", "Instance an existing collection");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "is_instancer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transflag", OB_DUPLI);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* drawing */
  prop = RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "dt");
  RNA_def_property_enum_items(prop, drawtype_items);
  RNA_def_property_ui_text(prop, "Display As", "How to display object in viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "show_bounds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWBOUNDOX);
  RNA_def_property_ui_text(prop, "Display Bounds", "Display the object's bounds");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "display_bounds_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "boundtype");
  RNA_def_property_enum_items(prop, boundtype_items);
  RNA_def_property_ui_text(prop, "Display Bounds Type", "Object boundary display type");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWNAME);
  RNA_def_property_ui_text(prop, "Display Name", "Display the object's name");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_AXIS);
  RNA_def_property_ui_text(prop, "Display Axes", "Display the object's origin and axes");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_texture_space", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_TEXSPACE);
  RNA_def_property_ui_text(prop, "Display Texture Space", "Display the object's texture space");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWWIRE);
  RNA_def_property_ui_text(prop, "Display Wire", "Add the object's wireframe over solid drawing");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_all_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAW_ALL_EDGES);
  RNA_def_property_ui_text(prop, "Display All Edges", "Display all edges for mesh objects");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "use_grease_pencil_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_USE_GPENCIL_LIGHTS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Use Lights", "Lights affect grease pencil object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_transparent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWTRANSP);
  RNA_def_property_ui_text(
      prop, "Display Transparent", "Display material transparency in the object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, NULL);

  prop = RNA_def_property(srna, "show_in_front", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dtx", OB_DRAWXRAY);
  RNA_def_property_ui_text(prop, "In Front", "Make the object draw in front of others");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_GPencil_update");

  /* pose */
  prop = RNA_def_property(srna, "pose_library", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "poselib");
  RNA_def_property_struct_type(prop, "Action");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Pose Library", "Action used as a pose library for armatures");

  prop = RNA_def_property(srna, "pose", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "pose");
  RNA_def_property_struct_type(prop, "Pose");
  RNA_def_property_ui_text(prop, "Pose", "Current pose for armatures");

  /* shape keys */
  prop = RNA_def_property(srna, "show_only_shape_key", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shapeflag", OB_SHAPE_LOCK);
  RNA_def_property_ui_text(
      prop, "Shape Key Lock", "Always show the current Shape for this Object");
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "use_shape_key_edit_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shapeflag", OB_SHAPE_EDIT_MODE);
  RNA_def_property_ui_text(
      prop, "Shape Key Edit Mode", "Apply shape keys in edit mode (for Meshes only)");
  RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);
  RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "active_shape_key", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ShapeKey");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE | PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(prop, "rna_Object_active_shape_key_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Active Shape Key", "Current shape key");

  prop = RNA_def_property(srna, "active_shape_key_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "shapenr");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* XXX this is really unpredictable... */
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_shape_key_index_get",
                             "rna_Object_active_shape_key_index_set",
                             "rna_Object_active_shape_key_index_range");
  RNA_def_property_ui_text(prop, "Active Shape Key Index", "Current shape key index");
  RNA_def_property_update(prop, 0, "rna_Object_active_shape_update");

  /* sculpt */
  prop = RNA_def_property(srna, "use_dynamic_topology_sculpting", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Object_use_dynamic_topology_sculpting_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Dynamic Topology Sculpting", NULL);

  /* Base Settings */
  prop = RNA_def_property(srna, "is_from_instancer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "base_flag", BASE_FROM_DUPLI);
  RNA_def_property_ui_text(prop, "Base from Instancer", "Object comes from a instancer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "is_from_set", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "base_flag", BASE_FROM_SET);
  RNA_def_property_ui_text(prop, "Base from Set", "Object comes from a background set");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* Object Display */
  prop = RNA_def_property(srna, "display", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ObjectDisplay");
  RNA_def_property_pointer_funcs(prop, "rna_Object_display_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Object Display", "Object display settings for 3d viewport");

  RNA_define_lib_overridable(false);

  /* anim */
  rna_def_animdata_common(srna);

  rna_def_animviz_common(srna);
  rna_def_motionpath_common(srna);

  RNA_api_object(srna);
}

void RNA_def_object(BlenderRNA *brna)
{
  rna_def_object(brna);

  RNA_define_animate_sdna(false);
  rna_def_vertex_group(brna);
  rna_def_face_map(brna);
  rna_def_material_slot(brna);
  rna_def_object_display(brna);
  RNA_define_animate_sdna(true);
}

#endif

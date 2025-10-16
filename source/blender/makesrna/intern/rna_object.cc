/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "DNA_action_types.h"
#include "DNA_layer_types.h"
#include "DNA_lightprobe_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"

#include "BLI_math_rotation.h"

#include "BLT_translation.hh"

#include "BKE_paint.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#include "ED_object_vgroup.hh"

#include "WM_api.hh"
#include "WM_types.hh"

const EnumPropertyItem rna_enum_object_mode_items[] = {
    {OB_MODE_OBJECT, "OBJECT", ICON_OBJECT_DATAMODE, "Object Mode", ""},
    {OB_MODE_EDIT, "EDIT", ICON_EDITMODE_HLT, "Edit Mode", ""},
    {OB_MODE_POSE, "POSE", ICON_POSE_HLT, "Pose Mode", ""},
    {OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt Mode", ""},
    {OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
    {OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
    {OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
    {OB_MODE_PARTICLE_EDIT, "PARTICLE_EDIT", ICON_PARTICLEMODE, "Particle Edit", ""},
    {OB_MODE_EDIT_GPENCIL_LEGACY,
     "EDIT_GPENCIL",
     ICON_EDITMODE_HLT,
     "Edit Mode",
     "Edit Grease Pencil Strokes"},
    {OB_MODE_SCULPT_GREASE_PENCIL,
     "SCULPT_GREASE_PENCIL",
     ICON_SCULPTMODE_HLT,
     "Sculpt Mode",
     "Sculpt Grease Pencil Strokes"},
    {OB_MODE_PAINT_GREASE_PENCIL,
     "PAINT_GREASE_PENCIL",
     ICON_GREASEPENCIL,
     "Draw Mode",
     "Paint Grease Pencil Strokes"},
    {OB_MODE_WEIGHT_GREASE_PENCIL,
     "WEIGHT_GREASE_PENCIL",
     ICON_WPAINT_HLT,
     "Weight Paint",
     "Grease Pencil Weight Paint Strokes"},
    {OB_MODE_VERTEX_GREASE_PENCIL,
     "VERTEX_GREASE_PENCIL",
     ICON_VPAINT_HLT,
     "Vertex Paint",
     "Grease Pencil Vertex Paint Strokes"},
    {OB_MODE_SCULPT_CURVES, "SCULPT_CURVES", ICON_SCULPTMODE_HLT, "Sculpt Mode", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_workspace_object_mode_items[] = {
    {OB_MODE_OBJECT, "OBJECT", ICON_OBJECT_DATAMODE, "Object Mode", ""},
    {OB_MODE_EDIT, "EDIT", ICON_EDITMODE_HLT, "Edit Mode", ""},
    {OB_MODE_POSE, "POSE", ICON_POSE_HLT, "Pose Mode", ""},
    {OB_MODE_SCULPT, "SCULPT", ICON_SCULPTMODE_HLT, "Sculpt Mode", ""},
    {OB_MODE_VERTEX_PAINT, "VERTEX_PAINT", ICON_VPAINT_HLT, "Vertex Paint", ""},
    {OB_MODE_WEIGHT_PAINT, "WEIGHT_PAINT", ICON_WPAINT_HLT, "Weight Paint", ""},
    {OB_MODE_TEXTURE_PAINT, "TEXTURE_PAINT", ICON_TPAINT_HLT, "Texture Paint", ""},
    {OB_MODE_PARTICLE_EDIT, "PARTICLE_EDIT", ICON_PARTICLEMODE, "Particle Edit", ""},
    {OB_MODE_EDIT_GPENCIL_LEGACY,
     "EDIT_GPENCIL",
     ICON_EDITMODE_HLT,
     "Grease Pencil Edit Mode",
     "Edit Grease Pencil Strokes"},
    {OB_MODE_SCULPT_GREASE_PENCIL,
     "SCULPT_GREASE_PENCIL",
     ICON_SCULPTMODE_HLT,
     "Grease Pencil Sculpt Mode",
     "Sculpt Grease Pencil Strokes"},
    {OB_MODE_PAINT_GREASE_PENCIL,
     "PAINT_GREASE_PENCIL",
     ICON_GREASEPENCIL,
     "Grease Pencil Draw",
     "Paint Grease Pencil Strokes"},
    {OB_MODE_VERTEX_GREASE_PENCIL,
     "VERTEX_GREASE_PENCIL",
     ICON_VPAINT_HLT,
     "Grease Pencil Vertex Paint",
     "Grease Pencil Vertex Paint Strokes"},
    {OB_MODE_WEIGHT_GREASE_PENCIL,
     "WEIGHT_GREASE_PENCIL",
     ICON_WPAINT_HLT,
     "Grease Pencil Weight Paint",
     "Grease Pencil Weight Paint Strokes"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_object_empty_image_depth_items[] = {
    {OB_EMPTY_IMAGE_DEPTH_DEFAULT, "DEFAULT", 0, "Default", ""},
    {OB_EMPTY_IMAGE_DEPTH_FRONT, "FRONT", 0, "Front", ""},
    {OB_EMPTY_IMAGE_DEPTH_BACK, "BACK", 0, "Back", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_object_gpencil_type_items[] = {
    {GP_EMPTY, "EMPTY", ICON_EMPTY_AXIS, "Blank", "Create an empty Grease Pencil object"},
    {GP_STROKE, "STROKE", ICON_STROKE, "Stroke", "Create a simple stroke with basic colors"},
    {GP_MONKEY, "MONKEY", ICON_MONKEY, "Monkey", "Construct a Suzanne Grease Pencil object"},
    RNA_ENUM_ITEM_SEPR,
    {GREASE_PENCIL_LINEART_SCENE,
     "LINEART_SCENE",
     ICON_SCENE_DATA,
     "Scene Line Art",
     "Quickly set up Line Art for the entire scene"},
    {GREASE_PENCIL_LINEART_COLLECTION,
     "LINEART_COLLECTION",
     ICON_OUTLINER_COLLECTION,
     "Collection Line Art",
     "Quickly set up Line Art for the active collection"},
    {GREASE_PENCIL_LINEART_OBJECT,
     "LINEART_OBJECT",
     ICON_OBJECT_DATA,
     "Object Line Art",
     "Quickly set up Line Art for the active object"},
    {0, nullptr, 0, nullptr, nullptr}};

static const EnumPropertyItem parent_type_items[] = {
    {PAROBJECT, "OBJECT", 0, "Object", "The object is parented to an object"},
    {PARSKEL, "ARMATURE", 0, "Armature", ""},
    /* PARSKEL reuse will give issues. */
    {PARSKEL, "LATTICE", 0, "Lattice", "The object is parented to a lattice"},
    {PARVERT1, "VERTEX", 0, "Vertex", "The object is parented to a vertex"},
    {PARVERT3, "VERTEX_3", 0, "3 Vertices", ""},
    {PARBONE, "BONE", 0, "Bone", "The object is parented to a bone"},
    {0, nullptr, 0, nullptr, nullptr},
};

#define INSTANCE_ITEMS_SHARED \
  {0, "NONE", 0, "None", ""}, \
      {OB_DUPLIVERTS, "VERTS", 0, "Vertices", "Instantiate child objects on all vertices"}, \
      {OB_DUPLIFACES, "FACES", 0, "Faces", "Instantiate child objects on all faces"}

#define INSTANCE_ITEM_COLLECTION \
  {OB_DUPLICOLLECTION, "COLLECTION", 0, "Collection", "Enable collection instancing"}
static const EnumPropertyItem instance_items[] = {
    INSTANCE_ITEMS_SHARED,
    INSTANCE_ITEM_COLLECTION,
    {0, nullptr, 0, nullptr, nullptr},
};
#ifdef RNA_RUNTIME
static EnumPropertyItem instance_items_nogroup[] = {
    INSTANCE_ITEMS_SHARED,
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem instance_items_empty[] = {
    {0, "NONE", 0, "None", ""},
    INSTANCE_ITEM_COLLECTION,
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem instance_items_font[] = {
    {0, "NONE", 0, "None", ""},
    {OB_DUPLIVERTS, "VERTS", 0, "Vertices", "Use Object Font on characters"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_lightprobes_type_items[] = {
    {LIGHTPROBE_TYPE_SPHERE, "SPHERE", ICON_LIGHTPROBE_SPHERE, "Sphere", ""},
    {LIGHTPROBE_TYPE_PLANE, "PLANE", ICON_LIGHTPROBE_PLANE, "Plane", ""},
    {LIGHTPROBE_TYPE_VOLUME, "VOLUME", ICON_LIGHTPROBE_VOLUME, "Volume", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* used for 2 enums */
#define OBTYPE_CU_CURVE {OB_CURVES_LEGACY, "CURVE", ICON_OUTLINER_OB_CURVE, "Curve", ""}
#define OBTYPE_CU_SURF {OB_SURF, "SURFACE", ICON_OUTLINER_OB_SURFACE, "Surface", ""}
#define OBTYPE_CU_FONT {OB_FONT, "FONT", ICON_OUTLINER_OB_FONT, "Text", ""}

const EnumPropertyItem rna_enum_object_type_items[] = {
    {OB_MESH, "MESH", ICON_OUTLINER_OB_MESH, "Mesh", ""},
    OBTYPE_CU_CURVE,
    OBTYPE_CU_SURF,
    {OB_MBALL, "META", ICON_OUTLINER_OB_META, "Metaball", ""},
    OBTYPE_CU_FONT,
    {OB_CURVES, "CURVES", ICON_OUTLINER_OB_CURVES, "Hair Curves", ""},
    {OB_POINTCLOUD, "POINTCLOUD", ICON_OUTLINER_OB_POINTCLOUD, "Point Cloud", ""},
    {OB_VOLUME, "VOLUME", ICON_OUTLINER_OB_VOLUME, "Volume", ""},
    {OB_GREASE_PENCIL, "GREASEPENCIL", ICON_OUTLINER_OB_GREASEPENCIL, "Grease Pencil", ""},
    RNA_ENUM_ITEM_SEPR,
    {OB_ARMATURE, "ARMATURE", ICON_OUTLINER_OB_ARMATURE, "Armature", ""},
    {OB_LATTICE, "LATTICE", ICON_OUTLINER_OB_LATTICE, "Lattice", ""},
    RNA_ENUM_ITEM_SEPR,
    {OB_EMPTY, "EMPTY", ICON_OUTLINER_OB_EMPTY, "Empty", ""},
    RNA_ENUM_ITEM_SEPR,
    {OB_LAMP, "LIGHT", ICON_OUTLINER_OB_LIGHT, "Light", ""},
    {OB_LIGHTPROBE, "LIGHT_PROBE", ICON_OUTLINER_OB_LIGHTPROBE, "Light Probe", ""},
    RNA_ENUM_ITEM_SEPR,
    {OB_CAMERA, "CAMERA", ICON_OUTLINER_OB_CAMERA, "Camera", ""},
    RNA_ENUM_ITEM_SEPR,
    {OB_SPEAKER, "SPEAKER", ICON_OUTLINER_OB_SPEAKER, "Speaker", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_object_type_curve_items[] = {
    OBTYPE_CU_CURVE,
    OBTYPE_CU_SURF,
    OBTYPE_CU_FONT,
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_object_axis_items[] = {
    {OB_POSX, "POS_X", 0, "+X", ""},
    {OB_POSY, "POS_Y", 0, "+Y", ""},
    {OB_POSZ, "POS_Z", 0, "+Z", ""},
    {OB_NEGX, "NEG_X", 0, "-X", ""},
    {OB_NEGY, "NEG_Y", 0, "-Y", ""},
    {OB_NEGZ, "NEG_Z", 0, "-Z", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include <algorithm>

#  include <fmt/format.h>

#  include "BLI_bounds.hh"

#  include "DNA_ID.h"
#  include "DNA_constraint_types.h"
#  include "DNA_gpencil_legacy_types.h"
#  include "DNA_grease_pencil_types.h"
#  include "DNA_key_types.h"
#  include "DNA_lattice_types.h"
#  include "DNA_material_types.h"
#  include "DNA_node_types.h"

#  include "BLI_math_matrix.h"
#  include "BLI_math_vector.h"

#  include "BKE_armature.hh"
#  include "BKE_brush.hh"
#  include "BKE_camera.h"
#  include "BKE_collection.hh"
#  include "BKE_constraint.h"
#  include "BKE_context.hh"
#  include "BKE_curve.hh"
#  include "BKE_deform.hh"
#  include "BKE_editlattice.h"
#  include "BKE_editmesh.hh"
#  include "BKE_effect.h"
#  include "BKE_global.hh"
#  include "BKE_key.hh"
#  include "BKE_layer.hh"
#  include "BKE_library.hh"
#  include "BKE_light_linking.h"
#  include "BKE_material.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_wrapper.hh"
#  include "BKE_modifier.hh"
#  include "BKE_object.hh"
#  include "BKE_object_deform.h"
#  include "BKE_particle.h"
#  include "BKE_scene.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "ED_curve.hh"
#  include "ED_lattice.hh"
#  include "ED_mesh.hh"
#  include "ED_object.hh"
#  include "ED_particle.hh"

#  include "DEG_depsgraph_query.hh"

static void rna_Object_internal_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_TRANSFORM);
}

static void rna_Object_internal_update_draw(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_SHADING);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ptr->owner_id);
}

static void rna_Object_matrix_world_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* Don't use compatibility so we get predictable rotation. */
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  BKE_object_apply_mat4(ob, ob->object_to_world().ptr(), false, true);
  rna_Object_internal_update(bmain, scene, ptr);
}

static void rna_Object_hide_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  BKE_main_collection_sync_remap(bmain);
  DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &ob->id);
}

static void rna_Object_duplicator_visibility_flag_update(Main * /*bmain*/,
                                                         Scene * /*scene*/,
                                                         PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
}

static void rna_grease_pencil_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (ob && ob->type == OB_GREASE_PENCIL) {
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob->data);
    DEG_id_tag_update(&grease_pencil->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, nullptr);
  }
}

static void rna_Object_matrix_world_get(PointerRNA *ptr, float *values)
{
  Object *ob = static_cast<Object *>(ptr->data);
  std::copy_n(ob->object_to_world().base_ptr(), 16, values);
}

static void rna_Object_matrix_world_set(PointerRNA *ptr, const float *values)
{
  Object *ob = static_cast<Object *>(ptr->data);
  ob->runtime->object_to_world = blender::float4x4(values);
}

static void rna_Object_matrix_local_get(PointerRNA *ptr, float values[16])
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  BKE_object_matrix_local_get(ob, (float (*)[4])values);
}

static void rna_Object_matrix_local_set(PointerRNA *ptr, const float values[16])
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  float local_mat[4][4];

  /* Local-space matrix is truly relative to the parent,
   * but parameters stored in object are relative to parentinv matrix.
   * Undo the parent inverse part before applying it as local matrix. */
  if (ob->parent) {
    float invmat[4][4];
    invert_m4_m4(invmat, ob->parentinv);
    mul_m4_m4m4(local_mat, invmat, (float (*)[4])values);
  }
  else {
    copy_m4_m4(local_mat, (float (*)[4])values);
  }

  /* Don't use compatible so we get predictable rotation, and do not use parenting either,
   * because it's a local matrix! */
  BKE_object_apply_mat4(ob, local_mat, false, false);
}

static void rna_Object_matrix_basis_get(PointerRNA *ptr, float values[16])
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  BKE_object_to_mat4(ob, (float (*)[4])values);
}

static void rna_Object_matrix_basis_set(PointerRNA *ptr, const float values[16])
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  BKE_object_apply_mat4(ob, (float (*)[4])values, false, false);
}

void rna_Object_internal_update_data_impl(PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ptr->owner_id);
}

void rna_Object_internal_update_data(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  rna_Object_internal_update_data_impl(ptr);
}

void rna_Object_internal_update_data_dependency(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_relations_tag_update(bmain);
  rna_Object_internal_update_data_impl(ptr);
}

static void rna_Object_active_shape_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  if (BKE_object_is_in_editmode(ob)) {
    /* exit/enter editmode to get new shape */
    switch (ob->type) {
      case OB_MESH: {
        Mesh *mesh = static_cast<Mesh *>(ob->data);
        BMEditMesh *em = mesh->runtime->edit_mesh.get();
        int select_mode = em->selectmode;
        EDBM_mesh_load(bmain, ob);
        EDBM_mesh_make(ob, select_mode, true);
        em = mesh->runtime->edit_mesh.get();

        DEG_id_tag_update(&mesh->id, 0);

        BKE_editmesh_looptris_and_normals_calc(em);
        break;
      }
      case OB_CURVES_LEGACY:
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

static void rna_Object_dependency_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_TRANSFORM);
  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_PARENT, ptr->owner_id);
}

void rna_Object_data_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Object_internal_update_data_dependency(bmain, scene, ptr);
}

static PointerRNA rna_Object_data_get(PointerRNA *ptr)
{
  Object *ob = static_cast<Object *>(ptr->data);
  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    mesh = BKE_mesh_wrapper_ensure_subdivision(mesh);
    return RNA_id_pointer_create(reinterpret_cast<ID *>(mesh));
  }
  return RNA_id_pointer_create(reinterpret_cast<ID *>(ob->data));
}

static void rna_Object_data_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  Object *ob = static_cast<Object *>(ptr->data);
  ID *id = static_cast<ID *>(value.data);

  if (ob->mode & OB_MODE_EDIT) {
    return;
  }

  /* assigning nullptr only for empties */
  if ((id == nullptr) && (ob->type != OB_EMPTY)) {
    return;
  }

  if (id && ((id->tag & ID_TAG_NO_MAIN) != (ob->id.tag & ID_TAG_NO_MAIN))) {
    BKE_report(reports,
               RPT_ERROR,
               "Can only assign evaluated data to evaluated object, or original data to "
               "original object");
    return;
  }

  if (ob->type == OB_EMPTY) {
    if (ob->data) {
      id_us_min(static_cast<ID *>(ob->data));
      ob->data = nullptr;
    }

    if (!id || GS(id->name) == ID_IM) {
      id_us_plus(id);
      ob->data = id;
    }
  }
  else if (ob->type == OB_MESH) {
    BKE_mesh_assign_object(G_MAIN, ob, reinterpret_cast<Mesh *>(id));
  }
  else {
    if (ob->data) {
      id_us_min(static_cast<ID *>(ob->data));
    }

    /* no need to type-check here ID. this is done in the _typef() function */
    BLI_assert(OB_DATA_SUPPORT_ID(GS(id->name)));
    id_us_plus(id);

    ob->data = id;
    BKE_object_materials_sync_length(G_MAIN, ob, id);

    if (GS(id->name) == ID_CU_LEGACY) {
      BKE_curve_type_test(ob, true);
    }
    else if (ob->type == OB_ARMATURE) {
      BKE_pose_rebuild(G_MAIN, ob, static_cast<bArmature *>(ob->data), true);
    }
  }
}

static StructRNA *rna_Object_data_typef(PointerRNA *ptr)
{
  Object *ob = static_cast<Object *>(ptr->data);

  /* keep in sync with OB_DATA_SUPPORT_ID() macro */
  switch (ob->type) {
    case OB_EMPTY:
      return &RNA_Image;
    case OB_MESH:
      return &RNA_Mesh;
    case OB_CURVES_LEGACY:
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
    case OB_GPENCIL_LEGACY:
      return &RNA_Annotation;
    case OB_GREASE_PENCIL:
      return &RNA_GreasePencil;
    case OB_CURVES:
      return &RNA_Curves;
    case OB_POINTCLOUD:
      return &RNA_PointCloud;
    case OB_VOLUME:
      return &RNA_Volume;
    default:
      return &RNA_ID;
  }
}

static void rna_Object_parent_set(PointerRNA *ptr, PointerRNA value, ReportList * /*reports*/)
{
  Object *ob = static_cast<Object *>(ptr->data);
  Object *par = static_cast<Object *>(value.data);

  {
    blender::ed::object::parent_set(ob, par, ob->partype, ob->parsubstr);
  }
}

static bool rna_Object_parent_override_apply(Main *bmain,
                                             RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  const int len_dst = rnaapply_ctx.len_src;
  const int len_src = rnaapply_ctx.len_src;
  const int len_storage = rnaapply_ctx.len_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert(opop->operation == LIBOVERRIDE_OP_REPLACE &&
             "Unsupported RNA override operation on object parent pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* We need a special handling here because setting parent resets invert parent matrix,
   * which is evil in our case. */
  Object *ob = static_cast<Object *>(ptr_dst->data);
  Object *parent_dst = static_cast<Object *>(RNA_property_pointer_get(ptr_dst, prop_dst).data);
  Object *parent_src = static_cast<Object *>(RNA_property_pointer_get(ptr_src, prop_src).data);

  if (parent_src == parent_dst) {
    return false;
  }

  if (parent_src == nullptr) {
    /* The only case where we do want default behavior (with matrix reset). */
    blender::ed::object::parent_set(ob, parent_src, ob->partype, ob->parsubstr);
  }
  else {
    ob->parent = parent_src;
  }
  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static void rna_Object_parent_type_set(PointerRNA *ptr, int value)
{
  Object *ob = static_cast<Object *>(ptr->data);

  /* Skip if type did not change (otherwise we loose parent inverse in
   * blender::ed::object::parent_set). */
  if (ob->partype == value) {
    return;
  }

  blender::ed::object::parent_set(ob, ob->parent, value, ob->parsubstr);
}

static bool rna_Object_parent_type_override_apply(Main *bmain,
                                                  RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  const int len_dst = rnaapply_ctx.len_src;
  const int len_src = rnaapply_ctx.len_src;
  const int len_storage = rnaapply_ctx.len_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert(opop->operation == LIBOVERRIDE_OP_REPLACE &&
             "Unsupported RNA override operation on object parent pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* We need a special handling here because setting parent resets invert parent matrix,
   * which is evil in our case. */
  Object *ob = (Object *)(ptr_dst->data);
  const int parent_type_dst = RNA_property_enum_get(ptr_dst, prop_dst);
  const int parent_type_src = RNA_property_enum_get(ptr_src, prop_src);

  if (parent_type_dst == parent_type_src) {
    return false;
  }

  ob->partype = parent_type_src;
  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static const EnumPropertyItem *rna_Object_parent_type_itemf(bContext * /*C*/,
                                                            PointerRNA *ptr,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  Object *ob = static_cast<Object *>(ptr->data);
  EnumPropertyItem *item = nullptr;
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
  Object *ob = static_cast<Object *>(ptr->data);

  BKE_object_empty_draw_type_set(ob, value);
}

static void rna_Object_parent_bone_set(PointerRNA *ptr, const char *value)
{
  Object *ob = static_cast<Object *>(ptr->data);

  blender::ed::object::parent_set(ob, ob->parent, ob->partype, value);
}

static bool rna_Object_parent_bone_override_apply(Main *bmain,
                                                  RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  const int len_dst = rnaapply_ctx.len_src;
  const int len_src = rnaapply_ctx.len_src;
  const int len_storage = rnaapply_ctx.len_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert(opop->operation == LIBOVERRIDE_OP_REPLACE &&
             "Unsupported RNA override operation on object parent bone property");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* We need a special handling here because setting parent resets invert parent matrix,
   * which is evil in our case. */
  Object *ob = (Object *)(ptr_dst->data);
  char parent_bone_dst[MAX_ID_NAME - 2];
  RNA_property_string_get(ptr_dst, prop_dst, parent_bone_dst);
  char parent_bone_src[MAX_ID_NAME - 2];
  RNA_property_string_get(ptr_src, prop_src, parent_bone_src);

  if (STREQ(parent_bone_src, parent_bone_dst)) {
    return false;
  }

  STRNCPY(ob->parsubstr, parent_bone_src);
  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static const EnumPropertyItem *rna_Object_instance_type_itemf(bContext * /*C*/,
                                                              PointerRNA *ptr,
                                                              PropertyRNA * /*prop*/,
                                                              bool * /*r_free*/)
{
  Object *ob = static_cast<Object *>(ptr->data);
  const EnumPropertyItem *item;

  if (ob->type == OB_EMPTY) {
    item = instance_items_empty;
  }
  else if (ob->type == OB_FONT) {
    item = instance_items_font;
  }
  else {
    item = instance_items_nogroup;
  }

  return item;
}

static void rna_Object_dup_collection_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  Object *ob = static_cast<Object *>(ptr->data);
  Collection *grp = static_cast<Collection *>(value.data);

  /* Must not let this be set if the object belongs in this group already,
   * thus causing a cycle/infinite-recursion leading to crashes on load #25298. */
  if (BKE_collection_has_object_recursive(grp, ob) == 0) {
    if (ob->type == OB_EMPTY) {
      id_us_min(&ob->instance_collection->id);
      ob->instance_collection = grp;
      id_us_plus(&ob->instance_collection->id);
    }
    else {
      BKE_report(nullptr, RPT_ERROR, "Only empty objects support collection instances");
    }
  }
  else {
    BKE_report(
        nullptr,
        RPT_ERROR,
        "Cannot set instance-collection as object belongs in collection being instanced, thus "
        "causing a cycle");
  }
}

static void rna_Object_vertex_groups_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  WM_main_add_notifier(NC_GEOM | ND_VERTEX_GROUP, ob->data);
  rna_Object_internal_update_data_impl(ptr);
}

static void rna_Object_vertex_groups_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  Object *ob = static_cast<Object *>(ptr->data);
  if (!BKE_object_supports_vertex_groups(ob)) {
    iter->valid = 0;
    return;
  }

  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);
  iter->valid = defbase != nullptr;

  rna_iterator_listbase_begin(iter, ptr, defbase, nullptr);
}

static void rna_VertexGroup_name_set(PointerRNA *ptr, const char *value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return;
  }

  bDeformGroup *dg = static_cast<bDeformGroup *>(ptr->data);
  BKE_object_defgroup_set_name(dg, ob, value);
}

static int rna_VertexGroup_index_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return -1;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  return BLI_findindex(defbase, ptr->data);
}

static PointerRNA rna_Object_active_vertex_group_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return PointerRNA_NULL;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);

  return RNA_pointer_create_with_parent(
      *ptr, &RNA_VertexGroup, BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1));
}

static void rna_Object_active_vertex_group_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               ReportList *reports)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);

  int index = BLI_findindex(defbase, value.data);
  if (index == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "VertexGroup '%s' not found in object '%s'",
                (static_cast<bDeformGroup *>(value.data))->name,
                ob->id.name + 2);
    return;
  }

  BKE_object_defgroup_active_index_set(ob, index + 1);
}

static int rna_Object_active_vertex_group_index_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return -1;
  }

  return BKE_object_defgroup_active_index_get(ob) - 1;
}

static void rna_Object_active_vertex_group_index_set(PointerRNA *ptr, int value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return;
  }

  BKE_object_defgroup_active_index_set(ob, value + 1);
}

static void rna_Object_active_vertex_group_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  *min = 0;
  if (!BKE_object_supports_vertex_groups(ob)) {
    *max = 0;
    return;
  }
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  *max = max_ii(0, BLI_listbase_count(defbase) - 1);
}

void rna_object_vgroup_name_index_get(PointerRNA *ptr, char *value, int index)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    value[0] = '\0';
    return;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  const bDeformGroup *dg = static_cast<bDeformGroup *>(BLI_findlink(defbase, index - 1));

  if (dg) {
    strcpy(value, dg->name);
  }
  else {
    value[0] = '\0';
  }
}

int rna_object_vgroup_name_index_length(PointerRNA *ptr, int index)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    return 0;
  }

  const ListBase *defbase = BKE_object_defgroup_list(ob);
  bDeformGroup *dg = static_cast<bDeformGroup *>(BLI_findlink(defbase, index - 1));
  return (dg) ? strlen(dg->name) : 0;
}

void rna_object_vgroup_name_index_set(PointerRNA *ptr, const char *value, short *index)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    *index = -1;
    return;
  }

  *index = BKE_object_defgroup_name_index(ob, value) + 1;
}

void rna_object_vgroup_name_set(PointerRNA *ptr,
                                const char *value,
                                char *result,
                                int result_maxncpy)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (!BKE_object_supports_vertex_groups(ob)) {
    result[0] = '\0';
    return;
  }

  bDeformGroup *dg = BKE_object_defgroup_find_name(ob, value);
  if (dg) {
    /* No need for BLI_strncpy_utf8, since this matches an existing group. */
    BLI_strncpy(result, value, result_maxncpy);
    return;
  }

  result[0] = '\0';
}

void rna_object_uvlayer_name_set(PointerRNA *ptr,
                                 const char *value,
                                 char *result,
                                 int result_maxncpy)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Mesh *mesh;
  CustomDataLayer *layer;
  int a;

  if (ob->type == OB_MESH && ob->data) {
    mesh = static_cast<Mesh *>(ob->data);

    for (a = 0; a < mesh->corner_data.totlayer; a++) {
      layer = &mesh->corner_data.layers[a];

      if (layer->type == CD_PROP_FLOAT2 && STREQ(layer->name, value)) {
        BLI_strncpy(result, value, result_maxncpy);
        return;
      }
    }
  }

  result[0] = '\0';
}

void rna_object_vcollayer_name_set(PointerRNA *ptr,
                                   const char *value,
                                   char *result,
                                   int result_maxncpy)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Mesh *mesh;
  CustomDataLayer *layer;
  int a;

  if (ob->type == OB_MESH && ob->data) {
    mesh = static_cast<Mesh *>(ob->data);

    for (a = 0; a < mesh->fdata_legacy.totlayer; a++) {
      layer = &mesh->fdata_legacy.layers[a];

      if (layer->type == CD_MCOL && STREQ(layer->name, value)) {
        BLI_strncpy(result, value, result_maxncpy);
        return;
      }
    }
  }

  result[0] = '\0';
}

static int rna_Object_active_material_index_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  return std::max<int>(ob->actcol - 1, 0);
}

static void rna_Object_active_material_index_set(PointerRNA *ptr, int value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  value = std::max(std::min(value, ob->totcol - 1), 0);
  ob->actcol = value + 1;

  if (ob->type == OB_MESH) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);

    if (mesh->runtime->edit_mesh) {
      mesh->runtime->edit_mesh->mat_nr = value;
    }
  }
}

static void rna_Object_active_material_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  *min = 0;
  *max = max_ii(ob->totcol - 1, 0);
}

/* returns active base material */
static PointerRNA rna_Object_active_material_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Material *ma;

  ma = (ob->totcol) ? BKE_object_material_get(ob, ob->actcol) : nullptr;
  return RNA_id_pointer_create(reinterpret_cast<ID *>(ma));
}

static void rna_Object_active_material_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           ReportList * /*reports*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  DEG_id_tag_update(static_cast<ID *>(value.data), 0);
  BLI_assert(BKE_id_is_in_global_main(&ob->id));
  BLI_assert(BKE_id_is_in_global_main(static_cast<ID *>(value.data)));
  BKE_object_material_assign(
      G_MAIN, ob, static_cast<Material *>(value.data), ob->actcol, BKE_MAT_ASSIGN_EXISTING);
}

static int rna_Object_active_material_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  bool is_editable;

  if ((ob->matbits == nullptr) || (ob->actcol == 0) || ob->matbits[ob->actcol - 1]) {
    is_editable = ID_IS_EDITABLE(ob);
  }
  else {
    is_editable = ob->data ? ID_IS_EDITABLE(ob->data) : false;
  }

  return is_editable ? int(PROP_EDITABLE) : 0;
}

static void rna_Object_active_particle_system_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  *min = 0;
  *max = max_ii(0, BLI_listbase_count(&ob->particlesystem) - 1);
}

static int rna_Object_active_particle_system_index_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  return psys_get_current_num(ob);
}

static void rna_Object_active_particle_system_index_set(PointerRNA *ptr, int value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  psys_set_current_num(ob, value);
}

static void rna_Object_particle_update(Main * /*bmain*/, Scene *scene, PointerRNA *ptr)
{
  /* TODO: Disabled for now, because bContext is not available. */
#  if 0
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  PE_current_changed(nullptr, scene, ob);
#  else
  (void)scene;
  (void)ptr;
#  endif
}

/* rotation - axis-angle */
static void rna_Object_rotation_axis_angle_get(PointerRNA *ptr, float *value)
{
  Object *ob = static_cast<Object *>(ptr->data);

  /* for now, assume that rotation mode is axis-angle */
  value[0] = ob->rotAngle;
  copy_v3_v3(&value[1], ob->rotAxis);
}

/* rotation - axis-angle */
static void rna_Object_rotation_axis_angle_set(PointerRNA *ptr, const float *value)
{
  Object *ob = static_cast<Object *>(ptr->data);

  /* for now, assume that rotation mode is axis-angle */
  ob->rotAngle = value[0];
  copy_v3_v3(ob->rotAxis, &value[1]);

  /* TODO: validate axis? */
}

static void rna_Object_rotation_mode_set(PointerRNA *ptr, int value)
{
  Object *ob = static_cast<Object *>(ptr->data);

  /* use API Method for conversions... */
  BKE_rotMode_change_values(
      ob->quat, ob->rot, ob->rotAxis, &ob->rotAngle, ob->rotmode, short(value));

  /* finally, set the new rotation type */
  ob->rotmode = value;
}

static void rna_Object_dimensions_get(PointerRNA *ptr, float *value)
{
  Object *ob = static_cast<Object *>(ptr->data);
  BKE_object_dimensions_eval_cached_get(ob, value);
}

static void rna_Object_dimensions_set(PointerRNA *ptr, const float *value)
{
  Object *ob = static_cast<Object *>(ptr->data);
  BKE_object_dimensions_set(ob, value, 0);
}

static int rna_Object_location_editable(const PointerRNA *ptr, int index)
{
  Object *ob = static_cast<Object *>(ptr->data);

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

static int rna_Object_scale_editable(const PointerRNA *ptr, int index)
{
  Object *ob = static_cast<Object *>(ptr->data);

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

static int rna_Object_rotation_euler_editable(const PointerRNA *ptr, int index)
{
  Object *ob = static_cast<Object *>(ptr->data);

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

static int rna_Object_rotation_4d_editable(const PointerRNA *ptr, int index)
{
  Object *ob = static_cast<Object *>(ptr->data);

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

static int rna_MaterialSlot_index(const PointerRNA *ptr)
{
  /* There is an offset, so that `ptr->data` is not null and unique across IDs. */
  return uintptr_t(ptr->data) - uintptr_t(ptr->owner_id);
}

static int rna_MaterialSlot_index_get(PointerRNA *ptr)
{
  return rna_MaterialSlot_index(ptr);
}

static int rna_MaterialSlot_material_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  const int index = rna_MaterialSlot_index(ptr);
  bool is_editable;

  if ((ob->matbits == nullptr) || ob->matbits[index]) {
    is_editable = ID_IS_EDITABLE(ob);
  }
  else {
    is_editable = ob->data ? ID_IS_EDITABLE(ob->data) : false;
  }

  return is_editable ? int(PROP_EDITABLE) : 0;
}

static PointerRNA rna_MaterialSlot_material_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Material *ma;
  const int index = rna_MaterialSlot_index(ptr);

  if (DEG_is_evaluated(ob)) {
    ma = BKE_object_material_get_eval(ob, index + 1);
  }
  else {
    ma = BKE_object_material_get(ob, index + 1);
  }
  return RNA_id_pointer_create(reinterpret_cast<ID *>(ma));
}

static void rna_MaterialSlot_material_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  int index = rna_MaterialSlot_index(ptr);

  BLI_assert(BKE_id_is_in_global_main(&ob->id));
  BLI_assert(BKE_id_is_in_global_main(static_cast<ID *>(value.data)));
  BKE_object_material_assign(
      G_MAIN, ob, static_cast<Material *>(value.data), index + 1, BKE_MAT_ASSIGN_EXISTING);
}

static bool rna_MaterialSlot_material_poll(PointerRNA *ptr, PointerRNA value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Material *ma = static_cast<Material *>(value.data);

  if (ELEM(ob->type, OB_GREASE_PENCIL)) {
    /* GP Materials only */
    return (ma->gp_style != nullptr);
  }
  else {
    /* Everything except GP materials */
    return (ma->gp_style == nullptr);
  }
}

static int rna_MaterialSlot_link_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  int index = rna_MaterialSlot_index(ptr);
  if (index < ob->totcol) {
    return ob->matbits[index] != 0;
  }
  return false;
}

static void rna_MaterialSlot_link_set(PointerRNA *ptr, int value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  int index = rna_MaterialSlot_index(ptr);

  if (value) {
    ob->matbits[index] = 1;
    /* DEPRECATED */
    // ob->colbits |= (1 << index);
  }
  else {
    ob->matbits[index] = 0;
    /* DEPRECATED */
    // ob->colbits &= ~(1 << index);
  }
}

static int rna_MaterialSlot_name_length(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Material *ma;
  int index = rna_MaterialSlot_index(ptr);

  ma = BKE_object_material_get(ob, index + 1);

  if (ma) {
    return strlen(ma->id.name + 2);
  }

  return 0;
}

static void rna_MaterialSlot_name_get(PointerRNA *ptr, char *value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Material *ma;
  int index = rna_MaterialSlot_index(ptr);

  ma = BKE_object_material_get(ob, index + 1);

  if (ma) {
    strcpy(value, ma->id.name + 2);
  }
  else {
    value[0] = '\0';
  }
}

static void rna_MaterialSlot_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  rna_Object_internal_update(bmain, scene, ptr);

  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, ptr->owner_id);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, nullptr);
  DEG_relations_tag_update(bmain);
}

static std::optional<std::string> rna_MaterialSlot_path(const PointerRNA *ptr)
{
  int index = rna_MaterialSlot_index(ptr);
  return fmt::format("material_slots[{}]", index);
}

static int rna_Object_material_slots_length(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (DEG_is_evaluated(ob)) {
    return BKE_object_material_count_eval(ob);
  }
  else {
    return ob->totcol;
  }
}

static void rna_Object_material_slots_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  const int length = rna_Object_material_slots_length(ptr);
  iter->parent = *ptr;
  iter->internal.count.item = 0;
  iter->internal.count.ptr = ptr->owner_id;
  iter->valid = length > 0;
}

static void rna_Object_material_slots_next(CollectionPropertyIterator *iter)
{
  const int length = rna_Object_material_slots_length(&iter->ptr);
  iter->internal.count.item++;
  iter->valid = iter->internal.count.item < length;
}

static PointerRNA rna_Object_material_slots_get(CollectionPropertyIterator *iter)
{
  ID *id = static_cast<ID *>(iter->internal.count.ptr);
  PointerRNA ptr = RNA_pointer_create_with_parent(
      iter->parent,
      &RNA_MaterialSlot,
      /* Add offset, so that `ptr->data` is not null and unique across IDs. */
      (void *)(iter->internal.count.item + uintptr_t(id)));
  return ptr;
}

static void rna_Object_material_slots_end(CollectionPropertyIterator * /*iter*/) {}

static PointerRNA rna_Object_display_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_ObjectDisplay, ptr->data);
}

static std::optional<std::string> rna_ObjectDisplay_path(const PointerRNA * /*ptr*/)
{
  return "display";
}

static PointerRNA rna_Object_active_particle_system_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  ParticleSystem *psys = psys_get_current(ob);
  return RNA_pointer_create_with_parent(*ptr, &RNA_ParticleSystem, psys);
}

static void rna_Object_active_shape_key_index_range(
    PointerRNA *ptr, int *min, int *max, int * /*softmin*/, int * /*softmax*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
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
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  return std::max<int>(ob->shapenr - 1, 0);
}

static void rna_Object_active_shape_key_index_set(PointerRNA *ptr, int value)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  ob->shapenr = value + 1;
}

static PointerRNA rna_Object_active_shape_key_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  Key *key = BKE_key_from_object(ob);
  KeyBlock *kb;

  if (key == nullptr) {
    return PointerRNA_NULL;
  }

  kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));
  PointerRNA keyptr = RNA_pointer_create_discrete(reinterpret_cast<ID *>(key), &RNA_ShapeKey, kb);
  return keyptr;
}

static PointerRNA rna_Object_field_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  return RNA_pointer_create_with_parent(*ptr, &RNA_FieldSettings, ob->pd);
}

static PointerRNA rna_Object_collision_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);

  if (ob->type != OB_MESH) {
    return PointerRNA_NULL;
  }

  return RNA_pointer_create_with_parent(*ptr, &RNA_CollisionSettings, ob->pd);
}

static PointerRNA rna_Object_active_constraint_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  bConstraint *con = BKE_constraints_active_get(&ob->constraints);
  return RNA_pointer_create_with_parent(*ptr, &RNA_Constraint, con);
}

static void rna_Object_active_constraint_set(PointerRNA *ptr,
                                             PointerRNA value,
                                             ReportList * /*reports*/)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  BKE_constraints_active_set(&ob->constraints, static_cast<bConstraint *>(value.data));
}

static bConstraint *rna_Object_constraints_new(Object *object, Main *bmain, int type)
{
  bConstraint *new_con = BKE_constraint_add_for_object(object, nullptr, type);

  blender::ed::object::constraint_tag_update(bmain, object, new_con);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, object);

  /* The Depsgraph needs to be updated to reflect the new relationship that was added. */
  DEG_relations_tag_update(bmain);

  return new_con;
}

static void rna_Object_constraints_remove(Object *object,
                                          Main *bmain,
                                          ReportList *reports,
                                          PointerRNA *con_ptr)
{
  bConstraint *con = static_cast<bConstraint *>(con_ptr->data);
  if (BLI_findindex(&object->constraints, con) == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Constraint '%s' not found in object '%s'",
                con->name,
                object->id.name + 2);
    return;
  }

  BKE_constraint_remove_ex(&object->constraints, object, con);
  con_ptr->invalidate();

  blender::ed::object::constraint_update(bmain, object);
  blender::ed::object::constraint_active_set(object, nullptr);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, object);
}

static void rna_Object_constraints_clear(Object *object, Main *bmain)
{
  BKE_constraints_free(&object->constraints);

  blender::ed::object::constraint_update(bmain, object);
  blender::ed::object::constraint_active_set(object, nullptr);

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

  blender::ed::object::constraint_tag_update(bmain, object, nullptr);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT, object);
}

static bConstraint *rna_Object_constraints_copy(Object *object, Main *bmain, PointerRNA *con_ptr)
{
  bConstraint *con = static_cast<bConstraint *>(con_ptr->data);
  bConstraint *new_con = BKE_constraint_copy_for_object(object, con);
  new_con->flag |= CONSTRAINT_OVERRIDE_LIBRARY_LOCAL;

  blender::ed::object::constraint_tag_update(bmain, object, new_con);
  WM_main_add_notifier(NC_OBJECT | ND_CONSTRAINT | NA_ADDED, object);

  return new_con;
}

bool rna_Object_constraints_override_apply(Main *bmain,
                                           RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(opop->operation == LIBOVERRIDE_OP_INSERT_AFTER &&
             "Unsupported RNA override operation on constraints collection");

  Object *ob_dst = reinterpret_cast<Object *>(ptr_dst->owner_id);
  Object *ob_src = reinterpret_cast<Object *>(ptr_src->owner_id);

  /* Remember that insertion operations are defined and stored in correct order, which means that
   * even if we insert several items in a row, we always insert first one, then second one, etc.
   * So we should always find 'anchor' constraint in both _src *and* _dst. */
  const size_t name_offset = offsetof(bConstraint, name);
  bConstraint *con_anchor = static_cast<bConstraint *>(
      BLI_listbase_string_or_index_find(&ob_dst->constraints,
                                        opop->subitem_reference_name,
                                        name_offset,
                                        opop->subitem_reference_index));
  /* If `con_anchor` is nullptr, `con_src` will be inserted in first position. */

  bConstraint *con_src = static_cast<bConstraint *>(BLI_listbase_string_or_index_find(
      &ob_src->constraints, opop->subitem_local_name, name_offset, opop->subitem_local_index));

  if (con_src == nullptr) {
    BLI_assert(con_src != nullptr);
    return false;
  }

  bConstraint *con_dst = BKE_constraint_duplicate_ex(con_src, 0, true);

  /* This handles nullptr anchor as expected by adding at head of list. */
  BLI_insertlinkafter(&ob_dst->constraints, con_anchor, con_dst);

  /* This should actually *not* be needed in typical cases.
   * However, if overridden source was edited, we *may* have some new conflicting names. */
  BKE_constraint_unique_name(con_dst, &ob_dst->constraints);

  //  printf("%s: We inserted a constraint...\n", __func__);
  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

static ModifierData *rna_Object_modifier_new(
    Object *object, bContext *C, ReportList *reports, const char *name, int type)
{
  ModifierData *md = blender::ed::object::modifier_add(
      reports, CTX_data_main(C), CTX_data_scene(C), object, name, type);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_ADDED, object);

  return md;
}

static void rna_Object_modifier_remove(Object *object,
                                       bContext *C,
                                       ReportList *reports,
                                       PointerRNA *md_ptr)
{
  ModifierData *md = static_cast<ModifierData *>(md_ptr->data);
  if (blender::ed::object::modifier_remove(
          reports, CTX_data_main(C), CTX_data_scene(C), object, md) == false)
  {
    /* error is already set */
    return;
  }

  md_ptr->invalidate();

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_modifier_clear(Object *object, bContext *C)
{
  blender::ed::object::modifiers_clear(CTX_data_main(C), CTX_data_scene(C), object);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_modifier_move(Object *object, ReportList *reports, int from, int to)
{
  ModifierData *md = static_cast<ModifierData *>(BLI_findlink(&object->modifiers, from));

  if (!md) {
    BKE_reportf(reports, RPT_ERROR, "Invalid original modifier index '%d'", from);
    return;
  }

  blender::ed::object::modifier_move_to_index(reports, RPT_ERROR, object, md, to, false);
}

static PointerRNA rna_Object_active_modifier_get(PointerRNA *ptr)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  ModifierData *md = BKE_object_active_modifier(ob);
  return RNA_pointer_create_with_parent(*ptr, &RNA_Modifier, md);
}

static void rna_Object_active_modifier_set(PointerRNA *ptr, PointerRNA value, ReportList *reports)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  ModifierData *md = static_cast<ModifierData *>(value.data);

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ob);

  if (RNA_pointer_is_null(&value)) {
    BKE_object_modifier_set_active(ob, nullptr);
    return;
  }

  if (BLI_findindex(&ob->modifiers, md) == -1) {
    BKE_reportf(
        reports, RPT_ERROR, "Modifier \"%s\" is not in the object's modifier list", md->name);
    return;
  }

  BKE_object_modifier_set_active(ob, md);
}

bool rna_Object_modifiers_override_apply(Main *bmain,
                                         RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(opop->operation == LIBOVERRIDE_OP_INSERT_AFTER &&
             "Unsupported RNA override operation on modifiers collection");

  Object *ob_dst = reinterpret_cast<Object *>(ptr_dst->owner_id);
  Object *ob_src = reinterpret_cast<Object *>(ptr_src->owner_id);

  /* Remember that insertion operations are defined and stored in correct order, which means that
   * even if we insert several items in a row, we always insert first one, then second one, etc.
   * So we should always find 'anchor' modifier in both _src *and* _dst. */
  const size_t name_offset = offsetof(ModifierData, name);
  ModifierData *mod_anchor = static_cast<ModifierData *>(
      BLI_listbase_string_or_index_find(&ob_dst->modifiers,
                                        opop->subitem_reference_name,
                                        name_offset,
                                        opop->subitem_reference_index));
  /* If `mod_anchor` is nullptr, `mod_src` will be inserted in first position. */

  ModifierData *mod_src = static_cast<ModifierData *>(BLI_listbase_string_or_index_find(
      &ob_src->modifiers, opop->subitem_local_name, name_offset, opop->subitem_local_index));

  if (mod_src == nullptr) {
    BLI_assert(mod_src != nullptr);
    return false;
  }

  /* While it would be nicer to use lower-level BKE_modifier_new() here, this one is lacking
   * special-cases handling (particles and other physics modifiers mostly), so using the ED version
   * instead, to avoid duplicating code. */
  ModifierData *mod_dst = blender::ed::object::modifier_add(
      nullptr, bmain, nullptr, ob_dst, mod_src->name, mod_src->type);

  if (mod_dst == nullptr) {
    /* This can happen e.g. when a modifier type is tagged as `eModifierTypeFlag_Single`, and that
     * modifier has somehow been added already by another code path (e.g.
     * `rna_CollisionSettings_dependency_update` does add the `eModifierType_Collision` singleton
     * modifier).
     *
     * Try to handle this by finding already existing one here. */
    const ModifierTypeInfo *mti = BKE_modifier_get_info((ModifierType)mod_src->type);
    if (mti->flags & eModifierTypeFlag_Single) {
      mod_dst = BKE_modifiers_findby_type(ob_dst, (ModifierType)mod_src->type);
    }

    if (mod_dst == nullptr) {
      BLI_assert(mod_src != nullptr);
      return false;
    }
  }

  /* XXX Current handling of 'copy' from particle-system modifier is *very* bad (it keeps same psys
   * pointer as source, then calling code copies psys of object separately and do some magic
   * remapping of pointers...), unfortunately several pieces of code in Object editing area rely on
   * this behavior. So for now, hacking around it to get it doing what we want it to do, as getting
   * a proper behavior would be everything but trivial, and this whole particle thingy is
   * end-of-life. */
  ParticleSystem *psys_dst = (mod_dst->type == eModifierType_ParticleSystem) ?
                                 (reinterpret_cast<ParticleSystemModifierData *>(mod_dst))->psys :
                                 nullptr;
  const int persistent_uid = mod_dst->persistent_uid;
  BKE_modifier_copydata(mod_src, mod_dst);
  mod_dst->persistent_uid = persistent_uid;
  if (mod_dst->type == eModifierType_ParticleSystem) {
    psys_dst->flag &= ~PSYS_DELETE;
    (reinterpret_cast<ParticleSystemModifierData *>(mod_dst))->psys = psys_dst;
  }

  BLI_remlink(&ob_dst->modifiers, mod_dst);
  /* This handles nullptr anchor as expected by adding at head of list. */
  BLI_insertlinkafter(&ob_dst->modifiers, mod_anchor, mod_dst);

  //  printf("%s: We inserted a modifier '%s'...\n", __func__, mod_dst->name);
  RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  return true;
}

/* shader fx */
static ShaderFxData *rna_Object_shaderfx_new(
    Object *object, bContext *C, ReportList *reports, const char *name, int type)
{
  return blender::ed::object::shaderfx_add(
      reports, CTX_data_main(C), CTX_data_scene(C), object, name, type);
}

static void rna_Object_shaderfx_remove(Object *object,
                                       bContext *C,
                                       ReportList *reports,
                                       PointerRNA *gmd_ptr)
{
  ShaderFxData *gmd = static_cast<ShaderFxData *>(gmd_ptr->data);
  if (blender::ed::object::shaderfx_remove(reports, CTX_data_main(C), object, gmd) == false) {
    /* error is already set */
    return;
  }

  gmd_ptr->invalidate();

  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_shaderfx_clear(Object *object, bContext *C)
{
  blender::ed::object::shaderfx_clear(CTX_data_main(C), object);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER | NA_REMOVED, object);
}

static void rna_Object_boundbox_get(PointerRNA *ptr, float *values)
{
  using namespace blender;
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (const std::optional<Bounds<float3>> bounds = BKE_object_boundbox_eval_cached_get(ob)) {
    *reinterpret_cast<std::array<float3, 8> *>(values) = blender::bounds::corners(*bounds);
  }
  else {
    copy_vn_fl(values, 8 * 3, 0.0f);
  }
}

static bool check_object_vgroup_support_and_warn(const Object *ob,
                                                 const char *op_name,
                                                 ReportList *reports)
{
  if (!BKE_object_supports_vertex_groups(ob)) {
    const char *ob_type_name = "Unknown";
    RNA_enum_name_from_value(rna_enum_object_type_items, ob->type, &ob_type_name);
    BKE_reportf(reports, RPT_ERROR, "%s is not supported for '%s' objects", op_name, ob_type_name);
    return false;
  }
  return true;
}

static bDeformGroup *rna_Object_vgroup_new(Object *ob,
                                           Main *bmain,
                                           ReportList *reports,
                                           const char *name)
{
  if (!check_object_vgroup_support_and_warn(ob, "VertexGroups.new()", reports)) {
    return nullptr;
  }

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
  if (!check_object_vgroup_support_and_warn(ob, "VertexGroups.remove()", reports)) {
    return;
  }

  bDeformGroup *defgroup = static_cast<bDeformGroup *>(defgroup_ptr->data);
  ListBase *defbase = BKE_object_defgroup_list_mutable(ob);

  if (BLI_findindex(defbase, defgroup) == -1) {
    BKE_reportf(reports,
                RPT_ERROR,
                "DeformGroup '%s' not in object '%s'",
                defgroup->name,
                ob->id.name + 2);
    return;
  }

  BKE_object_defgroup_remove(ob, defgroup);
  defgroup_ptr->invalidate();

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_Object_vgroup_clear(Object *ob, Main *bmain, ReportList *reports)
{
  if (!check_object_vgroup_support_and_warn(ob, "VertexGroups.clear()", reports)) {
    return;
  }

  BKE_object_defgroup_remove_all(ob);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

static void rna_VertexGroup_vertex_add(ID *id,
                                       bDeformGroup *def,
                                       ReportList *reports,
                                       const int *index,
                                       int index_num,
                                       float weight,
                                       int assignmode)
{
  Object *ob = reinterpret_cast<Object *>(id);

  if (BKE_object_is_in_editmode_vgroup(ob)) {
    BKE_report(
        reports, RPT_ERROR, "VertexGroup.add(): cannot be called while object is in edit mode");
    return;
  }

  while (index_num--) {
    /* XXX: not efficient calling within loop. */
    blender::ed::object::vgroup_vert_add(ob, def, *index++, weight, assignmode);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, static_cast<ID *>(ob->data));
}

static void rna_VertexGroup_vertex_remove(
    ID *id, bDeformGroup *dg, ReportList *reports, const int *index, int index_num)
{
  Object *ob = reinterpret_cast<Object *>(id);

  if (BKE_object_is_in_editmode_vgroup(ob)) {
    BKE_report(
        reports, RPT_ERROR, "VertexGroup.remove(): cannot be called while object is in edit mode");
    return;
  }

  while (index_num--) {
    blender::ed::object::vgroup_vert_remove(ob, dg, *index++);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_GEOM | ND_DATA, static_cast<ID *>(ob->data));
}

static float rna_VertexGroup_weight(ID *id, bDeformGroup *dg, ReportList *reports, int index)
{
  float weight = blender::ed::object::vgroup_vert_weight(
      reinterpret_cast<Object *>(id), dg, index);

  if (weight < 0) {
    BKE_report(reports, RPT_ERROR, "Vertex not in group");
  }
  return weight;
}

/* generic poll functions */
bool rna_Lattice_object_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  return (reinterpret_cast<Object *>(value.owner_id))->type == OB_LATTICE;
}

bool rna_Curve_object_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  return (reinterpret_cast<Object *>(value.owner_id))->type == OB_CURVES_LEGACY;
}

bool rna_Armature_object_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  return (reinterpret_cast<Object *>(value.owner_id))->type == OB_ARMATURE;
}

bool rna_Mesh_object_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  return (reinterpret_cast<Object *>(value.owner_id))->type == OB_MESH;
}

bool rna_Camera_object_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  return (reinterpret_cast<Object *>(value.owner_id))->type == OB_CAMERA;
}

bool rna_Light_object_poll(PointerRNA * /*ptr*/, PointerRNA value)
{
  return (reinterpret_cast<Object *>(value.owner_id))->type == OB_LAMP;
}

bool rna_Object_use_dynamic_topology_sculpting_get(PointerRNA *ptr)
{
  return BKE_object_sculpt_use_dyntopo(reinterpret_cast<Object *>(ptr->owner_id));
}

static void rna_object_lineart_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_MODIFIER, ptr->owner_id);
}

static std::optional<std::string> rna_ObjectLineArt_path(const PointerRNA * /*ptr*/)
{
  return "lineart";
}

static bool mesh_symmetry_get_common(PointerRNA *ptr, const eMeshSymmetryType sym)
{
  const Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (ob->type != OB_MESH) {
    return false;
  }

  const Mesh *mesh = static_cast<Mesh *>(ob->data);
  return mesh->symmetry & sym;
}

static bool rna_Object_mesh_symmetry_x_get(PointerRNA *ptr)
{
  return mesh_symmetry_get_common(ptr, ME_SYMMETRY_X);
}

static bool rna_Object_mesh_symmetry_y_get(PointerRNA *ptr)
{
  return mesh_symmetry_get_common(ptr, ME_SYMMETRY_Y);
}

static bool rna_Object_mesh_symmetry_z_get(PointerRNA *ptr)
{
  return mesh_symmetry_get_common(ptr, ME_SYMMETRY_Z);
}

static void mesh_symmetry_set_common(PointerRNA *ptr,
                                     const bool value,
                                     const eMeshSymmetryType sym)
{
  Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (ob->type != OB_MESH) {
    return;
  }

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  if (value) {
    mesh->symmetry |= sym;
  }
  else {
    mesh->symmetry &= ~sym;
  }
}

static void rna_Object_mesh_symmetry_x_set(PointerRNA *ptr, bool value)
{
  mesh_symmetry_set_common(ptr, value, ME_SYMMETRY_X);
}

static void rna_Object_mesh_symmetry_y_set(PointerRNA *ptr, bool value)
{
  mesh_symmetry_set_common(ptr, value, ME_SYMMETRY_Y);
}

static void rna_Object_mesh_symmetry_z_set(PointerRNA *ptr, bool value)
{
  mesh_symmetry_set_common(ptr, value, ME_SYMMETRY_Z);
}

static int rna_Object_mesh_symmetry_yz_editable(const PointerRNA *ptr, const char ** /*r_info*/)
{
  const Object *ob = reinterpret_cast<Object *>(ptr->owner_id);
  if (ob->type != OB_MESH) {
    return 0;
  }

  const Mesh *mesh = static_cast<Mesh *>(ob->data);
  if (ob->mode == OB_MODE_WEIGHT_PAINT && mesh->editflag & ME_EDIT_MIRROR_VERTEX_GROUPS) {
    /* Only X symmetry is available in weight-paint mode. */
    return 0;
  }

  return PROP_EDITABLE;
}

void rna_Object_lightgroup_get(PointerRNA *ptr, char *value)
{
  const LightgroupMembership *lgm = (reinterpret_cast<Object *>(ptr->owner_id))->lightgroup;
  char value_buf[sizeof(lgm->name)];
  int len = BKE_lightgroup_membership_get(lgm, value_buf);
  memcpy(value, value_buf, len + 1);
}

int rna_Object_lightgroup_length(PointerRNA *ptr)
{
  const LightgroupMembership *lgm = (reinterpret_cast<Object *>(ptr->owner_id))->lightgroup;
  return BKE_lightgroup_membership_length(lgm);
}

void rna_Object_lightgroup_set(PointerRNA *ptr, const char *value)
{
  BKE_lightgroup_membership_set(&(reinterpret_cast<Object *>(ptr->owner_id))->lightgroup, value);
}

static PointerRNA rna_Object_light_linking_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_ObjectLightLinking, ptr->data);
}

static std::optional<std::string> rna_ObjectLightLinking_path(const PointerRNA * /*ptr*/)
{
  return "light_linking";
}

bool rna_Object_light_linking_override_apply(Main *bmain,
                                             RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  /* NOTE: Here:
   *   - `dst` is the new, being updated liboverride data, which is a clean copy from the linked
   *     reference data.
   *   - `src` is the old, stored liboverride data, which is the source to copy overridden data
   *     from.
   */

  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  const int len_dst = rnaapply_ctx.len_src;
  const int len_src = rnaapply_ctx.len_src;
  const int len_storage = rnaapply_ctx.len_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  BLI_assert(len_dst == len_src && (!ptr_storage || len_dst == len_storage) && len_dst == 0);
  BLI_assert_msg(opop->operation == LIBOVERRIDE_OP_REPLACE,
                 "Unsupported RNA override operation on object light linking pointer");
  UNUSED_VARS_NDEBUG(ptr_storage, len_dst, len_src, len_storage, opop);

  /* LightLinking is a special case, since you cannot edit/replace it, it's either existent or not.
   * Further more, when a lightlinking is added to the linked reference later on, the one created
   * for the liboverride needs to be 'merged', such that its overridable data is kept. */
  Object *ob_dst = blender::id_cast<Object *>(ptr_dst->owner_id);
  Object *ob_src = blender::id_cast<Object *>(ptr_src->owner_id);

  if (ob_dst->light_linking == nullptr && ob_src->light_linking == nullptr) {
    /* Nothing to do. */
    return false;
  }

  if (ob_dst->light_linking == nullptr && ob_src->light_linking != nullptr) {
    /* Copy light linking data from previous liboverride data into final liboverride one. */
    BKE_light_linking_copy(ob_dst, ob_src, 0);
    return true;
  }
  else if (ob_dst->light_linking != nullptr && ob_src->light_linking == nullptr) {
    /* Override has cleared/removed light linking data from its reference. */
    BKE_light_linking_delete(ob_dst, 0);
    return true;
  }
  else {
    BLI_assert(ob_dst->light_linking != nullptr && ob_src->light_linking != nullptr);
    /* Override had to create a light linking data, but now its reference also has one, need to
     * merge them by keeping the overridable data from the liboverride, while using the light
     * linking of the reference.
     *
     * Note that this case will not be encountered when the linked reference data already had
     * light linking data, since there will be no operation for the light linking pointer itself
     * then, only potentially for its internal overridable data (collections...). */

    /* For these collections, only replace linked data with previously defined liboverride data if
     * the latter is non-null. Otherwise, assume that the previously defined liboverride data
     * property was 'unset', and can be replaced by the linked reference value. */
    if (ob_src->light_linking->receiver_collection != nullptr) {
      id_us_min(blender::id_cast<ID *>(ob_dst->light_linking->receiver_collection));
      ob_dst->light_linking->receiver_collection = ob_src->light_linking->receiver_collection;
      id_us_plus(blender::id_cast<ID *>(ob_dst->light_linking->receiver_collection));
    }
    if (ob_src->light_linking->blocker_collection != nullptr) {
      id_us_min(blender::id_cast<ID *>(ob_dst->light_linking->blocker_collection));
      ob_dst->light_linking->blocker_collection = ob_src->light_linking->blocker_collection;
      id_us_plus(blender::id_cast<ID *>(ob_dst->light_linking->blocker_collection));
    }

    /* Note: LightLinking runtime data is currently set by depsgraph evaluation, so no need to
     * handle them here. */
  }

  DEG_id_tag_update(&ob_dst->id, ID_RECALC_SHADING);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, &ob_dst->id);
  return true;
}

static PointerRNA rna_LightLinking_receiver_collection_get(PointerRNA *ptr)
{
  Object *object = reinterpret_cast<Object *>(ptr->owner_id);
  PointerRNA collection_ptr = RNA_id_pointer_create(
      reinterpret_cast<ID *>(BKE_light_linking_collection_get(object, LIGHT_LINKING_RECEIVER)));
  return collection_ptr;
}

static void rna_LightLinking_receiver_collection_set(PointerRNA *ptr,
                                                     PointerRNA value,
                                                     ReportList * /*reports*/)
{
  Object *object = reinterpret_cast<Object *>(ptr->owner_id);
  Collection *new_collection = static_cast<Collection *>(value.data);

  BKE_light_linking_collection_assign_only(object, new_collection, LIGHT_LINKING_RECEIVER);
}

static PointerRNA rna_LightLinking_blocker_collection_get(PointerRNA *ptr)
{
  Object *object = reinterpret_cast<Object *>(ptr->owner_id);
  PointerRNA collection_ptr = RNA_id_pointer_create(
      reinterpret_cast<ID *>(BKE_light_linking_collection_get(object, LIGHT_LINKING_BLOCKER)));
  return collection_ptr;
}

static void rna_LightLinking_blocker_collection_set(PointerRNA *ptr,
                                                    PointerRNA value,
                                                    ReportList * /*reports*/)
{
  Object *object = reinterpret_cast<Object *>(ptr->owner_id);
  Collection *new_collection = static_cast<Collection *>(value.data);

  BKE_light_linking_collection_assign_only(object, new_collection, LIGHT_LINKING_BLOCKER);
}

static void rna_LightLinking_collection_update(Main *bmain, Scene * /*scene*/, PointerRNA *ptr)
{
  DEG_id_tag_update(ptr->owner_id, ID_RECALC_SHADING);

  DEG_relations_tag_update(bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ptr->owner_id);
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "VertexGroup", nullptr);
  RNA_def_struct_sdna(srna, "bDeformGroup");
  RNA_def_struct_ui_text(
      srna, "Vertex Group", "Group of vertices, used for armature deform and other purposes");
  RNA_def_struct_ui_icon(srna, ICON_GROUP_VERTEX);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Vertex group name");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_VertexGroup_name_set");
  /* update data because modifiers may use #24761. */
  RNA_def_property_update(
      prop, NC_GEOM | ND_DATA | NA_RENAME, "rna_Object_internal_update_data_dependency");

  prop = RNA_def_property(srna, "lock_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "", "Maintain the relative weights for the group");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", 0);
  /* update data because modifiers may use #24761. */
  RNA_def_property_update(prop, NC_GEOM | ND_DATA | NA_RENAME, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_VertexGroup_index_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Index", "Index number of the vertex group");

  func = RNA_def_function(srna, "add", "rna_VertexGroup_vertex_add");
  RNA_def_function_ui_description(func, "Add vertices to the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  /* TODO: see how array size of 0 works, this shouldn't be used. */
  parm = RNA_def_int_array(func, "index", 1, nullptr, 0, 0, "", "List of indices", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);
  parm = RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight", 0.0f, 1.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func, "type", assign_mode_items, 0, "", "Vertex assign mode");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "remove", "rna_VertexGroup_vertex_remove");
  RNA_def_function_ui_description(func, "Remove vertices from the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  /* TODO: see how array size of 0 works, this shouldn't be used. */
  parm = RNA_def_int_array(func, "index", 1, nullptr, 0, 0, "", "List of indices", 0, 0);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);

  func = RNA_def_function(srna, "weight", "rna_VertexGroup_weight");
  RNA_def_function_ui_description(func, "Get a vertex weight from the group");
  RNA_def_function_flag(func, FUNC_USE_REPORTS | FUNC_USE_SELF_ID);
  parm = RNA_def_int(func, "index", 0, 0, INT_MAX, "Index", "The index of the vertex", 0, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float(func, "weight", 0, 0.0f, 1.0f, "", "Vertex weight", 0.0f, 1.0f);
  RNA_def_function_return(func, parm);
}

static void rna_def_material_slot(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem link_items[] = {
      {1, "OBJECT", ICON_OBJECT_DATAMODE, "Object", ""},
      {0, "DATA", ICON_MESH_DATA, "Data", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* NOTE: there is no MaterialSlot equivalent in DNA, so the internal
   * pointer data points to ob->mat + index, and we manually implement
   * get/set for the properties. */

  srna = RNA_def_struct(brna, "MaterialSlot", nullptr);
  RNA_def_struct_ui_text(srna, "Material Slot", "Material slot in an object");
  RNA_def_struct_ui_icon(srna, ICON_MATERIAL_DATA);

  RNA_define_lib_overridable(true);

  /* WARNING! Order is crucial for override to work properly here... :/
   * 'link' must come before material pointer,
   * since it defines where (in object or obdata) that one is set! */
  prop = RNA_def_property(srna, "link", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, link_items);
  RNA_def_property_enum_funcs(
      prop, "rna_MaterialSlot_link_get", "rna_MaterialSlot_link_set", nullptr);
  RNA_def_property_ui_text(prop, "Link", "Link material to object or the object's data");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_MaterialSlot_update");

  prop = RNA_def_property(srna, "material", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_editable_func(prop, "rna_MaterialSlot_material_editable");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_MaterialSlot_material_get",
                                 "rna_MaterialSlot_material_set",
                                 nullptr,
                                 "rna_MaterialSlot_material_poll");
  RNA_def_property_ui_text(prop, "Material", "Material data-block used by this material slot");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_MaterialSlot_update");

  prop = RNA_def_property(srna, "slot_index", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_MaterialSlot_index_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_MaterialSlot_name_get", "rna_MaterialSlot_name_length", nullptr);
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
  srna = RNA_def_struct(brna, "ObjectConstraints", nullptr);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Object Constraints", "Collection of object constraints");

  /* Collection active property */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Constraint");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Object_active_constraint_get",
                                 "rna_Object_active_constraint_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Constraint", "Active Object constraint");

  /* Constraint collection */
  func = RNA_def_function(srna, "new", "rna_Object_constraints_new");
  RNA_def_function_ui_description(func, "Add a new constraint to this object");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  /* object to add */
  parm = RNA_def_enum(
      func, "type", rna_enum_constraint_type_items, 1, "", "Constraint type to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "constraint", "Constraint", "", "New constraint");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Object_constraints_remove");
  RNA_def_function_ui_description(func, "Remove a constraint from this object");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  /* constraint to remove */
  parm = RNA_def_pointer(func, "constraint", "Constraint", "", "Removed constraint");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_Object_constraints_clear");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  RNA_def_function_ui_description(func, "Remove all constraint from this object");

  func = RNA_def_function(srna, "move", "rna_Object_constraints_move");
  RNA_def_function_ui_description(func, "Move a constraint to a different position");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, INT_MIN, INT_MAX, "From Index", "Index to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "to_index", -1, INT_MIN, INT_MAX, "To Index", "Target index", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

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
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
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
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "ObjectModifiers");
  srna = RNA_def_struct(brna, "ObjectModifiers", nullptr);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Object Modifiers", "Collection of object modifiers");

#  if 0
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "EditBone");
  RNA_def_property_pointer_sdna(prop, nullptr, "act_edbone");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active EditBone", "Armatures active edit bone");
  // RNA_def_property_update(prop, 0, "rna_Armature_act_editbone_update");
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_Armature_act_edit_bone_set", nullptr, nullptr);

  /* TODO: redraw. */
  // RNA_def_property_collection_active(prop, prop_act);
#  endif

  /* add modifier */
  func = RNA_def_function(srna, "new", "rna_Object_modifier_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new modifier");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the modifier");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* modifier to add */
  parm = RNA_def_enum(
      func, "type", rna_enum_object_modifier_type_items, 1, "", "Modifier type to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
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
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  /* clear all modifiers */
  func = RNA_def_function(srna, "clear", "rna_Object_modifier_clear");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  RNA_def_function_ui_description(func, "Remove all modifiers from the object");

  /* move a modifier */
  func = RNA_def_function(srna, "move", "rna_Object_modifier_move");
  RNA_def_function_ui_description(func, "Move a modifier to a different position");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int(
      func, "from_index", -1, INT_MIN, INT_MAX, "From Index", "Index to move", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "to_index", -1, INT_MIN, INT_MAX, "To Index", "Target index", 0, 10000);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Active modifier. */
  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Modifier");
  RNA_def_property_pointer_funcs(
      prop, "rna_Object_active_modifier_get", "rna_Object_active_modifier_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Active Modifier", "The active modifier in the list");
  RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, nullptr);
}

/* object.shaderfxs */
static void rna_def_object_shaderfxs(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;

  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ObjectShaderFx");
  srna = RNA_def_struct(brna, "ObjectShaderFx", nullptr);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Object Shader Effects", "Collection of object effects");

  /* add shader_fx */
  func = RNA_def_function(srna, "new", "rna_Object_shaderfx_new");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new shader fx");
  parm = RNA_def_string(func, "name", "Name", 0, "", "New name for the effect");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* shader to add */
  parm = RNA_def_enum(
      func, "type", rna_enum_object_shaderfx_type_items, 1, "", "Effect type to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
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
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

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

  // FunctionRNA *func;
  // PropertyRNA *parm;

  RNA_def_property_srna(cprop, "ParticleSystems");
  srna = RNA_def_struct(brna, "ParticleSystems", nullptr);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Particle Systems", "Collection of particle systems");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_pointer_funcs(
      prop, "rna_Object_active_particle_system_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Active Particle System", "Active particle system being displayed");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

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
  srna = RNA_def_struct(brna, "VertexGroups", nullptr);
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_ui_text(srna, "Vertex Groups", "Collection of vertex groups");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "VertexGroup");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Object_active_vertex_group_get",
                                 "rna_Object_active_vertex_group_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Active Vertex Group", "Vertex groups of the object");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_vertex_groups_update");

  prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_vertex_group_index_get",
                             "rna_Object_active_vertex_group_index_set",
                             "rna_Object_active_vertex_group_index_range");
  RNA_def_property_ui_text(
      prop, "Active Vertex Group Index", "Active index in vertex group array");
  RNA_def_property_update(prop, NC_GEOM | ND_DATA, "rna_Object_vertex_groups_update");

  /* vertex groups */ /* add_vertex_group */
  func = RNA_def_function(srna, "new", "rna_Object_vgroup_new");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add vertex group to object");
  RNA_def_string(func, "name", "Group", 0, "", "Vertex group name"); /* optional */
  parm = RNA_def_pointer(func, "group", "VertexGroup", "", "New vertex group");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Object_vgroup_remove");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete vertex group from object");
  parm = RNA_def_pointer(func, "group", "VertexGroup", "", "Vertex group to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "clear", "rna_Object_vgroup_clear");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Delete all vertex groups from object");
}

static void rna_def_object_display(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ObjectDisplay", nullptr);
  RNA_def_struct_ui_text(srna, "Object Display", "Object display settings for 3D viewport");
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_nested(brna, srna, "Object");
  RNA_def_struct_path_func(srna, "rna_ObjectDisplay_path");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "show_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "dtx", OB_DRAW_NO_SHADOW_CAST);
  RNA_def_property_ui_text(prop, "Shadow", "Object cast shadows in the 3D viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  RNA_define_lib_overridable(false);
}

static void rna_def_object_lineart(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem prop_feature_line_usage_items[] = {
      {OBJECT_LRT_INHERIT, "INHERIT", 0, "Inherit", "Use settings from the parent collection"},
      {OBJECT_LRT_INCLUDE,
       "INCLUDE",
       0,
       "Include",
       "Generate feature lines for this object's data"},
      {OBJECT_LRT_OCCLUSION_ONLY,
       "OCCLUSION_ONLY",
       0,
       "Occlusion Only",
       "Only use the object data to produce occlusion"},
      {OBJECT_LRT_EXCLUDE,
       "EXCLUDE",
       0,
       "Exclude",
       "Don't use this object for Line Art rendering"},
      {OBJECT_LRT_INTERSECTION_ONLY,
       "INTERSECTION_ONLY",
       0,
       "Intersection Only",
       "Only generate intersection lines for this collection"},
      {OBJECT_LRT_NO_INTERSECTION,
       "NO_INTERSECTION",
       0,
       "No Intersection",
       "Include this object but do not generate intersection lines"},
      {OBJECT_LRT_FORCE_INTERSECTION,
       "FORCE_INTERSECTION",
       0,
       "Force Intersection",
       "Generate intersection lines even with objects that disabled intersection"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "ObjectLineArt", nullptr);
  RNA_def_struct_ui_text(srna, "Object Line Art", "Object Line Art settings");
  RNA_def_struct_sdna(srna, "ObjectLineArt");
  RNA_def_struct_path_func(srna, "rna_ObjectLineArt_path");

  prop = RNA_def_property(srna, "usage", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, prop_feature_line_usage_items);
  RNA_def_property_ui_text(prop, "Usage", "How to use this object in Line Art calculation");
  RNA_def_property_update(prop, 0, "rna_object_lineart_update");

  prop = RNA_def_property(srna, "use_crease_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", OBJECT_LRT_OWN_CREASE);
  RNA_def_property_ui_text(
      prop, "Use Crease", "Use this object's crease setting to overwrite scene global");
  RNA_def_property_update(prop, 0, "rna_object_lineart_update");

  prop = RNA_def_property(srna, "crease_threshold", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, 0, DEG2RAD(180.0f));
  RNA_def_property_ui_range(prop, 0.0f, DEG2RAD(180.0f), 0.01f, 1);
  RNA_def_property_ui_text(prop, "Crease", "Angles smaller than this will be treated as creases");
  RNA_def_property_update(prop, 0, "rna_object_lineart_update");

  prop = RNA_def_property(srna, "use_intersection_priority_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", OBJECT_LRT_OWN_INTERSECTION_PRIORITY);
  RNA_def_property_ui_text(
      prop,
      "Use Intersection Priority",
      "Use this object's intersection priority to override collection setting");
  RNA_def_property_update(prop, 0, "rna_object_lineart_update");

  prop = RNA_def_property(srna, "intersection_priority", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 255);
  RNA_def_property_ui_text(prop,
                           "Intersection Priority",
                           "The intersection line will be included into the object with the "
                           "higher intersection priority value");
  RNA_def_property_update(prop, NC_GPENCIL | ND_SHADING, "rna_object_lineart_update");
}

static void rna_def_object_visibility(StructRNA *srna)
{
  PropertyRNA *prop;

  /* Hide options. */
  prop = RNA_def_property(srna, "hide_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_VIEWPORT);
  RNA_def_property_ui_text(prop, "Disable in Viewports", "Globally disable in viewports");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, -1);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_SELECT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Disable Selection", "Disable selection in viewport");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, -1);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_RENDER);
  RNA_def_property_ui_text(prop, "Disable in Renders", "Globally disable in renders");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, -1);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "hide_probe_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_PROBE_VOLUME);
  RNA_def_property_ui_text(prop, "Disable in Volume Probes", "Globally disable in volume probes");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "hide_probe_sphere", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_PROBE_CUBEMAP);
  RNA_def_property_ui_text(
      prop, "Disable in Spherical Light Probes", "Globally disable in spherical light probes");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "hide_probe_plane", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_PROBE_PLANAR);
  RNA_def_property_ui_text(
      prop, "Disable in Planar Light Probes", "Globally disable in planar light probes");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "hide_surface_pick", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HIDE_SURFACE_PICK);
  RNA_def_property_ui_text(
      prop,
      "Disable in Surface Picking",
      "Disable surface influence during selection, snapping and depth-picking operators. "
      "Usually used to avoid semi-transparent objects to affect scene navigation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  /* Instancer options. */
  prop = RNA_def_property(srna, "show_instancer_for_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "duplicator_visibility_flag", OB_DUPLI_FLAG_RENDER);
  RNA_def_property_ui_text(prop, "Render Instancer", "Make instancer visible when rendering");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_DRAW, "rna_Object_duplicator_visibility_flag_update");

  prop = RNA_def_property(srna, "show_instancer_for_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "duplicator_visibility_flag", OB_DUPLI_FLAG_VIEWPORT);
  RNA_def_property_ui_text(prop, "Display Instancer", "Make instancer visible in the viewport");
  RNA_def_property_update(
      prop, NC_OBJECT | ND_DRAW, "rna_Object_duplicator_visibility_flag_update");

  /* Ray visibility. */
  prop = RNA_def_property(srna, "visible_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "visibility_flag", OB_HIDE_CAMERA);
  RNA_def_property_ui_text(prop, "Camera Visibility", "Object visibility to camera rays");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "visible_diffuse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "visibility_flag", OB_HIDE_DIFFUSE);
  RNA_def_property_ui_text(prop, "Diffuse Visibility", "Object visibility to diffuse rays");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "visible_glossy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "visibility_flag", OB_HIDE_GLOSSY);
  RNA_def_property_ui_text(prop, "Glossy Visibility", "Object visibility to glossy rays");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "visible_transmission", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "visibility_flag", OB_HIDE_TRANSMISSION);
  RNA_def_property_ui_text(
      prop, "Transmission Visibility", "Object visibility to transmission rays");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "visible_volume_scatter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "visibility_flag", OB_HIDE_VOLUME_SCATTER);
  RNA_def_property_ui_text(
      prop, "Volume Scatter Visibility", "Object visibility to volume scattering rays");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "visible_shadow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "visibility_flag", OB_HIDE_SHADOW);
  RNA_def_property_ui_text(prop, "Shadow Visibility", "Object visibility to shadow rays");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  /* Holdout and shadow catcher. */
  prop = RNA_def_property(srna, "is_holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_HOLDOUT);
  RNA_def_property_ui_text(
      prop,
      "Holdout",
      "Render objects as a holdout or matte, creating a hole in the image with zero alpha, to "
      "fill out in compositing with real footage or another render");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_hide_update");

  prop = RNA_def_property(srna, "is_shadow_catcher", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "visibility_flag", OB_SHADOW_CATCHER);
  RNA_def_property_ui_text(
      prop,
      "Shadow Catcher",
      "Only render shadows and reflections on this object, for compositing renders into real "
      "footage. Objects with this setting are considered to already exist in the footage, "
      "objects without it are synthetic objects being composited into it.");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");
}

static void rna_def_object(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem up_items[] = {
      {OB_POSX, "X", 0, "X", ""},
      {OB_POSY, "Y", 0, "Y", ""},
      {OB_POSZ, "Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem boundtype_items[] = {
      {OB_BOUND_BOX, "BOX", 0, "Box", "Display bounds as box"},
      {OB_BOUND_SPHERE, "SPHERE", 0, "Sphere", "Display bounds as sphere"},
      {OB_BOUND_CYLINDER, "CYLINDER", 0, "Cylinder", "Display bounds as cylinder"},
      {OB_BOUND_CONE, "CONE", 0, "Cone", "Display bounds as cone"},
      {OB_BOUND_CAPSULE, "CAPSULE", 0, "Capsule", "Display bounds as capsule"},
      {0, nullptr, 0, nullptr, nullptr},
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
      prop, "rna_Object_data_get", "rna_Object_data_set", "rna_Object_data_typef", nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_ui_text(prop, "Data", "Object data");
  RNA_def_property_update(prop, 0, "rna_Object_data_update");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, rna_enum_object_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Type", "Type of object");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_object_mode_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mode", "Object interaction mode");

  /* for data access */
  prop = RNA_def_property(srna, "bound_box", PROP_FLOAT, PROP_NONE);
  RNA_def_property_multi_array(prop, 2, boundbox_dimsize);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_float_funcs(prop, "rna_Object_boundbox_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "Bounding Box",
      "Object's bounding box in object-space coordinates, all values are -1.0 when "
      "not available");

  /* parent */
  prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Object_parent_set", nullptr, nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK);
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Object_parent_override_apply");
  RNA_def_property_ui_text(prop, "Parent", "Parent object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "parent_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "partype");
  RNA_def_property_enum_items(prop, parent_type_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_Object_parent_type_set", "rna_Object_parent_type_itemf");
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Object_parent_type_override_apply");
  RNA_def_property_ui_text(prop, "Parent Type", "Type of parent relation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "parent_vertices", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "par1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Parent Vertices", "Indices of vertices in case of a vertex parenting relation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "parsubstr");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_Object_parent_bone_set");
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Object_parent_bone_override_apply");
  RNA_def_property_ui_text(
      prop, "Parent Bone", "Name of parent bone in case of a bone parenting relation");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "use_parent_final_indices", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transflag", OB_PARENT_USE_FINAL_INDICES);
  RNA_def_property_ui_text(
      prop,
      "Use Final Indices",
      "Use the final evaluated indices rather than the original mesh indices");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "use_camera_lock_parent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "transflag", OB_TRANSFORM_ADJUST_ROOT_PARENT_FOR_VIEW_LOCK);
  RNA_def_property_ui_text(prop,
                           "Camera Parent Lock",
                           "View Lock 3D viewport camera transformation affects the object's "
                           "parent instead");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  /* Track and Up flags */
  /* XXX: these have been saved here for a bit longer (after old track was removed),
   *      since some other tools still refer to this */
  prop = RNA_def_property(srna, "track_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "trackflag");
  RNA_def_property_enum_items(prop, rna_enum_object_axis_items);
  RNA_def_property_ui_text(
      prop,
      "Track Axis",
      "Axis that points in the 'forward' direction (applies to Instance Vertices when "
      "Align to Vertex Normal is enabled)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "up_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "upflag");
  RNA_def_property_enum_items(prop, up_items);
  RNA_def_property_ui_text(
      prop,
      "Up Axis",
      "Axis that points in the upward direction (applies to Instance Vertices when "
      "Align to Vertex Normal is enabled)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  /* materials */
  prop = RNA_def_property(srna, "material_slots", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "MaterialSlot");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_PROP_NAME);
  /* Don't dereference the material slot pointer, it is the slot index encoded in a pointer. */
  RNA_def_property_collection_funcs(prop,
                                    "rna_Object_material_slots_begin",
                                    "rna_Object_material_slots_next",
                                    "rna_Object_material_slots_end",
                                    "rna_Object_material_slots_get",
                                    "rna_Object_material_slots_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Material Slots", "Material slots in the object");

  prop = RNA_def_property(srna, "active_material", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Material");
  RNA_def_property_pointer_funcs(prop,
                                 "rna_Object_active_material_get",
                                 "rna_Object_active_material_set",
                                 nullptr,
                                 "rna_MaterialSlot_material_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_editable_func(prop, "rna_Object_active_material_editable");
  RNA_def_property_ui_text(prop, "Active Material", "Active material being displayed");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_MaterialSlot_update");

  prop = RNA_def_property(srna, "active_material_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "actcol");
  RNA_def_property_flag(prop, PROP_NO_DEG_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_material_index_get",
                             "rna_Object_active_material_index_set",
                             "rna_Object_active_material_index_range");
  RNA_def_property_ui_text(prop, "Active Material Index", "Index of active material slot");
  RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING_LINKS, nullptr);

  /* transform */
  prop = RNA_def_property(srna, "location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "loc");
  RNA_def_property_editable_array_func(prop, "rna_Object_location_editable");
  RNA_def_property_ui_text(prop, "Location", "Location of the object");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, nullptr, "quat");
  RNA_def_property_editable_array_func(prop, "rna_Object_rotation_4d_editable");
  RNA_def_property_ui_text(prop, "Quaternion Rotation", "Rotation in Quaternions");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* XXX: for axis-angle, it would have been nice to have 2 separate fields for UI purposes, but
   * having a single one is better for Keyframing and other property-management situations...
   */
  prop = RNA_def_property(srna, "rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(
      prop, "rna_Object_rotation_axis_angle_get", "rna_Object_rotation_axis_angle_set", nullptr);
  RNA_def_property_editable_array_func(prop, "rna_Object_rotation_4d_editable");
  RNA_def_property_float_array_default(prop, rna_default_axis_angle);
  RNA_def_property_ui_text(
      prop, "Axis-Angle Rotation", "Angle of Rotation for Axis-Angle rotation representation");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "rot");
  RNA_def_property_editable_array_func(prop, "rna_Object_rotation_euler_editable");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop, "Euler Rotation", "Rotation in Eulers");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "rotation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "rotmode");
  RNA_def_property_enum_items(prop, rna_enum_object_rotation_mode_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Object_rotation_mode_set", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Rotation Mode",
      /* This description is shared by other "rotation_mode" properties. */
      "The kind of rotation to apply, values from other rotation modes are not used");
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
      prop, "rna_Object_dimensions_get", "rna_Object_dimensions_set", nullptr);
  RNA_def_property_ui_range(prop, 0.0f, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_ui_text(prop,
                           "Dimensions",
                           "Absolute bounding box dimensions of the object.\n"
                           "Warning: Assigning to it or its members multiple consecutive times "
                           "will not work correctly, as this needs up-to-date evaluated data");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* delta transforms */
  prop = RNA_def_property(srna, "delta_location", PROP_FLOAT, PROP_TRANSLATION);
  RNA_def_property_float_sdna(prop, nullptr, "dloc");
  RNA_def_property_ui_text(
      prop, "Delta Location", "Extra translation added to the location of the object");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "delta_rotation_euler", PROP_FLOAT, PROP_EULER);
  RNA_def_property_float_sdna(prop, nullptr, "drot");
  RNA_def_property_ui_text(
      prop,
      "Delta Rotation (Euler)",
      "Extra rotation added to the rotation of the object (when using Euler rotations)");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 100, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "delta_rotation_quaternion", PROP_FLOAT, PROP_QUATERNION);
  RNA_def_property_float_sdna(prop, nullptr, "dquat");
  RNA_def_property_ui_text(
      prop,
      "Delta Rotation (Quaternion)",
      "Extra rotation added to the rotation of the object (when using Quaternion rotations)");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

#  if 0 /* XXX not supported well yet... */
  prop = RNA_def_property(srna, "delta_rotation_axis_angle", PROP_FLOAT, PROP_AXISANGLE);
  /* FIXME: this is not a single field any more! (drotAxis and drotAngle) */
  RNA_def_property_float_sdna(prop, nullptr, "dquat");
  RNA_def_property_float_array_default(prop, rna_default_axis_angle);
  RNA_def_property_ui_text(
      prop,
      "Delta Rotation (Axis Angle)",
      "Extra rotation added to the rotation of the object (when using Axis-Angle rotations)");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");
#  endif

  prop = RNA_def_property(srna, "delta_scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "dscale");
  RNA_def_property_flag(prop, PROP_PROPORTIONAL);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 1, 3);
  RNA_def_property_ui_text(prop, "Delta Scale", "Extra scaling added to the scale of the object");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* transform locks */
  prop = RNA_def_property(srna, "lock_location", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_bitset_array_sdna(prop, nullptr, "protectflag", OB_LOCK_LOCX, 3);
  RNA_def_property_ui_text(prop, "Lock Location", "Lock editing of location when transforming");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_bitset_array_sdna(prop, nullptr, "protectflag", OB_LOCK_ROTX, 3);
  RNA_def_property_ui_text(prop, "Lock Rotation", "Lock editing of rotation when transforming");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* XXX this is sub-optimal - it really should be included above,
   *     but due to technical reasons we can't do this! */
  prop = RNA_def_property(srna, "lock_rotation_w", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "protectflag", OB_LOCK_ROTW);
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_ui_text(
      prop,
      "Lock Rotation (4D Angle)",
      "Lock editing of 'angle' component of four-component rotations when transforming");
  /* XXX this needs a better name */
  prop = RNA_def_property(srna, "lock_rotations_4d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "protectflag", OB_LOCK_ROT4D);
  RNA_def_property_ui_text(
      prop,
      "Lock Rotations (4D)",
      "Lock editing of four component rotations by components (instead of as Eulers)");

  prop = RNA_def_property(srna, "lock_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_bitset_array_sdna(prop, nullptr, "protectflag", OB_LOCK_SCALEX, 3);
  RNA_def_property_ui_text(prop, "Lock Scale", "Lock editing of scale when transforming");
  RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* matrix */
  prop = RNA_def_property(srna, "matrix_world", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_float_funcs(
      prop, "rna_Object_matrix_world_get", "rna_Object_matrix_world_set", nullptr);
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
      "Parent relative transformation matrix.\n"
      "Warning: Only takes into account object parenting, so e.g. in case of bone parenting "
      "you get a matrix relative to the Armature object, not to the actual parent bone");
  RNA_def_property_float_funcs(
      prop, "rna_Object_matrix_local_get", "rna_Object_matrix_local_set", nullptr);
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
      prop, "rna_Object_matrix_basis_get", "rna_Object_matrix_basis_set", nullptr);
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* Parent_inverse. */
  prop = RNA_def_property(srna, "matrix_parent_inverse", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "parentinv");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Parent Inverse Matrix", "Inverse of object's parent matrix at time of parenting");
  RNA_def_property_update(prop, NC_OBJECT | ND_TRANSFORM, "rna_Object_internal_update");

  /* modifiers */
  prop = RNA_def_property(srna, "modifiers", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "Modifier");
  RNA_def_property_ui_text(
      prop, "Modifiers", "Modifiers affecting the geometric data of the object");
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Object_modifiers_override_apply");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_LIBRARY_INSERTION);
  rna_def_object_modifiers(brna, prop);

  /* Shader FX. */
  prop = RNA_def_property(srna, "shader_effects", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "shader_fx", nullptr);
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
  RNA_def_property_override_funcs(prop, nullptr, nullptr, "rna_Object_constraints_override_apply");
#  if 0
  RNA_def_property_collection_funcs(prop,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "constraints__add",
                                    "constraints__remove");
#  endif
  rna_def_object_constraints(brna, prop);

  /* vertex groups */
  prop = RNA_def_property(srna, "vertex_groups", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_funcs(prop,
                                    "rna_Object_vertex_groups_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_struct_type(prop, "VertexGroup");
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_ui_text(prop, "Vertex Groups", "Vertex groups of the object");
  rna_def_object_vertex_groups(brna, prop);

  /* empty */
  prop = RNA_def_property(srna, "empty_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "empty_drawtype");
  RNA_def_property_enum_items(prop, rna_enum_object_empty_drawtype_items);
  RNA_def_property_enum_funcs(prop, nullptr, "rna_Object_empty_display_type_set", nullptr);
  RNA_def_property_ui_text(prop, "Empty Display Type", "Viewport display style for empties");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "empty_display_size", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "empty_drawsize");
  RNA_def_property_range(prop, 0.0001f, 1000.0f);
  RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
  RNA_def_property_ui_text(
      prop, "Empty Display Size", "Size of display for empties in the viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "empty_image_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "ima_ofs");
  RNA_def_property_ui_text(prop, "Origin Offset", "Origin offset distance");
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 0.1f, 2);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "iuser");
  RNA_def_property_ui_text(
      prop,
      "Image User",
      "Parameters defining which layer, pass and frame of the image is displayed");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "empty_image_depth", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_object_empty_image_depth_items);
  RNA_def_property_ui_text(
      prop, "Empty Image Depth", "Determine which other objects will occlude the image");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_empty_image_perspective", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "empty_image_visibility_flag", OB_EMPTY_IMAGE_HIDE_PERSPECTIVE);
  RNA_def_property_ui_text(
      prop, "Display in Perspective Mode", "Display image in perspective mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_empty_image_orthographic", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "empty_image_visibility_flag", OB_EMPTY_IMAGE_HIDE_ORTHOGRAPHIC);
  RNA_def_property_ui_text(
      prop, "Display in Orthographic Mode", "Display image in orthographic mode");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_empty_image_only_axis_aligned", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "empty_image_visibility_flag", OB_EMPTY_IMAGE_HIDE_NON_AXIS_ALIGNED);
  RNA_def_property_ui_text(prop,
                           "Display Only Axis Aligned",
                           "Only display the image when it is aligned with the view axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "use_empty_image_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "empty_image_flag", OB_EMPTY_IMAGE_USE_ALPHA_BLEND);
  RNA_def_property_ui_text(
      prop,
      "Use Alpha",
      "Use alpha blending instead of alpha test (can produce sorting artifacts)");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  static const EnumPropertyItem prop_empty_image_side_items[] = {
      {0, "DOUBLE_SIDED", 0, "Both", ""},
      {OB_EMPTY_IMAGE_HIDE_BACK, "FRONT", 0, "Front", ""},
      {OB_EMPTY_IMAGE_HIDE_FRONT, "BACK", 0, "Back", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  prop = RNA_def_property(srna, "empty_image_side", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "empty_image_visibility_flag");
  RNA_def_property_enum_items(prop, prop_empty_image_side_items);
  RNA_def_property_ui_text(prop, "Empty Image Side", "Show front/back side");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_IMAGE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "add_rest_position_attribute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "modifier_flag", OB_MODIFIER_FLAG_ADD_REST_POSITION);
  RNA_def_property_ui_text(prop,
                           "Add Rest Position",
                           "Add a \"rest_position\" attribute that is a copy of the position "
                           "attribute before shape keys and modifiers are evaluated");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_data");

  /* render */
  prop = RNA_def_property(srna, "pass_index", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, nullptr, "index");
  RNA_def_property_ui_text(
      prop, "Pass Index", "Index number for the \"Object Index\" render pass");
  RNA_def_property_update(prop, NC_OBJECT, "rna_Object_internal_update_draw");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_ui_text(
      prop, "Color", "Object color and alpha, used when the Object Color mode is enabled");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update_draw");

  /* physics */
  prop = RNA_def_property(srna, "field", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "pd");
  RNA_def_property_struct_type(prop, "FieldSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Object_field_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Field Settings", "Settings for using the object as a field in physics simulation");

  prop = RNA_def_property(srna, "collision", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "pd");
  RNA_def_property_struct_type(prop, "CollisionSettings");
  RNA_def_property_pointer_funcs(prop, "rna_Object_collision_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Collision Settings",
                           "Settings for using the object as a collider in physics simulation");

  prop = RNA_def_property(srna, "soft_body", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "soft");
  RNA_def_property_struct_type(prop, "SoftBodySettings");
  RNA_def_property_ui_text(prop, "Soft Body Settings", "Settings for soft body simulation");

  prop = RNA_def_property(srna, "particle_systems", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "particlesystem", nullptr);
  RNA_def_property_struct_type(prop, "ParticleSystem");
  RNA_def_property_ui_text(prop, "Particle Systems", "Particle systems emitted from the object");
  rna_def_object_particle_systems(brna, prop);

  prop = RNA_def_property(srna, "rigid_body", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "rigidbody_object");
  RNA_def_property_struct_type(prop, "RigidBodyObject");
  RNA_def_property_ui_text(prop, "Rigid Body Settings", "Settings for rigid body simulation");

  prop = RNA_def_property(srna, "rigid_body_constraint", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "rigidbody_constraint");
  RNA_def_property_struct_type(prop, "RigidBodyConstraint");
  RNA_def_property_ui_text(prop, "Rigid Body Constraint", "Constraint constraining rigid bodies");

  prop = RNA_def_property(srna, "use_simulation_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", OB_FLAG_USE_SIMULATION_CACHE);
  RNA_def_property_ui_text(
      prop, "Use Simulation Cache", "Cache frames during simulation nodes playback");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  rna_def_object_visibility(srna);

  /* instancing */
  prop = RNA_def_property(srna, "instance_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "transflag");
  RNA_def_property_enum_items(prop, instance_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_Object_instance_type_itemf");
  RNA_def_property_ui_text(prop, "Instance Type", "If not None, object instancing method to use");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "use_instance_vertices_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transflag", OB_DUPLIROT);
  RNA_def_property_ui_text(
      prop, "Orient with Normals", "Rotate instance according to vertex normal");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "use_instance_faces_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transflag", OB_DUPLIFACES_SCALE);
  RNA_def_property_ui_text(prop, "Scale to Face Sizes", "Scale instance based on face size");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "instance_faces_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "instance_faces_scale");
  RNA_def_property_range(prop, 0.001f, 10000.0f);
  RNA_def_property_ui_text(prop, "Instance Faces Scale", "Scale the face instance objects");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "instance_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_pointer_sdna(prop, nullptr, "instance_collection");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_Object_dup_collection_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Instance Collection", "Instance an existing collection");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_dependency_update");

  prop = RNA_def_property(srna, "is_instancer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "transflag", OB_DUPLI);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* drawing */
  prop = RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dt");
  RNA_def_property_enum_items(prop, drawtype_items);
  RNA_def_property_ui_text(prop, "Display As", "How to display object in viewport");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_Object_internal_update");

  prop = RNA_def_property(srna, "show_bounds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_DRAWBOUNDOX);
  RNA_def_property_ui_text(prop, "Display Bounds", "Display the object's bounds");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "display_bounds_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "boundtype");
  RNA_def_property_enum_items(prop, boundtype_items);
  RNA_def_property_ui_text(prop, "Display Bounds Type", "Object boundary display type");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_DRAWNAME);
  RNA_def_property_ui_text(prop, "Display Name", "Display the object's name");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_AXIS);
  RNA_def_property_ui_text(prop, "Display Axes", "Display the object's origin and axes");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_texture_space", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_TEXSPACE);
  RNA_def_property_ui_text(prop, "Display Texture Space", "Display the object's texture space");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_DRAWWIRE);
  RNA_def_property_ui_text(
      prop, "Display Wire", "Display the object's wireframe over solid shading");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_all_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_DRAW_ALL_EDGES);
  RNA_def_property_ui_text(prop, "Display All Edges", "Display all edges for mesh objects");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "use_grease_pencil_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_USE_GPENCIL_LIGHTS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Use Lights", "Lights affect Grease Pencil object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_grease_pencil_update");

  prop = RNA_def_property(srna, "show_transparent", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_DRAWTRANSP);
  RNA_def_property_ui_text(
      prop, "Display Transparent", "Display material transparency in the object");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "show_in_front", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "dtx", OB_DRAW_IN_FRONT);
  RNA_def_property_ui_text(prop, "In Front", "Make the object display in front of others");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_grease_pencil_update");

  /* pose */
  prop = RNA_def_property(srna, "pose", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "pose");
  RNA_def_property_struct_type(prop, "Pose");
  RNA_def_property_ui_text(prop, "Pose", "Current pose for armatures");

  /* shape keys */
  prop = RNA_def_property(srna, "show_only_shape_key", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "shapeflag", OB_SHAPE_LOCK);
  RNA_def_property_ui_text(
      prop, "Solo Active Shape Key", "Only show the active shape key at full value");
  RNA_def_property_ui_icon(prop, ICON_SOLO_OFF, 1);
  RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "use_shape_key_edit_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "shapeflag", OB_SHAPE_EDIT_MODE);
  RNA_def_property_ui_text(
      prop, "Shape Key Edit Mode", "Display shape keys in edit mode (for meshes only)");
  RNA_def_property_ui_icon(prop, ICON_EDITMODE_HLT, 0);
  RNA_def_property_update(prop, 0, "rna_Object_internal_update_data");

  prop = RNA_def_property(srna, "active_shape_key", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ShapeKey");
  RNA_def_property_override_flag(prop, PROPOVERRIDE_IGNORE | PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_pointer_funcs(
      prop, "rna_Object_active_shape_key_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Shape Key", "Current shape key");

  prop = RNA_def_property(srna, "active_shape_key_index", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "shapenr");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE); /* XXX this is really unpredictable... */
  RNA_def_property_int_funcs(prop,
                             "rna_Object_active_shape_key_index_get",
                             "rna_Object_active_shape_key_index_set",
                             "rna_Object_active_shape_key_index_range");
  RNA_def_property_ui_text(prop, "Active Shape Key Index", "Current shape key index");
  RNA_def_property_update(prop, 0, "rna_Object_active_shape_update");

  /* sculpt */
  prop = RNA_def_property(srna, "use_dynamic_topology_sculpting", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Object_use_dynamic_topology_sculpting_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Dynamic Topology Sculpting", nullptr);

  /* Base Settings */
  prop = RNA_def_property(srna, "is_from_instancer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "base_flag", BASE_FROM_DUPLI);
  RNA_def_property_ui_text(prop, "Base from Instancer", "Object comes from a instancer");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  prop = RNA_def_property(srna, "is_from_set", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "base_flag", BASE_FROM_SET);
  RNA_def_property_ui_text(prop, "Base from Set", "Object comes from a background set");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_NO_COMPARISON);
  RNA_def_property_override_clear_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);

  /* Object Display */
  prop = RNA_def_property(srna, "display", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ObjectDisplay");
  RNA_def_property_pointer_funcs(prop, "rna_Object_display_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Object Display", "Object display settings for 3D viewport");

  /* Line Art */
  prop = RNA_def_property(srna, "lineart", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "ObjectLineArt");
  RNA_def_property_ui_text(prop, "Line Art", "Line Art settings for the object");

  /* Mesh Symmetry Settings */

  prop = RNA_def_property(srna, "use_mesh_mirror_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Object_mesh_symmetry_x_get", "rna_Object_mesh_symmetry_x_set");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "X", "Enable mesh symmetry in the X axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "use_mesh_mirror_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Object_mesh_symmetry_y_get", "rna_Object_mesh_symmetry_y_set");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_Object_mesh_symmetry_yz_editable");
  RNA_def_property_ui_text(prop, "Y", "Enable mesh symmetry in the Y axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "use_mesh_mirror_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Object_mesh_symmetry_z_get", "rna_Object_mesh_symmetry_z_set");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_editable_func(prop, "rna_Object_mesh_symmetry_yz_editable");
  RNA_def_property_ui_text(prop, "Z", "Enable mesh symmetry in the Z axis");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  /* Lightgroup Membership */
  prop = RNA_def_property(srna, "lightgroup", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_Object_lightgroup_get",
                                "rna_Object_lightgroup_length",
                                "rna_Object_lightgroup_set");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Lightgroup", "Lightgroup that the object belongs to");

  /* Light Linking. */
  prop = RNA_def_property(srna, "light_linking", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ObjectLightLinking");
  RNA_def_property_pointer_funcs(prop, "rna_Object_light_linking_get", nullptr, nullptr, nullptr);
  RNA_def_property_override_flag(prop, PROPOVERRIDE_OVERRIDABLE_LIBRARY);
  RNA_def_property_override_funcs(
      prop, nullptr, nullptr, "rna_Object_light_linking_override_apply");
  RNA_def_property_ui_text(prop, "Light Linking", "Light linking settings");

  /* Shadow terminator. */
  prop = RNA_def_property(srna, "shadow_terminator_normal_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 10, 0.01f, 4);
  RNA_def_property_ui_text(
      prop,
      "Shadow Terminator Normal Offset",
      "Offset rays from the surface to reduce shadow terminator artifact on low poly geometry. "
      "Only affect triangles that are affected by the geometry offset");

  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "shadow_terminator_geometry_offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 2);
  RNA_def_property_ui_text(prop,
                           "Shadow Terminator Geometry Offset",
                           "Offset rays from the surface to reduce shadow terminator artifact on "
                           "low poly geometry. Only affects triangles at grazing angles to light");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  prop = RNA_def_property(srna, "shadow_terminator_shading_offset", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0, 1.0, 1, 2);
  RNA_def_property_ui_text(
      prop,
      "Shadow Terminator Shading Offset",
      "Push the shadow terminator towards the light to hide artifacts on low poly geometry");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, nullptr);

  RNA_define_lib_overridable(false);

  /* anim */
  rna_def_animdata_common(srna);

  rna_def_animviz_common(srna);
  rna_def_motionpath_common(srna);

  RNA_api_object(srna);
}

static void rna_def_object_light_linking(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ObjectLightLinking", nullptr);
  RNA_def_struct_ui_text(srna, "Object Light Linking", "");
  RNA_def_struct_sdna(srna, "Object");
  RNA_def_struct_nested(brna, srna, "Object");
  RNA_def_struct_path_func(srna, "rna_ObjectLightLinking_path");

  RNA_define_lib_overridable(true);

  prop = RNA_def_property(srna, "receiver_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_LightLinking_receiver_collection_get",
                                 "rna_LightLinking_receiver_collection_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(prop,
                           "Receiver Collection",
                           "Collection which defines light linking relation of this emitter");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_LightLinking_collection_update");

  prop = RNA_def_property(srna, "blocker_collection", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Collection");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_pointer_funcs(prop,
                                 "rna_LightLinking_blocker_collection_get",
                                 "rna_LightLinking_blocker_collection_set",
                                 nullptr,
                                 nullptr);
  RNA_def_property_ui_text(prop,
                           "Blocker Collection",
                           "Collection which defines objects which block light from this emitter");
  RNA_def_property_update(prop, NC_OBJECT | ND_DRAW, "rna_LightLinking_collection_update");

  RNA_define_lib_overridable(false);
}

void RNA_def_object(BlenderRNA *brna)
{
  rna_def_object(brna);

  RNA_define_animate_sdna(false);
  rna_def_vertex_group(brna);
  rna_def_material_slot(brna);
  rna_def_object_display(brna);
  rna_def_object_lineart(brna);
  rna_def_object_light_linking(brna);
  RNA_define_animate_sdna(true);
}

#endif

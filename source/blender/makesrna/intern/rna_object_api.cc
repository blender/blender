/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "BLI_kdopbvh.hh"
#include "BLI_math_geom.h"

#include "RNA_define.hh"

#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"

#include "ED_outliner.hh"

#include "rna_internal.hh" /* own include */

#define MESH_DM_INFO_STR_MAX 16384

static const EnumPropertyItem space_items[] = {
    {CONSTRAINT_SPACE_WORLD, "WORLD", 0, "World Space", "The most global space in Blender"},
    {CONSTRAINT_SPACE_POSE,
     "POSE",
     0,
     "Pose Space",
     "The pose space of a bone (its armature's object space)"},
    {CONSTRAINT_SPACE_PARLOCAL,
     "LOCAL_WITH_PARENT",
     0,
     "Local With Parent",
     "The rest pose local space of a bone (this matrix includes parent transforms)"},
    {CONSTRAINT_SPACE_LOCAL, "LOCAL", 0, "Local Space", "The local space of an object/bone"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME

#  include "BKE_bvhutils.hh"
#  include "BKE_constraint.h"
#  include "BKE_context.hh"
#  include "BKE_crazyspace.hh"
#  include "BKE_customdata.hh"
#  include "BKE_global.hh"
#  include "BKE_layer.hh"
#  include "BKE_main.hh"
#  include "BKE_mball.hh"
#  include "BKE_mesh.hh"
#  include "BKE_mesh_runtime.hh"
#  include "BKE_modifier.hh"
#  include "BKE_object.hh"
#  include "BKE_object_types.hh"
#  include "BKE_report.hh"
#  include "BKE_vfont.hh"

#  include "ED_object.hh"
#  include "ED_screen.hh"

#  include "DNA_curve_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_view3d_types.h"

#  include "DEG_depsgraph_query.hh"

#  include "MEM_guardedalloc.h"

static Base *find_view_layer_base_with_synced_ensure(
    Object *ob, bContext *C, PointerRNA *view_layer_ptr, Scene **r_scene, ViewLayer **r_view_layer)
{
  Scene *scene;
  ViewLayer *view_layer;
  if (view_layer_ptr->data) {
    scene = (Scene *)view_layer_ptr->owner_id;
    view_layer = static_cast<ViewLayer *>(view_layer_ptr->data);
  }
  else {
    scene = CTX_data_scene(C);
    view_layer = CTX_data_view_layer(C);
  }
  if (r_scene != nullptr) {
    *r_scene = scene;
  }
  if (r_view_layer != nullptr) {
    *r_view_layer = view_layer;
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  return BKE_view_layer_base_find(view_layer, ob);
}

static void rna_Object_select_set(
    Object *ob, bContext *C, ReportList *reports, bool select, PointerRNA *view_layer_ptr)
{
  Scene *scene;
  ViewLayer *view_layer;
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, &scene, &view_layer);

  if (!base) {
    if (select) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Object '%s' cannot be selected because it is not in View Layer '%s'!",
                  ob->id.name + 2,
                  view_layer->name);
    }
    return;
  }

  blender::ed::object::base_select(
      base, select ? blender::ed::object::BA_SELECT : blender::ed::object::BA_DESELECT);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);
  ED_outliner_select_sync_from_object_tag(C);
}

static bool rna_Object_select_get(Object *ob, bContext *C, PointerRNA *view_layer_ptr)
{
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, nullptr, nullptr);
  if (!base) {
    return false;
  }

  return ((base->flag & BASE_SELECTED) != 0);
}

static void rna_Object_hide_set(
    Object *ob, bContext *C, ReportList *reports, bool hide, PointerRNA *view_layer_ptr)
{
  Scene *scene;
  ViewLayer *view_layer;
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, &scene, &view_layer);
  if (!base) {
    if (hide) {
      BKE_reportf(reports,
                  RPT_ERROR,
                  "Object '%s' cannot be hidden because it is not in View Layer '%s'!",
                  ob->id.name + 2,
                  view_layer->name);
    }
    return;
  }

  if (hide) {
    base->flag |= BASE_HIDDEN;
  }
  else {
    base->flag &= ~BASE_HIDDEN;
  }

  BKE_view_layer_need_resync_tag(view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
}

static bool rna_Object_hide_get(Object *ob, bContext *C, PointerRNA *view_layer_ptr)
{
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, nullptr, nullptr);
  if (!base) {
    return false;
  }

  return ((base->flag & BASE_HIDDEN) != 0);
}

static bool rna_Object_visible_get(Object *ob,
                                   bContext *C,
                                   PointerRNA *view_layer_ptr,
                                   View3D *v3d)
{
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, nullptr, nullptr);
  if (v3d == nullptr) {
    v3d = CTX_wm_view3d(C);
  }

  if (!base) {
    return false;
  }

  return BASE_VISIBLE(v3d, base);
}

static bool rna_Object_holdout_get(Object *ob, bContext *C, PointerRNA *view_layer_ptr)
{
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, nullptr, nullptr);
  if (!base) {
    return false;
  }

  return ((base->flag & BASE_HOLDOUT) != 0) || ((ob->visibility_flag & OB_HOLDOUT) != 0);
}

static bool rna_Object_indirect_only_get(Object *ob, bContext *C, PointerRNA *view_layer_ptr)
{
  Base *base = find_view_layer_base_with_synced_ensure(ob, C, view_layer_ptr, nullptr, nullptr);
  if (!base) {
    return false;
  }

  return ((base->flag & BASE_INDIRECT_ONLY) != 0);
}

static Base *rna_Object_local_view_property_helper(bScreen *screen,
                                                   View3D *v3d,
                                                   ViewLayer *view_layer,
                                                   Object *ob,
                                                   ReportList *reports,
                                                   Scene **r_scene)
{
  wmWindow *win = nullptr;
  if (v3d->localvd == nullptr) {
    BKE_report(reports, RPT_ERROR, "Viewport not in local view");
    return nullptr;
  }

  if (view_layer == nullptr) {
    win = ED_screen_window_find(screen, static_cast<wmWindowManager *>(G_MAIN->wm.first));
    view_layer = WM_window_get_active_view_layer(win);
  }

  BKE_view_layer_synced_ensure(win ? WM_window_get_active_scene(win) : nullptr, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);
  if (base == nullptr) {
    BKE_reportf(
        reports, RPT_WARNING, "Object %s not in view layer %s", ob->id.name + 2, view_layer->name);
  }
  if (r_scene != nullptr && win != nullptr) {
    *r_scene = win->scene;
  }
  return base;
}

static bool rna_Object_local_view_get(Object *ob, ReportList *reports, View3D *v3d)
{
  if (v3d->localvd == nullptr) {
    BKE_report(reports, RPT_ERROR, "Viewport not in local view");
    return false;
  }

  return ((ob->base_local_view_bits & v3d->local_view_uid) != 0);
}

static void rna_Object_local_view_set(Object *ob,
                                      ReportList *reports,
                                      PointerRNA *v3d_ptr,
                                      bool state)
{
  bScreen *screen = (bScreen *)v3d_ptr->owner_id;
  View3D *v3d = static_cast<View3D *>(v3d_ptr->data);
  Scene *scene;
  Base *base = rna_Object_local_view_property_helper(screen, v3d, nullptr, ob, reports, &scene);
  if (base == nullptr) {
    return; /* Error reported. */
  }
  const short local_view_bits_prev = base->local_view_bits;
  SET_FLAG_FROM_TEST(base->local_view_bits, state, v3d->local_view_uid);
  if (local_view_bits_prev != base->local_view_bits) {
    DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
    ScrArea *area = ED_screen_area_find_with_spacedata(screen, (SpaceLink *)v3d, true);
    if (area) {
      ED_area_tag_redraw(area);
    }
  }
}

static bool rna_Object_visible_in_viewport_get(Object *ob, View3D *v3d)
{
  return BKE_object_is_visible_in_viewport(v3d, ob);
}

/* Convert a given matrix from a space to another (using the object and/or a bone as
 * reference). */
static void rna_Object_mat_convert_space(Object *ob,
                                         ReportList *reports,
                                         bPoseChannel *pchan,
                                         const float mat[16],
                                         float mat_ret[16],
                                         int from,
                                         int to)
{
  copy_m4_m4((float (*)[4])mat_ret, (float (*)[4])mat);

  BLI_assert(!ELEM(from, CONSTRAINT_SPACE_OWNLOCAL));
  BLI_assert(!ELEM(to, CONSTRAINT_SPACE_OWNLOCAL));

  /* Error in case of invalid from/to values when pchan is nullptr */
  if (pchan == nullptr) {
    if (ELEM(from, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_PARLOCAL)) {
      const char *identifier = nullptr;
      RNA_enum_identifier(space_items, from, &identifier);
      BKE_reportf(reports,
                  RPT_ERROR,
                  "'from_space' '%s' is invalid when no pose bone is given!",
                  identifier);
      return;
    }
    if (ELEM(to, CONSTRAINT_SPACE_POSE, CONSTRAINT_SPACE_PARLOCAL)) {
      const char *identifier = nullptr;
      RNA_enum_identifier(space_items, to, &identifier);
      BKE_reportf(reports,
                  RPT_ERROR,
                  "'to_space' '%s' is invalid when no pose bone is given!",
                  identifier);
      return;
    }
  }
  /* These checks are extra security, they should never occur. */
  if (from == CONSTRAINT_SPACE_CUSTOM) {
    const char *identifier = nullptr;
    RNA_enum_identifier(space_items, from, &identifier);
    BKE_reportf(reports,
                RPT_ERROR,
                "'from_space' '%s' is invalid when no custom space is given!",
                identifier);
    return;
  }
  if (to == CONSTRAINT_SPACE_CUSTOM) {
    const char *identifier = nullptr;
    RNA_enum_identifier(space_items, to, &identifier);
    BKE_reportf(reports,
                RPT_ERROR,
                "'to_space' '%s' is invalid when no custom space is given!",
                identifier);
    return;
  }

  BKE_constraint_mat_convertspace(ob, pchan, nullptr, (float (*)[4])mat_ret, from, to, false);
}

static void rna_Object_calc_matrix_camera(Object *ob,
                                          Depsgraph *depsgraph,
                                          float mat_ret[16],
                                          int width,
                                          int height,
                                          float scalex,
                                          float scaley)
{
  const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  CameraParams params;

  /* setup parameters */
  BKE_camera_params_init(&params);
  BKE_camera_params_from_object(&params, ob_eval);

  /* Compute matrix, view-plane, etc. */
  BKE_camera_params_compute_viewplane(&params, width, height, scalex, scaley);
  BKE_camera_params_compute_matrix(&params);

  copy_m4_m4((float (*)[4])mat_ret, params.winmat);
}

static void rna_Object_camera_fit_coords(Object *ob,
                                         Depsgraph *depsgraph,
                                         const float *cos,
                                         int cos_num,
                                         float co_ret[3],
                                         float *scale_ret)
{
  BKE_camera_view_frame_fit_to_coords(
      depsgraph, (const float (*)[3])cos, cos_num / 3, ob, co_ret, scale_ret);
}

static void rna_Object_crazyspace_eval(Object *object,
                                       ReportList *reports,
                                       Depsgraph *depsgraph,
                                       Scene *scene)
{
  BKE_crazyspace_api_eval(depsgraph, scene, object, reports);
}

static void rna_Object_crazyspace_displacement_to_deformed(Object *object,
                                                           ReportList *reports,
                                                           const int vertex_index,
                                                           const float displacement[3],
                                                           float r_displacement_deformed[3])
{
  BKE_crazyspace_api_displacement_to_deformed(
      object, reports, vertex_index, displacement, r_displacement_deformed);
}

static void rna_Object_crazyspace_displacement_to_original(Object *object,
                                                           ReportList *reports,
                                                           const int vertex_index,
                                                           const float displacement_deformed[3],
                                                           float r_displacement[3])
{
  BKE_crazyspace_api_displacement_to_original(
      object, reports, vertex_index, displacement_deformed, r_displacement);
}

static void rna_Object_crazyspace_eval_clear(Object *object)
{
  BKE_crazyspace_api_eval_clear(object);
}

/* copied from Mesh_getFromObject and adapted to RNA interface */
static Mesh *rna_Object_to_mesh(Object *object,
                                ReportList *reports,
                                bool preserve_all_data_layers,
                                Depsgraph *depsgraph)
{
  /* TODO(sergey): Make it more re-usable function, de-duplicate with
   * rna_Main_meshes_new_from_object. */
  switch (object->type) {
    case OB_FONT:
    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_MBALL:
    case OB_MESH:
      break;
    default:
      BKE_report(reports, RPT_ERROR, "Object does not have geometry data");
      return nullptr;
  }

  return BKE_object_to_mesh(depsgraph, object, preserve_all_data_layers);
}

static void rna_Object_to_mesh_clear(Object *object)
{
  BKE_object_to_mesh_clear(object);
}

static Curve *rna_Object_to_curve(Object *object,
                                  ReportList *reports,
                                  Depsgraph *depsgraph,
                                  bool apply_modifiers)
{
  if (!ELEM(object->type, OB_FONT, OB_CURVES_LEGACY)) {
    BKE_report(reports, RPT_ERROR, "Object is not a curve or a text");
    return nullptr;
  }

  if (depsgraph == nullptr) {
    BKE_report(reports, RPT_ERROR, "Invalid depsgraph");
    return nullptr;
  }

  return BKE_object_to_curve(object, depsgraph, apply_modifiers);
}

static void rna_Object_to_curve_clear(Object *object)
{
  BKE_object_to_curve_clear(object);
}

static PointerRNA rna_Object_shape_key_add(
    Object *ob, bContext *C, ReportList *reports, const char *name, bool from_mix)
{
  Main *bmain = CTX_data_main(C);
  KeyBlock *kb = nullptr;

  if ((kb = BKE_object_shapekey_insert(bmain, ob, name, from_mix))) {
    /* Set the initial blend value. */
    kb->curval = 1.0f;
    CLAMP(kb->curval, kb->slidermin, kb->slidermax);

    PointerRNA keyptr = RNA_pointer_create_discrete(
        (ID *)BKE_key_from_object(ob), &RNA_ShapeKey, kb);
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(bmain);

    return keyptr;
  }
  else {
    BKE_reportf(reports, RPT_ERROR, "Object '%s' does not support shapes", ob->id.name + 2);
    return PointerRNA_NULL;
  }
}

static void rna_Object_shape_key_remove(Object *ob,
                                        Main *bmain,
                                        ReportList *reports,
                                        PointerRNA *kb_ptr)
{
  KeyBlock *kb = static_cast<KeyBlock *>(kb_ptr->data);
  Key *key = BKE_key_from_object(ob);

  if ((key == nullptr) || BLI_findindex(&key->block, kb) == -1) {
    BKE_report(reports, RPT_ERROR, "ShapeKey not found");
    return;
  }

  if (!BKE_object_shapekey_remove(bmain, ob, kb)) {
    BKE_report(reports, RPT_ERROR, "Could not remove ShapeKey");
    return;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);

  kb_ptr->invalidate();
}

static void rna_Object_shape_key_clear(Object *ob, Main *bmain)
{
  BKE_object_shapekey_free(bmain, ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, ob);
}

#  if 0
static void rna_Mesh_assign_verts_to_group(
    Object *ob, bDeformGroup *group, int *indices, int totindex, float weight, int assignmode)
{
  if (ob->type != OB_MESH) {
    BKE_report(reports, RPT_ERROR, "Object should be of mesh type");
    return;
  }

  Mesh *mesh = (Mesh *)ob->data;
  int group_index = BLI_findlink(&ob->defbase, group);
  if (group_index == -1) {
    BKE_report(reports, RPT_ERROR, "No vertex groups assigned to mesh");
    return;
  }

  if (assignmode != WEIGHT_REPLACE && assignmode != WEIGHT_ADD && assignmode != WEIGHT_SUBTRACT) {
    BKE_report(reports, RPT_ERROR, "Bad assignment mode");
    return;
  }

  /* makes a set of dVerts corresponding to the mVerts */
  if (!mesh->dvert) {
    create_dverts(&mesh->id);
  }

  /* Loop list adding verts to group. */
  for (i = 0; i < totindex; i++) {
    if (i < 0 || i >= mesh->verts_num) {
      BKE_report(reports, RPT_ERROR, "Bad vertex index in list");
      return;
    }

    add_vert_defnr(ob, group_index, i, weight, assignmode);
  }
}
#  endif

/* don't call inside a loop */
static int mesh_corner_tri_to_face_index(Mesh *mesh_eval, const int tri_index)
{
  const blender::Span<int> tri_faces = mesh_eval->corner_tri_faces();
  const int face_i = tri_faces[tri_index];
  const int *index_face_to_orig = static_cast<const int *>(
      CustomData_get_layer(&mesh_eval->face_data, CD_ORIGINDEX));
  return index_face_to_orig ? index_face_to_orig[face_i] : face_i;
}

/* TODO(sergey): Make the Python API more clear that evaluation might happen, or require
 * passing fully evaluated depsgraph. */
static Object *eval_object_ensure(Object *ob,
                                  bContext *C,
                                  ReportList *reports,
                                  PointerRNA *rnaptr_depsgraph)
{
  if (ob->runtime->data_eval == nullptr) {
    Object *ob_orig = ob;
    Depsgraph *depsgraph = rnaptr_depsgraph != nullptr ?
                               static_cast<Depsgraph *>(rnaptr_depsgraph->data) :
                               nullptr;
    if (depsgraph == nullptr) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    if (depsgraph != nullptr) {
      ob = DEG_get_evaluated(depsgraph, ob);
    }
    if (ob == nullptr || BKE_object_get_evaluated_mesh(ob) == nullptr) {
      BKE_reportf(
          reports, RPT_ERROR, "Object '%s' has no evaluated mesh data", ob_orig->id.name + 2);
      return nullptr;
    }
  }
  return ob;
}

static void rna_Object_ray_cast(Object *ob,
                                bContext *C,
                                ReportList *reports,
                                const float origin[3],
                                const float direction[3],
                                float distance,
                                PointerRNA *rnaptr_depsgraph,
                                bool *r_success,
                                float r_location[3],
                                float r_normal[3],
                                int *r_index)
{
  bool success = false;

  /* TODO(sergey): This isn't very reliable check. It is possible to have non-nullptr pointer
   * but which is out of date, and possibly dangling one. */
  if ((ob = eval_object_ensure(ob, C, reports, rnaptr_depsgraph)) == nullptr) {
    return;
  }

  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);

  /* Test bounding box first (efficiency) */
  const std::optional<blender::Bounds<blender::float3>> bounds = mesh_eval->bounds_min_max();
  if (!bounds) {
    return;
  }
  float distmin;

  /* Needed for valid distance check from #isect_ray_aabb_v3_simple() call. */
  float direction_unit[3];
  normalize_v3_v3(direction_unit, direction);

  if (isect_ray_aabb_v3_simple(
          origin, direction_unit, bounds->min, bounds->max, &distmin, nullptr) &&
      distmin <= distance)
  {

    /* No need to managing allocation or freeing of the BVH data.
     * This is generated and freed as needed. */
    blender::bke::BVHTreeFromMesh treeData = mesh_eval->bvh_corner_tris();

    /* may fail if the mesh has no faces, in that case the ray-cast misses */
    if (treeData.tree != nullptr) {
      BVHTreeRayHit hit;

      hit.index = -1;
      hit.dist = distance;

      if (BLI_bvhtree_ray_cast(treeData.tree,
                               origin,
                               direction_unit,
                               0.0f,
                               &hit,
                               treeData.raycast_callback,
                               &treeData) != -1)
      {
        if (hit.dist <= distance) {
          *r_success = success = true;

          copy_v3_v3(r_location, hit.co);
          copy_v3_v3(r_normal, hit.no);
          *r_index = mesh_corner_tri_to_face_index(mesh_eval, hit.index);
        }
      }
    }
  }
  if (success == false) {
    *r_success = false;

    zero_v3(r_location);
    zero_v3(r_normal);
    *r_index = -1;
  }
}

static void rna_Object_closest_point_on_mesh(Object *ob,
                                             bContext *C,
                                             ReportList *reports,
                                             const float origin[3],
                                             float distance,
                                             PointerRNA *rnaptr_depsgraph,
                                             bool *r_success,
                                             float r_location[3],
                                             float r_normal[3],
                                             int *r_index)
{

  if ((ob = eval_object_ensure(ob, C, reports, rnaptr_depsgraph)) == nullptr) {
    return;
  }

  /* No need to managing allocation or freeing of the BVH data.
   * this is generated and freed as needed. */
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob);
  blender::bke::BVHTreeFromMesh treeData = mesh_eval->bvh_corner_tris();

  if (treeData.tree == nullptr) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Object '%s' could not create internal data for finding nearest point",
                ob->id.name + 2);
    return;
  }
  else {
    BVHTreeNearest nearest;

    nearest.index = -1;
    nearest.dist_sq = distance * distance;

    if (BLI_bvhtree_find_nearest(
            treeData.tree, origin, &nearest, treeData.nearest_callback, &treeData) != -1)
    {
      *r_success = true;

      copy_v3_v3(r_location, nearest.co);
      copy_v3_v3(r_normal, nearest.no);
      *r_index = mesh_corner_tri_to_face_index(mesh_eval, nearest.index);
    }
    else {
      *r_success = false;

      zero_v3(r_location);
      zero_v3(r_normal);
      *r_index = -1;
    }
  }
}

static bool rna_Object_is_modified(Object *ob, Scene *scene, int settings)
{
  return BKE_object_is_modified(scene, ob) & settings;
}

static bool rna_Object_is_deform_modified(Object *ob, Scene *scene, int settings)
{
  return BKE_object_is_deform_modified(scene, ob) & settings;
}

#  ifndef NDEBUG

#    include "BKE_mesh_runtime.hh"

void rna_Object_me_eval_info(
    Object *ob, bContext *C, int type, PointerRNA *rnaptr_depsgraph, char *result)
{
  const Mesh *mesh_eval = nullptr;
  char *ret = nullptr;

  result[0] = '\0';

  switch (type) {
    case 1:
    case 2:
      if ((ob = eval_object_ensure(ob, C, nullptr, rnaptr_depsgraph)) == nullptr) {
        return;
      }
  }

  switch (type) {
    case 0:
      if (ob->type == OB_MESH) {
        mesh_eval = static_cast<Mesh *>(ob->data);
      }
      break;
    case 1:
      mesh_eval = BKE_object_get_mesh_deform_eval(ob);
      break;
    case 2:
      mesh_eval = BKE_object_get_evaluated_mesh(ob);
      break;
  }

  if (mesh_eval) {
    ret = BKE_mesh_debug_info(mesh_eval);
    if (ret) {
      BLI_strncpy(result, ret, MESH_DM_INFO_STR_MAX);
      MEM_freeN(ret);
    }
  }
}
#  else
void rna_Object_me_eval_info(Object * /*ob*/,
                             bContext * /*C*/,
                             int /*type*/,
                             PointerRNA * /*rnaptr_depsgraph*/,
                             char *result)
{
  result[0] = '\0';
}
#  endif /* NDEBUG */

static bool rna_Object_update_from_editmode(Object *ob, Main *bmain)
{
  /* fail gracefully if we aren't in edit-mode. */
  const bool result = blender::ed::object::editmode_load(bmain, ob);
  if (result) {
    /* Loading edit mesh to mesh changes geometry, and scripts might expect it to be properly
     * informed about changes. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
  return result;
}

#else /* RNA_RUNTIME */

void RNA_api_object(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem mesh_type_items[] = {
      {eModifierMode_Realtime, "PREVIEW", 0, "Preview", "Apply modifier preview settings"},
      {eModifierMode_Render, "RENDER", 0, "Render", "Apply modifier render settings"},
      {0, nullptr, 0, nullptr, nullptr},
  };

#  ifndef NDEBUG
  static const EnumPropertyItem mesh_dm_info_items[] = {
      {0, "SOURCE", 0, "Source", "Source mesh"},
      {1, "DEFORM", 0, "Deform", "Objects deform mesh"},
      {2, "FINAL", 0, "Final", "Objects final mesh"},
      {0, nullptr, 0, nullptr, nullptr},
  };
#  endif

  /* Special wrapper to access the base selection value */
  func = RNA_def_function(srna, "select_get", "rna_Object_select_get");
  RNA_def_function_ui_description(
      func, "Test if the object is selected. The selection state is per view layer.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_boolean(func, "result", false, "", "Object selected");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "select_set", "rna_Object_select_set");
  RNA_def_function_ui_description(
      func, "Select or deselect the object. The selection state is per view layer.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_boolean(func, "state", false, "", "Selection state to define");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);

  func = RNA_def_function(srna, "hide_get", "rna_Object_hide_get");
  RNA_def_function_ui_description(
      func,
      "Test if the object is hidden for viewport editing. This hiding state is per view layer.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_boolean(func, "result", false, "", "Object hidden");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "hide_set", "rna_Object_hide_set");
  RNA_def_function_ui_description(
      func, "Hide the object for viewport editing. This hiding state is per view layer.");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  parm = RNA_def_boolean(func, "state", false, "", "Hide state to define");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);

  func = RNA_def_function(srna, "visible_get", "rna_Object_visible_get");
  RNA_def_function_ui_description(func,
                                  "Test if the object is visible in the 3D viewport, taking into "
                                  "account all visibility settings");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_pointer(
      func, "viewport", "SpaceView3D", "", "Use this instead of the active 3D viewport");
  parm = RNA_def_boolean(func, "result", false, "", "Object visible");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "holdout_get", "rna_Object_holdout_get");
  RNA_def_function_ui_description(func, "Test if object is masked in the view layer");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_boolean(func, "result", false, "", "Object holdout");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "indirect_only_get", "rna_Object_indirect_only_get");
  RNA_def_function_ui_description(func,
                                  "Test if object is set to contribute only indirectly (through "
                                  "shadows and reflections) in the view layer");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "view_layer", "ViewLayer", "", "Use this instead of the active view layer");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  parm = RNA_def_boolean(func, "result", false, "", "Object indirect only");
  RNA_def_function_return(func, parm);

  /* Local View */
  func = RNA_def_function(srna, "local_view_get", "rna_Object_local_view_get");
  RNA_def_function_ui_description(func, "Get the local view state for this object");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "viewport", "SpaceView3D", "", "Viewport in local view");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", false, "", "Object local view state");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "local_view_set", "rna_Object_local_view_set");
  RNA_def_function_ui_description(func, "Set the local view state for this object");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "viewport", "SpaceView3D", "", "Viewport in local view");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR | PARM_REQUIRED);
  parm = RNA_def_boolean(func, "state", false, "", "Local view state to define");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* Viewport */
  func = RNA_def_function(srna, "visible_in_viewport_get", "rna_Object_visible_in_viewport_get");
  RNA_def_function_ui_description(
      func, "Check for local view and local collections for this viewport and object");
  parm = RNA_def_pointer(func, "viewport", "SpaceView3D", "", "Viewport in local collections");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", false, "", "Object viewport visibility");
  RNA_def_function_return(func, parm);

  /* Matrix space conversion */
  func = RNA_def_function(srna, "convert_space", "rna_Object_mat_convert_space");
  RNA_def_function_ui_description(
      func, "Convert (transform) the given matrix from one space to another");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func,
      "pose_bone",
      "PoseBone",
      "",
      "Bone to use to define spaces (may be None, in which case only the two 'WORLD' and "
      "'LOCAL' spaces are usable)");
  parm = RNA_def_property(func, "matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The matrix to transform");
  parm = RNA_def_property(func, "matrix_return", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The transformed matrix");
  RNA_def_function_output(func, parm);
  parm = RNA_def_enum(func,
                      "from_space",
                      space_items,
                      CONSTRAINT_SPACE_WORLD,
                      "",
                      "The space in which 'matrix' is currently");
  parm = RNA_def_enum(func,
                      "to_space",
                      space_items,
                      CONSTRAINT_SPACE_WORLD,
                      "",
                      "The space to which you want to transform 'matrix'");

  /* Camera-related operations */
  func = RNA_def_function(srna, "calc_matrix_camera", "rna_Object_calc_matrix_camera");
  RNA_def_function_ui_description(func,
                                  "Generate the camera projection matrix of this object "
                                  "(mostly useful for Camera and Light types)");
  parm = RNA_def_pointer(
      func, "depsgraph", "Depsgraph", "", "Depsgraph to get evaluated data from");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "result", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_multi_array(parm, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(parm, "", "The camera projection matrix");
  RNA_def_function_output(func, parm);
  parm = RNA_def_int(func, "x", 1, 0, INT_MAX, "", "Width of the render area", 0, 10000);
  parm = RNA_def_int(func, "y", 1, 0, INT_MAX, "", "Height of the render area", 0, 10000);
  parm = RNA_def_float(
      func, "scale_x", 1.0f, 1.0e-6f, FLT_MAX, "", "Width scaling factor", 1.0e-2f, 100.0f);
  parm = RNA_def_float(
      func, "scale_y", 1.0f, 1.0e-6f, FLT_MAX, "", "Height scaling factor", 1.0e-2f, 100.0f);

  func = RNA_def_function(srna, "camera_fit_coords", "rna_Object_camera_fit_coords");
  RNA_def_function_ui_description(func,
                                  "Compute the coordinate (and scale for ortho cameras) "
                                  "given object should be to 'see' all given coordinates");
  parm = RNA_def_pointer(
      func, "depsgraph", "Depsgraph", "", "Depsgraph to get evaluated data from");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float_array(func,
                             "coordinates",
                             1,
                             nullptr,
                             -FLT_MAX,
                             FLT_MAX,
                             "",
                             "Coordinates to fit in",
                             -FLT_MAX,
                             FLT_MAX);
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL | PROP_DYNAMIC, PARM_REQUIRED);
  parm = RNA_def_property(func, "co_return", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_property_ui_text(parm, "", "The location to aim to be able to see all given points");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_OUTPUT);
  parm = RNA_def_property(func, "scale_return", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      parm, "", "The ortho scale to aim to be able to see all given points (if relevant)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_OUTPUT);

  /* Crazy-space access. */

  func = RNA_def_function(srna, "crazyspace_eval", "rna_Object_crazyspace_eval");
  RNA_def_function_ui_description(
      func,
      "Compute orientation mapping between vertices of an original object and object with shape "
      "keys and deforming modifiers applied."
      "The evaluation is to be freed with the crazyspace_eval_free function");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "depsgraph", "Depsgraph", "Dependency Graph", "Evaluated dependency graph");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "scene", "Scene", "Scene", "Scene of the object");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna,
                          "crazyspace_displacement_to_deformed",
                          "rna_Object_crazyspace_displacement_to_deformed");
  RNA_def_function_ui_description(
      func, "Convert displacement vector from non-deformed object space to deformed object space");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_property(func, "vertex_index", PROP_INT, PROP_NONE);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "displacement", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  parm = RNA_def_property(func, "displacement_deformed", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna,
                          "crazyspace_displacement_to_original",
                          "rna_Object_crazyspace_displacement_to_original");
  RNA_def_function_ui_description(
      func, "Convert displacement vector from deformed object space to non-deformed object space");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_property(func, "vertex_index", PROP_INT, PROP_NONE);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_property(func, "displacement", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  parm = RNA_def_property(func, "displacement_original", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(parm, 3);
  RNA_def_function_output(func, parm);

  RNA_def_function(srna, "crazyspace_eval_clear", "rna_Object_crazyspace_eval_clear");
  RNA_def_function_ui_description(func, "Free evaluated state of crazyspace");

  /* mesh */
  func = RNA_def_function(srna, "to_mesh", "rna_Object_to_mesh");
  RNA_def_function_ui_description(
      func,
      "Create a Mesh data-block from the current state of the object. The object owns the "
      "data-block. To force free it use to_mesh_clear(). "
      "The result is temporary and cannot be used by objects from the main database.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_boolean(func,
                  "preserve_all_data_layers",
                  false,
                  "",
                  "Preserve all data layers in the mesh, like UV maps and vertex groups. "
                  "By default Blender only computes the subset of data layers needed for viewport "
                  "display and rendering, for better performance.");
  RNA_def_pointer(
      func,
      "depsgraph",
      "Depsgraph",
      "Dependency Graph",
      "Evaluated dependency graph which is required when preserve_all_data_layers is true");
  parm = RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh created from object");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "to_mesh_clear", "rna_Object_to_mesh_clear");
  RNA_def_function_ui_description(func, "Clears mesh data-block created by to_mesh()");

  /* curve */
  func = RNA_def_function(srna, "to_curve", "rna_Object_to_curve");
  RNA_def_function_ui_description(
      func,
      "Create a Curve data-block from the current state of the object. This only works for curve "
      "and text objects. The object owns the data-block. To force free it, use to_curve_clear(). "
      "The result is temporary and cannot be used by objects from the main database.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(
      func, "depsgraph", "Depsgraph", "Dependency Graph", "Evaluated dependency graph");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func,
                  "apply_modifiers",
                  false,
                  "",
                  "Apply the deform modifiers on the control points of the curve. This is only "
                  "supported for curve objects.");
  parm = RNA_def_pointer(func, "curve", "Curve", "", "Curve created from object");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "to_curve_clear", "rna_Object_to_curve_clear");
  RNA_def_function_ui_description(func, "Clears curve data-block created by to_curve()");

  /* Armature */
  func = RNA_def_function(srna, "find_armature", "BKE_modifiers_is_deformed_by_armature");
  RNA_def_function_ui_description(
      func, "Find armature influencing this object as a parent or via a modifier");
  parm = RNA_def_pointer(
      func, "ob_arm", "Object", "", "Armature object influencing this object or nullptr");
  RNA_def_function_return(func, parm);

  /* Shape key */
  func = RNA_def_function(srna, "shape_key_add", "rna_Object_shape_key_add");
  RNA_def_function_ui_description(func, "Add shape key to this object");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_string(func, "name", "Key", 0, "", "Unique name for the new key-block"); /* optional */
  RNA_def_boolean(func, "from_mix", true, "", "Create new shape from existing mix of shapes");
  parm = RNA_def_pointer(func, "key", "ShapeKey", "", "New shape key-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "shape_key_remove", "rna_Object_shape_key_remove");
  RNA_def_function_ui_description(func, "Remove a Shape Key from this object");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "key", "ShapeKey", "", "Key-block to be removed");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));

  func = RNA_def_function(srna, "shape_key_clear", "rna_Object_shape_key_clear");
  RNA_def_function_ui_description(func, "Remove all Shape Keys from this object");
  RNA_def_function_flag(func, FUNC_USE_MAIN);

  /* Ray Cast */
  func = RNA_def_function(srna, "ray_cast", "rna_Object_ray_cast");
  RNA_def_function_ui_description(
      func,
      "Cast a ray onto evaluated geometry, in object space "
      "(using context's or provided depsgraph to get evaluated mesh if needed)");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);

  /* ray start and end */
  parm = RNA_def_float_vector(func,
                              "origin",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "",
                              "Origin of the ray, in object space",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_float_vector(func,
                              "direction",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "",
                              "Direction of the ray, in object space",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_float(func,
                "distance",
                BVH_RAYCAST_DIST_MAX,
                0.0,
                BVH_RAYCAST_DIST_MAX,
                "",
                "Maximum distance",
                0.0,
                BVH_RAYCAST_DIST_MAX);
  parm = RNA_def_pointer(
      func,
      "depsgraph",
      "Depsgraph",
      "",
      "Depsgraph to use to get evaluated data, when called from original object "
      "(only needed if current Context's depsgraph is not suitable)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);

  /* return location and normal */
  parm = RNA_def_boolean(
      func, "result", false, "", "Whether the ray successfully hit the geometry");
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_vector(func,
                              "location",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "The hit location of this ray cast",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_vector(func,
                              "normal",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Normal",
                              "The face normal at the ray cast hit location",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_int(
      func, "index", 0, 0, 0, "", "The face index, -1 when original data isn't available", 0, 0);
  RNA_def_function_output(func, parm);

  /* Nearest Point */
  func = RNA_def_function(srna, "closest_point_on_mesh", "rna_Object_closest_point_on_mesh");
  RNA_def_function_ui_description(
      func,
      "Find the nearest point on evaluated geometry, in object space "
      "(using context's or provided depsgraph to get evaluated mesh if needed)");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);

  /* location of point for test and max distance */
  parm = RNA_def_float_vector(func,
                              "origin",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "",
                              "Point to find closest geometry from (in object space)",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* default is sqrt(FLT_MAX) */
  RNA_def_float(
      func, "distance", 1.844674352395373e+19, 0.0, FLT_MAX, "", "Maximum distance", 0.0, FLT_MAX);
  parm = RNA_def_pointer(
      func,
      "depsgraph",
      "Depsgraph",
      "",
      "Depsgraph to use to get evaluated data, when called from original object "
      "(only needed if current Context's depsgraph is not suitable)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);

  /* return location and normal */
  parm = RNA_def_boolean(func, "result", false, "", "Whether closest point on geometry was found");
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_vector(func,
                              "location",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Location",
                              "The location on the object closest to the point",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
  parm = RNA_def_float_vector(func,
                              "normal",
                              3,
                              nullptr,
                              -FLT_MAX,
                              FLT_MAX,
                              "Normal",
                              "The face normal at the closest point",
                              -1e4,
                              1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);

  parm = RNA_def_int(
      func, "index", 0, 0, 0, "", "The face index, -1 when original data isn't available", 0, 0);
  RNA_def_function_output(func, parm);

  /* View */

  /* utility function for checking if the object is modified */
  func = RNA_def_function(srna, "is_modified", "rna_Object_is_modified");
  RNA_def_function_ui_description(func,
                                  "Determine if this object is modified from the base mesh data");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene in which to check the object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_enum(func, "settings", mesh_type_items, 0, "", "Modifier settings to apply");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", false, "", "Whether the object is modified");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "is_deform_modified", "rna_Object_is_deform_modified");
  RNA_def_function_ui_description(
      func, "Determine if this object is modified by a deformation from the base mesh data");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene in which to check the object");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  parm = RNA_def_enum(func, "settings", mesh_type_items, 0, "", "Modifier settings to apply");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_boolean(func, "result", false, "", "Whether the object is deform-modified");
  RNA_def_function_return(func, parm);

#  ifndef NDEBUG
  /* mesh */
  func = RNA_def_function(srna, "dm_info", "rna_Object_me_eval_info");
  RNA_def_function_ui_description(
      func,
      "Returns a string for original/evaluated mesh data (debug builds only, "
      "using context's or provided depsgraph to get evaluated mesh if needed)");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  parm = RNA_def_enum(func, "type", mesh_dm_info_items, 0, "", "Modifier settings to apply");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(
      func,
      "depsgraph",
      "Depsgraph",
      "",
      "Depsgraph to use to get evaluated data, when called from original object "
      "(only needed if current Context's depsgraph is not suitable)");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);
  /* weak!, no way to return dynamic string type */
  parm = RNA_def_string(
      func, "result", nullptr, MESH_DM_INFO_STR_MAX, "", "Requested information");
  RNA_def_parameter_flags(
      parm, PROP_THICK_WRAP, ParameterFlag(0)); /* needed for string return value */
  RNA_def_function_output(func, parm);
#  endif /* !NDEBUG */

  func = RNA_def_function(srna, "update_from_editmode", "rna_Object_update_from_editmode");
  RNA_def_function_ui_description(func, "Load the objects edit-mode data into the object data");
  RNA_def_function_flag(func, FUNC_USE_MAIN);
  parm = RNA_def_boolean(func, "result", false, "", "Success");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "cache_release", "BKE_object_free_caches");
  RNA_def_function_ui_description(func,
                                  "Release memory used by caches associated with this object. "
                                  "Intended to be used by render engines only.");
}

#endif /* RNA_RUNTIME */

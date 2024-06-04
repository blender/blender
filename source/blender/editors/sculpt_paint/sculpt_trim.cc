/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "DNA_mesh_types.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_polyfill_2d.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "DNA_modifier_types.h"

#include "DEG_depsgraph.hh"

#include "ED_sculpt.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "bmesh.hh"
#include "tools/bmesh_boolean.hh"
#include "tools/bmesh_intersect.hh"

#include "paint_intern.hh"
#include "sculpt_intern.hh"

namespace blender::ed::sculpt_paint::trim {

enum class OperationType {
  Intersect = 0,
  Difference = 1,
  Union = 2,
  Join = 3,
};

/* Intersect is not exposed in the UI because it does not work correctly with symmetry (it deletes
 * the symmetrical part of the mesh in the first symmetry pass). */
static EnumPropertyItem operation_types[] = {
    {int(OperationType::Difference),
     "DIFFERENCE",
     0,
     "Difference",
     "Use a difference boolean operation"},
    {int(OperationType::Union), "UNION", 0, "Union", "Use a union boolean operation"},
    {int(OperationType::Join),
     "JOIN",
     0,
     "Join",
     "Join the new mesh as separate geometry, without performing any boolean operation"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class OrientationType {
  View = 0,
  Surface = 1,
};
static EnumPropertyItem orientation_types[] = {
    {int(OrientationType::View),
     "VIEW",
     0,
     "View",
     "Use the view to orientate the trimming shape"},
    {int(OrientationType::Surface),
     "SURFACE",
     0,
     "Surface",
     "Use the surface normal to orientate the trimming shape"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class ExtrudeMode {
  Project = 0,
  Fixed = 1,
};

static EnumPropertyItem extrude_modes[] = {
    {int(ExtrudeMode::Project), "PROJECT", 0, "Project", "Project back faces when extruding"},
    {int(ExtrudeMode::Fixed), "FIXED", 0, "Fixed", "Extrude back faces by fixed amount"},
    {0, nullptr, 0, nullptr, nullptr},
};

enum class SolverMode {
  Exact = 0,
  Fast = 1,
};

static EnumPropertyItem solver_modes[] = {
    {int(SolverMode::Exact), "EXACT", 0, "Exact", "Use the exact boolean solver"},
    {int(SolverMode::Fast), "FAST", 0, "Fast", "Use the fast float boolean solver"},
    {0, nullptr, 0, nullptr, nullptr},
};

struct TrimOperation {
  gesture::Operation op;

  /* Operation-generated geometry. */
  Mesh *mesh;
  float (*true_mesh_co)[3];

  /* Operator properties. */
  bool use_cursor_depth;

  bool initial_hit;
  blender::float3 initial_location;
  blender::float3 initial_normal;

  OperationType mode;
  SolverMode solver_mode;
  OrientationType orientation;
  ExtrudeMode extrude_mode;
};

/* Recalculate the mesh normals for the generated trim mesh. */
static void update_normals(gesture::GestureData &gesture_data)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  Mesh *trim_mesh = trim_operation->mesh;

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(trim_mesh);

  BMeshCreateParams bm_create_params{};
  bm_create_params.use_toolflags = true;
  BMesh *bm = BM_mesh_create(&allocsize, &bm_create_params);

  BMeshFromMeshParams bm_from_me_params{};
  bm_from_me_params.calc_face_normal = true;
  bm_from_me_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, trim_mesh, &bm_from_me_params);

  BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
  BMO_op_callf(bm,
               (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
               "recalc_face_normals faces=%hf",
               BM_ELEM_TAG);
  BM_mesh_elem_hflag_disable_all(bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_TAG, false);

  BMeshToMeshParams convert_params{};
  convert_params.calc_object_remap = false;
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm, &convert_params, trim_mesh);

  BM_mesh_free(bm);
  BKE_id_free(nullptr, trim_mesh);
  trim_operation->mesh = result;
}

/* Get the origin and normal that are going to be used for calculating the depth and position of
 * the trimming geometry. */
static void get_origin_and_normal(gesture::GestureData &gesture_data,
                                  float *r_origin,
                                  float *r_normal)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  /* Use the view origin and normal in world space. The trimming mesh coordinates are
   * calculated in world space, aligned to the view, and then converted to object space to
   * store them in the final trimming mesh which is going to be used in the boolean operation.
   */
  switch (trim_operation->orientation) {
    case OrientationType::View:
      mul_v3_m4v3(r_origin,
                  gesture_data.vc.obact->object_to_world().ptr(),
                  trim_operation->initial_location);
      copy_v3_v3(r_normal, gesture_data.world_space_view_normal);
      negate_v3(r_normal);
      break;
    case OrientationType::Surface:
      mul_v3_m4v3(r_origin,
                  gesture_data.vc.obact->object_to_world().ptr(),
                  trim_operation->initial_location);
      /* Transforming the normal does not take non uniform scaling into account. Sculpt mode is not
       * expected to work on object with non uniform scaling. */
      copy_v3_v3(r_normal, trim_operation->initial_normal);
      mul_mat3_m4_v3(gesture_data.vc.obact->object_to_world().ptr(), r_normal);
      break;
  }
}

/* Calculates the depth of the drawn shape inside the scene.*/
static void calculate_depth(gesture::GestureData &gesture_data,
                            float &r_depth_front,
                            float &r_depth_back)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;

  SculptSession &ss = *gesture_data.ss;
  ViewContext &vc = gesture_data.vc;

  const int totvert = SCULPT_vertex_count_get(ss);

  float shape_plane[4];
  float shape_origin[3];
  float shape_normal[3];
  get_origin_and_normal(gesture_data, shape_origin, shape_normal);
  plane_from_point_normal_v3(shape_plane, shape_origin, shape_normal);

  float depth_front = FLT_MAX;
  float depth_back = -FLT_MAX;

  for (int i = 0; i < totvert; i++) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(*ss.pbvh, i);

    const float *vco = SCULPT_vertex_co_get(ss, vertex);
    /* Convert the coordinates to world space to calculate the depth. When generating the trimming
     * mesh, coordinates are first calculated in world space, then converted to object space to
     * store them. */
    float world_space_vco[3];
    mul_v3_m4v3(world_space_vco, vc.obact->object_to_world().ptr(), vco);
    const float dist = dist_signed_to_plane_v3(world_space_vco, shape_plane);
    depth_front = min_ff(dist, depth_front);
    depth_back = max_ff(dist, depth_back);
  }

  if (trim_operation->use_cursor_depth) {
    float world_space_gesture_initial_location[3];
    mul_v3_m4v3(world_space_gesture_initial_location,
                vc.obact->object_to_world().ptr(),
                trim_operation->initial_location);

    float mid_point_depth;
    if (trim_operation->orientation == OrientationType::View) {
      mid_point_depth = trim_operation->initial_hit ?
                            dist_signed_to_plane_v3(world_space_gesture_initial_location,
                                                    shape_plane) :
                            (depth_back + depth_front) * 0.5f;
    }
    else {
      /* When using normal orientation, if the stroke started over the mesh, position the mid point
       * at 0 distance from the shape plane. This positions the trimming shape half inside of the
       * surface. */
      mid_point_depth = trim_operation->initial_hit ? 0.0f : (depth_back + depth_front) * 0.5f;
    }

    float depth_radius;

    if (trim_operation->initial_hit) {
      depth_radius = ss.cursor_radius;
    }
    else {
      /* ss.cursor_radius is only valid if the stroke started
       * over the sculpt mesh.  If it's not we must
       * compute the radius ourselves.  See #81452.
       */

      Sculpt *sd = CTX_data_tool_settings(vc.C)->sculpt;
      Brush *brush = BKE_paint_brush(&sd->paint);
      Scene *scene = CTX_data_scene(vc.C);

      if (!BKE_brush_use_locked_size(scene, brush)) {
        depth_radius = paint_calc_object_space_radius(
            vc, trim_operation->initial_location, BKE_brush_size_get(scene, brush));
      }
      else {
        depth_radius = BKE_brush_unprojected_radius_get(scene, brush);
      }
    }

    depth_front = mid_point_depth - depth_radius;
    depth_back = mid_point_depth + depth_radius;
  }

  r_depth_front = depth_front;
  r_depth_back = depth_back;
}

/* Calculates a scalar factor to use to ensure a drawn line gesture
 * encompasses the entire object to be acted on. */
static float calc_expand_factor(const gesture::GestureData &gesture_data)
{
  Object &object = *gesture_data.vc.obact;

  rcti rect;
  const Bounds<float3> bounds = *BKE_object_boundbox_get(&object);
  paint_convert_bb_to_rect(
      &rect, bounds.min, bounds.max, *gesture_data.vc.region, *gesture_data.vc.rv3d, object);

  const float2 min_corner(rect.xmin, rect.ymin);
  const float2 max_corner(rect.xmax, rect.ymax);

  /* Multiply the screen space bounds by an arbitrary factor to ensure the created points are
   * sufficiently far and enclose the mesh to be operated on. */
  return math::distance(min_corner, max_corner) * 2.0f;
}

/* Converts a line gesture's points into usable screen points. */
static Array<float2> gesture_to_screen_points(gesture::GestureData &gesture_data)
{
  if (gesture_data.shape_type != gesture::ShapeType::Line) {
    return gesture_data.gesture_points;
  }

  const float expand_factor = calc_expand_factor(gesture_data);

  float2 start(gesture_data.gesture_points[0]);
  float2 end(gesture_data.gesture_points[1]);

  const float2 dir = math::normalize(end - start);

  if (!gesture_data.line.use_side_planes) {
    end = end + dir * expand_factor;
    start = start - dir * expand_factor;
  }

  float2 perp(dir.y, -dir.x);

  if (gesture_data.line.flip) {
    perp *= -1;
  }

  const float2 parallel_start = start + perp * expand_factor;
  const float2 parallel_end = end + perp * expand_factor;

  return {start, end, parallel_end, parallel_start};
}

static void generate_geometry(gesture::GestureData &gesture_data)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  ViewContext &vc = gesture_data.vc;
  ARegion *region = vc.region;

  const Array<float2> screen_points = gesture_to_screen_points(gesture_data);
  BLI_assert(screen_points.size() > 1);

  const int trim_totverts = screen_points.size() * 2;
  const int trim_faces_nums = (2 * (screen_points.size() - 2)) + (2 * screen_points.size());
  trim_operation->mesh = BKE_mesh_new_nomain(
      trim_totverts, 0, trim_faces_nums, trim_faces_nums * 3);
  trim_operation->true_mesh_co = static_cast<float(*)[3]>(
      MEM_malloc_arrayN(trim_totverts, sizeof(float[3]), "mesh orco"));

  float shape_origin[3];
  float shape_normal[3];
  float shape_plane[4];
  get_origin_and_normal(gesture_data, shape_origin, shape_normal);
  plane_from_point_normal_v3(shape_plane, shape_origin, shape_normal);

  const float(*ob_imat)[4] = vc.obact->world_to_object().ptr();

  /* Write vertices coordinates OperationType::Difference for the front face. */
  MutableSpan<float3> positions = trim_operation->mesh->vert_positions_for_write();

  float depth_front;
  float depth_back;
  calculate_depth(gesture_data, depth_front, depth_back);

  if (!trim_operation->use_cursor_depth) {
    float pad_factor = (depth_back - depth_front) * 0.01f + 0.001f;

    /* When using cursor depth, don't modify the depth set by the cursor radius. If full depth is
     * used, adding a little padding to the trimming shape can help avoiding booleans with coplanar
     * faces. */
    depth_front -= pad_factor;
    depth_back += pad_factor;
  }

  float depth_point[3];

  /* Get origin point for OrientationType::View.
   * NOTE: for projection extrusion we add depth_front here
   * instead of in the loop.
   */
  if (trim_operation->extrude_mode == ExtrudeMode::Fixed) {
    copy_v3_v3(depth_point, shape_origin);
  }
  else {
    madd_v3_v3v3fl(depth_point, shape_origin, shape_normal, depth_front);
  }

  for (const int i : screen_points.index_range()) {
    float new_point[3];
    if (trim_operation->orientation == OrientationType::View) {
      ED_view3d_win_to_3d(vc.v3d, region, depth_point, screen_points[i], new_point);

      /* For fixed mode we add the shape normal here to avoid projection errors. */
      if (trim_operation->extrude_mode == ExtrudeMode::Fixed) {
        madd_v3_v3fl(new_point, shape_normal, depth_front);
      }
    }
    else {
      ED_view3d_win_to_3d_on_plane(region, shape_plane, screen_points[i], false, new_point);
      madd_v3_v3fl(new_point, shape_normal, depth_front);
    }

    copy_v3_v3(positions[i], new_point);
  }

  /* Write vertices coordinates for the back face. */
  madd_v3_v3v3fl(depth_point, shape_origin, shape_normal, depth_back);
  for (const int i : screen_points.index_range()) {
    float new_point[3];

    if (trim_operation->extrude_mode == ExtrudeMode::Project) {
      if (trim_operation->orientation == OrientationType::View) {
        ED_view3d_win_to_3d(vc.v3d, region, depth_point, screen_points[i], new_point);
      }
      else {
        ED_view3d_win_to_3d_on_plane(region, shape_plane, screen_points[i], false, new_point);
        madd_v3_v3fl(new_point, shape_normal, depth_back);
      }
    }
    else {
      copy_v3_v3(new_point, positions[i]);
      float dist = dist_signed_to_plane_v3(new_point, shape_plane);

      madd_v3_v3fl(new_point, shape_normal, depth_back - dist);
    }

    copy_v3_v3(positions[i + screen_points.size()], new_point);
  }

  /* Project to object space. */
  for (int i = 0; i < screen_points.size() * 2; i++) {
    float new_point[3];

    copy_v3_v3(new_point, positions[i]);
    mul_v3_m4v3(positions[i], ob_imat, new_point);
    mul_v3_m4v3(trim_operation->true_mesh_co[i], ob_imat, new_point);
  }

  /* Get the triangulation for the front/back poly. */
  const int face_tris_num = bke::mesh::face_triangles_num(screen_points.size());
  Array<uint3> tris(face_tris_num);
  BLI_polyfill_calc(reinterpret_cast<const float(*)[2]>(screen_points.data()),
                    screen_points.size(),
                    0,
                    reinterpret_cast<uint(*)[3]>(tris.data()));

  /* Write the front face triangle indices. */
  MutableSpan<int> face_offsets = trim_operation->mesh->face_offsets_for_write();
  MutableSpan<int> corner_verts = trim_operation->mesh->corner_verts_for_write();
  int face_index = 0;
  int loop_index = 0;
  for (const int i : tris.index_range()) {
    face_offsets[face_index] = loop_index;
    corner_verts[loop_index + 0] = tris[i][0];
    corner_verts[loop_index + 1] = tris[i][1];
    corner_verts[loop_index + 2] = tris[i][2];
    face_index++;
    loop_index += 3;
  }

  /* Write the back face triangle indices. */
  for (const int i : tris.index_range()) {
    face_offsets[face_index] = loop_index;
    corner_verts[loop_index + 0] = tris[i][0] + screen_points.size();
    corner_verts[loop_index + 1] = tris[i][1] + screen_points.size();
    corner_verts[loop_index + 2] = tris[i][2] + screen_points.size();
    face_index++;
    loop_index += 3;
  }

  /* Write the indices for the lateral triangles. */
  for (const int i : screen_points.index_range()) {
    face_offsets[face_index] = loop_index;
    int current_index = i;
    int next_index = current_index + 1;
    if (next_index >= screen_points.size()) {
      next_index = 0;
    }
    corner_verts[loop_index + 0] = next_index + screen_points.size();
    corner_verts[loop_index + 1] = next_index;
    corner_verts[loop_index + 2] = current_index;
    face_index++;
    loop_index += 3;
  }

  for (const int i : screen_points.index_range()) {
    face_offsets[face_index] = loop_index;
    int current_index = i;
    int next_index = current_index + 1;
    if (next_index >= screen_points.size()) {
      next_index = 0;
    }
    corner_verts[loop_index + 0] = current_index;
    corner_verts[loop_index + 1] = current_index + screen_points.size();
    corner_verts[loop_index + 2] = next_index + screen_points.size();
    face_index++;
    loop_index += 3;
  }

  bke::mesh_smooth_set(*trim_operation->mesh, false);
  bke::mesh_calc_edges(*trim_operation->mesh, false, false);
  update_normals(gesture_data);
}

static void gesture_begin(bContext &C, gesture::GestureData &gesture_data)
{
  Object *object = gesture_data.vc.obact;
  SculptSession &ss = *object->sculpt;

  switch (BKE_pbvh_type(*ss.pbvh)) {
    case PBVH_FACES:
      face_set::ensure_face_sets_mesh(*object).finish();
      break;
    default:
      BLI_assert_unreachable();
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(&C);
  generate_geometry(gesture_data);
  SCULPT_topology_islands_invalidate(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, gesture_data.vc.obact, false);
  undo::push_node(*gesture_data.vc.obact, nullptr, undo::Type::Geometry);
}

static int bm_face_isect_pair(BMFace *f, void * /*user_data*/)
{
  return BM_elem_flag_test(f, BM_ELEM_DRAW) ? 1 : 0;
}

static void apply_trim(gesture::GestureData &gesture_data)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  Mesh *sculpt_mesh = BKE_mesh_from_object(gesture_data.vc.obact);
  Mesh *trim_mesh = trim_operation->mesh;

  const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(sculpt_mesh, trim_mesh);

  BMeshCreateParams bm_create_params{};
  bm_create_params.use_toolflags = false;
  BMesh *bm = BM_mesh_create(&allocsize, &bm_create_params);

  BMeshFromMeshParams bm_from_me_params{};
  bm_from_me_params.calc_face_normal = true;
  bm_from_me_params.calc_vert_normal = true;
  BM_mesh_bm_from_me(bm, trim_mesh, &bm_from_me_params);
  BM_mesh_bm_from_me(bm, sculpt_mesh, &bm_from_me_params);

  const int corner_tris_tot = poly_to_tri_count(bm->totface, bm->totloop);
  Array<std::array<BMLoop *, 3>> corner_tris(corner_tris_tot);
  BM_mesh_calc_tessellation_beauty(bm, corner_tris);

  BMIter iter;
  int i;
  const int i_faces_end = trim_mesh->faces_num;

  /* We need face normals because of 'BM_face_split_edgenet'
   * we could calculate on the fly too (before calling split). */

  const short ob_src_totcol = trim_mesh->totcol;
  Array<short> material_remap(ob_src_totcol ? ob_src_totcol : 1);

  BMFace *efa;
  i = 0;
  BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
    normalize_v3(efa->no);

    /* Temp tag to test which side split faces are from. */
    BM_elem_flag_enable(efa, BM_ELEM_DRAW);

    /* Remap material. */
    if (efa->mat_nr < ob_src_totcol) {
      efa->mat_nr = material_remap[efa->mat_nr];
    }

    if (++i == i_faces_end) {
      break;
    }
  }

  /* Join does not do a boolean operation, it just adds the geometry. */
  if (trim_operation->mode != OperationType::Join) {
    int boolean_mode = 0;
    switch (trim_operation->mode) {
      case OperationType::Intersect:
        boolean_mode = eBooleanModifierOp_Intersect;
        break;
      case OperationType::Difference:
        boolean_mode = eBooleanModifierOp_Difference;
        break;
      case OperationType::Union:
        boolean_mode = eBooleanModifierOp_Union;
        break;
      case OperationType::Join:
        BLI_assert(false);
        break;
    }

    if (trim_operation->solver_mode == SolverMode::Exact) {
      BM_mesh_boolean(
          bm, corner_tris, bm_face_isect_pair, nullptr, 2, true, true, false, boolean_mode);
    }
    else {
      BM_mesh_intersect(bm,
                        corner_tris,
                        bm_face_isect_pair,
                        nullptr,
                        false,
                        false,
                        true,
                        true,
                        false,
                        false,
                        boolean_mode,
                        1e-6f);
    }
  }

  BMeshToMeshParams convert_params{};
  convert_params.calc_object_remap = false;
  Mesh *result = BKE_mesh_from_bmesh_nomain(bm, &convert_params, sculpt_mesh);

  BM_mesh_free(bm);
  BKE_mesh_nomain_to_mesh(
      result, static_cast<Mesh *>(gesture_data.vc.obact->data), gesture_data.vc.obact);
}

static void gesture_apply_for_symmetry_pass(bContext & /*C*/, gesture::GestureData &gesture_data)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  Mesh *trim_mesh = trim_operation->mesh;
  MutableSpan<float3> positions = trim_mesh->vert_positions_for_write();
  for (int i = 0; i < trim_mesh->verts_num; i++) {
    flip_v3_v3(positions[i], trim_operation->true_mesh_co[i], gesture_data.symmpass);
  }
  update_normals(gesture_data);
  apply_trim(gesture_data);
}

static void free_geometry(gesture::GestureData &gesture_data)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  BKE_id_free(nullptr, trim_operation->mesh);
  MEM_freeN(trim_operation->true_mesh_co);
}

static void gesture_end(bContext & /*C*/, gesture::GestureData &gesture_data)
{
  Object *object = gesture_data.vc.obact;
  Mesh *mesh = (Mesh *)object->data;

  /* Assign a new Face Set ID to the new faces created by the trim operation. */
  const int next_face_set_id = face_set::find_next_available_id(*object);
  face_set::initialize_none_to_id(mesh, next_face_set_id);

  free_geometry(gesture_data);

  undo::push_node(*gesture_data.vc.obact, nullptr, undo::Type::Geometry);
  BKE_mesh_batch_cache_dirty_tag(mesh, BKE_MESH_BATCH_DIRTY_ALL);
  DEG_id_tag_update(&gesture_data.vc.obact->id, ID_RECALC_GEOMETRY);
}

static void init_operation(gesture::GestureData &gesture_data, wmOperator &op)
{
  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;

  trim_operation->op.begin = gesture_begin;
  trim_operation->op.apply_for_symmetry_pass = gesture_apply_for_symmetry_pass;
  trim_operation->op.end = gesture_end;

  trim_operation->mode = OperationType(RNA_enum_get(op.ptr, "trim_mode"));
  trim_operation->use_cursor_depth = RNA_boolean_get(op.ptr, "use_cursor_depth");
  trim_operation->orientation = OrientationType(RNA_enum_get(op.ptr, "trim_orientation"));
  trim_operation->extrude_mode = ExtrudeMode(RNA_enum_get(op.ptr, "trim_extrude_mode"));
  trim_operation->solver_mode = SolverMode(RNA_enum_get(op.ptr, "trim_solver"));

  /* If the cursor was not over the mesh, force the orientation to view. */
  if (!trim_operation->initial_hit) {
    trim_operation->orientation = OrientationType::View;
  }

  if (gesture_data.shape_type == gesture::ShapeType::Line) {
    /* Line gestures only support Difference, no extrusion. */
    trim_operation->mode = OperationType::Difference;
  }
}

static void operator_properties(wmOperatorType *ot)
{
  PropertyRNA *prop;

  prop = RNA_def_int_vector(ot->srna,
                            "location",
                            2,
                            nullptr,
                            INT_MIN,
                            INT_MAX,
                            "Location",
                            "Mouse location",
                            INT_MIN,
                            INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);

  RNA_def_enum(ot->srna,
               "trim_mode",
               operation_types,
               int(OperationType::Difference),
               "Trim Mode",
               nullptr);
  RNA_def_boolean(
      ot->srna,
      "use_cursor_depth",
      false,
      "Use Cursor for Depth",
      "Use cursor location and radius for the dimensions and position of the trimming shape");
  RNA_def_enum(ot->srna,
               "trim_orientation",
               orientation_types,
               int(OrientationType::View),
               "Shape Orientation",
               nullptr);
  RNA_def_enum(ot->srna,
               "trim_extrude_mode",
               extrude_modes,
               int(ExtrudeMode::Fixed),
               "Extrude Mode",
               nullptr);

  RNA_def_enum(ot->srna, "trim_solver", solver_modes, int(SolverMode::Fast), "Solver", nullptr);
}

static bool can_invoke(const bContext &C)
{
  const View3D &v3d = *CTX_wm_view3d(&C);
  const Base &base = *CTX_data_active_base(&C);
  if (!BKE_base_is_visible(&v3d, &base)) {
    return false;
  }

  return true;
}

static bool can_exec(const bContext &C)
{
  const Object &object = *CTX_data_active_object(&C);
  const SculptSession &ss = *object.sculpt;
  if (BKE_pbvh_type(*ss.pbvh) != PBVH_FACES) {
    /* Not supported in Multires and Dyntopo. */
    return false;
  }

  if (ss.totvert == 0) {
    /* No geometry to trim or to detect a valid position for the trimming shape. */
    return false;
  }

  return true;
}

static void initialize_cursor_info(bContext &C,
                                   const wmOperator &op,
                                   gesture::GestureData &gesture_data)
{
  const Object &ob = *CTX_data_active_object(&C);
  SculptSession &ss = *ob.sculpt;

  SCULPT_vertex_random_access_ensure(ss);

  int mval[2];
  RNA_int_get_array(op.ptr, "location", mval);

  SculptCursorGeometryInfo sgi;
  const float mval_fl[2] = {float(mval[0]), float(mval[1])};

  TrimOperation *trim_operation = (TrimOperation *)gesture_data.operation;
  trim_operation->initial_hit = SCULPT_cursor_geometry_info_update(&C, &sgi, mval_fl, false);
  if (trim_operation->initial_hit) {
    copy_v3_v3(trim_operation->initial_location, sgi.location);
    copy_v3_v3(trim_operation->initial_normal, sgi.normal);
  }
}

static int gesture_box_exec(bContext *C, wmOperator *op)
{
  if (!can_exec(*C)) {
    return OPERATOR_CANCELLED;
  }

  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_box(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }

  gesture_data->operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<TrimOperation>(__func__));
  initialize_cursor_info(*C, *op, *gesture_data);
  init_operation(*gesture_data, *op);

  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_box_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!can_invoke(*C)) {
    return OPERATOR_CANCELLED;
  }

  RNA_int_set_array(op->ptr, "location", event->mval);

  return WM_gesture_box_invoke(C, op, event);
}

static int gesture_lasso_exec(bContext *C, wmOperator *op)
{
  if (!can_exec(*C)) {
    return OPERATOR_CANCELLED;
  }

  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_lasso(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }

  gesture_data->operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<TrimOperation>(__func__));
  initialize_cursor_info(*C, *op, *gesture_data);
  init_operation(*gesture_data, *op);

  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_lasso_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!can_invoke(*C)) {
    return OPERATOR_CANCELLED;
  }

  RNA_int_set_array(op->ptr, "location", event->mval);

  return WM_gesture_lasso_invoke(C, op, event);
}

static int gesture_line_exec(bContext *C, wmOperator *op)
{
  if (!can_exec(*C)) {
    return OPERATOR_CANCELLED;
  }

  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_line(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }

  gesture_data->operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<TrimOperation>(__func__));

  initialize_cursor_info(*C, *op, *gesture_data);
  init_operation(*gesture_data, *op);
  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_line_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!can_invoke(*C)) {
    return OPERATOR_CANCELLED;
  }

  RNA_int_set_array(op->ptr, "location", event->mval);

  return WM_gesture_straightline_active_side_invoke(C, op, event);
}

static int gesture_polyline_exec(bContext *C, wmOperator *op)
{
  if (!can_exec(*C)) {
    return OPERATOR_CANCELLED;
  }

  std::unique_ptr<gesture::GestureData> gesture_data = gesture::init_from_polyline(C, op);
  if (!gesture_data) {
    return OPERATOR_CANCELLED;
  }

  gesture_data->operation = reinterpret_cast<gesture::Operation *>(
      MEM_cnew<TrimOperation>(__func__));
  initialize_cursor_info(*C, *op, *gesture_data);
  init_operation(*gesture_data, *op);

  gesture::apply(*C, *gesture_data, *op);
  return OPERATOR_FINISHED;
}

static int gesture_polyline_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  if (!can_invoke(*C)) {
    return OPERATOR_CANCELLED;
  }

  RNA_int_set_array(op->ptr, "location", event->mval);

  return WM_gesture_polyline_invoke(C, op, event);
}

void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Lasso Gesture";
  ot->idname = "SCULPT_OT_trim_lasso_gesture";
  ot->description = "Execute a boolean operation on the mesh and a shape defined by the cursor";

  ot->invoke = gesture_lasso_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = gesture_lasso_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER | OPTYPE_DEPENDS_ON_CURSOR;

  /* Properties. */
  WM_operator_properties_gesture_lasso(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Lasso);

  operator_properties(ot);
}

void SCULPT_OT_trim_box_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Box Gesture";
  ot->idname = "SCULPT_OT_trim_box_gesture";
  ot->description =
      "Execute a boolean operation on the mesh and a rectangle defined by the cursor";

  ot->invoke = gesture_box_invoke;
  ot->modal = WM_gesture_box_modal;
  ot->exec = gesture_box_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_border(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Box);

  operator_properties(ot);
}

void SCULPT_OT_trim_line_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Line Gesture";
  ot->idname = "SCULPT_OT_trim_line_gesture";
  ot->description = "Remove a portion of the mesh on one side of a line";

  ot->invoke = gesture_line_invoke;
  ot->modal = WM_gesture_straightline_oneshot_modal;
  ot->exec = gesture_line_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_straightline(ot, WM_CURSOR_EDIT);
  gesture::operator_properties(ot, gesture::ShapeType::Line);

  operator_properties(ot);
}

void SCULPT_OT_trim_polyline_gesture(wmOperatorType *ot)
{
  ot->name = "Trim Polyline Gesture";
  ot->idname = "SCULPT_OT_trim_polyline_gesture";
  ot->description =
      "Execute a boolean operation on the mesh and a polygonal shape defined by the cursor";

  ot->invoke = gesture_polyline_invoke;
  ot->modal = WM_gesture_polyline_modal;
  ot->exec = gesture_polyline_exec;

  ot->poll = SCULPT_mode_poll_view3d;

  ot->flag = OPTYPE_REGISTER;

  /* Properties. */
  WM_operator_properties_gesture_polyline(ot);
  gesture::operator_properties(ot, gesture::ShapeType::Lasso);

  operator_properties(ot);
}
}  // namespace blender::ed::sculpt_paint::trim

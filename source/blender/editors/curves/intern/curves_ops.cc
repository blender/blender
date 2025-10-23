/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include "BLI_listbase.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"

#include "BLT_translation.hh"

#include "ED_curves.hh"
#include "ED_object.hh"
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"

#include "WM_api.hh"

#include "BKE_attribute.h"
#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_geometry_set.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_legacy_convert.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_mesh_sample.hh"
#include "BKE_object.hh"
#include "BKE_paint.hh"
#include "BKE_particle.h"
#include "BKE_report.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_join_geometries.hh"
#include "GEO_reverse_uv_sampler.hh"
#include "GEO_set_curve_type.hh"
#include "GEO_subdivide_curves.hh"
#include "GEO_transform.hh"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * `cu`: Local space of the curves object that is being edited.
 * `su`: Local space of the surface object.
 * `wo`: World space.
 * `ha`: Local space of an individual hair in the legacy hair system.
 */

namespace blender::ed::curves {

bool object_has_editable_curves(const Main &bmain, const Object &object)
{
  if (object.type != OB_CURVES) {
    return false;
  }
  if (!ELEM(object.mode, OB_MODE_SCULPT_CURVES, OB_MODE_EDIT)) {
    return false;
  }
  if (!BKE_id_is_editable(&bmain, static_cast<const ID *>(object.data))) {
    return false;
  }
  return true;
}

VectorSet<Curves *> get_unique_editable_curves(const bContext &C)
{
  VectorSet<Curves *> unique_curves;

  const Main &bmain = *CTX_data_main(&C);

  Object *object = CTX_data_active_object(&C);
  if (object && object_has_editable_curves(bmain, *object)) {
    unique_curves.add_new(static_cast<Curves *>(object->data));
  }

  CTX_DATA_BEGIN (&C, Object *, object, selected_objects) {
    if (object_has_editable_curves(bmain, *object)) {
      unique_curves.add(static_cast<Curves *>(object->data));
    }
  }
  CTX_DATA_END;

  return unique_curves;
}

static bool curves_poll_impl(bContext *C,
                             const bool check_editable,
                             const bool check_surface,
                             const bool check_edit_mode)
{
  Object *object = CTX_data_active_object(C);
  if (object == nullptr || object->type != OB_CURVES) {
    return false;
  }
  if (check_editable) {
    if (!ED_operator_object_active_editable_ex(C, object)) {
      return false;
    }
  }
  if (check_surface) {
    Curves &curves = *static_cast<Curves *>(object->data);
    if (curves.surface == nullptr || curves.surface->type != OB_MESH) {
      CTX_wm_operator_poll_msg_set(C, "Curves must have a mesh surface object set");
      return false;
    }
  }
  if (check_edit_mode) {
    if ((object->mode & OB_MODE_EDIT) == 0) {
      return false;
    }
  }
  return true;
}

bool editable_curves_in_edit_mode_poll(bContext *C)
{
  return curves_poll_impl(C, true, false, true);
}

bool editable_curves_with_surface_poll(bContext *C)
{
  return curves_poll_impl(C, true, true, false);
}

bool curves_with_surface_poll(bContext *C)
{
  return curves_poll_impl(C, false, true, false);
}

bool editable_curves_poll(bContext *C)
{
  return curves_poll_impl(C, false, false, false);
}

bool curves_poll(bContext *C)
{
  return curves_poll_impl(C, false, false, false);
}

static bool editable_curves_point_domain_poll(bContext *C)
{
  if (!curves::editable_curves_poll(C)) {
    return false;
  }
  const Curves *curves_id = static_cast<const Curves *>(CTX_data_active_object(C)->data);
  if (bke::AttrDomain(curves_id->selection_domain) != bke::AttrDomain::Point) {
    CTX_wm_operator_poll_msg_set(C, "Only available in point selection mode");
    return false;
  }
  return true;
}

using bke::CurvesGeometry;

namespace convert_to_particle_system {

static int find_mface_for_root_position(const Span<float3> positions,
                                        const MFace *mface,
                                        const Span<int> possible_mface_indices,
                                        const float3 &root_pos)
{
  BLI_assert(possible_mface_indices.size() >= 1);
  if (possible_mface_indices.size() == 1) {
    return possible_mface_indices.first();
  }
  /* Find the closest #MFace to #root_pos. */
  int mface_i;
  float best_distance_sq = FLT_MAX;
  for (const int possible_mface_i : possible_mface_indices) {
    const MFace &possible_mface = mface[possible_mface_i];
    {
      float3 point_in_triangle;
      closest_on_tri_to_point_v3(point_in_triangle,
                                 root_pos,
                                 positions[possible_mface.v1],
                                 positions[possible_mface.v2],
                                 positions[possible_mface.v3]);
      const float distance_sq = len_squared_v3v3(root_pos, point_in_triangle);
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        mface_i = possible_mface_i;
      }
    }
    /* Optionally check the second triangle if the #MFace is a quad. */
    if (possible_mface.v4) {
      float3 point_in_triangle;
      closest_on_tri_to_point_v3(point_in_triangle,
                                 root_pos,
                                 positions[possible_mface.v1],
                                 positions[possible_mface.v3],
                                 positions[possible_mface.v4]);
      const float distance_sq = len_squared_v3v3(root_pos, point_in_triangle);
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        mface_i = possible_mface_i;
      }
    }
  }
  return mface_i;
}

/**
 * \return Barycentric coordinates in the #MFace.
 */
static float4 compute_mface_weights_for_position(const Span<float3> positions,
                                                 const MFace &mface,
                                                 const float3 &position)
{
  float4 mface_weights;
  if (mface.v4) {
    float mface_positions_su[4][3];
    copy_v3_v3(mface_positions_su[0], positions[mface.v1]);
    copy_v3_v3(mface_positions_su[1], positions[mface.v2]);
    copy_v3_v3(mface_positions_su[2], positions[mface.v3]);
    copy_v3_v3(mface_positions_su[3], positions[mface.v4]);
    interp_weights_poly_v3(mface_weights, mface_positions_su, 4, position);
  }
  else {
    interp_weights_tri_v3(
        mface_weights, positions[mface.v1], positions[mface.v2], positions[mface.v3], position);
    mface_weights[3] = 0.0f;
  }
  return mface_weights;
}

static void try_convert_single_object(Object &curves_ob,
                                      Main &bmain,
                                      Scene &scene,
                                      bool *r_could_not_convert_some_curves)
{
  if (curves_ob.type != OB_CURVES) {
    return;
  }
  Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
  CurvesGeometry &curves = curves_id.geometry.wrap();
  if (curves_id.surface == nullptr) {
    return;
  }
  Object &surface_ob = *curves_id.surface;
  if (surface_ob.type != OB_MESH) {
    return;
  }
  Mesh &surface_me = *static_cast<Mesh *>(surface_ob.data);

  bke::BVHTreeFromMesh surface_bvh = surface_me.bvh_corner_tris();

  const Span<float3> positions_cu = curves.positions();
  const Span<int> tri_faces = surface_me.corner_tri_faces();

  if (tri_faces.is_empty()) {
    *r_could_not_convert_some_curves = true;
  }

  const OffsetIndices<int> points_by_curve = curves.points_by_curve();
  IndexMaskMemory memory;
  const IndexMask multi_point_curves = IndexMask::from_predicate(
      curves.curves_range(), GrainSize(4096), memory, [&](const int curve_i) {
        return points_by_curve[curve_i].size() > 1;
      });

  const int hair_num = multi_point_curves.size();

  if (hair_num == 0) {
    return;
  }

  ParticleSystem *particle_system = nullptr;
  LISTBASE_FOREACH (ParticleSystem *, psys, &surface_ob.particlesystem) {
    if (STREQ(psys->name, curves_ob.id.name + 2)) {
      particle_system = psys;
      break;
    }
  }
  if (particle_system == nullptr) {
    ParticleSystemModifierData &psmd = *reinterpret_cast<ParticleSystemModifierData *>(
        object_add_particle_system(&bmain, &scene, &surface_ob, curves_ob.id.name + 2));
    particle_system = psmd.psys;
    particle_system->part->draw_step = 3;
  }

  ParticleSettings &settings = *particle_system->part;

  psys_free_particles(particle_system);
  settings.type = PART_HAIR;
  settings.totpart = 0;
  psys_changed_type(&surface_ob, particle_system);

  MutableSpan<ParticleData> particles{MEM_calloc_arrayN<ParticleData>(hair_num, __func__),
                                      hair_num};

  /* The old hair system still uses #MFace, so make sure those are available on the mesh. */
  BKE_mesh_tessface_calc(&surface_me);

  /* Prepare utility data structure to map hair roots to #MFace's. */
  const Span<int> mface_to_poly_map{
      static_cast<const int *>(CustomData_get_layer(&surface_me.fdata_legacy, CD_ORIGINDEX)),
      surface_me.totface_legacy};
  Array<Vector<int>> poly_to_mface_map(surface_me.faces_num);
  for (const int mface_i : mface_to_poly_map.index_range()) {
    const int face_i = mface_to_poly_map[mface_i];
    poly_to_mface_map[face_i].append(mface_i);
  }

  /* Prepare transformation matrices. */
  const bke::CurvesSurfaceTransforms transforms{curves_ob, &surface_ob};

  const MFace *mfaces = (const MFace *)CustomData_get_layer(&surface_me.fdata_legacy, CD_MFACE);
  const Span<float3> positions = surface_me.vert_positions();

  multi_point_curves.foreach_index([&](const int curve_i, const int new_hair_i) {
    const IndexRange points = points_by_curve[curve_i];

    const float3 &root_pos_cu = positions_cu[points.first()];
    const float3 root_pos_su = math::transform_point(transforms.curves_to_surface, root_pos_cu);

    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    BLI_bvhtree_find_nearest(
        surface_bvh.tree, root_pos_su, &nearest, surface_bvh.nearest_callback, &surface_bvh);
    BLI_assert(nearest.index >= 0);

    const int tri_i = nearest.index;
    const int face_i = tri_faces[tri_i];

    const int mface_i = find_mface_for_root_position(
        positions, mfaces, poly_to_mface_map[face_i], root_pos_su);
    const MFace &mface = mfaces[mface_i];

    const float4 mface_weights = compute_mface_weights_for_position(positions, mface, root_pos_su);

    ParticleData &particle = particles[new_hair_i];
    const int num_keys = points.size();
    MutableSpan<HairKey> hair_keys{MEM_calloc_arrayN<HairKey>(num_keys, __func__), num_keys};

    particle.hair = hair_keys.data();
    particle.totkey = hair_keys.size();
    copy_v4_v4(particle.fuv, mface_weights);
    particle.num = mface_i;
    /* Not sure if there is a better way to initialize this. */
    particle.num_dmcache = DMCACHE_NOTFOUND;

    float4x4 hair_to_surface_mat;
    psys_mat_hair_to_object(
        &surface_ob, &surface_me, PART_FROM_FACE, &particle, hair_to_surface_mat.ptr());
    /* In theory, #psys_mat_hair_to_object should handle this, but it doesn't right now. */
    hair_to_surface_mat.location() = root_pos_su;
    const float4x4 surface_to_hair_mat = math::invert(hair_to_surface_mat);

    for (const int key_i : hair_keys.index_range()) {
      const float3 &key_pos_cu = positions_cu[points[key_i]];
      const float3 key_pos_su = math::transform_point(transforms.curves_to_surface, key_pos_cu);
      const float3 key_pos_ha = math::transform_point(surface_to_hair_mat, key_pos_su);

      HairKey &key = hair_keys[key_i];
      copy_v3_v3(key.co, key_pos_ha);
      const float key_fac = key_i / float(hair_keys.size() - 1);
      key.time = 100.0f * key_fac;
      key.weight = 1.0f - key_fac;
    }
  });

  particle_system->particles = particles.data();
  particle_system->totpart = particles.size();
  particle_system->flag |= PSYS_EDITED;
  particle_system->recalc |= ID_RECALC_PSYS_RESET;

  DEG_id_tag_update(&surface_ob.id, ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&settings.id, ID_RECALC_SYNC_TO_EVAL);
}

static wmOperatorStatus curves_convert_to_particle_system_exec(bContext *C, wmOperator *op)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);

  bool could_not_convert_some_curves = false;

  Object &active_object = *CTX_data_active_object(C);
  try_convert_single_object(active_object, bmain, scene, &could_not_convert_some_curves);

  CTX_DATA_BEGIN (C, Object *, curves_ob, selected_objects) {
    if (curves_ob != &active_object) {
      try_convert_single_object(*curves_ob, bmain, scene, &could_not_convert_some_curves);
    }
  }
  CTX_DATA_END;

  if (could_not_convert_some_curves) {
    BKE_report(op->reports,
               RPT_INFO,
               "Some curves could not be converted because they were not attached to the surface");
  }

  WM_main_add_notifier(NC_OBJECT | ND_PARTICLE | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace convert_to_particle_system

static void CURVES_OT_convert_to_particle_system(wmOperatorType *ot)
{
  ot->name = "Convert Curves to Particle System";
  ot->idname = "CURVES_OT_convert_to_particle_system";
  ot->description = "Add a new or update an existing hair particle system on the surface object";

  ot->poll = curves_with_surface_poll;
  ot->exec = convert_to_particle_system::curves_convert_to_particle_system_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

namespace convert_from_particle_system {

static bke::CurvesGeometry particles_to_curves(Object &object, ParticleSystem &psys)
{
  ParticleSettings &settings = *psys.part;
  if (psys.part->type != PART_HAIR) {
    return {};
  }

  const bool transfer_parents = (settings.draw & PART_DRAW_PARENT) || settings.childtype == 0;

  const Span<ParticleCacheKey *> parents_cache{psys.pathcache, psys.totcached};
  const Span<ParticleCacheKey *> children_cache{psys.childcache, psys.totchildcache};

  int points_num = 0;
  Vector<int> curve_offsets;
  Vector<int> parents_to_transfer;
  Vector<int> children_to_transfer;
  if (transfer_parents) {
    for (const int parent_i : parents_cache.index_range()) {
      const int segments = parents_cache[parent_i]->segments;
      if (segments <= 0) {
        continue;
      }
      parents_to_transfer.append(parent_i);
      curve_offsets.append(points_num);
      points_num += segments + 1;
    }
  }
  for (const int child_i : children_cache.index_range()) {
    const int segments = children_cache[child_i]->segments;
    if (segments <= 0) {
      continue;
    }
    children_to_transfer.append(child_i);
    curve_offsets.append(points_num);
    points_num += segments + 1;
  }
  const int curves_num = parents_to_transfer.size() + children_to_transfer.size();
  curve_offsets.append(points_num);
  BLI_assert(curve_offsets.size() == curves_num + 1);
  bke::CurvesGeometry curves(points_num, curves_num);
  curves.offsets_for_write().copy_from(curve_offsets);

  const float4x4 &object_to_world_mat = object.object_to_world();
  const float4x4 world_to_object_mat = math::invert(object_to_world_mat);

  MutableSpan<float3> positions = curves.positions_for_write();
  const OffsetIndices points_by_curve = curves.points_by_curve();

  const auto copy_hair_to_curves = [&](const Span<ParticleCacheKey *> hair_cache,
                                       const Span<int> indices_to_transfer,
                                       const int curve_index_offset) {
    threading::parallel_for(indices_to_transfer.index_range(), 256, [&](const IndexRange range) {
      for (const int i : range) {
        const int hair_i = indices_to_transfer[i];
        const int curve_i = i + curve_index_offset;
        const IndexRange points = points_by_curve[curve_i];
        const Span<ParticleCacheKey> keys{hair_cache[hair_i], points.size()};
        for (const int key_i : keys.index_range()) {
          const float3 key_pos_wo = keys[key_i].co;
          positions[points[key_i]] = math::transform_point(world_to_object_mat, key_pos_wo);
        }
      }
    });
  };

  if (transfer_parents) {
    copy_hair_to_curves(parents_cache, parents_to_transfer, 0);
  }
  copy_hair_to_curves(children_cache, children_to_transfer, parents_to_transfer.size());

  curves.update_curve_types();
  curves.tag_topology_changed();
  return curves;
}

static wmOperatorStatus curves_convert_from_particle_system_exec(bContext *C, wmOperator * /*op*/)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object *ob_from_orig = object::context_active_object(C);
  ParticleSystem *psys_orig = static_cast<ParticleSystem *>(
      CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data);
  if (psys_orig == nullptr) {
    psys_orig = psys_get_current(ob_from_orig);
  }
  if (psys_orig == nullptr) {
    return OPERATOR_CANCELLED;
  }
  Object *ob_from_eval = DEG_get_evaluated(&depsgraph, ob_from_orig);
  ParticleSystem *psys_eval = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &ob_from_eval->modifiers) {
    if (md->type != eModifierType_ParticleSystem) {
      continue;
    }
    ParticleSystemModifierData *psmd = reinterpret_cast<ParticleSystemModifierData *>(md);
    if (!STREQ(psmd->psys->name, psys_orig->name)) {
      continue;
    }
    psys_eval = psmd->psys;
  }

  Object *ob_new = BKE_object_add(&bmain, &scene, &view_layer, OB_CURVES, psys_eval->name);
  Curves *curves_id = static_cast<Curves *>(ob_new->data);
  BKE_object_apply_mat4(ob_new, ob_from_orig->object_to_world().ptr(), true, false);
  curves_id->geometry.wrap() = particles_to_curves(*ob_from_eval, *psys_eval);

  DEG_relations_tag_update(&bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

static bool curves_convert_from_particle_system_poll(bContext *C)
{
  return blender::ed::object::context_active_object(C) != nullptr;
}

}  // namespace convert_from_particle_system

static void CURVES_OT_convert_from_particle_system(wmOperatorType *ot)
{
  ot->name = "Convert Particle System to Curves";
  ot->idname = "CURVES_OT_convert_from_particle_system";
  ot->description = "Add a new curves object based on the current state of the particle system";

  ot->poll = convert_from_particle_system::curves_convert_from_particle_system_poll;
  ot->exec = convert_from_particle_system::curves_convert_from_particle_system_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

namespace snap_curves_to_surface {

enum class AttachMode {
  Nearest = 0,
  Deform = 1,
};

static void snap_curves_to_surface_exec_object(Object &curves_ob,
                                               const Object &surface_ob,
                                               const AttachMode attach_mode,
                                               bool *r_invalid_uvs,
                                               bool *r_missing_uvs)
{
  Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
  CurvesGeometry &curves = curves_id.geometry.wrap();

  const Mesh &surface_mesh = *static_cast<const Mesh *>(surface_ob.data);
  const Span<float3> surface_positions = surface_mesh.vert_positions();
  const Span<int> corner_verts = surface_mesh.corner_verts();
  const Span<int3> surface_corner_tris = surface_mesh.corner_tris();
  VArraySpan<float2> surface_uv_map;
  if (curves_id.surface_uv_map != nullptr) {
    const bke::AttributeAccessor surface_attributes = surface_mesh.attributes();
    surface_uv_map = *surface_attributes.lookup<float2>(curves_id.surface_uv_map,
                                                        bke::AttrDomain::Corner);
  }

  const OffsetIndices points_by_curve = curves.points_by_curve();
  MutableSpan<float3> positions_cu = curves.positions_for_write();
  MutableSpan<float2> surface_uv_coords = curves.surface_uv_coords_for_write();

  const bke::CurvesSurfaceTransforms transforms{curves_ob, &surface_ob};

  switch (attach_mode) {
    case AttachMode::Nearest: {
      bke::BVHTreeFromMesh surface_bvh = surface_mesh.bvh_corner_tris();

      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
        for (const int curve_i : curves_range) {
          const IndexRange points = points_by_curve[curve_i];
          const int first_point_i = points.first();
          const float3 old_first_point_pos_cu = positions_cu[first_point_i];
          const float3 old_first_point_pos_su = math::transform_point(transforms.curves_to_surface,
                                                                      old_first_point_pos_cu);

          BVHTreeNearest nearest;
          nearest.index = -1;
          nearest.dist_sq = FLT_MAX;
          BLI_bvhtree_find_nearest(surface_bvh.tree,
                                   old_first_point_pos_su,
                                   &nearest,
                                   surface_bvh.nearest_callback,
                                   &surface_bvh);
          const int tri_index = nearest.index;
          if (tri_index == -1) {
            continue;
          }

          const float3 new_first_point_pos_su = nearest.co;
          const float3 new_first_point_pos_cu = math::transform_point(transforms.surface_to_curves,
                                                                      new_first_point_pos_su);
          const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;

          for (float3 &pos_cu : positions_cu.slice(points)) {
            pos_cu += pos_diff_cu;
          }

          if (!surface_uv_map.is_empty()) {
            const int3 &tri = surface_corner_tris[tri_index];
            const float3 bary_coords = bke::mesh_surface_sample::compute_bary_coord_in_triangle(
                surface_positions, corner_verts, tri, new_first_point_pos_su);
            const float2 uv = bke::mesh_surface_sample::sample_corner_attribute_with_bary_coords(
                bary_coords, tri, surface_uv_map);
            surface_uv_coords[curve_i] = uv;
          }
        }
      });
      break;
    }
    case AttachMode::Deform: {
      if (surface_uv_map.is_empty()) {
        *r_missing_uvs = true;
        break;
      }
      using geometry::ReverseUVSampler;
      ReverseUVSampler reverse_uv_sampler{surface_uv_map, surface_corner_tris};

      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
        for (const int curve_i : curves_range) {
          const IndexRange points = points_by_curve[curve_i];
          const int first_point_i = points.first();
          const float3 old_first_point_pos_cu = positions_cu[first_point_i];

          const float2 uv = surface_uv_coords[curve_i];
          ReverseUVSampler::Result lookup_result = reverse_uv_sampler.sample(uv);
          if (lookup_result.type != ReverseUVSampler::ResultType::Ok) {
            *r_invalid_uvs = true;
            continue;
          }

          const int3 &tri = surface_corner_tris[lookup_result.tri_index];
          const float3 &bary_coords = lookup_result.bary_weights;

          const float3 &p0_su = surface_positions[corner_verts[tri[0]]];
          const float3 &p1_su = surface_positions[corner_verts[tri[1]]];
          const float3 &p2_su = surface_positions[corner_verts[tri[2]]];

          float3 new_first_point_pos_su;
          interp_v3_v3v3v3(new_first_point_pos_su, p0_su, p1_su, p2_su, bary_coords);
          const float3 new_first_point_pos_cu = math::transform_point(transforms.surface_to_curves,
                                                                      new_first_point_pos_su);

          const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;
          for (float3 &pos_cu : positions_cu.slice(points)) {
            pos_cu += pos_diff_cu;
          }
        }
      });
      break;
    }
  }

  curves.tag_positions_changed();
  DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
}

static wmOperatorStatus snap_curves_to_surface_exec(bContext *C, wmOperator *op)
{
  const AttachMode attach_mode = static_cast<AttachMode>(RNA_enum_get(op->ptr, "attach_mode"));

  bool found_invalid_uvs = false;
  bool found_missing_uvs = false;

  CTX_DATA_BEGIN (C, Object *, curves_ob, selected_objects) {
    if (curves_ob->type != OB_CURVES) {
      continue;
    }
    Curves &curves_id = *static_cast<Curves *>(curves_ob->data);
    if (curves_id.surface == nullptr) {
      continue;
    }
    if (curves_id.surface->type != OB_MESH) {
      continue;
    }
    snap_curves_to_surface_exec_object(
        *curves_ob, *curves_id.surface, attach_mode, &found_invalid_uvs, &found_missing_uvs);
  }
  CTX_DATA_END;

  if (found_missing_uvs) {
    BKE_report(op->reports,
               RPT_ERROR,
               "Curves do not have attachment information that can be used for deformation");
  }
  if (found_invalid_uvs) {
    BKE_report(op->reports, RPT_INFO, "Could not snap some curves to the surface");
  }

  /* Refresh the entire window to also clear eventual modifier and nodes editor warnings. */
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace snap_curves_to_surface

static void CURVES_OT_snap_curves_to_surface(wmOperatorType *ot)
{
  using namespace snap_curves_to_surface;

  ot->name = "Snap Curves to Surface";
  ot->idname = "CURVES_OT_snap_curves_to_surface";
  ot->description = "Move curves so that the first point is exactly on the surface mesh";

  ot->poll = editable_curves_with_surface_poll;
  ot->exec = snap_curves_to_surface_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  static const EnumPropertyItem attach_mode_items[] = {
      {int(AttachMode::Nearest),
       "NEAREST",
       0,
       "Nearest",
       "Find the closest point on the surface for the root point of every curve and move the root "
       "there"},
      {int(AttachMode::Deform),
       "DEFORM",
       0,
       "Deform",
       "Re-attach curves to a deformed surface using the existing attachment information. This "
       "only works when the topology of the surface mesh has not changed"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_enum(ot->srna,
               "attach_mode",
               attach_mode_items,
               int(AttachMode::Nearest),
               "Attach Mode",
               "How to find the point on the surface to attach to");
}

namespace set_selection_domain {

static wmOperatorStatus curves_set_selection_domain_exec(bContext *C, wmOperator *op)
{
  const bke::AttrDomain domain = bke::AttrDomain(RNA_enum_get(op->ptr, "domain"));

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    if (bke::AttrDomain(curves_id->selection_domain) == domain) {
      continue;
    }

    curves_id->selection_domain = char(domain);

    CurvesGeometry &curves = curves_id->geometry.wrap();
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    if (curves.is_empty()) {
      continue;
    }

    /* Adding and removing attributes with the C++ API doesn't affect the active attribute index.
     * In order to make the active attribute consistent before and after the change, save the name
     * and reset the active item afterwards.
     *
     * This would be unnecessary if the active attribute were stored as a string on the ID. */
    AttributeOwner owner = AttributeOwner::from_id(&curves_id->id);
    const std::string active_attribute = BKE_attributes_active_name_get(owner).value_or("");
    for (const StringRef selection_name : get_curves_selection_attribute_names(curves)) {
      if (const GVArray src = *attributes.lookup(selection_name, domain)) {
        const CPPType &type = src.type();
        void *dst = MEM_malloc_arrayN(attributes.domain_size(domain), type.size, __func__);
        src.materialize(dst);

        attributes.remove(selection_name);
        if (!attributes.add(selection_name,
                            domain,
                            bke::cpp_type_to_attribute_type(type),
                            bke::AttributeInitMoveArray(dst)))
        {
          MEM_freeN(dst);
        }
      }
    }
    if (!active_attribute.empty()) {
      BKE_attributes_active_set(owner, active_attribute);
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace set_selection_domain

static void CURVES_OT_set_selection_domain(wmOperatorType *ot)
{
  PropertyRNA *prop;

  ot->name = "Set Select Mode";
  ot->idname = __func__;
  ot->description = "Change the mode used for selection masking in curves sculpt mode";

  ot->exec = set_selection_domain::curves_set_selection_domain_exec;
  ot->poll = editable_curves_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = prop = RNA_def_enum(
      ot->srna, "domain", rna_enum_attribute_curves_domain_items, 0, "Domain", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static bool has_anything_selected(const Span<Curves *> curves_ids)
{
  return std::any_of(curves_ids.begin(), curves_ids.end(), [](const Curves *curves_id) {
    return has_anything_selected(curves_id->geometry.wrap());
  });
}

static wmOperatorStatus select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);

  if (action == SEL_TOGGLE) {
    action = has_anything_selected(unique_curves) ? SEL_DESELECT : SEL_SELECT;
  }

  for (Curves *curves_id : unique_curves) {
    /* (De)select all the curves. */
    select_all(curves_id->geometry.wrap(), bke::AttrDomain(curves_id->selection_domain), action);

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

static void CURVES_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->idname = "CURVES_OT_select_all";
  ot->description = "(De)select all control points";

  ot->exec = select_all_exec;
  ot->poll = editable_curves_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

static wmOperatorStatus select_random_exec(bContext *C, wmOperator *op)
{
  VectorSet<Curves *> unique_curves = curves::get_unique_editable_curves(*C);

  const int seed = RNA_int_get(op->ptr, "seed");
  const float probability = RNA_float_get(op->ptr, "probability");

  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = curves_id->geometry.wrap();
    const bke::AttrDomain selection_domain = bke::AttrDomain(curves_id->selection_domain);
    const int domain_size = curves.attributes().domain_size(selection_domain);

    IndexMaskMemory memory;
    const IndexMask inv_random_elements = random_mask(domain_size, seed, probability, memory)
                                              .complement(IndexRange(domain_size), memory);

    const bool was_anything_selected = has_anything_selected(curves);
    bke::GSpanAttributeWriter selection = ensure_selection_attribute(
        curves, selection_domain, bke::AttrType::Bool);
    if (!was_anything_selected) {
      curves::fill_selection_true(selection.span);
    }

    curves::fill_selection_false(selection.span, inv_random_elements);
    selection.finish();

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

static void select_random_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  layout->prop(op->ptr, "seed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "probability", UI_ITEM_R_SLIDER, IFACE_("Probability"), ICON_NONE);
}

static void CURVES_OT_select_random(wmOperatorType *ot)
{
  ot->name = "Select Random";
  ot->idname = __func__;
  ot->description = "Randomizes existing selection or create new random selection";

  ot->exec = select_random_exec;
  ot->poll = curves::editable_curves_poll;
  ot->ui = select_random_ui;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "seed",
              0,
              INT32_MIN,
              INT32_MAX,
              "Seed",
              "Source of randomness",
              INT32_MIN,
              INT32_MAX);
  RNA_def_float(ot->srna,
                "probability",
                0.5f,
                0.0f,
                1.0f,
                "Probability",
                "Chance of every point or curve being included in the selection",
                0.0f,
                1.0f);
}

static wmOperatorStatus select_ends_exec(bContext *C, wmOperator *op)
{
  VectorSet<Curves *> unique_curves = curves::get_unique_editable_curves(*C);
  const int amount_start = RNA_int_get(op->ptr, "amount_start");
  const int amount_end = RNA_int_get(op->ptr, "amount_end");

  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = curves_id->geometry.wrap();

    IndexMaskMemory memory;
    const IndexMask inverted_end_points_mask = end_points(
        curves, amount_start, amount_end, true, memory);

    const bool was_anything_selected = has_anything_selected(curves);
    bke::GSpanAttributeWriter selection = ensure_selection_attribute(
        curves, bke::AttrDomain::Point, bke::AttrType::Bool);
    if (!was_anything_selected) {
      fill_selection_true(selection.span);
    }

    if (selection.span.type().is<bool>()) {
      index_mask::masked_fill(selection.span.typed<bool>(), false, inverted_end_points_mask);
    }
    if (selection.span.type().is<float>()) {
      index_mask::masked_fill(selection.span.typed<float>(), 0.0f, inverted_end_points_mask);
    }
    selection.finish();

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

static void select_ends_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;

  layout->use_property_split_set(true);

  uiLayout *col = &layout->column(true);
  col->use_property_decorate_set(false);
  col->prop(op->ptr, "amount_start", UI_ITEM_NONE, IFACE_("Amount Start"), ICON_NONE);
  col->prop(op->ptr, "amount_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
}

static void CURVES_OT_select_ends(wmOperatorType *ot)
{
  ot->name = "Select Ends";
  ot->idname = __func__;
  ot->description = "Select end points of curves";

  ot->exec = select_ends_exec;
  ot->ui = select_ends_ui;
  ot->poll = editable_curves_point_domain_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_int(ot->srna,
              "amount_start",
              0,
              0,
              INT32_MAX,
              "Amount Front",
              "Number of points to select from the front",
              0,
              INT32_MAX);
  RNA_def_int(ot->srna,
              "amount_end",
              1,
              0,
              INT32_MAX,
              "Amount Back",
              "Number of points to select from the back",
              0,
              INT32_MAX);
}

static wmOperatorStatus select_linked_exec(bContext *C, wmOperator * /*op*/)
{
  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);
  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = curves_id->geometry.wrap();
    select_linked(curves);
    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

static void CURVES_OT_select_linked(wmOperatorType *ot)
{
  ot->name = "Select Linked";
  ot->idname = __func__;
  ot->description = "Select all points in curves with any point selection";

  ot->exec = select_linked_exec;
  ot->poll = editable_curves_point_domain_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus select_more_exec(bContext *C, wmOperator * /*op*/)
{
  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);
  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = curves_id->geometry.wrap();
    select_adjacent(curves, false);
    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

static void CURVES_OT_select_more(wmOperatorType *ot)
{
  ot->name = "Select More";
  ot->idname = __func__;
  ot->description = "Grow the selection by one point";

  ot->exec = select_more_exec;
  ot->poll = editable_curves_point_domain_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus select_less_exec(bContext *C, wmOperator * /*op*/)
{
  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);
  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = curves_id->geometry.wrap();
    select_adjacent(curves, true);
    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

static void CURVES_OT_select_less(wmOperatorType *ot)
{
  ot->name = "Select Less";
  ot->idname = __func__;
  ot->description = "Shrink the selection by one point";

  ot->exec = select_less_exec;
  ot->poll = editable_curves_point_domain_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace split {

static wmOperatorStatus split_exec(bContext *C, wmOperator * /*op*/)
{
  View3D *v3d = CTX_wm_view3d(C);
  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);
  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = curves_id->geometry.wrap();
    IndexMaskMemory memory;
    const IndexMask points_to_split = retrieve_all_selected_points(
        curves, v3d->overlay.handle_display, memory);
    if (points_to_split.is_empty()) {
      continue;
    }
    curves = split_points(curves, points_to_split);

    curves.calculate_bezier_auto_handles();

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

}  // namespace split

static void CURVES_OT_split(wmOperatorType *ot)
{
  ot->name = "Split";
  ot->idname = __func__;
  ot->description = "Split selected points";

  ot->exec = split::split_exec;
  ot->poll = editable_curves_point_domain_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace surface_set {

static bool surface_set_poll(bContext *C)
{
  const Object *object = CTX_data_active_object(C);
  if (object == nullptr) {
    return false;
  }
  if (object->type != OB_MESH) {
    return false;
  }
  return true;
}

static wmOperatorStatus surface_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  Object &new_surface_ob = *CTX_data_active_object(C);

  Mesh &new_surface_mesh = *static_cast<Mesh *>(new_surface_ob.data);
  const StringRef new_uv_map_name = new_surface_mesh.active_uv_map_name();

  CTX_DATA_BEGIN (C, Object *, selected_ob, selected_objects) {
    if (selected_ob->type != OB_CURVES) {
      continue;
    }
    Object &curves_ob = *selected_ob;
    Curves &curves_id = *static_cast<Curves *>(curves_ob.data);

    MEM_SAFE_FREE(curves_id.surface_uv_map);
    if (!new_uv_map_name.is_empty()) {
      curves_id.surface_uv_map = BLI_strdupn(new_uv_map_name.data(), new_uv_map_name.size());
    }

    bool missing_uvs;
    bool invalid_uvs;
    snap_curves_to_surface::snap_curves_to_surface_exec_object(
        curves_ob,
        new_surface_ob,
        snap_curves_to_surface::AttachMode::Nearest,
        &invalid_uvs,
        &missing_uvs);

    /* Add deformation modifier if necessary. */
    ensure_surface_deformation_node_exists(*C, curves_ob);

    curves_id.surface = &new_surface_ob;
    object::parent_set(op->reports,
                       C,
                       scene,
                       &curves_ob,
                       &new_surface_ob,
                       object::PAR_OBJECT,
                       false,
                       true,
                       nullptr);

    DEG_id_tag_update(&curves_ob.id, ID_RECALC_TRANSFORM);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, &curves_id);
    WM_event_add_notifier(C, NC_NODE | NA_ADDED, nullptr);

    /* Required for deformation. */
    new_surface_ob.modifier_flag |= OB_MODIFIER_FLAG_ADD_REST_POSITION;
    DEG_id_tag_update(&new_surface_ob.id, ID_RECALC_GEOMETRY);
  }
  CTX_DATA_END;

  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

}  // namespace surface_set

static void CURVES_OT_surface_set(wmOperatorType *ot)
{
  ot->name = "Set Curves Surface Object";
  ot->idname = __func__;
  ot->description =
      "Use the active object as surface for selected curves objects and set it as the parent";

  ot->exec = surface_set::surface_set_exec;
  ot->poll = surface_set::surface_set_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace curves_delete {

static wmOperatorStatus delete_exec(bContext *C, wmOperator * /*op*/)
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    if (remove_selection(curves, bke::AttrDomain(curves_id->selection_domain))) {
      DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
      WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
    }
  }

  return OPERATOR_FINISHED;
}

}  // namespace curves_delete

static void CURVES_OT_delete(wmOperatorType *ot)
{
  ot->name = "Delete";
  ot->idname = __func__;
  ot->description = "Remove selected control points or curves";

  ot->exec = curves_delete::delete_exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace curves_duplicate {

static wmOperatorStatus duplicate_exec(bContext *C, wmOperator * /*op*/)
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    IndexMaskMemory memory;
    switch (bke::AttrDomain(curves_id->selection_domain)) {
      case bke::AttrDomain::Point:
        duplicate_points(curves, retrieve_selected_points(*curves_id, memory));
        break;
      case bke::AttrDomain::Curve:
        duplicate_curves(curves, retrieve_selected_curves(*curves_id, memory));
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace curves_duplicate

static void CURVES_OT_duplicate(wmOperatorType *ot)
{
  ot->name = "Duplicate";
  ot->idname = __func__;
  ot->description = "Copy selected points or curves";

  ot->exec = curves_duplicate::duplicate_exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace clear_tilt {

static wmOperatorStatus exec(bContext *C, wmOperator * /*op*/)
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_points(*curves_id, memory);
    if (selection.is_empty()) {
      continue;
    }

    if (selection.size() == curves.points_num()) {
      curves.attributes_for_write().remove("tilt");
    }
    else {
      index_mask::masked_fill(curves.tilt_for_write(), 0.0f, selection);
    }

    curves.tag_normals_changed();
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace clear_tilt

static void CURVES_OT_tilt_clear(wmOperatorType *ot)
{
  ot->name = "Clear Tilt";
  ot->idname = __func__;
  ot->description = "Clear the tilt of selected control points";

  ot->exec = clear_tilt::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace cyclic_toggle {

static wmOperatorStatus exec(bContext *C, wmOperator * /*op*/)
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_curves(*curves_id, memory);
    if (selection.is_empty()) {
      continue;
    }

    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    bke::SpanAttributeWriter<bool> cyclic = attributes.lookup_or_add_for_write_span<bool>(
        "cyclic", bke::AttrDomain::Curve);
    selection.foreach_index(GrainSize(4096),
                            [&](const int i) { cyclic.span[i] = !cyclic.span[i]; });
    cyclic.finish();

    if (!cyclic.span.contains(true)) {
      attributes.remove("cyclic");
    }

    curves.calculate_bezier_auto_handles();

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace cyclic_toggle

static void CURVES_OT_cyclic_toggle(wmOperatorType *ot)
{
  ot->name = "Toggle Cyclic";
  ot->idname = __func__;
  ot->description = "Make active curve closed/opened loop";

  ot->exec = cyclic_toggle::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace curve_type_set {

static wmOperatorStatus exec(bContext *C, wmOperator *op)
{
  const CurveType dst_type = CurveType(RNA_enum_get(op->ptr, "type"));
  const bool use_handles = RNA_boolean_get(op->ptr, "use_handles");

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_curves(*curves_id, memory);
    if (selection.is_empty()) {
      continue;
    }

    geometry::ConvertCurvesOptions options;
    options.convert_bezier_handles_to_poly_points = use_handles;
    options.convert_bezier_handles_to_catmull_rom_points = use_handles;
    options.keep_bezier_shape_as_nurbs = use_handles;
    options.keep_catmull_rom_shape_as_nurbs = use_handles;

    curves = geometry::convert_curves(curves, selection, dst_type, {}, options);

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace curve_type_set

static void CURVES_OT_curve_type_set(wmOperatorType *ot)
{
  ot->name = "Set Curve Type";
  ot->idname = __func__;
  ot->description = "Set type of selected curves";

  ot->exec = curve_type_set::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_curves_type_items, CURVE_TYPE_POLY, "Type", "Curve type");

  RNA_def_boolean(ot->srna,
                  "use_handles",
                  false,
                  "Handles",
                  "Take handle information into account in the conversion");
}

namespace switch_direction {

static wmOperatorStatus exec(bContext *C, wmOperator * /*op*/)
{
  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    IndexMaskMemory memory;
    const IndexMask selection = retrieve_selected_curves(*curves_id, memory);
    if (selection.is_empty()) {
      continue;
    }

    curves.reverse_curves(selection);

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace switch_direction

static void CURVES_OT_switch_direction(wmOperatorType *ot)
{
  ot->name = "Switch Direction";
  ot->idname = __func__;
  ot->description = "Reverse the direction of the selected curves";

  ot->exec = switch_direction::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

namespace subdivide {

static wmOperatorStatus exec(bContext *C, wmOperator *op)
{
  const int number_cuts = RNA_int_get(op->ptr, "number_cuts");

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    const int points_num = curves.points_num();
    IndexMaskMemory memory;
    const IndexMask points_selection = retrieve_selected_points(*curves_id, memory);
    if (points_selection.is_empty()) {
      continue;
    }

    Array<bool> points_selection_span(points_num);
    points_selection.to_bools(points_selection_span);

    Array<int> segment_cuts(points_num, number_cuts);

    const OffsetIndices points_by_curve = curves.points_by_curve();
    threading::parallel_for(points_by_curve.index_range(), 512, [&](const IndexRange range) {
      for (const int curve_i : range) {
        const IndexRange points = points_by_curve[curve_i];
        if (points.size() <= 1) {
          continue;
        }
        for (const int point_i : points.drop_back(1)) {
          if (!points_selection_span[point_i] || !points_selection_span[point_i + 1]) {
            segment_cuts[point_i] = 0;
          }
        }
        /* Cyclic segment. Doesn't matter if it is computed even if the curve is not cyclic. */
        if (!points_selection_span[points.last()] || !points_selection_span[points.first()]) {
          segment_cuts[points.last()] = 0;
        }
      }
    });

    curves = geometry::subdivide_curves(
        curves, curves.curves_range(), VArray<int>::from_span(segment_cuts), {});

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace subdivide

static void CURVES_OT_subdivide(wmOperatorType *ot)
{
  ot->name = "Subdivide";
  ot->idname = __func__;
  ot->description = "Subdivide selected curve segments";

  ot->exec = subdivide::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop;
  prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, 1000, "Number of Cuts", "", 1, 10);
  /* Avoid re-using last value because it can cause an unexpectedly high number of subdivisions. */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/** Add new curves primitive to an existing curves object in edit mode. */
static void append_primitive_curve(bContext *C,
                                   Curves &curves_id,
                                   CurvesGeometry new_curves,
                                   wmOperator &op)
{
  const int new_points_num = new_curves.points_num();
  const int new_curves_num = new_curves.curves_num();

  /* Create geometry sets so that generic join code can be used. */
  bke::GeometrySet old_geometry = bke::GeometrySet::from_curves(
      &curves_id, bke::GeometryOwnershipType::ReadOnly);
  bke::GeometrySet new_geometry = bke::GeometrySet::from_curves(
      bke::curves_new_nomain(std::move(new_curves)));

  /* Transform primitive according to settings. */
  float3 location;
  float3 rotation;
  float3 scale;
  object::add_generic_get_opts(C, &op, 'Z', location, rotation, scale, nullptr, nullptr, nullptr);
  const float4x4 transform = math::from_loc_rot_scale<float4x4>(
      location, math::EulerXYZ(rotation), scale);
  geometry::transform_geometry(new_geometry, transform);

  bke::GeometrySet joined_geometry = geometry::join_geometries({old_geometry, new_geometry}, {});
  Curves *joined_curves_id = joined_geometry.get_curves_for_write();
  CurvesGeometry &dst_curves = curves_id.geometry.wrap();
  dst_curves = std::move(joined_curves_id->geometry.wrap());

  /* Only select the new curves. */
  const bke::AttrDomain selection_domain = bke::AttrDomain(curves_id.selection_domain);
  const int new_element_num = selection_domain == bke::AttrDomain::Point ? new_points_num :
                                                                           new_curves_num;
  foreach_selection_attribute_writer(
      dst_curves, selection_domain, [&](bke::GSpanAttributeWriter &selection) {
        fill_selection_false(selection.span.drop_back(new_element_num));
        fill_selection_true(selection.span.take_back(new_element_num));
      });

  dst_curves.tag_topology_changed();
}

namespace add_circle {

static CurvesGeometry generate_circle_primitive(const float radius)
{
  CurvesGeometry curves{4, 1};

  MutableSpan<int> offsets = curves.offsets_for_write();
  offsets[0] = 0;
  offsets[1] = 4;

  curves.fill_curve_types(CURVE_TYPE_BEZIER);
  curves.cyclic_for_write().fill(true);
  curves.handle_types_left_for_write().fill(BEZIER_HANDLE_AUTO);
  curves.handle_types_right_for_write().fill(BEZIER_HANDLE_AUTO);
  curves.resolution_for_write().fill(12);

  MutableSpan<float3> positions = curves.positions_for_write();
  positions[0] = float3(-radius, 0, 0);
  positions[1] = float3(0, radius, 0);
  positions[2] = float3(radius, 0, 0);
  positions[3] = float3(0, -radius, 0);

  /* Ensure these attributes exist. */
  curves.handle_positions_left_for_write();
  curves.handle_positions_right_for_write();

  curves.calculate_bezier_auto_handles();

  return curves;
}

static wmOperatorStatus exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_edit_object(C);
  Curves *active_curves_id = static_cast<Curves *>(object->data);

  const float radius = RNA_float_get(op->ptr, "radius");
  append_primitive_curve(C, *active_curves_id, generate_circle_primitive(radius), *op);

  DEG_id_tag_update(&active_curves_id->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, active_curves_id);
  return OPERATOR_FINISHED;
}

}  // namespace add_circle

static void CURVES_OT_add_circle(wmOperatorType *ot)
{
  ot->name = "Add Circle";
  ot->idname = __func__;
  ot->description = "Add new circle curve";

  ot->exec = add_circle::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  object::add_unit_props_radius(ot);
  object::add_generic_props(ot, true);
}

namespace add_bezier {

static CurvesGeometry generate_bezier_primitive(const float radius)
{
  CurvesGeometry curves{2, 1};

  MutableSpan<int> offsets = curves.offsets_for_write();
  offsets[0] = 0;
  offsets[1] = 2;

  curves.fill_curve_types(CURVE_TYPE_BEZIER);
  curves.handle_types_left_for_write().fill(BEZIER_HANDLE_ALIGN);
  curves.handle_types_right_for_write().fill(BEZIER_HANDLE_ALIGN);
  curves.resolution_for_write().fill(12);

  MutableSpan<float3> positions = curves.positions_for_write();
  MutableSpan<float3> left_handles = curves.handle_positions_left_for_write();
  MutableSpan<float3> right_handles = curves.handle_positions_right_for_write();

  left_handles[0] = float3(-1.5f, -0.5, 0) * radius;
  positions[0] = float3(-1.0f, 0, 0) * radius;
  right_handles[0] = float3(-0.5f, 0.5f, 0) * radius;

  left_handles[1] = float3(0, 0, 0) * radius;
  positions[1] = float3(1.0f, 0, 0) * radius;
  right_handles[1] = float3(2.0f, 0, 0) * radius;

  return curves;
}

static wmOperatorStatus exec(bContext *C, wmOperator *op)
{
  Object *object = CTX_data_edit_object(C);
  Curves *active_curves_id = static_cast<Curves *>(object->data);

  const float radius = RNA_float_get(op->ptr, "radius");
  append_primitive_curve(C, *active_curves_id, generate_bezier_primitive(radius), *op);

  DEG_id_tag_update(&active_curves_id->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, active_curves_id);
  return OPERATOR_FINISHED;
}

}  // namespace add_bezier

static void CURVES_OT_add_bezier(wmOperatorType *ot)
{
  ot->name = "Add Bézier";
  ot->idname = __func__;
  ot->description = "Add new Bézier curve";

  ot->exec = add_bezier::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  object::add_unit_props_radius(ot);
  object::add_generic_props(ot, true);
}

namespace set_handle_type {

static wmOperatorStatus exec(bContext *C, wmOperator *op)
{
  const SetHandleType dst_type = SetHandleType(RNA_enum_get(op->ptr, "type"));

  auto new_handle_type = [&](const int8_t handle_type) {
    switch (dst_type) {
      case SetHandleType::Free:
        return int8_t(BEZIER_HANDLE_FREE);
      case SetHandleType::Auto:
        return int8_t(BEZIER_HANDLE_AUTO);
      case SetHandleType::Vector:
        return int8_t(BEZIER_HANDLE_VECTOR);
      case SetHandleType::Align:
        return int8_t(BEZIER_HANDLE_ALIGN);
      case SetHandleType::Toggle:
        return int8_t(handle_type == BEZIER_HANDLE_FREE ? BEZIER_HANDLE_ALIGN :
                                                          BEZIER_HANDLE_FREE);
    }
    BLI_assert_unreachable();
    return int8_t(0);
  };

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();
    const bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

    const VArraySpan<bool> selection = *attributes.lookup_or_default<bool>(
        ".selection", bke::AttrDomain::Point, true);
    const VArraySpan<bool> selection_left = *attributes.lookup_or_default<bool>(
        ".selection_handle_left", bke::AttrDomain::Point, true);
    const VArraySpan<bool> selection_right = *attributes.lookup_or_default<bool>(
        ".selection_handle_right", bke::AttrDomain::Point, true);

    MutableSpan<int8_t> handle_types_left = curves.handle_types_left_for_write();
    MutableSpan<int8_t> handle_types_right = curves.handle_types_right_for_write();

    threading::parallel_for(curves.points_range(), 4096, [&](const IndexRange range) {
      for (const int point_i : range) {
        if (selection_left[point_i] || selection[point_i]) {
          handle_types_left[point_i] = new_handle_type(handle_types_left[point_i]);
        }
        if (selection_right[point_i] || selection[point_i]) {
          handle_types_right[point_i] = new_handle_type(handle_types_right[point_i]);
        }
      }
    });

    curves.calculate_bezier_auto_handles();
    curves.tag_topology_changed();

    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }
  return OPERATOR_FINISHED;
}

}  // namespace set_handle_type

const EnumPropertyItem rna_enum_set_handle_type_items[] = {
    {int(SetHandleType::Auto),
     "AUTO",
     ICON_HANDLE_AUTO,
     "Auto",
     "The location is automatically calculated to be smooth"},
    {int(SetHandleType::Vector),
     "VECTOR",
     ICON_HANDLE_VECTOR,
     "Vector",
     "The location is calculated to point to the next/previous control point"},
    {int(SetHandleType::Align),
     "ALIGN",
     ICON_HANDLE_ALIGNED,
     "Align",
     "The location is constrained to point in the opposite direction as the other handle"},
    {int(SetHandleType::Free),
     "FREE_ALIGN",
     ICON_HANDLE_FREE,
     "Free",
     "The handle can be moved anywhere, and does not influence the point's other handle"},
    {int(SetHandleType::Toggle),
     "TOGGLE_FREE_ALIGN",
     0,
     "Toggle Free/Align",
     "Replace Free handles with Align, and all Align with Free handles"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void CURVES_OT_handle_type_set(wmOperatorType *ot)
{
  ot->name = "Set Handle Type";
  ot->idname = __func__;
  ot->description = "Set the handle type for bezier curves";

  ot->invoke = WM_menu_invoke;
  ot->exec = set_handle_type::exec;
  ot->poll = editable_curves_in_edit_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna,
                          "type",
                          rna_enum_set_handle_type_items,
                          int(ed::curves::SetHandleType::Auto),
                          "Type",
                          nullptr);
}

void operatortypes_curves()
{
  WM_operatortype_append(CURVES_OT_attribute_set);
  WM_operatortype_append(CURVES_OT_convert_to_particle_system);
  WM_operatortype_append(CURVES_OT_convert_from_particle_system);
  WM_operatortype_append(CURVES_OT_draw);
  WM_operatortype_append(CURVES_OT_extrude);
  WM_operatortype_append(CURVES_OT_snap_curves_to_surface);
  WM_operatortype_append(CURVES_OT_set_selection_domain);
  WM_operatortype_append(CURVES_OT_select_all);
  WM_operatortype_append(CURVES_OT_select_random);
  WM_operatortype_append(CURVES_OT_select_ends);
  WM_operatortype_append(CURVES_OT_select_linked);
  WM_operatortype_append(CURVES_OT_select_linked_pick);
  WM_operatortype_append(CURVES_OT_select_more);
  WM_operatortype_append(CURVES_OT_select_less);
  WM_operatortype_append(CURVES_OT_separate);
  WM_operatortype_append(CURVES_OT_split);
  WM_operatortype_append(CURVES_OT_surface_set);
  WM_operatortype_append(CURVES_OT_delete);
  WM_operatortype_append(CURVES_OT_duplicate);
  WM_operatortype_append(CURVES_OT_tilt_clear);
  WM_operatortype_append(CURVES_OT_cyclic_toggle);
  WM_operatortype_append(CURVES_OT_curve_type_set);
  WM_operatortype_append(CURVES_OT_switch_direction);
  WM_operatortype_append(CURVES_OT_subdivide);
  WM_operatortype_append(CURVES_OT_add_circle);
  WM_operatortype_append(CURVES_OT_add_bezier);
  WM_operatortype_append(CURVES_OT_handle_type_set);

  ED_operatortypes_curves_pen();
}

void operatormacros_curves()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("CURVES_OT_duplicate_move",
                                    "Duplicate",
                                    "Make copies of selected elements and move them",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CURVES_OT_duplicate");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);

  ot = WM_operatortype_append_macro("CURVES_OT_extrude_move",
                                    "Extrude Curve and Move",
                                    "Extrude curve and move result",
                                    OPTYPE_UNDO | OPTYPE_REGISTER);
  WM_operatortype_macro_define(ot, "CURVES_OT_extrude");
  otmacro = WM_operatortype_macro_define(ot, "TRANSFORM_OT_translate");
  RNA_boolean_set(otmacro->ptr, "use_proportional_edit", false);
  RNA_boolean_set(otmacro->ptr, "mirror", false);
}

void keymap_curves(wmKeyConfig *keyconf)
{
  /* Only set in editmode curves, by space_view3d listener. */
  wmKeyMap *keymap = WM_keymap_ensure(keyconf, "Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = editable_curves_in_edit_mode_poll;

  ED_curves_pentool_modal_keymap(keyconf);
}

}  // namespace blender::ed::curves

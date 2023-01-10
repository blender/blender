/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include <atomic>

#include "BLI_array_utils.hh"
#include "BLI_index_mask_ops.hh"
#include "BLI_utildefines.h"
#include "BLI_vector_set.hh"

#include "ED_curves.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_select_utils.h"

#include "WM_api.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"
#include "BKE_mesh_legacy_convert.h"
#include "BKE_mesh_runtime.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_report.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "GEO_reverse_uv_sampler.hh"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * `cu`: Local space of the curves object that is being edited.
 * `su`: Local space of the surface object.
 * `wo`: World space.
 * `ha`: Local space of an individual hair in the legacy hair system.
 */

namespace blender::ed::curves {

static bool object_has_editable_curves(const Main &bmain, const Object &object)
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

static bool curves_poll_impl(bContext *C, const bool check_editable, const bool check_surface)
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
  return true;
}

bool editable_curves_with_surface_poll(bContext *C)
{
  return curves_poll_impl(C, true, true);
}

bool curves_with_surface_poll(bContext *C)
{
  return curves_poll_impl(C, false, true);
}

bool editable_curves_poll(bContext *C)
{
  return curves_poll_impl(C, false, false);
}

bool curves_poll(bContext *C)
{
  return curves_poll_impl(C, false, false);
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
  CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
  if (curves_id.surface == nullptr) {
    return;
  }
  Object &surface_ob = *curves_id.surface;
  if (surface_ob.type != OB_MESH) {
    return;
  }
  Mesh &surface_me = *static_cast<Mesh *>(surface_ob.data);

  BVHTreeFromMesh surface_bvh;
  BKE_bvhtree_from_mesh_get(&surface_bvh, &surface_me, BVHTREE_FROM_LOOPTRI, 2);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

  const Span<float3> positions_cu = curves.positions();
  const Span<MLoopTri> looptris = surface_me.looptris();

  if (looptris.is_empty()) {
    *r_could_not_convert_some_curves = true;
  }

  const int hair_num = curves.curves_num();
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

  MutableSpan<ParticleData> particles{
      static_cast<ParticleData *>(MEM_calloc_arrayN(hair_num, sizeof(ParticleData), __func__)),
      hair_num};

  /* The old hair system still uses #MFace, so make sure those are available on the mesh. */
  BKE_mesh_tessface_calc(&surface_me);

  /* Prepare utility data structure to map hair roots to #MFace's. */
  const Span<int> mface_to_poly_map{
      static_cast<const int *>(CustomData_get_layer(&surface_me.fdata, CD_ORIGINDEX)),
      surface_me.totface};
  Array<Vector<int>> poly_to_mface_map(surface_me.totpoly);
  for (const int mface_i : mface_to_poly_map.index_range()) {
    const int poly_i = mface_to_poly_map[mface_i];
    poly_to_mface_map[poly_i].append(mface_i);
  }

  /* Prepare transformation matrices. */
  const bke::CurvesSurfaceTransforms transforms{curves_ob, &surface_ob};

  const MFace *mfaces = (const MFace *)CustomData_get_layer(&surface_me.fdata, CD_MFACE);
  const Span<float3> positions = surface_me.vert_positions();

  for (const int new_hair_i : IndexRange(hair_num)) {
    const int curve_i = new_hair_i;
    const IndexRange points = curves.points_for_curve(curve_i);

    const float3 &root_pos_cu = positions_cu[points.first()];
    const float3 root_pos_su = transforms.curves_to_surface * root_pos_cu;

    BVHTreeNearest nearest;
    nearest.dist_sq = FLT_MAX;
    BLI_bvhtree_find_nearest(
        surface_bvh.tree, root_pos_su, &nearest, surface_bvh.nearest_callback, &surface_bvh);
    BLI_assert(nearest.index >= 0);

    const int looptri_i = nearest.index;
    const MLoopTri &looptri = looptris[looptri_i];
    const int poly_i = looptri.poly;

    const int mface_i = find_mface_for_root_position(
        positions, mfaces, poly_to_mface_map[poly_i], root_pos_su);
    const MFace &mface = mfaces[mface_i];

    const float4 mface_weights = compute_mface_weights_for_position(positions, mface, root_pos_su);

    ParticleData &particle = particles[new_hair_i];
    const int num_keys = points.size();
    MutableSpan<HairKey> hair_keys{
        static_cast<HairKey *>(MEM_calloc_arrayN(num_keys, sizeof(HairKey), __func__)), num_keys};

    particle.hair = hair_keys.data();
    particle.totkey = hair_keys.size();
    copy_v4_v4(particle.fuv, mface_weights);
    particle.num = mface_i;
    /* Not sure if there is a better way to initialize this. */
    particle.num_dmcache = DMCACHE_NOTFOUND;

    float4x4 hair_to_surface_mat;
    psys_mat_hair_to_object(
        &surface_ob, &surface_me, PART_FROM_FACE, &particle, hair_to_surface_mat.values);
    /* In theory, #psys_mat_hair_to_object should handle this, but it doesn't right now. */
    copy_v3_v3(hair_to_surface_mat.values[3], root_pos_su);
    const float4x4 surface_to_hair_mat = hair_to_surface_mat.inverted();

    for (const int key_i : hair_keys.index_range()) {
      const float3 &key_pos_cu = positions_cu[points[key_i]];
      const float3 key_pos_su = transforms.curves_to_surface * key_pos_cu;
      const float3 key_pos_ha = surface_to_hair_mat * key_pos_su;

      HairKey &key = hair_keys[key_i];
      copy_v3_v3(key.co, key_pos_ha);
      key.time = 100.0f * key_i / float(hair_keys.size() - 1);
    }
  }

  particle_system->particles = particles.data();
  particle_system->totpart = particles.size();
  particle_system->flag |= PSYS_EDITED;
  particle_system->recalc |= ID_RECALC_PSYS_RESET;

  DEG_id_tag_update(&surface_ob.id, ID_RECALC_GEOMETRY);
  DEG_id_tag_update(&settings.id, ID_RECALC_COPY_ON_WRITE);
}

static int curves_convert_to_particle_system_exec(bContext *C, wmOperator *op)
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

  const float4x4 object_to_world_mat = object.object_to_world;
  const float4x4 world_to_object_mat = object_to_world_mat.inverted();

  MutableSpan<float3> positions = curves.positions_for_write();

  const auto copy_hair_to_curves = [&](const Span<ParticleCacheKey *> hair_cache,
                                       const Span<int> indices_to_transfer,
                                       const int curve_index_offset) {
    threading::parallel_for(indices_to_transfer.index_range(), 256, [&](const IndexRange range) {
      for (const int i : range) {
        const int hair_i = indices_to_transfer[i];
        const int curve_i = i + curve_index_offset;
        const IndexRange points = curves.points_for_curve(curve_i);
        const Span<ParticleCacheKey> keys{hair_cache[hair_i], points.size()};
        for (const int key_i : keys.index_range()) {
          const float3 key_pos_wo = keys[key_i].co;
          positions[points[key_i]] = world_to_object_mat * key_pos_wo;
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

static int curves_convert_from_particle_system_exec(bContext *C, wmOperator * /*op*/)
{
  Main &bmain = *CTX_data_main(C);
  Scene &scene = *CTX_data_scene(C);
  ViewLayer &view_layer = *CTX_data_view_layer(C);
  Depsgraph &depsgraph = *CTX_data_depsgraph_pointer(C);
  Object *ob_from_orig = ED_object_active_context(C);
  ParticleSystem *psys_orig = static_cast<ParticleSystem *>(
      CTX_data_pointer_get_type(C, "particle_system", &RNA_ParticleSystem).data);
  if (psys_orig == nullptr) {
    psys_orig = psys_get_current(ob_from_orig);
  }
  if (psys_orig == nullptr) {
    return OPERATOR_CANCELLED;
  }
  Object *ob_from_eval = DEG_get_evaluated_object(&depsgraph, ob_from_orig);
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
  BKE_object_apply_mat4(ob_new, ob_from_orig->object_to_world, true, false);
  bke::CurvesGeometry::wrap(curves_id->geometry) = particles_to_curves(*ob_from_eval, *psys_eval);

  DEG_relations_tag_update(&bmain);
  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

static bool curves_convert_from_particle_system_poll(bContext *C)
{
  return ED_object_active_context(C) != nullptr;
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
  Nearest,
  Deform,
};

static void snap_curves_to_surface_exec_object(Object &curves_ob,
                                               const Object &surface_ob,
                                               const AttachMode attach_mode,
                                               bool *r_invalid_uvs,
                                               bool *r_missing_uvs)
{
  Curves &curves_id = *static_cast<Curves *>(curves_ob.data);
  CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);

  const Mesh &surface_mesh = *static_cast<const Mesh *>(surface_ob.data);
  const Span<float3> surface_positions = surface_mesh.vert_positions();
  const Span<MLoop> loops = surface_mesh.loops();
  const Span<MLoopTri> surface_looptris = surface_mesh.looptris();
  VArraySpan<float2> surface_uv_map;
  if (curves_id.surface_uv_map != nullptr) {
    const bke::AttributeAccessor surface_attributes = surface_mesh.attributes();
    surface_uv_map = surface_attributes
                         .lookup(curves_id.surface_uv_map, ATTR_DOMAIN_CORNER, CD_PROP_FLOAT2)
                         .typed<float2>();
  }

  MutableSpan<float3> positions_cu = curves.positions_for_write();
  MutableSpan<float2> surface_uv_coords = curves.surface_uv_coords_for_write();

  const bke::CurvesSurfaceTransforms transforms{curves_ob, &surface_ob};

  switch (attach_mode) {
    case AttachMode::Nearest: {
      BVHTreeFromMesh surface_bvh;
      BKE_bvhtree_from_mesh_get(&surface_bvh, &surface_mesh, BVHTREE_FROM_LOOPTRI, 2);
      BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&surface_bvh); });

      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
        for (const int curve_i : curves_range) {
          const IndexRange points = curves.points_for_curve(curve_i);
          const int first_point_i = points.first();
          const float3 old_first_point_pos_cu = positions_cu[first_point_i];
          const float3 old_first_point_pos_su = transforms.curves_to_surface *
                                                old_first_point_pos_cu;

          BVHTreeNearest nearest;
          nearest.index = -1;
          nearest.dist_sq = FLT_MAX;
          BLI_bvhtree_find_nearest(surface_bvh.tree,
                                   old_first_point_pos_su,
                                   &nearest,
                                   surface_bvh.nearest_callback,
                                   &surface_bvh);
          const int looptri_index = nearest.index;
          if (looptri_index == -1) {
            continue;
          }

          const float3 new_first_point_pos_su = nearest.co;
          const float3 new_first_point_pos_cu = transforms.surface_to_curves *
                                                new_first_point_pos_su;
          const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;

          for (float3 &pos_cu : positions_cu.slice(points)) {
            pos_cu += pos_diff_cu;
          }

          if (!surface_uv_map.is_empty()) {
            const MLoopTri &looptri = surface_looptris[looptri_index];
            const int corner0 = looptri.tri[0];
            const int corner1 = looptri.tri[1];
            const int corner2 = looptri.tri[2];
            const float2 &uv0 = surface_uv_map[corner0];
            const float2 &uv1 = surface_uv_map[corner1];
            const float2 &uv2 = surface_uv_map[corner2];
            const float3 &p0_su = surface_positions[loops[corner0].v];
            const float3 &p1_su = surface_positions[loops[corner1].v];
            const float3 &p2_su = surface_positions[loops[corner2].v];
            float3 bary_coords;
            interp_weights_tri_v3(bary_coords, p0_su, p1_su, p2_su, new_first_point_pos_su);
            const float2 uv = attribute_math::mix3(bary_coords, uv0, uv1, uv2);
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
      ReverseUVSampler reverse_uv_sampler{surface_uv_map, surface_looptris};

      threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
        for (const int curve_i : curves_range) {
          const IndexRange points = curves.points_for_curve(curve_i);
          const int first_point_i = points.first();
          const float3 old_first_point_pos_cu = positions_cu[first_point_i];

          const float2 uv = surface_uv_coords[curve_i];
          ReverseUVSampler::Result lookup_result = reverse_uv_sampler.sample(uv);
          if (lookup_result.type != ReverseUVSampler::ResultType::Ok) {
            *r_invalid_uvs = true;
            continue;
          }

          const MLoopTri &looptri = surface_looptris[lookup_result.looptri_index];
          const float3 &bary_coords = lookup_result.bary_weights;

          const float3 &p0_su = surface_positions[loops[looptri.tri[0]].v];
          const float3 &p1_su = surface_positions[loops[looptri.tri[1]].v];
          const float3 &p2_su = surface_positions[loops[looptri.tri[2]].v];

          float3 new_first_point_pos_su;
          interp_v3_v3v3v3(new_first_point_pos_su, p0_su, p1_su, p2_su, bary_coords);
          const float3 new_first_point_pos_cu = transforms.surface_to_curves *
                                                new_first_point_pos_su;

          const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;
          for (float3 &pos_cu : positions_cu.slice(points)) {
            pos_cu += pos_diff_cu;
          }
        }
      });
      break;
    }
  }

  DEG_id_tag_update(&curves_id.id, ID_RECALC_GEOMETRY);
}

static int snap_curves_to_surface_exec(bContext *C, wmOperator *op)
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

static int curves_set_selection_domain_exec(bContext *C, wmOperator *op)
{
  const eAttrDomain domain = eAttrDomain(RNA_enum_get(op->ptr, "domain"));

  for (Curves *curves_id : get_unique_editable_curves(*C)) {
    if (curves_id->selection_domain == domain) {
      continue;
    }

    curves_id->selection_domain = domain;

    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    if (curves.points_num() == 0) {
      continue;
    }
    const GVArray src = attributes.lookup(".selection", domain);
    if (src.is_empty()) {
      continue;
    }

    const CPPType &type = src.type();
    void *dst = MEM_malloc_arrayN(attributes.domain_size(domain), type.size(), __func__);
    src.materialize(dst);

    attributes.remove(".selection");
    if (!attributes.add(".selection",
                        domain,
                        bke::cpp_type_to_custom_data_type(type),
                        bke::AttributeInitMoveArray(dst))) {
      MEM_freeN(dst);
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
  RNA_def_property_flag(prop, (PropertyFlag)(PROP_HIDDEN | PROP_SKIP_SAVE));
}

static bool contains(const VArray<bool> &varray, const bool value)
{
  const CommonVArrayInfo info = varray.common_info();
  if (info.type == CommonVArrayInfo::Type::Single) {
    return *static_cast<const bool *>(info.data) == value;
  }
  if (info.type == CommonVArrayInfo::Type::Span) {
    const Span<bool> span(static_cast<const bool *>(info.data), varray.size());
    return threading::parallel_reduce(
        span.index_range(),
        4096,
        false,
        [&](const IndexRange range, const bool init) {
          return init || span.slice(range).contains(value);
        },
        [&](const bool a, const bool b) { return a || b; });
  }
  return threading::parallel_reduce(
      varray.index_range(),
      2048,
      false,
      [&](const IndexRange range, const bool init) {
        if (init) {
          return init;
        }
        /* Alternatively, this could use #materialize to retrieve many values at once. */
        for (const int64_t i : range) {
          if (varray[i] == value) {
            return true;
          }
        }
        return false;
      },
      [&](const bool a, const bool b) { return a || b; });
}

bool has_anything_selected(const Curves &curves_id)
{
  const CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
  const VArray<bool> selection = curves.attributes().lookup<bool>(".selection");
  return !selection || contains(selection, true);
}

static bool has_anything_selected(const Span<Curves *> curves_ids)
{
  return std::any_of(curves_ids.begin(), curves_ids.end(), [](const Curves *curves_id) {
    return has_anything_selected(*curves_id);
  });
}

namespace select_all {

static void invert_selection(MutableSpan<float> selection)
{
  threading::parallel_for(selection.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      selection[i] = 1.0f - selection[i];
    }
  });
}

static void invert_selection(GMutableSpan selection)
{
  if (selection.type().is<bool>()) {
    array_utils::invert_booleans(selection.typed<bool>());
  }
  else if (selection.type().is<float>()) {
    invert_selection(selection.typed<float>());
  }
}

static int select_all_exec(bContext *C, wmOperator *op)
{
  int action = RNA_enum_get(op->ptr, "action");

  VectorSet<Curves *> unique_curves = get_unique_editable_curves(*C);

  if (action == SEL_TOGGLE) {
    action = has_anything_selected(unique_curves) ? SEL_DESELECT : SEL_SELECT;
  }

  for (Curves *curves_id : unique_curves) {
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id->geometry);
    bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
    if (action == SEL_SELECT) {
      /* As an optimization, just remove the selection attributes when everything is selected. */
      attributes.remove(".selection");
    }
    else if (!attributes.contains(".selection")) {
      BLI_assert(ELEM(action, SEL_INVERT, SEL_DESELECT));
      /* If the attribute doesn't exist and it's either deleted or inverted, create
       * it with nothing selected, since that means everything was selected before. */
      attributes.add(".selection",
                     eAttrDomain(curves_id->selection_domain),
                     CD_PROP_BOOL,
                     bke::AttributeInitDefaultValue());
    }
    else {
      bke::GSpanAttributeWriter selection = attributes.lookup_for_write_span(".selection");
      if (action == SEL_DESELECT) {
        fill_selection_false(selection.span);
      }
      else if (action == SEL_INVERT) {
        invert_selection(selection.span);
      }
      selection.finish();
    }

    /* Use #ID_RECALC_GEOMETRY instead of #ID_RECALC_SELECT because it is handled as a generic
     * attribute for now. */
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GEOM | ND_DATA, curves_id);
  }

  return OPERATOR_FINISHED;
}

}  // namespace select_all

static void SCULPT_CURVES_OT_select_all(wmOperatorType *ot)
{
  ot->name = "(De)select All";
  ot->idname = __func__;
  ot->description = "(De)select all control points";

  ot->exec = select_all::select_all_exec;
  ot->poll = editable_curves_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
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

static int surface_set_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  Object &new_surface_ob = *CTX_data_active_object(C);

  Mesh &new_surface_mesh = *static_cast<Mesh *>(new_surface_ob.data);
  const char *new_uv_map_name = CustomData_get_active_layer_name(&new_surface_mesh.ldata,
                                                                 CD_PROP_FLOAT2);

  CTX_DATA_BEGIN (C, Object *, selected_ob, selected_objects) {
    if (selected_ob->type != OB_CURVES) {
      continue;
    }
    Object &curves_ob = *selected_ob;
    Curves &curves_id = *static_cast<Curves *>(curves_ob.data);

    MEM_SAFE_FREE(curves_id.surface_uv_map);
    if (new_uv_map_name != nullptr) {
      curves_id.surface_uv_map = BLI_strdup(new_uv_map_name);
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
    blender::ed::curves::ensure_surface_deformation_node_exists(*C, curves_ob);

    curves_id.surface = &new_surface_ob;
    ED_object_parent_set(
        op->reports, C, scene, &curves_ob, &new_surface_ob, PAR_OBJECT, false, true, nullptr);

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

}  // namespace blender::ed::curves

void ED_operatortypes_curves()
{
  using namespace blender::ed::curves;
  WM_operatortype_append(CURVES_OT_convert_to_particle_system);
  WM_operatortype_append(CURVES_OT_convert_from_particle_system);
  WM_operatortype_append(CURVES_OT_snap_curves_to_surface);
  WM_operatortype_append(CURVES_OT_set_selection_domain);
  WM_operatortype_append(SCULPT_CURVES_OT_select_all);
  WM_operatortype_append(CURVES_OT_surface_set);
}

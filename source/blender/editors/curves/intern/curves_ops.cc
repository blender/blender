/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edcurves
 */

#include <atomic>

#include "BLI_utildefines.h"

#include "ED_curves.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "WM_api.h"

#include "BKE_bvhutils.h"
#include "BKE_context.h"
#include "BKE_curves.hh"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"
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

#include "RNA_access.h"
#include "RNA_define.h"

/**
 * The code below uses a suffix naming convention to indicate the coordinate space:
 * `cu`: Local space of the curves object that is being edited.
 * `su`: Local space of the surface object.
 * `wo`: World space.
 * `ha`: Local space of an individual hair in the legacy hair system.
 */

namespace blender::ed::curves {

using bke::CurvesGeometry;

namespace convert_to_particle_system {

static int find_mface_for_root_position(const Mesh &mesh,
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
    const MFace &possible_mface = mesh.mface[possible_mface_i];
    {
      float3 point_in_triangle;
      closest_on_tri_to_point_v3(point_in_triangle,
                                 root_pos,
                                 mesh.mvert[possible_mface.v1].co,
                                 mesh.mvert[possible_mface.v2].co,
                                 mesh.mvert[possible_mface.v3].co);
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
                                 mesh.mvert[possible_mface.v1].co,
                                 mesh.mvert[possible_mface.v3].co,
                                 mesh.mvert[possible_mface.v4].co);
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
static float4 compute_mface_weights_for_position(const Mesh &mesh,
                                                 const MFace &mface,
                                                 const float3 &position)
{
  float4 mface_weights;
  if (mface.v4) {
    float mface_verts_su[4][3];
    copy_v3_v3(mface_verts_su[0], mesh.mvert[mface.v1].co);
    copy_v3_v3(mface_verts_su[1], mesh.mvert[mface.v2].co);
    copy_v3_v3(mface_verts_su[2], mesh.mvert[mface.v3].co);
    copy_v3_v3(mface_verts_su[3], mesh.mvert[mface.v4].co);
    interp_weights_poly_v3(mface_weights, mface_verts_su, 4, position);
  }
  else {
    interp_weights_tri_v3(mface_weights,
                          mesh.mvert[mface.v1].co,
                          mesh.mvert[mface.v2].co,
                          mesh.mvert[mface.v3].co,
                          position);
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

  const Span<float3> positions_cu = curves.positions();
  const VArray<int> looptri_indices = curves.surface_triangle_indices();
  const Span<MLoopTri> looptris{BKE_mesh_runtime_looptri_ensure(&surface_me),
                                BKE_mesh_runtime_looptri_len(&surface_me)};

  /* Find indices of curves that can be transferred to the old hair system. */
  Vector<int> curves_indices_to_transfer;
  for (const int curve_i : curves.curves_range()) {
    const int looptri_i = looptri_indices[curve_i];
    if (looptri_i >= 0 && looptri_i < looptris.size()) {
      curves_indices_to_transfer.append(curve_i);
    }
    else {
      *r_could_not_convert_some_curves = true;
    }
  }

  const int hairs_num = curves_indices_to_transfer.size();
  if (hairs_num == 0) {
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
      static_cast<ParticleData *>(MEM_calloc_arrayN(hairs_num, sizeof(ParticleData), __func__)),
      hairs_num};

  /* The old hair system still uses #MFace, so make sure those are available on the mesh. */
  BKE_mesh_tessface_calc(&surface_me);

  /* Prepare utility data structure to map hair roots to mfaces. */
  const Span<int> mface_to_poly_map{
      static_cast<int *>(CustomData_get_layer(&surface_me.fdata, CD_ORIGINDEX)),
      surface_me.totface};
  Array<Vector<int>> poly_to_mface_map(surface_me.totpoly);
  for (const int mface_i : mface_to_poly_map.index_range()) {
    const int poly_i = mface_to_poly_map[mface_i];
    poly_to_mface_map[poly_i].append(mface_i);
  }

  /* Prepare transformation matrices. */
  const float4x4 curves_to_world_mat = curves_ob.obmat;
  const float4x4 surface_to_world_mat = surface_ob.obmat;
  const float4x4 world_to_surface_mat = surface_to_world_mat.inverted();
  const float4x4 curves_to_surface_mat = world_to_surface_mat * curves_to_world_mat;

  for (const int new_hair_i : curves_indices_to_transfer.index_range()) {
    const int curve_i = curves_indices_to_transfer[new_hair_i];
    const IndexRange points = curves.points_for_curve(curve_i);

    const int looptri_i = looptri_indices[curve_i];
    const MLoopTri &looptri = looptris[looptri_i];
    const int poly_i = looptri.poly;

    const float3 &root_pos_cu = positions_cu[points.first()];
    const float3 root_pos_su = curves_to_surface_mat * root_pos_cu;

    const int mface_i = find_mface_for_root_position(
        surface_me, poly_to_mface_map[poly_i], root_pos_su);
    const MFace &mface = surface_me.mface[mface_i];

    const float4 mface_weights = compute_mface_weights_for_position(
        surface_me, mface, root_pos_su);

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
      const float3 key_pos_su = curves_to_surface_mat * key_pos_cu;
      const float3 key_pos_ha = surface_to_hair_mat * key_pos_su;

      HairKey &key = hair_keys[key_i];
      copy_v3_v3(key.co, key_pos_ha);
      key.time = 100.0f * key_i / (float)(hair_keys.size() - 1);
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

static bool curves_convert_to_particle_system_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_CURVES) {
    return false;
  }
  Curves &curves = *static_cast<Curves *>(ob->data);
  return curves.surface != nullptr;
}

}  // namespace convert_to_particle_system

static void CURVES_OT_convert_to_particle_system(wmOperatorType *ot)
{
  ot->name = "Convert Curves to Particle System";
  ot->idname = "CURVES_OT_convert_to_particle_system";
  ot->description = "Add a new or update an existing hair particle system on the surface object";

  ot->poll = convert_to_particle_system::curves_convert_to_particle_system_poll;
  ot->exec = convert_to_particle_system::curves_convert_to_particle_system_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;
}

namespace snap_curves_to_surface {

enum class AttachMode {
  Nearest,
  Deform,
};

static bool snap_curves_to_surface_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_CURVES) {
    return false;
  }
  if (!ED_operator_object_active_editable_ex(C, ob)) {
    return false;
  }
  Curves &curves = *static_cast<Curves *>(ob->data);
  if (curves.surface == nullptr) {
    return false;
  }
  return true;
}

static int snap_curves_to_surface_exec(bContext *C, wmOperator *op)
{
  const AttachMode attach_mode = static_cast<AttachMode>(RNA_enum_get(op->ptr, "attach_mode"));

  std::atomic<bool> found_invalid_looptri_index = false;

  CTX_DATA_BEGIN (C, Object *, curves_ob, selected_objects) {
    if (curves_ob->type != OB_CURVES) {
      continue;
    }
    Curves &curves_id = *static_cast<Curves *>(curves_ob->data);
    CurvesGeometry &curves = CurvesGeometry::wrap(curves_id.geometry);
    if (curves_id.surface == nullptr) {
      continue;
    }
    Object &surface_ob = *curves_id.surface;
    if (surface_ob.type != OB_MESH) {
      continue;
    }
    Mesh &surface_mesh = *static_cast<Mesh *>(surface_ob.data);

    MutableSpan<float3> positions_cu = curves.positions_for_write();
    MutableSpan<int> surface_triangle_indices = curves.surface_triangle_indices_for_write();
    MutableSpan<float2> surface_triangle_coords = curves.surface_triangle_coords_for_write();

    const Span<MLoopTri> surface_looptris = {BKE_mesh_runtime_looptri_ensure(&surface_mesh),
                                             BKE_mesh_runtime_looptri_len(&surface_mesh)};

    const float4x4 curves_to_world_mat = curves_ob->obmat;
    const float4x4 world_to_curves_mat = curves_to_world_mat.inverted();
    const float4x4 surface_to_world_mat = surface_ob.obmat;
    const float4x4 world_to_surface_mat = surface_to_world_mat.inverted();
    const float4x4 curves_to_surface_mat = world_to_surface_mat * curves_to_world_mat;
    const float4x4 surface_to_curves_mat = world_to_curves_mat * surface_to_world_mat;

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
            const float3 old_first_point_pos_su = curves_to_surface_mat * old_first_point_pos_cu;

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
            const float3 new_first_point_pos_cu = surface_to_curves_mat * new_first_point_pos_su;
            const float3 pos_diff_cu = new_first_point_pos_cu - old_first_point_pos_cu;

            for (float3 &pos_cu : positions_cu.slice(points)) {
              pos_cu += pos_diff_cu;
            }

            surface_triangle_indices[curve_i] = looptri_index;

            const MLoopTri &looptri = surface_looptris[looptri_index];
            const float3 &p0_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[0]].v].co;
            const float3 &p1_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[1]].v].co;
            const float3 &p2_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[2]].v].co;
            float3 bary_coords;
            interp_weights_tri_v3(bary_coords, p0_su, p1_su, p2_su, new_first_point_pos_su);
            surface_triangle_coords[curve_i] = bke::curves::encode_surface_bary_coord(bary_coords);
          }
        });
        break;
      }
      case AttachMode::Deform: {
        threading::parallel_for(curves.curves_range(), 256, [&](const IndexRange curves_range) {
          for (const int curve_i : curves_range) {
            const IndexRange points = curves.points_for_curve(curve_i);
            const int first_point_i = points.first();
            const float3 old_first_point_pos_cu = positions_cu[first_point_i];

            const int looptri_index = surface_triangle_indices[curve_i];
            if (!surface_looptris.index_range().contains(looptri_index)) {
              found_invalid_looptri_index = true;
              continue;
            }

            const MLoopTri &looptri = surface_looptris[looptri_index];

            const float3 bary_coords = bke::curves::decode_surface_bary_coord(
                surface_triangle_coords[curve_i]);

            const float3 &p0_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[0]].v].co;
            const float3 &p1_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[1]].v].co;
            const float3 &p2_su = surface_mesh.mvert[surface_mesh.mloop[looptri.tri[2]].v].co;

            float3 new_first_point_pos_su;
            interp_v3_v3v3v3(new_first_point_pos_su, p0_su, p1_su, p2_su, bary_coords);
            const float3 new_first_point_pos_cu = surface_to_curves_mat * new_first_point_pos_su;

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
  CTX_DATA_END;

  if (found_invalid_looptri_index) {
    BKE_report(op->reports, RPT_INFO, "Could not snap some curves to the surface");
  }

  WM_main_add_notifier(NC_OBJECT | ND_DRAW, nullptr);

  return OPERATOR_FINISHED;
}

}  // namespace snap_curves_to_surface

static void CURVES_OT_snap_curves_to_surface(wmOperatorType *ot)
{
  using namespace snap_curves_to_surface;

  ot->name = "Snap Curves to Surface";
  ot->idname = "CURVES_OT_snap_curves_to_surface";
  ot->description = "Move curves so that the first point is exactly on the surface mesh";

  ot->poll = snap_curves_to_surface_poll;
  ot->exec = snap_curves_to_surface_exec;

  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER;

  static const EnumPropertyItem attach_mode_items[] = {
      {static_cast<int>(AttachMode::Nearest),
       "NEAREST",
       0,
       "Nearest",
       "Find the closest point on the surface for the root point of every curve and move the root "
       "there"},
      {static_cast<int>(AttachMode::Deform),
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
               static_cast<int>(AttachMode::Nearest),
               "Attach Mode",
               "How to find the point on the surface to attach to");
}

}  // namespace blender::ed::curves

void ED_operatortypes_curves()
{
  using namespace blender::ed::curves;
  WM_operatortype_append(CURVES_OT_convert_to_particle_system);
  WM_operatortype_append(CURVES_OT_snap_curves_to_surface);
}

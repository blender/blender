/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 *
 * Deform coordinates by a armature object (used by modifier).
 */

#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_math_matrix.h"
#include "BLI_math_quaternion.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_task.h"

#include "DNA_armature_types.h"
#include "DNA_lattice_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_customdata.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_lattice.hh"
#include "BKE_mesh.hh"

#include "CLG_log.h"

static CLG_LogRef LOG = {"bke.armature_deform"};

/* -------------------------------------------------------------------- */
/** \name Armature Deform Internal Utilities
 * \{ */

static float bone_envelope_falloff(const float distance_squared,
                                   const float closest_radius,
                                   const float falloff_distance)
{
  using namespace blender;

  if (distance_squared < closest_radius * closest_radius) {
    return 1.0f;
  }

  /* Zero influence beyond falloff distance. */
  if (falloff_distance == 0.0f ||
      distance_squared >= math::square(closest_radius + falloff_distance))
  {
    return 0.0f;
  }

  /* Compute influence from envelope over the falloff distance. */
  const float dist_envelope = sqrtf(distance_squared) - closest_radius;
  return 1.0f - (dist_envelope * dist_envelope) / (falloff_distance * falloff_distance);
}

float distfactor_to_bone(const blender::float3 &position,
                         const blender::float3 &head,
                         const blender::float3 &tail,
                         const float radius_head,
                         const float radius_tail,
                         const float falloff_distance)
{
  using namespace blender;

  float bone_length;
  const float3 bone_axis = math::normalize_and_get_length(tail - head, bone_length);
  /* Distance along the bone axis from head. */
  const float height = math::dot(position - head, bone_axis);

  if (height < 0.0f) {
    /* Below the start of the bone use the head radius. */
    const float distance_squared = math::distance_squared(position, head);
    return bone_envelope_falloff(distance_squared, radius_head, falloff_distance);
  }
  else if (height > bone_length) {
    /* After the end of the bone use the tail radius. */
    const float distance_squared = math::distance_squared(tail, position);
    return bone_envelope_falloff(distance_squared, radius_tail, falloff_distance);
  }
  else {
    /* Interpolate radius. */
    const float distance_squared = math::distance_squared(position, head) - height * height;
    const float closest_radius = bone_length != 0.0f ? math::interpolate(radius_head,
                                                                         radius_tail,
                                                                         height / bone_length) :
                                                       radius_head;
    return bone_envelope_falloff(distance_squared, closest_radius, falloff_distance);
  }
}

namespace blender::bke {

/**
 * Utility class for accumulating linear bone deformation.
 * If full_deform is true the deformation matrix is also computed.
 */
template<bool full_deform> struct BoneDeformLinearMixer {

  float3 position_delta = float3(0.0f);
  float3x3 deform = float3x3::zero();

  void accumulate(const bPoseChannel &pchan, const float3 &co, const float weight)
  {
    const float4x4 &pose_mat = float4x4(pchan.chan_mat);

    position_delta += weight * (math::transform_point(pose_mat, co) - co);
    if constexpr (full_deform) {
      deform += weight * pose_mat.view<3, 3>();
    }
  }

  void accumulate_bbone(const bPoseChannel &pchan,
                        const float3 &co,
                        const float weight,
                        const int index)
  {
    const Span<float4x4> pose_mats = Span<Mat4>(pchan.runtime.bbone_deform_mats,
                                                pchan.runtime.bbone_segments + 2)
                                         .cast<float4x4>();
    const float4x4 &pose_mat = pose_mats[index + 1];

    position_delta += weight * (math::transform_point(pose_mat, co) - co);
    if constexpr (full_deform) {
      deform += weight * pose_mat.view<3, 3>();
    }
  }

  void finalize(const float3 & /*co*/,
                float total,
                float armature_weight,
                float3 &r_delta_co,
                float3x3 &r_deform_mat)
  {
    const float scale_factor = armature_weight / total;
    r_delta_co = position_delta * scale_factor;
    r_deform_mat = deform * scale_factor;
  };
};

/**
 * Utility class for accumulating dual quaternion bone deformation.
 * If full_deform is true the deformation matrix is also computed.
 */
template<bool full_deform> struct BoneDeformDualQuaternionMixer {
  DualQuat dq = {};

  void accumulate(const bPoseChannel &pchan, const float3 &co, const float weight)
  {
    const DualQuat &deform_quat = pchan.runtime.deform_dual_quat;

    add_weighted_dq_dq_pivot(&dq, &deform_quat, co, weight, full_deform);
  }

  void accumulate_bbone(const bPoseChannel &pchan,
                        const float3 &co,
                        const float weight,
                        const int index)
  {
    const Span<DualQuat> quats = {pchan.runtime.bbone_dual_quats,
                                  pchan.runtime.bbone_segments + 1};
    const DualQuat &deform_quat = quats[index];

    add_weighted_dq_dq_pivot(&dq, &deform_quat, co, weight, full_deform);
  }

  void finalize(const float3 &co,
                float total,
                float armature_weight,
                float3 &r_delta_co,
                float3x3 &r_deform_mat)
  {
    normalize_dq(&dq, total);
    float3 dco = co;
    float3x3 dmat;
    mul_v3m3_dq(dco, dmat.ptr(), &dq);
    r_delta_co = (dco - co) * armature_weight;
    /* Quaternion already is scale corrected. */
    r_deform_mat = dmat;
  }
};

/* Add interpolated deformation along a b-bone segment of the pose channel. */
template<typename MixerT>
static void b_bone_deform(const bPoseChannel &pchan,
                          const float3 &co,
                          const float weight,
                          MixerT &mixer)
{
  /* Calculate the indices of the 2 affecting b_bone segments. */
  int index;
  float blend;
  BKE_pchan_bbone_deform_segment_index(&pchan, co, &index, &blend);

  mixer.accumulate_bbone(pchan, co, weight * (1.0f - blend), index);
  mixer.accumulate_bbone(pchan, co, weight * blend, index + 1);
}

/* Add bone deformation based on envelope distance. */
template<typename MixerT>
static float dist_bone_deform(const bPoseChannel &pchan, const float3 &co, MixerT &mixer)
{
  const Bone *bone = pchan.bone;
  if (bone == nullptr || bone->weight == 0.0f) {
    return 0.0f;
  }

  const float fac = distfactor_to_bone(co,
                                       float3(bone->arm_head),
                                       float3(bone->arm_tail),
                                       bone->rad_head,
                                       bone->rad_tail,
                                       bone->dist);
  if (fac == 0.0f) {
    return 0.0f;
  }

  const float weight = fac * bone->weight;
  if (bone->segments > 1 && pchan.runtime.bbone_segments == bone->segments) {
    b_bone_deform(pchan, co, weight, mixer);
  }
  else {
    mixer.accumulate(pchan, co, weight);
  }

  return weight;
}

/* Add bone deformation based on vertex group weight. */
template<typename MixerT>
static float pchan_bone_deform(const bPoseChannel &pchan,
                               const float weight,
                               const float3 &co,
                               MixerT &mixer)
{
  const Bone *bone = pchan.bone;

  if (!weight) {
    return 0.0f;
  }

  if (bone->segments > 1 && pchan.runtime.bbone_segments == bone->segments) {
    b_bone_deform(pchan, co, weight, mixer);
  }
  else {
    mixer.accumulate(pchan, co, weight);
  }

  return weight;
}

}  // namespace blender::bke

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Deform #BKE_armature_deform_coords API
 *
 * #BKE_armature_deform_coords and related functions.
 * \{ */

namespace blender::bke {

struct ArmatureUserdata {
  MutableSpan<float3> vert_coords;
  std::optional<MutableSpan<float3x3>> vert_deform_mats;
  std::optional<Span<float3>> vert_coords_prev;

  bool use_envelope;
  bool use_quaternion;
  bool invert_vgroup;
  bool use_dverts;

  int armature_def_nr;

  /* List of all pose channels on the target object. */
  ConstListBaseWrapper<bPoseChannel> pose_channels = {{nullptr, nullptr}};
  /* Maps vertex group index (def_nr) to pose channels, if vertex groups are used.
   * Vertex groups used for deform can be different from the target object vertex groups list,
   * the def_nr needs to be mapped to the correct pose channel first. */
  Array<bPoseChannel *> pose_channel_by_vertex_group;

  float4x4 target_to_armature;
  float4x4 armature_to_target;

  const Mesh *me_target;
  std::optional<Span<MDeformVert>> dverts;
  struct {
    int cd_dvert_offset;
  } bmesh;
};

static ArmatureUserdata get_armature_deform_data(
    const Object &ob_arm,
    const Object &ob_target,
    const ListBase *defbase,
    MutableSpan<float3> vert_coords,
    std::optional<Span<float3>> vert_coords_prev,
    std::optional<MutableSpan<float3x3>> vert_deform_mats,
    const int deformflag,
    blender::StringRefNull defgrp_name,
    const bool use_dverts,
    const Mesh *me_target,
    const std::optional<Span<MDeformVert>> dverts,
    std::optional<int> cd_dvert_offset)
{
  const bool dverts_supported = BKE_object_supports_vertex_groups(&ob_target);

  ArmatureUserdata data{};
  data.vert_coords = vert_coords;
  data.vert_deform_mats = vert_deform_mats;
  data.vert_coords_prev = vert_coords_prev;
  data.use_envelope = bool(deformflag & ARM_DEF_ENVELOPE);
  data.use_quaternion = bool(deformflag & ARM_DEF_QUATERNION);
  data.invert_vgroup = bool(deformflag & ARM_DEF_INVERT_VGROUP);

  data.pose_channels = {ob_arm.pose->chanbase};
  data.use_dverts = dverts_supported && use_dverts && (deformflag & ARM_DEF_VGROUP);
  if (data.use_dverts) {
    const int defbase_len = BLI_listbase_count(defbase);
    data.pose_channel_by_vertex_group.reinitialize(defbase_len);
    /* TODO(sergey): Some considerations here:
     *
     * - Check whether keeping this consistent across frames gives speedup.
     */
    int i;
    LISTBASE_FOREACH_INDEX (bDeformGroup *, dg, defbase, i) {
      bPoseChannel *pchan = BKE_pose_channel_find_name(ob_arm.pose, dg->name);
      /* Exclude non-deforming bones. */
      data.pose_channel_by_vertex_group[i] = (pchan && !(pchan->bone->flag & BONE_NO_DEFORM)) ?
                                                 pchan :
                                                 nullptr;
    }
  }

  /* Index of singular vertex group, if used. */
  data.armature_def_nr = dverts_supported ? BKE_defgroup_name_index(defbase, defgrp_name) : -1;

  data.me_target = me_target;
  data.dverts = dverts;
  data.bmesh.cd_dvert_offset = cd_dvert_offset ? *cd_dvert_offset : -1;

/* TODO using the existing matrices directly is better, but fails tests because old code was
 * doing a double-inverse of the object matrix, leading to small differences on the order of 10^-5.
 * Test data needs to be updated if the transforms change. */
#if 0
  data.target_to_armature = ob_arm.world_to_object() * ob_target.object_to_world();
  data.armature_to_target = ob_target.world_to_object() * ob_arm.object_to_world();
#else
  data.armature_to_target = ob_target.world_to_object() * ob_arm.object_to_world();
  data.target_to_armature = math::invert(data.armature_to_target);
#endif

  return data;
}

/* Accumulate bone deformations using the mixer implementation. */
template<typename MixerT>
static void armature_vert_task_with_mixer(const ArmatureUserdata &data,
                                          const int i,
                                          const MDeformVert *dvert,
                                          MixerT &mixer)
{
  const bool full_deform = data.vert_deform_mats.has_value();

  /* Overall influence, can change by masking with a vertex group. */
  float armature_weight = 1.0f;
  float prevco_weight = 0.0f; /* weight for optional cached vertexcos */
  if (data.armature_def_nr != -1 && dvert) {
    const float mask_weight = BKE_defvert_find_weight(dvert, data.armature_def_nr);
    /* On multi-modifier the mask is used to blend with previous coordinates. */
    if (data.vert_coords_prev) {
      prevco_weight = data.invert_vgroup ? mask_weight : 1.0f - mask_weight;
      if (prevco_weight == 1.0f) {
        return;
      }
    }
    else {
      armature_weight = data.invert_vgroup ? 1.0f - mask_weight : mask_weight;
      if (armature_weight == 0.0f) {
        return;
      }
    }
  }

  /* Input coordinates to start from. */
  float3 co = data.vert_coords_prev ? (*data.vert_coords_prev)[i] : data.vert_coords[i];
  /* Transform to armature space. */
  co = math::transform_point(data.target_to_armature, co);

  float contrib = 0.0f;
  bool deformed = false;
  /* Apply vertex group deformation if enabled. */
  if (data.use_dverts && dvert) {
    /* Range of valid def_nr in MDeformWeight. */
    const IndexRange def_nr_range = data.pose_channel_by_vertex_group.index_range();
    const Span<MDeformWeight> dweights(dvert->dw, dvert->totweight);
    for (const auto &dw : dweights) {
      const bPoseChannel *pchan = def_nr_range.contains(dw.def_nr) ?
                                      data.pose_channel_by_vertex_group[dw.def_nr] :
                                      nullptr;
      if (pchan == nullptr) {
        continue;
      }

      float weight = dw.weight;

      /* Bone option to mix with envelope weight. */
      const Bone *bone = pchan->bone;
      if (bone && bone->flag & BONE_MULT_VG_ENV) {
        weight *= distfactor_to_bone(co,
                                     float3(bone->arm_head),
                                     float3(bone->arm_tail),
                                     bone->rad_head,
                                     bone->rad_tail,
                                     bone->dist);
      }

      contrib += pchan_bone_deform(*pchan, weight, co, mixer);
      deformed = true;
    }
  }
  /* Use envelope if enabled and no bone deformed the vertex yet. */
  if (!deformed && data.use_envelope) {
    for (const bPoseChannel *pchan : data.pose_channels) {
      if (!(pchan->bone->flag & BONE_NO_DEFORM)) {
        contrib += dist_bone_deform(*pchan, co, mixer);
      }
    }
  }

  /* TODO Actually should be EPSILON? Weight values and contrib can be like 10e-39 small. */
  constexpr float contrib_threshold = 0.0001f;
  if (contrib > contrib_threshold) {
    float3 delta_co;
    float3x3 local_deform_mat;
    mixer.finalize(co, contrib, armature_weight, delta_co, local_deform_mat);

    co += delta_co;
    if (full_deform) {
      float3x3 &deform_mat = (*data.vert_deform_mats)[i];
      const float3x3 armature_to_target = data.armature_to_target.view<3, 3>();
      const float3x3 target_to_armature = data.target_to_armature.view<3, 3>();
      deform_mat = armature_to_target * local_deform_mat * target_to_armature * deform_mat;
    }
  }

  /* Transform back to target object space. */
  co = math::transform_point(data.armature_to_target, co);

  /* Multi-modifier: Interpolate with previous modifier position using the vertex group mask. */
  if (data.vert_coords_prev) {
    copy_v3_v3(data.vert_coords[i], math::interpolate(co, data.vert_coords[i], prevco_weight));
  }
  else {
    copy_v3_v3(data.vert_coords[i], co);
  }
}

/* Accumulate bone deformations for a vertex. */
static void armature_vert_task_with_dvert(const ArmatureUserdata &data,
                                          const int i,
                                          const MDeformVert *dvert)
{
  const bool full_deform = data.vert_deform_mats.has_value();
  if (data.use_quaternion) {
    if (full_deform) {
      bke::BoneDeformDualQuaternionMixer<true> mixer;
      armature_vert_task_with_mixer(data, i, dvert, mixer);
    }
    else {
      bke::BoneDeformDualQuaternionMixer<false> mixer;
      armature_vert_task_with_mixer(data, i, dvert, mixer);
    }
  }
  else {
    if (full_deform) {
      bke::BoneDeformLinearMixer<true> mixer;
      armature_vert_task_with_mixer(data, i, dvert, mixer);
    }
    else {
      bke::BoneDeformLinearMixer<false> mixer;
      armature_vert_task_with_mixer(data, i, dvert, mixer);
    }
  }
}

static void armature_vert_task(void *__restrict userdata,
                               const int i,
                               const TaskParallelTLS *__restrict /*tls*/)
{
  const ArmatureUserdata *data = static_cast<const ArmatureUserdata *>(userdata);
  const MDeformVert *dvert;
  if (data->use_dverts || data->armature_def_nr != -1) {
    if (data->me_target) {
      BLI_assert(i < data->me_target->verts_num);
      if (data->dverts.has_value()) {
        dvert = &(*data->dverts)[i];
      }
      else {
        dvert = nullptr;
      }
    }
    else if (data->dverts && i < data->dverts->size()) {
      dvert = &(*data->dverts)[i];
    }
    else {
      dvert = nullptr;
    }
  }
  else {
    dvert = nullptr;
  }

  armature_vert_task_with_dvert(*data, i, dvert);
}

static void armature_vert_task_editmesh(void *__restrict userdata,
                                        MempoolIterData *iter,
                                        const TaskParallelTLS *__restrict /*tls*/)
{
  const ArmatureUserdata *data = static_cast<const ArmatureUserdata *>(userdata);
  BMVert *v = (BMVert *)iter;
  const MDeformVert *dvert = static_cast<const MDeformVert *>(
      BM_ELEM_CD_GET_VOID_P(v, data->bmesh.cd_dvert_offset));
  armature_vert_task_with_dvert(*data, BM_elem_index_get(v), dvert);
}

static void armature_vert_task_editmesh_no_dvert(void *__restrict userdata,
                                                 MempoolIterData *iter,
                                                 const TaskParallelTLS *__restrict /*tls*/)
{
  const ArmatureUserdata *data = static_cast<const ArmatureUserdata *>(userdata);
  BMVert *v = (BMVert *)iter;
  armature_vert_task_with_dvert(*data, BM_elem_index_get(v), nullptr);
}

static void armature_deform_coords_impl(
    const Object *ob_arm,
    const Object *ob_target,
    const ListBase *defbase,
    const MutableSpan<float3> vert_coords,
    const std::optional<MutableSpan<float3x3>> vert_deform_mats,
    const int vert_coords_len,
    const int deformflag,
    const std::optional<Span<float3>> vert_coords_prev,
    const char *defgrp_name,
    const Span<MDeformVert> dverts,
    const Mesh *me_target,
    const BMEditMesh *em_target)
{
  const bArmature *arm = static_cast<const bArmature *>(ob_arm->data);
  bool use_dverts = false;
  int cd_dvert_offset = -1;

  /* in editmode, or not an armature */
  if (arm->edbo || (ob_arm->pose == nullptr)) {
    return;
  }

  if ((ob_arm->pose->flag & POSE_RECALC) != 0) {
    CLOG_ERROR(&LOG,
               "Trying to evaluate influence of armature '%s' which needs Pose recalc!",
               ob_arm->id.name);
    BLI_assert(0);
  }

  if (BKE_object_supports_vertex_groups(ob_target)) {
    /* Get a vertex-deform-index to pose-channel array. */
    if (deformflag & ARM_DEF_VGROUP) {
      /* if we have a Mesh, only use dverts if it has them */
      if (em_target) {
        cd_dvert_offset = CustomData_get_offset(&em_target->bm->vdata, CD_MDEFORMVERT);
        use_dverts = (cd_dvert_offset != -1);
      }
      else if (me_target) {
        use_dverts = !me_target->deform_verts().is_empty();
      }
      else if (dverts.size() == vert_coords_len) {
        use_dverts = true;
      }
    }
  }

  ArmatureUserdata data = get_armature_deform_data(*ob_arm,
                                                   *ob_target,
                                                   defbase,
                                                   vert_coords,
                                                   vert_coords_prev,
                                                   vert_deform_mats,
                                                   deformflag,
                                                   defgrp_name,
                                                   use_dverts,
                                                   me_target,
                                                   dverts,
                                                   cd_dvert_offset);

  if (em_target != nullptr) {
    /* While this could cause an extra loop over mesh data, in most cases this will
     * have already been properly set. */
    BM_mesh_elem_index_ensure(em_target->bm, BM_VERT);

    TaskParallelSettings settings;
    BLI_parallel_mempool_settings_defaults(&settings);

    if (use_dverts) {
      BLI_task_parallel_mempool(
          em_target->bm->vpool, &data, armature_vert_task_editmesh, &settings);
    }
    else {
      BLI_task_parallel_mempool(
          em_target->bm->vpool, &data, armature_vert_task_editmesh_no_dvert, &settings);
    }
  }
  else {
    TaskParallelSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.min_iter_per_thread = 32;
    BLI_task_parallel_range(0, vert_coords_len, &data, armature_vert_task, &settings);
  }
}

}  // namespace blender::bke

void BKE_armature_deform_coords_with_curves(
    const Object &ob_arm,
    const Object &ob_target,
    const ListBase *defbase,
    blender::MutableSpan<blender::float3> vert_coords,
    std::optional<blender::Span<blender::float3>> vert_coords_prev,
    std::optional<blender::MutableSpan<blender::float3x3>> vert_deform_mats,
    blender::Span<MDeformVert> dverts,
    int deformflag,
    blender::StringRefNull defgrp_name)
{
  using namespace blender;

  /* Vertex groups must be provided explicitly, cannot rely on object vertex groups since this is
   * used for Grease Pencil layers as well. */
  BLI_assert(dverts.size() == vert_coords.size());

  bke::armature_deform_coords_impl(&ob_arm,
                                   &ob_target,
                                   defbase,
                                   vert_coords,
                                   vert_deform_mats,
                                   vert_coords.size(),
                                   deformflag,
                                   vert_coords_prev,
                                   defgrp_name.c_str(),
                                   dverts,
                                   nullptr,
                                   nullptr);
}

void BKE_armature_deform_coords_with_mesh(const Object *ob_arm,
                                          const Object *ob_target,
                                          float (*vert_coords)[3],
                                          float (*vert_deform_mats)[3][3],
                                          int vert_coords_len,
                                          int deformflag,
                                          float (*vert_coords_prev)[3],
                                          const char *defgrp_name,
                                          const Mesh *me_target)
{
  using namespace blender;

  /* Note armature modifier on legacy curves calls this, so vertex groups are not guaranteed to
   * exist. */
  const ID *id_target = static_cast<const ID *>(ob_target->data);
  const ListBase *defbase = nullptr;
  if (me_target) {
    /* Use the vertex groups from the evaluated mesh that is being deformed. */
    defbase = BKE_id_defgroup_list_get(&me_target->id);
  }
  else if (BKE_id_supports_vertex_groups(id_target)) {
    /* Take the vertex groups from the original object data. */
    defbase = BKE_id_defgroup_list_get(id_target);
  }

  blender::Span<MDeformVert> dverts;
  if (ob_target->type == OB_MESH) {
    if (me_target == nullptr) {
      me_target = static_cast<const Mesh *>(ob_target->data);
    }
    dverts = me_target->deform_verts();
  }
  else if (ob_target->type == OB_LATTICE) {
    const Lattice *lt = static_cast<const Lattice *>(ob_target->data);
    dverts = blender::Span<MDeformVert>(lt->dvert, lt->pntsu * lt->pntsv * lt->pntsw);
  }

  MutableSpan<float3> vert_coords_span(reinterpret_cast<float3 *>(vert_coords), vert_coords_len);
  std::optional<MutableSpan<float3x3>> vert_deform_mats_span;
  if (vert_deform_mats) {
    vert_deform_mats_span = {reinterpret_cast<float3x3 *>(vert_deform_mats), vert_coords_len};
  }
  std::optional<Span<float3>> vert_coords_prev_span;
  if (vert_coords_prev) {
    vert_coords_prev_span = {reinterpret_cast<const float3 *>(vert_coords_prev), vert_coords_len};
  }
  bke::armature_deform_coords_impl(ob_arm,
                                   ob_target,
                                   defbase,
                                   vert_coords_span,
                                   vert_deform_mats_span,
                                   vert_coords_len,
                                   deformflag,
                                   vert_coords_prev_span,
                                   defgrp_name,
                                   dverts,
                                   me_target,
                                   nullptr);
}

void BKE_armature_deform_coords_with_editmesh(const Object *ob_arm,
                                              const Object *ob_target,
                                              float (*vert_coords)[3],
                                              float (*vert_deform_mats)[3][3],
                                              int vert_coords_len,
                                              int deformflag,
                                              float (*vert_coords_prev)[3],
                                              const char *defgrp_name,
                                              const BMEditMesh *em_target)
{
  using namespace blender;

  const ListBase *defbase = BKE_id_defgroup_list_get(static_cast<const ID *>(ob_target->data));

  MutableSpan<float3> vert_coords_span(reinterpret_cast<float3 *>(vert_coords), vert_coords_len);
  std::optional<MutableSpan<float3x3>> vert_deform_mats_span;
  if (vert_deform_mats) {
    vert_deform_mats_span = {reinterpret_cast<float3x3 *>(vert_deform_mats), vert_coords_len};
  }
  std::optional<Span<float3>> vert_coords_prev_span;
  if (vert_coords_prev) {
    vert_coords_prev_span = {reinterpret_cast<const float3 *>(vert_coords_prev), vert_coords_len};
  }
  bke::armature_deform_coords_impl(ob_arm,
                                   ob_target,
                                   defbase,
                                   vert_coords_span,
                                   vert_deform_mats_span,
                                   vert_coords_len,
                                   deformflag,
                                   vert_coords_prev_span,
                                   defgrp_name,
                                   {},
                                   nullptr,
                                   em_target);
}

/** \} */

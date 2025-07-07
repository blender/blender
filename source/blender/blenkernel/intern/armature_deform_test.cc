/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_action.hh"
#include "BKE_armature.hh"
#include "BKE_curves.hh"
#include "BKE_deform.hh"
#include "BKE_editmesh.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_object_deform.h"

#include "CLG_log.h"

#include "DNA_armature_types.h"
#include "DNA_curves_types.h"
#include "DNA_grease_pencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "testing/testing.h"

/* TODO Parameterized tests are not registered in `blender_test`.
 * Possible reason: the cmake gtest_add_tests function may not be able to detect them reliably.
 * https://cmake.org/cmake/help/latest/module/GoogleTest.html
 * https://www.kitware.com/dynamic-google-test-discovery-in-cmake-3-10/
 */
// #define USE_PARAMETERIZED_TESTS

namespace blender::bke::tests {

/* Type of data that is being deformed.
 * This distinction is needed to handle some inconsistent behaviors. */
enum class TargetDataType {
  Mesh,
  EditMesh,
  Curves,
  GreasePencil,
};

enum class InterpolationTest {
  /* Linear interpolation. */
  Linear,
  /* Dual-quaternion method, aka. "Preserve Volume" (ARM_DEF_QUATERNION). */
  DualQuaternion,
};

enum class OutputValueTest {
  /* Only position vector. */
  Position,
  /* Vector and deformation matrix ("full deform"). */
  PositionAndDeformMatrix,
};

enum class WeightingTest {
  /* Disabled (no deform). */
  None,
  /* Falloff from closest envelope point. */
  Envelope,
  /* Vertex group weight. */
  VertexGroups,
  /* Use both envelopes and vertex groups. */
  EnvelopeAndVertexGroups,
};

enum class MaskingTest {
  /* Deform all vertices. */
  All,
  /* Limit deformation to one vertex group. */
  VertexGroup,
};

/* Defines the source of vertex groups and weights for mesh deformation. */
enum class VertexWeightSource {
  /* Read vertex groups and weights from the target object's mesh data. */
  TargetObject,
  /* Use a separate mesh for defining vertex weights. */
  SeparateMesh,
};

class ArmatureDeformTestBase {
 public:
  Main *bmain;

  /* Translation for bones.
   * Rotation is omitted here for simplicity, the goal is to test all the code paths rather than
   * the details of the bone transformation. Other tests are more suitable for comparing
   * deformation results. */
  static float3 offset_bone1()
  {
    return float3(5, 0, 1);
  }
  static float3 offset_bone2()
  {
    return float3(0, -2, 0);
  }

  /* This happens usually in BKE_pose_bone_done. Update here to avoid creating a full depsgraph. */
  static void update_pose_matrices(bPoseChannel &pchan)
  {
    BKE_pchan_calc_mat(&pchan);
    if (!(pchan.bone->flag & BONE_NO_DEFORM)) {
      mat4_to_dquat(&pchan.runtime.deform_dual_quat, pchan.bone->arm_mat, pchan.chan_mat);
    }
  }

  Object *create_test_armature_object() const
  {
    Object *ob = BKE_object_add_only_object(bmain, OB_ARMATURE, "Test Armature Object");
    bArmature *arm = BKE_id_new<bArmature>(bmain, "Test Armature");
    ob->data = arm;

    Bone *bone1 = MEM_callocN<Bone>("Bone1");
    STRNCPY(bone1->name, "Bone1");
    copy_v3_v3(bone1->tail, float3(0, 0, 0));
    copy_v3_v3(bone1->head, float3(0, 0, 1));
    BLI_addtail(&arm->bonebase, bone1);
    BKE_armature_where_is_bone(bone1, nullptr, false);
    bone1->weight = 1.0f;
    /* Bone envelope large enough to include all vertices.
     * Falloff math isn't tested here, just have to make sure vertices are included. */
    bone1->rad_head = 2.0f;
    bone1->rad_tail = 2.0f;

    Bone *bone2 = MEM_callocN<Bone>("Bone2");
    STRNCPY(bone2->name, "Bone2");
    copy_v3_v3(bone2->tail, float3(0, 0, 0));
    copy_v3_v3(bone2->head, float3(0, 0, 1));
    BLI_addtail(&arm->bonebase, bone2);
    BKE_armature_where_is_bone(bone2, nullptr, false);
    bone2->weight = 1.0f;
    bone2->rad_head = 2.0f;
    bone2->rad_tail = 2.0f;

    BKE_pose_ensure(bmain, ob, arm, false);

    bPoseChannel *pchan1 = BKE_pose_channel_find_name(ob->pose, "Bone1");
    bPoseChannel *pchan2 = BKE_pose_channel_find_name(ob->pose, "Bone2");
    copy_v3_v3(pchan1->loc, offset_bone1());
    copy_v3_v3(pchan2->loc, offset_bone2());
    update_pose_matrices(*pchan1);
    update_pose_matrices(*pchan2);

    return ob;
  }

  static Span<float3> vertex_positions()
  {
    static Array<float3> data = {float3(-1, -1, -1),
                                 float3(1, -1, -1),
                                 float3(-1, 1, -1),
                                 float3(1, 1, -1),
                                 float3(-1, -1, 1),
                                 float3(1, -1, 1),
                                 float3(-1, 1, 1),
                                 float3(1, 1, 1)};
    return data;
  }

  static Array<float3x3> identity_deform_mats()
  {
    return Array<float3x3>(8, float3x3::identity());
  }

  static Span<float> vertex_weights_bone1()
  {
    static Array<float> data = {1, 1, 1, 1, 1, 1, 1, 1};
    return data;
  }

  static Span<float> vertex_weights_bone2()
  {
    static Array<float> data = {0, 0, 0, 0, 1, 1, 1, 1};
    return data;
  }

  static Span<int> curve_offsets()
  {
    static Array<int> data = {0, 2, 5, 8};
    return data;
  }

  static Mesh *create_test_mesh()
  {
    Mesh *mesh = BKE_mesh_new_nomain(vertex_positions().size(), 0, 0, 0);
    mesh->vert_positions_for_write().copy_from(vertex_positions());
    MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
    for (const int i : dverts.index_range()) {
      const float weight0 = vertex_weights_bone1()[i];
      const float weight1 = vertex_weights_bone2()[i];

      if (weight0 > 0.0f) {
        BKE_defvert_add_index_notest(&dverts[i], 0, weight0);
      }
      if (weight1 > 0.0f) {
        BKE_defvert_add_index_notest(&dverts[i], 1, weight1);
      }
    }
    mesh->tag_positions_changed();

    bDeformGroup *defgroup1 = MEM_callocN<bDeformGroup>(__func__);
    bDeformGroup *defgroup2 = MEM_callocN<bDeformGroup>(__func__);
    STRNCPY(defgroup1->name, "Bone1");
    STRNCPY(defgroup2->name, "Bone2");
    BLI_addtail(&mesh->vertex_group_names, defgroup1);
    BLI_addtail(&mesh->vertex_group_names, defgroup2);

    return mesh;
  }

  /* Creates a cube with all vertices in "Bone1" group and the top face in "Bone2" group. */
  Object *create_test_mesh_object() const
  {
    Object *ob = BKE_object_add_only_object(bmain, OB_MESH, "Test Mesh Object");
    Mesh *mesh_in_main = BKE_mesh_add(bmain, "Test Mesh");
    ob->data = mesh_in_main;

    Mesh *mesh = create_test_mesh();
    BKE_mesh_nomain_to_mesh(mesh, mesh_in_main, ob);
    BLI_assert(!mesh_in_main->deform_verts().is_empty());

    return ob;
  }

  /* Creates curves with a mix of vertices in "Bone1" and "Bone2" groups.
   * Curves datablock does not support vertex groups at this point, these are ignored. */
  Object *create_test_curves_object() const
  {
    Object *ob = BKE_object_add_only_object(bmain, OB_CURVES, "Test Curves Object");
    Curves *curves_id = BKE_curves_add(bmain, "Test Curves");
    ob->data = curves_id;
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();

    curves.resize(vertex_positions().size(), 3);
    curves.offsets_for_write().copy_from(curve_offsets());

    curves.positions_for_write().copy_from(vertex_positions());
    MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();
    for (const int i : dverts.index_range()) {
      const float weight0 = vertex_weights_bone1()[i];
      const float weight1 = vertex_weights_bone2()[i];

      if (weight0 > 0.0f) {
        BKE_defvert_add_index_notest(&dverts[i], 0, weight0);
      }
      if (weight1 > 0.0f) {
        BKE_defvert_add_index_notest(&dverts[i], 1, weight1);
      }
    }
    curves.tag_topology_changed();
    curves.tag_positions_changed();

    return ob;
  }

  /* Creates grease pencil with a mix of vertices in "Bone1" and "Bone2" groups. */
  Object *create_test_grease_pencil_object() const
  {
    Object *ob = BKE_object_add_only_object(bmain, OB_GREASE_PENCIL, "Test Grease Pencil Object");
    GreasePencil *grease_pencil = BKE_grease_pencil_add(bmain, "Test Grease Pencil");
    ob->data = grease_pencil;

    bke::greasepencil::Layer &layer = grease_pencil->add_layer("Test");
    greasepencil::Drawing &drawing = grease_pencil->insert_frame(layer, 1)->wrap();
    bke::CurvesGeometry &curves = drawing.geometry.wrap();

    curves.resize(vertex_positions().size(), 3);
    curves.offsets_for_write().copy_from(curve_offsets());

    curves.positions_for_write().copy_from(vertex_positions());
    MutableSpan<MDeformVert> dverts = curves.deform_verts_for_write();
    for (const int i : dverts.index_range()) {
      const float weight0 = vertex_weights_bone1()[i];
      const float weight1 = vertex_weights_bone2()[i];

      if (weight0 > 0.0f) {
        BKE_defvert_add_index_notest(&dverts[i], 0, weight0);
      }
      if (weight1 > 0.0f) {
        BKE_defvert_add_index_notest(&dverts[i], 1, weight1);
      }
    }
    curves.tag_topology_changed();
    curves.tag_positions_changed();

    BKE_object_defgroup_new(ob, "Bone1");
    BKE_object_defgroup_new(ob, "Bone2");

    return ob;
  }

  /* Select the expected positions result based on data type and parameters. */
  static Span<float3> expected_positions(const TargetDataType target_type,
                                         const WeightingTest weighting,
                                         const MaskingTest masking)
  {
    /* Both bones weighted equally. */
    static Array<float3> data_envelope = {float3(1.5f, -2, -0.5f),
                                          float3(3.5f, -2, -0.5f),
                                          float3(1.5f, 0, -0.5f),
                                          float3(3.5f, 0, -0.5f),
                                          float3(1.5f, -2, 1.5f),
                                          float3(3.5f, -2, 1.5f),
                                          float3(1.5f, 0, 1.5f),
                                          float3(3.5f, 0, 1.5f)};
    static Array<float3> data_envelope_masked = {float3(-1, -1, -1),
                                                 float3(1, -1, -1),
                                                 float3(-1, 1, -1),
                                                 float3(1, 1, -1),
                                                 float3(1.5f, -2, 1.5f),
                                                 float3(3.5f, -2, 1.5f),
                                                 float3(1.5f, 0, 1.5f),
                                                 float3(3.5f, 0, 1.5f)};
    /* Bottom verts deformed only by Bone1, top group deformed equally by both bones. */
    static Array<float3> data_vgroups = {float3(4, -1, 0),
                                         float3(6, -1, 0),
                                         float3(4, 1, 0),
                                         float3(6, 1, 0),
                                         float3(1.5f, -2, 1.5f),
                                         float3(3.5f, -2, 1.5f),
                                         float3(1.5f, 0, 1.5f),
                                         float3(3.5f, 0, 1.5f)};
    static Array<float3> data_vgroups_masked = {float3(-1, -1, -1),
                                                float3(1, -1, -1),
                                                float3(-1, 1, -1),
                                                float3(1, 1, -1),
                                                float3(1.5f, -2, 1.5f),
                                                float3(3.5f, -2, 1.5f),
                                                float3(1.5f, 0, 1.5f),
                                                float3(3.5f, 0, 1.5f)};

    /* Curves data type does not support vertext groups. */
    const bool vgroups_supported = ELEM(
        target_type, TargetDataType::Mesh, TargetDataType::EditMesh, TargetDataType::GreasePencil);
    /* Might be a bug: Mesh and GreasePencil allows masking even in envelope mode, but EditMesh
     * does not! Curves does not have vgroups and therefore masking in the first place. */
    const bool allow_masking_with_envelope = ELEM(
        target_type, TargetDataType::Mesh, TargetDataType::GreasePencil);
    const bool vertex_groups_enabled = ELEM(
        weighting, WeightingTest::VertexGroups, WeightingTest::EnvelopeAndVertexGroups);
    const bool masked = (masking == MaskingTest::VertexGroup) &&
                        ((vgroups_supported && vertex_groups_enabled) ||
                         allow_masking_with_envelope);

    switch (weighting) {
      case WeightingTest::None:
        return vertex_positions();
      case WeightingTest::Envelope:
        return masked ? data_envelope_masked : data_envelope;
      case WeightingTest::VertexGroups:
        if (vgroups_supported) {
          return masked ? data_vgroups_masked : data_vgroups;
        }
        else {
          return vertex_positions();
        }
      case WeightingTest::EnvelopeAndVertexGroups:
        if (vgroups_supported) {
          return masked ? data_vgroups_masked : data_vgroups;
        }
        else {
          return masked ? data_envelope_masked : data_envelope;
        }
    }
    BLI_assert_unreachable();
    return {};
  }

  static Span<float3x3> expected_deform_mats(const WeightingTest weighting)
  {
    static Array<float3x3> data_zero = identity_deform_mats();
    static Array<float3x3> data = {float3x3::identity(),
                                   float3x3::identity(),
                                   float3x3::identity(),
                                   float3x3::identity(),
                                   float3x3::identity(),
                                   float3x3::identity(),
                                   float3x3::identity(),
                                   float3x3::identity()};

    switch (weighting) {
      case WeightingTest::None:
        return data_zero;
      case WeightingTest::Envelope:
      case WeightingTest::VertexGroups:
      case WeightingTest::EnvelopeAndVertexGroups:
        return data;
    }
    return {};
  }

  static int get_deform_flag(const InterpolationTest interpolation, const WeightingTest weighting)
  {
    int deform_flag = 0;

    switch (interpolation) {
      case InterpolationTest::Linear:
        /* Nothing to change, default mode. */
        break;
      case InterpolationTest::DualQuaternion:
        deform_flag |= ARM_DEF_QUATERNION;
        break;
    }

    switch (weighting) {
      case WeightingTest::None:
        /* Nothing to do. */
        break;
      case WeightingTest::Envelope:
        deform_flag |= ARM_DEF_ENVELOPE;
        break;
      case WeightingTest::VertexGroups:
        deform_flag |= ARM_DEF_VGROUP;
        break;
      case WeightingTest::EnvelopeAndVertexGroups:
        deform_flag |= ARM_DEF_ENVELOPE | ARM_DEF_VGROUP;
        break;
    }

    return deform_flag;
  }

  static const char *get_defgrp_name(const MaskingTest masking)
  {
    switch (masking) {
      case MaskingTest::All:
        return "";
      case MaskingTest::VertexGroup:
        return "Bone2";
    }
    BLI_assert_unreachable();
    return "";
  }

  void mesh_test(const InterpolationTest interpolation,
                 const OutputValueTest output,
                 const WeightingTest weighting,
                 const MaskingTest masking,
                 const VertexWeightSource dvert_source)
  {
    Object *ob_arm = this->create_test_armature_object();
    Object *ob_target = this->create_test_mesh_object();
    Mesh *mesh = static_cast<Mesh *>(ob_target->data);
    /* Mesh deform function supports a separate Mesh data block for deform_groups and dverts. */
    Mesh *mesh_target = (dvert_source == VertexWeightSource::SeparateMesh) ? create_test_mesh() :
                                                                             nullptr;

    MutableSpan<float3> vert_positions = mesh->vert_positions_for_write();

    Array<float3x3> deform_mats;
    std::optional<MutableSpan<float3x3>> deform_mats_opt;
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      deform_mats = identity_deform_mats();
      deform_mats_opt = deform_mats;
    }

    const int deform_flag = get_deform_flag(interpolation, weighting);
    const char *defgrp_name = get_defgrp_name(masking);
    BKE_armature_deform_coords_with_mesh(*ob_arm,
                                         *ob_target,
                                         vert_positions,
                                         std::nullopt,
                                         deform_mats_opt,
                                         deform_flag,
                                         defgrp_name,
                                         mesh_target);

    EXPECT_EQ_SPAN(expected_positions(TargetDataType::Mesh, weighting, masking),
                   vert_positions.as_span());
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      EXPECT_EQ_SPAN(expected_deform_mats(weighting), deform_mats.as_span());
    }

    if (mesh_target) {
      /* Not in bmain. */
      BKE_id_free(nullptr, mesh_target);
    }
    BKE_id_delete(bmain, ob_arm);
    BKE_id_delete(bmain, ob_target);
  }

  void edit_mesh_test(const InterpolationTest interpolation,
                      const OutputValueTest output,
                      const WeightingTest weighting,
                      const MaskingTest masking)
  {
    Object *ob_arm = this->create_test_armature_object();
    Object *ob_target = this->create_test_mesh_object();
    Mesh *mesh = static_cast<Mesh *>(ob_target->data);

    BMeshCreateParams create_params{};
    create_params.use_toolflags = true;
    BMesh *bm = BKE_mesh_to_bmesh(mesh, 0, false, &create_params);
    BMEditMesh *edit_mesh = BKE_editmesh_create(bm);
    Array<float3> bm_verts_wrapper = BM_mesh_vert_coords_alloc(edit_mesh->bm);

    Array<float3x3> deform_mats;
    std::optional<MutableSpan<float3x3>> deform_mats_opt;
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      deform_mats = identity_deform_mats();
      deform_mats_opt = deform_mats;
    }

    const int deform_flag = get_deform_flag(interpolation, weighting);
    const char *defgrp_name = get_defgrp_name(masking);
    BKE_armature_deform_coords_with_editmesh(*ob_arm,
                                             *ob_target,
                                             bm_verts_wrapper,
                                             std::nullopt,
                                             deform_mats_opt,
                                             deform_flag,
                                             defgrp_name,
                                             *edit_mesh);

    EXPECT_EQ_SPAN(expected_positions(TargetDataType::EditMesh, weighting, masking),
                   bm_verts_wrapper.as_span());
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      EXPECT_EQ_SPAN(expected_deform_mats(weighting), deform_mats.as_span());
    }

    BKE_editmesh_free_data(edit_mesh);
    MEM_delete(edit_mesh);
    BKE_id_delete(bmain, ob_arm);
    BKE_id_delete(bmain, ob_target);
  }

  void curves_test(const InterpolationTest interpolation,
                   const OutputValueTest output,
                   const WeightingTest weighting,
                   const MaskingTest masking)
  {
    Object *ob_arm = this->create_test_armature_object();
    Object *ob_target = this->create_test_curves_object();
    Curves *curves_id = static_cast<Curves *>(ob_target->data);
    bke::CurvesGeometry &curves = curves_id->geometry.wrap();

    Array<float3x3> deform_mats;
    std::optional<MutableSpan<float3x3>> deform_mats_opt;
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      deform_mats = identity_deform_mats();
      deform_mats_opt = deform_mats;
    }

    const int deform_flag = get_deform_flag(interpolation, weighting);
    const char *defgrp_name = get_defgrp_name(masking);
    BKE_armature_deform_coords_with_curves(*ob_arm,
                                           *ob_target,
                                           nullptr,
                                           curves.positions_for_write(),
                                           std::nullopt,
                                           deform_mats_opt,
                                           curves.deform_verts(),
                                           deform_flag,
                                           defgrp_name);

    /* Note: Curves objects don't support vertex groups. */
    EXPECT_EQ_SPAN(expected_positions(TargetDataType::Curves, weighting, masking),
                   curves.positions());
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      EXPECT_EQ_SPAN(expected_deform_mats(weighting), deform_mats.as_span());
    }

    BKE_id_delete(bmain, ob_arm);
    BKE_id_delete(bmain, ob_target);
  }

  void grease_pencil_test(const InterpolationTest interpolation,
                          const OutputValueTest output,
                          const WeightingTest weighting,
                          const MaskingTest masking)
  {
    Object *ob_arm = this->create_test_armature_object();
    Object *ob_target = this->create_test_grease_pencil_object();
    GreasePencil *grease_pencil = static_cast<GreasePencil *>(ob_target->data);

    BLI_assert(!grease_pencil->drawings().is_empty());
    GreasePencilDrawingBase *drawing_base = grease_pencil->drawings()[0];
    BLI_assert(drawing_base->type == GP_DRAWING);
    greasepencil::Drawing &drawing = reinterpret_cast<GreasePencilDrawing *>(drawing_base)->wrap();
    bke::CurvesGeometry &curves = drawing.geometry.wrap();

    Array<float3x3> deform_mats;
    std::optional<MutableSpan<float3x3>> deform_mats_opt;
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      deform_mats = identity_deform_mats();
      deform_mats_opt = deform_mats;
    }

    const int deform_flag = get_deform_flag(interpolation, weighting);
    const char *defgrp_name = get_defgrp_name(masking);
    BKE_armature_deform_coords_with_curves(*ob_arm,
                                           *ob_target,
                                           &grease_pencil->vertex_group_names,
                                           curves.positions_for_write(),
                                           std::nullopt,
                                           deform_mats_opt,
                                           curves.deform_verts(),
                                           deform_flag,
                                           defgrp_name);

    EXPECT_EQ_SPAN(expected_positions(TargetDataType::GreasePencil, weighting, masking),
                   curves.positions());
    if (output == OutputValueTest::PositionAndDeformMatrix) {
      EXPECT_EQ_SPAN(expected_deform_mats(weighting), deform_mats.as_span());
    }

    BKE_id_delete(bmain, ob_arm);
    BKE_id_delete(bmain, ob_target);
  }
};

#ifdef USE_PARAMETERIZED_TESTS

using ArmatureDeformTestParams =
    std::tuple<InterpolationTest, OutputValueTest, WeightingTest, MaskingTest, VertexWeightSource>;

class ArmatureDeformParamTest : public ArmatureDeformTestBase,
                                public testing::TestWithParam<ArmatureDeformTestParams> {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_P(ArmatureDeformParamTest, MeshDeformParameterized)
{
  const ArmatureDeformTestParams &params = this->GetParam();
  InterpolationTest interpolation = std::get<0>(params);
  OutputValueTest output = std::get<1>(params);
  WeightingTest weighting = std::get<2>(params);
  MaskingTest masking = std::get<3>(params);
  VertexWeightSource dvert_source = std::get<4>(params);

  mesh_test(interpolation, output, weighting, masking, dvert_source);
}

TEST_P(ArmatureDeformParamTest, EditMeshDeformParameterized)
{
  const ArmatureDeformTestParams &params = this->GetParam();
  InterpolationTest interpolation = std::get<0>(params);
  OutputValueTest output = std::get<1>(params);
  WeightingTest weighting = std::get<2>(params);
  MaskingTest masking = std::get<3>(params);

  edit_mesh_test(interpolation, output, weighting, masking);
}

TEST_P(ArmatureDeformParamTest, CurvesDeformParameterized)
{
  const ArmatureDeformTestParams &params = this->GetParam();
  InterpolationTest interpolation = std::get<0>(params);
  OutputValueTest output = std::get<1>(params);
  WeightingTest weighting = std::get<2>(params);
  MaskingTest masking = std::get<3>(params);

  curves_test(interpolation, output, weighting, masking);
}

TEST_P(ArmatureDeformParamTest, GreasePencilDeformParameterized)
{
  const ArmatureDeformTestParams &params = this->GetParam();
  InterpolationTest interpolation = std::get<0>(params);
  OutputValueTest output = std::get<1>(params);
  WeightingTest weighting = std::get<2>(params);
  MaskingTest masking = std::get<3>(params);

  grease_pencil_test(interpolation, output, weighting, masking);
}

INSTANTIATE_TEST_SUITE_P(
    AllArmatureDeformTests,
    ArmatureDeformParamTest,
    testing::Combine(
        testing::Values(InterpolationTest::Linear, InterpolationTest::DualQuaternion),
        testing::Values(OutputValueTest::Position, OutputValueTest::PositionAndDeformMatrix),
        testing::Values(WeightingTest::None,
                        WeightingTest::Envelope,
                        WeightingTest::VertexGroups,
                        WeightingTest::EnvelopeAndVertexGroups),
        testing::Values(MaskingTest::All, MaskingTest::VertexGroup),
        testing::Values(VertexWeightSource::TargetObject, VertexWeightSource::SeparateMesh)),
    [](const testing::TestParamInfo<ArmatureDeformTestParams> &info) {
      InterpolationTest interpolation = std::get<0>(info.param);
      OutputValueTest output = std::get<1>(info.param);
      WeightingTest weighting = std::get<2>(info.param);
      MaskingTest masking = std::get<3>(info.param);
      VertexWeightSource dvert_source = std::get<4>(info.param);

      std::string name;
      switch (interpolation) {
        case InterpolationTest::Linear:
          name += "linear";
          break;
        case InterpolationTest::DualQuaternion:
          name += "dualquat";
          break;
      }
      switch (output) {
        case OutputValueTest::Position:
          name += "_pos";
          break;
        case OutputValueTest::PositionAndDeformMatrix:
          name += "_posdefmat";
          break;
      }
      switch (weighting) {
        case WeightingTest::None:
          name += "_disabled";
          break;
        case WeightingTest::Envelope:
          name += "_envelope";
          break;
        case WeightingTest::VertexGroups:
          name += "_vgroups";
          break;
        case WeightingTest::EnvelopeAndVertexGroups:
          name += "_envelopevgroups";
          break;
      }
      switch (masking) {
        case MaskingTest::All:
          break;
        case MaskingTest::VertexGroup:
          name += "_masked";
          break;
      }
      switch (dvert_source) {
        case VertexWeightSource::TargetObject:
          break;
        case VertexWeightSource::SeparateMesh:
          name += "_dvertsource";
          break;
      }
      return name;
    });

#else  // USE_PARAMETERIZED_TESTS

class ArmatureDeformTest : public ArmatureDeformTestBase, public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    bmain = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain);
  }
};

TEST_F(ArmatureDeformTest, MeshDeform)
{
  for (InterpolationTest ipol : {InterpolationTest::Linear, InterpolationTest::DualQuaternion}) {
    for (WeightingTest weight : {WeightingTest::None,
                                 WeightingTest::Envelope,
                                 WeightingTest::VertexGroups,
                                 WeightingTest::EnvelopeAndVertexGroups})
    {
      for (OutputValueTest output :
           {OutputValueTest::Position, OutputValueTest::PositionAndDeformMatrix})
      {
        for (MaskingTest mask : {MaskingTest::All, MaskingTest::VertexGroup}) {
          for (VertexWeightSource dvert_source :
               {VertexWeightSource::TargetObject, VertexWeightSource::SeparateMesh})
          {
            mesh_test(ipol, output, weight, mask, dvert_source);
          }
        }
      }
    }
  }
}

TEST_F(ArmatureDeformTest, EditMeshDeform)
{
  for (InterpolationTest ipol : {InterpolationTest::Linear, InterpolationTest::DualQuaternion}) {
    for (OutputValueTest output :
         {OutputValueTest::Position, OutputValueTest::PositionAndDeformMatrix})
    {
      for (WeightingTest weight : {WeightingTest::None,
                                   WeightingTest::Envelope,
                                   WeightingTest::VertexGroups,
                                   WeightingTest::EnvelopeAndVertexGroups})
      {
        for (MaskingTest mask : {MaskingTest::All, MaskingTest::VertexGroup}) {
          edit_mesh_test(ipol, output, weight, mask);
        }
      }
    }
  }
}

TEST_F(ArmatureDeformTest, CurvesDeform)
{
  for (InterpolationTest ipol : {InterpolationTest::Linear, InterpolationTest::DualQuaternion}) {
    for (OutputValueTest output :
         {OutputValueTest::Position, OutputValueTest::PositionAndDeformMatrix})
    {
      for (WeightingTest weight : {WeightingTest::None,
                                   WeightingTest::Envelope,
                                   WeightingTest::VertexGroups,
                                   WeightingTest::EnvelopeAndVertexGroups})
      {
        for (MaskingTest mask : {MaskingTest::All, MaskingTest::VertexGroup}) {
          curves_test(ipol, output, weight, mask);
        }
      }
    }
  }
}

TEST_F(ArmatureDeformTest, GreasePencilDeform)
{
  for (InterpolationTest ipol : {InterpolationTest::Linear, InterpolationTest::DualQuaternion}) {
    for (OutputValueTest output :
         {OutputValueTest::Position, OutputValueTest::PositionAndDeformMatrix})
    {
      for (WeightingTest weight : {WeightingTest::None,
                                   WeightingTest::Envelope,
                                   WeightingTest::VertexGroups,
                                   WeightingTest::EnvelopeAndVertexGroups})
      {
        for (MaskingTest mask : {MaskingTest::All, MaskingTest::VertexGroup}) {
          grease_pencil_test(ipol, output, weight, mask);
        }
      }
    }
  }
}

#endif

}  // namespace blender::bke::tests

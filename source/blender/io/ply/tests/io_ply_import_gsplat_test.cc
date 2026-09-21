/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_windowmanager_types.h"

#include "BKE_attribute.hh"
#include "BKE_gtest_setup.hh"
#include "BKE_lib_id.hh"
#include "BKE_report.hh"

#include "DNA_pointcloud_types.h"

#include "IO_gsplat.hh"
#include "IO_ply.hh"

#include "intern/ply_data.hh"
#include "ply_import_gsplat.hh"

namespace blender::io::ply {

/* Deterministic gaussian splat PLY data, built fully in memory.
 *
 * The f_rest_<i> attribute values encode their (channel, coefficient, point) index, so that any
 * confusion of the channel-major layout in the importer produces values that fail comparison
 * loudly rather than shifting silently. */
class GsplatPlyDataBuilder {
 public:
  int num_points;
  /* Total number of f_rest_<i> attributes, i.e. 3x the per-channel coefficient count for
   * consistent files. */
  int num_rest_attributes;

  GsplatPlyDataBuilder(const int num_points, const int num_rest_attributes)
      : num_points(num_points), num_rest_attributes(num_rest_attributes)
  {
  }

  float3 position(const int point) const
  {
    return float3(1.0f, 2.0f, 3.0f) * float(point + 1);
  }
  float f_dc(const int point, const int channel) const
  {
    return 0.5f + channel + 0.25f * point;
  }
  float opacity(const int point) const
  {
    return 0.25f * point - 0.5f;
  }
  float scale(const int point, const int axis) const
  {
    return -1.0f + 0.5f * axis + 0.125f * point;
  }
  float rotation(const int point, const int component) const
  {
    return (component == 0) ? 1.0f : 0.0625f * point;
  }
  float f_rest(const int point, const int attribute_index) const
  {
    /* Channel-major layout: the per-channel coefficient count of the file itself determines
     * which (channel, coefficient) pair an attribute holds. */
    const int num_file_coefficients = std::max(num_rest_attributes / 3, 1);
    const int channel = attribute_index / num_file_coefficients;
    const int coefficient = attribute_index % num_file_coefficients;
    return 100.0f * channel + coefficient + 0.25f * point;
  }
  /* The value the importer is expected to store for a given SH coefficient. */
  float3 expected_sh(const int point, const int coefficient) const
  {
    return float3(coefficient + 0.25f * point,
                  100.0f + coefficient + 0.25f * point,
                  200.0f + coefficient + 0.25f * point);
  }

  PlyData build() const
  {
    PlyData data;
    for (int point = 0; point < num_points; point++) {
      data.vertices.append(position(point));
    }

    auto add_attribute = [&](const std::string &name, auto value_for_point) {
      PlyCustomAttribute attribute(name, num_points);
      for (int point = 0; point < num_points; point++) {
        attribute.data[point] = value_for_point(point);
      }
      data.vertex_custom_attr.append(attribute);
    };

    for (int channel = 0; channel < 3; channel++) {
      add_attribute("f_dc_" + std::to_string(channel),
                    [&](const int point) { return f_dc(point, channel); });
    }
    add_attribute("opacity", [&](const int point) { return opacity(point); });
    for (int axis = 0; axis < 3; axis++) {
      add_attribute("scale_" + std::to_string(axis),
                    [&](const int point) { return scale(point, axis); });
    }
    for (int component = 0; component < 4; component++) {
      add_attribute("rot_" + std::to_string(component),
                    [&](const int point) { return rotation(point, component); });
    }
    for (int i = 0; i < num_rest_attributes; i++) {
      add_attribute("f_rest_" + std::to_string(i),
                    [&](const int point) { return f_rest(point, i); });
    }

    return data;
  }
};

class io_ply_import_gsplat : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    bke::gtest_setup();
  }
  static void TearDownTestSuite()
  {
    bke::gtest_teardown();
  }

  void SetUp() override
  {
    BKE_reports_init(&reports_, RPT_STORE);
    params_.reports = &reports_;
  }

  void TearDown() override
  {
    BKE_reports_free(&reports_);
  }

  bool has_warning_containing(const char *needle)
  {
    char *message = BKE_reports_string(&reports_, RPT_WARNING);
    if (message == nullptr) {
      return false;
    }
    const bool found = strstr(message, needle) != nullptr;
    MEM_delete(message);
    return found;
  }

  bool has_any_report()
  {
    return !BLI_listbase_is_empty(&reports_.list);
  }

  static int count_sh_attributes(const PointCloud &point_cloud)
  {
    const bke::AttributeAccessor attributes = point_cloud.attributes();
    int count = 0;
    while (attributes.contains("radiance:sh_" + std::to_string(count))) {
      count++;
    }
    return count;
  }

  void expect_sh_attributes(const PointCloud &point_cloud,
                            const GsplatPlyDataBuilder &builder,
                            const int expected_num_coefficients)
  {
    EXPECT_EQ(count_sh_attributes(point_cloud), expected_num_coefficients);

    const bke::AttributeAccessor attributes = point_cloud.attributes();
    for (int coefficient = 0; coefficient < expected_num_coefficients; coefficient++) {
      const bke::AttributeReader<float3> sh = attributes.lookup<float3>(
          "radiance:sh_" + std::to_string(coefficient), bke::AttrDomain::Point);
      ASSERT_TRUE(sh);
      for (int point = 0; point < builder.num_points; point++) {
        const float3 value = sh.varray[point];
        const float3 expected = builder.expected_sh(point, coefficient);
        for (int channel = 0; channel < 3; channel++) {
          EXPECT_FLOAT_EQ(value[channel], expected[channel])
              << "coefficient " << coefficient << " channel " << channel << " point " << point;
        }
      }
    }
  }

 protected:
  PLYImportParams params_;
  ReportList reports_;
};

TEST_F(io_ply_import_gsplat, missing_required_attributes)
{
  PlyData data;
  data.vertices.append(float3(0.0f));
  PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(data, params_);
  EXPECT_EQ(point_cloud, nullptr);
}

TEST_F(io_ply_import_gsplat, basic_attributes)
{
  const GsplatPlyDataBuilder builder(3, 9);
  PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(builder.build(), params_);
  ASSERT_NE(point_cloud, nullptr);
  ASSERT_EQ(point_cloud->totpoint, 3);
  EXPECT_EQ(point_cloud->type, PointCloudType::GSplat);

  const Span<float3> positions = point_cloud->positions();
  const bke::AttributeAccessor attributes = point_cloud->attributes();
  const bke::AttributeReader<float4> radiance_base = attributes.lookup<float4>(
      "radiance:base", bke::AttrDomain::Point);
  const bke::AttributeReader<float3> scales = attributes.lookup<float3>("scale",
                                                                        bke::AttrDomain::Point);
  const bke::AttributeReader<math::Quaternion> rotations = attributes.lookup<math::Quaternion>(
      "rotation", bke::AttrDomain::Point);
  ASSERT_TRUE(radiance_base);
  ASSERT_TRUE(scales);
  ASSERT_TRUE(rotations);

  for (int point = 0; point < builder.num_points; point++) {
    const float4 base = radiance_base.varray[point];
    const float3 scale = scales.varray[point];
    const math::Quaternion rotation = rotations.varray[point];
    for (int axis = 0; axis < 3; axis++) {
      EXPECT_FLOAT_EQ(positions[point][axis], builder.position(point)[axis]);
      EXPECT_FLOAT_EQ(base[axis], builder.f_dc(point, axis));
      EXPECT_FLOAT_EQ(
          scale[axis],
          gsplat::OriginalActivationFunctions::decode_scale(float3(
              builder.scale(point, 0), builder.scale(point, 1), builder.scale(point, 2)))[axis]);
    }
    EXPECT_FLOAT_EQ(base.w,
                    gsplat::OriginalActivationFunctions::decode_opacity(builder.opacity(point)));
    /* The importer normalizes the rotation. */
    const math::Quaternion expected_rotation = math::normalize(
        math::Quaternion(builder.rotation(point, 0),
                         builder.rotation(point, 1),
                         builder.rotation(point, 2),
                         builder.rotation(point, 3)));
    EXPECT_FLOAT_EQ(rotation.w, expected_rotation.w);
    EXPECT_FLOAT_EQ(rotation.x, expected_rotation.x);
    EXPECT_FLOAT_EQ(rotation.y, expected_rotation.y);
    EXPECT_FLOAT_EQ(rotation.z, expected_rotation.z);
  }

  BKE_id_free(nullptr, &point_cloud->id);
}

TEST_F(io_ply_import_gsplat, canonical_counts_import_all_coefficients)
{
  for (const int num_coefficients : {0, 3, 8, 15, 24}) {
    const GsplatPlyDataBuilder builder(2, num_coefficients * 3);
    PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(builder.build(), params_);
    ASSERT_NE(point_cloud, nullptr);

    expect_sh_attributes(*point_cloud, builder, num_coefficients);
    EXPECT_FALSE(has_any_report()) << "for coefficient count " << num_coefficients;

    BKE_id_free(nullptr, &point_cloud->id);
  }
}

TEST_F(io_ply_import_gsplat, non_canonical_count_keeps_channels)
{
  /* 4 coefficients per channel: only the first complete band (3 coefficients, degree 1) can be
   * imported, and the values must still come from the right channels of the file's own
   * channel-major layout. */
  const GsplatPlyDataBuilder builder(2, 4 * 3);
  PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(builder.build(), params_);
  ASSERT_NE(point_cloud, nullptr);

  expect_sh_attributes(*point_cloud, builder, 3);
  EXPECT_TRUE(has_warning_containing("spherical harmonics"));

  BKE_id_free(nullptr, &point_cloud->id);
}

TEST_F(io_ply_import_gsplat, mid_band_count_keeps_channels)
{
  /* 17 coefficients per channel: degrees up to 3 (15 coefficients) are complete. */
  const GsplatPlyDataBuilder builder(2, 17 * 3);
  PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(builder.build(), params_);
  ASSERT_NE(point_cloud, nullptr);

  expect_sh_attributes(*point_cloud, builder, 15);
  EXPECT_TRUE(has_warning_containing("spherical harmonics"));

  BKE_id_free(nullptr, &point_cloud->id);
}

TEST_F(io_ply_import_gsplat, count_not_multiple_of_three_drops_sh)
{
  const GsplatPlyDataBuilder builder(2, 7);
  PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(builder.build(), params_);
  ASSERT_NE(point_cloud, nullptr);

  EXPECT_EQ(count_sh_attributes(*point_cloud), 0);
  EXPECT_TRUE(has_warning_containing("spherical harmonics"));

  BKE_id_free(nullptr, &point_cloud->id);
}

TEST_F(io_ply_import_gsplat, above_max_degree_drops_sh)
{
  /* 25 coefficients per channel is beyond the supported degree 4. The channel offsets of such a
   * file do not line up with any supported layout, so no coefficients can be imported. */
  const GsplatPlyDataBuilder builder(2, 25 * 3);
  PointCloud *point_cloud = convert_gsplat_ply_to_point_cloud(builder.build(), params_);
  ASSERT_NE(point_cloud, nullptr);

  EXPECT_EQ(count_sh_attributes(*point_cloud), 0);
  EXPECT_TRUE(has_warning_containing("spherical harmonics"));

  BKE_id_free(nullptr, &point_cloud->id);
}

}  // namespace blender::io::ply

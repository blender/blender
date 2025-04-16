/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "vk_memory_layout.hh"

namespace blender::gpu {

template<typename Layout>
static void def_attr(const shader::Type type,
                     const int array_size,
                     const uint32_t expected_alignment,
                     const uint32_t expected_reserve,
                     uint32_t *r_offset)
{
  align<Layout>(type, array_size, r_offset);
  EXPECT_EQ(*r_offset, expected_alignment);
  reserve<Layout>(type, array_size, r_offset);
  EXPECT_EQ(*r_offset, expected_reserve);
}

TEST(std140, fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 0, 0, 4, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, _2fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::float_t, 0, 4, 8, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, _3fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::float_t, 0, 4, 8, &offset);
  def_attr<Std140>(shader::Type::float_t, 0, 8, 12, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, _4fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::float_t, 0, 4, 8, &offset);
  def_attr<Std140>(shader::Type::float_t, 0, 8, 12, &offset);
  def_attr<Std140>(shader::Type::float_t, 0, 12, 16, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, fl2)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 2, 0, 32, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 32);
}

TEST(std140, fl_fl2)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::float_t, 2, 16, 48, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 48);
}

TEST(std140, fl_vec2)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float_t, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::float2_t, 0, 8, 16, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, gpu_shader_2D_widget_base)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::float4_t, 12, 0, 192, &offset);
  def_attr<Std140>(shader::Type::float4x4_t, 0, 192, 256, &offset);
  def_attr<Std140>(shader::Type::float3_t, 0, 256, 268, &offset);
  def_attr<Std140>(shader::Type::bool_t, 0, 268, 272, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 272);
}

TEST(std430, overlay_grid)
{
  uint32_t offset = 0;

  def_attr<Std430>(shader::Type::float3_t, 0, 0, 12, &offset);
  def_attr<Std430>(shader::Type::int_t, 0, 12, 16, &offset);

  align_end_of_struct<Std430>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std430, simple_lighting)
{
  uint32_t offset = 0;

  def_attr<Std430>(shader::Type::float4x4_t, 0, 0, 64, &offset);
  def_attr<Std430>(shader::Type::float3x3_t, 0, 64, 112, &offset);

  align_end_of_struct<Std430>(&offset);
  EXPECT_EQ(offset, 112);
}

TEST(std430, compositor_cryptomatte_matte_compute)
{
  uint32_t offset = 0;

  def_attr<Std430>(shader::Type::float2_t, 0, 0, 8, &offset);
  def_attr<Std430>(shader::Type::float_t, 0, 8, 12, &offset);
  def_attr<Std430>(shader::Type::float_t, 32, 12, 140, &offset);

  align_end_of_struct<Std430>(&offset);
  EXPECT_EQ(offset, 144);
}

}  // namespace blender::gpu

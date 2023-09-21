/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "../vulkan/vk_memory_layout.hh"

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

  def_attr<Std140>(shader::Type::FLOAT, 0, 0, 4, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, _2fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::FLOAT, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 0, 4, 8, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, _3fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::FLOAT, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 0, 4, 8, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 0, 8, 12, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, _4fl)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::FLOAT, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 0, 4, 8, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 0, 8, 12, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 0, 12, 16, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, fl2)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::FLOAT, 2, 0, 32, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 32);
}

TEST(std140, fl_fl2)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::FLOAT, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::FLOAT, 2, 16, 48, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 48);
}

TEST(std140, fl_vec2)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::FLOAT, 0, 0, 4, &offset);
  def_attr<Std140>(shader::Type::VEC2, 0, 8, 16, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 16);
}

TEST(std140, gpu_shader_2D_widget_base)
{
  uint32_t offset = 0;

  def_attr<Std140>(shader::Type::VEC4, 12, 0, 192, &offset);
  def_attr<Std140>(shader::Type::MAT4, 0, 192, 256, &offset);
  def_attr<Std140>(shader::Type::VEC3, 0, 256, 268, &offset);
  def_attr<Std140>(shader::Type::BOOL, 0, 268, 272, &offset);

  align_end_of_struct<Std140>(&offset);
  EXPECT_EQ(offset, 272);
}

TEST(std430, overlay_grid)
{
  uint32_t offset = 0;

  def_attr<Std430>(shader::Type::VEC3, 0, 0, 12, &offset);
  def_attr<Std430>(shader::Type::INT, 0, 12, 16, &offset);

  align_end_of_struct<Std430>(&offset);
  EXPECT_EQ(offset, 16);
}

}  // namespace blender::gpu

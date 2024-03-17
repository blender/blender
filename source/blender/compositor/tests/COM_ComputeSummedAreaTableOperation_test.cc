/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "COM_SummedAreaTableOperation.h"

namespace blender::compositor::tests {

struct SatParams {
  /* Input parameters. */
  SummedAreaTableOperation::eMode mode;
  rcti area;
  float4 fill_value;

  /* Expected output values. */
  std::vector<std::vector<float>> values;
};

class SummedAreaTableTestP : public testing::TestWithParam<SatParams> {};

TEST_P(SummedAreaTableTestP, Values)
{
  SatParams params = GetParam();

  SummedAreaTableOperation sat = SummedAreaTableOperation();

  sat.set_mode(params.mode);
  const rcti area = params.area;
  MemoryBuffer output(DataType::Color, area);

  std::shared_ptr<MemoryBuffer> input = std::make_shared<MemoryBuffer>(DataType::Color, area);
  input->fill(area, &params.fill_value.x);

  sat.update_memory_buffer(&output, area, Span<MemoryBuffer *>{input.get()});

  /* First row. */
  EXPECT_FLOAT_EQ(output.get_elem(0, 0)[0], params.values[0][0]);
  EXPECT_FLOAT_EQ(output.get_elem(1, 0)[1], params.values[0][1]);
  EXPECT_FLOAT_EQ(output.get_elem(2, 0)[2], params.values[0][2]);

  /* Second row. */
  EXPECT_FLOAT_EQ(output.get_elem(0, 1)[3], params.values[1][0]);
  EXPECT_FLOAT_EQ(output.get_elem(1, 1)[0], params.values[1][1]);
  EXPECT_FLOAT_EQ(output.get_elem(2, 1)[1], params.values[1][2]);
}

INSTANTIATE_TEST_SUITE_P(FullFrame5x2_IdentityOnes,
                         SummedAreaTableTestP,
                         testing::Values(SatParams{
                             SummedAreaTableOperation::eMode::Identity,
                             rcti{0, 5, 0, 2},         /* Area. */
                             {1.0f, 1.0f, 1.0f, 1.0f}, /* Fill value. */

                             /* Expected output. */
                             {{1.0f, 2.0f, 3.0f, 4.0f, 5.0f}, {2.0f, 4.0f, 6.0f, 8.0f, 10.0f}}

                         }));

INSTANTIATE_TEST_SUITE_P(
    FullFrame5x2_SquaredOnes,
    SummedAreaTableTestP,
    testing::Values(SatParams{
        SummedAreaTableOperation::eMode::Squared,
        rcti{0, 5, 0, 2},         /* Area. */
        {1.0f, 1.0f, 1.0f, 1.0f}, /* Fill value. */

        /* Expect identical to when using Identity SAT, since all inputs are 1. */
        {{1.0f, 2.0f, 3.0f, 4.0f, 5.0f}, {2.0f, 4.0f, 6.0f, 8.0f, 10.0f}}

    }));

INSTANTIATE_TEST_SUITE_P(FullFrame3x2_Squared,
                         SummedAreaTableTestP,
                         testing::Values(SatParams{SummedAreaTableOperation::eMode::Squared,
                                                   rcti{0, 3, 0, 2},        /* Area. */
                                                   {2.0f, 2.0f, 1.5f, .1f}, /* Fill value. */

                                                   /* Expected output. */
                                                   {
                                                       {4.0f, 8.0f, 6.75f},
                                                       {0.02f, 16.0f, 24.0f},
                                                   }}));

class SummedAreaTableSumTest : public ::testing::Test {
 public:
  SummedAreaTableSumTest()
  {
    operation_ = std::make_shared<SummedAreaTableOperation>();
  }

 protected:
  void SetUp() override
  {
    operation_->set_mode(SummedAreaTableOperation::eMode::Squared);

    area_ = rcti{0, 5, 0, 4};
    sat_ = std::make_shared<MemoryBuffer>(DataType::Color, area_);

    const float val[4] = {1.0f, 2.0f, 1.5f, 0.1f};
    std::shared_ptr<MemoryBuffer> input = std::make_shared<MemoryBuffer>(DataType::Color, area_);
    input->fill(area_, val);
    std::shared_ptr<MemoryBuffer> offset = std::make_shared<MemoryBuffer>(
        DataType::Value, area_, true);
    offset->fill(area_, &offset_);

    operation_->update_memory_buffer(
        sat_.get(), area_, Span<MemoryBuffer *>{input.get(), offset.get()});
  }

  std::shared_ptr<SummedAreaTableOperation> operation_;
  std::shared_ptr<MemoryBuffer> sat_;
  rcti area_;
  float offset_ = 0.0f;
};

TEST_F(SummedAreaTableSumTest, FullyInside)
{
  rcti area;
  area.xmin = 1;
  area.xmax = 3;
  area.ymin = 1;
  area.ymax = 3;
  float4 sum = summed_area_table_sum(sat_.get(), area);
  EXPECT_EQ(sum[0], 9);
}

TEST_F(SummedAreaTableSumTest, LeftEdge)
{
  rcti area;
  area.xmin = 0;
  area.xmax = 2;
  area.ymin = 0;
  area.ymax = 2;
  float4 sum = summed_area_table_sum(sat_.get(), area);
  EXPECT_EQ(sum[0], 9);
}

TEST_F(SummedAreaTableSumTest, RightEdge)
{
  rcti area;
  area.xmin = area_.xmax - 2;
  area.xmax = area_.xmax;
  area.ymin = 0;
  area.ymax = 2;
  float4 sum = summed_area_table_sum(sat_.get(), area);
  EXPECT_EQ(sum[0], 6);
}

TEST_F(SummedAreaTableSumTest, LowerRightCorner)
{
  rcti area;
  area.xmin = area_.xmax - 1;
  area.xmax = area_.xmax;
  area.ymin = area_.ymax - 1;
  area.ymax = area_.ymax;
  float4 sum = summed_area_table_sum(sat_.get(), area);
  EXPECT_EQ(sum[0], 1);
}

TEST_F(SummedAreaTableSumTest, TopLine)
{
  rcti area;
  area.xmin = 0;
  area.xmax = 1;
  area.ymin = 0;
  area.ymax = 0;
  float4 sum = summed_area_table_sum(sat_.get(), area);
  EXPECT_EQ(sum[0], 2);
}

TEST_F(SummedAreaTableSumTest, ButtomLine)
{
  rcti area;
  area.xmin = 0;
  area.xmax = 4;
  area.ymin = 3;
  area.ymax = 3;
  float4 sum = summed_area_table_sum(sat_.get(), area);
  EXPECT_EQ(sum[0], 5);
}

}  // namespace blender::compositor::tests

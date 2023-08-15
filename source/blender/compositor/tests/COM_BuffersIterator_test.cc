/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_array.hh"
#include "COM_BuffersIterator.h"

namespace blender::compositor::tests {

constexpr int BUFFER_WIDTH = 5;
constexpr int BUFFER_HEIGHT = 4;
constexpr int BUFFER_OFFSET_X = 5;
constexpr int BUFFER_OFFSET_Y = 6;
constexpr int NUM_CHANNELS = 4;
constexpr int FULL_BUFFER_LEN = BUFFER_WIDTH * BUFFER_HEIGHT * NUM_CHANNELS;
constexpr int SINGLE_ELEM_BUFFER_LEN = NUM_CHANNELS;
constexpr int NUM_INPUTS = 2;

static float *create_buffer(int len)
{
  return (float *)MEM_callocN(len * sizeof(float), "COM_BuffersIteratorTest");
}

static const float *create_input_buffer(int input_idx, bool is_a_single_elem)
{
  const int len = is_a_single_elem ? SINGLE_ELEM_BUFFER_LEN : FULL_BUFFER_LEN;
  float *buf = create_buffer(len);
  /* Fill buffer with variable data. */
  for (int i = 0; i < len; i++) {
    buf[i] = input_idx * 1.5f * (i + 1) + i * 0.9f;
  }
  return buf;
}

using IterFunc = std::function<void(BuffersIterator<float> &it, const rcti &area)>;
using ValidateElemFunc = std::function<void(float *out, Span<const float *> ins, int x, int y)>;

class BuffersIteratorTest : public testing::Test {
 private:
  float *output_;
  bool use_offsets_;
  bool use_single_elem_inputs_;
  bool use_inputs_;

  static rcti buffer_area;
  static rcti buffer_offset_area;
  static Array<const float *, NUM_INPUTS> single_elem_inputs;
  static Array<const float *, NUM_INPUTS> full_buffer_inputs;

 public:
  void set_inputs_enabled(bool value)
  {
    use_inputs_ = value;
  }

  void test_iteration(IterFunc iter_func, ValidateElemFunc validate_elem_func = {})
  {
    use_single_elem_inputs_ = false;
    validate_iteration(iter_func, validate_elem_func);
    if (use_inputs_) {
      use_single_elem_inputs_ = true;
      validate_iteration(iter_func, validate_elem_func);
    }
  }

 protected:
  static void SetUpTestCase()
  {
    BLI_rcti_init(&buffer_area, 0, BUFFER_WIDTH, 0, BUFFER_HEIGHT);
    BLI_rcti_init(&buffer_offset_area,
                  BUFFER_OFFSET_X,
                  BUFFER_OFFSET_X + BUFFER_WIDTH,
                  BUFFER_OFFSET_Y,
                  BUFFER_OFFSET_Y + BUFFER_HEIGHT);
    for (int i = 0; i < NUM_INPUTS; i++) {
      single_elem_inputs[i] = create_input_buffer(i, true);
      full_buffer_inputs[i] = create_input_buffer(i, false);
    }
  }

  static void TearDownTestCase()
  {
    for (int i = 0; i < NUM_INPUTS; i++) {
      MEM_freeN((void *)single_elem_inputs[i]);
      single_elem_inputs[i] = nullptr;
      MEM_freeN((void *)full_buffer_inputs[i]);
      full_buffer_inputs[i] = nullptr;
    }
  }

  void SetUp() override
  {
    use_offsets_ = false;
    use_single_elem_inputs_ = false;
    use_inputs_ = false;
    output_ = create_buffer(FULL_BUFFER_LEN);
  }

  void TearDown() override
  {
    MEM_freeN(output_);
  }

 private:
  void validate_iteration(IterFunc iter_func, ValidateElemFunc validate_elem_func)
  {
    {
      use_offsets_ = false;
      BuffersIterator<float> it = iterate();
      iter_func(it, buffer_area);
      validate_result(buffer_area, validate_elem_func);
    }
    {
      use_offsets_ = true;
      BuffersIterator<float> it = offset_iterate(buffer_offset_area);
      iter_func(it, buffer_offset_area);
      validate_result(buffer_offset_area, validate_elem_func);
    }
    {
      use_offsets_ = true;
      rcti area = buffer_offset_area;
      area.xmin += 1;
      area.ymin += 1;
      area.xmax -= 1;
      area.ymax -= 1;
      BuffersIterator<float> it = offset_iterate(area);
      iter_func(it, area);
      validate_result(area, validate_elem_func);
    }
  }

  void validate_result(rcti &area, ValidateElemFunc validate_elem_func)
  {
    Span<const float *> inputs = get_inputs();
    Array<const float *> ins(inputs.size());
    for (int y = area.ymin; y < area.ymax; y++) {
      for (int x = area.xmin; x < area.xmax; x++) {
        const int out_offset = get_buffer_relative_y(y) * BUFFER_WIDTH * NUM_CHANNELS +
                               get_buffer_relative_x(x) * NUM_CHANNELS;
        float *out = &output_[out_offset];

        const int in_offset = use_single_elem_inputs_ ? 0 : out_offset;
        for (int i = 0; i < inputs.size(); i++) {
          ins[i] = &inputs[i][in_offset];
        }

        if (validate_elem_func) {
          validate_elem_func(out, ins, x, y);
        }
      }
    }
  }

  Span<const float *> get_inputs()
  {
    if (use_inputs_) {
      return use_single_elem_inputs_ ? single_elem_inputs : full_buffer_inputs;
    }
    return {};
  }

  int get_buffer_relative_x(int x)
  {
    return use_offsets_ ? x - BUFFER_OFFSET_X : x;
  }
  int get_buffer_relative_y(int y)
  {
    return use_offsets_ ? y - BUFFER_OFFSET_Y : y;
  }

  /** Iterates whole buffers with no offsets. */
  BuffersIterator<float> iterate()
  {
    BLI_assert(!use_offsets_);
    BuffersIteratorBuilder<float> builder(output_, BUFFER_WIDTH, BUFFER_HEIGHT, NUM_CHANNELS);
    if (use_inputs_) {
      const int input_stride = use_single_elem_inputs_ ? 0 : NUM_CHANNELS;
      for (const float *input : get_inputs()) {
        builder.add_input(input, BUFFER_WIDTH, input_stride);
      }
    }
    return builder.build();
  }

  /** Iterates a given buffers area with default offsets. */
  BuffersIterator<float> offset_iterate(const rcti &area)
  {
    BLI_assert(use_offsets_);
    const rcti &buf_area = buffer_offset_area;
    BuffersIteratorBuilder<float> builder(output_, buf_area, area, NUM_CHANNELS);
    if (use_inputs_) {
      const int input_stride = use_single_elem_inputs_ ? 0 : NUM_CHANNELS;
      for (const float *input : get_inputs()) {
        builder.add_input(input, buf_area, input_stride);
      }
    }
    return builder.build();
  }
};

rcti BuffersIteratorTest::buffer_area;
rcti BuffersIteratorTest::buffer_offset_area;
Array<const float *, NUM_INPUTS> BuffersIteratorTest::single_elem_inputs(NUM_INPUTS);
Array<const float *, NUM_INPUTS> BuffersIteratorTest::full_buffer_inputs(NUM_INPUTS);

static void iterate_coordinates(BuffersIterator<float> &it, const rcti &area)
{
  int x = area.xmin;
  int y = area.ymin;
  for (; !it.is_end(); ++it) {
    EXPECT_EQ(x, it.x);
    EXPECT_EQ(y, it.y);
    x++;
    if (x == area.xmax) {
      x = area.xmin;
      y++;
    }
  }
  EXPECT_EQ(x, area.xmin);
  EXPECT_EQ(y, area.ymax);
}

TEST_F(BuffersIteratorTest, CoordinatesIterationWithNoInputs)
{
  set_inputs_enabled(false);
  test_iteration(iterate_coordinates);
}

TEST_F(BuffersIteratorTest, CoordinatesIterationWithInputs)
{
  set_inputs_enabled(true);
  test_iteration(iterate_coordinates);
}

TEST_F(BuffersIteratorTest, OutputIteration)
{
  set_inputs_enabled(false);
  test_iteration(
      [](BuffersIterator<float> &it, const rcti & /*area*/) {
        EXPECT_EQ(it.get_num_inputs(), 0);
        for (; !it.is_end(); ++it) {
          const int dummy = it.y * BUFFER_WIDTH + it.x;
          it.out[0] = dummy + 1.0f;
          it.out[1] = dummy + 2.0f;
          it.out[2] = dummy + 3.0f;
          it.out[3] = dummy + 4.0f;
        }
      },
      [](float *out, Span<const float *> /*ins*/, const int x, const int y) {
        const int dummy = y * BUFFER_WIDTH + x;
        EXPECT_NEAR(out[0], dummy + 1.0f, FLT_EPSILON);
        EXPECT_NEAR(out[1], dummy + 2.0f, FLT_EPSILON);
        EXPECT_NEAR(out[2], dummy + 3.0f, FLT_EPSILON);
        EXPECT_NEAR(out[3], dummy + 4.0f, FLT_EPSILON);
      });
}

TEST_F(BuffersIteratorTest, OutputAndInputsIteration)
{
  set_inputs_enabled(true);
  test_iteration(
      [](BuffersIterator<float> &it, const rcti & /*area*/) {
        EXPECT_EQ(it.get_num_inputs(), NUM_INPUTS);
        for (; !it.is_end(); ++it) {
          const float *in1 = it.in(0);
          const float *in2 = it.in(1);
          it.out[0] = in1[0] + in2[0];
          it.out[1] = in1[1] + in2[3];
          it.out[2] = in1[2] - in2[2];
          it.out[3] = in1[3] - in2[1];
        }
      },
      [](float *out, Span<const float *> ins, const int /*x*/, const int /*y*/) {
        const float *in1 = ins[0];
        const float *in2 = ins[1];
        EXPECT_NEAR(out[0], in1[0] + in2[0], FLT_EPSILON);
        EXPECT_NEAR(out[1], in1[1] + in2[3], FLT_EPSILON);
        EXPECT_NEAR(out[2], in1[2] - in2[2], FLT_EPSILON);
        EXPECT_NEAR(out[3], in1[3] - in2[1], FLT_EPSILON);
      });
}

}  // namespace blender::compositor::tests

/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PlaneCornerPinOperation.h"
#include "COM_ConstantOperation.h"

namespace blender::compositor {

constexpr int LOWER_LEFT_CORNER_INDEX = 0;
constexpr int LOWER_RIGHT_CORNER_INDEX = 1;
constexpr int UPPER_RIGHT_CORNER_INDEX = 2;
constexpr int UPPER_LEFT_CORNER_INDEX = 3;

static bool check_corners(float corners[4][2])
{
  int i, next, prev;
  float cross = 0.0f;

  for (i = 0; i < 4; i++) {
    float v1[2], v2[2], cur_cross;

    next = (i + 1) % 4;
    prev = (4 + i - 1) % 4;

    sub_v2_v2v2(v1, corners[i], corners[prev]);
    sub_v2_v2v2(v2, corners[next], corners[i]);

    cur_cross = cross_v2v2(v1, v2);
    if (fabsf(cur_cross) <= FLT_EPSILON) {
      return false;
    }

    if (cross == 0.0f) {
      cross = cur_cross;
    }
    else if (cross * cur_cross < 0.0f) {
      return false;
    }
  }

  return true;
}

static void set_default_corner(const int corner_idx, float r_corner[2])
{
  BLI_assert(corner_idx >= 0 && corner_idx < 4);
  switch (corner_idx) {
    case LOWER_LEFT_CORNER_INDEX:
      r_corner[0] = 0.0f;
      r_corner[1] = 0.0f;
      break;
    case LOWER_RIGHT_CORNER_INDEX:
      r_corner[0] = 1.0f;
      r_corner[1] = 0.0f;
      break;
    case UPPER_RIGHT_CORNER_INDEX:
      r_corner[0] = 1.0f;
      r_corner[1] = 1.0f;
      break;
    case UPPER_LEFT_CORNER_INDEX:
      r_corner[0] = 0.0f;
      r_corner[1] = 1.0f;
      break;
  }
}

static void read_input_corners(NodeOperation *op, const int first_input_idx, float r_corners[4][2])
{
  for (const int i : IndexRange(4)) {
    NodeOperation *input = op->get_input_operation(i + first_input_idx);
    if (input->get_flags().is_constant_operation) {
      ConstantOperation *corner_input = static_cast<ConstantOperation *>(input);
      copy_v2_v2(r_corners[i], corner_input->get_constant_elem());
    }
    else {
      set_default_corner(i, r_corners[i]);
    }
  }

  /* Convexity check: concave corners need to be prevented, otherwise
   * #BKE_tracking_homography_between_two_quads will freeze. */
  if (!check_corners(r_corners)) {
    /* Revert to default corners. There could be a more elegant solution,
     * this prevents freezing at least. */
    for (const int i : IndexRange(4)) {
      set_default_corner(i, r_corners[i]);
    }
  }
}

/* ******** PlaneCornerPinMaskOperation ******** */

PlaneCornerPinMaskOperation::PlaneCornerPinMaskOperation()
{
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
}

void PlaneCornerPinMaskOperation::init_data()
{
  float corners[4][2];
  read_input_corners(this, 0, corners);
  calculate_corners(corners, true, 0);
}

void PlaneCornerPinMaskOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  /* Determine input canvases. */
  PlaneDistortMaskOperation::determine_canvas(preferred_area, r_area);
  r_area = preferred_area;
}

void PlaneCornerPinMaskOperation::get_area_of_interest(const int /*input_idx*/,
                                                       const rcti & /*output_area*/,
                                                       rcti &r_input_area)
{
  /* All corner inputs are used as constants. */
  r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
}

/* ******** PlaneCornerPinWarpImageOperation ******** */

PlaneCornerPinWarpImageOperation::PlaneCornerPinWarpImageOperation()
{
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
}

void PlaneCornerPinWarpImageOperation::init_data()
{
  float corners[4][2];
  read_input_corners(this, 1, corners);
  calculate_corners(corners, true, 0);
}

void PlaneCornerPinWarpImageOperation::get_area_of_interest(const int input_idx,
                                                            const rcti &output_area,
                                                            rcti &r_input_area)
{
  if (input_idx == 0) {
    PlaneDistortWarpImageOperation::get_area_of_interest(input_idx, output_area, r_input_area);
  }
  else {
    /* Corner inputs are used as constants. */
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
  }
}

}  // namespace blender::compositor

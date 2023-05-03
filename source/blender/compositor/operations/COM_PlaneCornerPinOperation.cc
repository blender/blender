/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2014 Blender Foundation. */

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

/* TODO(manzanilla): to be removed with tiled implementation. */
static void read_corners_from_sockets(rcti *rect, SocketReader *readers[4], float corners[4][2])
{
  for (int i = 0; i < 4; i++) {
    float result[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    readers[i]->read_sampled(result, rect->xmin, rect->ymin, PixelSampler::Nearest);
    corners[i][0] = result[0];
    corners[i][1] = result[1];
  }

  /* convexity check:
   * concave corners need to be prevented, otherwise
   * BKE_tracking_homography_between_two_quads will freeze
   */
  if (!check_corners(corners)) {
    /* simply revert to default corners
     * there could be a more elegant solution,
     * this prevents freezing at least.
     */
    corners[0][0] = 0.0f;
    corners[0][1] = 0.0f;
    corners[1][0] = 1.0f;
    corners[1][1] = 0.0f;
    corners[2][0] = 1.0f;
    corners[2][1] = 1.0f;
    corners[3][0] = 0.0f;
    corners[3][1] = 1.0f;
  }
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

PlaneCornerPinMaskOperation::PlaneCornerPinMaskOperation() : corners_ready_(false)
{
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);

  /* XXX this is stupid: we need to make this "complex",
   * so we can use the initialize_tile_data function
   * to read corners from input sockets ...
   */
  flags_.complex = true;
}

void PlaneCornerPinMaskOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    float corners[4][2];
    read_input_corners(this, 0, corners);
    calculate_corners(corners, true, 0);
  }
}

/* TODO(manzanilla): to be removed with tiled implementation. Same for #deinit_execution and do the
 * same on #PlaneCornerPinWarpImageOperation. */
void PlaneCornerPinMaskOperation::init_execution()
{
  PlaneDistortMaskOperation::init_execution();

  init_mutex();
}

void PlaneCornerPinMaskOperation::deinit_execution()
{
  PlaneDistortMaskOperation::deinit_execution();

  deinit_mutex();
}

void *PlaneCornerPinMaskOperation::initialize_tile_data(rcti *rect)
{
  void *data = PlaneDistortMaskOperation::initialize_tile_data(rect);

  /* get corner values once, by reading inputs at (0,0)
   * XXX this assumes invariable values (no image inputs),
   * we don't have a nice generic system for that yet
   */
  lock_mutex();
  if (!corners_ready_) {
    SocketReader *readers[4] = {
        get_input_socket_reader(0),
        get_input_socket_reader(1),
        get_input_socket_reader(2),
        get_input_socket_reader(3),
    };
    float corners[4][2];
    read_corners_from_sockets(rect, readers, corners);
    calculate_corners(corners, true, 0);

    corners_ready_ = true;
  }
  unlock_mutex();

  return data;
}

void PlaneCornerPinMaskOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    /* Determine input canvases. */
    PlaneDistortMaskOperation::determine_canvas(preferred_area, r_area);
  }
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

PlaneCornerPinWarpImageOperation::PlaneCornerPinWarpImageOperation() : corners_ready_(false)
{
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
  add_input_socket(DataType::Vector);
}

void PlaneCornerPinWarpImageOperation::init_data()
{
  if (execution_model_ == eExecutionModel::FullFrame) {
    float corners[4][2];
    read_input_corners(this, 1, corners);
    calculate_corners(corners, true, 0);
  }
}

void PlaneCornerPinWarpImageOperation::init_execution()
{
  PlaneDistortWarpImageOperation::init_execution();

  init_mutex();
}

void PlaneCornerPinWarpImageOperation::deinit_execution()
{
  PlaneDistortWarpImageOperation::deinit_execution();

  deinit_mutex();
}

void *PlaneCornerPinWarpImageOperation::initialize_tile_data(rcti *rect)
{
  void *data = PlaneDistortWarpImageOperation::initialize_tile_data(rect);

  /* get corner values once, by reading inputs at (0,0)
   * XXX this assumes invariable values (no image inputs),
   * we don't have a nice generic system for that yet
   */
  lock_mutex();
  if (!corners_ready_) {
    /* corner sockets start at index 1 */
    SocketReader *readers[4] = {
        get_input_socket_reader(1),
        get_input_socket_reader(2),
        get_input_socket_reader(3),
        get_input_socket_reader(4),
    };
    float corners[4][2];
    read_corners_from_sockets(rect, readers, corners);
    calculate_corners(corners, true, 0);

    corners_ready_ = true;
  }
  unlock_mutex();

  return data;
}

bool PlaneCornerPinWarpImageOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  for (int i = 0; i < 4; i++) {
    if (get_input_operation(i + 1)->determine_depending_area_of_interest(
            input, read_operation, output))
    {
      return true;
    }
  }

  /* XXX this is bad, but unavoidable with the current design:
   * we don't know the actual corners and matrix at this point,
   * so all we can do is get the full input image
   */
  output->xmin = 0;
  output->ymin = 0;
  output->xmax = get_input_operation(0)->get_width();
  output->ymax = get_input_operation(0)->get_height();
  return true;
#if 0
  return PlaneDistortWarpImageOperation::determine_depending_area_of_interest(
      input, read_operation, output);
#endif
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

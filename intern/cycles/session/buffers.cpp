/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include <stdlib.h>

#include "device/device.h"
#include "session/buffers.h"

#include "util/foreach.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/time.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* --------------------------------------------------------------------
 * Convert part information to an index of `BufferParams::pass_offset_`.
 */

static int pass_type_mode_to_index(PassType pass_type, PassMode mode)
{
  int index = static_cast<int>(pass_type) * 2;

  if (mode == PassMode::DENOISED) {
    ++index;
  }

  return index;
}

static int pass_to_index(const BufferPass &pass)
{
  return pass_type_mode_to_index(pass.type, pass.mode);
}

/* --------------------------------------------------------------------
 * Buffer pass.
 */

NODE_DEFINE(BufferPass)
{
  NodeType *type = NodeType::add("buffer_pass", create);

  const NodeEnum *pass_type_enum = Pass::get_type_enum();
  const NodeEnum *pass_mode_enum = Pass::get_mode_enum();

  SOCKET_ENUM(type, "Type", *pass_type_enum, PASS_COMBINED);
  SOCKET_ENUM(mode, "Mode", *pass_mode_enum, static_cast<int>(PassMode::DENOISED));
  SOCKET_STRING(name, "Name", ustring());
  SOCKET_BOOLEAN(include_albedo, "Include Albedo", false);
  SOCKET_STRING(lightgroup, "Light Group", ustring());

  SOCKET_INT(offset, "Offset", -1);

  return type;
}

BufferPass::BufferPass() : Node(get_node_type()) {}

BufferPass::BufferPass(const Pass *scene_pass)
    : Node(get_node_type()),
      type(scene_pass->get_type()),
      mode(scene_pass->get_mode()),
      name(scene_pass->get_name()),
      include_albedo(scene_pass->get_include_albedo()),
      lightgroup(scene_pass->get_lightgroup())
{
}

PassInfo BufferPass::get_info() const
{
  return Pass::get_info(type, include_albedo, !lightgroup.empty());
}

/* --------------------------------------------------------------------
 * Buffer Params.
 */

NODE_DEFINE(BufferParams)
{
  NodeType *type = NodeType::add("buffer_params", create);

  SOCKET_INT(width, "Width", 0);
  SOCKET_INT(height, "Height", 0);

  SOCKET_INT(window_x, "Window X", 0);
  SOCKET_INT(window_y, "Window Y", 0);
  SOCKET_INT(window_width, "Window Width", 0);
  SOCKET_INT(window_height, "Window Height", 0);

  SOCKET_INT(full_x, "Full X", 0);
  SOCKET_INT(full_y, "Full Y", 0);
  SOCKET_INT(full_width, "Full Width", 0);
  SOCKET_INT(full_height, "Full Height", 0);

  SOCKET_STRING(layer, "Layer", ustring());
  SOCKET_STRING(view, "View", ustring());
  SOCKET_INT(samples, "Samples", 0);
  SOCKET_FLOAT(exposure, "Exposure", 1.0f);
  SOCKET_BOOLEAN(use_approximate_shadow_catcher, "Use Approximate Shadow Catcher", false);
  SOCKET_BOOLEAN(use_transparent_background, "Transparent Background", false);

  /* Notes:
   *  - Skip passes since they do not follow typical container socket definition.
   *    Might look into covering those as a socket in the future.
   *
   *  - Skip offset, stride, and pass stride since those can be delivered from the passes and
   *    rest of the sockets. */

  return type;
}

BufferParams::BufferParams() : Node(get_node_type())
{
  reset_pass_offset();
}

void BufferParams::update_passes()
{
  update_offset_stride();
  reset_pass_offset();

  pass_stride = 0;
  for (const BufferPass &pass : passes) {
    if (pass.offset != PASS_UNUSED) {
      const int index = pass_to_index(pass);
      if (pass_offset_[index] == PASS_UNUSED) {
        pass_offset_[index] = pass_stride;
      }

      pass_stride += pass.get_info().num_components;
    }
  }
}

void BufferParams::update_passes(const vector<Pass *> &scene_passes)
{
  passes.clear();

  pass_stride = 0;
  for (const Pass *scene_pass : scene_passes) {
    BufferPass buffer_pass(scene_pass);

    if (scene_pass->is_written()) {
      buffer_pass.offset = pass_stride;
      pass_stride += scene_pass->get_info().num_components;
    }
    else {
      buffer_pass.offset = PASS_UNUSED;
    }

    passes.emplace_back(std::move(buffer_pass));
  }

  update_passes();
}

void BufferParams::reset_pass_offset()
{
  for (int i = 0; i < kNumPassOffsets; ++i) {
    pass_offset_[i] = PASS_UNUSED;
  }
}

int BufferParams::get_pass_offset(PassType pass_type, PassMode mode) const
{
  if (pass_type == PASS_NONE) {
    return PASS_UNUSED;
  }

  const int index = pass_type_mode_to_index(pass_type, mode);
  return pass_offset_[index];
}

const BufferPass *BufferParams::find_pass(string_view name) const
{
  for (const BufferPass &pass : passes) {
    if (pass.name == name) {
      return &pass;
    }
  }

  return nullptr;
}

const BufferPass *BufferParams::find_pass(PassType type, PassMode mode) const
{
  for (const BufferPass &pass : passes) {
    if (pass.type == type && pass.mode == mode) {
      return &pass;
    }
  }

  return nullptr;
}

const BufferPass *BufferParams::get_actual_display_pass(PassType type, PassMode mode) const
{
  const BufferPass *pass = find_pass(type, mode);
  return get_actual_display_pass(pass);
}

const BufferPass *BufferParams::get_actual_display_pass(const BufferPass *pass) const
{
  if (!pass) {
    return nullptr;
  }

  if (pass->type == PASS_COMBINED && pass->lightgroup.empty()) {
    const BufferPass *shadow_catcher_matte_pass = find_pass(PASS_SHADOW_CATCHER_MATTE, pass->mode);
    if (shadow_catcher_matte_pass) {
      pass = shadow_catcher_matte_pass;
    }
  }

  return pass;
}

void BufferParams::update_offset_stride()
{
  offset = -(full_x + full_y * width);
  stride = width;
}

bool BufferParams::modified(const BufferParams &other) const
{
  if (width != other.width || height != other.height) {
    return true;
  }

  if (full_x != other.full_x || full_y != other.full_y || full_width != other.full_width ||
      full_height != other.full_height)
  {
    return true;
  }

  if (window_x != other.window_x || window_y != other.window_y ||
      window_width != other.window_width || window_height != other.window_height)
  {
    return true;
  }

  if (offset != other.offset || stride != other.stride || pass_stride != other.pass_stride) {
    return true;
  }

  if (layer != other.layer || view != other.view) {
    return true;
  }

  if (exposure != other.exposure ||
      use_approximate_shadow_catcher != other.use_approximate_shadow_catcher ||
      use_transparent_background != other.use_transparent_background)
  {
    return true;
  }

  return !(passes == other.passes);
}

/* --------------------------------------------------------------------
 * Render Buffers.
 */

RenderBuffers::RenderBuffers(Device *device) : buffer(device, "RenderBuffers", MEM_READ_WRITE) {}

RenderBuffers::~RenderBuffers()
{
  buffer.free();
}

void RenderBuffers::reset(const BufferParams &params_)
{
  DCHECK(params_.pass_stride != -1);

  params = params_;

  /* re-allocate buffer */
  buffer.alloc(params.width * params.pass_stride, params.height);
}

void RenderBuffers::zero()
{
  buffer.zero_to_device();
}

bool RenderBuffers::copy_from_device()
{
  DCHECK(params.pass_stride != -1);

  if (!buffer.device_pointer)
    return false;

  buffer.copy_from_device(0, params.width * params.pass_stride, params.height);

  return true;
}

void RenderBuffers::copy_to_device()
{
  buffer.copy_to_device();
}

void render_buffers_host_copy_denoised(RenderBuffers *dst,
                                       const BufferParams &dst_params,
                                       const RenderBuffers *src,
                                       const BufferParams &src_params,
                                       const size_t src_offset)
{
  DCHECK_EQ(dst_params.width, src_params.width);
  /* TODO(sergey): More sanity checks to avoid buffer overrun. */

  /* Create a map of pass offsets to be copied.
   * Assume offsets are different to allow copying passes between buffers with different set of
   * passes. */

  struct {
    int dst_offset;
    int src_offset;
  } pass_offsets[PASS_NUM];

  int num_passes = 0;

  for (int i = 0; i < PASS_NUM; ++i) {
    const PassType pass_type = static_cast<PassType>(i);

    const int dst_pass_offset = dst_params.get_pass_offset(pass_type, PassMode::DENOISED);
    if (dst_pass_offset == PASS_UNUSED) {
      continue;
    }

    const int src_pass_offset = src_params.get_pass_offset(pass_type, PassMode::DENOISED);
    if (src_pass_offset == PASS_UNUSED) {
      continue;
    }

    pass_offsets[num_passes].dst_offset = dst_pass_offset;
    pass_offsets[num_passes].src_offset = src_pass_offset;
    ++num_passes;
  }

  /* Copy passes. */
  /* TODO(sergey): Make it more reusable, allowing implement copy of noisy passes. */

  const int64_t dst_width = dst_params.width;
  const int64_t dst_height = dst_params.height;
  const int64_t dst_pass_stride = dst_params.pass_stride;
  const int64_t dst_num_pixels = dst_width * dst_height;

  const int64_t src_pass_stride = src_params.pass_stride;
  const int64_t src_offset_in_floats = src_offset * src_pass_stride;

  const float *src_pixel = src->buffer.data() + src_offset_in_floats;
  float *dst_pixel = dst->buffer.data();

  for (int i = 0; i < dst_num_pixels;
       ++i, src_pixel += src_pass_stride, dst_pixel += dst_pass_stride)
  {
    for (int pass_offset_idx = 0; pass_offset_idx < num_passes; ++pass_offset_idx) {
      const int dst_pass_offset = pass_offsets[pass_offset_idx].dst_offset;
      const int src_pass_offset = pass_offsets[pass_offset_idx].src_offset;

      /* TODO(sergey): Support non-RGBA passes. */
      dst_pixel[dst_pass_offset + 0] = src_pixel[src_pass_offset + 0];
      dst_pixel[dst_pass_offset + 1] = src_pixel[src_pass_offset + 1];
      dst_pixel[dst_pass_offset + 2] = src_pixel[src_pass_offset + 2];
      dst_pixel[dst_pass_offset + 3] = src_pixel[src_pass_offset + 3];
    }
  }
}

CCL_NAMESPACE_END

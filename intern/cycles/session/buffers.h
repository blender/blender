/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#include "device/memory.h"
#include "graph/node.h"
#include "scene/pass.h"

#include "kernel/types.h"

#include "util/half.h"
#include "util/string.h"
#include "util/thread.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

class Device;
struct DeviceDrawParams;
struct float4;

/* NOTE: Is not a real scene node. Using Node API for ease of (de)serialization. */
class BufferPass : public Node {
 public:
  NODE_DECLARE

  PassType type = PASS_NONE;
  PassMode mode = PassMode::NOISY;
  ustring name;
  bool include_albedo = false;

  int offset = -1;

  BufferPass();
  explicit BufferPass(const Pass *scene_pass);

  BufferPass(BufferPass &&other) noexcept = default;
  BufferPass(const BufferPass &other) = default;

  BufferPass &operator=(BufferPass &&other) = default;
  BufferPass &operator=(const BufferPass &other) = default;

  ~BufferPass() = default;

  PassInfo get_info() const;

  inline bool operator==(const BufferPass &other) const
  {
    return type == other.type && mode == other.mode && name == other.name &&
           include_albedo == other.include_albedo && offset == other.offset;
  }
  inline bool operator!=(const BufferPass &other) const
  {
    return !(*this == other);
  }
};

/* Buffer Parameters
 * Size of render buffer and how it fits in the full image (border render). */

/* NOTE: Is not a real scene node. Using Node API for ease of (de)serialization. */
class BufferParams : public Node {
 public:
  NODE_DECLARE

  /* Width/height of the physical buffer. */
  int width = 0;
  int height = 0;

  /* Windows defines which part of the buffers is visible. The part outside of the window is
   * considered an "overscan".
   *
   * Window X and Y are relative to the position of the buffer in the full buffer. */
  int window_x = 0;
  int window_y = 0;
  int window_width = 0;
  int window_height = 0;

  /* Offset into and width/height of the full buffer. */
  int full_x = 0;
  int full_y = 0;
  int full_width = 0;
  int full_height = 0;

  /* Runtime fields, only valid after `update_passes()` or `update_offset_stride()`. */
  int offset = -1, stride = -1;

  /* Runtime fields, only valid after `update_passes()`. */
  int pass_stride = -1;

  /* Properties which are used for accessing buffer pixels outside of scene graph. */
  vector<BufferPass> passes;
  ustring layer;
  ustring view;
  int samples = 0;
  float exposure = 1.0f;
  bool use_approximate_shadow_catcher = false;
  bool use_transparent_background = false;

  BufferParams();

  BufferParams(BufferParams &&other) noexcept = default;
  BufferParams(const BufferParams &other) = default;

  BufferParams &operator=(BufferParams &&other) = default;
  BufferParams &operator=(const BufferParams &other) = default;

  ~BufferParams() = default;

  /* Pre-calculate all fields which depends on the passes.
   *
   * When the scene passes are given, the buffer passes will be created from them and stored in
   * this params, and then params are updated for those passes.
   * The `update_passes()` without parameters updates offsets and strides which are stored outside
   * of the passes. */
  void update_passes();
  void update_passes(const vector<Pass *> &scene_passes);

  /* Returns PASS_UNUSED if there is no such pass in the buffer. */
  int get_pass_offset(PassType type, PassMode mode = PassMode::NOISY) const;

  /* Returns nullptr if pass with given name does not exist. */
  const BufferPass *find_pass(string_view name) const;
  const BufferPass *find_pass(PassType type, PassMode mode = PassMode::NOISY) const;

  /* Get display pass from its name.
   * Will do special logic to replace combined pass with shadow catcher matte. */
  const BufferPass *get_actual_display_pass(PassType type, PassMode mode = PassMode::NOISY) const;
  const BufferPass *get_actual_display_pass(const BufferPass *pass) const;

  void update_offset_stride();

  bool modified(const BufferParams &other) const;

 protected:
  void reset_pass_offset();

  /* Multiplied by 2 to be able to store noisy and denoised pass types. */
  static constexpr int kNumPassOffsets = PASS_NUM * 2;

  /* Indexed by an index derived from pass type and mode, indicates offset of the corresponding
   * pass in the buffer.
   * If there are multiple passes with same type and mode contains lowest offset of all of them. */
  int pass_offset_[kNumPassOffsets];
};

/* Render Buffers */

class RenderBuffers {
 public:
  /* buffer parameters */
  BufferParams params;

  /* float buffer */
  device_vector<float> buffer;

  explicit RenderBuffers(Device *device);
  ~RenderBuffers();

  void reset(const BufferParams &params);
  void zero();

  bool copy_from_device();
  void copy_to_device();
};

/* Copy denoised passes form source to destination.
 *
 * Buffer parameters are provided explicitly, allowing to copy pixels between render buffers which
 * content corresponds to a render result at a non-unit resolution divider.
 *
 * `src_offset` allows to offset source pixel index which is used when a fraction of the source
 * buffer is to be copied.
 *
 * Copy happens of the number of pixels in the destination. */
void render_buffers_host_copy_denoised(RenderBuffers *dst,
                                       const BufferParams &dst_params,
                                       const RenderBuffers *src,
                                       const BufferParams &src_params,
                                       const size_t src_offset = 0);

CCL_NAMESPACE_END

#endif /* __BUFFERS_H__ */

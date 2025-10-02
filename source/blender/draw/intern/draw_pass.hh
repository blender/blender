/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup draw
 *
 * Passes record draw commands. Commands are executed only when a pass is submitted for execution.
 *
 * `PassMain`:
 * Should be used on heavy load passes such as ones that may contain scene objects. Draw call
 * submission is optimized for large number of draw calls. But has a significant overhead per
 * #Pass. Use many #PassSub along with a main #Pass to reduce the overhead and allow groupings of
 * commands. \note The draw call order inside a batch of multiple draw with the exact same state is
 * not guaranteed and is not even deterministic. Use a #PassSimple or #PassSortable if ordering is
 * needed. Custom vertex count and custom first vertex will effectively disable batching.
 *
 * `PassSimple`:
 * Does not have the overhead of #PassMain but does not have the culling and batching optimization.
 * It should be used for passes that needs a few commands or that needs guaranteed draw call order.
 *
 * `Pass<T>::Sub`:
 * A lightweight #Pass that lives inside a main #Pass. It can only be created from #Pass.sub()
 * and is auto managed. This mean it can be created, filled and thrown away. A #PassSub reference
 * is valid until the next #Pass.init() of the parent pass. Commands recorded inside a #PassSub are
 * inserted inside the parent #Pass where the sub have been created during submission.
 *
 * `PassSortable`:
 * This is a sort of `PassMain` augmented with a per sub-pass sorting value. They can't directly
 * contain draw command, everything needs to be inside sub-passes. Sub-passes are automatically
 * sorted before submission.
 *
 * \note A pass can be recorded once and resubmitted any number of time. This can be a good
 * optimization for passes that are always the same for each frame. The only thing to be aware of
 * is the life time of external resources. If a pass contains draw-calls with non default
 * #ResourceIndex (not 0) or a reference to any non static resources
 * (#gpu::Batch, #PushConstant ref, #ResourceBind ref) it will have to be re-recorded
 * if any of these reference becomes invalid.
 */

#include "BLI_assert.h"
#include "BLI_listbase_wrapper.hh"
#include "BLI_vector.hh"

#include "BKE_image.hh"

#include "GPU_batch.hh"
#include "GPU_debug.hh"
#include "GPU_index_buffer.hh"
#include "GPU_material.hh"
#include "GPU_pass.hh"

#include "DRW_gpu_wrapper.hh"

#include "draw_command.hh"
#include "draw_handle.hh"
#include "draw_manager.hh"
#include "draw_shader_shared.hh"
#include "draw_state.hh"

#include <cstdint>
#include <sstream>

namespace blender::draw {
using namespace blender::draw;
using namespace blender::draw::command;

using DispatchIndirectBuf = draw::StorageBuffer<DispatchCommand>;
using DrawIndirectBuf = draw::StorageBuffer<DrawCommand, true>;

class Manager;

namespace command {
class DrawCommandBuf;
}

/* -------------------------------------------------------------------- */
/** \name Pass API
 * \{ */

namespace detail {

/**
 * Special container that never moves allocated items and has fast indexing.
 */
template<typename T,
         /** Numbers of element of type T to allocate together. */
         int64_t block_size = 16>
class SubPassVector {
 private:
  Vector<std::unique_ptr<Vector<T, block_size>>, 0> blocks_;

 public:
  void clear()
  {
    blocks_.clear();
  }

  int64_t append_and_get_index(T &&elem)
  {
    /* Do not go over the inline size so that existing members never move. */
    if (blocks_.is_empty() || blocks_.last()->size() == block_size) {
      blocks_.append(std::make_unique<Vector<T, block_size>>());
    }
    return blocks_.last()->append_and_get_index(std::move(elem)) +
           (blocks_.size() - 1) * block_size;
  }

  T &operator[](int64_t index)
  {
    return (*blocks_[index / block_size])[index % block_size];
  }

  const T &operator[](int64_t index) const
  {
    return (*blocks_[index / block_size])[index % block_size];
  }
};

/**
 * Public API of a draw pass.
 */
template<
    /** Type of command buffer used to create the draw calls. */
    typename DrawCommandBufType>
class PassBase {
  friend Manager;
  friend DrawCommandBuf;

  /** Will use texture own internal sampler state. */
  static constexpr GPUSamplerState sampler_auto = GPUSamplerState::internal_sampler();

 protected:
  /** Highest level of the command stream. Split command stream in different command types. */
  Vector<command::Header, 0> headers_;
  /** Commands referenced by headers (which contains their types). */
  Vector<command::Undetermined, 0> commands_;
  /** Reference to draw commands buffer. Either own or from parent pass. */
  DrawCommandBufType &draw_commands_buf_;
  /** Reference to sub-pass commands buffer. Either own or from parent pass. */
  SubPassVector<PassBase<DrawCommandBufType>> &sub_passes_;
  /** Currently bound shader. Used for interface queries. */
  gpu::Shader *shader_;

  uint64_t manager_fingerprint_ = 0;
  uint64_t view_fingerprint_ = 0;

  bool is_empty_ = true;

 public:
  const char *debug_name;

  bool use_custom_ids;

  PassBase(const char *name,
           DrawCommandBufType &draw_command_buf,
           SubPassVector<PassBase<DrawCommandBufType>> &sub_passes,
           gpu::Shader *shader = nullptr)
      : draw_commands_buf_(draw_command_buf),
        sub_passes_(sub_passes),
        shader_(shader),
        debug_name(name),
        use_custom_ids(false) {};

  /**
   * Reset the pass command pool.
   * \note Implemented in derived class. Not a virtual function to avoid indirection. Here only for
   * API readability listing.
   */
  void init();

  /**
   * Returns true if the pass and its sub-passes don't contain any draw or dispatch command.
   */
  bool is_empty() const;

  /**
   * Create a sub-pass inside this pass.
   */
  PassBase<DrawCommandBufType> &sub(const char *name);

  /**
   * Changes the fixed function pipeline state.
   * Starts as DRW_STATE_NO_DRAW at the start of a Pass submission.
   * SubPass inherit previous pass state.
   *
   * IMPORTANT: This does not set the stencil mask/reference values. Add a call to state_stencil()
   * to ensure correct behavior of stencil aware draws.
   *
   * TODO(fclem): clip_plane_count should be part of shader state.
   */
  void state_set(DRWState state, int clip_plane_count = 0);

  /**
   * Clear the current frame-buffer.
   */
  void clear_color(float4 color);
  void clear_depth(float depth);
  void clear_stencil(uint8_t stencil);
  void clear_depth_stencil(float depth, uint8_t stencil);
  void clear_color_depth_stencil(float4 color, float depth, uint8_t stencil);
  /**
   * Clear each color attachment with different values. Span needs to be appropriately sized.
   * IMPORTANT: The source is dereference on pass submission.
   */
  void clear_multi(Span<float4> colors);

  /**
   * Reminders:
   * - `compare_mask & reference` is what is tested against `compare_mask & stencil_value`
   *   stencil_value being the value stored in the stencil buffer.
   * - `write-mask & reference` is what gets written if the test condition is fulfilled.
   *
   * This will modify the stencil state until another call to this function.
   * If not specified before any draw-call, these states will be undefined.
   *
   * For more information see:
   * https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkStencilOpState.html
   */
  void state_stencil(uint8_t write_mask, uint8_t reference, uint8_t compare_mask);

  /**
   * Bind a shader. Any following bind() or push_constant() call will use its interface.
   */
  void shader_set(gpu::Shader *shader);

  /**
   * Bind a framebuffer. This is equivalent to a deferred GPU_framebuffer_bind() call.
   * \note Changes the global GPU state (outside of DRW).
   * \note Capture reference to the framebuffer so it can be initialized later.
   */
  void framebuffer_set(gpu::FrameBuffer **framebuffer);

  /**
   * Start a new sub-pass and change framebuffer attachments status.
   * \note Affect the currently bound framebuffer at the time of submission and execution.
   * \note States are copied and stored in the command.
   */
  void subpass_transition(GPUAttachmentState depth_attachment,
                          Span<GPUAttachmentState> color_attachments);

  /**
   * Bind a material shader along with its associated resources. Any following bind() or
   * push_constant() call will use its interface.
   * IMPORTANT: Assumes material is compiled and can be used (no compilation error).
   */
  void material_set(Manager &manager,
                    GPUMaterial *material,
                    bool deferred_texture_loading = false);

  /**
   * Record a draw call.
   * \note Setting the count or first to -1 will use the values from the batch.
   * \note An instance or vertex count of 0 will discard the draw call. It will not be recorded.
   */
  void draw(gpu::Batch *batch,
            uint instance_len = -1,
            uint vertex_len = -1,
            uint vertex_first = -1,
            ResourceIndexRange res_index = {},
            uint custom_id = 0);

  /**
   * Shorter version for the common case.
   * \note Implemented in derived class. Not a virtual function to avoid indirection.
   */
  void draw(gpu::Batch *batch, ResourceIndexRange res_index, uint custom_id = 0);

  /**
   * Record a procedural draw call. Geometry is **NOT** source from a gpu::Batch.
   * \note An instance or vertex count of 0 will discard the draw call. It will not be recorded.
   */
  void draw_procedural(GPUPrimType primitive,
                       uint instance_len,
                       uint vertex_len,
                       uint vertex_first = -1,
                       ResourceIndexRange res_index = {},
                       uint custom_id = 0);

  /**
   * Record a regular draw call but replaces each original primitive by a set of the given
   * primitive. Geometry attributes are still sourced from a `gpu::Batch`, however, the attributes
   * indexing needs to be done manually inside the shader.
   *
   * \note primitive_type and primitive_len must be baked into the shader and without using
   * specialization constant!
   *
   * \note A primitive_len count of 0 will discard the draw call. It will not be recorded.
   * \note vertex_len and vertex_first are relative to the original primitive list.
   * \note Only works for GPU_PRIM_POINTS, GPU_PRIM_LINES, GPU_PRIM_TRIS, GPU_PRIM_LINES_ADJ and
   * GPU_PRIM_TRIS_ADJ original primitive types.
   */
  void draw_expand(gpu::Batch *batch,
                   GPUPrimType primitive_type,
                   uint primitive_len,
                   uint instance_len,
                   uint vertex_len,
                   uint vertex_first,
                   ResourceIndexRange res_index = {},
                   uint custom_id = 0);

  /**
   * Shorter version for the common case.
   * \note Implemented in derived class. Not a virtual function to avoid indirection.
   */
  void draw_expand(gpu::Batch *batch,
                   GPUPrimType primitive_type,
                   uint primitive_len,
                   uint instance_len,
                   ResourceIndexRange res_index = {},
                   uint custom_id = 0);

  /**
   * Indirect variants.
   * \note If needed, the resource id need to also be set accordingly in the DrawCommand.
   */
  void draw_indirect(gpu::Batch *batch,
                     StorageBuffer<DrawCommand, true> &indirect_buffer,
                     ResourceIndex res_index = {0});
  void draw_procedural_indirect(GPUPrimType primitive,
                                StorageBuffer<DrawCommand, true> &indirect_buffer,
                                ResourceIndex res_index = {0});

  /**
   * Record a compute dispatch call.
   */
  void dispatch(int group_len);
  void dispatch(int2 group_len);
  void dispatch(int3 group_len);
  void dispatch(int3 *group_len);
  void dispatch(StorageBuffer<DispatchCommand> &indirect_buffer);

  /**
   * Record a barrier call to synchronize arbitrary load/store operation between draw calls.
   */
  void barrier(GPUBarrier type);

  /**
   * Bind a shader resource.
   *
   * Reference versions are to be used when the resource might be resize / realloc or even change
   * between the time it is referenced and the time it is dereferenced for drawing.
   *
   * IMPORTANT: Will keep a reference to the data and dereference it upon drawing. Make sure data
   * still alive until pass submission.
   *
   * \note Variations using slot will not query a shader interface and can be used before
   * binding a shader.
   */
  void bind_image(const char *name, gpu::Texture *image);
  void bind_image(const char *name, gpu::Texture **image);
  void bind_image(int slot, gpu::Texture *image);
  void bind_image(int slot, gpu::Texture **image);
  void bind_texture(const char *name, gpu::Texture *texture, GPUSamplerState state = sampler_auto);
  void bind_texture(const char *name,
                    gpu::Texture **texture,
                    GPUSamplerState state = sampler_auto);
  void bind_texture(const char *name, gpu::VertBuf *buffer);
  void bind_texture(const char *name, gpu::VertBuf **buffer);
  void bind_texture(const char *name, gpu::VertBufPtr &buffer);
  void bind_texture(int slot, gpu::Texture *texture, GPUSamplerState state = sampler_auto);
  void bind_texture(int slot, gpu::Texture **texture, GPUSamplerState state = sampler_auto);
  void bind_texture(int slot, gpu::VertBuf *buffer);
  void bind_texture(int slot, gpu::VertBuf **buffer);
  void bind_texture(int slot, gpu::VertBufPtr &buffer);
  void bind_ssbo(const char *name, gpu::StorageBuf *buffer);
  void bind_ssbo(const char *name, gpu::StorageBuf **buffer);
  void bind_ssbo(int slot, gpu::StorageBuf *buffer);
  void bind_ssbo(int slot, gpu::StorageBuf **buffer);
  void bind_ssbo(const char *name, gpu::UniformBuf *buffer);
  void bind_ssbo(const char *name, gpu::UniformBuf **buffer);
  void bind_ssbo(int slot, gpu::UniformBuf *buffer);
  void bind_ssbo(int slot, gpu::UniformBuf **buffer);
  void bind_ssbo(const char *name, gpu::VertBuf *buffer);
  void bind_ssbo(const char *name, gpu::VertBuf **buffer);
  void bind_ssbo(const char *name, gpu::VertBufPtr &buffer);
  void bind_ssbo(int slot, gpu::VertBuf *buffer);
  void bind_ssbo(int slot, gpu::VertBuf **buffer);
  void bind_ssbo(int slot, gpu::VertBufPtr &buffer);
  void bind_ssbo(const char *name, gpu::IndexBuf *buffer);
  void bind_ssbo(const char *name, gpu::IndexBuf **buffer);
  void bind_ssbo(int slot, gpu::IndexBuf *buffer);
  void bind_ssbo(int slot, gpu::IndexBuf **buffer);
  void bind_ubo(const char *name, gpu::UniformBuf *buffer);
  void bind_ubo(const char *name, gpu::UniformBuf **buffer);
  void bind_ubo(int slot, gpu::UniformBuf *buffer);
  void bind_ubo(int slot, gpu::UniformBuf **buffer);

  /**
   * Update a shader constant.
   *
   * Reference versions are to be used when the resource might change between the time it is
   * referenced and the time it is dereferenced for drawing.
   *
   * IMPORTANT: Will keep a reference to the data and dereference it upon drawing. Make sure data
   * still alive until pass submission.
   *
   * \note bool reference version is expected to take bool32_t reference which is aliased to int.
   */
  void push_constant(const char *name, const float &data);
  void push_constant(const char *name, const float2 &data);
  void push_constant(const char *name, const float3 &data);
  void push_constant(const char *name, const float4 &data);
  void push_constant(const char *name, const int &data);
  void push_constant(const char *name, const int2 &data);
  void push_constant(const char *name, const int3 &data);
  void push_constant(const char *name, const int4 &data);
  void push_constant(const char *name, const bool &data);
  void push_constant(const char *name, const float4x4 &data);
  void push_constant(const char *name, const float *data, int array_len = 1);
  void push_constant(const char *name, const float2 *data, int array_len = 1);
  void push_constant(const char *name, const float3 *data, int array_len = 1);
  void push_constant(const char *name, const float4 *data, int array_len = 1);
  void push_constant(const char *name, const int *data, int array_len = 1);
  void push_constant(const char *name, const int2 *data, int array_len = 1);
  void push_constant(const char *name, const int3 *data, int array_len = 1);
  void push_constant(const char *name, const int4 *data, int array_len = 1);
  void push_constant(const char *name, const float4x4 *data);

  /**
   * Update a shader specialization constant.
   *
   * IMPORTANT: Non-specialized constants can have undefined values.
   * Specialize every constant before binding a shader.
   *
   * Reference versions are to be used when the resource might change between the time it is
   * referenced and the time it is dereferenced for drawing.
   *
   * IMPORTANT: Will keep a reference to the data and dereference it upon drawing. Make sure data
   * still alive until pass submission.
   */
  void specialize_constant(gpu::Shader *shader, const char *name, const float &data);
  void specialize_constant(gpu::Shader *shader, const char *name, const int &data);
  void specialize_constant(gpu::Shader *shader, const char *name, const uint &data);
  void specialize_constant(gpu::Shader *shader, const char *name, const bool &data);
  void specialize_constant(gpu::Shader *shader, const char *name, const float *data);
  void specialize_constant(gpu::Shader *shader, const char *name, const int *data);
  void specialize_constant(gpu::Shader *shader, const char *name, const uint *data);
  void specialize_constant(gpu::Shader *shader, const char *name, const bool *data);

  /**
   * Custom resource binding.
   * Syntactic sugar to avoid calling `resources.bind_resources(pass)` which is semantically less
   * pleasing.
   * `U` type must have a `bind_resources<Pass<T> &pass>()` method.
   */
  template<class U> void bind_resources(U &resources)
  {
    resources.bind_resources(*this);
  }

  /**
   * Turn the pass into a string for inspection.
   */
  std::string serialize(std::string line_prefix = "") const;

  friend std::ostream &operator<<(std::ostream &stream, const PassBase &pass)
  {
    return stream << pass.serialize();
  }

 protected:
  /**
   * Internal Helpers
   */

  int push_constant_offset(const char *name);

  void clear(GPUFrameBufferBits planes, float4 color, float depth, uint8_t stencil);

  gpu::Batch *procedural_batch_get(GPUPrimType primitive);

  /**
   * Return a new command recorded with the given type.
   */
  command::Undetermined &create_command(command::Type type);

  /**
   * Make sure the shader specialization constants are already compiled.
   * This avoid stalling the real submission call because of specialization.
   */
  void warm_shader_specialization(command::RecordingState &state) const;

  void submit(command::RecordingState &state) const;

  bool has_generated_commands() const
  {
    /* NOTE: Even though manager fingerprint is not enough to check for update, it is still
     * guaranteed to not be 0. So we can check weather or not this pass has generated commands
     * after sync. Asserts will catch invalid usage . */
    return manager_fingerprint_ != 0;
  }
};

template<typename DrawCommandBufType> class Pass : public detail::PassBase<DrawCommandBufType> {
 public:
  using Sub = detail::PassBase<DrawCommandBufType>;

 private:
  /** Sub-passes referenced by headers. */
  SubPassVector<detail::PassBase<DrawCommandBufType>> sub_passes_main_;
  /** Draws are recorded as indirect draws for compatibility with the multi-draw pipeline. */
  DrawCommandBufType draw_commands_buf_main_;

 public:
  Pass(const char *name)
      : detail::PassBase<DrawCommandBufType>(name, draw_commands_buf_main_, sub_passes_main_) {};

  void init()
  {
    this->manager_fingerprint_ = 0;
    this->view_fingerprint_ = 0;
    this->headers_.clear();
    this->commands_.clear();
    this->sub_passes_.clear();
    this->draw_commands_buf_.clear();
    this->is_empty_ = true;
  }
};  // namespace blender::draw

}  // namespace detail

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pass types
 * \{ */

/**
 * Normal pass type. No visibility or draw-call optimization.
 */
// using PassSimple = detail::Pass<DrawCommandBuf>;

/**
 * Main pass type.
 * Optimized for many draw calls and sub-pass.
 *
 * IMPORTANT: To be used only for passes containing lots of draw calls since it has a potentially
 * high overhead due to batching and culling optimizations.
 */
// using PassMain = detail::Pass<DrawMultiBuf>;

/**
 * Special pass type for rendering transparent objects.
 * The base level can only be composed of sub passes that will be ordered by a sorting value.
 */
class PassSortable : public PassMain {
  friend Manager;

 private:
  /** Sorting value associated with each sub pass. */
  Vector<float> sorting_values_;

  bool sorted_ = false;

 public:
  PassSortable(const char *name_) : PassMain(name_) {};

  void init()
  {
    sorting_values_.clear();
    sorted_ = false;
    PassMain::init();
  }

  PassMain::Sub &sub(const char *name, float sorting_value)
  {
    int64_t index = sub_passes_.append_and_get_index(
        PassBase(name, draw_commands_buf_, sub_passes_, shader_));
    headers_.append({Type::SubPass, uint(index)});
    /* Some sub-pass can also create sub-sub-passes (curve, point-clouds...) which will de-sync
     * the `sub_passes_.size()` and `sorting_values_.size()`, making the  `Header::index` not
     * reusable for the sorting value in the `sort()` function. To fix this, we flood the
     * `sorting_values_` to ensure the same index is valid for `sorting_values_` and
     * `sub_passes_`. */
    int64_t sorting_index;
    do {
      sorting_index = sorting_values_.append_and_get_index(sorting_value);
    } while (sorting_index != index);
    return sub_passes_[index];
  }

  std::string serialize(std::string line_prefix = "") const
  {
    if (sorted_ == false) {
      const_cast<PassSortable *>(this)->sort();
    }
    return PassMain::serialize(line_prefix);
  }

 protected:
  void sort()
  {
    if (sorted_ == false) {
      std::sort(headers_.begin(), headers_.end(), [&](Header &a, Header &b) {
        BLI_assert(a.type == Type::SubPass && b.type == Type::SubPass);
        float a_val = sorting_values_[a.index];
        float b_val = sorting_values_[b.index];
        return a_val < b_val || (a_val == b_val && a.index < b.index);
      });
      sorted_ = true;
    }
  }
};

/** \} */

namespace detail {

/* -------------------------------------------------------------------- */
/** \name PassBase Implementation
 * \{ */

template<class T> inline bool PassBase<T>::is_empty() const
{
  if (!is_empty_) {
    return false;
  }

  for (const command::Header &header : headers_) {
    if (header.type != Type::SubPass) {
      continue;
    }
    if (!sub_passes_[header.index].is_empty()) {
      return false;
    }
  }

  return true;
}

template<class T> inline command::Undetermined &PassBase<T>::create_command(command::Type type)
{
  /* After render commands have been generated, the pass is read only.
   * Call `init()` to be able modify it again. */
  BLI_assert_msg(this->has_generated_commands() == false, "Command added after submission");
  int64_t index = commands_.append_and_get_index({});
  headers_.append({type, uint(index)});

  if (ELEM(type,
           Type::Barrier,
           Type::Clear,
           Type::ClearMulti,
           Type::Dispatch,
           Type::DispatchIndirect,
           Type::Draw,
           Type::DrawIndirect))
  {
    is_empty_ = false;
  }

  return commands_[index];
}

template<class T>
inline void PassBase<T>::clear(GPUFrameBufferBits planes,
                               float4 color,
                               float depth,
                               uint8_t stencil)
{
  create_command(command::Type::Clear).clear = {uint8_t(planes), stencil, depth, color};
}

template<class T> inline void PassBase<T>::clear_multi(Span<float4> colors)
{
  create_command(command::Type::ClearMulti).clear_multi = {colors.data(),
                                                           static_cast<int>(colors.size())};
}

template<class T> inline gpu::Batch *PassBase<T>::procedural_batch_get(GPUPrimType primitive)
{
  switch (primitive) {
    case GPU_PRIM_POINTS:
      return GPU_batch_procedural_points_get();
    case GPU_PRIM_LINES:
      return GPU_batch_procedural_lines_get();
    case GPU_PRIM_TRIS:
      return GPU_batch_procedural_triangles_get();
    case GPU_PRIM_TRI_STRIP:
      return GPU_batch_procedural_triangle_strips_get();
    default:
      /* Add new one as needed. */
      BLI_assert_unreachable();
      return nullptr;
  }
}

template<class T> inline PassBase<T> &PassBase<T>::sub(const char *name)
{
  int64_t index = sub_passes_.append_and_get_index(
      PassBase(name, draw_commands_buf_, sub_passes_, shader_));
  headers_.append({command::Type::SubPass, uint(index)});
  return sub_passes_[index];
}

template<class T>
void PassBase<T>::warm_shader_specialization(command::RecordingState &state) const
{
  GPU_debug_group_begin("warm_shader_specialization");
  GPU_debug_group_begin(this->debug_name);

  for (const command::Header &header : headers_) {
    switch (header.type) {
      default:
      case Type::None:
        break;
      case Type::SubPass:
        sub_passes_[header.index].warm_shader_specialization(state);
        break;
      case command::Type::FramebufferBind:
        break;
      case command::Type::SubPassTransition:
        break;
      case command::Type::ShaderBind:
        commands_[header.index].shader_bind.execute(state);
        break;
      case command::Type::ResourceBind:
        break;
      case command::Type::PushConstant:
        break;
      case command::Type::SpecializeConstant:
        commands_[header.index].specialize_constant.execute(state);
        break;
      case command::Type::Draw:
        break;
      case command::Type::DrawMulti:
        break;
      case command::Type::DrawIndirect:
        break;
      case command::Type::Dispatch:
        break;
      case command::Type::DispatchIndirect:
        break;
      case command::Type::Barrier:
        break;
      case command::Type::Clear:
        break;
      case command::Type::ClearMulti:
        break;
      case command::Type::StateSet:
        break;
      case command::Type::StencilSet:
        break;
    }
  }

  GPU_debug_group_end();
  GPU_debug_group_end();
}

template<class T> void PassBase<T>::submit(command::RecordingState &state) const
{
  if (headers_.is_empty()) {
    return;
  }

  GPU_debug_group_begin(debug_name);

  for (const command::Header &header : headers_) {
    switch (header.type) {
      default:
      case Type::None:
        break;
      case Type::SubPass:
        sub_passes_[header.index].submit(state);
        break;
      case command::Type::FramebufferBind:
        commands_[header.index].framebuffer_bind.execute();
        break;
      case command::Type::SubPassTransition:
        commands_[header.index].subpass_transition.execute();
        break;
      case command::Type::ShaderBind:
        commands_[header.index].shader_bind.execute(state);
        break;
      case command::Type::ResourceBind:
        commands_[header.index].resource_bind.execute();
        break;
      case command::Type::PushConstant:
        commands_[header.index].push_constant.execute(state);
        break;
      case command::Type::SpecializeConstant:
        commands_[header.index].specialize_constant.execute(state);
        break;
      case command::Type::Draw:
        commands_[header.index].draw.execute(state);
        break;
      case command::Type::DrawMulti:
        commands_[header.index].draw_multi.execute(state);
        break;
      case command::Type::DrawIndirect:
        commands_[header.index].draw_indirect.execute(state);
        break;
      case command::Type::Dispatch:
        commands_[header.index].dispatch.execute(state);
        break;
      case command::Type::DispatchIndirect:
        commands_[header.index].dispatch_indirect.execute(state);
        break;
      case command::Type::Barrier:
        commands_[header.index].barrier.execute();
        break;
      case command::Type::Clear:
        commands_[header.index].clear.execute();
        break;
      case command::Type::ClearMulti:
        commands_[header.index].clear_multi.execute();
        break;
      case command::Type::StateSet:
        commands_[header.index].state_set.execute(state);
        break;
      case command::Type::StencilSet:
        commands_[header.index].stencil_set.execute();
        break;
    }
  }

  GPU_debug_group_end();
}

template<class T> std::string PassBase<T>::serialize(std::string line_prefix) const
{
  std::stringstream ss;
  ss << line_prefix << "." << debug_name << std::endl;
  line_prefix += "  ";
  for (const command::Header &header : headers_) {
    switch (header.type) {
      default:
      case Type::None:
        break;
      case Type::SubPass:
        ss << sub_passes_[header.index].serialize(line_prefix);
        break;
      case Type::FramebufferBind:
        ss << line_prefix << commands_[header.index].framebuffer_bind.serialize() << std::endl;
        break;
      case Type::SubPassTransition:
        ss << line_prefix << commands_[header.index].subpass_transition.serialize() << std::endl;
        break;
      case Type::ShaderBind:
        ss << line_prefix << commands_[header.index].shader_bind.serialize() << std::endl;
        break;
      case Type::ResourceBind:
        ss << line_prefix << commands_[header.index].resource_bind.serialize() << std::endl;
        break;
      case Type::PushConstant:
        ss << line_prefix << commands_[header.index].push_constant.serialize() << std::endl;
        break;
      case Type::Draw:
        ss << line_prefix << commands_[header.index].draw.serialize() << std::endl;
        break;
      case Type::DrawMulti:
        ss << commands_[header.index].draw_multi.serialize(line_prefix);
        break;
      case Type::DrawIndirect:
        ss << line_prefix << commands_[header.index].draw_indirect.serialize() << std::endl;
        break;
      case Type::Dispatch:
        ss << line_prefix << commands_[header.index].dispatch.serialize() << std::endl;
        break;
      case Type::DispatchIndirect:
        ss << line_prefix << commands_[header.index].dispatch_indirect.serialize() << std::endl;
        break;
      case Type::Barrier:
        ss << line_prefix << commands_[header.index].barrier.serialize() << std::endl;
        break;
      case Type::Clear:
        ss << line_prefix << commands_[header.index].clear.serialize() << std::endl;
        break;
      case Type::ClearMulti:
        ss << line_prefix << commands_[header.index].clear_multi.serialize() << std::endl;
        break;
      case Type::StateSet:
        ss << line_prefix << commands_[header.index].state_set.serialize() << std::endl;
        break;
      case Type::StencilSet:
        ss << line_prefix << commands_[header.index].stencil_set.serialize() << std::endl;
        break;
    }
  }
  return ss.str();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw calls
 * \{ */

template<class T>
inline void PassBase<T>::draw(gpu::Batch *batch,
                              uint instance_len,
                              uint vertex_len,
                              uint vertex_first,
                              ResourceIndexRange res_index,
                              uint custom_id)
{
  if (instance_len == 0 || vertex_len == 0) {
    return;
  }
  BLI_assert(batch);
  BLI_assert(shader_);
  draw_commands_buf_.append_draw(headers_,
                                 commands_,
                                 batch,
                                 instance_len,
                                 vertex_len,
                                 vertex_first,
                                 res_index,
                                 custom_id,
                                 GPU_PRIM_NONE,
                                 0);
  is_empty_ = false;
}

template<class T>
inline void PassBase<T>::draw(gpu::Batch *batch, ResourceIndexRange res_index, uint custom_id)
{
  this->draw(batch, -1, -1, -1, res_index, custom_id);
}

template<class T>
inline void PassBase<T>::draw_expand(gpu::Batch *batch,
                                     GPUPrimType primitive_type,
                                     uint primitive_len,
                                     uint instance_len,
                                     uint vertex_len,
                                     uint vertex_first,
                                     ResourceIndexRange res_index,
                                     uint custom_id)
{
  if (instance_len == 0 || vertex_len == 0 || primitive_len == 0) {
    return;
  }
  BLI_assert(shader_);
  draw_commands_buf_.append_draw(headers_,
                                 commands_,
                                 batch,
                                 instance_len,
                                 vertex_len,
                                 vertex_first,
                                 res_index,
                                 custom_id,
                                 primitive_type,
                                 primitive_len);
  is_empty_ = false;
}

template<class T>
inline void PassBase<T>::draw_expand(gpu::Batch *batch,
                                     GPUPrimType primitive_type,
                                     uint primitive_len,
                                     uint instance_len,
                                     ResourceIndexRange res_index,
                                     uint custom_id)
{
  this->draw_expand(
      batch, primitive_type, primitive_len, instance_len, -1, -1, res_index, custom_id);
}

template<class T>
inline void PassBase<T>::draw_procedural(GPUPrimType primitive,
                                         uint instance_len,
                                         uint vertex_len,
                                         uint vertex_first,
                                         ResourceIndexRange res_index,
                                         uint custom_id)
{
  this->draw(procedural_batch_get(primitive),
             instance_len,
             vertex_len,
             vertex_first,
             res_index,
             custom_id);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indirect draw calls
 * \{ */

template<class T>
inline void PassBase<T>::draw_indirect(gpu::Batch *batch,
                                       StorageBuffer<DrawCommand, true> &indirect_buffer,
                                       ResourceIndex res_index)
{
  BLI_assert(shader_);
  create_command(Type::DrawIndirect).draw_indirect = {batch, &indirect_buffer, res_index};
}

template<class T>
inline void PassBase<T>::draw_procedural_indirect(
    GPUPrimType primitive,
    StorageBuffer<DrawCommand, true> &indirect_buffer,
    ResourceIndex res_index)
{
  this->draw_indirect(procedural_batch_get(primitive), indirect_buffer, res_index);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compute Dispatch Implementation
 * \{ */

template<class T> inline void PassBase<T>::dispatch(int group_len)
{
  BLI_assert(shader_);
  create_command(Type::Dispatch).dispatch = {int3(group_len, 1, 1)};
}

template<class T> inline void PassBase<T>::dispatch(int2 group_len)
{
  BLI_assert(shader_);
  create_command(Type::Dispatch).dispatch = {int3(group_len.x, group_len.y, 1)};
}

template<class T> inline void PassBase<T>::dispatch(int3 group_len)
{
  BLI_assert(shader_);
  create_command(Type::Dispatch).dispatch = {group_len};
}

template<class T> inline void PassBase<T>::dispatch(int3 *group_len)
{
  BLI_assert(shader_);
  create_command(Type::Dispatch).dispatch = {group_len};
}

template<class T>
inline void PassBase<T>::dispatch(StorageBuffer<DispatchCommand> &indirect_buffer)
{
  BLI_assert(shader_);
  create_command(Type::DispatchIndirect).dispatch_indirect = {&indirect_buffer};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clear Implementation
 * \{ */

template<class T> inline void PassBase<T>::clear_color(float4 color)
{
  this->clear(GPU_COLOR_BIT, color, 0.0f, 0);
}

template<class T> inline void PassBase<T>::clear_depth(float depth)
{
  this->clear(GPU_DEPTH_BIT, float4(0.0f), depth, 0);
}

template<class T> inline void PassBase<T>::clear_stencil(uint8_t stencil)
{
  this->clear(GPU_STENCIL_BIT, float4(0.0f), 0.0f, stencil);
}

template<class T> inline void PassBase<T>::clear_depth_stencil(float depth, uint8_t stencil)
{
  this->clear(GPU_DEPTH_BIT | GPU_STENCIL_BIT, float4(0.0f), depth, stencil);
}

template<class T>
inline void PassBase<T>::clear_color_depth_stencil(float4 color, float depth, uint8_t stencil)
{
  this->clear(GPU_DEPTH_BIT | GPU_STENCIL_BIT | GPU_COLOR_BIT, color, depth, stencil);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Barrier Implementation
 * \{ */

template<class T> inline void PassBase<T>::barrier(GPUBarrier type)
{
  create_command(Type::Barrier).barrier = {type};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name State Implementation
 * \{ */

template<class T> inline void PassBase<T>::state_set(DRWState state, int clip_plane_count)
{
  /** \note This is for compatibility with the old clip plane API. */
  if (clip_plane_count > 0) {
    state |= DRW_STATE_CLIP_PLANES;
  }
  /* Assumed to always be enabled. */
  state |= DRW_STATE_PROGRAM_POINT_SIZE;
  create_command(Type::StateSet).state_set = {state, clip_plane_count};
}

template<class T>
inline void PassBase<T>::state_stencil(uint8_t write_mask, uint8_t reference, uint8_t compare_mask)
{
  create_command(Type::StencilSet).stencil_set = {write_mask, compare_mask, reference};
}

template<class T> inline void PassBase<T>::shader_set(gpu::Shader *shader)
{
  shader_ = shader;
  create_command(Type::ShaderBind).shader_bind = {shader};
}

template<class T> inline void PassBase<T>::framebuffer_set(gpu::FrameBuffer **framebuffer)
{
  create_command(Type::FramebufferBind).framebuffer_bind = {framebuffer};
}

template<class T>
inline void PassBase<T>::subpass_transition(GPUAttachmentState depth_attachment,
                                            Span<GPUAttachmentState> color_attachments)
{
  uint8_t color_states[8] = {GPU_ATTACHMENT_IGNORE};
  for (auto i : color_attachments.index_range()) {
    color_states[i] = uint8_t(color_attachments[i]);
  }
  create_command(Type::SubPassTransition).subpass_transition = {uint8_t(depth_attachment),
                                                                {color_states[0],
                                                                 color_states[1],
                                                                 color_states[2],
                                                                 color_states[3],
                                                                 color_states[4],
                                                                 color_states[5],
                                                                 color_states[6],
                                                                 color_states[7]}};
}

template<class T>
inline void PassBase<T>::material_set(Manager &manager,
                                      GPUMaterial *material,
                                      bool deferred_texture_loading)
{
  GPUPass *gpupass = GPU_material_get_pass(material);
  shader_set(GPU_pass_shader_get(gpupass));

  /* Bind all textures needed by the material. */
  ListBase textures = GPU_material_textures(material);
  for (GPUMaterialTexture *tex : ListBaseWrapper<GPUMaterialTexture>(textures)) {
    if (tex->ima) {
      /* Image */
      const bool use_tile_mapping = tex->tiled_mapping_name[0];
      ImageUser *iuser = tex->iuser_available ? &tex->iuser : nullptr;

      ImageGPUTextures gputex;
      if (deferred_texture_loading) {
        gputex = BKE_image_get_gpu_material_texture_try(tex->ima, iuser, use_tile_mapping);
      }
      else {
        gputex = BKE_image_get_gpu_material_texture(tex->ima, iuser, use_tile_mapping);
      }

      if (*gputex.texture == nullptr) {
        /* Texture not yet loaded. Register a reference inside the draw pass.
         * The texture will be acquired once it is created. */
        bind_texture(tex->sampler_name, gputex.texture, tex->sampler_state);
        if (gputex.tile_mapping) {
          bind_texture(tex->tiled_mapping_name, gputex.tile_mapping, tex->sampler_state);
        }
      }
      else {
        /* Texture is loaded. Acquire. */
        manager.acquire_texture(*gputex.texture);
        bind_texture(tex->sampler_name, *gputex.texture, tex->sampler_state);
        if (gputex.tile_mapping) {
          manager.acquire_texture(*gputex.tile_mapping);
          bind_texture(tex->tiled_mapping_name, *gputex.tile_mapping, tex->sampler_state);
        }
      }
    }
    else if (tex->colorband) {
      /* Color Ramp */
      bind_texture(tex->sampler_name, *tex->colorband);
    }
    else if (tex->sky) {
      /* Sky */
      bind_texture(tex->sampler_name, *tex->sky, tex->sampler_state);
    }
  }

  gpu::UniformBuf *ubo = GPU_material_uniform_buffer_get(material);
  if (ubo != nullptr) {
    bind_ubo(GPU_NODE_TREE_UBO_SLOT, ubo);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resource bind Implementation
 * \{ */

template<class T> inline int PassBase<T>::push_constant_offset(const char *name)
{
  return GPU_shader_get_uniform(shader_, name);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::StorageBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::UniformBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::UniformBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::VertBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::VertBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::VertBufPtr &buffer)
{
  BLI_assert(buffer.get() != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer.get());
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::IndexBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::IndexBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ubo(const char *name, gpu::UniformBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ubo(GPU_shader_get_ubo_binding(shader_, name), buffer);
}

template<class T>
inline void PassBase<T>::bind_texture(const char *name,
                                      gpu::Texture *texture,
                                      GPUSamplerState state)
{
  BLI_assert(texture != nullptr);
  this->bind_texture(GPU_shader_get_sampler_binding(shader_, name), texture, state);
}

template<class T> inline void PassBase<T>::bind_texture(const char *name, gpu::VertBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_texture(GPU_shader_get_sampler_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_texture(const char *name, gpu::VertBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_texture(GPU_shader_get_sampler_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_texture(const char *name, gpu::VertBufPtr &buffer)
{
  BLI_assert(buffer.get() != nullptr);
  this->bind_texture(GPU_shader_get_sampler_binding(shader_, name), buffer.get());
}

template<class T> inline void PassBase<T>::bind_image(const char *name, gpu::Texture *image)
{
  BLI_assert(image != nullptr);
  this->bind_image(GPU_shader_get_sampler_binding(shader_, name), image);
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::StorageBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::UniformBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer, ResourceBind::Type::UniformAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::UniformBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer, ResourceBind::Type::UniformAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::VertBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer, ResourceBind::Type::VertexAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::VertBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer, ResourceBind::Type::VertexAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::VertBufPtr &buffer)
{
  BLI_assert(buffer.get() != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer.get(), ResourceBind::Type::VertexAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::IndexBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer, ResourceBind::Type::IndexAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::IndexBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {
      slot, buffer, ResourceBind::Type::IndexAsStorageBuf};
}

template<class T> inline void PassBase<T>::bind_ubo(int slot, gpu::UniformBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer};
}

template<class T>
inline void PassBase<T>::bind_texture(int slot, gpu::Texture *texture, GPUSamplerState state)
{
  BLI_assert(texture != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, texture, state};
}

template<class T> inline void PassBase<T>::bind_texture(int slot, gpu::VertBuf *buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer};
}

template<class T> inline void PassBase<T>::bind_texture(int slot, gpu::VertBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer};
}

template<class T> inline void PassBase<T>::bind_texture(int slot, gpu::VertBufPtr &buffer)
{
  BLI_assert(buffer.get() != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer.get()};
}

template<class T> inline void PassBase<T>::bind_image(int slot, gpu::Texture *image)
{
  BLI_assert(image != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, as_image(image)};
}

template<class T> inline void PassBase<T>::bind_ssbo(const char *name, gpu::StorageBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ssbo(GPU_shader_get_ssbo_binding(shader_, name), buffer);
}

template<class T> inline void PassBase<T>::bind_ubo(const char *name, gpu::UniformBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  this->bind_ubo(GPU_shader_get_ubo_binding(shader_, name), buffer);
}

template<class T>
inline void PassBase<T>::bind_texture(const char *name,
                                      gpu::Texture **texture,
                                      GPUSamplerState state)
{
  BLI_assert(texture != nullptr);
  this->bind_texture(GPU_shader_get_sampler_binding(shader_, name), texture, state);
}

template<class T> inline void PassBase<T>::bind_image(const char *name, gpu::Texture **image)
{
  BLI_assert(image != nullptr);
  this->bind_image(GPU_shader_get_sampler_binding(shader_, name), image);
}

template<class T> inline void PassBase<T>::bind_ssbo(int slot, gpu::StorageBuf **buffer)
{

  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer};
}

template<class T> inline void PassBase<T>::bind_ubo(int slot, gpu::UniformBuf **buffer)
{
  BLI_assert(buffer != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, buffer};
}

template<class T>
inline void PassBase<T>::bind_texture(int slot, gpu::Texture **texture, GPUSamplerState state)
{
  BLI_assert(texture != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, texture, state};
}

template<class T> inline void PassBase<T>::bind_image(int slot, gpu::Texture **image)
{
  BLI_assert(image != nullptr);
  create_command(Type::ResourceBind).resource_bind = {slot, as_image(image)};
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Push Constant Implementation
 * \{ */

template<class T> inline void PassBase<T>::push_constant(const char *name, const float &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const float2 &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const float3 &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const float4 &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const int &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const int2 &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const int3 &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const int4 &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const bool &data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const float *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const float2 *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const float3 *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const float4 *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const int *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const int2 *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const int3 *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T>
inline void PassBase<T>::push_constant(const char *name, const int4 *data, int array_len)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data, array_len};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const float4x4 *data)
{
  create_command(Type::PushConstant).push_constant = {push_constant_offset(name), data};
}

template<class T> inline void PassBase<T>::push_constant(const char *name, const float4x4 &data)
{
  /* WORKAROUND: Push 3 consecutive commands to hold the 64 bytes of the float4x4.
   * This assumes that all commands are always stored in flat array of memory. */
  Undetermined commands[3];

  PushConstant &cmd = commands[0].push_constant;
  cmd.location = push_constant_offset(name);
  cmd.array_len = 1;
  cmd.comp_len = 16;
  cmd.type = PushConstant::Type::FloatValue;
  /* Copy overrides the next 2 commands. We append them as Type::None to not evaluate them. */
  *reinterpret_cast<float4x4 *>(&cmd.float4_value) = data;

  create_command(Type::PushConstant) = commands[0];
  create_command(Type::None) = commands[1];
  create_command(Type::None) = commands[2];
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Resource bind Implementation
 * \{ */

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const int &constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const uint &constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const float &constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const bool &constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const int *constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const uint *constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const float *constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

template<class T>
inline void PassBase<T>::specialize_constant(gpu::Shader *shader,
                                             const char *constant_name,
                                             const bool *constant_value)
{
  create_command(Type::SpecializeConstant).specialize_constant = {
      shader, GPU_shader_get_constant(shader, constant_name), constant_value};
}

/** \} */

}  // namespace detail

}  // namespace blender::draw

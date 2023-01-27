/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. */

#pragma once

/** \file
 * \ingroup draw
 *
 * Commands stored inside draw passes. Converted into GPU commands upon pass submission.
 *
 * Draw calls (primitive rendering commands) are managed by either `DrawCommandBuf` or
 * `DrawMultiBuf`. See implementation details at their definition.
 */

#include "BKE_global.h"
#include "BLI_map.hh"
#include "DRW_gpu_wrapper.hh"

#include "draw_command_shared.hh"
#include "draw_handle.hh"
#include "draw_state.h"
#include "draw_view.hh"

/* Forward declarations. */
namespace blender::draw::detail {
template<typename T, int64_t block_size> class SubPassVector;
template<typename DrawCommandBufType> class PassBase;
}  // namespace blender::draw::detail

namespace blender::draw::command {

class DrawCommandBuf;
class DrawMultiBuf;

/* -------------------------------------------------------------------- */
/** \name Recording State
 * \{ */

/**
 * Command recording state.
 * Keep track of several states and avoid redundant state changes.
 */
struct RecordingState {
  GPUShader *shader = nullptr;
  bool front_facing = true;
  bool inverted_view = false;
  DRWState pipeline_state = DRW_STATE_NO_DRAW;
  int clip_plane_count = 0;
  /** Used for gl_BaseInstance workaround. */
  GPUStorageBuf *resource_id_buf = nullptr;

  void front_facing_set(bool facing)
  {
    /* Facing is inverted if view is not in expected handedness. */
    facing = this->inverted_view == facing;
    /* Remove redundant changes. */
    if (assign_if_different(this->front_facing, facing)) {
      GPU_front_facing(!facing);
    }
  }

  void cleanup()
  {
    if (front_facing == false) {
      GPU_front_facing(false);
    }

    if (G.debug & G_DEBUG_GPU) {
      GPU_storagebuf_unbind_all();
      GPU_texture_image_unbind_all();
      GPU_texture_unbind_all();
      GPU_uniformbuf_unbind_all();
    }
  }
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Regular Commands
 * \{ */

enum class Type : uint8_t {
  /**
   * None Type commands are either uninitialized or are repurposed as data storage.
   * They are skipped during submission.
   */
  None = 0,

  /** Commands stored as Undetermined in regular command buffer. */
  Barrier,
  Clear,
  ClearMulti,
  Dispatch,
  DispatchIndirect,
  Draw,
  DrawIndirect,
  FramebufferBind,
  PushConstant,
  ResourceBind,
  ShaderBind,
  StateSet,
  StencilSet,

  /** Special commands stored in separate buffers. */
  SubPass,
  DrawMulti,
};

/**
 * The index of the group is implicit since it is known by the one who want to
 * access it. This also allows to have an indexed object to split the command
 * stream.
 */
struct Header {
  /** Command type. */
  Type type;
  /** Command index in command heap of this type. */
  uint index;
};

struct ShaderBind {
  GPUShader *shader;

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct FramebufferBind {
  GPUFrameBuffer **framebuffer;

  void execute() const;
  std::string serialize() const;
};

struct ResourceBind {
  eGPUSamplerState sampler;
  int slot;
  bool is_reference;

  enum class Type : uint8_t {
    Sampler = 0,
    BufferSampler,
    Image,
    UniformBuf,
    StorageBuf,
    UniformAsStorageBuf,
    VertexAsStorageBuf,
    IndexAsStorageBuf,
  } type;

  union {
    /** TODO: Use draw::Texture|StorageBuffer|UniformBuffer as resources as they will give more
     * debug info. */
    GPUUniformBuf *uniform_buf;
    GPUUniformBuf **uniform_buf_ref;
    GPUStorageBuf *storage_buf;
    GPUStorageBuf **storage_buf_ref;
    /** NOTE: Texture is used for both Sampler and Image binds. */
    GPUTexture *texture;
    GPUTexture **texture_ref;
    GPUVertBuf *vertex_buf;
    GPUVertBuf **vertex_buf_ref;
    GPUIndexBuf *index_buf;
    GPUIndexBuf **index_buf_ref;
  };

  ResourceBind() = default;

  ResourceBind(int slot_, GPUUniformBuf *res)
      : slot(slot_), is_reference(false), type(Type::UniformBuf), uniform_buf(res){};
  ResourceBind(int slot_, GPUUniformBuf **res)
      : slot(slot_), is_reference(true), type(Type::UniformBuf), uniform_buf_ref(res){};
  ResourceBind(int slot_, GPUStorageBuf *res)
      : slot(slot_), is_reference(false), type(Type::StorageBuf), storage_buf(res){};
  ResourceBind(int slot_, GPUStorageBuf **res)
      : slot(slot_), is_reference(true), type(Type::StorageBuf), storage_buf_ref(res){};
  ResourceBind(int slot_, GPUUniformBuf *res, Type /* type */)
      : slot(slot_), is_reference(false), type(Type::UniformAsStorageBuf), uniform_buf(res){};
  ResourceBind(int slot_, GPUUniformBuf **res, Type /* type */)
      : slot(slot_), is_reference(true), type(Type::UniformAsStorageBuf), uniform_buf_ref(res){};
  ResourceBind(int slot_, GPUVertBuf *res, Type /* type */)
      : slot(slot_), is_reference(false), type(Type::VertexAsStorageBuf), vertex_buf(res){};
  ResourceBind(int slot_, GPUVertBuf **res, Type /* type */)
      : slot(slot_), is_reference(true), type(Type::VertexAsStorageBuf), vertex_buf_ref(res){};
  ResourceBind(int slot_, GPUIndexBuf *res, Type /* type */)
      : slot(slot_), is_reference(false), type(Type::IndexAsStorageBuf), index_buf(res){};
  ResourceBind(int slot_, GPUIndexBuf **res, Type /* type */)
      : slot(slot_), is_reference(true), type(Type::IndexAsStorageBuf), index_buf_ref(res){};
  ResourceBind(int slot_, draw::Image *res)
      : slot(slot_), is_reference(false), type(Type::Image), texture(draw::as_texture(res)){};
  ResourceBind(int slot_, draw::Image **res)
      : slot(slot_), is_reference(true), type(Type::Image), texture_ref(draw::as_texture(res)){};
  ResourceBind(int slot_, GPUTexture *res, eGPUSamplerState state)
      : sampler(state), slot(slot_), is_reference(false), type(Type::Sampler), texture(res){};
  ResourceBind(int slot_, GPUTexture **res, eGPUSamplerState state)
      : sampler(state), slot(slot_), is_reference(true), type(Type::Sampler), texture_ref(res){};
  ResourceBind(int slot_, GPUVertBuf *res)
      : slot(slot_), is_reference(false), type(Type::BufferSampler), vertex_buf(res){};
  ResourceBind(int slot_, GPUVertBuf **res)
      : slot(slot_), is_reference(true), type(Type::BufferSampler), vertex_buf_ref(res){};

  void execute() const;
  std::string serialize() const;
};

struct PushConstant {
  int location;
  uint8_t array_len;
  uint8_t comp_len;
  enum class Type : uint8_t {
    IntValue = 0,
    FloatValue,
    IntReference,
    FloatReference,
  } type;
  /**
   * IMPORTANT: Data is at the end of the struct as it can span over the next commands.
   * These next commands are not real commands but just memory to hold the data and are not
   * referenced by any Command::Header.
   * This is a hack to support float4x4 copy.
   */
  union {
    int int1_value;
    int2 int2_value;
    int3 int3_value;
    int4 int4_value;
    float float1_value;
    float2 float2_value;
    float3 float3_value;
    float4 float4_value;
    const int *int_ref;
    const int2 *int2_ref;
    const int3 *int3_ref;
    const int4 *int4_ref;
    const float *float_ref;
    const float2 *float2_ref;
    const float3 *float3_ref;
    const float4 *float4_ref;
    const float4x4 *float4x4_ref;
  };

  PushConstant() = default;

  PushConstant(int loc, const float &val)
      : location(loc), array_len(1), comp_len(1), type(Type::FloatValue), float1_value(val){};
  PushConstant(int loc, const float2 &val)
      : location(loc), array_len(1), comp_len(2), type(Type::FloatValue), float2_value(val){};
  PushConstant(int loc, const float3 &val)
      : location(loc), array_len(1), comp_len(3), type(Type::FloatValue), float3_value(val){};
  PushConstant(int loc, const float4 &val)
      : location(loc), array_len(1), comp_len(4), type(Type::FloatValue), float4_value(val){};

  PushConstant(int loc, const int &val)
      : location(loc), array_len(1), comp_len(1), type(Type::IntValue), int1_value(val){};
  PushConstant(int loc, const int2 &val)
      : location(loc), array_len(1), comp_len(2), type(Type::IntValue), int2_value(val){};
  PushConstant(int loc, const int3 &val)
      : location(loc), array_len(1), comp_len(3), type(Type::IntValue), int3_value(val){};
  PushConstant(int loc, const int4 &val)
      : location(loc), array_len(1), comp_len(4), type(Type::IntValue), int4_value(val){};

  PushConstant(int loc, const float *val, int arr)
      : location(loc), array_len(arr), comp_len(1), type(Type::FloatReference), float_ref(val){};
  PushConstant(int loc, const float2 *val, int arr)
      : location(loc), array_len(arr), comp_len(2), type(Type::FloatReference), float2_ref(val){};
  PushConstant(int loc, const float3 *val, int arr)
      : location(loc), array_len(arr), comp_len(3), type(Type::FloatReference), float3_ref(val){};
  PushConstant(int loc, const float4 *val, int arr)
      : location(loc), array_len(arr), comp_len(4), type(Type::FloatReference), float4_ref(val){};
  PushConstant(int loc, const float4x4 *val)
      : location(loc), array_len(1), comp_len(16), type(Type::FloatReference), float4x4_ref(val){};

  PushConstant(int loc, const int *val, int arr)
      : location(loc), array_len(arr), comp_len(1), type(Type::IntReference), int_ref(val){};
  PushConstant(int loc, const int2 *val, int arr)
      : location(loc), array_len(arr), comp_len(2), type(Type::IntReference), int2_ref(val){};
  PushConstant(int loc, const int3 *val, int arr)
      : location(loc), array_len(arr), comp_len(3), type(Type::IntReference), int3_ref(val){};
  PushConstant(int loc, const int4 *val, int arr)
      : location(loc), array_len(arr), comp_len(4), type(Type::IntReference), int4_ref(val){};

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct Draw {
  GPUBatch *batch;
  uint instance_len;
  uint vertex_len;
  uint vertex_first;
  ResourceHandle handle;

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct DrawMulti {
  GPUBatch *batch;
  DrawMultiBuf *multi_draw_buf;
  uint group_first;
  uint uuid;

  void execute(RecordingState &state) const;
  std::string serialize(std::string line_prefix) const;
};

struct DrawIndirect {
  GPUBatch *batch;
  GPUStorageBuf **indirect_buf;
  ResourceHandle handle;

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct Dispatch {
  bool is_reference;
  union {
    int3 size;
    int3 *size_ref;
  };

  Dispatch() = default;

  Dispatch(int3 group_len) : is_reference(false), size(group_len){};
  Dispatch(int3 *group_len) : is_reference(true), size_ref(group_len){};

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct DispatchIndirect {
  GPUStorageBuf **indirect_buf;

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct Barrier {
  eGPUBarrier type;

  void execute() const;
  std::string serialize() const;
};

struct Clear {
  uint8_t clear_channels; /* #eGPUFrameBufferBits. But want to save some bits. */
  uint8_t stencil;
  float depth;
  float4 color;

  void execute() const;
  std::string serialize() const;
};

struct ClearMulti {
  /** \note This should be a Span<float4> but we need have to only have trivial types here. */
  const float4 *colors;
  int colors_len;

  void execute() const;
  std::string serialize() const;
};

struct StateSet {
  DRWState new_state;
  int clip_plane_count;

  void execute(RecordingState &state) const;
  std::string serialize() const;
};

struct StencilSet {
  uint write_mask;
  uint compare_mask;
  uint reference;

  void execute() const;
  std::string serialize() const;
};

union Undetermined {
  ShaderBind shader_bind;
  ResourceBind resource_bind;
  FramebufferBind framebuffer_bind;
  PushConstant push_constant;
  Draw draw;
  DrawMulti draw_multi;
  DrawIndirect draw_indirect;
  Dispatch dispatch;
  DispatchIndirect dispatch_indirect;
  Barrier barrier;
  Clear clear;
  ClearMulti clear_multi;
  StateSet state_set;
  StencilSet stencil_set;
};

/** Try to keep the command size as low as possible for performance. */
BLI_STATIC_ASSERT(sizeof(Undetermined) <= 24, "One of the command type is too large.")

/** \} */

/* -------------------------------------------------------------------- */
/** \name Draw Commands
 *
 * A draw command buffer used to issue single draw commands without instance merging or any
 * other optimizations.
 *
 * It still uses a ResourceIdBuf to keep the same shader interface as multi draw commands.
 *
 * \{ */

class DrawCommandBuf {
  friend Manager;

 private:
  using ResourceIdBuf = StorageArrayBuffer<uint, 128, false>;
  using SubPassVector = detail::SubPassVector<detail::PassBase<DrawCommandBuf>, 16>;

  /** Array of resource id. One per instance. Generated on GPU and send to GPU. */
  ResourceIdBuf resource_id_buf_;
  /** Used items in the resource_id_buf_. Not it's allocated length. */
  uint resource_id_count_ = 0;

 public:
  void clear(){};

  void append_draw(Vector<Header, 0> &headers,
                   Vector<Undetermined, 0> &commands,
                   GPUBatch *batch,
                   uint instance_len,
                   uint vertex_len,
                   uint vertex_first,
                   ResourceHandle handle)
  {
    vertex_first = vertex_first != -1 ? vertex_first : 0;
    instance_len = instance_len != -1 ? instance_len : 1;

    int64_t index = commands.append_and_get_index({});
    headers.append({Type::Draw, uint(index)});
    commands[index].draw = {batch, instance_len, vertex_len, vertex_first, handle};
  }

  void bind(RecordingState &state,
            Vector<Header, 0> &headers,
            Vector<Undetermined, 0> &commands,
            SubPassVector &sub_passes);

 private:
  static void finalize_commands(Vector<Header, 0> &headers,
                                Vector<Undetermined, 0> &commands,
                                SubPassVector &sub_passes,
                                uint &resource_id_count,
                                ResourceIdBuf &resource_id_buf);
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Multi Draw Commands
 *
 * For efficient rendering of large scene we strive to minimize the number of draw call and state
 * changes. To this end, we group many rendering commands and sort them per render state using
 * `DrawGroup` as a container. This is done automatically for any successive commands with the
 * same state.
 *
 * A `DrawGroup` is the combination of a `GPUBatch` (VBO state) and a `command::DrawMulti`
 * (Pipeline State).
 *
 * Inside each `DrawGroup` all instances of a same `GPUBatch` is merged into a single indirect
 * command.
 *
 * To support this arbitrary reordering, we only need to know the offset of all the commands for a
 * specific `DrawGroup`. This is done on CPU by doing a simple prefix sum. The result is pushed to
 * GPU and used on CPU to issue the right command indirect.
 *
 * Each draw command is stored in an unsorted array of `DrawPrototype` and sent directly to the
 * GPU.
 *
 * A command generation compute shader then go over each `DrawPrototype`. For each it adds it (or
 * not depending on visibility) to the correct draw command using the offset of the `DrawGroup`
 * computed on CPU. After that, it also outputs one resource ID for each instance inside a
 * `DrawPrototype`.
 *
 * \{ */

class DrawMultiBuf {
  friend Manager;
  friend DrawMulti;

 private:
  using DrawGroupBuf = StorageArrayBuffer<DrawGroup, 16>;
  using DrawPrototypeBuf = StorageArrayBuffer<DrawPrototype, 16>;
  using DrawCommandBuf = StorageArrayBuffer<DrawCommand, 16, true>;
  using ResourceIdBuf = StorageArrayBuffer<uint, 128, true>;

  using DrawGroupKey = std::pair<uint, GPUBatch *>;
  using DrawGroupMap = Map<DrawGroupKey, uint>;
  /** Maps a DrawMulti command and a gpu batch to their unique DrawGroup command. */
  DrawGroupMap group_ids_;

  /** DrawGroup Command heap. Uploaded to GPU for sorting. */
  DrawGroupBuf group_buf_ = {"DrawGroupBuf"};
  /** Command Prototypes. Unsorted */
  DrawPrototypeBuf prototype_buf_ = {"DrawPrototypeBuf"};
  /** Command list generated by the sorting / compaction steps. Lives on GPU. */
  DrawCommandBuf command_buf_ = {"DrawCommandBuf"};
  /** Array of resource id. One per instance. Lives on GPU. */
  ResourceIdBuf resource_id_buf_ = {"ResourceIdBuf"};
  /** Give unique ID to each header so we can use that as hash key. */
  uint header_id_counter_ = 0;
  /** Number of groups inside group_buf_. */
  uint group_count_ = 0;
  /** Number of prototype command inside prototype_buf_. */
  uint prototype_count_ = 0;
  /** Used items in the resource_id_buf_. Not it's allocated length. */
  uint resource_id_count_ = 0;

 public:
  void clear()
  {
    header_id_counter_ = 0;
    group_count_ = 0;
    prototype_count_ = 0;
    group_ids_.clear();
  }

  void append_draw(Vector<Header, 0> &headers,
                   Vector<Undetermined, 0> &commands,
                   GPUBatch *batch,
                   uint instance_len,
                   uint vertex_len,
                   uint vertex_first,
                   ResourceHandle handle)
  {
    /* Custom draw-calls cannot be batched and will produce one group per draw. */
    const bool custom_group = ((vertex_first != 0 && vertex_first != -1) || vertex_len != -1);

    instance_len = instance_len != -1 ? instance_len : 1;

    /* If there was some state changes since previous call, we have to create another command. */
    if (headers.is_empty() || headers.last().type != Type::DrawMulti) {
      uint index = commands.append_and_get_index({});
      headers.append({Type::DrawMulti, index});
      commands[index].draw_multi = {batch, this, (uint)-1, header_id_counter_++};
    }

    DrawMulti &cmd = commands.last().draw_multi;

    uint &group_id = group_ids_.lookup_or_add(DrawGroupKey(cmd.uuid, batch), uint(-1));

    bool inverted = handle.has_inverted_handedness();

    DrawPrototype &draw = prototype_buf_.get_or_resize(prototype_count_++);
    draw.resource_handle = handle.raw;
    draw.instance_len = instance_len;
    draw.group_id = group_id;

    if (group_id == uint(-1) || custom_group) {
      uint new_group_id = group_count_++;
      draw.group_id = new_group_id;

      DrawGroup &group = group_buf_.get_or_resize(new_group_id);
      group.next = cmd.group_first;
      group.len = instance_len;
      group.front_facing_len = inverted ? 0 : instance_len;
      group.gpu_batch = batch;
      group.front_proto_len = 0;
      group.back_proto_len = 0;
      group.vertex_len = vertex_len;
      group.vertex_first = vertex_first;
      /* Custom group are not to be registered in the group_ids_. */
      if (!custom_group) {
        group_id = new_group_id;
      }
      /* For serialization only. */
      (inverted ? group.back_proto_len : group.front_proto_len)++;
      /* Append to list. */
      cmd.group_first = new_group_id;
    }
    else {
      DrawGroup &group = group_buf_[group_id];
      group.len += instance_len;
      group.front_facing_len += inverted ? 0 : instance_len;
      /* For serialization only. */
      (inverted ? group.back_proto_len : group.front_proto_len)++;
    }
  }

  void bind(RecordingState &state,
            Vector<Header, 0> &headers,
            Vector<Undetermined, 0> &commands,
            VisibilityBuf &visibility_buf,
            int visibility_word_per_draw,
            int view_len);
};

/** \} */

};  // namespace blender::draw::command

/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#pragma once

#include "MEM_guardedalloc.h"

#include "gpu_context_private.hh"

#include "GPU_common_types.h"
#include "GPU_context.h"

/* Don't generate OpenGL deprecation warning. This is a known thing, and is not something easily
 * solvable in a short term. */
#ifdef __clang__
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "intern/GHOST_Context.hh"
#include "intern/GHOST_ContextCGL.hh"
#include "intern/GHOST_Window.hh"

#include "mtl_backend.hh"
#include "mtl_capabilities.hh"
#include "mtl_common.hh"
#include "mtl_framebuffer.hh"
#include "mtl_memory.hh"
#include "mtl_shader.hh"
#include "mtl_shader_interface.hh"
#include "mtl_texture.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>
#include <mutex>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLRenderPipelineState;

namespace blender::gpu {

/* Forward Declarations */
class MTLContext;
class MTLCommandBufferManager;
class MTLUniformBuf;
class MTLStorageBuf;

/* Structs containing information on current binding state for textures and samplers. */
struct MTLTextureBinding {
  bool used;

  /* Same value as index in bindings array. */
  uint slot_index;
  gpu::MTLTexture *texture_resource;
};

struct MTLSamplerBinding {
  bool used;
  MTLSamplerState state;

  bool operator==(MTLSamplerBinding const &other) const
  {
    return (used == other.used && state == other.state);
  }
};

/* Caching of resource bindings for active MTLRenderCommandEncoder.
 * In Metal, resource bindings are local to the MTLCommandEncoder,
 * not globally to the whole pipeline/cmd buffer. */
struct MTLBoundShaderState {
  MTLShader *shader_ = nullptr;
  uint pso_index_;
  void set(MTLShader *shader, uint pso_index)
  {
    shader_ = shader;
    pso_index_ = pso_index;
  }
};

/* Caching of CommandEncoder Vertex/Fragment buffer bindings. */
struct BufferBindingCached {
  /* Whether the given binding slot uses byte data (Push Constant equivalent)
   * or an MTLBuffer. */
  bool is_bytes;
  id<MTLBuffer> metal_buffer;
  uint64_t offset;
};

/* Caching of CommandEncoder textures bindings. */
struct TextureBindingCached {
  id<MTLTexture> metal_texture;
};

/* Cached of CommandEncoder sampler states. */
struct SamplerStateBindingCached {
  MTLSamplerState binding_state;
  id<MTLSamplerState> sampler_state;
  bool is_arg_buffer_binding;
};

/* Metal Context Render Pass State -- Used to track active RenderCommandEncoder state based on
 * bound MTLFrameBuffer's.Owned by MTLContext. */
class MTLRenderPassState {
  friend class MTLContext;

 public:
  MTLRenderPassState(MTLContext &context, MTLCommandBufferManager &command_buffer_manager)
      : ctx(context), cmd(command_buffer_manager){};

  /* Given a RenderPassState is associated with a live RenderCommandEncoder,
   * this state sits within the MTLCommandBufferManager. */
  MTLContext &ctx;
  MTLCommandBufferManager &cmd;

  MTLBoundShaderState last_bound_shader_state;
  id<MTLRenderPipelineState> bound_pso = nil;
  id<MTLDepthStencilState> bound_ds_state = nil;
  uint last_used_stencil_ref_value = 0;
  MTLScissorRect last_scissor_rect;

  BufferBindingCached cached_vertex_buffer_bindings[MTL_MAX_BUFFER_BINDINGS];
  BufferBindingCached cached_fragment_buffer_bindings[MTL_MAX_BUFFER_BINDINGS];
  TextureBindingCached cached_vertex_texture_bindings[MTL_MAX_TEXTURE_SLOTS];
  TextureBindingCached cached_fragment_texture_bindings[MTL_MAX_TEXTURE_SLOTS];
  SamplerStateBindingCached cached_vertex_sampler_state_bindings[MTL_MAX_TEXTURE_SLOTS];
  SamplerStateBindingCached cached_fragment_sampler_state_bindings[MTL_MAX_TEXTURE_SLOTS];

  /* Reset RenderCommandEncoder binding state. */
  void reset_state();

  /* Texture Binding (RenderCommandEncoder). */
  void bind_vertex_texture(id<MTLTexture> tex, uint slot);
  void bind_fragment_texture(id<MTLTexture> tex, uint slot);

  /* Sampler Binding (RenderCommandEncoder). */
  void bind_vertex_sampler(MTLSamplerBinding &sampler_binding,
                           bool use_argument_buffer_for_samplers,
                           uint slot);
  void bind_fragment_sampler(MTLSamplerBinding &sampler_binding,
                             bool use_argument_buffer_for_samplers,
                             uint slot);

  /* Buffer binding (RenderCommandEncoder). */
  void bind_vertex_buffer(id<MTLBuffer> buffer, uint64_t buffer_offset, uint index);
  void bind_fragment_buffer(id<MTLBuffer> buffer, uint64_t buffer_offset, uint index);
  void bind_vertex_bytes(void *bytes, uint64_t length, uint index);
  void bind_fragment_bytes(void *bytes, uint64_t length, uint index);
};

/* Metal Context Compute Pass State -- Used to track active ComputeCommandEncoder state. */
class MTLComputeState {
  friend class MTLContext;

 public:
  MTLComputeState(MTLContext &context, MTLCommandBufferManager &command_buffer_manager)
      : ctx(context), cmd(command_buffer_manager){};

  /* Given a ComputePassState is associated with a live ComputeCommandEncoder,
   * this state sits within the MTLCommandBufferManager. */
  MTLContext &ctx;
  MTLCommandBufferManager &cmd;

  id<MTLComputePipelineState> bound_pso = nil;
  BufferBindingCached cached_compute_buffer_bindings[MTL_MAX_BUFFER_BINDINGS];
  TextureBindingCached cached_compute_texture_bindings[MTL_MAX_TEXTURE_SLOTS];
  SamplerStateBindingCached cached_compute_sampler_state_bindings[MTL_MAX_TEXTURE_SLOTS];

  /* Reset ComputeCommandEncoder binding state. */
  void reset_state();

  /* PSO Binding. */
  void bind_pso(id<MTLComputePipelineState> pso);

  /* Texture Binding (ComputeCommandEncoder). */
  void bind_compute_texture(id<MTLTexture> tex, uint slot);
  /* Sampler Binding (ComputeCommandEncoder). */
  void bind_compute_sampler(MTLSamplerBinding &sampler_binding,
                            bool use_argument_buffer_for_samplers,
                            uint slot);
  /* Buffer binding (ComputeCommandEncoder). */
  void bind_compute_buffer(id<MTLBuffer> buffer,
                           uint64_t buffer_offset,
                           uint index,
                           bool writeable = false);
  void bind_compute_bytes(void *bytes, uint64_t length, uint index);
};

/* Depth Stencil State */
struct MTLContextDepthStencilState {

  /* Depth State. */
  bool depth_write_enable;
  bool depth_test_enabled;
  float depth_range_near;
  float depth_range_far;
  MTLCompareFunction depth_function;
  float depth_bias;
  float depth_slope_scale;
  bool depth_bias_enabled_for_points;
  bool depth_bias_enabled_for_lines;
  bool depth_bias_enabled_for_tris;

  /* Stencil State. */
  bool stencil_test_enabled;
  uint stencil_read_mask;
  uint stencil_write_mask;
  uint stencil_ref;
  MTLCompareFunction stencil_func;

  MTLStencilOperation stencil_op_front_stencil_fail;
  MTLStencilOperation stencil_op_front_depth_fail;
  MTLStencilOperation stencil_op_front_depthstencil_pass;

  MTLStencilOperation stencil_op_back_stencil_fail;
  MTLStencilOperation stencil_op_back_depth_fail;
  MTLStencilOperation stencil_op_back_depthstencil_pass;

  /* Frame-buffer State -- We need to mark this, in case stencil state remains unchanged,
   * but attachment state has changed. */
  bool has_depth_target;
  bool has_stencil_target;

  /* TODO(Metal): Consider optimizing this function using `memcmp`.
   * Un-used, but differing, stencil state leads to over-generation
   * of state objects when doing trivial compare. */
  bool operator==(const MTLContextDepthStencilState &other) const
  {
    bool depth_state_equality = (has_depth_target == other.has_depth_target &&
                                 depth_write_enable == other.depth_write_enable &&
                                 depth_test_enabled == other.depth_test_enabled &&
                                 depth_function == other.depth_function);

    bool stencil_state_equality = true;
    if (has_stencil_target) {
      stencil_state_equality =
          (has_stencil_target == other.has_stencil_target &&
           stencil_test_enabled == other.stencil_test_enabled &&
           stencil_op_front_stencil_fail == other.stencil_op_front_stencil_fail &&
           stencil_op_front_depth_fail == other.stencil_op_front_depth_fail &&
           stencil_op_front_depthstencil_pass == other.stencil_op_front_depthstencil_pass &&
           stencil_op_back_stencil_fail == other.stencil_op_back_stencil_fail &&
           stencil_op_back_depth_fail == other.stencil_op_back_depth_fail &&
           stencil_op_back_depthstencil_pass == other.stencil_op_back_depthstencil_pass &&
           stencil_func == other.stencil_func && stencil_read_mask == other.stencil_read_mask &&
           stencil_write_mask == other.stencil_write_mask);
    }

    return depth_state_equality && stencil_state_equality;
  }

  /* Depth stencil state will get hashed in order to prepare
   * MTLDepthStencilState objects. The hash should comprise of
   * all elements which fill the MTLDepthStencilDescriptor.
   * These are bound when [rec setDepthStencilState:...] is called.
   * Depth bias and stencil reference value are set dynamically on the RenderCommandEncoder:
   *  - setStencilReferenceValue:
   *  - setDepthBias:slopeScale:clamp:
   */
  std::size_t hash() const
  {
    std::size_t boolean_bitmask = (this->depth_write_enable ? 1 : 0) |
                                  ((this->depth_test_enabled ? 1 : 0) << 1) |
                                  ((this->depth_bias_enabled_for_points ? 1 : 0) << 2) |
                                  ((this->depth_bias_enabled_for_lines ? 1 : 0) << 3) |
                                  ((this->depth_bias_enabled_for_tris ? 1 : 0) << 4) |
                                  ((this->stencil_test_enabled ? 1 : 0) << 5) |
                                  ((this->has_depth_target ? 1 : 0) << 6) |
                                  ((this->has_stencil_target ? 1 : 0) << 7);

    std::size_t stencilop_bitmask = ((std::size_t)this->stencil_op_front_stencil_fail) |
                                    ((std::size_t)this->stencil_op_front_depth_fail << 3) |
                                    ((std::size_t)this->stencil_op_front_depthstencil_pass << 6) |
                                    ((std::size_t)this->stencil_op_back_stencil_fail << 9) |
                                    ((std::size_t)this->stencil_op_back_depth_fail << 12) |
                                    ((std::size_t)this->stencil_op_back_depthstencil_pass << 15);

    std::size_t main_hash = (std::size_t)this->depth_function;
    if (this->has_stencil_target) {
      main_hash += (std::size_t)(this->stencil_read_mask & 0xFF) << 8;
      main_hash += (std::size_t)(this->stencil_write_mask & 0xFF) << 16;
    }
    main_hash ^= (std::size_t)this->stencil_func << 16;
    main_hash ^= stencilop_bitmask;

    std::size_t final_hash = (main_hash << 8) | boolean_bitmask;
    return final_hash;
  }
};

struct MTLContextTextureUtils {

  /* Depth Update Utilities */
  /* Depth texture updates are not directly supported with Blit operations, similarly, we cannot
   * use a compute shader to write to depth, so we must instead render to a depth target.
   * These processes use vertex/fragment shaders to render texture data from an intermediate
   * source, in order to prime the depth buffer. */
  blender::Map<DepthTextureUpdateRoutineSpecialisation, GPUShader *> depth_2d_update_shaders;
  GPUShader *fullscreen_blit_shader = nullptr;

  /* Texture Read/Update routines */
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_1d_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_1d_array_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_2d_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_2d_array_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_3d_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_cube_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_cube_array_read_compute_psos;
  blender::Map<TextureReadRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_buffer_read_compute_psos;

  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_1d_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_1d_array_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_2d_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_2d_array_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_3d_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_cube_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_cube_array_update_compute_psos;
  blender::Map<TextureUpdateRoutineSpecialisation, id<MTLComputePipelineState>>
      texture_buffer_update_compute_psos;

  template<typename T> void free_cached_pso_map(blender::Map<T, id<MTLComputePipelineState>> &map)
  {
    for (typename blender::MutableMapItem<T, id<MTLComputePipelineState>> item : map.items()) {
      [item.value release];
    }
    map.clear();
  }

  void init()
  {
    fullscreen_blit_shader = nullptr;
  }

  void cleanup()
  {
    if (fullscreen_blit_shader) {
      GPU_shader_free(fullscreen_blit_shader);
    }

    /* Free depth 2D Update shaders */
    for (auto item : depth_2d_update_shaders.items()) {
      GPU_shader_free(item.value);
    }
    depth_2d_update_shaders.clear();

    /* Free Read shader maps */
    free_cached_pso_map(texture_1d_read_compute_psos);
    free_cached_pso_map(texture_1d_read_compute_psos);
    free_cached_pso_map(texture_1d_array_read_compute_psos);
    free_cached_pso_map(texture_2d_read_compute_psos);
    free_cached_pso_map(texture_2d_array_read_compute_psos);
    free_cached_pso_map(texture_3d_read_compute_psos);
    free_cached_pso_map(texture_cube_read_compute_psos);
    free_cached_pso_map(texture_cube_array_read_compute_psos);
    free_cached_pso_map(texture_buffer_read_compute_psos);
    free_cached_pso_map(texture_1d_update_compute_psos);
    free_cached_pso_map(texture_1d_array_update_compute_psos);
    free_cached_pso_map(texture_2d_update_compute_psos);
    free_cached_pso_map(texture_2d_array_update_compute_psos);
    free_cached_pso_map(texture_3d_update_compute_psos);
    free_cached_pso_map(texture_cube_update_compute_psos);
    free_cached_pso_map(texture_cube_array_update_compute_psos);
    free_cached_pso_map(texture_buffer_update_compute_psos);
  }
};

/* Combined sampler state configuration for Argument Buffer caching. */
struct MTLSamplerArray {
  uint num_samplers;
  /* MTLSamplerState permutations between 0..256 - slightly more than a byte. */
  MTLSamplerState mtl_sampler_flags[MTL_MAX_TEXTURE_SLOTS];
  id<MTLSamplerState> mtl_sampler[MTL_MAX_TEXTURE_SLOTS];

  bool operator==(const MTLSamplerArray &other) const
  {
    if (this->num_samplers != other.num_samplers) {
      return false;
    }
    return (memcmp(this->mtl_sampler_flags,
                   other.mtl_sampler_flags,
                   sizeof(MTLSamplerState) * this->num_samplers) == 0);
  }

  uint32_t hash() const
  {
    uint32_t hash = this->num_samplers;
    for (int i = 0; i < this->num_samplers; i++) {
      hash ^= uint32_t(this->mtl_sampler_flags[i]) << (i % 3);
    }
    return hash;
  }
};

typedef enum MTLPipelineStateDirtyFlag {
  MTL_PIPELINE_STATE_NULL_FLAG = 0,
  /* Whether we need to call setViewport. */
  MTL_PIPELINE_STATE_VIEWPORT_FLAG = (1 << 0),
  /* Whether we need to call setScissor. */
  MTL_PIPELINE_STATE_SCISSOR_FLAG = (1 << 1),
  /* Whether we need to update/rebind active depth stencil state. */
  MTL_PIPELINE_STATE_DEPTHSTENCIL_FLAG = (1 << 2),
  /* Whether we need to update/rebind active PSO. */
  MTL_PIPELINE_STATE_PSO_FLAG = (1 << 3),
  /* Whether we need to update the frontFacingWinding state. */
  MTL_PIPELINE_STATE_FRONT_FACING_FLAG = (1 << 4),
  /* Whether we need to update the culling state. */
  MTL_PIPELINE_STATE_CULLMODE_FLAG = (1 << 5),
  /* Full pipeline state needs applying. Occurs when beginning a new render pass. */
  MTL_PIPELINE_STATE_ALL_FLAG =
      (MTL_PIPELINE_STATE_VIEWPORT_FLAG | MTL_PIPELINE_STATE_SCISSOR_FLAG |
       MTL_PIPELINE_STATE_DEPTHSTENCIL_FLAG | MTL_PIPELINE_STATE_PSO_FLAG |
       MTL_PIPELINE_STATE_FRONT_FACING_FLAG | MTL_PIPELINE_STATE_CULLMODE_FLAG)
} MTLPipelineStateDirtyFlag;

/* Ignore full flag bit-mask `MTL_PIPELINE_STATE_ALL_FLAG`. */
ENUM_OPERATORS(MTLPipelineStateDirtyFlag, MTL_PIPELINE_STATE_CULLMODE_FLAG);

struct MTLUniformBufferBinding {
  bool bound;
  MTLUniformBuf *ubo;
};

struct MTLStorageBufferBinding {
  bool bound;
  MTLStorageBuf *ssbo;
};

struct MTLContextGlobalShaderPipelineState {
  bool initialised;

  /* Whether the pipeline state has been modified since application.
   * `dirty_flags` is a bitmask of the types of state which have been updated.
   * This is in order to optimize calls and only re-apply state as needed.
   * Some state parameters are dynamically applied on the RenderCommandEncoder,
   * others may be encapsulated in GPU-resident state objects such as
   * MTLDepthStencilState or MTLRenderPipelineState. */
  bool dirty;
  MTLPipelineStateDirtyFlag dirty_flags;

  /* Shader resources. */
  MTLShader *null_shader;

  /* Active Shader State. */
  MTLShader *active_shader;

  /* Global Uniform Buffers. */
  MTLUniformBufferBinding ubo_bindings[MTL_MAX_BUFFER_BINDINGS];

  /* Storage buffer. */
  MTLStorageBufferBinding ssbo_bindings[MTL_MAX_BUFFER_BINDINGS];

  /* Context Texture bindings. */
  MTLTextureBinding texture_bindings[MTL_MAX_TEXTURE_SLOTS];
  MTLSamplerBinding sampler_bindings[MTL_MAX_SAMPLER_SLOTS];

  /* Image bindings. */
  MTLTextureBinding image_bindings[MTL_MAX_TEXTURE_SLOTS];

  /*** --- Render Pipeline State --- ***/
  /* Track global render pipeline state for the current context. The functions in GPU_state.h
   * modify these parameters. Certain values, tagged [PSO], are parameters which are required to be
   * passed into PSO creation, rather than dynamic state functions on the RenderCommandEncoder.
   */

  /* Blending State. */
  MTLColorWriteMask color_write_mask;     /* [PSO] */
  bool blending_enabled;                  /* [PSO] */
  MTLBlendOperation alpha_blend_op;       /* [PSO] */
  MTLBlendOperation rgb_blend_op;         /* [PSO] */
  MTLBlendFactor dest_alpha_blend_factor; /* [PSO] */
  MTLBlendFactor dest_rgb_blend_factor;   /* [PSO] */
  MTLBlendFactor src_alpha_blend_factor;  /* [PSO] */
  MTLBlendFactor src_rgb_blend_factor;    /* [PSO] */

  /* Culling State. */
  bool culling_enabled;
  eGPUFaceCullTest cull_mode;
  eGPUFrontFace front_face;

  /* Depth State. */
  MTLContextDepthStencilState depth_stencil_state;

  /* Viewport/Scissor Region. */
  int viewport_offset_x;
  int viewport_offset_y;
  int viewport_width;
  int viewport_height;
  bool scissor_enabled;
  int scissor_x;
  int scissor_y;
  int scissor_width;
  int scissor_height;

  /* Image data access state. */
  uint unpack_row_length;

  /* Render parameters. */
  float point_size = 1.0f;
  float line_width = 1.0f;

  /* Clipping plane enablement. */
  bool clip_distance_enabled[6] = {false};
};

/* Command Buffer Manager - Owned by MTLContext.
 * The MTLCommandBufferManager represents all work associated with
 * a command buffer of a given identity. This manager is a fixed-state
 * on the context, which coordinates the lifetime of command buffers
 * for particular categories of work.
 *
 * This ensures operations on command buffers, and the state associated,
 * is correctly tracked and managed. Workload submission and MTLCommandEncoder
 * coordination is managed from here.
 *
 * There is currently only one MTLCommandBufferManager for managing submission
 * of the "main" rendering commands. A secondary upload command buffer track,
 * or asynchronous compute command buffer track may be added in the future. */
class MTLCommandBufferManager {
  friend class MTLContext;

 public:
  /* Event to coordinate sequential execution across all "main" command buffers. */
  static id<MTLEvent> sync_event;
  static uint64_t event_signal_val;

  /* Counter for active command buffers. */
  static int num_active_cmd_bufs;

 private:
  /* Associated Context and properties. */
  MTLContext &context_;
  bool supports_render_ = false;

  /* CommandBuffer tracking. */
  id<MTLCommandBuffer> active_command_buffer_ = nil;
  id<MTLCommandBuffer> last_submitted_command_buffer_ = nil;

  /* Active MTLCommandEncoders. */
  enum {
    MTL_NO_COMMAND_ENCODER = 0,
    MTL_RENDER_COMMAND_ENCODER = 1,
    MTL_BLIT_COMMAND_ENCODER = 2,
    MTL_COMPUTE_COMMAND_ENCODER = 3
  } active_command_encoder_type_ = MTL_NO_COMMAND_ENCODER;

  id<MTLRenderCommandEncoder> active_render_command_encoder_ = nil;
  id<MTLBlitCommandEncoder> active_blit_command_encoder_ = nil;
  id<MTLComputeCommandEncoder> active_compute_command_encoder_ = nil;

  /* State associated with active RenderCommandEncoder. */
  MTLRenderPassState render_pass_state_;
  MTLFrameBuffer *active_frame_buffer_ = nullptr;
  MTLRenderPassDescriptor *active_pass_descriptor_ = nullptr;

  /* State associated with active ComputeCommandEncoder. */
  MTLComputeState compute_state_;

  /* Workload heuristics - We may need to split command buffers to optimize workload and balancing.
   */
  int current_draw_call_count_ = 0;
  int encoder_count_ = 0;
  int vertex_submitted_count_ = 0;
  bool empty_ = true;

 public:
  MTLCommandBufferManager(MTLContext &context)
      : context_(context), render_pass_state_(context, *this), compute_state_(context, *this){};
  void prepare(bool supports_render = true);

  /* If wait is true, CPU will stall until GPU work has completed. */
  bool submit(bool wait);

  /* Fetch/query current encoder. */
  bool is_inside_render_pass();
  bool is_inside_blit();
  bool is_inside_compute();
  id<MTLRenderCommandEncoder> get_active_render_command_encoder();
  id<MTLBlitCommandEncoder> get_active_blit_command_encoder();
  id<MTLComputeCommandEncoder> get_active_compute_command_encoder();
  MTLFrameBuffer *get_active_framebuffer();

  /* RenderPassState for RenderCommandEncoder. */
  MTLRenderPassState &get_render_pass_state()
  {
    /* Render pass state should only be valid if we are inside a render pass. */
    BLI_assert(this->is_inside_render_pass());
    return render_pass_state_;
  }

  /* RenderPassState for RenderCommandEncoder. */
  MTLComputeState &get_compute_state()
  {
    /* Render pass state should only be valid if we are inside a compute encoder. */
    BLI_assert(this->is_inside_compute());
    return compute_state_;
  }

  /* Rendering Heuristics. */
  void register_draw_counters(int vertex_submission);
  void reset_counters();
  bool do_break_submission();

  /* Encoder and Pass management. */
  /* End currently active MTLCommandEncoder. */
  bool end_active_command_encoder();
  id<MTLRenderCommandEncoder> ensure_begin_render_command_encoder(MTLFrameBuffer *ctx_framebuffer,
                                                                  bool force_begin,
                                                                  bool *new_pass);
  id<MTLBlitCommandEncoder> ensure_begin_blit_encoder();
  id<MTLComputeCommandEncoder> ensure_begin_compute_encoder();

  /* Workload Synchronization. */
  bool insert_memory_barrier(eGPUBarrier barrier_bits,
                             eGPUStageBarrierBits before_stages,
                             eGPUStageBarrierBits after_stages);
  void encode_signal_event(id<MTLEvent> event, uint64_t value);
  void encode_wait_for_event(id<MTLEvent> event, uint64_t value);
  /* TODO(Metal): Support fences in command buffer class. */

  /* Debug. */
  void push_debug_group(const char *name, int index);
  void pop_debug_group();

 private:
  /* Begin new command buffer. */
  id<MTLCommandBuffer> ensure_begin();

  void register_encoder_counters();
};

/** MTLContext -- Core render loop and state management. **/
/* NOTE(Metal): Partial #MTLContext stub to provide wrapper functionality
 * for work-in-progress `MTL*` classes. */

class MTLContext : public Context {
  friend class MTLBackend;
  friend class MTLRenderPassState;
  friend class MTLComputeState;

 public:
  /* Swap-chain and latency management. */
  static std::atomic<int> max_drawables_in_flight;
  static std::atomic<int64_t> avg_drawable_latency_us;
  static int64_t frame_latency[MTL_FRAME_AVERAGE_COUNT];

 public:
  /* Shaders and Pipeline state. */
  MTLContextGlobalShaderPipelineState pipeline_state;

  /* Metal API Resource Handles. */
  id<MTLCommandQueue> queue = nil;
  id<MTLDevice> device = nil;

#ifndef NDEBUG
  /* Label for Context debug name assignment. */
  NSString *label = nil;
#endif

  /* Memory Management. */
  MTLScratchBufferManager memory_manager;
  static std::mutex global_memory_manager_reflock;
  static int global_memory_manager_refcount;
  static MTLBufferPool *global_memory_manager;

  /* CommandBuffer managers. */
  MTLCommandBufferManager main_command_buffer;

 private:
  /* Parent Context. */
  GHOST_ContextCGL *ghost_context_;

  /* Render Passes and Frame-buffers. */
  id<MTLTexture> default_fbo_mtltexture_ = nil;
  gpu::MTLTexture *default_fbo_gputexture_ = nullptr;

  /* Depth-stencil state cache. */
  blender::Map<MTLContextDepthStencilState, id<MTLDepthStencilState>> depth_stencil_state_cache;

  /* Compute and specialization caches. */
  MTLContextTextureUtils texture_utils_;

  /* Texture Samplers. */
  /* Cache of generated #MTLSamplerState objects based on permutations of the members of
   * `GPUSamplerState`. */
  id<MTLSamplerState> sampler_state_cache_[GPU_SAMPLER_EXTEND_MODES_COUNT]
                                          [GPU_SAMPLER_EXTEND_MODES_COUNT]
                                          [GPU_SAMPLER_FILTERING_TYPES_COUNT];
  id<MTLSamplerState> custom_sampler_state_cache_[GPU_SAMPLER_CUSTOM_TYPES_COUNT];
  id<MTLSamplerState> default_sampler_state_ = nil;

  /* When texture sampler count exceeds the resource bind limit, an
   * argument buffer is used to pass samplers to the shader.
   * Each unique configurations of multiple samplers can be cached, so as to not require
   * re-generation. `samplers_` stores the current list of bound sampler objects.
   * `cached_sampler_buffers_` is a cache of encoded argument buffers which can be re-used. */
  MTLSamplerArray samplers_;
  blender::Map<MTLSamplerArray, gpu::MTLBuffer *> cached_sampler_buffers_;

  /* Frame. */
  bool is_inside_frame_ = false;
  uint current_frame_index_;

  /* Visibility buffer for MTLQuery results. */
  gpu::MTLBuffer *visibility_buffer_ = nullptr;
  bool visibility_is_dirty_ = false;

  /* Null buffers for empty/uninitialized bindings.
   * Null attribute buffer follows default attribute format of OpenGL Backend. */
  id<MTLBuffer> null_buffer_;           /* All zero's. */
  id<MTLBuffer> null_attribute_buffer_; /* Value float4(0.0,0.0,0.0,1.0). */

  /** Dummy Resources */
  /* Maximum of 32 texture types. Though most combinations invalid. */
  gpu::MTLTexture *dummy_textures_[GPU_SAMPLER_TYPE_MAX][GPU_TEXTURE_BUFFER] = {{nullptr}};
  GPUVertFormat dummy_vertformat_[GPU_SAMPLER_TYPE_MAX];
  GPUVertBuf *dummy_verts_[GPU_SAMPLER_TYPE_MAX] = {nullptr};

 public:
  /* GPUContext interface. */
  MTLContext(void *ghost_window, void *ghost_context);
  ~MTLContext();

  static void check_error(const char *info);

  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;
  void finish() override;

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  static MTLContext *get()
  {
    return static_cast<MTLContext *>(Context::get());
  }

  void debug_group_begin(const char *name, int index) override;
  void debug_group_end() override;
  bool debug_capture_begin() override;
  void debug_capture_end() override;
  void *debug_capture_scope_create(const char *name) override;
  bool debug_capture_scope_begin(void *scope) override;
  void debug_capture_scope_end(void *scope) override;

  /*** MTLContext Utility functions. */
  /*
   * All below functions modify the global state for the context, controlling the flow of
   * rendering, binding resources, setting global state, resource management etc;
   */

  /** Metal Context Core functions. **/

  /* Bind frame-buffer to context. */
  void framebuffer_bind(MTLFrameBuffer *framebuffer);

  /* Restore frame-buffer used by active context to default back-buffer. */
  void framebuffer_restore();

  /* Ensure a render-pass using the Context frame-buffer (active_fb_) is in progress. */
  id<MTLRenderCommandEncoder> ensure_begin_render_pass();

  MTLFrameBuffer *get_current_framebuffer();
  MTLFrameBuffer *get_default_framebuffer();

  /* Context Global-State Texture Binding. */
  void texture_bind(gpu::MTLTexture *mtl_texture, uint texture_unit, bool is_image);
  void sampler_bind(MTLSamplerState, uint sampler_unit);
  void texture_unbind(gpu::MTLTexture *mtl_texture, bool is_image);
  void texture_unbind_all(bool is_image);
  void sampler_state_cache_init();
  id<MTLSamplerState> get_sampler_from_state(MTLSamplerState state);
  id<MTLSamplerState> get_default_sampler_state();

  /* Metal Context pipeline state. */
  void pipeline_state_init();
  MTLShader *get_active_shader();

  /* These functions ensure that the current RenderCommandEncoder has
   * the correct global state assigned. This should be called prior
   * to every draw call, to ensure that all state is applied and up
   * to date. We handle:
   *
   * - Buffer bindings (Vertex buffers, Uniforms, UBOs, transform feedback)
   * - Texture bindings
   * - Sampler bindings (+ argument buffer bindings)
   * - Dynamic Render pipeline state (on encoder)
   * - Baking Pipeline State Objects (PSOs) for current shader, based
   *   on final pipeline state.
   *
   * `ensure_render_pipeline_state` will return false if the state is
   * invalid and cannot be applied. This should cancel a draw call. */
  bool ensure_render_pipeline_state(MTLPrimitiveType prim_type);
  bool ensure_buffer_bindings(id<MTLRenderCommandEncoder> rec,
                              const MTLShaderInterface *shader_interface,
                              const MTLRenderPipelineStateInstance *pipeline_state_instance);
  bool ensure_buffer_bindings(id<MTLComputeCommandEncoder> rec,
                              const MTLShaderInterface *shader_interface,
                              const MTLComputePipelineStateInstance &pipeline_state_instance);
  void ensure_texture_bindings(id<MTLRenderCommandEncoder> rec,
                               MTLShaderInterface *shader_interface,
                               const MTLRenderPipelineStateInstance *pipeline_state_instance);
  void ensure_texture_bindings(id<MTLComputeCommandEncoder> rec,
                               MTLShaderInterface *shader_interface,
                               const MTLComputePipelineStateInstance &pipeline_state_instance);
  void ensure_depth_stencil_state(MTLPrimitiveType prim_type);

  id<MTLBuffer> get_null_buffer();
  id<MTLBuffer> get_null_attribute_buffer();
  gpu::MTLTexture *get_dummy_texture(eGPUTextureType type, eGPUSamplerFormat sampler_format);
  void free_dummy_resources();

  /* Compute. */
  bool ensure_compute_pipeline_state();
  void compute_dispatch(int groups_x_len, int groups_y_len, int groups_z_len);
  void compute_dispatch_indirect(StorageBuf *indirect_buf);

  /* State assignment. */
  void set_viewport(int origin_x, int origin_y, int width, int height);
  void set_scissor(int scissor_x, int scissor_y, int scissor_width, int scissor_height);
  void set_scissor_enabled(bool scissor_enabled);

  /* Visibility buffer control. */
  void set_visibility_buffer(gpu::MTLBuffer *buffer);
  gpu::MTLBuffer *get_visibility_buffer() const;

  /* Flag whether the visibility buffer for query results
   * has changed. This requires a new RenderPass in order
   * to update. */
  bool is_visibility_dirty() const;

  /* Reset dirty flag state for visibility buffer. */
  void clear_visibility_dirty();

  /* Texture utilities. */
  MTLContextTextureUtils &get_texture_utils()
  {
    return texture_utils_;
  }

  bool get_active()
  {
    return is_active_;
  }

  bool get_inside_frame()
  {
    return is_inside_frame_;
  }

  uint get_current_frame_index()
  {
    return current_frame_index_;
  }

  MTLScratchBufferManager &get_scratchbuffer_manager()
  {
    return this->memory_manager;
  }

  static void global_memory_manager_acquire_ref()
  {
    MTLContext::global_memory_manager_reflock.lock();
    if (MTLContext::global_memory_manager == nullptr) {
      BLI_assert(MTLContext::global_memory_manager_refcount == 0);
      MTLContext::global_memory_manager = new MTLBufferPool();
    }
    MTLContext::global_memory_manager_refcount++;
    MTLContext::global_memory_manager_reflock.unlock();
  }

  static void global_memory_manager_release_ref()
  {
    MTLContext::global_memory_manager_reflock.lock();
    MTLContext::global_memory_manager_refcount--;
    BLI_assert(MTLContext::global_memory_manager_refcount >= 0);
    BLI_assert(MTLContext::global_memory_manager != nullptr);

    if (MTLContext::global_memory_manager_refcount <= 0) {
      delete MTLContext::global_memory_manager;
      MTLContext::global_memory_manager = nullptr;
    }
    MTLContext::global_memory_manager_reflock.unlock();
  }

  static MTLBufferPool *get_global_memory_manager()
  {
    BLI_assert(MTLContext::global_memory_manager != nullptr);
    return MTLContext::global_memory_manager;
  }

  /* Swap-chain and latency management. */
  static void latency_resolve_average(int64_t frame_latency_us)
  {
    int64_t avg = 0;
    int64_t frame_c = 0;
    for (int i = MTL_FRAME_AVERAGE_COUNT - 1; i > 0; i--) {
      MTLContext::frame_latency[i] = MTLContext::frame_latency[i - 1];
      avg += MTLContext::frame_latency[i];
      frame_c += (MTLContext::frame_latency[i] > 0) ? 1 : 0;
    }
    MTLContext::frame_latency[0] = frame_latency_us;
    avg += MTLContext::frame_latency[0];
    if (frame_c > 0) {
      avg /= frame_c;
    }
    else {
      avg = 0;
    }
    MTLContext::avg_drawable_latency_us = avg;
  }

 private:
  void set_ghost_context(GHOST_ContextHandle ghostCtxHandle);
  void set_ghost_window(GHOST_WindowHandle ghostWinHandle);
};

/* GHOST Context callback and present. */
void present(MTLRenderPassDescriptor *blit_descriptor,
             id<MTLRenderPipelineState> blit_pso,
             id<MTLTexture> swapchain_texture,
             id<CAMetalDrawable> drawable);

}  // namespace blender::gpu

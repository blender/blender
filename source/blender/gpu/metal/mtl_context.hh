/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "MEM_guardedalloc.h"

#include "gpu_context_private.hh"

#include "GPU_common_types.h"
#include "GPU_context.h"

#include "mtl_capabilities.hh"
#include "mtl_texture.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLRenderPipelineState;

namespace blender::gpu {

class MTLShader;
class MTLUniformBuf;
class MTLBuffer;

/* Depth Stencil State */
typedef struct MTLContextDepthStencilState {

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
  unsigned int stencil_read_mask;
  unsigned int stencil_write_mask;
  unsigned int stencil_ref;
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

  /* TODO(Metal): Consider optimizing this function using memcmp.
   * Un-used, but differing, stencil state leads to over-generation
   * of state objects when doing trivial compare.  */
  inline bool operator==(const MTLContextDepthStencilState &other) const
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
  inline std::size_t hash() const
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
} MTLContextDepthStencilState;

typedef struct MTLContextTextureUtils {

  /* Depth Update Utilities */
  /* Depth texture updates are not directly supported with Blit operations, similarly, we cannot
   * use a compute shader to write to depth, so we must instead render to a depth target.
   * These processes use vertex/fragment shaders to render texture data from an intermediate
   * source, in order to prime the depth buffer*/
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

  template<typename T>
  inline void free_cached_pso_map(blender::Map<T, id<MTLComputePipelineState>> &map)
  {
    for (typename blender::Map<T, id<MTLComputePipelineState>>::MutableItem item : map.items()) {
      [item.value release];
    }
    map.clear();
  }

  inline void init()
  {
    fullscreen_blit_shader = nullptr;
  }

  inline void cleanup()
  {
    if (fullscreen_blit_shader) {
      GPU_shader_free(fullscreen_blit_shader);
    }

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

} MTLContextTextureUtils;

/* Structs containing information on current binding state for textures and samplers. */
typedef struct MTLTextureBinding {
  bool used;

  /* Same value as index in bindings array. */
  unsigned int texture_slot_index;
  gpu::MTLTexture *texture_resource;

} MTLTextureBinding;

typedef struct MTLSamplerBinding {
  bool used;
  MTLSamplerState state;

  bool operator==(MTLSamplerBinding const &other) const
  {
    return (used == other.used && state == other.state);
  }
} MTLSamplerBinding;

/* Combined sampler state configuration for Argument Buffer caching. */
struct MTLSamplerArray {
  unsigned int num_samplers;
  /* MTLSamplerState permutations between 0..256 - slightly more than a byte. */
  MTLSamplerState mtl_sampler_flags[MTL_MAX_TEXTURE_SLOTS];
  id<MTLSamplerState> mtl_sampler[MTL_MAX_TEXTURE_SLOTS];

  inline bool operator==(const MTLSamplerArray &other) const
  {
    if (this->num_samplers != other.num_samplers) {
      return false;
    }
    return (memcmp(this->mtl_sampler_flags,
                   other.mtl_sampler_flags,
                   sizeof(MTLSamplerState) * this->num_samplers) == 0);
  }

  inline uint32_t hash() const
  {
    uint32_t hash = this->num_samplers;
    for (int i = 0; i < this->num_samplers; i++) {
      hash ^= (uint32_t)this->mtl_sampler_flags[i] << (i % 3);
    }
    return hash;
  }
};

typedef enum MTLPipelineStateDirtyFlag {
  MTL_PIPELINE_STATE_NULL_FLAG = 0,
  /* Whether we need to call setViewport. */
  MTL_PIPELINE_STATE_VIEWPORT_FLAG = (1 << 0),
  /* Whether we need to call setScissor.*/
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

typedef struct MTLUniformBufferBinding {
  bool bound;
  MTLUniformBuf *ubo;
} MTLUniformBufferBinding;

typedef struct MTLContextGlobalShaderPipelineState {
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
  MTLUniformBufferBinding ubo_bindings[MTL_MAX_UNIFORM_BUFFER_BINDINGS];

  /* Context Texture bindings. */
  MTLTextureBinding texture_bindings[MTL_MAX_TEXTURE_SLOTS];
  MTLSamplerBinding sampler_bindings[MTL_MAX_SAMPLER_SLOTS];

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

} MTLContextGlobalShaderPipelineState;

/* Metal Buffer */
typedef struct MTLTemporaryBufferRange {
  id<MTLBuffer> metal_buffer;
  void *host_ptr;
  unsigned long long buffer_offset;
  unsigned long long size;
  MTLResourceOptions options;

  void flush();
  bool requires_flush();
} MTLTemporaryBufferRange;

/** MTLContext -- Core render loop and state management. **/
/* NOTE(Metal): Partial MTLContext stub to provide wrapper functionality
 * for work-in-progress MTL* classes. */

class MTLContext : public Context {
  friend class MTLBackend;

 private:
  /* Compute and specialization caches. */
  MTLContextTextureUtils texture_utils_;

  /* Texture Samplers. */
  /* Cache of generated MTLSamplerState objects based on permutations of `eGPUSamplerState`. */
  id<MTLSamplerState> sampler_state_cache_[GPU_SAMPLER_MAX] = {0};
  id<MTLSamplerState> default_sampler_state_ = nil;

  /* When texture sampler count exceeds the resource bind limit, an
   * argument buffer is used to pass samplers to the shader.
   * Each unique configurations of multiple samplers can be cached, so as to not require
   * re-generation. `samplers_` stores the current list of bound sampler objects.
   * `cached_sampler_buffers_` is a cache of encoded argument buffers which can be re-used. */
  MTLSamplerArray samplers_;
  blender::Map<MTLSamplerArray, gpu::MTLBuffer *> cached_sampler_buffers_;

 public:
  /* Shaders and Pipeline state. */
  MTLContextGlobalShaderPipelineState pipeline_state;

  /* Metal API Resource Handles. */
  id<MTLCommandQueue> queue = nil;
  id<MTLDevice> device = nil;

  /* GPUContext interface. */
  MTLContext(void *ghost_window);
  ~MTLContext();

  static void check_error(const char *info);

  void activate(void) override;
  void deactivate(void) override;

  void flush(void) override;
  void finish(void) override;

  void memory_statistics_get(int *total_mem, int *free_mem) override;

  void debug_group_begin(const char *name, int index) override;
  void debug_group_end(void) override;

  /*** MTLContext Utility functions. */
  /*
   * All below functions modify the global state for the context, controlling the flow of
   * rendering, binding resources, setting global state, resource management etc;
   */

  /* Metal Context Core functions. */
  /* Command Buffer Management. */
  id<MTLCommandBuffer> get_active_command_buffer();

  /* Render Pass State and Management. */
  void begin_render_pass();
  void end_render_pass();
  bool is_render_pass_active();

  /* Texture Binding. */
  void texture_bind(gpu::MTLTexture *mtl_texture, unsigned int texture_unit);
  void sampler_bind(MTLSamplerState, unsigned int sampler_unit);
  void texture_unbind(gpu::MTLTexture *mtl_texture);
  void texture_unbind_all(void);
  id<MTLSamplerState> get_sampler_from_state(MTLSamplerState state);
  id<MTLSamplerState> generate_sampler_from_state(MTLSamplerState state);
  id<MTLSamplerState> get_default_sampler_state();

  /* Metal Context pipeline state. */
  void pipeline_state_init(void);
  MTLShader *get_active_shader(void);

  /* State assignment. */
  void set_viewport(int origin_x, int origin_y, int width, int height);
  void set_scissor(int scissor_x, int scissor_y, int scissor_width, int scissor_height);
  void set_scissor_enabled(bool scissor_enabled);

  /* Texture utilities. */
  MTLContextTextureUtils &get_texture_utils()
  {
    return this->texture_utils_;
  }
};

}  // namespace blender::gpu

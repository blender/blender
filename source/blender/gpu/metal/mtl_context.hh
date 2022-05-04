/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */
#include "MEM_guardedalloc.h"

#include "gpu_context_private.hh"

#include "GPU_context.h"

#include "mtl_texture.hh"

#include <Cocoa/Cocoa.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

@class CAMetalLayer;
@class MTLCommandQueue;
@class MTLRenderPipelineState;

namespace blender::gpu {

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

typedef struct MTLContextGlobalShaderPipelineState {
  /* ..TODO(Metal): More elements to be added as backend fleshed out.. */

  /*** DATA and IMAGE access state ***/
  uint unpack_row_length;
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

/** MTLContext -- Core render loop and state management **/
/* Note(Metal): Partial MTLContext stub to provide wrapper functionality
 * for work-in-progress MTL* classes. */

class MTLContext : public Context {
  friend class MTLBackend;

 private:
  /* Compute and specialization caches. */
  MTLContextTextureUtils texture_utils_;

 public:
  /* METAL API Resource Handles. */
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

  /*** Context Utility functions */
  /*
   * All below functions modify the global state for the context, controlling the flow of
   * rendering, binding resources, setting global state, resource management etc;
   */

  /* Metal Context Core functions */
  /* Command Buffer Management */
  id<MTLCommandBuffer> get_active_command_buffer();

  /* Render Pass State and Management */
  void begin_render_pass();
  void end_render_pass();

  /* Shaders and Pipeline state */
  MTLContextGlobalShaderPipelineState pipeline_state;

  /* Texture utilities */
  MTLContextTextureUtils &get_texture_utils()
  {
    return this->texture_utils_;
  }
};

}  // namespace blender::gpu

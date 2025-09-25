/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_context_private.hh"

#include "GPU_framebuffer.hh"

#include "BKE_global.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "gl_state.hh"

#include <mutex>

namespace blender {
namespace gpu {

class GLVaoCache;

class GLSharedOrphanLists {
  class OrphanList {
    /** Mutex for the below structures. */
    std::mutex mutex_;
    /** Buffers and textures are shared across context. Any context can free them. */
    Vector<GLuint> handles_;

   public:
    void clear(FunctionRef<void(GLuint, GLuint *)> free_fn);
    void append(GLuint handle);
  };

 public:
  /** Shaders, Buffers and textures are shared across context. */
  OrphanList textures;
  OrphanList buffers;
  OrphanList shaders;
  OrphanList programs;

  void orphans_clear();
};

class GLContext : public Context {
 public:
  /** Capabilities. */

  static GLint max_cubemap_size;
  static GLint max_ubo_binds;
  static GLint max_ssbo_binds;

  /** Extensions. */

  static bool debug_layer_support;
  static bool direct_state_access_support;
  static bool explicit_location_support;
  static bool framebuffer_fetch_support;
  static bool layered_rendering_support;
  static bool native_barycentric_support;
  static bool multi_bind_support;
  static bool multi_bind_image_support;
  static bool stencil_texturing_support;
  static bool texture_barrier_support;
  static bool texture_filter_anisotropic_support;

  /** Workarounds. */

  static bool debug_layer_workaround;
  static bool unused_fb_slot_workaround;
  static bool generate_mipmap_workaround;

  /** VBO for missing vertex attribute binding. Avoid undefined behavior on some implementation. */
  GLuint default_attr_vbo_;

  /** Used for debugging purpose. Bit-flags of all bound slots. */
  uint16_t bound_ubo_slots;
  uint16_t bound_ssbo_slots;

 private:
  /**
   * #Batch & #GPUFramebuffer have references to the context they are from, in the case the
   * context is destroyed, we need to remove any reference to it.
   */
  Set<GLVaoCache *> vao_caches_;
  Set<gpu::FrameBuffer *> framebuffers_;
  /** Mutex for the below structures. */
  std::mutex lists_mutex_;
  /** VertexArrays and framebuffers are not shared across context. */
  Vector<GLuint> orphaned_vertarrays_;
  Vector<GLuint> orphaned_framebuffers_;
  /** #GLBackend owns this data. */
  GLSharedOrphanLists &shared_orphan_list_;

  struct TimeQuery {
    std::string name;
    union {
      GLuint handles[2];
      struct {
        GLuint handle_start, handle_end;
      };
    };
    bool finished;
    int64_t cpu_start;
    int64_t cpu_end;
  };
  struct FrameQueries {
    Vector<TimeQuery> queries;
  };
  Vector<FrameQueries> frame_timings;

  void process_frame_timings();

 public:
  GLContext(void *ghost_window, GLSharedOrphanLists &shared_orphan_list);
  ~GLContext();

  static void check_error(const char *info);

  void activate() override;
  void deactivate() override;
  void begin_frame() override;
  void end_frame() override;

  void flush() override;
  void finish() override;

  void memory_statistics_get(int *r_total_mem, int *r_free_mem) override;

  static GLContext *get()
  {
    return static_cast<GLContext *>(Context::get());
  }

  static GLStateManager *state_manager_active_get()
  {
    GLContext *ctx = GLContext::get();
    return static_cast<GLStateManager *>(ctx->state_manager);
  };

  /* These need to be called with the context the id was created with. */
  void vao_free(GLuint vao_id);
  void fbo_free(GLuint fbo_id);
  /* These can be called by any threads even without OpenGL ctx. Deletion will be delayed. */
  static void buffer_free(GLuint buf_id);
  static void texture_free(GLuint tex_id);
  static void shader_free(GLuint shader_id);
  static void program_free(GLuint program_id);

  void vao_cache_register(GLVaoCache *cache);
  void vao_cache_unregister(GLVaoCache *cache);

  void debug_group_begin(const char *name, int index) override;
  void debug_group_end() override;
  bool debug_capture_begin(const char *title) override;
  void debug_capture_end() override;
  void *debug_capture_scope_create(const char *name) override;
  bool debug_capture_scope_begin(void *scope) override;
  void debug_capture_scope_end(void *scope) override;

  void debug_unbind_all_ubo() override;
  void debug_unbind_all_ssbo() override;

 private:
  static void orphans_add(Vector<GLuint> &orphan_list, std::mutex &list_mutex, GLuint id);
  void orphans_clear();

  MEM_CXX_CLASS_ALLOC_FUNCS("GLContext")
};

}  // namespace gpu
}  // namespace blender

/* SPDX-FileCopyrightText: 2016 by Mike Erwin. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Manage GL vertex array IDs in a thread-safe way
 * Use these instead of glGenBuffers & its friends
 * - alloc must be called from a thread that is bound
 *   to the context that will be used for drawing with
 *   this VAO.
 * - free can be called from any thread
 */

#include "BKE_global.hh"

#include "BLI_assert.h"
#include "BLI_threads.h"
#include "BLI_vector_set.hh"

#include "DNA_userdef_types.h"

#include "GHOST_C-api.h"
#include "GHOST_Types.h"

#include "GPU_context.hh"

#include "GPU_batch.hh"
#include "GPU_pass.hh"
#include "gpu_backend.hh"
#include "gpu_context_private.hh"
#include "gpu_matrix_private.hh"
#include "gpu_private.hh"
#include "gpu_shader_private.hh"

#ifdef WITH_VULKAN_BACKEND
#  include "vk_backend.hh"
#endif
#ifdef WITH_OPENGL_BACKEND
#  include "gl_backend.hh"
#  include "gl_context.hh"
#endif
#ifdef WITH_METAL_BACKEND
#  include "mtl_backend.hh"
#endif
#include "dummy_backend.hh"

#include "draw_debug.hh"

#include <mutex>

using namespace blender::gpu;

static thread_local Context *active_ctx = nullptr;

static blender::Mutex backend_users_mutex;
static int num_backend_users = 0;

static void gpu_backend_create();
static void gpu_backend_discard();

/* -------------------------------------------------------------------- */
/** \name gpu::Context methods
 * \{ */

namespace blender::gpu {

int Context::context_counter = 0;
Context::Context()
{
  thread_ = pthread_self();
  is_active_ = false;
  matrix_state = GPU_matrix_state_create();
  texture_pool = new TexturePool();

  context_id = Context::context_counter;
  Context::context_counter++;
}

Context::~Context()
{
  /* Derived class should have called free_resources already. */
  BLI_assert(front_left == nullptr);
  BLI_assert(back_left == nullptr);
  BLI_assert(front_right == nullptr);
  BLI_assert(back_right == nullptr);
  BLI_assert(texture_pool == nullptr);

  /** IMPORTANT: Do not free resources (texture, batch, buffers) in this function. These objects
   * are likely to reference the GL/VK/MTLContext which is already destroyed at this point. */

  GPU_matrix_state_discard(matrix_state);
  delete state_manager;
  delete imm;
}

void Context::free_resources()
{
  delete front_left;
  delete back_left;
  delete front_right;
  delete back_right;
  front_left = nullptr;
  back_left = nullptr;
  front_right = nullptr;
  back_right = nullptr;

  GPU_BATCH_DISCARD_SAFE(procedural_points_batch);
  GPU_BATCH_DISCARD_SAFE(procedural_lines_batch);
  GPU_BATCH_DISCARD_SAFE(procedural_triangles_batch);
  GPU_BATCH_DISCARD_SAFE(procedural_triangle_strips_batch);
  GPU_VERTBUF_DISCARD_SAFE(dummy_vbo);

  delete texture_pool;
  texture_pool = nullptr;
}

bool Context::is_active_on_thread()
{
  return (this == active_ctx) && pthread_equal(pthread_self(), thread_);
}

Context *Context::get()
{
  return active_ctx;
}

VertBuf *Context::dummy_vbo_get()
{
  if (this->dummy_vbo) {
    return this->dummy_vbo;
  }

  /* TODO(fclem): get rid of this dummy VBO. */
  GPUVertFormat format = {0};
  GPU_vertformat_attr_add(&format, "dummy", gpu::VertAttrType::SFLOAT_32);
  this->dummy_vbo = GPU_vertbuf_create_with_format(format);
  GPU_vertbuf_data_alloc(*this->dummy_vbo, 1);
  return this->dummy_vbo;
}

Batch *Context::procedural_points_batch_get()
{
  if (procedural_points_batch) {
    return procedural_points_batch;
  }

  procedural_points_batch = GPU_batch_create(GPU_PRIM_POINTS, dummy_vbo_get(), nullptr);
  return procedural_points_batch;
}

Batch *Context::procedural_lines_batch_get()
{
  if (procedural_lines_batch) {
    return procedural_lines_batch;
  }

  procedural_lines_batch = GPU_batch_create(GPU_PRIM_LINES, dummy_vbo_get(), nullptr);
  return procedural_lines_batch;
}

Batch *Context::procedural_triangles_batch_get()
{
  if (procedural_triangles_batch) {
    return procedural_triangles_batch;
  }

  procedural_triangles_batch = GPU_batch_create(GPU_PRIM_TRIS, dummy_vbo_get(), nullptr);
  return procedural_triangles_batch;
}

Batch *Context::procedural_triangle_strips_batch_get()
{
  if (procedural_triangle_strips_batch) {
    return procedural_triangle_strips_batch;
  }

  procedural_triangle_strips_batch = GPU_batch_create(
      GPU_PRIM_TRI_STRIP, dummy_vbo_get(), nullptr);
  return procedural_triangle_strips_batch;
}

}  // namespace blender::gpu

/** \} */

/* -------------------------------------------------------------------- */

GPUContext *GPU_context_create(void *ghost_window, void *ghost_context)
{
  {
    std::scoped_lock lock(backend_users_mutex);
    if (num_backend_users == 0) {
      /* Automatically create backend when first context is created. */
      gpu_backend_create();
    }
    num_backend_users++;
  }

  Context *ctx = GPUBackend::get()->context_alloc(ghost_window, ghost_context);

  GPU_context_active_set(wrap(ctx));

  blender::draw::DebugDraw::get().acquire();

  return wrap(ctx);
}

void GPU_context_discard(GPUContext *ctx_)
{
  Context *ctx = unwrap(ctx_);
  BLI_assert(active_ctx == ctx);

  blender::draw::DebugDraw::get().release();

  GPUBackend *backend = GPUBackend::get();
  /* Flush any remaining printf while making sure we are inside render boundaries. */
  backend->render_begin();
  printf_end(ctx);
  backend->render_end();

  delete ctx;
  active_ctx = nullptr;

  {
    std::scoped_lock lock(backend_users_mutex);
    num_backend_users--;
    BLI_assert(num_backend_users >= 0);
    if (num_backend_users == 0) {
      /* Discard backend when last context is discarded. */
      gpu_backend_discard();
    }
  }
}

void GPU_context_active_set(GPUContext *ctx_)
{
  Context *ctx = unwrap(ctx_);

  if (active_ctx) {
    GPU_shader_unbind();
    active_ctx->deactivate();
  }

  active_ctx = ctx;

  if (ctx) {
    ctx->activate();
    /* It can happen that the previous context drew with a different color-space.
     * In the case where the new context is drawing with the same shader that was previously bound
     * (shader binding optimization), the uniform would not be set again because the dirty flag
     * would not have been set (since the color space of this new context never changed). The
     * shader would reuse the same color-space as the previous context frame-buffer (see #137855).
     */
    ctx->shader_builtin_srgb_is_dirty = true;
  }
}

GPUContext *GPU_context_active_get()
{
  return wrap(Context::get());
}

void GPU_context_begin_frame(GPUContext *ctx)
{
  blender::gpu::Context *_ctx = unwrap(ctx);
  if (_ctx) {
    _ctx->begin_frame();
  }
}

void GPU_context_end_frame(GPUContext *ctx)
{
  blender::gpu::Context *_ctx = unwrap(ctx);
  if (_ctx) {
    _ctx->end_frame();
  }
}

/* -------------------------------------------------------------------- */
/** \name Main context global mutex
 *
 * Used to avoid crash on some old drivers.
 * \{ */

static blender::Mutex main_context_mutex;

void GPU_context_main_lock()
{
  main_context_mutex.lock();
}

void GPU_context_main_unlock()
{
  main_context_mutex.unlock();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPU Begin/end work blocks
 *
 * Used to explicitly define a per-frame block within which GPU work will happen.
 * Used for global autoreleasepool flushing in Metal
 * \{ */

void GPU_render_begin()
{
  GPUBackend *backend = GPUBackend::get();
  BLI_assert(backend);
  /* WORKAROUND: Currently a band-aid for the heist production. Has no side effect for GL backend
   * but should be fixed for Metal. */
  if (backend) {
    backend->render_begin();
    printf_begin(active_ctx);
  }
}
void GPU_render_end()
{
  GPUBackend *backend = GPUBackend::get();
  BLI_assert(backend);
  if (backend) {
    printf_end(active_ctx);
    backend->render_end();
  }
}
void GPU_render_step(bool force_resource_release)
{
  GPUBackend *backend = GPUBackend::get();
  BLI_assert(backend);
  if (backend) {
    printf_end(active_ctx);
    backend->render_step(force_resource_release);
    printf_begin(active_ctx);
  }

  GPU_pass_cache_update();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backend selection
 * \{ */

static GPUBackendType g_backend_type = GPU_BACKEND_OPENGL;
static std::optional<GPUBackendType> g_backend_type_override = std::nullopt;
static std::optional<bool> g_backend_type_supported = std::nullopt;
static std::optional<int> g_vsync_override = std::nullopt;
static GPUBackend *g_backend = nullptr;
static GHOST_SystemHandle g_ghost_system = nullptr;

void GPU_backend_ghost_system_set(void *ghost_system_handle)
{
  g_ghost_system = reinterpret_cast<GHOST_SystemHandle>(ghost_system_handle);
}

void *GPU_backend_ghost_system_get()
{
  return g_ghost_system;
}

void GPU_backend_type_selection_set(const GPUBackendType backend)
{
  g_backend_type = backend;
  g_backend_type_supported = std::nullopt;
}

int GPU_backend_vsync_get()
{
  return g_vsync_override.value();
}

void GPU_backend_vsync_set_override(const int vsync)
{
  g_vsync_override = vsync;
}

bool GPU_backend_vsync_is_overridden()
{
  return g_vsync_override.has_value();
}

GPUBackendType GPU_backend_type_selection_get()
{
  return g_backend_type;
}

void GPU_backend_type_selection_set_override(const GPUBackendType backend_type)
{
  g_backend_type_override = backend_type;
}

bool GPU_backend_type_selection_is_overridden()
{
  return g_backend_type_override.has_value();
}

bool GPU_backend_type_selection_detect()
{
  blender::VectorSet<GPUBackendType> backends_to_check;
  if (g_backend_type_override.has_value()) {
    backends_to_check.add(*g_backend_type_override);
  }
#if defined(WITH_OPENGL_BACKEND)
  backends_to_check.add(GPU_BACKEND_OPENGL);
#elif defined(WITH_METAL_BACKEND)
  backends_to_check.add(GPU_BACKEND_METAL);
#endif

#if defined(WITH_VULKAN_BACKEND)
  backends_to_check.add(GPU_BACKEND_VULKAN);
#endif

  for (const GPUBackendType backend_type : backends_to_check) {
    GPU_backend_type_selection_set(backend_type);
    if (GPU_backend_supported()) {
      return true;
    }
    G.f |= G_FLAG_GPU_BACKEND_FALLBACK;
  }

  GPU_backend_type_selection_set(GPU_BACKEND_NONE);
  return false;
}

static bool gpu_backend_supported()
{
  switch (g_backend_type) {
    case GPU_BACKEND_OPENGL:
#ifdef WITH_OPENGL_BACKEND
      return true;
#else
      return false;
#endif
    case GPU_BACKEND_VULKAN:
#ifdef WITH_VULKAN_BACKEND
      return VKBackend::is_supported();
#else
      return false;
#endif
    case GPU_BACKEND_METAL:
#ifdef WITH_METAL_BACKEND
      return MTLBackend::metal_is_supported();
#else
      return false;
#endif
    case GPU_BACKEND_NONE:
      return true;
    default:
      BLI_assert(false && "No backend specified");
      return false;
  }
}

bool GPU_backend_supported()
{
  if (!g_backend_type_supported.has_value()) {
    g_backend_type_supported = gpu_backend_supported();
  }
  return *g_backend_type_supported;
}

static void gpu_backend_create()
{
  BLI_assert(g_backend == nullptr);
  BLI_assert(GPU_backend_supported());

  switch (g_backend_type) {
#ifdef WITH_OPENGL_BACKEND
    case GPU_BACKEND_OPENGL:
      g_backend = MEM_new<GLBackend>(__func__);
      break;
#endif
#ifdef WITH_VULKAN_BACKEND
    case GPU_BACKEND_VULKAN:
      g_backend = MEM_new<VKBackend>(__func__);
      break;
#endif
#ifdef WITH_METAL_BACKEND
    case GPU_BACKEND_METAL:
      g_backend = MEM_new<MTLBackend>(__func__);
      break;
#endif
    case GPU_BACKEND_NONE:
      g_backend = MEM_new<DummyBackend>(__func__);
      break;
    default:
      BLI_assert(0);
      break;
  }
}

void gpu_backend_init_resources()
{
  BLI_assert(g_backend);
  g_backend->init_resources();
}

void gpu_backend_delete_resources()
{
  BLI_assert(g_backend);
  g_backend->delete_resources();
}

void gpu_backend_discard()
{
  /* TODO: assert no resource left. */
  MEM_delete(g_backend);
  g_backend = nullptr;
}

GPUBackendType GPU_backend_get_type()
{

#ifdef WITH_OPENGL_BACKEND
  if (g_backend && dynamic_cast<GLBackend *>(g_backend) != nullptr) {
    return GPU_BACKEND_OPENGL;
  }
#endif

#ifdef WITH_METAL_BACKEND
  if (g_backend && dynamic_cast<MTLBackend *>(g_backend) != nullptr) {
    return GPU_BACKEND_METAL;
  }
#endif

#ifdef WITH_VULKAN_BACKEND
  if (g_backend && dynamic_cast<VKBackend *>(g_backend) != nullptr) {
    return GPU_BACKEND_VULKAN;
  }
#endif

  return GPU_BACKEND_NONE;
}

const char *GPU_backend_get_name()
{
  switch (GPU_backend_get_type()) {
    case GPU_BACKEND_OPENGL:
      return "OpenGL";
    case GPU_BACKEND_VULKAN:
      return "Vulkan";
    case GPU_BACKEND_METAL:
      return "Metal";
    case GPU_BACKEND_NONE:
      return "None";
    case GPU_BACKEND_ANY:
      break;
  }

  return "Unknown";
}

GPUBackend *GPUBackend::get()
{
  return g_backend;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUSecondaryContext
 * \{ */

static GHOST_TDrawingContextType ghost_context_type()
{
  switch (GPU_backend_type_selection_get()) {
#ifdef WITH_OPENGL_BACKEND
    case GPU_BACKEND_OPENGL:
      return GHOST_kDrawingContextTypeOpenGL;
#endif
#ifdef WITH_VULKAN_BACKEND
    case GPU_BACKEND_VULKAN:
      return GHOST_kDrawingContextTypeVulkan;
#endif
#ifdef WITH_METAL_BACKEND
    case GPU_BACKEND_METAL:
      return GHOST_kDrawingContextTypeMetal;
#endif
    default:
      BLI_assert_unreachable();
      return GHOST_kDrawingContextTypeNone;
  }
}

GPUSecondaryContext::GPUSecondaryContext()
{
  /* Contexts can only be created on the main thread. */
  BLI_assert(BLI_thread_is_main());

  GHOST_ContextHandle main_thread_ghost_context = GHOST_GetActiveGPUContext();
  GPUContext *main_thread_gpu_context = GPU_context_active_get();

  /* GPU settings for context creation. */
  GHOST_GPUSettings gpu_settings = {0};
  gpu_settings.context_type = ghost_context_type();
  if (G.debug & G_DEBUG_GPU) {
    gpu_settings.flags |= GHOST_gpuDebugContext;
  }
  gpu_settings.preferred_device.index = U.gpu_preferred_index;
  gpu_settings.preferred_device.vendor_id = U.gpu_preferred_vendor_id;
  gpu_settings.preferred_device.device_id = U.gpu_preferred_device_id;

  /* Grab the system handle. */
  GHOST_SystemHandle ghost_system = reinterpret_cast<GHOST_SystemHandle>(
      GPU_backend_ghost_system_get());
  BLI_assert(ghost_system);

  /* Create a Ghost GPU Context using the system handle. */
  ghost_context_ = GHOST_CreateGPUContext(ghost_system, gpu_settings);
  BLI_assert(ghost_context_);

  /* Activate it so GPU_context_create has a valid device for info queries. */
  GHOST_ActivateGPUContext(reinterpret_cast<GHOST_ContextHandle>(ghost_context_));

  /* Create a GPU context for the secondary thread to use. */
  gpu_context_ = GPU_context_create(nullptr, ghost_context_);
  BLI_assert(gpu_context_);

  /* Release the Ghost GPU Context from this thread. */
  GHOST_TSuccess success = GHOST_ReleaseGPUContext(
      reinterpret_cast<GHOST_ContextHandle>(ghost_context_));
  BLI_assert(success);
  UNUSED_VARS_NDEBUG(success);

  /* Restore the main thread contexts.
   * (required as the above context creation also makes it active). */
  GHOST_ActivateGPUContext(main_thread_ghost_context);
  GPU_context_active_set(main_thread_gpu_context);
}

GPUSecondaryContext::~GPUSecondaryContext()
{
  /* Contexts should be destructed on the thread they were activated. */
  BLI_assert(!BLI_thread_is_main());

  GPU_context_discard(gpu_context_);

  GHOST_ReleaseGPUContext(reinterpret_cast<GHOST_ContextHandle>(ghost_context_));

  GHOST_SystemHandle ghost_system = reinterpret_cast<GHOST_SystemHandle>(
      GPU_backend_ghost_system_get());
  BLI_assert(ghost_system);
  GHOST_DisposeGPUContext(ghost_system, reinterpret_cast<GHOST_ContextHandle>(ghost_context_));
}

void GPUSecondaryContext::activate()
{
  /* Contexts need to be activated in the thread they're going to be used. */
  BLI_assert(!BLI_thread_is_main());

  GHOST_ActivateGPUContext(reinterpret_cast<GHOST_ContextHandle>(ghost_context_));
  GPU_context_active_set(gpu_context_);
}

/** \} */

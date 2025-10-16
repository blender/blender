/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * A #blender::gpu::Shader is a container for backend specific shader program.
 */

#pragma once

#include <mutex>
#include <optional>

#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "GPU_common_types.hh"
#include "GPU_shader_builtin.hh"

namespace blender::gpu {
class VertBuf;
class Shader;
}  // namespace blender::gpu

/** Opaque type hiding #blender::gpu::shader::ShaderCreateInfo */
struct GPUShaderCreateInfo;

/* Hardware limit is 16. Position attribute is always needed so we reduce to 15.
 * This makes sure the GPUVertexFormat name buffer does not overflow. */
constexpr static int GPU_MAX_ATTR = 15;

/* Determined by the maximum uniform buffer size divided by chunk size. */
constexpr static int GPU_MAX_UNIFORM_ATTR = 8;

/* -------------------------------------------------------------------- */
/** \name Creation
 * \{ */

/**
 * Preprocess a raw GLSL source to adhere to our backend compatible shader language.
 * Needed if the string was not part of our build system and is used in a #GPUShaderCreateInfo.
 */
std::string GPU_shader_preprocess_source(blender::StringRefNull original);

/**
 * Create a shader using the given #GPUShaderCreateInfo.
 * Can return a null pointer if compilation fails.
 */
blender::gpu::Shader *GPU_shader_create_from_info(const GPUShaderCreateInfo *_info);

/**
 * Same as GPU_shader_create_from_info but will run preprocessor on source strings.
 */
blender::gpu::Shader *GPU_shader_create_from_info_python(const GPUShaderCreateInfo *_info);

/**
 * Create a shader using a named #GPUShaderCreateInfo registered at startup.
 * These are declared inside `*_info.hh` files using the `GPU_SHADER_CREATE_INFO()` macro.
 * They are also expected to have been flagged using `do_static_compilation`.
 * Can return a null pointer if compilation fails.
 */
blender::gpu::Shader *GPU_shader_create_from_info_name(const char *info_name);

/**
 * Fetch a named #GPUShaderCreateInfo registered at startup.
 * These are declared inside `*_info.hh` files using the `GPU_SHADER_CREATE_INFO()` macro.
 * Can return a null pointer if no match is found.
 */
const GPUShaderCreateInfo *GPU_shader_create_info_get(const char *info_name);

/**
 * Error checking for user created shaders.
 * \return true is create info is valid.
 */
bool GPU_shader_create_info_check_error(const GPUShaderCreateInfo *_info, char r_error[128]);

enum class CompilationPriority { Low, Medium, High };

using BatchHandle = int64_t;
/**
 * Request the creation of multiple shaders at once, allowing the backend to use multithreaded
 * compilation. Returns a handle that can be used to poll if all shaders have been compiled, and to
 * retrieve the compiled shaders.
 * NOTE: This function is asynchronous on OpenGL, but it's blocking on Vulkan.
 * WARNING: The GPUShaderCreateInfo pointers should be valid until `GPU_shader_batch_finalize` has
 * returned.
 */
BatchHandle GPU_shader_batch_create_from_infos(
    blender::Span<const GPUShaderCreateInfo *> infos,
    CompilationPriority priority = CompilationPriority::High);
/**
 * Returns true if all the shaders from the batch have finished their compilation.
 */
bool GPU_shader_batch_is_ready(BatchHandle handle);
/**
 * Retrieve the compiled shaders, in the same order as the `GPUShaderCreateInfo`s.
 * If the compilation has not finished yet, this call will block the thread until all the shaders
 * are ready.
 * Shaders with compilation errors are returned as null pointers.
 * WARNING: The handle will be invalidated by this call, you can't request the same batch twice.
 */
blender::Vector<blender::gpu::Shader *> GPU_shader_batch_finalize(BatchHandle &handle);
/**
 * Cancel the compilation of the batch.
 * WARNING: The handle will be invalidated by this call.
 */
void GPU_shader_batch_cancel(BatchHandle &handle);
/**
 *  Returns true if there's any batch still being compiled.
 */
bool GPU_shader_batch_is_compiling();
/**
 *  Wait until all the requested batches have been compiled.
 */
void GPU_shader_batch_wait_for_all();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Free
 * \{ */

void GPU_shader_free(blender::gpu::Shader *shader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Binding
 * \{ */

/**
 * Set the given shader as active shader for the active GPU context.
 * It replaces any already bound shader.
 * All following draw-calls and dispatches will use this shader.
 * Uniform functions need to have the shader bound in order to work. (TODO: until we use
 * glProgramUniform)
 */
void GPU_shader_bind(
    blender::gpu::Shader *shader,
    const blender::gpu::shader::SpecializationConstants *constants_state = nullptr);

/**
 * Unbind the active shader.
 * \note this is a no-op in release builds. But it make sense to actually do it in user land code
 * to detect incorrect API usage.
 */
void GPU_shader_unbind();

/**
 * Return the currently bound shader to the active GPU context.
 * \return null pointer if no shader is bound of if no context is active.
 */
blender::gpu::Shader *GPU_shader_get_bound();

/** \} */

/* -------------------------------------------------------------------- */
/** \name Debugging introspection API.
 * \{ */

const char *GPU_shader_get_name(blender::gpu::Shader *shader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform API.
 * \{ */

/**
 * Returns binding point location.
 * Binding location are given to be set at shader compile time and immutable.
 */
int GPU_shader_get_ubo_binding(blender::gpu::Shader *shader, const char *name);
int GPU_shader_get_ssbo_binding(blender::gpu::Shader *shader, const char *name);
int GPU_shader_get_sampler_binding(blender::gpu::Shader *shader, const char *name);

/**
 * Returns uniform location.
 * If cached, it is faster than querying the interface for each uniform assignment.
 */
int GPU_shader_get_uniform(blender::gpu::Shader *shader, const char *name);

/**
 * Returns specialization constant location.
 */
int GPU_shader_get_constant(blender::gpu::Shader *shader, const char *name);

/**
 * Sets a generic push constant (a.k.a. uniform).
 * \a length and \a array_size should match the create info push_constant declaration.
 */
void GPU_shader_uniform_float_ex(
    blender::gpu::Shader *shader, int location, int length, int array_size, const float *value);
void GPU_shader_uniform_int_ex(
    blender::gpu::Shader *shader, int location, int length, int array_size, const int *value);

/**
 * Sets a generic push constant (a.k.a. uniform).
 * \a length and \a array_size should match the create info push_constant declaration.
 * These functions need to have the shader bound in order to work. (TODO: until we use
 * glProgramUniform)
 */
void GPU_shader_uniform_1i(blender::gpu::Shader *sh, const char *name, int value);
void GPU_shader_uniform_1b(blender::gpu::Shader *sh, const char *name, bool value);
void GPU_shader_uniform_1f(blender::gpu::Shader *sh, const char *name, float value);
void GPU_shader_uniform_2f(blender::gpu::Shader *sh, const char *name, float x, float y);
void GPU_shader_uniform_3f(blender::gpu::Shader *sh, const char *name, float x, float y, float z);
void GPU_shader_uniform_4f(
    blender::gpu::Shader *sh, const char *name, float x, float y, float z, float w);
void GPU_shader_uniform_2fv(blender::gpu::Shader *sh, const char *name, const float data[2]);
void GPU_shader_uniform_3fv(blender::gpu::Shader *sh, const char *name, const float data[3]);
void GPU_shader_uniform_4fv(blender::gpu::Shader *sh, const char *name, const float data[4]);
void GPU_shader_uniform_2iv(blender::gpu::Shader *sh, const char *name, const int data[2]);
void GPU_shader_uniform_3iv(blender::gpu::Shader *sh, const char *name, const int data[3]);
void GPU_shader_uniform_mat4(blender::gpu::Shader *sh, const char *name, const float data[4][4]);
void GPU_shader_uniform_mat3_as_mat4(blender::gpu::Shader *sh,
                                     const char *name,
                                     const float data[3][3]);
void GPU_shader_uniform_1f_array(blender::gpu::Shader *sh,
                                 const char *name,
                                 int len,
                                 const float *val);
void GPU_shader_uniform_2fv_array(blender::gpu::Shader *sh,
                                  const char *name,
                                  int len,
                                  const float (*val)[2]);
void GPU_shader_uniform_4fv_array(blender::gpu::Shader *sh,
                                  const char *name,
                                  int len,
                                  const float (*val)[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute API.
 *
 * Used to create #GPUVertexFormat from the shader's vertex input layout.
 * \{ */

uint GPU_shader_get_attribute_len(const blender::gpu::Shader *shader);
uint GPU_shader_get_ssbo_input_len(const blender::gpu::Shader *shader);
int GPU_shader_get_attribute(const blender::gpu::Shader *shader, const char *name);
bool GPU_shader_get_attribute_info(const blender::gpu::Shader *shader,
                                   int attr_location,
                                   char r_name[256],
                                   int *r_type);
bool GPU_shader_get_ssbo_input_info(const blender::gpu::Shader *shader,
                                    int ssbo_location,
                                    char r_name[256]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Specialization API.
 *
 * Used to allow specialization constants.
 * IMPORTANT: All constants must be specified before binding a shader that needs specialization.
 * Otherwise, it will produce undefined behavior.
 * \{ */

/* Return the default constants.
 * All constants available for this shader should fit the returned structure. */
const blender::gpu::shader::SpecializationConstants &GPU_shader_get_default_constant_state(
    blender::gpu::Shader *sh);

using SpecializationBatchHandle = int64_t;

struct ShaderSpecialization {
  blender::gpu::Shader *shader;
  blender::gpu::shader::SpecializationConstants constants;
};

/**
 * Request the compilation of multiple specialization constant variations at once,
 * allowing the backend to use multi-threaded compilation.
 * Returns a handle that can be used to poll if all variations have been compiled.
 * A NULL handle indicates no compilation of any variant was possible (likely due to
 * some state being currently available) and so no batch was created. Compilation
 * of the specialized variant will instead occur at draw/dispatch time.
 * NOTE: This function is asynchronous on OpenGL and Metal and a no-op on Vulkan.
 * Batches are processed one by one in FIFO order.
 * WARNING: Binding a specialization before the batch finishes will fail.
 */
SpecializationBatchHandle GPU_shader_batch_specializations(
    blender::Span<ShaderSpecialization> specializations,
    CompilationPriority priority = CompilationPriority::High);

/**
 * Returns true if all the specializations from the batch have finished their compilation.
 * NOTE: Polling this function is required for the compilation process to keep progressing.
 * WARNING: Invalidates the handle if it returns true.
 */
bool GPU_shader_batch_specializations_is_ready(SpecializationBatchHandle &handle);

/**
 * Cancel the specialization batch.
 * WARNING: The handle will be invalidated by this call.
 */
void GPU_shader_batch_specializations_cancel(SpecializationBatchHandle &handle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Legacy API
 *
 * All of this section is deprecated and should be ported to use the API described above.
 * \{ */

/**
 * Shader cache warming.
 * For each shader, rendering APIs perform a two-step compilation:
 *
 *  * The first stage is Front-End compilation which only needs to be performed once, and generates
 * a portable intermediate representation. This happens during `gpu::Shader::finalize()`.
 *
 *  * The second is Back-End compilation which compiles a device-specific executable shader
 * program. This compilation requires some contextual pipeline state which is baked into the
 * executable shader source, producing a Pipeline State Object (PSO). In OpenGL, backend
 * compilation happens in the background, within the driver, but can still incur runtime stutters.
 * In Metal/Vulkan, PSOs are compiled explicitly. These are currently resolved within the backend
 * based on the current pipeline state and can incur runtime stalls when they occur.
 *
 * Shader Cache warming uses the specified parent shader set using `GPU_shader_set_parent(..)` as a
 * template reference for pre-compiling Render Pipeline State Objects (PSOs) outside of the main
 * render pipeline.
 *
 * PSOs require descriptors containing information on the render state for a given shader, which
 * includes input vertex data layout and output pixel formats, along with some state such as
 * blend mode and color output masks. As this state information is usually consistent between
 * similar draws, we can assign a parent shader and use this shader's cached pipeline state's to
 * prime compilations.
 *
 * Shaders do not necessarily have to be similar in functionality to be used as a parent, so long
 * as the #GPUVertFormat and #gpu::FrameBuffer which they are used with remain the same.
 * Other bindings such as textures, uniforms and UBOs are all assigned independently as dynamic
 * state.
 *
 * This function should be called asynchronously, mitigating the impact of run-time stuttering from
 * dynamic compilation of PSOs during normal rendering.
 *
 * \param: shader: The shader whose cache to warm.
 * \param limit: The maximum number of PSOs to compile within a call. Specifying
 * a limit <= 0 will compile a PSO for all cached PSOs in the parent shader. */
void GPU_shader_warm_cache(blender::gpu::Shader *shader, int limit);

/* We expect the parent shader to be compiled and already have some cached PSOs when being assigned
 * as a reference. Ensure the parent shader still exists when `GPU_shader_cache_warm(..)` is
 * called. */
void GPU_shader_set_parent(blender::gpu::Shader *shader, blender::gpu::Shader *parent);

/**
 * Indexed commonly used uniform name for faster lookup into the uniform cache.
 */
enum GPUUniformBuiltin {
  GPU_UNIFORM_MODEL = 0,      /* mat4 ModelMatrix */
  GPU_UNIFORM_VIEW,           /* mat4 ViewMatrix */
  GPU_UNIFORM_MODELVIEW,      /* mat4 ModelViewMatrix */
  GPU_UNIFORM_PROJECTION,     /* mat4 ProjectionMatrix */
  GPU_UNIFORM_VIEWPROJECTION, /* mat4 ViewProjectionMatrix */
  GPU_UNIFORM_MVP,            /* mat4 ModelViewProjectionMatrix */

  GPU_UNIFORM_MODEL_INV,          /* mat4 ModelMatrixInverse */
  GPU_UNIFORM_VIEW_INV,           /* mat4 ViewMatrixInverse */
  GPU_UNIFORM_MODELVIEW_INV,      /* mat4 ModelViewMatrixInverse */
  GPU_UNIFORM_PROJECTION_INV,     /* mat4 ProjectionMatrixInverse */
  GPU_UNIFORM_VIEWPROJECTION_INV, /* mat4 ViewProjectionMatrixInverse */

  GPU_UNIFORM_NORMAL,     /* mat3 NormalMatrix */
  GPU_UNIFORM_CLIPPLANES, /* vec4 WorldClipPlanes[] */

  GPU_UNIFORM_COLOR,              /* vec4 color */
  GPU_UNIFORM_BASE_INSTANCE,      /* int baseInstance */
  GPU_UNIFORM_RESOURCE_CHUNK,     /* int resourceChunk */
  GPU_UNIFORM_RESOURCE_ID,        /* int resourceId */
  GPU_UNIFORM_SRGB_TRANSFORM,     /* bool srgbTarget */
  GPU_UNIFORM_SCENE_LINEAR_XFORM, /* float3x3 gpu_scene_linear_to_xyz */
};
#define GPU_NUM_UNIFORMS (GPU_UNIFORM_SCENE_LINEAR_XFORM + 1)

/**
 * TODO: To be moved as private API. Not really used outside of gpu_matrix.cc and doesn't really
 * offer a noticeable performance boost.
 */
int GPU_shader_get_builtin_uniform(blender::gpu::Shader *shader, int builtin);

/**
 * Compile all statically defined shaders and print a report to the console.
 *
 * This is used for platform support, where bug reports can list all failing shaders.
 */
void GPU_shader_compile_static();

void GPU_shader_cache_dir_clear_old();

/** DEPRECATED: Use hard-coded buffer location instead. */
enum GPUUniformBlockBuiltin {
  GPU_UNIFORM_BLOCK_VIEW = 0, /* viewBlock */
  GPU_UNIFORM_BLOCK_MODEL,    /* modelBlock */
  GPU_UNIFORM_BLOCK_INFO,     /* infoBlock */

  GPU_UNIFORM_BLOCK_DRW_VIEW,
  GPU_UNIFORM_BLOCK_DRW_MODEL,
  GPU_UNIFORM_BLOCK_DRW_INFOS,
  GPU_UNIFORM_BLOCK_DRW_CLIPPING,

  GPU_NUM_UNIFORM_BLOCKS, /* Special value, denotes number of builtin uniforms block. */
};

/** DEPRECATED: Kept only because of Python GPU API. */
int GPU_shader_get_uniform_block(blender::gpu::Shader *shader, const char *name);

/** \} */

#define GPU_SHADER_FREE_SAFE(shader) \
  do { \
    if (shader != nullptr) { \
      GPU_shader_free(shader); \
      shader = nullptr; \
    } \
  } while (0)

#include "BLI_utility_mixins.hh"
#include <atomic>
#include <mutex>

namespace blender::gpu {

/* blender::gpu::Shader wrapper that makes compilation threadsafe.
 * The compilation is deferred until the first get() call.
 * Concurrently using the shader from multiple threads is still unsafe. */
class StaticShader : NonCopyable {
 private:
  std::string info_name_;
  std::atomic<blender::gpu::Shader *> shader_ = nullptr;
  /* TODO: Failed compilation detection should be supported by the blender::gpu::Shader API. */
  std::atomic<bool> failed_ = false;
  std::mutex mutex_;
  /* Handle for async compilation. */
  BatchHandle compilation_handle_ = 0;

  void move(StaticShader &&other)
  {
    std::scoped_lock lock1(mutex_);
    std::scoped_lock lock2(other.mutex_);
    BLI_assert(shader_ == nullptr && info_name_.empty());
    std::swap(info_name_, other.info_name_);
    /* No std::swap support for atomics. */
    shader_.exchange(other.shader_.exchange(shader_));
    failed_.exchange(other.failed_.exchange(failed_));
    std::swap(compilation_handle_, other.compilation_handle_);
  }

 public:
  StaticShader(std::string info_name) : info_name_(info_name) {}

  StaticShader() = default;
  StaticShader(StaticShader &&other)
  {
    move(std::move(other));
  }
  StaticShader &operator=(StaticShader &&other)
  {
    move(std::move(other));
    return *this;
  };

  ~StaticShader()
  {
    if (compilation_handle_) {
      GPU_shader_batch_cancel(compilation_handle_);
    }
    GPU_SHADER_FREE_SAFE(shader_);
  }

  /* Schedule the shader to be compile in a worker thread. */
  void ensure_compile_async()
  {
    if (is_ready()) {
      return;
    }

    std::scoped_lock lock(mutex_);

    if (compilation_handle_) {
      if (GPU_shader_batch_is_ready(compilation_handle_)) {
        shader_ = GPU_shader_batch_finalize(compilation_handle_)[0];
        failed_ = shader_ == nullptr;
      }
      return;
    }

    if (!shader_ && !failed_ && !compilation_handle_) {
      BLI_assert(!info_name_.empty());
      const GPUShaderCreateInfo *create_info = GPU_shader_create_info_get(info_name_.c_str());
      compilation_handle_ = GPU_shader_batch_create_from_infos({&create_info, 1});
    }
  }

  bool is_ready()
  {
    return shader_ || failed_;
  }

  blender::gpu::Shader *get()
  {
    if (is_ready()) {
      return shader_;
    }

    std::scoped_lock lock(mutex_);

    if (!shader_ && !failed_) {
      if (compilation_handle_) {
        shader_ = GPU_shader_batch_finalize(compilation_handle_)[0];
      }
      else {
        BLI_assert(!info_name_.empty());
        shader_ = GPU_shader_create_from_info_name(info_name_.c_str());
      }
      failed_ = shader_ == nullptr;
    }

    return shader_;
  }

  /* For batch compiled shaders. */
  /* TODO: Find a better way to handle this. */
  void set(blender::gpu::Shader *shader)
  {
    std::scoped_lock lock(mutex_);
    BLI_assert(shader_ == nullptr);
    shader_ = shader;
  }
};

/* Thread-safe container for StaticShader cache classes.
 * The class instance creation is deferred until the first get() call. */
template<typename T> class StaticShaderCache {
  std::atomic<T *> cache_ = nullptr;
  std::mutex mutex_;

 public:
  ~StaticShaderCache()
  {
    BLI_assert(cache_ == nullptr);
  }

  template<typename... Args> T &get(Args &&...constructor_args)
  {
    if (cache_) {
      return *cache_;
    }

    std::lock_guard lock(mutex_);

    if (cache_ == nullptr) {
      cache_ = new T(std::forward<Args>(constructor_args)...);
    }
    return *cache_;
  }

  void release()
  {
    if (!cache_) {
      return;
    }

    std::lock_guard lock(mutex_);

    if (cache_) {
      delete cache_;
      cache_ = nullptr;
    }
  }

  std::lock_guard<std::mutex> lock_guard()
  {
    return std::lock_guard(mutex_);
  }
};

}  // namespace blender::gpu

/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "MEM_guardedalloc.h"

#include <epoxy/gl.h>

#include "BLI_map.hh"
#include "BLI_subprocess.hh"
#include "BLI_utility_mixins.hh"

#include "GPU_capabilities.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"

#include <functional>
#include <mutex>

namespace blender::gpu {

/**
 * Shaders that uses specialization constants must keep track of the sources in order to rebuild
 * shader stages.
 *
 * Some sources are shared and won't be copied. For example for dependencies. In this case we
 * would only store the source_ref.
 *
 * Other sources would be stored in the #source attribute. #source_ref
 * would still be updated.
 */
struct GLSource {
  std::string source;
  std::optional<StringRefNull> source_ref;

  GLSource() = default;
  GLSource(StringRefNull other_source);
};
class GLSources : public Vector<GLSource> {
 public:
  GLSources &operator=(Span<StringRefNull> other);
  Vector<StringRefNull> sources_get() const;
  std::string to_string() const;
};

/**
 * The full sources for each shader stage, baked into a single string from their respective
 * GLSources. (Can be retrieved from GLShader::get_sources())
 */
struct GLSourcesBaked : NonCopyable {
  std::string comp;
  std::string vert;
  std::string geom;
  std::string frag;

  /* Returns the size (in bytes) required to store the source of all the used stages. */
  size_t size();
};

/**
 * Implementation of shader compilation and uniforms handling using OpenGL.
 */
class GLShader : public Shader {
  friend shader::ShaderCreateInfo;
  friend shader::StageInterfaceInfo;
  friend class GLSubprocessShaderCompiler;
  friend class GLShaderCompiler;

 private:
  struct GLProgram {
    /** Handle for program. */
    GLuint program_id = 0;
    /** Handle for individual shader stages. */
    GLuint vert_shader = 0;
    GLuint geom_shader = 0;
    GLuint frag_shader = 0;
    GLuint compute_shader = 0;

    std::mutex compilation_mutex;

    GLProgram() {}
    ~GLProgram();

    void program_link(StringRefNull shader_name);
  };

  using GLProgramCacheKey = Vector<shader::SpecializationConstant::Value>;
  /** Contains all specialized shader variants. */
  Map<GLProgramCacheKey, std::unique_ptr<GLProgram>> program_cache_;

  std::mutex program_cache_mutex_;

  /** Main program instance. This is the default specialized variant that is first compiled. */
  GLProgram *main_program_ = nullptr;

  /* When true, the shader generates its GLSources but it's not compiled.
   * (Used for batch compilation) */
  bool async_compilation_ = false;

  /**
   * When the shader uses Specialization Constants these attribute contains the sources to
   * rebuild shader stages. When Specialization Constants aren't used they are empty to
   * reduce memory needs.
   */
  GLSources vertex_sources_;
  GLSources geometry_sources_;
  GLSources fragment_sources_;
  GLSources compute_sources_;

  Vector<const char *> specialization_constant_names_;

  void update_program_and_sources(GLSources &stage_sources, MutableSpan<StringRefNull> sources);

  /**
   * Return a GLProgram that reflects the given `constants_state`.
   * The returned program_id is in linked state, or an error happened during linking.
   */
  GLShader::GLProgram &program_get(const shader::SpecializationConstants *constants_state);

  /** True if any shader failed to compile. */
  bool compilation_failed_ = false;

  std::string debug_source;

 public:
  GLShader(const char *name);
  ~GLShader();

  void init(const shader::ShaderCreateInfo &info, bool is_batch_compilation) override;

  const shader::ShaderCreateInfo &patch_create_info(
      const shader::ShaderCreateInfo &original_info) override
  {
    return original_info;
  }

  /** Return true on success. */
  void vertex_shader_from_glsl(const shader::ShaderCreateInfo &info,
                               MutableSpan<StringRefNull> sources) override;
  void geometry_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                 MutableSpan<StringRefNull> sources) override;
  void fragment_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                 MutableSpan<StringRefNull> sources) override;
  void compute_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                MutableSpan<StringRefNull> sources) override;
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  bool post_finalize(const shader::ShaderCreateInfo *info = nullptr);
  void warm_cache(int /*limit*/) override {};

  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string constants_declare(const shader::SpecializationConstants &constants_state) const;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;

  void bind(const shader::SpecializationConstants *constants_state) override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;

  bool is_compute() const
  {
    if (!vertex_sources_.is_empty()) {
      return false;
    }
    if (!compute_sources_.is_empty()) {
      return true;
    }
    return main_program_->compute_shader != 0;
  }

  GLSourcesBaked get_sources();

 private:
  StringRefNull glsl_patch_get(GLenum gl_stage);

  bool has_specialization_constants() const
  {
    return constants->types.is_empty() == false;
  }

  /** Create, compile and attach the shader stage to the shader program. */
  GLuint create_shader_stage(GLenum gl_stage,
                             MutableSpan<StringRefNull> sources,
                             GLSources &gl_sources,
                             const shader::SpecializationConstants &constants_state);

  /**
   * \brief features available on newer implementation such as native barycentric coordinates
   * and layered rendering, necessitate a geometry shader to work on older hardware.
   */
  std::string workaround_geometry_shader_source_create(const shader::ShaderCreateInfo &info);

  bool do_geometry_shader_injection(const shader::ShaderCreateInfo *info) const;

  MEM_CXX_CLASS_ALLOC_FUNCS("GLShader");
};

class GLShaderCompiler : public ShaderCompiler {
 public:
  GLShaderCompiler()
      : ShaderCompiler(GPU_max_parallel_compilations(), GPUWorker::ContextType::PerThread, true) {
        };

  virtual void specialize_shader(ShaderSpecialization &specialization) override;
};

#if BLI_SUBPROCESS_SUPPORT

class GLCompilerWorker {
  friend class GLSubprocessShaderCompiler;

 private:
  BlenderSubprocess subprocess_;
  std::unique_ptr<SharedMemory> shared_mem_;
  std::unique_ptr<SharedSemaphore> start_semaphore_;
  std::unique_ptr<SharedSemaphore> end_semaphore_;
  std::unique_ptr<SharedSemaphore> close_semaphore_;
  enum State {
    /* The worker has been acquired and the compilation has been requested. */
    COMPILATION_REQUESTED,
    /* The shader binary result is ready to be read. */
    COMPILATION_READY,
    /* The binary result has been loaded into a program and the worker can be released. */
    COMPILATION_FINISHED,
    /* The worker is not currently in use and can be acquired. */
    AVAILABLE
  };
  std::atomic<State> state_ = AVAILABLE;
  double compilation_start = 0;

  GLCompilerWorker();
  ~GLCompilerWorker();

  void compile(const GLSourcesBaked &sources);
  bool block_until_ready();
  bool load_program_binary(GLint program);
  void release();

  /* Check if the process may have closed/crashed/hanged. */
  bool is_lost();
};

class GLSubprocessShaderCompiler : public ShaderCompiler {
 private:
  Vector<GLCompilerWorker *> workers_;
  std::mutex workers_mutex_;

  GLCompilerWorker *get_compiler_worker();

  GLShader::GLProgram *specialization_program_get(ShaderSpecialization &specialization);

 public:
  GLSubprocessShaderCompiler()
      : ShaderCompiler(GPU_max_parallel_compilations(), GPUWorker::ContextType::PerThread, true) {
        };
  virtual ~GLSubprocessShaderCompiler() override;

  virtual Shader *compile_shader(const shader::ShaderCreateInfo &info) override;
  virtual void specialize_shader(ShaderSpecialization &specialization) override;
};

#else

class GLSubprocessShaderCompiler : public ShaderCompiler {};

#endif

class GLLogParser : public GPULogParser {
 public:
  const char *parse_line(const char *source_combined,
                         const char *log_line,
                         GPULogItem &log_item) override;

 protected:
  const char *skip_severity_prefix(const char *log_line, GPULogItem &log_item);
  const char *skip_severity_keyword(const char *log_line, GPULogItem &log_item);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLLogParser");
};

}  // namespace blender::gpu

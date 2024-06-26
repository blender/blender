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

#include "gpu_shader_create_info.hh"
#include "gpu_shader_private.hh"

#include <functional>

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
  const char *source_ref;

  GLSource() = default;
  GLSource(const char *other_source);
};
class GLSources : public Vector<GLSource> {
 public:
  GLSources &operator=(Span<const char *> other);
  Vector<const char *> sources_get() const;
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

    GLProgram() {}
    GLProgram(GLProgram &&other)
    {
      program_id = other.program_id;
      vert_shader = other.vert_shader;
      geom_shader = other.geom_shader;
      frag_shader = other.frag_shader;
      compute_shader = other.compute_shader;
      other.program_id = 0;
      other.vert_shader = 0;
      other.geom_shader = 0;
      other.frag_shader = 0;
      other.compute_shader = 0;
    }
    ~GLProgram();
  };

  using GLProgramCacheKey = Vector<shader::SpecializationConstant::Value>;
  Map<GLProgramCacheKey, GLProgram> program_cache_;

  /**
   * Points to the active program. When binding a shader the active program is
   * setup.
   */
  GLProgram *program_active_ = nullptr;

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

  /**
   * Initialize an this instance.
   *
   * - Ensures that program_cache at least has a default GLProgram.
   * - Ensures that active program is set.
   * - Active GLProgram has a shader_program (at least in creation state).
   * - Does nothing when instance was already initialized.
   */
  void init_program();

  void update_program_and_sources(GLSources &stage_sources, MutableSpan<const char *> sources);

  /**
   * Link the active program.
   */
  void program_link();
  bool check_link_status();

  /**
   * Return a GLProgram program id that reflects the current state of shader.constants.values.
   * The returned program_id is in linked state, or an error happened during linking.
   */
  GLuint program_get();

  /** True if any shader failed to compile. */
  bool compilation_failed_ = false;

  eGPUShaderTFBType transform_feedback_type_ = GPU_SHADER_TFB_NONE;

  std::string debug_source;

 public:
  GLShader(const char *name);
  ~GLShader();

  void init(const shader::ShaderCreateInfo &info, bool is_batch_compilation) override;

  /** Return true on success. */
  void vertex_shader_from_glsl(MutableSpan<const char *> sources) override;
  void geometry_shader_from_glsl(MutableSpan<const char *> sources) override;
  void fragment_shader_from_glsl(MutableSpan<const char *> sources) override;
  void compute_shader_from_glsl(MutableSpan<const char *> sources) override;
  bool finalize(const shader::ShaderCreateInfo *info = nullptr) override;
  bool post_finalize(const shader::ShaderCreateInfo *info = nullptr);
  void warm_cache(int /*limit*/) override{};

  std::string resources_declare(const shader::ShaderCreateInfo &info) const override;
  std::string constants_declare() const;
  std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const override;
  std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const override;
  std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const override;

  /** Should be called before linking. */
  void transform_feedback_names_set(Span<const char *> name_list,
                                    eGPUShaderTFBType geom_type) override;
  bool transform_feedback_enable(VertBuf *buf) override;
  void transform_feedback_disable() override;

  void bind() override;
  void unbind() override;

  void uniform_float(int location, int comp_len, int array_size, const float *data) override;
  void uniform_int(int location, int comp_len, int array_size, const int *data) override;

  /* Unused: SSBO vertex fetch draw parameters. */
  bool get_uses_ssbo_vertex_fetch() const override
  {
    return false;
  }
  int get_ssbo_vertex_fetch_output_num_verts() const override
  {
    return 0;
  }

  /** DEPRECATED: Kept only because of BGL API. */
  int program_handle_get() const override;

  bool is_compute() const
  {
    if (!vertex_sources_.is_empty()) {
      return false;
    }
    if (!compute_sources_.is_empty()) {
      return true;
    }
    return program_active_->compute_shader != 0;
  }

  GLSourcesBaked get_sources();

 private:
  const char *glsl_patch_get(GLenum gl_stage);

  /** Create, compile and attach the shader stage to the shader program. */
  GLuint create_shader_stage(GLenum gl_stage,
                             MutableSpan<const char *> sources,
                             GLSources &gl_sources);

  /**
   * \brief features available on newer implementation such as native barycentric coordinates
   * and layered rendering, necessitate a geometry shader to work on older hardware.
   */
  std::string workaround_geometry_shader_source_create(const shader::ShaderCreateInfo &info);

  bool do_geometry_shader_injection(const shader::ShaderCreateInfo *info);

  MEM_CXX_CLASS_ALLOC_FUNCS("GLShader");
};

#if BLI_SUBPROCESS_SUPPORT

class GLCompilerWorker {
  friend class GLShaderCompiler;

 private:
  BlenderSubprocess subprocess_;
  std::unique_ptr<SharedMemory> shared_mem_;
  std::unique_ptr<SharedSemaphore> start_semaphore_;
  std::unique_ptr<SharedSemaphore> end_semaphore_;
  std::unique_ptr<SharedSemaphore> close_semaphore_;
  enum eState {
    /* The worker has been acquired and the compilation has been requested. */
    COMPILATION_REQUESTED,
    /* The shader binary result is ready to be read. */
    COMPILATION_READY,
    /* The binary result has been loaded into a program and the worker can be released. */
    COMPILATION_FINISHED,
    /* The worker is not currently in use and can be acquired. */
    AVAILABLE
  };
  eState state_ = AVAILABLE;
  double compilation_start = 0;

  GLCompilerWorker();
  ~GLCompilerWorker();

  void compile(const GLSourcesBaked &sources);
  bool is_ready();
  bool load_program_binary(GLint program);
  void release();

  /* Check if the process may have closed/crashed/hanged. */
  bool is_lost();
};

class GLShaderCompiler : public ShaderCompiler {
 private:
  std::mutex mutex_;
  Vector<GLCompilerWorker *> workers_;

  struct CompilationWork {
    const shader::ShaderCreateInfo *info = nullptr;
    GLShader *shader = nullptr;
    GLSourcesBaked sources;

    GLCompilerWorker *worker = nullptr;
    bool do_async_compilation = false;
    bool is_ready = false;
  };

  struct Batch {
    Vector<CompilationWork> items;
    bool is_ready = false;
  };

  Map<BatchHandle, Batch> batches;

  struct SpecializationRequest {
    BatchHandle handle;
    Vector<ShaderSpecialization> specializations;
  };

  Vector<SpecializationRequest> specialization_queue;

  struct SpecializationWork {
    GLShader *shader = nullptr;
    GLShader::GLProgram *program = nullptr;
    GLSourcesBaked sources;

    GLCompilerWorker *worker = nullptr;
    bool do_async_compilation = false;
    bool is_ready = false;
  };

  struct SpecializationBatch {
    SpecializationBatchHandle handle = 0;
    Vector<SpecializationWork> items;
    bool is_ready = true;
  };

  SpecializationBatch current_specialization_batch;
  void prepare_next_specialization_batch();

  /* Shared across regular and specialization batches,
   * to prevent the use of a wrong handle type. */
  int64_t next_batch_handle = 1;

  GLCompilerWorker *get_compiler_worker(const GLSourcesBaked &sources);
  bool worker_is_lost(GLCompilerWorker *&worker);

 public:
  virtual ~GLShaderCompiler() override;

  virtual BatchHandle batch_compile(Span<const shader::ShaderCreateInfo *> &infos) override;
  virtual bool batch_is_ready(BatchHandle handle) override;
  virtual Vector<Shader *> batch_finalize(BatchHandle &handle) override;

  virtual SpecializationBatchHandle precompile_specializations(
      Span<ShaderSpecialization> specializations) override;

  virtual bool specialization_batch_is_ready(SpecializationBatchHandle &handle) override;
};

#else

class GLShaderCompiler : public ShaderCompilerGeneric {};

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

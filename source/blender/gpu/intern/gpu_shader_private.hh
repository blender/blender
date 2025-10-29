/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "GPU_shader.hh"
#include "GPU_worker.hh"
#include "gpu_shader_create_info.hh"
#include "gpu_shader_interface.hh"

#include <deque>
#include <string>

namespace blender::gpu {

class GPULogParser;
class Context;

/* Set to 1 to log the full source of shaders that fail to compile. */
#define DEBUG_LOG_SHADER_SRC_ON_ERROR 0

/**
 * Compilation is done on a list of GLSL sources. This list contains placeholders that should be
 * provided by the backend shader. These defines contains the locations where the backend can patch
 * the sources.
 */
#define SOURCES_INDEX_VERSION 0
#define SOURCES_INDEX_SPECIALIZATION_CONSTANTS 1

struct PatchedShaderCreateInfo {
  shader::ShaderCreateInfo info;
  shader::ShaderCreateInfoStringCache names;

  PatchedShaderCreateInfo(const shader::ShaderCreateInfo &info_) : info(info_) {}
};

/**
 * Implementation of shader compilation and uniforms handling.
 * Base class which is then specialized for each implementation (GL, VK, ...).
 */
class Shader {
 public:
  /** Uniform & attribute locations for shader. */
  ShaderInterface *interface = nullptr;
  /** Bit-set indicating the frame-buffer color attachments that this shader writes to. */
  uint16_t fragment_output_bits = 0;

  /* Default specialization constants state as defined inside ShaderCreateInfo.
   * Should be considered as const after init(). */
  std::unique_ptr<const shader::SpecializationConstants> constants;

  /* WORKAROUND: True if this shader is a polyline shader and needs an appropriate setup to render.
   * Eventually, in the future, we should modify the user code instead of relying on such hacks. */
  bool is_polyline = false;

 protected:
  /** For debugging purpose. */
  char name[64];

  /* Parent shader can be used for shaders which are derived from the same source material.
   * The child shader can pull information from its parent to prepare additional resources
   * such as PSOs upfront. This enables asynchronous PSO compilation which mitigates stuttering
   * when updating new materials. */
  Shader *parent_shader_ = nullptr;

  /* In some situation, a backend might want to transform the create infos before it is being
   * parsed. */
  std::unique_ptr<PatchedShaderCreateInfo> patched_info_;

 public:
  Shader(const char *name);
  virtual ~Shader();

  /* TODO: Remove `is_batch_compilation`. */
  virtual void init(const shader::ShaderCreateInfo &info, bool is_batch_compilation) = 0;

  /* Patch create infos for any additional resources that could be needed. */
  virtual const shader::ShaderCreateInfo &patch_create_info(
      const shader::ShaderCreateInfo &original_info) = 0;

  virtual void vertex_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                       MutableSpan<StringRefNull> sources) = 0;
  virtual void geometry_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                         MutableSpan<StringRefNull> sources) = 0;
  virtual void fragment_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                         MutableSpan<StringRefNull> sources) = 0;
  virtual void compute_shader_from_glsl(const shader::ShaderCreateInfo &info,
                                        MutableSpan<StringRefNull> sources) = 0;
  virtual bool finalize(const shader::ShaderCreateInfo *info = nullptr) = 0;
  /* Pre-warms PSOs using parent shader's cached PSO descriptors. Limit specifies maximum PSOs to
   * warm. If -1, compiles all PSO permutations in parent shader.
   *
   * See `GPU_shader_warm_cache(..)` in `GPU_shader.hh` for more information. */
  virtual void warm_cache(int limit) = 0;

  virtual void bind(const shader::SpecializationConstants *constants_state) = 0;
  virtual void unbind() = 0;

  virtual void uniform_float(int location, int comp_len, int array_size, const float *data) = 0;
  virtual void uniform_int(int location, int comp_len, int array_size, const int *data) = 0;

  /* Add specialization constant declarations to shader instance. */
  void specialization_constants_init(const shader::ShaderCreateInfo &info);

  static std::string defines_declare(const shader::ShaderCreateInfo &info);
  virtual std::string resources_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string vertex_interface_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string fragment_interface_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string geometry_interface_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string geometry_layout_declare(const shader::ShaderCreateInfo &info) const = 0;
  virtual std::string compute_layout_declare(const shader::ShaderCreateInfo &info) const = 0;

  StringRefNull name_get() const
  {
    return name;
  }

  void parent_set(Shader *parent)
  {
    parent_shader_ = parent;
  }

  Shader *parent_get() const
  {
    return parent_shader_;
  }

  static void set_scene_linear_to_xyz_uniform(gpu::Shader *shader);
  static void set_srgb_uniform(Context *ctx, gpu::Shader *shader);
  static void set_framebuffer_srgb_target(int use_srgb_to_linear);

 protected:
  void print_log(Span<StringRefNull> sources,
                 const char *log,
                 const char *stage,
                 bool error,
                 GPULogParser *parser);
};

class ShaderCompiler {
  struct Sources {
    std::string vert;
    std::string geom;
    std::string frag;
    std::string comp;
  };

  struct Batch {
    Vector<Shader *> shaders;
    Vector<const shader::ShaderCreateInfo *> infos;

    Vector<ShaderSpecialization> specializations;

    std::atomic<int> pending_compilations = 0;

    bool is_specialization_batch()
    {
      return !specializations.is_empty();
    }

    bool is_ready()
    {
      BLI_assert(pending_compilations >= 0);
      return pending_compilations == 0;
    }

    void free_shaders()
    {
      for (Shader *shader : shaders) {
        if (shader) {
          GPU_shader_free(shader);
        }
      }
      shaders.clear();
    }
  };
  Map<BatchHandle, Batch *> batches_;
  std::mutex mutex_;
  std::condition_variable compilation_finished_notification_;

  struct ParallelWork {
    Batch *batch = nullptr;
    int shader_index = 0;
  };

  struct CompilationQueue {
    std::deque<ParallelWork> low_priority;
    std::deque<ParallelWork> normal_priority;
    std::deque<ParallelWork> high_priority;

    void push(ParallelWork &&work, CompilationPriority priority)
    {
      switch (priority) {
        case CompilationPriority::Low:
          low_priority.push_back(work);
          break;
        case CompilationPriority::Medium:
          normal_priority.push_back(work);
          break;
        case CompilationPriority::High:
          high_priority.push_back(work);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
    }

    ParallelWork pop()
    {
      if (!high_priority.empty()) {
        ParallelWork work = high_priority.front();
        high_priority.pop_front();
        return work;
      }
      if (!normal_priority.empty()) {
        ParallelWork work = normal_priority.front();
        normal_priority.pop_front();
        return work;
      }
      if (!low_priority.empty()) {
        ParallelWork work = low_priority.front();
        low_priority.pop_front();
        return work;
      }
      BLI_assert_unreachable();
      return {};
    }

    bool is_empty()
    {
      return low_priority.empty() && normal_priority.empty() && high_priority.empty();
    }

    void remove_batch(Batch *batch)
    {
      auto remove = [](std::deque<ParallelWork> &queue, Batch *batch) {
        for (ParallelWork &work : queue) {
          if (work.batch == batch) {
            work = {};
            batch->pending_compilations--;
          }
        }

        queue.erase(std::remove_if(queue.begin(),
                                   queue.end(),
                                   [](const ParallelWork &work) { return !work.batch; }),
                    queue.end());
      };

      remove(low_priority, batch);
      remove(normal_priority, batch);
      remove(high_priority, batch);
    }
  };
  CompilationQueue compilation_queue_;

  std::unique_ptr<GPUWorker> compilation_worker_;

  bool support_specializations_;

  void *pop_work();
  void do_work(void *work_payload);

  BatchHandle next_batch_handle_ = 1;

  bool is_compiling_impl();

 protected:
  /* Must be called earlier from the destructor of the subclass if the compilation process relies
   * on subclass resources. */
  void destruct_compilation_worker()
  {
    compilation_worker_.reset();
  }

 public:
  ShaderCompiler(uint32_t threads_count = 1,
                 GPUWorker::ContextType context_type = GPUWorker::ContextType::PerThread,
                 bool support_specializations = false);
  virtual ~ShaderCompiler();

  Shader *compile(const shader::ShaderCreateInfo &info, bool is_batch_compilation);

  virtual Shader *compile_shader(const shader::ShaderCreateInfo &info);
  virtual void specialize_shader(ShaderSpecialization & /*specialization*/) {};

  BatchHandle batch_compile(Span<const shader::ShaderCreateInfo *> &infos,
                            CompilationPriority priority);
  void batch_cancel(BatchHandle &handle);
  bool batch_is_ready(BatchHandle handle);
  Vector<Shader *> batch_finalize(BatchHandle &handle);

  SpecializationBatchHandle precompile_specializations(Span<ShaderSpecialization> specializations,
                                                       CompilationPriority priority);

  bool specialization_batch_is_ready(SpecializationBatchHandle &handle);

  bool is_compiling();
  void wait_for_all();
};

enum class Severity {
  Unknown,
  Warning,
  Error,
  Note,
};

struct LogCursor {
  int source = -1;
  int row = -1;
  int column = -1;
  std::string file_name_and_error_line;
};

struct GPULogItem {
  LogCursor cursor;
  Severity severity = Severity::Unknown;
};

class GPULogParser {
 public:
  virtual const char *parse_line(const char *source_combined,
                                 const char *log_line,
                                 GPULogItem &log_item) = 0;

 protected:
  const char *skip_severity(const char *log_line,
                            GPULogItem &log_item,
                            const char *error_msg,
                            const char *warning_msg,
                            const char *note_msg) const;
  const char *skip_separators(const char *log_line, const StringRef separators) const;
  const char *skip_until(const char *log_line, char stop_char) const;
  bool at_number(const char *log_line) const;
  bool at_any(const char *log_line, const StringRef chars) const;
  int parse_number(const char *log_line, const char **r_new_position) const;

  static size_t line_start_get(StringRefNull source_combined, size_t target_line);
  static StringRef filename_get(StringRefNull source_combined, size_t pos);
  static size_t source_line_get(StringRefNull source_combined, size_t pos);

  MEM_CXX_CLASS_ALLOC_FUNCS("GPULogParser");
};

void printf_begin(Context *ctx);
void printf_end(Context *ctx);

}  // namespace blender::gpu

/* XXX do not use it. Special hack to use OCIO with batch API. */
blender::gpu::Shader *immGetShader();

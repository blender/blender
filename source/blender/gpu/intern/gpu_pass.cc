/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Convert material node-trees to GLSL.
 */

#include "MEM_guardedalloc.h"

#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_time.h"
#include "BLI_vector.hh"

#include "GPU_capabilities.hh"
#include "GPU_context.hh"
#include "GPU_pass.hh"
#include "GPU_vertex_format.hh"
#include "gpu_codegen.hh"

#include <mutex>
#include <string>

using namespace blender;
using namespace blender::gpu::shader;

static bool gpu_pass_validate(GPUCodegenCreateInfo *create_info);

/* -------------------------------------------------------------------- */
/** \name GPUPass
 * \{ */

struct GPUPass {
  static inline std::atomic<uint64_t> compilation_counts = 0;

  GPUCodegenCreateInfo *create_info = nullptr;
  BatchHandle compilation_handle = 0;
  std::atomic<blender::gpu::Shader *> shader = nullptr;
  std::atomic<GPUPassStatus> status = GPU_PASS_QUEUED;
  /* Orphaned GPUPasses gets freed by the garbage collector. */
  std::atomic<int> refcount = 1;
  double creation_timestamp = 0.0f;
  /* The last time the refcount was greater than 0. */
  double gc_timestamp = 0.0f;

  uint64_t compilation_timestamp = 0;

  /** Hint that an optimized variant of this pass should be created.
   *  Based on a complexity heuristic from pass code generation. */
  bool should_optimize = false;
  bool is_optimization_pass = false;

  /* Number of seconds after creation required before compiling an optimization pass. */
  static constexpr float optimization_delay = 10.0f;

  GPUPass(GPUCodegenCreateInfo *info,
          bool deferred_compilation,
          bool is_optimization_pass,
          bool should_optimize)
      : create_info(info),
        creation_timestamp(BLI_time_now_seconds()),
        should_optimize(should_optimize),
        is_optimization_pass(is_optimization_pass)
  {
    BLI_assert(!is_optimization_pass || !should_optimize);
    if (is_optimization_pass && deferred_compilation) {
      // Defer until all non optimization passes are compiled.
      return;
    }

    GPUShaderCreateInfo *base_info = reinterpret_cast<GPUShaderCreateInfo *>(create_info);

    if (deferred_compilation) {
      compilation_handle = GPU_shader_batch_create_from_infos(
          Span<GPUShaderCreateInfo *>(&base_info, 1), compilation_priority());
    }
    else {
      shader = GPU_shader_create_from_info(base_info);
      finalize_compilation();
    }
  }

  ~GPUPass()
  {
    if (compilation_handle) {
      GPU_shader_batch_cancel(compilation_handle);
    }
    else {
      BLI_assert(create_info == nullptr || (is_optimization_pass && status == GPU_PASS_QUEUED));
    }
    MEM_delete(create_info);
    GPU_SHADER_FREE_SAFE(shader);
  }

  CompilationPriority compilation_priority()
  {
    return is_optimization_pass ? CompilationPriority::Low : CompilationPriority::Medium;
  }

  void finalize_compilation()
  {
    BLI_assert_msg(create_info, "GPUPass::finalize_compilation() called more than once.");

    if (compilation_handle) {
      shader = GPU_shader_batch_finalize(compilation_handle).first();
    }

    compilation_timestamp = ++compilation_counts;

    if (!shader && !gpu_pass_validate(create_info)) {
      fprintf(stderr, "blender::gpu::Shader: error: too many samplers in shader.\n");
    }

    status = shader ? GPU_PASS_SUCCESS : GPU_PASS_FAILED;

    MEM_delete(create_info);
    create_info = nullptr;
  }

  void update(double timestamp)
  {
    update_compilation(timestamp);
    update_gc_timestamp(timestamp);
  }

  void update_compilation(double timestamp)
  {
    if (compilation_handle) {
      if (GPU_shader_batch_is_ready(compilation_handle)) {
        finalize_compilation();
      }
    }
    else if (status == GPU_PASS_QUEUED && refcount > 0 &&
             ((creation_timestamp + optimization_delay) <= timestamp))
    {
      BLI_assert(is_optimization_pass);
      GPUShaderCreateInfo *base_info = reinterpret_cast<GPUShaderCreateInfo *>(create_info);
      compilation_handle = GPU_shader_batch_create_from_infos(
          Span<GPUShaderCreateInfo *>(&base_info, 1), compilation_priority());
    }
  }

  void update_gc_timestamp(double timestamp)
  {
    if (refcount != 0 || gc_timestamp == 0.0f) {
      gc_timestamp = timestamp;
    }
  }

  bool should_gc(int gc_collect_rate, double timestamp)
  {
    BLI_assert(gc_timestamp != 0.0f);
    return !compilation_handle && status != GPU_PASS_FAILED &&
           (timestamp - gc_timestamp) >= gc_collect_rate;
  }
};

GPUPassStatus GPU_pass_status(GPUPass *pass)
{
  return pass->status;
}

bool GPU_pass_should_optimize(GPUPass *pass)
{
  /* Returns optimization heuristic prepared during
   * initial codegen.
   * NOTE: Only enabled on Metal, since it doesn't seem to yield any performance improvements for
   * other backends. */
  return (GPU_backend_get_type() == GPU_BACKEND_METAL) && pass->should_optimize;
}

blender::gpu::Shader *GPU_pass_shader_get(GPUPass *pass)
{
  return pass->shader;
}

void GPU_pass_acquire(GPUPass *pass)
{
  int previous_refcount = pass->refcount++;
  UNUSED_VARS_NDEBUG(previous_refcount);
  BLI_assert(previous_refcount > 0);
}

void GPU_pass_release(GPUPass *pass)
{
  int previous_refcount = pass->refcount--;
  UNUSED_VARS_NDEBUG(previous_refcount);
  BLI_assert(previous_refcount > 0);
}

uint64_t GPU_pass_global_compilation_count()
{
  return GPUPass::compilation_counts;
}

uint64_t GPU_pass_compilation_timestamp(GPUPass *pass)
{
  return pass->compilation_timestamp;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPUPass Cache
 *
 * Internal shader cache: This prevent the shader recompilation / stall when
 * using undo/redo AND also allows for GPUPass reuse if the Shader code is the
 * same for 2 different Materials. Unused GPUPasses are free by Garbage collection.
 * \{ */

class GPUPassCache {

  /** Number of seconds with 0 users required before garbage collecting a pass. */
  static constexpr float gc_collect_rate_ = 60.0f;
  static constexpr float optimization_gc_collect_rate_ = 1.0f;

  Map<uint32_t, std::unique_ptr<GPUPass>> passes_[GPU_MAT_ENGINE_MAX][2 /*is_optimization_pass*/];
  std::mutex mutex_;

 public:
  void add(eGPUMaterialEngine engine,
           GPUCodegen &codegen,
           bool deferred_compilation,
           bool is_optimization_pass)
  {
    std::lock_guard lock(mutex_);

    passes_[engine][is_optimization_pass].add(
        codegen.hash_get(),
        std::make_unique<GPUPass>(codegen.create_info,
                                  deferred_compilation,
                                  is_optimization_pass,
                                  codegen.should_optimize_heuristic()));
  };

  GPUPass *get(eGPUMaterialEngine engine,
               size_t hash,
               bool allow_deferred,
               bool is_optimization_pass)
  {
    std::lock_guard lock(mutex_);
    std::unique_ptr<GPUPass> *pass = passes_[engine][is_optimization_pass].lookup_ptr(hash);
    if (!allow_deferred && pass && pass->get()->status == GPU_PASS_QUEUED) {
      pass->get()->finalize_compilation();
    }
    return pass ? pass->get() : nullptr;
  }

  void update()
  {
    std::lock_guard lock(mutex_);

    double timestamp = BLI_time_now_seconds();

    /* Base Passes. */
    for (auto &engine_passes : passes_) {
      for (std::unique_ptr<GPUPass> &pass : engine_passes[false].values()) {
        pass->update(timestamp);
      }

      engine_passes[false].remove_if(
          [&](auto item) { return item.value->should_gc(gc_collect_rate_, timestamp); });
    }

    /* Optimization Passes */
    for (auto &engine_passes : passes_) {
      for (std::unique_ptr<GPUPass> &pass : engine_passes[true].values()) {
        pass->update(timestamp);
      }

      engine_passes[true].remove_if([&](auto item) {
        return item.value->should_gc(optimization_gc_collect_rate_, timestamp);
      });
    }
  }

  std::mutex &get_mutex()
  {
    return mutex_;
  }
};

static GPUPassCache *g_cache = nullptr;

void GPU_pass_ensure_its_ready(GPUPass *pass)
{
  if (pass->status == GPU_PASS_QUEUED) {
    std::lock_guard lock(g_cache->get_mutex());
    if (pass->status == GPU_PASS_QUEUED) {
      pass->finalize_compilation();
    }
  }
}

void GPU_pass_cache_init()
{
  g_cache = MEM_new<GPUPassCache>(__func__);
}

void GPU_pass_cache_update()
{
  g_cache->update();
}

void GPU_pass_cache_wait_for_all()
{
  GPU_shader_batch_wait_for_all();
  g_cache->update();
}

void GPU_pass_cache_free()
{
  MEM_SAFE_DELETE(g_cache);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compilation
 * \{ */

static bool gpu_pass_validate(GPUCodegenCreateInfo *create_info)
{
  int samplers_len = 0;
  for (const ShaderCreateInfo::Resource &res : create_info->resources_get_all_()) {
    if (res.bind_type == ShaderCreateInfo::Resource::BindType::SAMPLER) {
      samplers_len++;
    }
  }

  /* Validate against GPU limit. */
  if ((samplers_len > GPU_max_textures_frag()) || (samplers_len > GPU_max_textures_vert())) {
    return false;
  }

  return (samplers_len * 2 <= GPU_max_textures());
}

GPUPass *GPU_generate_pass(GPUMaterial *material,
                           GPUNodeGraph *graph,
                           const char *debug_name,
                           eGPUMaterialEngine engine,
                           bool deferred_compilation,
                           GPUCodegenCallbackFn finalize_source_cb,
                           void *thunk,
                           bool optimize_graph)
{
  gpu_node_graph_prune_unused(graph);

  /* If Optimize flag is passed in, we are generating an optimized
   * variant of the GPUMaterial's GPUPass. */
  if (optimize_graph) {
    gpu_node_graph_optimize(graph);
  }

  /* Extract attributes before compiling so the generated VBOs are ready to accept the future
   * shader. */
  gpu_node_graph_finalize_uniform_attrs(graph);

  GPUCodegen codegen(material, graph, debug_name);
  codegen.generate_graphs();
  codegen.generate_cryptomatte();

  GPUPass *pass = nullptr;

  if (!optimize_graph) {
    /* The optimized version of the shader should not re-generate a UBO.
     * The UBO will not be used for this variant. */
    codegen.generate_uniform_buffer();
  }

  /* Cache lookup: Reuse shaders already compiled. */
  pass = g_cache->get(engine, codegen.hash_get(), deferred_compilation, optimize_graph);

  if (pass) {
    pass->refcount++;
    return pass;
  }

  /* The shader is not compiled, continue generating the shader strings. */
  codegen.generate_attribs();
  codegen.generate_resources();

  /* Make engine add its own code and implement the generated functions. */
  finalize_source_cb(thunk, material, &codegen.output);

  codegen.create_info->finalize();
  g_cache->add(engine, codegen, deferred_compilation, optimize_graph);
  codegen.create_info = nullptr;

  return g_cache->get(engine, codegen.hash_get(), deferred_compilation, optimize_graph);
}

/** \} */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "DEG_depsgraph_query.h"

#include "GPU_material.h"
#include "GPU_shader.h"

#include "WM_api.h"
#include "WM_types.h"

#include "draw_manager.h"

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_depth_only_frag_glsl[];
extern char datatoc_common_fullscreen_vert_glsl[];

#define USE_DEFERRED_COMPILATION 1

/* -------------------------------------------------------------------- */
/** \name Deferred Compilation (DRW_deferred)
 *
 * Since compiling shader can take a long time, we do it in a non blocking
 * manner in another thread.
 *
 * \{ */

typedef struct DRWDeferredShader {
  struct DRWDeferredShader *prev, *next;

  GPUMaterial *mat;
} DRWDeferredShader;

typedef struct DRWShaderCompiler {
  ListBase queue;          /* DRWDeferredShader */
  ListBase queue_conclude; /* DRWDeferredShader */
  SpinLock list_lock;

  DRWDeferredShader *mat_compiling;
  ThreadMutex compilation_lock;

  void *gl_context;
  bool own_context;

  int shaders_done; /* To compute progress. */
} DRWShaderCompiler;

static void drw_deferred_shader_free(DRWDeferredShader *dsh)
{
  /* Make sure it is not queued before freeing. */
  MEM_freeN(dsh);
}

static void drw_deferred_shader_queue_free(ListBase *queue)
{
  DRWDeferredShader *dsh;
  while ((dsh = BLI_pophead(queue))) {
    drw_deferred_shader_free(dsh);
  }
}

static void drw_deferred_shader_compilation_exec(void *custom_data,
                                                 short *stop,
                                                 short *do_update,
                                                 float *progress)
{
  DRWShaderCompiler *comp = (DRWShaderCompiler *)custom_data;
  void *gl_context = comp->gl_context;

#if TRUST_NO_ONE
  BLI_assert(gl_context != NULL);
#endif

  WM_opengl_context_activate(gl_context);

  while (true) {
    BLI_spin_lock(&comp->list_lock);

    if (*stop != 0) {
      /* We don't want user to be able to cancel the compilation
       * but wm can kill the task if we are closing blender. */
      BLI_spin_unlock(&comp->list_lock);
      break;
    }

    /* Pop tail because it will be less likely to lock the main thread
     * if all GPUMaterials are to be freed (see DRW_deferred_shader_remove()). */
    comp->mat_compiling = BLI_poptail(&comp->queue);
    if (comp->mat_compiling == NULL) {
      /* No more Shader to compile. */
      BLI_spin_unlock(&comp->list_lock);
      break;
    }

    comp->shaders_done++;
    int total = BLI_listbase_count(&comp->queue) + comp->shaders_done;

    BLI_mutex_lock(&comp->compilation_lock);
    BLI_spin_unlock(&comp->list_lock);

    /* Do the compilation. */
    GPU_material_compile(comp->mat_compiling->mat);

    *progress = (float)comp->shaders_done / (float)total;
    *do_update = true;

    GPU_flush();
    BLI_mutex_unlock(&comp->compilation_lock);

    BLI_spin_lock(&comp->list_lock);
    if (GPU_material_status(comp->mat_compiling->mat) == GPU_MAT_QUEUED) {
      BLI_addtail(&comp->queue_conclude, comp->mat_compiling);
    }
    else {
      drw_deferred_shader_free(comp->mat_compiling);
    }
    comp->mat_compiling = NULL;
    BLI_spin_unlock(&comp->list_lock);
  }

  WM_opengl_context_release(gl_context);
}

static void drw_deferred_shader_compilation_free(void *custom_data)
{
  DRWShaderCompiler *comp = (DRWShaderCompiler *)custom_data;

  drw_deferred_shader_queue_free(&comp->queue);

  if (!BLI_listbase_is_empty(&comp->queue_conclude)) {
    /* Compile the shaders in the context they will be deleted. */
    DRW_opengl_context_enable_ex(false);
    DRWDeferredShader *mat_conclude;
    while ((mat_conclude = BLI_poptail(&comp->queue_conclude))) {
      GPU_material_compile(mat_conclude->mat);
      drw_deferred_shader_free(mat_conclude);
    }
    DRW_opengl_context_disable_ex(true);
  }

  BLI_spin_end(&comp->list_lock);
  BLI_mutex_end(&comp->compilation_lock);

  if (comp->own_context) {
    /* Only destroy if the job owns the context. */
    WM_opengl_context_dispose(comp->gl_context);
  }

  MEM_freeN(comp);
}

static void drw_deferred_shader_add(GPUMaterial *mat, bool deferred)
{
  /* Do not defer the compilation if we are rendering for image.
   * deferred rendering is only possible when `evil_C` is available */
  if (DST.draw_ctx.evil_C == NULL || DRW_state_is_image_render() || !USE_DEFERRED_COMPILATION ||
      !deferred) {
    /* Double checking that this GPUMaterial is not going to be
     * compiled by another thread. */
    DRW_deferred_shader_remove(mat);
    GPU_material_compile(mat);
    return;
  }

  DRWDeferredShader *dsh = MEM_callocN(sizeof(DRWDeferredShader), "Deferred Shader");

  dsh->mat = mat;

  BLI_assert(DST.draw_ctx.evil_C);
  wmWindowManager *wm = CTX_wm_manager(DST.draw_ctx.evil_C);
  wmWindow *win = CTX_wm_window(DST.draw_ctx.evil_C);

  /* Use original scene ID since this is what the jobs template tests for. */
  Scene *scene = (Scene *)DEG_get_original_id(&DST.draw_ctx.scene->id);

  /* Get the running job or a new one if none is running. Can only have one job per type & owner.
   */
  wmJob *wm_job = WM_jobs_get(
      wm, win, scene, "Shaders Compilation", WM_JOB_PROGRESS, WM_JOB_TYPE_SHADER_COMPILATION);

  DRWShaderCompiler *old_comp = (DRWShaderCompiler *)WM_jobs_customdata_get(wm_job);

  DRWShaderCompiler *comp = MEM_callocN(sizeof(DRWShaderCompiler), "DRWShaderCompiler");
  BLI_spin_init(&comp->list_lock);
  BLI_mutex_init(&comp->compilation_lock);

  if (old_comp) {
    BLI_spin_lock(&old_comp->list_lock);
    BLI_movelisttolist(&comp->queue, &old_comp->queue);
    BLI_spin_unlock(&old_comp->list_lock);
    /* Do not recreate context, just pass ownership. */
    if (old_comp->gl_context) {
      comp->gl_context = old_comp->gl_context;
      old_comp->own_context = false;
      comp->own_context = true;
    }
  }

  BLI_addtail(&comp->queue, dsh);

  /* Create only one context. */
  if (comp->gl_context == NULL) {
    comp->gl_context = WM_opengl_context_create();
    WM_opengl_context_activate(DST.gl_context);
    comp->own_context = true;
  }

  WM_jobs_customdata_set(wm_job, comp, drw_deferred_shader_compilation_free);
  WM_jobs_timer(wm_job, 0.1, NC_MATERIAL | ND_SHADING_DRAW, 0);
  WM_jobs_delay_start(wm_job, 0.1);
  WM_jobs_callbacks(wm_job, drw_deferred_shader_compilation_exec, NULL, NULL, NULL);

  G.is_break = false;

  WM_jobs_start(wm, wm_job);
}

void DRW_deferred_shader_remove(GPUMaterial *mat)
{
  Scene *scene = GPU_material_scene(mat);

  for (wmWindowManager *wm = G_MAIN->wm.first; wm; wm = wm->id.next) {
    if (WM_jobs_test(wm, scene, WM_JOB_TYPE_SHADER_COMPILATION) == false) {
      /* No job running, do not create a new one by calling WM_jobs_get. */
      continue;
    }
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      wmJob *wm_job = WM_jobs_get(
          wm, win, scene, "Shaders Compilation", WM_JOB_PROGRESS, WM_JOB_TYPE_SHADER_COMPILATION);

      DRWShaderCompiler *comp = (DRWShaderCompiler *)WM_jobs_customdata_get(wm_job);
      if (comp != NULL) {
        BLI_spin_lock(&comp->list_lock);
        DRWDeferredShader *dsh;
        dsh = (DRWDeferredShader *)BLI_findptr(
            &comp->queue, mat, offsetof(DRWDeferredShader, mat));
        if (dsh) {
          BLI_remlink(&comp->queue, dsh);
        }

        /* Wait for compilation to finish */
        if ((comp->mat_compiling != NULL) && (comp->mat_compiling->mat == mat)) {
          BLI_mutex_lock(&comp->compilation_lock);
          BLI_mutex_unlock(&comp->compilation_lock);
        }

        BLI_spin_unlock(&comp->list_lock);

        if (dsh) {
          drw_deferred_shader_free(dsh);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */

/** \{ */

GPUShader *DRW_shader_create(const char *vert,
                             const char *geom,
                             const char *frag,
                             const char *defines)
{
  return GPU_shader_create(vert, frag, geom, NULL, defines, __func__);
}

GPUShader *DRW_shader_create_with_lib(
    const char *vert, const char *geom, const char *frag, const char *lib, const char *defines)
{
  GPUShader *sh;
  char *vert_with_lib = NULL;
  char *frag_with_lib = NULL;
  char *geom_with_lib = NULL;

  vert_with_lib = BLI_string_joinN(lib, vert);
  frag_with_lib = BLI_string_joinN(lib, frag);
  if (geom) {
    geom_with_lib = BLI_string_joinN(lib, geom);
  }

  sh = GPU_shader_create(vert_with_lib, frag_with_lib, geom_with_lib, NULL, defines, __func__);

  MEM_freeN(vert_with_lib);
  MEM_freeN(frag_with_lib);
  if (geom) {
    MEM_freeN(geom_with_lib);
  }

  return sh;
}

GPUShader *DRW_shader_create_with_transform_feedback(const char *vert,
                                                     const char *geom,
                                                     const char *defines,
                                                     const eGPUShaderTFBType prim_type,
                                                     const char **varying_names,
                                                     const int varying_count)
{
  return GPU_shader_create_ex(vert,
                              datatoc_gpu_shader_depth_only_frag_glsl,
                              geom,
                              NULL,
                              defines,
                              prim_type,
                              varying_names,
                              varying_count,
                              __func__);
}

GPUShader *DRW_shader_create_2d(const char *frag, const char *defines)
{
  return GPU_shader_create(datatoc_gpu_shader_2D_vert_glsl, frag, NULL, NULL, defines, __func__);
}

GPUShader *DRW_shader_create_3d(const char *frag, const char *defines)
{
  return GPU_shader_create(datatoc_gpu_shader_3D_vert_glsl, frag, NULL, NULL, defines, __func__);
}

GPUShader *DRW_shader_create_fullscreen(const char *frag, const char *defines)
{
  return GPU_shader_create(
      datatoc_common_fullscreen_vert_glsl, frag, NULL, NULL, defines, __func__);
}

GPUShader *DRW_shader_create_3d_depth_only(eGPUShaderConfig sh_cfg)
{
  return GPU_shader_get_builtin_shader_with_config(GPU_SHADER_3D_DEPTH_ONLY, sh_cfg);
}

GPUMaterial *DRW_shader_find_from_world(World *wo,
                                        const void *engine_type,
                                        const int options,
                                        bool deferred)
{
  GPUMaterial *mat = GPU_material_from_nodetree_find(&wo->gpumaterial, engine_type, options);
  if (DRW_state_is_image_render() || !deferred) {
    if (mat != NULL && GPU_material_status(mat) == GPU_MAT_QUEUED) {
      /* XXX Hack : we return NULL so that the engine will call DRW_shader_create_from_XXX
       * with the shader code and we will resume the compilation from there. */
      return NULL;
    }
  }
  return mat;
}

GPUMaterial *DRW_shader_find_from_material(Material *ma,
                                           const void *engine_type,
                                           const int options,
                                           bool deferred)
{
  GPUMaterial *mat = GPU_material_from_nodetree_find(&ma->gpumaterial, engine_type, options);
  if (DRW_state_is_image_render() || !deferred) {
    if (mat != NULL && GPU_material_status(mat) == GPU_MAT_QUEUED) {
      /* XXX Hack : we return NULL so that the engine will call DRW_shader_create_from_XXX
       * with the shader code and we will resume the compilation from there. */
      return NULL;
    }
  }
  return mat;
}

GPUMaterial *DRW_shader_create_from_world(struct Scene *scene,
                                          World *wo,
                                          struct bNodeTree *ntree,
                                          const void *engine_type,
                                          const int options,
                                          const bool is_volume_shader,
                                          const char *vert,
                                          const char *geom,
                                          const char *frag_lib,
                                          const char *defines,
                                          bool deferred)
{
  GPUMaterial *mat = NULL;
  if (DRW_state_is_image_render() || !deferred) {
    mat = GPU_material_from_nodetree_find(&wo->gpumaterial, engine_type, options);
  }

  if (mat == NULL) {
    scene = (Scene *)DEG_get_original_id(&DST.draw_ctx.scene->id);
    mat = GPU_material_from_nodetree(scene,
                                     NULL,
                                     ntree,
                                     &wo->gpumaterial,
                                     engine_type,
                                     options,
                                     is_volume_shader,
                                     vert,
                                     geom,
                                     frag_lib,
                                     defines,
                                     wo->id.name);
  }

  if (GPU_material_status(mat) == GPU_MAT_QUEUED) {
    drw_deferred_shader_add(mat, deferred);
  }

  return mat;
}

GPUMaterial *DRW_shader_create_from_material(struct Scene *scene,
                                             Material *ma,
                                             struct bNodeTree *ntree,
                                             const void *engine_type,
                                             const int options,
                                             const bool is_volume_shader,
                                             const char *vert,
                                             const char *geom,
                                             const char *frag_lib,
                                             const char *defines,
                                             bool deferred)
{
  GPUMaterial *mat = NULL;
  if (DRW_state_is_image_render() || !deferred) {
    mat = GPU_material_from_nodetree_find(&ma->gpumaterial, engine_type, options);
  }

  if (mat == NULL) {
    scene = (Scene *)DEG_get_original_id(&DST.draw_ctx.scene->id);
    mat = GPU_material_from_nodetree(scene,
                                     ma,
                                     ntree,
                                     &ma->gpumaterial,
                                     engine_type,
                                     options,
                                     is_volume_shader,
                                     vert,
                                     geom,
                                     frag_lib,
                                     defines,
                                     ma->id.name);
  }

  if (GPU_material_status(mat) == GPU_MAT_QUEUED) {
    drw_deferred_shader_add(mat, deferred);
  }

  return mat;
}

void DRW_shader_free(GPUShader *shader)
{
  GPU_shader_free(shader);
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Shader Library
 *
 * Simple include system for glsl files.
 *
 * Usage: Create a DRWShaderLibrary and add the library in the right order.
 * You can have nested dependencies but each new library needs to have all its dependencies already
 * added to the DRWShaderLibrary.
 * Finally you can use DRW_shader_library_create_shader_string to get a shader string that also
 * contains the needed libraries for this shader.
 * \{ */

/* 32 because we use a 32bit bitmap. */
#define MAX_LIB 32
#define MAX_LIB_NAME 64
#define MAX_LIB_DEPS 8

struct DRWShaderLibrary {
  char *libs[MAX_LIB];
  char libs_name[MAX_LIB][MAX_LIB_NAME];
  uint32_t libs_deps[MAX_LIB];
};

DRWShaderLibrary *DRW_shader_library_create(void)
{
  return MEM_callocN(sizeof(DRWShaderLibrary), "DRWShaderLibrary");
}

void DRW_shader_library_free(DRWShaderLibrary *lib)
{
  MEM_SAFE_FREE(lib);
}

static int drw_shader_library_search(DRWShaderLibrary *lib, const char *name)
{
  for (int i = 0; i < MAX_LIB; i++) {
    if (lib->libs[i]) {
      if (!strncmp(lib->libs_name[i], name, strlen(lib->libs_name[i]))) {
        return i;
      }
    }
    else {
      break;
    }
  }
  return -1;
}

/* Return bitmap of dependencies. */
static uint32_t drw_shader_dependencies_get(DRWShaderLibrary *lib, char *lib_code)
{
  /* Search dependencies. */
  uint32_t deps = 0;
  char *haystack = lib_code;
  while ((haystack = strstr(haystack, "BLENDER_REQUIRE("))) {
    haystack += 16;
    int dep = drw_shader_library_search(lib, haystack);
    if (dep == -1) {
      printf(
          "Error: Dependency not found.\n"
          "This might be due to bad lib ordering.\n");
      BLI_assert(0);
    }
    else {
      deps |= 1u << (uint32_t)dep;
    }
  }
  return deps;
}

void DRW_shader_library_add_file(DRWShaderLibrary *lib, char *lib_code, const char *lib_name)
{
  int index = -1;
  for (int i = 0; i < MAX_LIB; i++) {
    if (lib->libs[i] == NULL) {
      index = i;
      break;
    }
  }

  if (index > -1) {
    lib->libs[index] = lib_code;
    BLI_strncpy(lib->libs_name[index], lib_name, MAX_LIB_NAME);
    lib->libs_deps[index] = drw_shader_dependencies_get(lib, lib_code);
  }
  else {
    printf("Error: Too many libraries. Cannot add %s.\n", lib_name);
    BLI_assert(0);
  }
}

/* Return an allocN'ed string containing the shader code with its dependencies prepended.
 * Caller must free the string with MEM_freeN after use. */
char *DRW_shader_library_create_shader_string(DRWShaderLibrary *lib, char *shader_code)
{
  uint32_t deps = drw_shader_dependencies_get(lib, shader_code);

  DynStr *ds = BLI_dynstr_new();
  /* Add all dependencies recursively. */
  for (int i = MAX_LIB - 1; i > -1; i--) {
    if (lib->libs[i] && (deps & (1u << (uint32_t)i))) {
      deps |= lib->libs_deps[i];
    }
  }
  /* Concatenate all needed libs into one string. */
  for (int i = 0; i < MAX_LIB; i++) {
    if (deps & 1u) {
      BLI_dynstr_append(ds, lib->libs[i]);
    }
    deps = deps >> 1;
  }

  BLI_dynstr_append(ds, shader_code);

  char *str = BLI_dynstr_get_cstring(ds);
  BLI_dynstr_free(ds);

  return str;
}

/** \} */

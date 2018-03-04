/*
 * Copyright 2016, Blender Foundation.
 *
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
 * Contributor(s): Blender Institute
 *
 */

/** \file blender/draw/intern/draw_manager_shader.c
 *  \ingroup draw
 */

#include "draw_manager.h"

#include "DNA_world_types.h"
#include "DNA_material_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_threads.h"
#include "BLI_task.h"

#include "GPU_shader.h"
#include "GPU_material.h"

#include "WM_api.h"

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_fullscreen_vert_glsl[];


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
	char *vert, *geom, *frag, *defs;

	ThreadMutex compilation_mutex;
} DRWDeferredShader;

typedef struct DRWShaderCompiler {
	ListBase queue; /* DRWDeferredShader */
	ThreadMutex list_mutex;

	DRWDeferredShader *mat_compiling;
	ThreadMutex compilation_mutex;

	TaskScheduler *task_scheduler; /* NULL if nothing is running. */
	TaskPool *task_pool;

	void *ogl_context;
} DRWShaderCompiler;

static DRWShaderCompiler DSC = {{NULL}};

static void drw_deferred_shader_free(DRWDeferredShader *dsh)
{
	/* Make sure it is not queued before freeing. */
	BLI_assert(BLI_findindex(&DSC.queue, dsh) == -1);

	MEM_SAFE_FREE(dsh->vert);
	MEM_SAFE_FREE(dsh->geom);
	MEM_SAFE_FREE(dsh->frag);
	MEM_SAFE_FREE(dsh->defs);

	MEM_freeN(dsh);
}

static void drw_deferred_shader_compilation_exec(TaskPool * __restrict UNUSED(pool), void *UNUSED(taskdata), int UNUSED(threadid))
{
	WM_opengl_context_activate(DSC.ogl_context);

	while (true) {
		BLI_mutex_lock(&DSC.list_mutex);
		DSC.mat_compiling = BLI_pophead(&DSC.queue);
		if (DSC.mat_compiling == NULL) {
			break;
		}
		BLI_mutex_lock(&DSC.compilation_mutex);
		BLI_mutex_unlock(&DSC.list_mutex);

		/* Do the compilation. */
		GPU_material_generate_pass(
		        DSC.mat_compiling->mat,
		        DSC.mat_compiling->vert,
		        DSC.mat_compiling->geom,
		        DSC.mat_compiling->frag,
		        DSC.mat_compiling->defs);

		BLI_mutex_unlock(&DSC.compilation_mutex);

		drw_deferred_shader_free(DSC.mat_compiling);
	}

	WM_opengl_context_release(DSC.ogl_context);
	BLI_mutex_unlock(&DSC.list_mutex);
}

static void drw_deferred_shader_add(
        GPUMaterial *mat, const char *vert, const char *geom, const char *frag_lib, const char *defines)
{
	if (DRW_state_is_image_render()) {
		/* Do not deferre the compilation if we are rendering for image. */
		GPU_material_generate_pass(mat, vert, geom, frag_lib, defines);
		return;
	}

	DRWDeferredShader *dsh = MEM_callocN(sizeof(DRWDeferredShader), "Deferred Shader");

	dsh->mat = mat;
	if (vert)     dsh->vert = BLI_strdup(vert);
	if (geom)     dsh->geom = BLI_strdup(geom);
	if (frag_lib) dsh->frag = BLI_strdup(frag_lib);
	if (defines)  dsh->defs = BLI_strdup(defines);

	BLI_mutex_lock(&DSC.list_mutex);
	BLI_addtail(&DSC.queue, dsh);
	if (DSC.mat_compiling == NULL) {
		/* Set value so that other threads do not start a new task. */
		DSC.mat_compiling = (void *)1;

		if (DSC.task_scheduler == NULL) {
			DSC.task_scheduler = BLI_task_scheduler_create(1);
			DSC.task_pool = BLI_task_pool_create_background(DSC.task_scheduler, NULL);
		}
		BLI_task_pool_push(DSC.task_pool, drw_deferred_shader_compilation_exec, NULL, false, TASK_PRIORITY_LOW);
	}
	BLI_mutex_unlock(&DSC.list_mutex);
}

void DRW_deferred_shader_remove(GPUMaterial *mat)
{
	BLI_mutex_lock(&DSC.list_mutex);
	DRWDeferredShader *dsh = (DRWDeferredShader *)BLI_findptr(&DSC.queue, mat, offsetof(DRWDeferredShader, mat));
	if (dsh) {
		BLI_remlink(&DSC.queue, dsh);
	}
	if (DSC.mat_compiling != NULL) {
		if (DSC.mat_compiling->mat == mat) {
			/* Wait for compilation to finish */
			BLI_mutex_lock(&DSC.compilation_mutex);
			BLI_mutex_unlock(&DSC.compilation_mutex);
		}
	}
	BLI_mutex_unlock(&DSC.list_mutex);
	if (dsh) {
		drw_deferred_shader_free(dsh);
	}
}


static void drw_deferred_compiler_finish(void)
{
	if (DSC.task_scheduler != NULL) {
		BLI_task_pool_work_and_wait(DSC.task_pool);
		BLI_task_pool_free(DSC.task_pool);
		BLI_task_scheduler_free(DSC.task_scheduler);
		DSC.task_scheduler = NULL;
	}
}

void DRW_deferred_compiler_init(void)
{
	BLI_mutex_init(&DSC.list_mutex);
	BLI_mutex_init(&DSC.compilation_mutex);
	DSC.ogl_context = WM_opengl_context_create();
}

void DRW_deferred_compiler_exit(void)
{
	drw_deferred_compiler_finish();
	WM_opengl_context_dispose(DSC.ogl_context);
}

/** \} */

/* -------------------------------------------------------------------- */

GPUShader *DRW_shader_create(const char *vert, const char *geom, const char *frag, const char *defines)
{
	return GPU_shader_create(vert, frag, geom, NULL, defines);
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

	sh = GPU_shader_create(vert_with_lib, frag_with_lib, geom_with_lib, NULL, defines);

	MEM_freeN(vert_with_lib);
	MEM_freeN(frag_with_lib);
	if (geom) {
		MEM_freeN(geom_with_lib);
	}

	return sh;
}

GPUShader *DRW_shader_create_2D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_2D_vert_glsl, frag, NULL, NULL, defines);
}

GPUShader *DRW_shader_create_3D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_3D_vert_glsl, frag, NULL, NULL, defines);
}

GPUShader *DRW_shader_create_fullscreen(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_fullscreen_vert_glsl, frag, NULL, NULL, defines);
}

GPUShader *DRW_shader_create_3D_depth_only(void)
{
	return GPU_shader_get_builtin_shader(GPU_SHADER_3D_DEPTH_ONLY);
}

GPUMaterial *DRW_shader_create_from_world(
        struct Scene *scene, World *wo, const void *engine_type, int options,
        const char *vert, const char *geom, const char *frag_lib, const char *defines)
{
	GPUMaterial *mat = GPU_material_from_nodetree(
	        scene, wo->nodetree, &wo->gpumaterial, engine_type, options,
	        vert, geom, frag_lib, defines, true);

	drw_deferred_shader_add(mat, vert, geom, frag_lib, defines);

	return mat;
}

GPUMaterial *DRW_shader_create_from_material(
        struct Scene *scene, Material *ma, const void *engine_type, int options,
        const char *vert, const char *geom, const char *frag_lib, const char *defines)
{
	GPUMaterial *mat = GPU_material_from_nodetree(
	        scene, ma->nodetree, &ma->gpumaterial, engine_type, options,
	        vert, geom, frag_lib, defines, true);

	drw_deferred_shader_add(mat, vert, geom, frag_lib, defines);

	return mat;
}

void DRW_shader_free(GPUShader *shader)
{
	GPU_shader_free(shader);
}

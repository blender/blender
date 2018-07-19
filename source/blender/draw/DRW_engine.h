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

/** \file DRW_engine.h
 *  \ingroup draw
 */

#ifndef __DRW_ENGINE_H__
#define __DRW_ENGINE_H__

#include "BLI_sys_types.h"  /* for bool */

struct ARegion;
struct CollectionEngineSettings;
struct Depsgraph;
struct DRWPass;
struct DRWInstanceDataList;
struct Main;
struct Material;
struct Scene;
struct DrawEngineType;
struct ID;
struct IDProperty;
struct bContext;
struct Object;
struct ViewLayer;
struct ViewContext;
struct ViewportEngineData;
struct View3D;
struct rcti;
struct GPUMaterial;
struct GPUOffScreen;
struct GPUViewport;
struct RenderEngine;
struct RenderEngineType;
struct WorkSpace;

#include "DNA_object_enums.h"

/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
	struct GPUFrameBuffer *default_fb;
	struct GPUFrameBuffer *color_only_fb;
	struct GPUFrameBuffer *depth_only_fb;
	struct GPUFrameBuffer *multisample_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
	struct GPUTexture *color;
	struct GPUTexture *depth;
	struct GPUTexture *multisample_color;
	struct GPUTexture *multisample_depth;
} DefaultTextureList;

void DRW_engines_register(void);
void DRW_engines_free(void);

bool DRW_engine_render_support(struct DrawEngineType *draw_engine_type);
void DRW_engine_register(struct DrawEngineType *draw_engine_type);
void DRW_engine_viewport_data_size_get(
        const void *engine_type,
        int *r_fbl_len, int *r_txl_len, int *r_psl_len, int *r_stl_len);

typedef struct DRWUpdateContext {
	struct Main *bmain;
	struct Depsgraph *depsgraph;
	struct Scene *scene;
	struct ViewLayer *view_layer;
	struct ARegion *ar;
	struct View3D *v3d;
	struct RenderEngineType *engine_type;
} DRWUpdateContext;
void DRW_notify_view_update(const DRWUpdateContext *update_ctx);


typedef enum eDRWSelectStage { DRW_SELECT_PASS_PRE = 1, DRW_SELECT_PASS_POST, } eDRWSelectStage;
typedef bool (*DRW_SelectPassFn)(
        eDRWSelectStage stage, void *user_data);
typedef bool (*DRW_ObjectFilterFn)(
        struct Object *ob, void *user_data);

void DRW_draw_view(const struct bContext *C);

void DRW_draw_render_loop_ex(
        struct Depsgraph *depsgraph,
        struct RenderEngineType *engine_type,
        struct ARegion *ar, struct View3D *v3d,
        struct GPUViewport *viewport,
        const struct bContext *evil_C);
void DRW_draw_render_loop(
        struct Depsgraph *depsgraph,
        struct ARegion *ar, struct View3D *v3d,
        struct GPUViewport *viewport);
void DRW_draw_render_loop_offscreen(
        struct Depsgraph *depsgraph,
        struct RenderEngineType *engine_type,
        struct ARegion *ar, struct View3D *v3d,
        const bool draw_background,
        struct GPUOffScreen *ofs,
        struct GPUViewport *viewport);
void DRW_draw_select_loop(
        struct Depsgraph *depsgraph,
        struct ARegion *ar, struct View3D *v3d,
        bool use_obedit_skip, bool use_nearest, const struct rcti *rect,
        DRW_SelectPassFn select_pass_fn, void *select_pass_user_data,
        DRW_ObjectFilterFn object_filter_fn, void *object_filter_user_data);
void DRW_draw_depth_loop(
        struct Depsgraph *depsgraph,
        struct ARegion *ar, struct View3D *v3d);

/* This is here because GPUViewport needs it */
void DRW_pass_free(struct DRWPass *pass);
struct DRWInstanceDataList *DRW_instance_data_list_create(void);
void DRW_instance_data_list_free(struct DRWInstanceDataList *idatalist);

void DRW_opengl_context_create(void);
void DRW_opengl_context_destroy(void);
void DRW_opengl_context_enable(void);
void DRW_opengl_context_disable(void);

/* Never use this. Only for closing blender. */
void DRW_opengl_context_enable_ex(bool restore);
void DRW_opengl_context_disable_ex(bool restore);

void DRW_opengl_render_context_enable(void *re_gl_context);
void DRW_opengl_render_context_disable(void *re_gl_context);
void DRW_gawain_render_context_enable(void *re_gpu_context);
void DRW_gawain_render_context_disable(void *re_gpu_context);

void DRW_deferred_shader_remove(struct GPUMaterial *mat);

struct DrawDataList *DRW_drawdatalist_from_id(struct ID *id);
void DRW_drawdata_free(struct ID *id);

#endif /* __DRW_ENGINE_H__ */

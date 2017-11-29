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

/** \file DRW_render.h
 *  \ingroup draw
 */

/* This is the Render Functions used by Realtime engines to draw with OpenGL */

#ifndef __DRW_RENDER_H__
#define __DRW_RENDER_H__

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_material.h"
#include "BKE_scene.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "draw_common.h"
#include "draw_cache.h"
#include "draw_view.h"

#include "draw_manager_profiling.h"

#include "MEM_guardedalloc.h"

#include "RE_engine.h"

struct bContext;
struct GPUFrameBuffer;
struct GPUShader;
struct GPUMaterial;
struct GPUTexture;
struct GPUUniformBuffer;
struct Object;
struct Gwn_Batch;
struct DefaultFramebufferList;
struct DefaultTextureList;
struct DRWTextStore;
struct LampEngineData;
struct RenderEngineType;
struct ViewportEngineData;
struct ViewportEngineData_Info;

typedef struct DRWUniform DRWUniform;
typedef struct DRWInterface DRWInterface;
typedef struct DRWPass DRWPass;
typedef struct DRWShadingGroup DRWShadingGroup;

/* declare members as empty (unused) */
typedef char DRWViewportEmptyList;

#define DRW_VIEWPORT_LIST_SIZE(list) \
	(sizeof(list) == sizeof(DRWViewportEmptyList) ? 0 : ((sizeof(list)) / sizeof(void *)))

/* Unused members must be either pass list or 'char *' when not usd. */
#define DRW_VIEWPORT_DATA_SIZE(ty) { \
	DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->fbl)), \
	DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->txl)), \
	DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->psl)), \
	DRW_VIEWPORT_LIST_SIZE(*(((ty *)NULL)->stl)) \
}

/* Use of multisample framebuffers. */
#define MULTISAMPLE_SYNC_ENABLE(dfbl) { \
	if (dfbl->multisample_fb != NULL) { \
		DRW_stats_query_start("Multisample Blit"); \
		DRW_framebuffer_blit(dfbl->default_fb, dfbl->multisample_fb, false, false); \
		DRW_framebuffer_blit(dfbl->default_fb, dfbl->multisample_fb, true, false); \
		DRW_framebuffer_bind(dfbl->multisample_fb); \
		DRW_stats_query_end(); \
	} \
}

#define MULTISAMPLE_SYNC_DISABLE(dfbl) { \
	if (dfbl->multisample_fb != NULL) { \
		DRW_stats_query_start("Multisample Resolve"); \
		DRW_framebuffer_blit(dfbl->multisample_fb, dfbl->default_fb, false, false); \
		DRW_framebuffer_blit(dfbl->multisample_fb, dfbl->default_fb, true, false); \
		DRW_framebuffer_bind(dfbl->default_fb); \
		DRW_stats_query_end(); \
	} \
}



typedef struct DrawEngineDataSize {
	int fbl_len;
	int txl_len;
	int psl_len;
	int stl_len;
} DrawEngineDataSize;

typedef struct DrawEngineType {
	struct DrawEngineType *next, *prev;

	char idname[32];

	const DrawEngineDataSize *vedata_size;

	void (*engine_init)(void *vedata);
	void (*engine_free)(void);

	void (*cache_init)(void *vedata);
	void (*cache_populate)(void *vedata, struct Object *ob);
	void (*cache_finish)(void *vedata);

	void (*draw_background)(void *vedata);
	void (*draw_scene)(void *vedata);

	void (*view_update)(void *vedata);
	void (*id_update)(void *vedata, struct ID *id);
} DrawEngineType;

#ifndef __DRW_ENGINE_H__
/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
	struct GPUFrameBuffer *default_fb;
	struct GPUFrameBuffer *multisample_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
	struct GPUTexture *color;
	struct GPUTexture *depth;
	struct GPUTexture *multisample_color;
	struct GPUTexture *multisample_depth;
} DefaultTextureList;
#endif

/* Textures */

typedef enum {
	DRW_TEX_RGBA_8,
	DRW_TEX_RGBA_16,
	DRW_TEX_RGBA_32,
	DRW_TEX_RGB_11_11_10,
	DRW_TEX_RGB_8,
	DRW_TEX_RGB_16,
	DRW_TEX_RGB_32,
	DRW_TEX_RG_8,
	DRW_TEX_RG_16,
	DRW_TEX_RG_32,
	DRW_TEX_R_8,
	DRW_TEX_R_16,
	DRW_TEX_R_32,
	DRW_TEX_DEPTH_16,
	DRW_TEX_DEPTH_24,
	DRW_TEX_DEPTH_24_STENCIL_8,
	DRW_TEX_DEPTH_32,
} DRWTextureFormat;

typedef enum {
	DRW_TEX_FILTER = (1 << 0),
	DRW_TEX_WRAP = (1 << 1),
	DRW_TEX_COMPARE = (1 << 2),
	DRW_TEX_MIPMAP = (1 << 3),
	DRW_TEX_TEMP = (1 << 4),
} DRWTextureFlag;

struct GPUTexture *DRW_texture_create_1D(
        int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_2D(
        int w, int h, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_2D_array(
        int w, int h, int d, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_3D(
        int w, int h, int d, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_cube(
        int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
void DRW_texture_generate_mipmaps(struct GPUTexture *tex);
void DRW_texture_update(struct GPUTexture *tex, const float *pixels);
void DRW_texture_free(struct GPUTexture *tex);
#define DRW_TEXTURE_FREE_SAFE(tex) do { \
	if (tex != NULL) { \
		DRW_texture_free(tex); \
		tex = NULL; \
	} \
} while (0)

/* UBOs */
struct GPUUniformBuffer *DRW_uniformbuffer_create(int size, const void *data);
void DRW_uniformbuffer_update(struct GPUUniformBuffer *ubo, const void *data);
void DRW_uniformbuffer_free(struct GPUUniformBuffer *ubo);
#define DRW_UBO_FREE_SAFE(ubo) do { \
	if (ubo != NULL) { \
		DRW_uniformbuffer_free(ubo); \
		ubo = NULL; \
	} \
} while (0)

/* Buffers */
#define MAX_FBO_TEX			5

typedef struct DRWFboTexture {
	struct GPUTexture **tex;
	int format;
	DRWTextureFlag flag;
} DRWFboTexture;

void DRW_framebuffer_init(
        struct GPUFrameBuffer **fb, void *engine_type, int width, int height,
        DRWFboTexture textures[MAX_FBO_TEX], int textures_len);
void DRW_framebuffer_bind(struct GPUFrameBuffer *fb);
void DRW_framebuffer_clear(bool color, bool depth, bool stencil, float clear_col[4], float clear_depth);
void DRW_framebuffer_read_data(int x, int y, int w, int h, int channels, int slot, float *data);
void DRW_framebuffer_texture_attach(struct GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int mip);
void DRW_framebuffer_texture_layer_attach(struct GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int layer, int mip);
void DRW_framebuffer_cubeface_attach(struct GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int face, int mip);
void DRW_framebuffer_texture_detach(struct GPUTexture *tex);
void DRW_framebuffer_blit(struct GPUFrameBuffer *fb_read, struct GPUFrameBuffer *fb_write, bool depth, bool stencil);
void DRW_framebuffer_recursive_downsample(
        struct GPUFrameBuffer *fb, struct GPUTexture *tex, int num_iter,
        void (*callback)(void *userData, int level), void *userData);
void DRW_framebuffer_viewport_size(struct GPUFrameBuffer *fb_read, int x, int y, int w, int h);
void DRW_framebuffer_free(struct GPUFrameBuffer *fb);
#define DRW_FRAMEBUFFER_FREE_SAFE(fb) do { \
	if (fb != NULL) { \
		DRW_framebuffer_free(fb); \
		fb = NULL; \
	} \
} while (0)

void DRW_transform_to_display(struct GPUTexture *tex);

/* Shaders */
struct GPUShader *DRW_shader_create(
        const char *vert, const char *geom, const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_with_lib(
        const char *vert, const char *geom, const char *frag, const char *lib, const char *defines);
struct GPUShader *DRW_shader_create_2D(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3D(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_fullscreen(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3D_depth_only(void);
void DRW_shader_free(struct GPUShader *shader);
#define DRW_SHADER_FREE_SAFE(shader) do { \
	if (shader != NULL) { \
		DRW_shader_free(shader); \
		shader = NULL; \
	} \
} while (0)

/* Batches */

typedef enum {
	DRW_STATE_WRITE_DEPTH   = (1 << 0),
	DRW_STATE_WRITE_COLOR   = (1 << 1),
	DRW_STATE_DEPTH_LESS    = (1 << 2),
	DRW_STATE_DEPTH_EQUAL   = (1 << 3),
	DRW_STATE_DEPTH_GREATER = (1 << 4),
	DRW_STATE_DEPTH_ALWAYS  = (1 << 5),
	DRW_STATE_CULL_BACK     = (1 << 6),
	DRW_STATE_CULL_FRONT    = (1 << 7),
	DRW_STATE_WIRE          = (1 << 8),
	DRW_STATE_WIRE_LARGE    = (1 << 9),
	DRW_STATE_POINT         = (1 << 10),
	DRW_STATE_STIPPLE_2     = (1 << 11),
	DRW_STATE_STIPPLE_3     = (1 << 12),
	DRW_STATE_STIPPLE_4     = (1 << 13),
	DRW_STATE_BLEND         = (1 << 14),
	DRW_STATE_ADDITIVE      = (1 << 15),
	DRW_STATE_MULTIPLY      = (1 << 16),
	DRW_STATE_TRANSMISSION  = (1 << 17),
	DRW_STATE_CLIP_PLANES   = (1 << 18),

	DRW_STATE_WRITE_STENCIL    = (1 << 27),
	DRW_STATE_STENCIL_EQUAL    = (1 << 28),
} DRWState;

#define DRW_STATE_DEFAULT (DRW_STATE_WRITE_DEPTH | DRW_STATE_WRITE_COLOR | DRW_STATE_DEPTH_LESS)


DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_material_create(struct GPUMaterial *material, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_material_instance_create(
        struct GPUMaterial *material, DRWPass *pass, struct Gwn_Batch *geom, struct Object *ob);
DRWShadingGroup *DRW_shgroup_material_empty_tri_batch_create(struct GPUMaterial *material, DRWPass *pass, int size);
DRWShadingGroup *DRW_shgroup_instance_create(struct GPUShader *shader, DRWPass *pass, struct Gwn_Batch *geom);
DRWShadingGroup *DRW_shgroup_point_batch_create(struct GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_line_batch_create(struct GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_empty_tri_batch_create(struct GPUShader *shader, DRWPass *pass, int size);

typedef void (DRWCallGenerateFn)(
        DRWShadingGroup *shgroup,
        void (*draw_fn)(DRWShadingGroup *shgroup, struct Gwn_Batch *geom),
        void *user_data);

void DRW_shgroup_instance_batch(DRWShadingGroup *shgroup, struct Gwn_Batch *instances);

void DRW_shgroup_free(struct DRWShadingGroup *shgroup);
void DRW_shgroup_call_add(DRWShadingGroup *shgroup, struct Gwn_Batch *geom, float (*obmat)[4]);
void DRW_shgroup_call_object_add(DRWShadingGroup *shgroup, struct Gwn_Batch *geom, struct Object *ob);
void DRW_shgroup_call_sculpt_add(DRWShadingGroup *shgroup, struct Object *ob, float (*obmat)[4]);
void DRW_shgroup_call_generate_add(
        DRWShadingGroup *shgroup, DRWCallGenerateFn *geometry_fn, void *user_data, float (*obmat)[4]);
void DRW_shgroup_call_dynamic_add_array(DRWShadingGroup *shgroup, const void *attr[], unsigned int attr_len);
#define DRW_shgroup_call_dynamic_add(shgroup, ...) do { \
	const void *array[] = {__VA_ARGS__}; \
	DRW_shgroup_call_dynamic_add_array(shgroup, array, (sizeof(array) / sizeof(*array))); \
} while (0)
/* Use this only to make your instances selectable. */
#define DRW_shgroup_call_dynamic_add_empty(shgroup) do { \
	DRW_shgroup_call_dynamic_add_array(shgroup, NULL, 0); \
} while (0)
/* Use this to set a high number of instances. */
void DRW_shgroup_set_instance_count(DRWShadingGroup *shgroup, int count);

void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state);
void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state);
void DRW_shgroup_stencil_mask(DRWShadingGroup *shgroup, unsigned int mask);
void DRW_shgroup_attrib_float(DRWShadingGroup *shgroup, const char *name, int size);

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const struct GPUTexture *tex);
void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup, const char *name, const struct GPUUniformBuffer *ubo);
void DRW_shgroup_uniform_buffer(DRWShadingGroup *shgroup, const char *name, struct GPUTexture **tex);
void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup, const char *name, const bool *value, int arraysize);
void DRW_shgroup_uniform_float(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_vec2(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_vec3(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_short_to_int(DRWShadingGroup *shgroup, const char *name, const short *value, int arraysize);
void DRW_shgroup_uniform_short_to_float(DRWShadingGroup *shgroup, const char *name, const short *value, int arraysize);
void DRW_shgroup_uniform_int(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize);
void DRW_shgroup_uniform_ivec2(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize);
void DRW_shgroup_uniform_ivec3(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize);
void DRW_shgroup_uniform_mat3(DRWShadingGroup *shgroup, const char *name, const float *value);
void DRW_shgroup_uniform_mat4(DRWShadingGroup *shgroup, const char *name, const float *value);

/* Passes */
DRWPass *DRW_pass_create(const char *name, DRWState state);
void DRW_pass_foreach_shgroup(DRWPass *pass, void (*callback)(void *userData, DRWShadingGroup *shgrp), void *userData);
void DRW_pass_sort_shgroup_z(DRWPass *pass);

/* Viewport */
typedef enum {
	DRW_MAT_PERS = 0,
	DRW_MAT_PERSINV,
	DRW_MAT_VIEW,
	DRW_MAT_VIEWINV,
	DRW_MAT_WIN,
	DRW_MAT_WININV,
} DRWViewportMatrixType;

void DRW_viewport_init(const bContext *C);
void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type);
void DRW_viewport_matrix_override_set(float mat[4][4], DRWViewportMatrixType type);
void DRW_viewport_matrix_override_unset(DRWViewportMatrixType type);
const float *DRW_viewport_size_get(void);
const float *DRW_viewport_screenvecs_get(void);
const float *DRW_viewport_pixelsize_get(void);
bool DRW_viewport_is_persp_get(void);

struct DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void);
struct DefaultTextureList     *DRW_viewport_texture_list_get(void);

void DRW_viewport_request_redraw(void);

/* ViewLayers */
void *DRW_view_layer_engine_data_get(DrawEngineType *engine_type);
void **DRW_view_layer_engine_data_ensure(DrawEngineType *engine_type, void (*callback)(void *storage));

/* Objects */
void *DRW_object_engine_data_get(Object *ob, DrawEngineType *engine_type);
void **DRW_object_engine_data_ensure(
        Object *ob, DrawEngineType *engine_type, void (*callback)(void *storage));
struct LampEngineData *DRW_lamp_engine_data_ensure(Object *ob, struct RenderEngineType *engine_type);
void DRW_lamp_engine_data_free(struct LampEngineData *led);

/* Settings */
bool DRW_object_is_renderable(struct Object *ob);
bool DRW_object_is_flat_normal(const struct Object *ob);
int  DRW_object_is_mode_shade(const struct Object *ob);

/* Draw commands */
void DRW_draw_pass(DRWPass *pass);
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group);

void DRW_draw_text_cache_queue(struct DRWTextStore *dt);

void DRW_draw_callbacks_pre_scene(void);
void DRW_draw_callbacks_post_scene(void);

int DRW_draw_region_engine_info_offset(void);
void DRW_draw_region_engine_info(void);

void DRW_state_reset_ex(DRWState state);
void DRW_state_reset(void);

void DRW_state_invert_facing(void);

void DRW_state_clip_planes_add(float plane_eq[4]);
void DRW_state_clip_planes_reset(void);

/* Selection */
void DRW_select_load_id(unsigned int id);

/* Draw State */
void DRW_state_dfdy_factors_get(float dfdyfac[2]);
bool DRW_state_is_fbo(void);
bool DRW_state_is_select(void);
bool DRW_state_is_depth(void);
bool DRW_state_is_image_render(void);
bool DRW_state_is_scene_render(void);
bool DRW_state_show_text(void);
bool DRW_state_draw_support(void);

struct DRWTextStore *DRW_state_text_cache_get(void);

/* Avoid too many lookups while drawing */
typedef struct DRWContextState {
	struct ARegion *ar;         /* 'CTX_wm_region(C)' */
	struct RegionView3D *rv3d;  /* 'CTX_wm_region_view3d(C)' */
	struct View3D *v3d;     /* 'CTX_wm_view3d(C)' */

	struct Scene *scene;    /* 'CTX_data_scene(C)' */
	struct ViewLayer *view_layer;  /* 'CTX_data_view_layer(C)' */

	/* Use 'scene->obedit' for edit-mode */
	struct Object *obact;   /* 'OBACT' */

	struct RenderEngineType *engine_type;

	/* Last resort (some functions take this as an arg so we can't easily avoid).
	 * May be NULL when used for selection or depth buffer. */
	const struct bContext *evil_C;
} DRWContextState;

const DRWContextState *DRW_context_state_get(void);

#endif /* __DRW_RENDER_H__ */

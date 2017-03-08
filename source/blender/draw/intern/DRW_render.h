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

#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "draw_common.h"
#include "draw_cache.h"
#include "draw_view.h"

#include "MEM_guardedalloc.h"

#include "RE_engine.h"

struct bContext;
struct GPUFrameBuffer;
struct GPUShader;
struct GPUTexture;
struct GPUUniformBuffer;
struct Object;
struct Batch;
struct DefaultFramebufferList;
struct DefaultTextureList;

typedef struct DRWUniform DRWUniform;
typedef struct DRWInterface DRWInterface;
typedef struct DRWPass DRWPass;
typedef struct DRWShadingGroup DRWShadingGroup;

typedef struct DrawEngineType {
	struct DrawEngineType *next, *prev;

	char idname[32];

	void (*engine_init)(void);
	void (*engine_free)(void);

	void (*cache_init)(void);
	void (*cache_populate)(struct Object *ob);
	void (*cache_finish)(void);

	void (*draw_background)(void);
	void (*draw_scene)(void);
} DrawEngineType;

#ifndef __DRW_ENGINE_H__
/* Buffer and textures used by the viewport by default */
typedef struct DefaultFramebufferList {
	struct GPUFrameBuffer *default_fb;
} DefaultFramebufferList;

typedef struct DefaultTextureList {
	struct GPUTexture *color;
	struct GPUTexture *depth;
} DefaultTextureList;
#endif

/* Textures */

typedef enum {
	DRW_TEX_RGBA_8,
	DRW_TEX_RGBA_16,
	DRW_TEX_RGBA_32,
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
	DRW_TEX_DEPTH_32,
} DRWTextureFormat;

typedef enum {
	DRW_TEX_FILTER = (1 << 0),
	DRW_TEX_WRAP = (1 << 1),
	DRW_TEX_COMPARE = (1 << 2),
} DRWTextureFlag;

struct GPUTexture *DRW_texture_create_1D(
        int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_2D(
        int w, int h, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels);
struct GPUTexture *DRW_texture_create_2D_array(
        int w, int h, int d, DRWTextureFormat UNUSED(format), DRWTextureFlag flags, const float *fpixels);
void DRW_texture_free(struct GPUTexture *tex);

/* UBOs */
struct GPUUniformBuffer *DRW_uniformbuffer_create(int size, const void *data);
void DRW_uniformbuffer_update(struct GPUUniformBuffer *ubo, const void *data);
void DRW_uniformbuffer_free(struct GPUUniformBuffer *ubo);

/* Buffers */

/* DRWFboTexture->format */
#define DRW_BUF_DEPTH_16		1
#define DRW_BUF_DEPTH_24		2
#define DRW_BUF_R_8				3
#define DRW_BUF_R_16			4
#define DRW_BUF_R_32			5
#define DRW_BUF_RG_8			6
#define DRW_BUF_RG_16			7
#define DRW_BUF_RG_32			8
#define DRW_BUF_RGB_8			9
#define DRW_BUF_RGB_16			10
#define DRW_BUF_RGB_32			11
#define DRW_BUF_RGBA_8			12
#define DRW_BUF_RGBA_16			13
#define DRW_BUF_RGBA_32			14

#define MAX_FBO_TEX			5

typedef struct DRWFboTexture {
	struct GPUTexture **tex;
	int format;
} DRWFboTexture;

void DRW_framebuffer_init(struct GPUFrameBuffer **fb, int width, int height, DRWFboTexture textures[MAX_FBO_TEX], int texnbr);
void DRW_framebuffer_bind(struct GPUFrameBuffer *fb);
void DRW_framebuffer_clear(bool color, bool depth, float clear_col[4], float clear_depth);
void DRW_framebuffer_texture_attach(struct GPUFrameBuffer *fb, struct GPUTexture *tex, int slot);
void DRW_framebuffer_texture_detach(struct GPUTexture *tex);
void DRW_framebuffer_blit(struct GPUFrameBuffer *fb_read, struct GPUFrameBuffer *fb_write, bool depth);
/* Shaders */
struct GPUShader *DRW_shader_create(const char *vert, const char *geom, const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_2D(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3D(const char *frag, const char *defines);
struct GPUShader *DRW_shader_create_3D_depth_only(void);
void DRW_shader_free(struct GPUShader *shader);

/* Batches */

typedef enum {
	DRW_STATE_WRITE_DEPTH   = (1 << 0),
	DRW_STATE_WRITE_COLOR   = (1 << 1),
	DRW_STATE_DEPTH_LESS    = (1 << 2),
	DRW_STATE_DEPTH_EQUAL   = (1 << 3),
	DRW_STATE_DEPTH_GREATER = (1 << 4),
	DRW_STATE_CULL_BACK     = (1 << 5),
	DRW_STATE_CULL_FRONT    = (1 << 6),
	DRW_STATE_WIRE          = (1 << 7),
	DRW_STATE_WIRE_LARGE    = (1 << 8),
	DRW_STATE_POINT         = (1 << 9),
	DRW_STATE_STIPPLE_2     = (1 << 10),
	DRW_STATE_STIPPLE_3     = (1 << 11),
	DRW_STATE_STIPPLE_4     = (1 << 12),
	DRW_STATE_BLEND         = (1 << 13),
} DRWState;

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_instance_create(struct GPUShader *shader, DRWPass *pass, struct Batch *geom);
DRWShadingGroup *DRW_shgroup_point_batch_create(struct GPUShader *shader, DRWPass *pass);
DRWShadingGroup *DRW_shgroup_line_batch_create(struct GPUShader *shader, DRWPass *pass);

void DRW_shgroup_free(struct DRWShadingGroup *shgroup);
void DRW_shgroup_call_add(DRWShadingGroup *shgroup, struct Batch *geom, float (*obmat)[4]);
void DRW_shgroup_dynamic_call_add(DRWShadingGroup *shgroup, ...);
void DRW_shgroup_state_set(DRWShadingGroup *shgroup, DRWState state);
void DRW_shgroup_attrib_int(DRWShadingGroup *shgroup, const char *name, int size);
void DRW_shgroup_attrib_float(DRWShadingGroup *shgroup, const char *name, int size);

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const struct GPUTexture *tex, int loc);
void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup, const char *name, const struct GPUUniformBuffer *ubo, int loc);
void DRW_shgroup_uniform_buffer(DRWShadingGroup *shgroup, const char *name, struct GPUTexture **tex, int loc);
void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup, const char *name, const bool *value, int arraysize);
void DRW_shgroup_uniform_float(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_vec2(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_vec3(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize);
void DRW_shgroup_uniform_int(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize);
void DRW_shgroup_uniform_ivec2(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize);
void DRW_shgroup_uniform_ivec3(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize);
void DRW_shgroup_uniform_mat3(DRWShadingGroup *shgroup, const char *name, const float *value);
void DRW_shgroup_uniform_mat4(DRWShadingGroup *shgroup, const char *name, const float *value);

/* Passes */
DRWPass *DRW_pass_create(const char *name, DRWState state);

/* Viewport */
typedef enum {
	DRW_MAT_PERS,
	DRW_MAT_WIEW,
	DRW_MAT_WIN,
} DRWViewportMatrixType;

void DRW_viewport_init(const bContext *C);
void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type);
void DRW_viewport_engine_data_set(const char *engine_name, void *fbl, void *txl, void *psl,	void *stl);
void *DRW_viewport_engine_data_get(const char *engine_name);
float *DRW_viewport_size_get(void);
float *DRW_viewport_screenvecs_get(void);
float *DRW_viewport_pixelsize_get(void);
bool DRW_viewport_is_persp_get(void);
bool DRW_viewport_cache_is_dirty(void);

struct DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void);
struct DefaultTextureList     *DRW_viewport_texture_list_get(void);

/* Settings */
bool DRW_is_object_renderable(struct Object *ob);
#ifndef __DRW_ENGINE_H__
void *DRW_material_settings_get(Material *ma, const char *engine_name);
void *DRW_render_settings_get(Scene *scene, const char *engine_name);
#endif /* __DRW_ENGINE_H__ */

/* Draw commands */
void DRW_draw_pass(DRWPass *pass);

void DRW_draw_callbacks_pre_scene(void);
void DRW_draw_callbacks_post_scene(void);

void DRW_state_reset(void);

/* Other */
void DRW_get_dfdy_factors(float dfdyfac[2]);
const struct bContext *DRW_get_context(void);
#endif /* __DRW_RENDER_H__ */

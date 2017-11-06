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

/** \file blender/draw/intern/draw_manager.c
 *  \ingroup draw
 */

#include <stdio.h>

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_mempool.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BIF_glutil.h"

#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_pbvh.h"
#include "BKE_paint.h"
#include "BKE_workspace.h"

#include "BLT_translation.h"
#include "BLF_api.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "intern/gpu_codegen.h"
#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_lamp.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"
#include "GPU_matrix.h"

#include "IMB_colormanagement.h"

#include "RE_engine.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "draw_manager_text.h"
#include "draw_manager_profiling.h"

/* only for callbacks */
#include "draw_cache_impl.h"

#include "draw_mode_engines.h"
#include "engines/clay/clay_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/basic/basic_engine.h"
#include "engines/external/external_engine.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* -------------------------------------------------------------------- */
/** \name Local Features
 * \{ */

#define USE_PROFILE

#ifdef USE_PROFILE
#include "PIL_time.h"

#define PROFILE_TIMER_FALLOFF 0.1

#define PROFILE_START(time_start) \
	double time_start = PIL_check_seconds_timer();

#define PROFILE_END_ACCUM(time_accum, time_start) { \
	time_accum += (PIL_check_seconds_timer() - time_start) * 1e3; \
} ((void)0)

/* exp average */
#define PROFILE_END_UPDATE(time_update, time_start) { \
	double _time_delta = (PIL_check_seconds_timer() - time_start) * 1e3; \
	time_update = (time_update * (1.0 - PROFILE_TIMER_FALLOFF)) + \
	              (_time_delta * PROFILE_TIMER_FALLOFF); \
} ((void)0)

#else

#define PROFILE_START(time_start) ((void)0)
#define PROFILE_END_ACCUM(time_accum, time_start) ((void)0)
#define PROFILE_END_UPDATE(time_update, time_start) ((void)0)

#endif  /* USE_PROFILE */


/* Use draw manager to call GPU_select, see: DRW_draw_select_loop */
#define USE_GPU_SELECT

#ifdef USE_GPU_SELECT
#  include "ED_view3d.h"
#  include "ED_armature.h"
#  include "GPU_select.h"
#endif

/** \} */


#define MAX_ATTRIB_NAME 32
#define MAX_ATTRIB_COUNT 6 /* Can be adjusted for more */
#define MAX_PASS_NAME 32
#define MAX_CLIP_PLANES 6 /* GL_MAX_CLIP_PLANES is at least 6 */

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_fullscreen_vert_glsl[];

/* Prototypes. */
static void DRW_engines_enable_external(void);

/* Structures */
typedef enum {
	DRW_UNIFORM_BOOL,
	DRW_UNIFORM_SHORT_TO_INT,
	DRW_UNIFORM_SHORT_TO_FLOAT,
	DRW_UNIFORM_INT,
	DRW_UNIFORM_FLOAT,
	DRW_UNIFORM_TEXTURE,
	DRW_UNIFORM_BUFFER,
	DRW_UNIFORM_MAT3,
	DRW_UNIFORM_MAT4,
	DRW_UNIFORM_BLOCK
} DRWUniformType;

typedef enum {
	DRW_ATTRIB_INT,
	DRW_ATTRIB_FLOAT,
} DRWAttribType;

struct DRWUniform {
	struct DRWUniform *next;
	DRWUniformType type;
	int location;
	int length;
	int arraysize;
	const void *value;
};

typedef struct DRWAttrib {
	struct DRWAttrib *prev;
	char name[MAX_ATTRIB_NAME];
	int location;
	int format_id;
	int size; /* number of component */
	int type;
} DRWAttrib;

struct DRWInterface {
	DRWUniform *uniforms;   /* DRWUniform, single-linked list */
	DRWAttrib *attribs;     /* DRWAttrib, single-linked list */
	DRWAttrib *attribs_first; /* First added attrib to traverse in the right order */
	int attribs_count;
	int attribs_stride;
	int attribs_size[16];
	int attribs_loc[16];
	/* matrices locations */
	int model;
	int modelinverse;
	int modelview;
	int modelviewinverse;
	int projection;
	int projectioninverse;
	int view;
	int viewinverse;
	int modelviewprojection;
	int viewprojection;
	int viewprojectioninverse;
	int normal;
	int worldnormal;
	int camtexfac;
	int orcotexfac;
	int eye;
	int clipplanes;
	/* Dynamic batch */
	Gwn_Batch *instance_batch; /* contains instances attributes */
	GLuint instance_vbo; /* same as instance_batch but generated from DRWCalls */
	int instance_count;
	Gwn_VertFormat vbo_format;
};

struct DRWPass {
	/* Single linked list with last member to append */
	DRWShadingGroup *shgroups;
	DRWShadingGroup *shgroups_last;

	DRWState state;
	char name[MAX_PASS_NAME];
};

typedef struct DRWCallHeader {
	void *prev;

#ifdef USE_GPU_SELECT
	int select_id;
#endif
	uchar type;
} DRWCallHeader;

typedef struct DRWCall {
	DRWCallHeader head;

	float obmat[4][4];
	Gwn_Batch *geometry;

	Object *ob; /* Optional */
	ID *ob_data; /* Optional. */
} DRWCall;

typedef struct DRWCallGenerate {
	DRWCallHeader head;

	float obmat[4][4];

	DRWCallGenerateFn *geometry_fn;
	void *user_data;
} DRWCallGenerate;

typedef struct DRWCallDynamic {
	DRWCallHeader head;

	const void *data[MAX_ATTRIB_COUNT];
} DRWCallDynamic;

struct DRWShadingGroup {
	struct DRWShadingGroup *next;

	GPUShader *shader;               /* Shader to bind */
	DRWInterface interface;          /* Uniforms pointers */

	/* DRWCall or DRWCallDynamic depending of type */
	void *calls;
	void *calls_first; /* To be able to traverse the list in the order of addition */

	DRWState state_extra;            /* State changes for this batch only (or'd with the pass's state) */
	DRWState state_extra_disable;    /* State changes for this batch only (and'd with the pass's state) */
	int type;

	ID *instance_data;         /* Object->data to instance */
	Gwn_Batch *instance_geom;  /* Geometry to instance */
	Gwn_Batch *batch_geom;     /* Result of call batching */

#ifdef USE_GPU_SELECT
	/* backlink to pass we're in */
	DRWPass *pass_parent;
#endif
};

/* Used by DRWShadingGroup.type */
enum {
	DRW_SHG_NORMAL,
	DRW_SHG_POINT_BATCH,
	DRW_SHG_LINE_BATCH,
	DRW_SHG_TRIANGLE_BATCH,
	DRW_SHG_INSTANCE,
};

/* Used by DRWCall.type */
enum {
	/* A single batch */
	DRW_CALL_SINGLE,
	/* Uses a callback to draw with any number of batches. */
	DRW_CALL_GENERATE,
	/* Arbitrary number of multiple args. */
	DRW_CALL_DYNAMIC,
};

/* only 16 bits long */
enum {
	STENCIL_SELECT          = (1 << 0),
	STENCIL_ACTIVE          = (1 << 1),
};

/** Render State: No persistent data between draw calls. */
static struct DRWGlobalState {
	/* Cache generation */
	ViewportMemoryPool *vmempool;
	DRWUniform *last_uniform;
	DRWAttrib *last_attrib;
	DRWCall *last_call;
	DRWCallGenerate *last_callgenerate;
	DRWCallDynamic *last_calldynamic;
	DRWShadingGroup *last_shgroup;

	/* Rendering state */
	GPUShader *shader;

	/* Managed by `DRW_state_set`, `DRW_state_reset` */
	DRWState state;

	/* Per viewport */
	GPUViewport *viewport;
	struct GPUFrameBuffer *default_framebuffer;
	float size[2];
	float screenvecs[2][3];
	float pixsize;

	GLenum backface, frontface;

	/* Clip planes */
	int num_clip_planes;
	float clip_planes_eq[MAX_CLIP_PLANES][4];

	struct {
		unsigned int is_select : 1;
		unsigned int is_depth : 1;
		unsigned int is_image_render : 1;
		unsigned int is_scene_render : 1;
	} options;

	/* Current rendering context */
	DRWContextState draw_ctx;

	/* Convenience pointer to text_store owned by the viewport */
	struct DRWTextStore **text_store_p;

	ListBase enabled_engines; /* RenderEngineType */

	/* Profiling */
	double cache_time;
} DST = {NULL};

/** GPU Resource State: Memory storage between drawing. */
static struct DRWResourceState {
	GPUTexture **bound_texs;

	bool *bound_tex_slots;

	int bind_tex_inc;
	int bind_ubo_inc;
} RST = {NULL};

static struct DRWMatrixOveride {
	float mat[6][4][4];
	bool override[6];
} viewport_matrix_override = {{{{0}}}};

ListBase DRW_engines = {NULL, NULL};

#ifdef USE_GPU_SELECT
static unsigned int g_DRW_select_id = (unsigned int)-1;

void DRW_select_load_id(unsigned int id)
{
	BLI_assert(G.f & G_PICKSEL);
	g_DRW_select_id = id;
}
#endif


/* -------------------------------------------------------------------- */

/** \name Textures (DRW_texture)
 * \{ */

static void drw_texture_get_format(
        DRWTextureFormat format,
        GPUTextureFormat *r_data_type, int *r_channels)
{
	switch (format) {
		case DRW_TEX_RGBA_8: *r_data_type = GPU_RGBA8; break;
		case DRW_TEX_RGBA_16: *r_data_type = GPU_RGBA16F; break;
		case DRW_TEX_RGB_16: *r_data_type = GPU_RGB16F; break;
		case DRW_TEX_RGB_11_11_10: *r_data_type = GPU_R11F_G11F_B10F; break;
		case DRW_TEX_RG_8: *r_data_type = GPU_RG8; break;
		case DRW_TEX_RG_16: *r_data_type = GPU_RG16F; break;
		case DRW_TEX_RG_32: *r_data_type = GPU_RG32F; break;
		case DRW_TEX_R_8: *r_data_type = GPU_R8; break;
		case DRW_TEX_R_16: *r_data_type = GPU_R16F; break;
		case DRW_TEX_R_32: *r_data_type = GPU_R32F; break;
#if 0
		case DRW_TEX_RGBA_32: *r_data_type = GPU_RGBA32F; break;
		case DRW_TEX_RGB_8: *r_data_type = GPU_RGB8; break;
		case DRW_TEX_RGB_32: *r_data_type = GPU_RGB32F; break;
#endif
		case DRW_TEX_DEPTH_16: *r_data_type = GPU_DEPTH_COMPONENT16; break;
		case DRW_TEX_DEPTH_24: *r_data_type = GPU_DEPTH_COMPONENT24; break;
		case DRW_TEX_DEPTH_32: *r_data_type = GPU_DEPTH_COMPONENT32F; break;
		default :
			/* file type not supported you must uncomment it from above */
			BLI_assert(false);
			break;
	}

	switch (format) {
		case DRW_TEX_RGBA_8:
		case DRW_TEX_RGBA_16:
		case DRW_TEX_RGBA_32:
			*r_channels = 4;
			break;
		case DRW_TEX_RGB_8:
		case DRW_TEX_RGB_16:
		case DRW_TEX_RGB_32:
		case DRW_TEX_RGB_11_11_10:
			*r_channels = 3;
			break;
		case DRW_TEX_RG_8:
		case DRW_TEX_RG_16:
		case DRW_TEX_RG_32:
			*r_channels = 2;
			break;
		default:
			*r_channels = 1;
			break;
	}
}

static void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags)
{
	GPU_texture_bind(tex, 0);
	if (flags & DRW_TEX_MIPMAP) {
		GPU_texture_mipmap_mode(tex, true, flags & DRW_TEX_FILTER);
		DRW_texture_generate_mipmaps(tex);
	}
	else {
		GPU_texture_filter_mode(tex, flags & DRW_TEX_FILTER);
	}
	GPU_texture_wrap_mode(tex, flags & DRW_TEX_WRAP);
	GPU_texture_compare_mode(tex, flags & DRW_TEX_COMPARE);
	GPU_texture_unbind(tex);
}

GPUTexture *DRW_texture_create_1D(int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_1D_custom(w, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D(int w, int h, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_2D_custom(w, h, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_2D_array(
        int w, int h, int d, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_2D_array_custom(w, h, d, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_3D(
        int w, int h, int d, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_3D_custom(w, h, d, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

GPUTexture *DRW_texture_create_cube(int w, DRWTextureFormat format, DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;
	GPUTextureFormat data_type;
	int channels;

	drw_texture_get_format(format, &data_type, &channels);
	tex = GPU_texture_create_cube_custom(w, channels, data_type, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

void DRW_texture_generate_mipmaps(GPUTexture *tex)
{
	GPU_texture_bind(tex, 0);
	GPU_texture_generate_mipmap(tex);
	GPU_texture_unbind(tex);
}

void DRW_texture_update(GPUTexture *tex, const float *pixels)
{
	GPU_texture_update(tex, pixels);
}

void DRW_texture_free(GPUTexture *tex)
{
	GPU_texture_free(tex);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Uniform Buffer Object (DRW_uniformbuffer)
 * \{ */

GPUUniformBuffer *DRW_uniformbuffer_create(int size, const void *data)
{
	return GPU_uniformbuffer_create(size, data, NULL);
}

void DRW_uniformbuffer_update(GPUUniformBuffer *ubo, const void *data)
{
	GPU_uniformbuffer_update(ubo, data);
}

void DRW_uniformbuffer_free(GPUUniformBuffer *ubo)
{
	GPU_uniformbuffer_free(ubo);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Shaders (DRW_shader)
 * \{ */

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

	DynStr *ds_vert = BLI_dynstr_new();
	BLI_dynstr_append(ds_vert, lib);
	BLI_dynstr_append(ds_vert, vert);
	vert_with_lib = BLI_dynstr_get_cstring(ds_vert);
	BLI_dynstr_free(ds_vert);

	DynStr *ds_frag = BLI_dynstr_new();
	BLI_dynstr_append(ds_frag, lib);
	BLI_dynstr_append(ds_frag, frag);
	frag_with_lib = BLI_dynstr_get_cstring(ds_frag);
	BLI_dynstr_free(ds_frag);

	if (geom) {
		DynStr *ds_geom = BLI_dynstr_new();
		BLI_dynstr_append(ds_geom, lib);
		BLI_dynstr_append(ds_geom, geom);
		geom_with_lib = BLI_dynstr_get_cstring(ds_geom);
		BLI_dynstr_free(ds_geom);
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

void DRW_shader_free(GPUShader *shader)
{
	GPU_shader_free(shader);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Interface (DRW_interface)
 * \{ */

static void DRW_interface_create(DRWInterface *interface, GPUShader *shader)
{
	interface->model = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_MODEL);
	interface->modelinverse = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_MODEL_INV);
	interface->modelview = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_MODELVIEW);
	interface->modelviewinverse = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_MODELVIEW_INV);
	interface->projection = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_PROJECTION);
	interface->projectioninverse = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_PROJECTION_INV);
	interface->view = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_VIEW);
	interface->viewinverse = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_VIEW_INV);
	interface->viewprojection = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_VIEWPROJECTION);
	interface->viewprojectioninverse = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_VIEWPROJECTION_INV);
	interface->modelviewprojection = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_MVP);
	interface->normal = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_NORMAL);
	interface->worldnormal = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_WORLDNORMAL);
	interface->camtexfac = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_CAMERATEXCO);
	interface->orcotexfac = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_ORCO);
	interface->clipplanes = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_CLIPPLANES);
	interface->eye = GPU_shader_get_builtin_uniform(shader, GWN_UNIFORM_EYE);
	interface->instance_count = 0;
	interface->attribs_count = 0;
	interface->attribs_stride = 0;
	interface->instance_vbo = 0;
	interface->instance_batch = NULL;

	memset(&interface->vbo_format, 0, sizeof(Gwn_VertFormat));

	interface->uniforms = NULL;
	interface->attribs = NULL;
	interface->attribs_first = NULL;
}


static void DRW_interface_uniform(DRWShadingGroup *shgroup, const char *name,
                                  DRWUniformType type, const void *value, int length, int arraysize)
{
	int location;
	if (type == DRW_UNIFORM_BLOCK) {
		location = GPU_shader_get_uniform_block(shgroup->shader, name);
	}
	else {
		location = GPU_shader_get_uniform(shgroup->shader, name);
	}

	if (location == -1) {
		if (G.debug & G_DEBUG)
			fprintf(stderr, "Uniform '%s' not found!\n", name);
		/* Nice to enable eventually, for now eevee uses uniforms that might not exist. */
		// BLI_assert(0);
		return;
	}

	DRWUniform *uni = BLI_mempool_alloc(DST.vmempool->uniforms);

	BLI_assert(arraysize > 0);

	uni->location = location;
	uni->type = type;
	uni->value = value;
	uni->length = length;
	uni->arraysize = arraysize;

	/* Prepend */
	uni->next = shgroup->interface.uniforms;
	shgroup->interface.uniforms = uni;
}

static void DRW_interface_attrib(DRWShadingGroup *shgroup, const char *name, DRWAttribType type, int size, bool dummy)
{
	DRWAttrib *attrib = BLI_mempool_alloc(DST.vmempool->attribs);
	GLuint program = GPU_shader_get_program(shgroup->shader);

	attrib->location = glGetAttribLocation(program, name);
	attrib->type = type;
	attrib->size = size;

/* Adding attribute even if not found for now (to keep memory alignment).
 * Should ideally take vertex format automatically from batch eventually */
#if 0
	if (attrib->location == -1 && !dummy) {
		if (G.debug & G_DEBUG)
			fprintf(stderr, "Attribute '%s' not found!\n", name);
		BLI_assert(0);
		MEM_freeN(attrib);
		return;
	}
#else
	UNUSED_VARS(dummy);
#endif

	BLI_assert(BLI_strnlen(name, 32) < 32);
	BLI_strncpy(attrib->name, name, 32);

	shgroup->interface.attribs_count += 1;
	BLI_assert(shgroup->interface.attribs_count < MAX_ATTRIB_COUNT);

	/* Prepend */
	if (shgroup->interface.attribs == NULL) {
		shgroup->interface.attribs = attrib;
		shgroup->interface.attribs_first = attrib;
	}
	else {
		shgroup->interface.attribs->prev = attrib;
		shgroup->interface.attribs = attrib;
	}
	attrib->prev = NULL;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Shading Group (DRW_shgroup)
 * \{ */

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWShadingGroup *shgroup = BLI_mempool_alloc(DST.vmempool->shgroups);

	/* Append */
	if (pass->shgroups != NULL) {
		pass->shgroups_last->next = shgroup;
	}
	else {
		pass->shgroups = shgroup;
	}
	pass->shgroups_last = shgroup;
	shgroup->next = NULL;

	DRW_interface_create(&shgroup->interface, shader);

	shgroup->type = DRW_SHG_NORMAL;
	shgroup->shader = shader;
	shgroup->state_extra = 0;
	shgroup->state_extra_disable = ~0x0;
	shgroup->batch_geom = NULL;
	shgroup->instance_geom = NULL;
	shgroup->instance_data = NULL;

	shgroup->calls = NULL;
	shgroup->calls_first = NULL;

#ifdef USE_GPU_SELECT
	shgroup->pass_parent = pass;
#endif

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_material_create(struct GPUMaterial *material, DRWPass *pass)
{
	double time = 0.0; /* TODO make time variable */

	/* TODO : Ideally we should not convert. But since the whole codegen
	 * is relying on GPUPass we keep it as is for now. */
	GPUPass *gpupass = GPU_material_get_pass(material);

	if (!gpupass) {
		/* Shader compilation error */
		return NULL;
	}

	struct GPUShader *shader = GPU_pass_shader(gpupass);

	DRWShadingGroup *grp = DRW_shgroup_create(shader, pass);

	/* Converting dynamic GPUInput to DRWUniform */
	ListBase *inputs = &gpupass->inputs;

	for (GPUInput *input = inputs->first; input; input = input->next) {
		/* Textures */
		if (input->ima) {
			GPUTexture *tex = GPU_texture_from_blender(
			        input->ima, input->iuser, input->textarget, input->image_isdata, time, 1);

			if (input->bindtex) {
				DRW_shgroup_uniform_texture(grp, input->shadername, tex);
			}
		}
		/* Color Ramps */
		else if (input->tex) {
			DRW_shgroup_uniform_texture(grp, input->shadername, input->tex);
		}
		/* Floats */
		else {
			switch (input->type) {
				case GPU_FLOAT:
					DRW_shgroup_uniform_float(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case GPU_VEC2:
					DRW_shgroup_uniform_vec2(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case GPU_VEC3:
					DRW_shgroup_uniform_vec3(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case GPU_VEC4:
					DRW_shgroup_uniform_vec4(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case GPU_MAT3:
					DRW_shgroup_uniform_mat3(grp, input->shadername, (float *)input->dynamicvec);
					break;
				case GPU_MAT4:
					DRW_shgroup_uniform_mat4(grp, input->shadername, (float *)input->dynamicvec);
					break;
				default:
					break;
			}
		}
	}

	GPUUniformBuffer *ubo = GPU_material_get_uniform_buffer(material);
	if (ubo != NULL) {
		DRW_shgroup_uniform_block(grp, GPU_UBO_BLOCK_NAME, ubo);
	}

	return grp;
}

DRWShadingGroup *DRW_shgroup_material_instance_create(
        struct GPUMaterial *material, DRWPass *pass, Gwn_Batch *geom, Object *ob)
{
	DRWShadingGroup *shgroup = DRW_shgroup_material_create(material, pass);

	if (shgroup) {
		shgroup->type = DRW_SHG_INSTANCE;
		shgroup->instance_geom = geom;
		shgroup->instance_data = ob->data;
	}

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_material_empty_tri_batch_create(
        struct GPUMaterial *material, DRWPass *pass, int size)
{
	DRWShadingGroup *shgroup = DRW_shgroup_material_create(material, pass);

	if (shgroup) {
		shgroup->type = DRW_SHG_TRIANGLE_BATCH;
		shgroup->interface.instance_count = size * 3;
		DRW_interface_attrib(shgroup, "dummy", DRW_ATTRIB_FLOAT, 1, true);
	}

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_instance_create(struct GPUShader *shader, DRWPass *pass, Gwn_Batch *geom)
{
	DRWShadingGroup *shgroup = DRW_shgroup_create(shader, pass);

	shgroup->type = DRW_SHG_INSTANCE;
	shgroup->instance_geom = geom;

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_point_batch_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWShadingGroup *shgroup = DRW_shgroup_create(shader, pass);

	shgroup->type = DRW_SHG_POINT_BATCH;
	DRW_shgroup_attrib_float(shgroup, "pos", 3);

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_line_batch_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWShadingGroup *shgroup = DRW_shgroup_create(shader, pass);

	shgroup->type = DRW_SHG_LINE_BATCH;
	DRW_shgroup_attrib_float(shgroup, "pos", 3);

	return shgroup;
}

/* Very special batch. Use this if you position
 * your vertices with the vertex shader
 * and dont need any VBO attrib */
DRWShadingGroup *DRW_shgroup_empty_tri_batch_create(struct GPUShader *shader, DRWPass *pass, int size)
{
	DRWShadingGroup *shgroup = DRW_shgroup_create(shader, pass);

	shgroup->type = DRW_SHG_TRIANGLE_BATCH;
	shgroup->interface.instance_count = size * 3;
	DRW_interface_attrib(shgroup, "dummy", DRW_ATTRIB_FLOAT, 1, true);

	return shgroup;
}

void DRW_shgroup_free(struct DRWShadingGroup *shgroup)
{
	if (shgroup->interface.instance_vbo &&
	    (shgroup->interface.instance_batch == 0))
	{
		glDeleteBuffers(1, &shgroup->interface.instance_vbo);
	}

	GWN_BATCH_DISCARD_SAFE(shgroup->batch_geom);
}

void DRW_shgroup_instance_batch(DRWShadingGroup *shgroup, struct Gwn_Batch *instances)
{
	BLI_assert(shgroup->type == DRW_SHG_INSTANCE);
	BLI_assert(shgroup->interface.instance_batch == NULL);

	shgroup->interface.instance_batch = instances;
}

#define CALL_PREPEND(shgroup, call) { \
	if (shgroup->calls == NULL) { \
		shgroup->calls = call; \
		shgroup->calls_first = call; \
	} \
	else { \
		((DRWCall *)(shgroup->calls))->head.prev = call; \
		shgroup->calls = call; \
	} \
	call->head.prev = NULL; \
} ((void)0)


void DRW_shgroup_call_add(DRWShadingGroup *shgroup, Gwn_Batch *geom, float (*obmat)[4])
{
	BLI_assert(geom != NULL);

	DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);

	CALL_PREPEND(shgroup, call);

	call->head.type = DRW_CALL_SINGLE;
#ifdef USE_GPU_SELECT
	call->head.select_id = g_DRW_select_id;
#endif

	if (obmat != NULL) {
		copy_m4_m4(call->obmat, obmat);
	}

	call->geometry = geom;
	call->ob_data = NULL;
}

void DRW_shgroup_call_object_add(DRWShadingGroup *shgroup, Gwn_Batch *geom, Object *ob)
{
	BLI_assert(geom != NULL);

	DRWCall *call = BLI_mempool_alloc(DST.vmempool->calls);

	CALL_PREPEND(shgroup, call);

	call->head.type = DRW_CALL_SINGLE;
#ifdef USE_GPU_SELECT
	call->head.select_id = g_DRW_select_id;
#endif

	copy_m4_m4(call->obmat, ob->obmat);
	call->geometry = geom;
	call->ob_data = ob->data;
}

void DRW_shgroup_call_generate_add(
        DRWShadingGroup *shgroup,
        DRWCallGenerateFn *geometry_fn, void *user_data,
        float (*obmat)[4])
{
	BLI_assert(geometry_fn != NULL);

	DRWCallGenerate *call = BLI_mempool_alloc(DST.vmempool->calls_generate);

	CALL_PREPEND(shgroup, call);

	call->head.type = DRW_CALL_GENERATE;
#ifdef USE_GPU_SELECT
	call->head.select_id = g_DRW_select_id;
#endif

	if (obmat != NULL) {
		copy_m4_m4(call->obmat, obmat);
	}

	call->geometry_fn = geometry_fn;
	call->user_data = user_data;
}

static void sculpt_draw_cb(
        DRWShadingGroup *shgroup,
        void (*draw_fn)(DRWShadingGroup *shgroup, Gwn_Batch *geom),
        void *user_data)
{
	Object *ob = user_data;
	PBVH *pbvh = ob->sculpt->pbvh;

	if (pbvh) {
		BKE_pbvh_draw_cb(
		        pbvh, NULL, NULL, false,
		        (void (*)(void *, Gwn_Batch *))draw_fn, shgroup);
	}
}

void DRW_shgroup_call_sculpt_add(DRWShadingGroup *shgroup, Object *ob, float (*obmat)[4])
{
	DRW_shgroup_call_generate_add(shgroup, sculpt_draw_cb, ob, obmat);
}

void DRW_shgroup_call_dynamic_add_array(DRWShadingGroup *shgroup, const void *attr[], unsigned int attr_len)
{
	DRWInterface *interface = &shgroup->interface;

#ifdef USE_GPU_SELECT
	if ((G.f & G_PICKSEL) && (interface->instance_count > 0)) {
		DRWShadingGroup *original_shgroup = shgroup;
		shgroup = BLI_mempool_alloc(DST.vmempool->shgroups);
		memcpy(shgroup, original_shgroup, sizeof(DRWShadingGroup));

		shgroup->calls = NULL;
		shgroup->calls_first = NULL;

		interface = &shgroup->interface;
		interface->instance_count = 0;

		/* Append */
		if (shgroup->pass_parent->shgroups != NULL) {
			shgroup->pass_parent->shgroups_last->next = shgroup;
		}
		else {
			shgroup->pass_parent->shgroups = shgroup;
		}
		shgroup->pass_parent->shgroups_last = shgroup;
		shgroup->next = NULL;
	}
#endif

	DRWCallDynamic *call = BLI_mempool_alloc(DST.vmempool->calls_dynamic);

	CALL_PREPEND(shgroup, call);

	BLI_assert(attr_len == interface->attribs_count);
	UNUSED_VARS_NDEBUG(attr_len);

	call->head.type = DRW_CALL_DYNAMIC;
#ifdef USE_GPU_SELECT
	call->head.select_id = g_DRW_select_id;
#endif

	if (interface->attribs_count != 0) {
		memcpy((void *)call->data, attr, sizeof(void *) * interface->attribs_count);
	}

	interface->instance_count += 1;
}

/* Used for instancing with no attributes */
void DRW_shgroup_set_instance_count(DRWShadingGroup *shgroup, int count)
{
	DRWInterface *interface = &shgroup->interface;

	BLI_assert(interface->attribs_count == 0);

	interface->instance_count = count;
}

/**
 * State is added to #Pass.state while drawing.
 * Use to temporarily enable draw options.
 */
void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state)
{
	shgroup->state_extra |= state;
}

void DRW_shgroup_state_disable(DRWShadingGroup *shgroup, DRWState state)
{
	shgroup->state_extra_disable &= ~state;
}

void DRW_shgroup_attrib_float(DRWShadingGroup *shgroup, const char *name, int size)
{
	DRW_interface_attrib(shgroup, name, DRW_ATTRIB_FLOAT, size, false);
}

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_TEXTURE, tex, 0, 1);
}

void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup, const char *name, const GPUUniformBuffer *ubo)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BLOCK, ubo, 0, 1);
}

void DRW_shgroup_uniform_buffer(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BUFFER, tex, 0, 1);
}

void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup, const char *name, const bool *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BOOL, value, 1, arraysize);
}

void DRW_shgroup_uniform_float(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 1, arraysize);
}

void DRW_shgroup_uniform_vec2(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 2, arraysize);
}

void DRW_shgroup_uniform_vec3(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 3, arraysize);
}

void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 4, arraysize);
}

void DRW_shgroup_uniform_short_to_int(DRWShadingGroup *shgroup, const char *name, const short *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_SHORT_TO_INT, value, 1, arraysize);
}

void DRW_shgroup_uniform_short_to_float(DRWShadingGroup *shgroup, const char *name, const short *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_SHORT_TO_FLOAT, value, 1, arraysize);
}

void DRW_shgroup_uniform_int(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_INT, value, 1, arraysize);
}

void DRW_shgroup_uniform_ivec2(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_INT, value, 2, arraysize);
}

void DRW_shgroup_uniform_ivec3(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_INT, value, 3, arraysize);
}

void DRW_shgroup_uniform_mat3(DRWShadingGroup *shgroup, const char *name, const float *value)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_MAT3, value, 9, 1);
}

void DRW_shgroup_uniform_mat4(DRWShadingGroup *shgroup, const char *name, const float *value)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_MAT4, value, 16, 1);
}

/* Creates a VBO containing OGL primitives for all DRWCallDynamic */
static void shgroup_dynamic_batch(DRWShadingGroup *shgroup)
{
	DRWInterface *interface = &shgroup->interface;
	int nbr = interface->instance_count;

	Gwn_PrimType type = (shgroup->type == DRW_SHG_POINT_BATCH) ? GWN_PRIM_POINTS :
	                     (shgroup->type == DRW_SHG_TRIANGLE_BATCH) ? GWN_PRIM_TRIS : GWN_PRIM_LINES;

	if (nbr == 0)
		return;

	/* Upload Data */
	if (interface->vbo_format.attrib_ct == 0) {
		for (DRWAttrib *attrib = interface->attribs_first; attrib; attrib = attrib->prev) {
			BLI_assert(attrib->size <= 4); /* matrices have no place here for now */
			if (attrib->type == DRW_ATTRIB_FLOAT) {
				attrib->format_id = GWN_vertformat_attr_add(
				        &interface->vbo_format, attrib->name, GWN_COMP_F32, attrib->size, GWN_FETCH_FLOAT);
			}
			else if (attrib->type == DRW_ATTRIB_INT) {
				attrib->format_id = GWN_vertformat_attr_add(
				        &interface->vbo_format, attrib->name, GWN_COMP_I8, attrib->size, GWN_FETCH_INT);
			}
			else {
				BLI_assert(false);
			}
		}
	}

	Gwn_VertBuf *vbo = GWN_vertbuf_create_with_format(&interface->vbo_format);
	GWN_vertbuf_data_alloc(vbo, nbr);

	int j = 0;
	for (DRWCallDynamic *call = shgroup->calls_first; call; call = call->head.prev, j++) {
		int i = 0;
		for (DRWAttrib *attrib = interface->attribs_first; attrib; attrib = attrib->prev, i++) {
			GWN_vertbuf_attr_set(vbo, attrib->format_id, j, call->data[i]);
		}
	}

	/* TODO make the batch dynamic instead of freeing it every times */
	if (shgroup->batch_geom)
		GWN_batch_discard(shgroup->batch_geom);

	shgroup->batch_geom = GWN_batch_create_ex(type, vbo, NULL, GWN_BATCH_OWNS_VBO);
}

static void shgroup_dynamic_instance(DRWShadingGroup *shgroup)
{
	int i = 0;
	int offset = 0;
	DRWInterface *interface = &shgroup->interface;
	int buffer_size = 0;

	if (interface->instance_batch != NULL) {
		return;
	}

	/* TODO We still need this because gawain does not support Matrix attribs. */
	if (interface->instance_count == 0) {
		if (interface->instance_vbo) {
			glDeleteBuffers(1, &interface->instance_vbo);
			interface->instance_vbo = 0;
		}
		return;
	}

	/* only once */
	if (interface->attribs_stride == 0) {
		for (DRWAttrib *attrib = interface->attribs_first; attrib; attrib = attrib->prev, i++) {
			BLI_assert(attrib->type == DRW_ATTRIB_FLOAT); /* Only float for now */
			interface->attribs_stride += attrib->size;
			interface->attribs_size[i] = attrib->size;
			interface->attribs_loc[i] = attrib->location;
		}
	}

	/* Gather Data */
	buffer_size = sizeof(float) * interface->attribs_stride * interface->instance_count;
	float *data = MEM_mallocN(buffer_size, "Instance VBO data");

	for (DRWCallDynamic *call = shgroup->calls_first; call; call = call->head.prev) {
		for (int j = 0; j < interface->attribs_count; ++j) {
			memcpy(data + offset, call->data[j], sizeof(float) * interface->attribs_size[j]);
			offset += interface->attribs_size[j];
		}
	}

	/* TODO poke mike to add this to gawain */
	if (interface->instance_vbo) {
		glDeleteBuffers(1, &interface->instance_vbo);
		interface->instance_vbo = 0;
	}

	glGenBuffers(1, &interface->instance_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, interface->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER, buffer_size, data, GL_STATIC_DRAW);

	MEM_freeN(data);
}

static void shgroup_dynamic_batch_from_calls(DRWShadingGroup *shgroup)
{
	if ((shgroup->interface.instance_vbo || shgroup->batch_geom) &&
	    (G.debug_value == 667))
	{
		return;
	}

	if (shgroup->type == DRW_SHG_INSTANCE) {
		shgroup_dynamic_instance(shgroup);
	}
	else {
		shgroup_dynamic_batch(shgroup);
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Passes (DRW_pass)
 * \{ */

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
	DRWPass *pass = BLI_mempool_alloc(DST.vmempool->passes);
	pass->state = state;
	BLI_strncpy(pass->name, name, MAX_PASS_NAME);

	pass->shgroups = NULL;
	pass->shgroups_last = NULL;

	return pass;
}

void DRW_pass_free(DRWPass *pass)
{
	for (DRWShadingGroup *shgroup = pass->shgroups; shgroup; shgroup = shgroup->next) {
		DRW_shgroup_free(shgroup);
	}

	pass->shgroups = NULL;
	pass->shgroups_last = NULL;
}

void DRW_pass_foreach_shgroup(DRWPass *pass, void (*callback)(void *userData, DRWShadingGroup *shgrp), void *userData)
{
	for (DRWShadingGroup *shgroup = pass->shgroups; shgroup; shgroup = shgroup->next) {
		callback(userData, shgroup);
	}
}

typedef struct ZSortData {
	float *axis;
	float *origin;
} ZSortData;

static int pass_shgroup_dist_sort(void *thunk, const void *a, const void *b)
{
	const ZSortData *zsortdata = (ZSortData *)thunk;
	const DRWShadingGroup *shgrp_a = (const DRWShadingGroup *)a;
	const DRWShadingGroup *shgrp_b = (const DRWShadingGroup *)b;

	const DRWCall *call_a;
	const DRWCall *call_b;

	call_a = shgrp_a->calls_first;
	call_b = shgrp_b->calls_first;

	if (call_a == NULL) return -1;
	if (call_b == NULL) return -1;

	float tmp[3];
	sub_v3_v3v3(tmp, zsortdata->origin, call_a->obmat[3]);
	const float a_sq = dot_v3v3(zsortdata->axis, tmp);
	sub_v3_v3v3(tmp, zsortdata->origin, call_b->obmat[3]);
	const float b_sq = dot_v3v3(zsortdata->axis, tmp);

	if      (a_sq < b_sq) return  1;
	else if (a_sq > b_sq) return -1;
	else {
		/* If there is a depth prepass put it before */
		if ((shgrp_a->state_extra & DRW_STATE_WRITE_DEPTH) != 0) {
			return -1;
		}
		else if ((shgrp_b->state_extra & DRW_STATE_WRITE_DEPTH) != 0) {
			return  1;
		}
		else return  0;
	}
}

/* ------------------ Shading group sorting --------------------- */

#define SORT_IMPL_LINKTYPE DRWShadingGroup

#define SORT_IMPL_USE_THUNK
#define SORT_IMPL_FUNC shgroup_sort_fn_r
#include "../../blenlib/intern/list_sort_impl.h"
#undef SORT_IMPL_FUNC
#undef SORT_IMPL_USE_THUNK

#undef SORT_IMPL_LINKTYPE

/**
 * Sort Shading groups by decreasing Z of their first draw call.
 * This is usefull for order dependant effect such as transparency.
 **/
void DRW_pass_sort_shgroup_z(DRWPass *pass)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	float (*viewinv)[4];
	viewinv = (viewport_matrix_override.override[DRW_MAT_VIEWINV])
	          ? viewport_matrix_override.mat[DRW_MAT_VIEWINV] : rv3d->viewinv;

	ZSortData zsortdata = {viewinv[2], viewinv[3]};

	if (pass->shgroups && pass->shgroups->next) {
		pass->shgroups = shgroup_sort_fn_r(pass->shgroups, pass_shgroup_dist_sort, &zsortdata);

		/* Find the next last */
		DRWShadingGroup *last = pass->shgroups;
		while ((last = last->next)) {
			/* Do nothing */
		};
		pass->shgroups_last = last;
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Draw (DRW_draw)
 * \{ */

static void DRW_state_set(DRWState state)
{
	if (DST.state == state) {
		return;
	}


#define CHANGED_TO(f) \
	((DST.state & (f)) ? \
		((state & (f)) ?  0 : -1) : \
		((state & (f)) ?  1 :  0))

#define CHANGED_ANY(f) \
	((DST.state & (f)) != (state & (f)))

#define CHANGED_ANY_STORE_VAR(f, enabled) \
	((DST.state & (f)) != (enabled = (state & (f))))

	/* Depth Write */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_WRITE_DEPTH))) {
			if (test == 1) {
				glDepthMask(GL_TRUE);
			}
			else {
				glDepthMask(GL_FALSE);
			}
		}
	}

	/* Color Write */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_WRITE_COLOR))) {
			if (test == 1) {
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			}
			else {
				glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			}
		}
	}

	/* Cull */
	{
		DRWState test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_CULL_BACK | DRW_STATE_CULL_FRONT,
		        test))
		{
			if (test) {
				glEnable(GL_CULL_FACE);

				if ((state & DRW_STATE_CULL_BACK) != 0) {
					glCullFace(GL_BACK);
				}
				else if ((state & DRW_STATE_CULL_FRONT) != 0) {
					glCullFace(GL_FRONT);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				glDisable(GL_CULL_FACE);
			}
		}
	}

	/* Depth Test */
	{
		DRWState test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER | DRW_STATE_DEPTH_ALWAYS,
		        test))
		{
			if (test) {
				glEnable(GL_DEPTH_TEST);

				if (state & DRW_STATE_DEPTH_LESS) {
					glDepthFunc(GL_LEQUAL);
				}
				else if (state & DRW_STATE_DEPTH_EQUAL) {
					glDepthFunc(GL_EQUAL);
				}
				else if (state & DRW_STATE_DEPTH_GREATER) {
					glDepthFunc(GL_GREATER);
				}
				else if (state & DRW_STATE_DEPTH_ALWAYS) {
					glDepthFunc(GL_ALWAYS);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				glDisable(GL_DEPTH_TEST);
			}
		}
	}

	/* Wire Width */
	{
		if (CHANGED_ANY(DRW_STATE_WIRE | DRW_STATE_WIRE_LARGE)) {
			if ((state & DRW_STATE_WIRE) != 0) {
				glLineWidth(1.0f);
			}
			else if ((state & DRW_STATE_WIRE_LARGE) != 0) {
				glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
			}
			else {
				/* do nothing */
			}
		}
	}

	/* Points Size */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_POINT))) {
			if (test == 1) {
				GPU_enable_program_point_size();
				glPointSize(5.0f);
			}
			else {
				GPU_disable_program_point_size();
			}
		}
	}

	/* Blending (all buffer) */
	{
		int test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_BLEND | DRW_STATE_ADDITIVE | DRW_STATE_MULTIPLY | DRW_STATE_TRANSMISSION,
		        test))
		{
			if (test) {
				glEnable(GL_BLEND);

				if ((state & DRW_STATE_BLEND) != 0) {
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				}
				else if ((state & DRW_STATE_MULTIPLY) != 0) {
					glBlendFunc(GL_DST_COLOR, GL_ZERO);
				}
				else if ((state & DRW_STATE_TRANSMISSION) != 0) {
					glBlendFunc(GL_ONE, GL_SRC_ALPHA);
				}
				else if ((state & DRW_STATE_ADDITIVE) != 0) {
					glBlendFunc(GL_SRC_ALPHA, GL_ONE);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				glDisable(GL_BLEND);
			}
		}
	}

	/* Clip Planes */
	{
		int test;
		if ((test = CHANGED_TO(DRW_STATE_CLIP_PLANES))) {
			if (test == 1) {
				for (int i = 0; i < DST.num_clip_planes; ++i) {
					glEnable(GL_CLIP_DISTANCE0 + i);
				}
			}
			else {
				for (int i = 0; i < MAX_CLIP_PLANES; ++i) {
					glDisable(GL_CLIP_DISTANCE0 + i);
				}
			}
		}
	}

	/* Line Stipple */
	{
		int test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_STIPPLE_2 | DRW_STATE_STIPPLE_3 | DRW_STATE_STIPPLE_4,
		        test))
		{
			if (test) {
				if ((state & DRW_STATE_STIPPLE_2) != 0) {
					setlinestyle(2);
				}
				else if ((state & DRW_STATE_STIPPLE_3) != 0) {
					setlinestyle(3);
				}
				else if ((state & DRW_STATE_STIPPLE_4) != 0) {
					setlinestyle(4);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				setlinestyle(0);
			}
		}
	}

	/* Stencil */
	{
		DRWState test;
		if (CHANGED_ANY_STORE_VAR(
		        DRW_STATE_WRITE_STENCIL_SELECT |
		        DRW_STATE_WRITE_STENCIL_ACTIVE |
		        DRW_STATE_TEST_STENCIL_SELECT |
		        DRW_STATE_TEST_STENCIL_ACTIVE,
		        test))
		{
			if (test) {
				glEnable(GL_STENCIL_TEST);

				/* Stencil Write */
				if ((state & DRW_STATE_WRITE_STENCIL_SELECT) != 0) {
					glStencilMask(STENCIL_SELECT);
					glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
					glStencilFunc(GL_ALWAYS, 0xFF, STENCIL_SELECT);
				}
				else if ((state & DRW_STATE_WRITE_STENCIL_ACTIVE) != 0) {
					glStencilMask(STENCIL_ACTIVE);
					glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
					glStencilFunc(GL_ALWAYS, 0xFF, STENCIL_ACTIVE);
				}
				/* Stencil Test */
				else if ((state & DRW_STATE_TEST_STENCIL_SELECT) != 0) {
					glStencilMask(0x00); /* disable write */
					glStencilFunc(GL_NOTEQUAL, 0xFF, STENCIL_SELECT);
				}
				else if ((state & DRW_STATE_TEST_STENCIL_ACTIVE) != 0) {
					glStencilMask(0x00); /* disable write */
					glStencilFunc(GL_NOTEQUAL, 0xFF, STENCIL_ACTIVE);
				}
				else {
					BLI_assert(0);
				}
			}
			else {
				/* disable write & test */
				glStencilMask(0x00);
				glStencilFunc(GL_ALWAYS, 1, 0xFF);
				glDisable(GL_STENCIL_TEST);
			}
		}
	}

#undef CHANGED_TO
#undef CHANGED_ANY
#undef CHANGED_ANY_STORE_VAR

	DST.state = state;
}

typedef struct DRWBoundTexture {
	struct DRWBoundTexture *next, *prev;
	GPUTexture *tex;
} DRWBoundTexture;

static void draw_geometry_prepare(
        DRWShadingGroup *shgroup, const float (*obmat)[4], const float *texcoloc, const float *texcosize)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;
	DRWInterface *interface = &shgroup->interface;

	float mvp[4][4], mv[4][4], mi[4][4], mvi[4][4], pi[4][4], n[3][3], wn[3][3];
	float orcofacs[2][3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}};
	float eye[3] = { 0.0f, 0.0f, 1.0f }; /* looking into the screen */

	bool do_pi = (interface->projectioninverse != -1);
	bool do_mvp = (interface->modelviewprojection != -1);
	bool do_mi = (interface->modelinverse != -1);
	bool do_mv = (interface->modelview != -1);
	bool do_mvi = (interface->modelviewinverse != -1);
	bool do_n = (interface->normal != -1);
	bool do_wn = (interface->worldnormal != -1);
	bool do_eye = (interface->eye != -1);
	bool do_orco = (interface->orcotexfac != -1) && (texcoloc != NULL) && (texcosize != NULL);

	/* Matrix override */
	float (*persmat)[4];
	float (*persinv)[4];
	float (*viewmat)[4];
	float (*viewinv)[4];
	float (*winmat)[4];
	float (*wininv)[4];

	persmat = (viewport_matrix_override.override[DRW_MAT_PERS])
	          ? viewport_matrix_override.mat[DRW_MAT_PERS] : rv3d->persmat;
	persinv = (viewport_matrix_override.override[DRW_MAT_PERSINV])
	          ? viewport_matrix_override.mat[DRW_MAT_PERSINV] : rv3d->persinv;
	viewmat = (viewport_matrix_override.override[DRW_MAT_VIEW])
	          ? viewport_matrix_override.mat[DRW_MAT_VIEW] : rv3d->viewmat;
	viewinv = (viewport_matrix_override.override[DRW_MAT_VIEWINV])
	          ? viewport_matrix_override.mat[DRW_MAT_VIEWINV] : rv3d->viewinv;
	winmat = (viewport_matrix_override.override[DRW_MAT_WIN])
	          ? viewport_matrix_override.mat[DRW_MAT_WIN] : rv3d->winmat;
	wininv = viewport_matrix_override.mat[DRW_MAT_WININV];

	if (do_pi) {
		if (!viewport_matrix_override.override[DRW_MAT_WININV]) {
			invert_m4_m4(pi, winmat);
			wininv = pi;
		}
	}
	if (do_mi) {
		invert_m4_m4(mi, obmat);
	}
	if (do_mvp) {
		mul_m4_m4m4(mvp, persmat, obmat);
	}
	if (do_mv || do_mvi || do_n || do_eye) {
		mul_m4_m4m4(mv, viewmat, obmat);
	}
	if (do_mvi) {
		invert_m4_m4(mvi, mv);
	}
	if (do_n || do_eye) {
		copy_m3_m4(n, mv);
		invert_m3(n);
		transpose_m3(n);
	}
	if (do_wn) {
		copy_m3_m4(wn, obmat);
		invert_m3(wn);
		transpose_m3(wn);
	}
	if (do_eye) {
		/* Used by orthographic wires */
		float tmp[3][3];
		invert_m3_m3(tmp, n);
		/* set eye vector, transformed to object coords */
		mul_m3_v3(tmp, eye);
	}
	if (do_orco) {
		mul_v3_v3fl(orcofacs[1], texcosize, 2.0f);
		invert_v3(orcofacs[1]);
		sub_v3_v3v3(orcofacs[0], texcoloc, texcosize);
		negate_v3(orcofacs[0]);
		mul_v3_v3(orcofacs[0], orcofacs[1]); /* result in a nice MADD in the shader */
	}

	/* Should be really simple */
	/* step 1 : bind object dependent matrices */
	/* TODO : Some of these are not object dependant.
	 * They should be grouped inside a UBO updated once per redraw.
	 * The rest can also go into a UBO to reduce API calls. */
	GPU_shader_uniform_vector(shgroup->shader, interface->model, 16, 1, (float *)obmat);
	GPU_shader_uniform_vector(shgroup->shader, interface->modelinverse, 16, 1, (float *)mi);
	GPU_shader_uniform_vector(shgroup->shader, interface->modelviewprojection, 16, 1, (float *)mvp);
	GPU_shader_uniform_vector(shgroup->shader, interface->viewinverse, 16, 1, (float *)viewinv);
	GPU_shader_uniform_vector(shgroup->shader, interface->viewprojection, 16, 1, (float *)persmat);
	GPU_shader_uniform_vector(shgroup->shader, interface->viewprojectioninverse, 16, 1, (float *)persinv);
	GPU_shader_uniform_vector(shgroup->shader, interface->projection, 16, 1, (float *)winmat);
	GPU_shader_uniform_vector(shgroup->shader, interface->projectioninverse, 16, 1, (float *)wininv);
	GPU_shader_uniform_vector(shgroup->shader, interface->view, 16, 1, (float *)viewmat);
	GPU_shader_uniform_vector(shgroup->shader, interface->modelview, 16, 1, (float *)mv);
	GPU_shader_uniform_vector(shgroup->shader, interface->modelviewinverse, 16, 1, (float *)mvi);
	GPU_shader_uniform_vector(shgroup->shader, interface->normal, 9, 1, (float *)n);
	GPU_shader_uniform_vector(shgroup->shader, interface->worldnormal, 9, 1, (float *)wn);
	GPU_shader_uniform_vector(shgroup->shader, interface->camtexfac, 4, 1, (float *)rv3d->viewcamtexcofac);
	GPU_shader_uniform_vector(shgroup->shader, interface->orcotexfac, 3, 2, (float *)orcofacs);
	GPU_shader_uniform_vector(shgroup->shader, interface->eye, 3, 1, (float *)eye);
	GPU_shader_uniform_vector(shgroup->shader, interface->clipplanes, 4, DST.num_clip_planes, (float *)DST.clip_planes_eq);
}

static void draw_geometry_execute(DRWShadingGroup *shgroup, Gwn_Batch *geom)
{
	DRWInterface *interface = &shgroup->interface;
	/* step 2 : bind vertex array & draw */
	GWN_batch_program_set(geom, GPU_shader_get_program(shgroup->shader), GPU_shader_get_interface(shgroup->shader));
	if (interface->instance_batch) {
		GWN_batch_draw_stupid_instanced_with_batch(geom, interface->instance_batch);
	}
	else if (interface->instance_vbo) {
		GWN_batch_draw_stupid_instanced(
		        geom, interface->instance_vbo, interface->instance_count, interface->attribs_count,
		        interface->attribs_stride, interface->attribs_size, interface->attribs_loc);
	}
	else {
		GWN_batch_draw_stupid(geom);
	}
	/* XXX this just tells gawain we are done with the shader.
	 * This does not unbind the shader. */
	GWN_batch_program_unset(geom);
}

static void draw_geometry(DRWShadingGroup *shgroup, Gwn_Batch *geom, const float (*obmat)[4], ID *ob_data)
{
	float *texcoloc = NULL;
	float *texcosize = NULL;

	if (ob_data != NULL) {
		switch (GS(ob_data->name)) {
			case ID_ME:
				BKE_mesh_texspace_get_reference((Mesh *)ob_data, NULL, &texcoloc, NULL, &texcosize);
				break;
			default:
				/* TODO, curve, metaball? */
				break;
		}
	}

	draw_geometry_prepare(shgroup, obmat, texcoloc, texcosize);

	draw_geometry_execute(shgroup, geom);
}

static void bind_texture(GPUTexture *tex)
{
	int bind_num = GPU_texture_bound_number(tex);
	if (bind_num == -1) {
		for (int i = 0; i < GPU_max_textures(); ++i) {
			RST.bind_tex_inc = (RST.bind_tex_inc + 1) % GPU_max_textures();
			if (RST.bound_tex_slots[RST.bind_tex_inc] == false) {
				if (RST.bound_texs[RST.bind_tex_inc] != NULL) {
					GPU_texture_unbind(RST.bound_texs[RST.bind_tex_inc]);
				}
				GPU_texture_bind(tex, RST.bind_tex_inc);
				RST.bound_texs[RST.bind_tex_inc] = tex;
				RST.bound_tex_slots[RST.bind_tex_inc] = true;
				return;
			}
		}

		printf("Not enough texture slots! Reduce number of textures used by your shader.\n");
	}
	RST.bound_tex_slots[bind_num] = true;
}

static void bind_ubo(GPUUniformBuffer *ubo)
{
	if (RST.bind_ubo_inc < GPU_max_ubo_binds()) {
		GPU_uniformbuffer_bind(ubo, RST.bind_ubo_inc);
		RST.bind_ubo_inc++;
	}
	else {
		/* This is not depending on user input.
		 * It is our responsability to make sure there enough slots. */
		BLI_assert(0 && "Not enough ubo slots! This should not happen!\n");

		/* printf so user can report bad behaviour */
		printf("Not enough ubo slots! This should not happen!\n");
	}
}

static void release_texture_slots(void)
{
	memset(RST.bound_tex_slots, 0x0, sizeof(bool) * GPU_max_textures());
}

static void release_ubo_slots(void)
{
	RST.bind_ubo_inc = 0;
}

static void draw_shgroup(DRWShadingGroup *shgroup, DRWState pass_state)
{
	BLI_assert(shgroup->shader);

	DRWInterface *interface = &shgroup->interface;
	GPUTexture *tex;
	GPUUniformBuffer *ubo;
	int val;
	float fval;

	if (DST.shader != shgroup->shader) {
		if (DST.shader) GPU_shader_unbind();
		GPU_shader_bind(shgroup->shader);
		DST.shader = shgroup->shader;
	}

	const bool is_normal = ELEM(shgroup->type, DRW_SHG_NORMAL);

	if (!is_normal) {
		shgroup_dynamic_batch_from_calls(shgroup);
	}

	release_texture_slots();
	release_ubo_slots();

	DRW_state_set((pass_state & shgroup->state_extra_disable) | shgroup->state_extra);


	/* Binding Uniform */
	/* Don't check anything, Interface should already contain the least uniform as possible */
	for (DRWUniform *uni = interface->uniforms; uni; uni = uni->next) {
		switch (uni->type) {
			case DRW_UNIFORM_SHORT_TO_INT:
				val = (int)*((short *)uni->value);
				GPU_shader_uniform_vector_int(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)&val);
				break;
			case DRW_UNIFORM_SHORT_TO_FLOAT:
				fval = (float)*((short *)uni->value);
				GPU_shader_uniform_vector(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)&fval);
				break;
			case DRW_UNIFORM_BOOL:
			case DRW_UNIFORM_INT:
				GPU_shader_uniform_vector_int(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)uni->value);
				break;
			case DRW_UNIFORM_FLOAT:
			case DRW_UNIFORM_MAT3:
			case DRW_UNIFORM_MAT4:
				GPU_shader_uniform_vector(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)uni->value);
				break;
			case DRW_UNIFORM_TEXTURE:
				tex = (GPUTexture *)uni->value;
				BLI_assert(tex);
				bind_texture(tex);
				GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
				break;
			case DRW_UNIFORM_BUFFER:
				if (!DRW_state_is_fbo()) {
					break;
				}
				tex = *((GPUTexture **)uni->value);
				BLI_assert(tex);
				bind_texture(tex);
				GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
				break;
			case DRW_UNIFORM_BLOCK:
				ubo = (GPUUniformBuffer *)uni->value;
				bind_ubo(ubo);
				GPU_shader_uniform_buffer(shgroup->shader, uni->location, ubo);
				break;
		}
	}

#ifdef USE_GPU_SELECT
	/* use the first item because of selection we only ever add one */
#  define GPU_SELECT_LOAD_IF_PICKSEL(_call) \
	if ((G.f & G_PICKSEL) && (_call)) { \
		GPU_select_load_id((_call)->head.select_id); \
	} ((void)0)

#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(_call_last, _call_first) \
	if ((G.f & G_PICKSEL) && _call_first) { \
		BLI_assert(_call_first && (_call_first == _call_last)); \
		GPU_select_load_id(((DRWCall *)_call_first)->head.select_id); \
	} ((void)0)

#else
#  define GPU_SELECT_LOAD_IF_PICKSEL(call)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(call, _call_first)
#endif

	/* Rendering Calls */
	if (!is_normal) {
		/* Replacing multiple calls with only one */
		float obmat[4][4];
		unit_m4(obmat);

		if (shgroup->type == DRW_SHG_INSTANCE &&
		    (interface->instance_count > 0 || interface->instance_batch != NULL))
		{
			GPU_SELECT_LOAD_IF_PICKSEL_LIST(shgroup->calls, shgroup->calls_first);
			draw_geometry(shgroup, shgroup->instance_geom, obmat, shgroup->instance_data);
		}
		else {
			/* Some dynamic batch can have no geom (no call to aggregate) */
			if (shgroup->batch_geom) {
				GPU_SELECT_LOAD_IF_PICKSEL_LIST(shgroup->calls, shgroup->calls_first);
				draw_geometry(shgroup, shgroup->batch_geom, obmat, NULL);
			}
		}
	}
	else {
		for (DRWCall *call = shgroup->calls_first; call; call = call->head.prev)
		{
			bool neg_scale = is_negative_m4(call->obmat);

			/* Negative scale objects */
			if (neg_scale) {
				glFrontFace(DST.backface);
			}

			GPU_SELECT_LOAD_IF_PICKSEL(call);

			if (call->head.type == DRW_CALL_SINGLE) {
				draw_geometry(shgroup, call->geometry, call->obmat, call->ob_data);
			}
			else {
				BLI_assert(call->head.type == DRW_CALL_GENERATE);
				DRWCallGenerate *callgen = ((DRWCallGenerate *)call);
				draw_geometry_prepare(shgroup, callgen->obmat, NULL, NULL);
				callgen->geometry_fn(shgroup, draw_geometry_execute, callgen->user_data);
			}

			/* Reset state */
			if (neg_scale) {
				glFrontFace(DST.frontface);
			}
		}
	}

	/* TODO: remove, (currently causes alpha issue with sculpt, need to investigate) */
	DRW_state_reset();
}

static void DRW_draw_pass_ex(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
	/* Start fresh */
	DST.shader = NULL;

	DRW_state_set(pass->state);

	DRW_stats_query_start(pass->name);

	for (DRWShadingGroup *shgroup = start_group; shgroup; shgroup = shgroup->next) {
		draw_shgroup(shgroup, pass->state);
		/* break if upper limit */
		if (shgroup == end_group) {
			break;
		}
	}

	/* Clear Bound textures */
	for (int i = 0; i < GPU_max_textures(); i++) {
		if (RST.bound_texs[i] != NULL) {
			GPU_texture_unbind(RST.bound_texs[i]);
			RST.bound_texs[i] = NULL;
		}
	}

	if (DST.shader) {
		GPU_shader_unbind();
		DST.shader = NULL;
	}

	DRW_stats_query_end();
}

void DRW_draw_pass(DRWPass *pass)
{
	DRW_draw_pass_ex(pass, pass->shgroups, pass->shgroups_last);
}

/* Draw only a subset of shgroups. Used in special situations as grease pencil strokes */
void DRW_draw_pass_subset(DRWPass *pass, DRWShadingGroup *start_group, DRWShadingGroup *end_group)
{
	DRW_draw_pass_ex(pass, start_group, end_group);
}

void DRW_draw_callbacks_pre_scene(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	gpuLoadProjectionMatrix(rv3d->winmat);
	gpuLoadMatrix(rv3d->viewmat);
}

void DRW_draw_callbacks_post_scene(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	gpuLoadProjectionMatrix(rv3d->winmat);
	gpuLoadMatrix(rv3d->viewmat);
}

/* Reset state to not interfer with other UI drawcall */
void DRW_state_reset_ex(DRWState state)
{
	DST.state = ~state;
	DRW_state_set(state);
}

void DRW_state_reset(void)
{
	/* Reset blending function */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	DRW_state_reset_ex(DRW_STATE_DEFAULT);
}

/* NOTE : Make sure to reset after use! */
void DRW_state_invert_facing(void)
{
	SWAP(GLenum, DST.backface, DST.frontface);
	glFrontFace(DST.frontface);
}

/**
 * This only works if DRWPasses have been tagged with DRW_STATE_CLIP_PLANES,
 * and if the shaders have support for it (see usage of gl_ClipDistance).
 * Be sure to call DRW_state_clip_planes_reset() after you finish drawing.
 **/
void DRW_state_clip_planes_add(float plane_eq[4])
{
	copy_v4_v4(DST.clip_planes_eq[DST.num_clip_planes++], plane_eq);
}

void DRW_state_clip_planes_reset(void)
{
	DST.num_clip_planes = 0;
}

/** \} */


struct DRWTextStore *DRW_text_cache_ensure(void)
{
	BLI_assert(DST.text_store_p);
	if (*DST.text_store_p == NULL) {
		*DST.text_store_p = DRW_text_cache_create();
	}
	return *DST.text_store_p;
}


/* -------------------------------------------------------------------- */

/** \name Settings
 * \{ */

bool DRW_object_is_renderable(Object *ob)
{
	Scene *scene = DST.draw_ctx.scene;
	Object *obedit = scene->obedit;

	if (!BKE_object_is_visible(ob)) {
		return false;
	}

	if (ob->type == OB_MESH) {
		if (ob == obedit) {
			IDProperty *props = BKE_layer_collection_engine_evaluated_get(ob, COLLECTION_MODE_EDIT, "");
			bool do_show_occlude_wire = BKE_collection_engine_property_value_get_bool(props, "show_occlude_wire");
			if (do_show_occlude_wire) {
				return false;
			}
			bool do_show_weight = BKE_collection_engine_property_value_get_bool(props, "show_weight");
			if (do_show_weight) {
				return false;
			}
		}
	}

	return true;
}

bool DRW_object_is_flat_normal(const Object *ob)
{
	if (ob->type == OB_MESH) {
		const Mesh *me = ob->data;
		if (me->mpoly && me->mpoly[0].flag & ME_SMOOTH) {
			return false;
		}
	}
	return true;
}


/**
 * Return true if the object has its own draw mode.
 * Caller must check this is active */
int DRW_object_is_mode_shade(const Object *ob)
{
	BLI_assert(ob == DST.draw_ctx.obact);
	if ((ob->mode & OB_MODE_EDIT) == 0) {
		if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)) {
			if ((DST.draw_ctx.v3d->flag2 & V3D_SHOW_MODE_SHADE_OVERRIDE) == 0) {
				return true;
			}
			else {
				return false;
			}
		}
	}
	return -1;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Framebuffers (DRW_framebuffer)
 * \{ */

static GPUTextureFormat convert_tex_format(
        int fbo_format,
        int *r_channels, bool *r_is_depth)
{
	*r_is_depth = ELEM(fbo_format, DRW_TEX_DEPTH_16, DRW_TEX_DEPTH_24);

	switch (fbo_format) {
		case DRW_TEX_R_16:     *r_channels = 1; return GPU_R16F;
		case DRW_TEX_R_32:     *r_channels = 1; return GPU_R32F;
		case DRW_TEX_RG_8:     *r_channels = 2; return GPU_RG8;
		case DRW_TEX_RG_16:    *r_channels = 2; return GPU_RG16F;
		case DRW_TEX_RG_32:    *r_channels = 2; return GPU_RG32F;
		case DRW_TEX_RGBA_8:   *r_channels = 4; return GPU_RGBA8;
		case DRW_TEX_RGBA_16:  *r_channels = 4; return GPU_RGBA16F;
		case DRW_TEX_RGBA_32:  *r_channels = 4; return GPU_RGBA32F;
		case DRW_TEX_DEPTH_16: *r_channels = 1; return GPU_DEPTH_COMPONENT16;
		case DRW_TEX_DEPTH_24: *r_channels = 1; return GPU_DEPTH_COMPONENT24;
		case DRW_TEX_DEPTH_32: *r_channels = 1; return GPU_DEPTH_COMPONENT32F;
		case DRW_TEX_RGB_11_11_10: *r_channels = 3; return GPU_R11F_G11F_B10F;
		default:
			BLI_assert(false && "Texture format unsupported as render target!");
			*r_channels = 4; return GPU_RGBA8;
	}
}

void DRW_framebuffer_init(
        struct GPUFrameBuffer **fb, void *engine_type, int width, int height,
        DRWFboTexture textures[MAX_FBO_TEX], int textures_len)
{
	BLI_assert(textures_len <= MAX_FBO_TEX);

	bool create_fb = false;
	int color_attachment = -1;

	if (!*fb) {
		*fb = GPU_framebuffer_create();
		create_fb = true;
	}

	for (int i = 0; i < textures_len; ++i) {
		int channels;
		bool is_depth;

		DRWFboTexture fbotex = textures[i];
		bool is_temp = (fbotex.flag & DRW_TEX_TEMP) != 0;

		GPUTextureFormat gpu_format = convert_tex_format(fbotex.format, &channels, &is_depth);

		if (!*fbotex.tex || is_temp) {
			/* Temp textures need to be queried each frame, others not. */
			if (is_temp) {
				*fbotex.tex = GPU_viewport_texture_pool_query(
				        DST.viewport, engine_type, width, height, channels, gpu_format);
			}
			else if (create_fb) {
				*fbotex.tex = GPU_texture_create_2D_custom(
				        width, height, channels, gpu_format, NULL, NULL);
			}
		}

		if (create_fb) {
			if (!is_depth) {
				++color_attachment;
			}
			drw_texture_set_parameters(*fbotex.tex, fbotex.flag);
			GPU_framebuffer_texture_attach(*fb, *fbotex.tex, color_attachment, 0);
		}
	}

	if (create_fb && (textures_len > 0)) {
		if (!GPU_framebuffer_check_valid(*fb, NULL)) {
			printf("Error invalid framebuffer\n");
		}

		/* Detach temp textures */
		for (int i = 0; i < textures_len; ++i) {
			DRWFboTexture fbotex = textures[i];

			if ((fbotex.flag & DRW_TEX_TEMP) != 0) {
				GPU_framebuffer_texture_detach(*fbotex.tex);
			}
		}

		GPU_framebuffer_bind(DST.default_framebuffer);
	}
}

void DRW_framebuffer_free(struct GPUFrameBuffer *fb)
{
	GPU_framebuffer_free(fb);
}

void DRW_framebuffer_bind(struct GPUFrameBuffer *fb)
{
	GPU_framebuffer_bind(fb);
}

void DRW_framebuffer_clear(bool color, bool depth, bool stencil, float clear_col[4], float clear_depth)
{
	if (color) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(clear_col[0], clear_col[1], clear_col[2], clear_col[3]);
	}
	if (depth) {
		glDepthMask(GL_TRUE);
		glClearDepth(clear_depth);
	}
	if (stencil) {
		glStencilMask(0xFF);
	}
	glClear(((color) ? GL_COLOR_BUFFER_BIT : 0) |
	        ((depth) ? GL_DEPTH_BUFFER_BIT : 0) |
	        ((stencil) ? GL_STENCIL_BUFFER_BIT : 0));
}

void DRW_framebuffer_read_data(int x, int y, int w, int h, int channels, int slot, float *data)
{
	GLenum type;
	switch (channels) {
		case 1: type = GL_RED; break;
		case 2: type = GL_RG; break;
		case 3: type = GL_RGB; break;
		case 4: type = GL_RGBA;	break;
		default:
			BLI_assert(false && "wrong number of read channels");
			return;
	}
	glReadBuffer(GL_COLOR_ATTACHMENT0 + slot);
	glReadPixels(x, y, w, h, type, GL_FLOAT, data);
}

void DRW_framebuffer_texture_attach(struct GPUFrameBuffer *fb, GPUTexture *tex, int slot, int mip)
{
	GPU_framebuffer_texture_attach(fb, tex, slot, mip);
}

void DRW_framebuffer_texture_layer_attach(struct GPUFrameBuffer *fb, struct GPUTexture *tex, int slot, int layer, int mip)
{
	GPU_framebuffer_texture_layer_attach(fb, tex, slot, layer, mip);
}

void DRW_framebuffer_cubeface_attach(struct GPUFrameBuffer *fb, GPUTexture *tex, int slot, int face, int mip)
{
	GPU_framebuffer_texture_cubeface_attach(fb, tex, slot, face, mip);
}

void DRW_framebuffer_texture_detach(GPUTexture *tex)
{
	GPU_framebuffer_texture_detach(tex);
}

void DRW_framebuffer_blit(struct GPUFrameBuffer *fb_read, struct GPUFrameBuffer *fb_write, bool depth)
{
	GPU_framebuffer_blit(fb_read, 0, fb_write, 0, depth);
}

void DRW_framebuffer_recursive_downsample(
        struct GPUFrameBuffer *fb, struct GPUTexture *tex, int num_iter,
        void (*callback)(void *userData, int level), void *userData)
{
	GPU_framebuffer_recursive_downsample(fb, tex, num_iter, callback, userData);
}

void DRW_framebuffer_viewport_size(struct GPUFrameBuffer *UNUSED(fb_read), int x, int y, int w, int h)
{
	glViewport(x, y, w, h);
}

/* Use color management profile to draw texture to framebuffer */
void DRW_transform_to_display(GPUTexture *tex)
{
	DRW_state_set(DRW_STATE_WRITE_COLOR);

	Gwn_VertFormat *vert_format = immVertexFormat();
	unsigned int pos = GWN_vertformat_attr_add(vert_format, "pos", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);
	unsigned int texco = GWN_vertformat_attr_add(vert_format, "texCoord", GWN_COMP_F32, 2, GWN_FETCH_FLOAT);

	const float dither = 1.0f;

	bool use_ocio = false;

	{
		Scene *scene = DST.draw_ctx.scene;
		/* View transform is already applied for offscreen, don't apply again, see: T52046 */
		ColorManagedViewSettings *view_settings =
		        (DST.options.is_image_render && !DST.options.is_scene_render) ?
		        NULL : &scene->view_settings;
		use_ocio = IMB_colormanagement_setup_glsl_draw_from_space(
		        view_settings, &scene->display_settings, NULL, dither, false);
	}

	if (!use_ocio) {
		immBindBuiltinProgram(GPU_SHADER_2D_IMAGE_LINEAR_TO_SRGB);
		immUniform1i("image", 0);
	}

	GPU_texture_bind(tex, 0); /* OCIO texture bind point is 0 */

	float mat[4][4];
	unit_m4(mat);
	immUniformMatrix4fv("ModelViewProjectionMatrix", mat);

	/* Full screen triangle */
	immBegin(GWN_PRIM_TRIS, 3);
	immAttrib2f(texco, 0.0f, 0.0f);
	immVertex2f(pos, -1.0f, -1.0f);

	immAttrib2f(texco, 2.0f, 0.0f);
	immVertex2f(pos, 3.0f, -1.0f);

	immAttrib2f(texco, 0.0f, 2.0f);
	immVertex2f(pos, -1.0f, 3.0f);
	immEnd();

	GPU_texture_unbind(tex);

	if (use_ocio) {
		IMB_colormanagement_finish_glsl_draw();
	}
	else {
		immUnbindProgram();
	}
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Viewport (DRW_viewport)
 * \{ */

static void *DRW_viewport_engine_data_get(void *engine_type)
{
	void *data = GPU_viewport_engine_data_get(DST.viewport, engine_type);

	if (data == NULL) {
		data = GPU_viewport_engine_data_create(DST.viewport, engine_type);
	}
	return data;
}

void DRW_engine_viewport_data_size_get(
        const void *engine_type_v,
        int *r_fbl_len, int *r_txl_len, int *r_psl_len, int *r_stl_len)
{
	const DrawEngineType *engine_type = engine_type_v;

	if (r_fbl_len) {
		*r_fbl_len = engine_type->vedata_size->fbl_len;
	}
	if (r_txl_len) {
		*r_txl_len = engine_type->vedata_size->txl_len;
	}
	if (r_psl_len) {
		*r_psl_len = engine_type->vedata_size->psl_len;
	}
	if (r_stl_len) {
		*r_stl_len = engine_type->vedata_size->stl_len;
	}
}

const float *DRW_viewport_size_get(void)
{
	return &DST.size[0];
}

const float *DRW_viewport_screenvecs_get(void)
{
	return &DST.screenvecs[0][0];
}

const float *DRW_viewport_pixelsize_get(void)
{
	return &DST.pixsize;
}

static void DRW_viewport_cache_resize(void)
{
	/* Release the memiter before clearing the mempools that references them */
	GPU_viewport_cache_release(DST.viewport);

	if (DST.vmempool != NULL) {
		BLI_mempool_clear_ex(DST.vmempool->calls, BLI_mempool_count(DST.vmempool->calls));
		BLI_mempool_clear_ex(DST.vmempool->calls_generate, BLI_mempool_count(DST.vmempool->calls_generate));
		BLI_mempool_clear_ex(DST.vmempool->calls_dynamic, BLI_mempool_count(DST.vmempool->calls_dynamic));
		BLI_mempool_clear_ex(DST.vmempool->shgroups, BLI_mempool_count(DST.vmempool->shgroups));
		BLI_mempool_clear_ex(DST.vmempool->uniforms, BLI_mempool_count(DST.vmempool->uniforms));
		BLI_mempool_clear_ex(DST.vmempool->attribs, BLI_mempool_count(DST.vmempool->attribs));
		BLI_mempool_clear_ex(DST.vmempool->passes, BLI_mempool_count(DST.vmempool->passes));
	}
}

/* It also stores viewport variable to an immutable place: DST
 * This is because a cache uniform only store reference
 * to its value. And we don't want to invalidate the cache
 * if this value change per viewport */
static void DRW_viewport_var_init(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	/* Refresh DST.size */
	if (DST.viewport) {
		int size[2];
		GPU_viewport_size_get(DST.viewport, size);
		DST.size[0] = size[0];
		DST.size[1] = size[1];

		DefaultFramebufferList *fbl = (DefaultFramebufferList *)GPU_viewport_framebuffer_list_get(DST.viewport);
		DST.default_framebuffer = fbl->default_fb;

		DST.vmempool = GPU_viewport_mempool_get(DST.viewport);

		if (DST.vmempool->calls == NULL) {
			DST.vmempool->calls = BLI_mempool_create(sizeof(DRWCall), 0, 512, 0);
		}
		if (DST.vmempool->calls_generate == NULL) {
			DST.vmempool->calls_generate = BLI_mempool_create(sizeof(DRWCallGenerate), 0, 512, 0);
		}
		if (DST.vmempool->calls_dynamic == NULL) {
			DST.vmempool->calls_dynamic = BLI_mempool_create(sizeof(DRWCallDynamic), 0, 512, 0);
		}
		if (DST.vmempool->shgroups == NULL) {
			DST.vmempool->shgroups = BLI_mempool_create(sizeof(DRWShadingGroup), 0, 256, 0);
		}
		if (DST.vmempool->uniforms == NULL) {
			DST.vmempool->uniforms = BLI_mempool_create(sizeof(DRWUniform), 0, 512, 0);
		}
		if (DST.vmempool->attribs == NULL) {
			DST.vmempool->attribs = BLI_mempool_create(sizeof(DRWAttrib), 0, 256, 0);
		}
		if (DST.vmempool->passes == NULL) {
			DST.vmempool->passes = BLI_mempool_create(sizeof(DRWPass), 0, 64, 0);
		}
	}
	else {
		DST.size[0] = 0;
		DST.size[1] = 0;

		DST.default_framebuffer = NULL;
		DST.vmempool = NULL;
	}
	/* Refresh DST.screenvecs */
	copy_v3_v3(DST.screenvecs[0], rv3d->viewinv[0]);
	copy_v3_v3(DST.screenvecs[1], rv3d->viewinv[1]);
	normalize_v3(DST.screenvecs[0]);
	normalize_v3(DST.screenvecs[1]);

	/* Refresh DST.pixelsize */
	DST.pixsize = rv3d->pixsize;

	/* Reset facing */
	DST.frontface = GL_CCW;
	DST.backface = GL_CW;
	glFrontFace(DST.frontface);

	if (DST.draw_ctx.scene->obedit) {
		ED_view3d_init_mats_rv3d(DST.draw_ctx.scene->obedit, rv3d);
	}

	/* Alloc array of texture reference. */
	if (RST.bound_texs == NULL) {
		RST.bound_texs = MEM_callocN(sizeof(GPUTexture *) * GPU_max_textures(), "Bound GPUTexture refs");
	}
	if (RST.bound_tex_slots == NULL) {
		RST.bound_tex_slots = MEM_callocN(sizeof(bool) * GPU_max_textures(), "Bound Texture Slots");
	}

	memset(viewport_matrix_override.override, 0x0, sizeof(viewport_matrix_override.override));
}

void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;
	BLI_assert(type >= DRW_MAT_PERS && type <= DRW_MAT_WININV);

	if (viewport_matrix_override.override[type]) {
		copy_m4_m4(mat, viewport_matrix_override.mat[type]);
	}
	else {
		switch (type) {
			case DRW_MAT_PERS:
				copy_m4_m4(mat, rv3d->persmat);
				break;
			case DRW_MAT_PERSINV:
				copy_m4_m4(mat, rv3d->persinv);
				break;
			case DRW_MAT_VIEW:
				copy_m4_m4(mat, rv3d->viewmat);
				break;
			case DRW_MAT_VIEWINV:
				copy_m4_m4(mat, rv3d->viewinv);
				break;
			case DRW_MAT_WIN:
				copy_m4_m4(mat, rv3d->winmat);
				break;
			case DRW_MAT_WININV:
				invert_m4_m4(mat, rv3d->winmat);
				break;
			default:
				BLI_assert(!"Matrix type invalid");
				break;
		}
	}
}

void DRW_viewport_matrix_override_set(float mat[4][4], DRWViewportMatrixType type)
{
	copy_m4_m4(viewport_matrix_override.mat[type], mat);
	viewport_matrix_override.override[type] = true;
}

void DRW_viewport_matrix_override_unset(DRWViewportMatrixType type)
{
	viewport_matrix_override.override[type] = false;
}

bool DRW_viewport_is_persp_get(void)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;
	return rv3d->is_persp;
}

DefaultFramebufferList *DRW_viewport_framebuffer_list_get(void)
{
	return GPU_viewport_framebuffer_list_get(DST.viewport);
}

DefaultTextureList *DRW_viewport_texture_list_get(void)
{
	return GPU_viewport_texture_list_get(DST.viewport);
}

void DRW_viewport_request_redraw(void)
{
	GPU_viewport_tag_update(DST.viewport);
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name SceneLayers (DRW_scenelayer)
 * \{ */

void **DRW_scene_layer_engine_data_get(DrawEngineType *engine_type, void (*callback)(void *storage))
{
	SceneLayerEngineData *sled;

	for (sled = DST.draw_ctx.scene_layer->drawdata.first; sled; sled = sled->next) {
		if (sled->engine_type == engine_type) {
			return &sled->storage;
		}
	}

	sled = MEM_callocN(sizeof(SceneLayerEngineData), "SceneLayerEngineData");
	sled->engine_type = engine_type;
	sled->free = callback;
	BLI_addtail(&DST.draw_ctx.scene_layer->drawdata, sled);

	return &sled->storage;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Objects (DRW_object)
 * \{ */

void **DRW_object_engine_data_get(
        Object *ob, DrawEngineType *engine_type, void (*callback)(void *storage))
{
	ObjectEngineData *oed;

	for (oed = ob->drawdata.first; oed; oed = oed->next) {
		if (oed->engine_type == engine_type) {
			return &oed->storage;
		}
	}

	oed = MEM_callocN(sizeof(ObjectEngineData), "ObjectEngineData");
	oed->engine_type = engine_type;
	oed->free = callback;
	BLI_addtail(&ob->drawdata, oed);

	return &oed->storage;
}

/* XXX There is definitly some overlap between this and DRW_object_engine_data_get.
 * We should get rid of one of the two. */
LampEngineData *DRW_lamp_engine_data_get(Object *ob, RenderEngineType *engine_type)
{
	BLI_assert(ob->type == OB_LAMP);

	Scene *scene = DST.draw_ctx.scene;

	/* TODO Dupliobjects */
	/* TODO Should be per scenelayer */
	return GPU_lamp_engine_data_get(scene, ob, NULL, engine_type);
}

void DRW_lamp_engine_data_free(LampEngineData *led)
{
	GPU_lamp_engine_data_free(led);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Rendering (DRW_engines)
 * \{ */

static void DRW_engines_init(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		PROFILE_START(stime);

		if (engine->engine_init) {
			engine->engine_init(data);
		}

		PROFILE_END_UPDATE(data->init_time, stime);
	}
}

static void DRW_engines_cache_init(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		if (data->text_draw_cache) {
			DRW_text_cache_destroy(data->text_draw_cache);
			data->text_draw_cache = NULL;
		}
		if (DST.text_store_p == NULL) {
			DST.text_store_p = &data->text_draw_cache;
		}

		if (engine->cache_init) {
			engine->cache_init(data);
		}
	}
}

static void DRW_engines_cache_populate(Object *ob)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		if (engine->cache_populate) {
			engine->cache_populate(data, ob);
		}
	}
}

static void DRW_engines_cache_finish(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		if (engine->cache_finish) {
			engine->cache_finish(data);
		}
	}
}

static void DRW_engines_draw_background(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		if (engine->draw_background) {
			PROFILE_START(stime);

			DRW_stats_group_start(engine->idname);
			engine->draw_background(data);
			DRW_stats_group_end();

			PROFILE_END_UPDATE(data->background_time, stime);
			return;
		}
	}

	/* No draw_background found, doing default background */
	DRW_draw_background();
}

static void DRW_engines_draw_scene(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		PROFILE_START(stime);

		if (engine->draw_scene) {
			DRW_stats_group_start(engine->idname);
			engine->draw_scene(data);
			DRW_stats_group_end();
		}

		PROFILE_END_UPDATE(data->render_time, stime);
	}
}

static void DRW_engines_draw_text(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		PROFILE_START(stime);

		if (data->text_draw_cache) {
			DRW_text_cache_draw(data->text_draw_cache, DST.draw_ctx.v3d, DST.draw_ctx.ar, false);
		}

		PROFILE_END_UPDATE(data->render_time, stime);
	}
}

#define MAX_INFO_LINES 10

/**
 * Returns the offset required for the drawing of engines info.
 */
int DRW_draw_region_engine_info_offset(void)
{
	int lines = 0;
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		/* Count the number of lines. */
		if (data->info[0] != '\0') {
			lines++;
			char *c = data->info;
			while (*c++ != '\0') {
				if (*c == '\n') {
					lines++;
				}
			}
		}
	}
	return MIN2(MAX_INFO_LINES, lines) * UI_UNIT_Y;
}

/**
 * Actual drawing;
 */
void DRW_draw_region_engine_info(void)
{
	const char *info_array_final[MAX_INFO_LINES + 1];
	/* This should be maxium number of engines running at the same time. */
	char info_array[MAX_INFO_LINES][GPU_INFO_SIZE];
	int i = 0;

	const DRWContextState *draw_ctx = DRW_context_state_get();
	ARegion *ar = draw_ctx->ar;
	float fill_color[4] = {0.0f, 0.0f, 0.0f, 0.25f};

	UI_GetThemeColor3fv(TH_HIGH_GRAD, fill_color);
	mul_v3_fl(fill_color, fill_color[3]);

	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		if (data->info[0] != '\0') {
			char *chr_current = data->info;
			char *chr_start = chr_current;
			int line_len = 0;

			while (*chr_current++ != '\0') {
				line_len++;
				if (*chr_current == '\n') {
					BLI_strncpy(info_array[i++], chr_start, line_len + 1);
					/* Re-start counting. */
					chr_start = chr_current + 1;
					line_len = -1;
				}
			}

			BLI_strncpy(info_array[i++], chr_start, line_len + 1);

			if (i >= MAX_INFO_LINES) {
				break;
			}
		}
	}

	for (int j = 0; j < i; j++) {
		info_array_final[j] = info_array[j];
	}
	info_array_final[i] = NULL;

	if (info_array[0] != NULL) {
		ED_region_info_draw_multiline(ar, info_array_final, fill_color, true);
	}
}

#undef MAX_INFO_LINES

static void use_drw_engine(DrawEngineType *engine)
{
	LinkData *ld = MEM_callocN(sizeof(LinkData), "enabled engine link data");
	ld->data = engine;
	BLI_addtail(&DST.enabled_engines, ld);
}

/* TODO revisit this when proper layering is implemented */
/* Gather all draw engines needed and store them in DST.enabled_engines
 * That also define the rendering order of engines */
static void DRW_engines_enable_from_engine(RenderEngineType *engine)
{
	/* TODO layers */
	if (engine->draw_engine != NULL) {
		use_drw_engine(engine->draw_engine);
	}

	if ((engine->flag & RE_INTERNAL) == 0) {
		DRW_engines_enable_external();
	}
}

static void DRW_engines_enable_from_object_mode(void)
{
	use_drw_engine(&draw_engine_object_type);
}

static void DRW_engines_enable_from_mode(int mode)
{
	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			use_drw_engine(&draw_engine_edit_mesh_type);
			break;
		case CTX_MODE_EDIT_CURVE:
			use_drw_engine(&draw_engine_edit_curve_type);
			break;
		case CTX_MODE_EDIT_SURFACE:
			use_drw_engine(&draw_engine_edit_surface_type);
			break;
		case CTX_MODE_EDIT_TEXT:
			use_drw_engine(&draw_engine_edit_text_type);
			break;
		case CTX_MODE_EDIT_ARMATURE:
			use_drw_engine(&draw_engine_edit_armature_type);
			break;
		case CTX_MODE_EDIT_METABALL:
			use_drw_engine(&draw_engine_edit_metaball_type);
			break;
		case CTX_MODE_EDIT_LATTICE:
			use_drw_engine(&draw_engine_edit_lattice_type);
			break;
		case CTX_MODE_POSE:
			use_drw_engine(&draw_engine_pose_type);
			break;
		case CTX_MODE_SCULPT:
			use_drw_engine(&draw_engine_sculpt_type);
			break;
		case CTX_MODE_PAINT_WEIGHT:
			use_drw_engine(&draw_engine_pose_type);
			use_drw_engine(&draw_engine_paint_weight_type);
			break;
		case CTX_MODE_PAINT_VERTEX:
			use_drw_engine(&draw_engine_paint_vertex_type);
			break;
		case CTX_MODE_PAINT_TEXTURE:
			use_drw_engine(&draw_engine_paint_texture_type);
			break;
		case CTX_MODE_PARTICLE:
			use_drw_engine(&draw_engine_particle_type);
			break;
		case CTX_MODE_OBJECT:
			break;
		default:
			BLI_assert(!"Draw mode invalid");
			break;
	}
}

/**
 * Use for select and depth-drawing.
 */
static void DRW_engines_enable_basic(void)
{
	use_drw_engine(DRW_engine_viewport_basic_type.draw_engine);
}

/**
 * Use for external render engines.
 */
static void DRW_engines_enable_external(void)
{
	use_drw_engine(DRW_engine_viewport_external_type.draw_engine);
}

static void DRW_engines_enable(const Scene *scene, SceneLayer *sl, RenderEngineType *engine)
{
	Object *obact = OBACT_NEW(sl);
	const int mode = CTX_data_mode_enum_ex(scene->obedit, obact);

	DRW_engines_enable_from_engine(engine);

	if (DRW_state_draw_support()) {
		DRW_engines_enable_from_object_mode();
		DRW_engines_enable_from_mode(mode);
	}
}

static void DRW_engines_disable(void)
{
	BLI_freelistN(&DST.enabled_engines);
}

static unsigned int DRW_engines_get_hash(void)
{
	unsigned int hash = 0;
	/* The cache depends on enabled engines */
	/* FIXME : if collision occurs ... segfault */
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		hash += BLI_ghashutil_strhash_p(engine->idname);
	}

	return hash;
}

static void draw_stat(rcti *rect, int u, int v, const char *txt, const int size)
{
	BLF_draw_default_ascii(rect->xmin + (1 + u * 5) * U.widget_unit,
	                       rect->ymax - (3 + v) * U.widget_unit, 0.0f,
	                       txt, size);
}

/* CPU stats */
static void DRW_debug_cpu_stats(void)
{
	int u, v;
	double init_tot_time = 0.0, background_tot_time = 0.0, render_tot_time = 0.0, tot_time = 0.0;
	/* local coordinate visible rect inside region, to accomodate overlapping ui */
	rcti rect;
	struct ARegion *ar = DST.draw_ctx.ar;
	ED_region_visible_rect(ar, &rect);

	UI_FontThemeColor(BLF_default(), TH_TEXT_HI);

	/* row by row */
	v = 0; u = 0;
	/* Label row */
	char col_label[32];
	sprintf(col_label, "Engine");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	sprintf(col_label, "Init");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	sprintf(col_label, "Background");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	sprintf(col_label, "Render");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	sprintf(col_label, "Total (w/o cache)");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	v++;

	/* Engines rows */
	char time_to_txt[16];
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		u = 0;
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);

		draw_stat(&rect, u++, v, engine->idname, sizeof(engine->idname));

		init_tot_time += data->init_time;
		sprintf(time_to_txt, "%.2fms", data->init_time);
		draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));

		background_tot_time += data->background_time;
		sprintf(time_to_txt, "%.2fms", data->background_time);
		draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));

		render_tot_time += data->render_time;
		sprintf(time_to_txt, "%.2fms", data->render_time);
		draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));

		tot_time += data->init_time + data->background_time + data->render_time;
		sprintf(time_to_txt, "%.2fms", data->init_time + data->background_time + data->render_time);
		draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
		v++;
	}

	/* Totals row */
	u = 0;
	sprintf(col_label, "Sub Total");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	sprintf(time_to_txt, "%.2fms", init_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", background_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", render_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	v += 2;

	u = 0;
	sprintf(col_label, "Cache Time");
	draw_stat(&rect, u++, v, col_label, sizeof(col_label));
	sprintf(time_to_txt, "%.2fms", DST.cache_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
}

/* Display GPU time for each passes */
static void DRW_debug_gpu_stats(void)
{
	/* local coordinate visible rect inside region, to accomodate overlapping ui */
	rcti rect;
	struct ARegion *ar = DST.draw_ctx.ar;
	ED_region_visible_rect(ar, &rect);

	UI_FontThemeColor(BLF_default(), TH_TEXT_HI);

	int v = BLI_listbase_count(&DST.enabled_engines) + 5;

	char stat_string[32];

	/* Memory Stats */
	unsigned int tex_mem = GPU_texture_memory_usage_get();
	unsigned int vbo_mem = GWN_vertbuf_get_memory_usage();

	sprintf(stat_string, "GPU Memory");
	draw_stat(&rect, 0, v, stat_string, sizeof(stat_string));
	sprintf(stat_string, "%.2fMB", (double)(tex_mem + vbo_mem) / 1000000.0);
	draw_stat(&rect, 1, v++, stat_string, sizeof(stat_string));
	sprintf(stat_string, "   |--> Textures");
	draw_stat(&rect, 0, v, stat_string, sizeof(stat_string));
	sprintf(stat_string, "%.2fMB", (double)tex_mem / 1000000.0);
	draw_stat(&rect, 1, v++, stat_string, sizeof(stat_string));
	sprintf(stat_string, "   |--> Meshes");
	draw_stat(&rect, 0, v, stat_string, sizeof(stat_string));
	sprintf(stat_string, "%.2fMB", (double)vbo_mem / 1000000.0);
	draw_stat(&rect, 1, v++, stat_string, sizeof(stat_string));

	/* Pre offset for stats_draw */
	rect.ymax -= (3 + ++v) * U.widget_unit;

	/* Rendering Stats */
	DRW_stats_draw(&rect);
}

/* -------------------------------------------------------------------- */

/** \name View Update
 * \{ */

void DRW_notify_view_update(const bContext *C)
{
	struct Depsgraph *graph = CTX_data_depsgraph(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);
	RegionView3D *rv3d = ar->regiondata;
	Scene *scene = DEG_get_evaluated_scene(graph);
	RenderEngineType *engine = CTX_data_engine(C);
	SceneLayer *scene_layer = CTX_data_scene_layer(C);

	if (rv3d->viewport == NULL) {
		return;
	}


	/* Reset before using it. */
	memset(&DST, 0x0, sizeof(DST));

	DST.viewport = rv3d->viewport;
	DST.draw_ctx = (DRWContextState){
		ar, rv3d, v3d, scene, scene_layer, OBACT_NEW(scene_layer), engine, C,
	};

	DRW_engines_enable(scene, scene_layer, engine);

	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *draw_engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(draw_engine);

		if (draw_engine->view_update) {
			draw_engine->view_update(data);
		}
	}

	DST.viewport = NULL;

	DRW_engines_disable();
}

/** \} */

/* -------------------------------------------------------------------- */

/** \name Main Draw Loops (DRW_draw)
 * \{ */

/* Everything starts here.
 * This function takes care of calling all cache and rendering functions
 * for each relevant engine / mode engine. */
void DRW_draw_view(const bContext *C)
{
	struct Depsgraph *graph = CTX_data_depsgraph(C);
	RenderEngineType *engine = CTX_data_engine(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);

	/* Reset before using it. */
	memset(&DST, 0x0, sizeof(DST));
	DRW_draw_render_loop_ex(graph, engine, ar, v3d, C);
}

/**
 * Used for both regular and off-screen drawing.
 * Need to reset DST before calling this function
 */
void DRW_draw_render_loop_ex(
        struct Depsgraph *graph,
        RenderEngineType *engine,
        ARegion *ar, View3D *v3d,
        const bContext *evil_C)
{
	Scene *scene = DEG_get_evaluated_scene(graph);
	SceneLayer *scene_layer = DEG_get_evaluated_scene_layer(graph);
	RegionView3D *rv3d = ar->regiondata;

	DST.draw_ctx.evil_C = evil_C;

	DST.viewport = rv3d->viewport;
	v3d->zbuf = true;

	/* Setup viewport */
	GPU_viewport_engines_data_validate(DST.viewport, DRW_engines_get_hash());

	DST.draw_ctx = (DRWContextState){
	    ar, rv3d, v3d, scene, scene_layer, OBACT_NEW(scene_layer), engine,

	    /* reuse if caller sets */
	    DST.draw_ctx.evil_C,
	};

	DRW_viewport_var_init();

	/* Get list of enabled engines */
	DRW_engines_enable(scene, scene_layer, engine);

	/* Update ubos */
	DRW_globals_update();

	/* Init engines */
	DRW_engines_init();

	/* TODO : tag to refresh by the deps graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
	{
		PROFILE_START(stime);
		DRW_engines_cache_init();

		DEG_OBJECT_ITER(graph, ob, DEG_OBJECT_ITER_FLAG_ALL);
		{
			DRW_engines_cache_populate(ob);
			/* XXX find a better place for this. maybe Depsgraph? */
			ob->deg_update_flag = 0;
		}
		DEG_OBJECT_ITER_END

		DRW_engines_cache_finish();
		PROFILE_END_ACCUM(DST.cache_time, stime);
	}

	DRW_stats_begin();

	/* Start Drawing */
	DRW_state_reset();
	DRW_engines_draw_background();

	/* WIP, single image drawn over the camera view (replace) */
	bool do_bg_image = false;
	if (rv3d->persp == RV3D_CAMOB) {
		Object *cam_ob = v3d->camera;
		if (cam_ob && cam_ob->type == OB_CAMERA) {
			Camera *cam = cam_ob->data;
			if (!BLI_listbase_is_empty(&cam->bg_images)) {
				do_bg_image = true;
			}
		}
	}

	extern void view3d_draw_bgpic_test(Scene *scene, ARegion *ar, View3D *v3d,
									   const bool do_foreground, const bool do_camera_frame);
	if (do_bg_image) {
		view3d_draw_bgpic_test(scene, ar, v3d, false, true);
	}


	DRW_draw_callbacks_pre_scene();
	if (DST.draw_ctx.evil_C) {
		ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.ar, REGION_DRAW_PRE_VIEW);
	}

	DRW_engines_draw_scene();

	DRW_draw_callbacks_post_scene();
	if (DST.draw_ctx.evil_C) {
		ED_region_draw_cb_draw(DST.draw_ctx.evil_C, DST.draw_ctx.ar, REGION_DRAW_POST_VIEW);
	}

	DRW_state_reset();

	DRW_engines_draw_text();

	if (DST.draw_ctx.evil_C) {
		/* needed so manipulator isn't obscured */
		glDisable(GL_DEPTH_TEST);
		DRW_draw_manipulator();
		glEnable(GL_DEPTH_TEST);

		DRW_draw_region_info();
	}

	DRW_stats_reset();

	if (do_bg_image) {
		view3d_draw_bgpic_test(scene, ar, v3d, true, true);
	}

	if (G.debug_value > 20) {
		DRW_debug_cpu_stats();
		DRW_debug_gpu_stats();
	}

	DRW_state_reset();
	DRW_engines_disable();

	DRW_viewport_cache_resize();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	memset(&DST, 0xFF, sizeof(DST));
#endif
}

void DRW_draw_render_loop(
        struct Depsgraph *graph,
        ARegion *ar, View3D *v3d)
{
	/* Reset before using it. */
	memset(&DST, 0x0, sizeof(DST));

	Scene *scene = DEG_get_evaluated_scene(graph);
	RenderEngineType *engine = RE_engines_find(scene->view_render.engine_id);

	DRW_draw_render_loop_ex(graph, engine, ar, v3d, NULL);
}

void DRW_draw_render_loop_offscreen(
        struct Depsgraph *graph, RenderEngineType *engine,
        ARegion *ar, View3D *v3d, GPUOffScreen *ofs)
{
	RegionView3D *rv3d = ar->regiondata;

	/* backup */
	void *backup_viewport = rv3d->viewport;
	{
		/* backup (_never_ use rv3d->viewport) */
		rv3d->viewport = GPU_viewport_create_from_offscreen(ofs);
	}

	/* Reset before using it. */
	memset(&DST, 0x0, sizeof(DST));
	DST.options.is_image_render = true;
	DRW_draw_render_loop_ex(graph, engine, ar, v3d, NULL);

	/* restore */
	{
		/* don't free data owned by 'ofs' */
		GPU_viewport_clear_from_offscreen(rv3d->viewport);
		GPU_viewport_free(rv3d->viewport);
		MEM_freeN(rv3d->viewport);

		rv3d->viewport = backup_viewport;
	}

	/* we need to re-bind (annoying!) */
	GPU_offscreen_bind(ofs, false);
}

/**
 * object mode select-loop, see: ED_view3d_draw_select_loop (legacy drawing).
 */
void DRW_draw_select_loop(
        struct Depsgraph *graph,
        ARegion *ar, View3D *v3d,
        bool UNUSED(use_obedit_skip), bool UNUSED(use_nearest), const rcti *rect)
{
	Scene *scene = DEG_get_evaluated_scene(graph);
	RenderEngineType *engine = RE_engines_find(scene->view_render.engine_id);
	SceneLayer *sl = DEG_get_evaluated_scene_layer(graph);
#ifndef USE_GPU_SELECT
	UNUSED_VARS(vc, scene, sl, v3d, ar, rect);
#else
	RegionView3D *rv3d = ar->regiondata;

	/* Reset before using it. */
	memset(&DST, 0x0, sizeof(DST));

	/* backup (_never_ use rv3d->viewport) */
	void *backup_viewport = rv3d->viewport;
	rv3d->viewport = NULL;

	bool use_obedit = false;
	int obedit_mode = 0;
	if (scene->obedit && scene->obedit->type == OB_MBALL) {
		use_obedit = true;
		DRW_engines_cache_populate(scene->obedit);
		obedit_mode = CTX_MODE_EDIT_METABALL;
	}
	else if ((scene->obedit && scene->obedit->type == OB_ARMATURE)) {
		/* if not drawing sketch, draw bones */
		// if (!BDR_drawSketchNames(vc))
		{
			use_obedit = true;
			obedit_mode = CTX_MODE_EDIT_ARMATURE;
		}
	}

	struct GPUViewport *viewport = GPU_viewport_create();
	GPU_viewport_size_set(viewport, (const int[2]){BLI_rcti_size_x(rect), BLI_rcti_size_y(rect)});

	bool cache_is_dirty;
	DST.viewport = viewport;
	v3d->zbuf = true;

	DST.options.is_select = true;

	/* Get list of enabled engines */
	if (use_obedit) {
		DRW_engines_enable_from_mode(obedit_mode);
	}
	else {
		DRW_engines_enable_basic();
		DRW_engines_enable_from_object_mode();
	}

	/* Setup viewport */
	cache_is_dirty = true;

	/* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
	DST.draw_ctx = (DRWContextState){
		ar, rv3d, v3d, scene, sl, OBACT_NEW(sl), engine, (bContext *)NULL,
	};

	DRW_viewport_var_init();

	/* Update ubos */
	DRW_globals_update();

	/* Init engines */
	DRW_engines_init();

	/* TODO : tag to refresh by the deps graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
	if (cache_is_dirty) {

		DRW_engines_cache_init();

		if (use_obedit) {
			DRW_engines_cache_populate(scene->obedit);
		}
		else {
			DEG_OBJECT_ITER(graph, ob, DEG_OBJECT_ITER_FLAG_DUPLI)
			{
				if ((ob->base_flag & BASE_SELECTABLED) != 0) {
					DRW_select_load_id(ob->select_color);
					DRW_engines_cache_populate(ob);
				}
			}
			DEG_OBJECT_ITER_END
		}

		DRW_engines_cache_finish();
	}

	/* Start Drawing */
	DRW_state_reset();
	DRW_draw_callbacks_pre_scene();
	DRW_engines_draw_scene();
	DRW_draw_callbacks_post_scene();

	DRW_state_reset();
	DRW_engines_disable();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	memset(&DST, 0xFF, sizeof(DST));
#endif

	/* Cleanup for selection state */
	GPU_viewport_free(viewport);
	MEM_freeN(viewport);

	/* restore */
	rv3d->viewport = backup_viewport;
#endif  /* USE_GPU_SELECT */
}

/**
 * object mode select-loop, see: ED_view3d_draw_depth_loop (legacy drawing).
 */
void DRW_draw_depth_loop(
        Depsgraph *graph,
        ARegion *ar, View3D *v3d)
{
	Scene *scene = DEG_get_evaluated_scene(graph);
	RenderEngineType *engine = RE_engines_find(scene->view_render.engine_id);
	SceneLayer *sl = DEG_get_evaluated_scene_layer(graph);
	RegionView3D *rv3d = ar->regiondata;

	/* backup (_never_ use rv3d->viewport) */
	void *backup_viewport = rv3d->viewport;
	rv3d->viewport = NULL;

	/* Reset before using it. */
	memset(&DST, 0x0, sizeof(DST));

	struct GPUViewport *viewport = GPU_viewport_create();
	GPU_viewport_size_set(viewport, (const int[2]){ar->winx, ar->winy});

	bool cache_is_dirty;
	DST.viewport = viewport;
	v3d->zbuf = true;

	DST.options.is_depth = true;

	/* Get list of enabled engines */
	{
		DRW_engines_enable_basic();
		DRW_engines_enable_from_object_mode();
	}

	/* Setup viewport */
	cache_is_dirty = true;

	/* Instead of 'DRW_context_state_init(C, &DST.draw_ctx)', assign from args */
	DST.draw_ctx = (DRWContextState){
		ar, rv3d, v3d, scene, sl, OBACT_NEW(sl), engine, (bContext *)NULL,
	};

	DRW_viewport_var_init();

	/* Update ubos */
	DRW_globals_update();

	/* Init engines */
	DRW_engines_init();

	/* TODO : tag to refresh by the deps graph */
	/* ideally only refresh when objects are added/removed */
	/* or render properties / materials change */
	if (cache_is_dirty) {

		DRW_engines_cache_init();

		DEG_OBJECT_ITER(graph, ob, DEG_OBJECT_ITER_FLAG_ALL)
		{
			DRW_engines_cache_populate(ob);
		}
		DEG_OBJECT_ITER_END

		DRW_engines_cache_finish();
	}

	/* Start Drawing */
	DRW_state_reset();
	DRW_draw_callbacks_pre_scene();
	DRW_engines_draw_scene();
	DRW_draw_callbacks_post_scene();

	DRW_state_reset();
	DRW_engines_disable();

#ifdef DEBUG
	/* Avoid accidental reuse. */
	memset(&DST, 0xFF, sizeof(DST));
#endif

	/* Cleanup for selection state */
	GPU_viewport_free(viewport);
	MEM_freeN(viewport);

	/* restore */
	rv3d->viewport = backup_viewport;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Draw Manager State (DRW_state)
 * \{ */

void DRW_state_dfdy_factors_get(float dfdyfac[2])
{
	GPU_get_dfdy_factors(dfdyfac);
}

/**
 * When false, drawing doesn't output to a pixel buffer
 * eg: Occlusion queries, or when we have setup a context to draw in already.
 */
bool DRW_state_is_fbo(void)
{
	return (DST.default_framebuffer != NULL);
}

/**
 * For when engines need to know if this is drawing for selection or not.
 */
bool DRW_state_is_select(void)
{
	return DST.options.is_select;
}

bool DRW_state_is_depth(void)
{
	return DST.options.is_depth;
}

/**
 * Whether we are rendering for an image
 */
bool DRW_state_is_image_render(void)
{
	return DST.options.is_image_render;
}

/**
 * Whether we are rendering only the render engine,
 * or if we should also render the mode engines.
 */
bool DRW_state_is_scene_render(void)
{
	BLI_assert(DST.options.is_scene_render ?
	           DST.options.is_image_render : true);
	return DST.options.is_scene_render;
}

/**
 * Should text draw in this mode?
 */
bool DRW_state_show_text(void)
{
	return (DST.options.is_select) == 0 &&
	       (DST.options.is_depth) == 0 &&
	       (DST.options.is_scene_render) == 0;
}

/**
 * Should draw support elements
 * Objects center, selection outline, probe data, ...
 */
bool DRW_state_draw_support(void)
{
	View3D *v3d = DST.draw_ctx.v3d;
	return (DRW_state_is_scene_render() == false) &&
	        (v3d != NULL) &&
	        ((v3d->flag2 & V3D_RENDER_OVERRIDE) == 0);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Context State (DRW_context_state)
 * \{ */

const DRWContextState *DRW_context_state_get(void)
{
	return &DST.draw_ctx;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Init/Exit (DRW_engines)
 * \{ */

void DRW_engine_register(DrawEngineType *draw_engine_type)
{
	BLI_addtail(&DRW_engines, draw_engine_type);
}

void DRW_engines_register(void)
{
#ifdef WITH_CLAY_ENGINE
	RE_engines_register(NULL, &DRW_engine_viewport_clay_type);
#endif
	RE_engines_register(NULL, &DRW_engine_viewport_eevee_type);

	DRW_engine_register(&draw_engine_object_type);
	DRW_engine_register(&draw_engine_edit_armature_type);
	DRW_engine_register(&draw_engine_edit_curve_type);
	DRW_engine_register(&draw_engine_edit_lattice_type);
	DRW_engine_register(&draw_engine_edit_mesh_type);
	DRW_engine_register(&draw_engine_edit_metaball_type);
	DRW_engine_register(&draw_engine_edit_surface_type);
	DRW_engine_register(&draw_engine_edit_text_type);
	DRW_engine_register(&draw_engine_paint_texture_type);
	DRW_engine_register(&draw_engine_paint_vertex_type);
	DRW_engine_register(&draw_engine_paint_weight_type);
	DRW_engine_register(&draw_engine_particle_type);
	DRW_engine_register(&draw_engine_pose_type);
	DRW_engine_register(&draw_engine_sculpt_type);

	/* setup callbacks */
	{
		/* BKE: curve.c */
		extern void *BKE_curve_batch_cache_dirty_cb;
		extern void *BKE_curve_batch_cache_free_cb;
		/* BKE: mesh.c */
		extern void *BKE_mesh_batch_cache_dirty_cb;
		extern void *BKE_mesh_batch_cache_free_cb;
		/* BKE: lattice.c */
		extern void *BKE_lattice_batch_cache_dirty_cb;
		extern void *BKE_lattice_batch_cache_free_cb;
		/* BKE: particle.c */
		extern void *BKE_particle_batch_cache_dirty_cb;
		extern void *BKE_particle_batch_cache_free_cb;

		BKE_curve_batch_cache_dirty_cb = DRW_curve_batch_cache_dirty;
		BKE_curve_batch_cache_free_cb = DRW_curve_batch_cache_free;

		BKE_mesh_batch_cache_dirty_cb = DRW_mesh_batch_cache_dirty;
		BKE_mesh_batch_cache_free_cb = DRW_mesh_batch_cache_free;

		BKE_lattice_batch_cache_dirty_cb = DRW_lattice_batch_cache_dirty;
		BKE_lattice_batch_cache_free_cb = DRW_lattice_batch_cache_free;

		BKE_particle_batch_cache_dirty_cb = DRW_particle_batch_cache_dirty;
		BKE_particle_batch_cache_free_cb = DRW_particle_batch_cache_free;
	}
}

extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
extern struct GPUTexture *globals_ramp; /* draw_common.c */
void DRW_engines_free(void)
{
	DRW_shape_cache_free();
	DRW_stats_free();

	DrawEngineType *next;
	for (DrawEngineType *type = DRW_engines.first; type; type = next) {
		next = type->next;
		BLI_remlink(&R_engines, type);

		if (type->engine_free) {
			type->engine_free();
		}
	}

	if (globals_ubo)
		GPU_uniformbuffer_free(globals_ubo);

	if (globals_ramp)
		GPU_texture_free(globals_ramp);

	MEM_SAFE_FREE(RST.bound_texs);
	MEM_SAFE_FREE(RST.bound_tex_slots);

#ifdef WITH_CLAY_ENGINE
	BLI_remlink(&R_engines, &DRW_engine_viewport_clay_type);
#endif
}

/** \} */

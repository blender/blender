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
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BIF_glutil.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"

#include "BLT_translation.h"
#include "BLF_api.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_view3d_types.h"
#include "DNA_screen_types.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "intern/gpu_codegen.h"
#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_lamp.h"
#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"
#include "GPU_matrix.h"

#include "PIL_time.h"

#include "RE_engine.h"

#include "UI_resources.h"

#include "draw_manager_text.h"

/* only for callbacks */
#include "draw_cache_impl.h"

#include "draw_mode_engines.h"

#include "engines/clay/clay_engine.h"
#include "engines/eevee/eevee_engine.h"
#include "engines/basic/basic_engine.h"
#include "engines/external/external_engine.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#define MAX_ATTRIB_NAME 32
#define MAX_PASS_NAME 32

/* Use draw manager to call GPU_select, see: DRW_draw_select_loop */
#define USE_GPU_SELECT

#ifdef USE_GPU_SELECT
#  include "ED_view3d.h"
#  include "ED_armature.h"
#  include "GPU_select.h"
#endif

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_fullscreen_vert_glsl[];

/* Prototypes. */
static void DRW_engines_enable_external(void);

/* Structures */
typedef enum {
	DRW_UNIFORM_BOOL,
	DRW_UNIFORM_SHORT,
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
	struct DRWUniform *next, *prev;
	DRWUniformType type;
	int location;
	int length;
	int arraysize;
	int bindloc;
	const void *value;
};

typedef struct DRWAttrib {
	struct DRWAttrib *next, *prev;
	char name[MAX_ATTRIB_NAME];
	int location;
	int format_id;
	int size; /* number of component */
	int type;
} DRWAttrib;

struct DRWInterface {
	ListBase uniforms;   /* DRWUniform */
	ListBase attribs;    /* DRWAttrib */
	int attribs_count;
	int attribs_stride;
	int attribs_size[16];
	int attribs_loc[16];
	/* matrices locations */
	int model;
	int modelview;
	int projection;
	int view;
	int viewinverse;
	int modelviewprojection;
	int viewprojection;
	int normal;
	int worldnormal;
	int eye;
	/* Dynamic batch */
	GLuint instance_vbo;
	int instance_count;
	VertexFormat vbo_format;
};

struct DRWPass {
	ListBase shgroups; /* DRWShadingGroup */
	DRWState state;
	char name[MAX_PASS_NAME];
	/* use two query to not stall the cpu waiting for queries to complete */
	unsigned int timer_queries[2];
	/* alternate between front and back query */
	unsigned int front_idx;
	unsigned int back_idx;
	bool wasdrawn; /* if it was drawn during this frame */
};

typedef struct DRWCall {
	struct DRWCall *next, *prev;
#ifdef USE_GPU_SELECT
	int select_id;
#endif
	Batch *geometry;
	float (*obmat)[4];
} DRWCall;

typedef struct DRWCallDynamic {
	struct DRWCallDynamic *next, *prev;
#ifdef USE_GPU_SELECT
	int select_id;
#endif
	const void *data[];
} DRWCallDynamic;

struct DRWShadingGroup {
	struct DRWShadingGroup *next, *prev;

	GPUShader *shader;               /* Shader to bind */
	DRWInterface *interface;         /* Uniforms pointers */
	ListBase calls;                  /* DRWCall or DRWCallDynamic depending of type */
	DRWState state_extra;            /* State changes for this batch only (or'd with the pass's state) */
	int type;

	Batch *instance_geom;  /* Geometry to instance */
	Batch *batch_geom;     /* Result of call batching */

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
	DRW_SHG_INSTANCE,
};

/* only 16 bits long */
enum {
	STENCIL_SELECT          = (1 << 0),
	STENCIL_ACTIVE          = (1 << 1),
};

/* Render State */
static struct DRWGlobalState {
	/* Rendering state */
	GPUShader *shader;
	ListBase bound_texs;
	int tex_bind_id;

	/* Managed by `DRW_state_set`, `DRW_state_reset` */
	DRWState state;

	/* Per viewport */
	GPUViewport *viewport;
	struct GPUFrameBuffer *default_framebuffer;
	float size[2];
	float screenvecs[2][3];
	float pixsize;

	struct {
		unsigned int is_select : 1;
		unsigned int is_depth : 1;
	} options;

	/* Current rendering context */
	DRWContextState draw_ctx;

	/* Convenience pointer to text_store owned by the viewport */
	struct DRWTextStore **text_store_p;

	ListBase enabled_engines; /* RenderEngineType */
} DST = {NULL};

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

static void drw_texture_get_format(DRWTextureFormat format, GPUTextureFormat *data_type, int *channels)
{
	switch (format) {
		case DRW_TEX_RGBA_8: *data_type = GPU_RGBA8; break;
		case DRW_TEX_RGBA_16: *data_type = GPU_RGBA16F; break;
		case DRW_TEX_RGB_16: *data_type = GPU_RGB16F; break;
		case DRW_TEX_RG_16: *data_type = GPU_RG16F; break;
		case DRW_TEX_RG_32: *data_type = GPU_RG32F; break;
		case DRW_TEX_R_8: *data_type = GPU_R8; break;
		case DRW_TEX_R_16: *data_type = GPU_R16F; break;
#if 0
		case DRW_TEX_RGBA_32: *data_type = GPU_RGBA32F; break;
		case DRW_TEX_RGB_8: *data_type = GPU_RGB8; break;
		case DRW_TEX_RGB_32: *data_type = GPU_RGB32F; break;
		case DRW_TEX_RG_8: *data_type = GPU_RG8; break;
		case DRW_TEX_R_32: *data_type = GPU_R32F; break;
#endif
		case DRW_TEX_DEPTH_16: *data_type = GPU_DEPTH_COMPONENT16; break;
		case DRW_TEX_DEPTH_24: *data_type = GPU_DEPTH_COMPONENT24; break;
		case DRW_TEX_DEPTH_32: *data_type = GPU_DEPTH_COMPONENT32F; break;
		default :
			/* file type not supported you must uncomment it from above */
			BLI_assert(false);
			break;
	}

	switch (format) {
		case DRW_TEX_RGBA_8:
		case DRW_TEX_RGBA_16:
		case DRW_TEX_RGBA_32:
			*channels = 4;
			break;
		case DRW_TEX_RGB_8:
		case DRW_TEX_RGB_16:
		case DRW_TEX_RGB_32:
			*channels = 3;
			break;
		case DRW_TEX_RG_8:
		case DRW_TEX_RG_16:
		case DRW_TEX_RG_32:
			*channels = 2;
			break;
		default:
			*channels = 1;
			break;
	}
}

static void drw_texture_set_parameters(GPUTexture *tex, DRWTextureFlag flags)
{
	GPU_texture_bind(tex, 0);
	GPU_texture_filter_mode(tex, flags & DRW_TEX_FILTER);
	if (flags & DRW_TEX_MIPMAP) {
		GPU_texture_mipmap_mode(tex, true);
		DRW_texture_generate_mipmaps(tex);
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

static DRWInterface *DRW_interface_create(GPUShader *shader)
{
	DRWInterface *interface = MEM_mallocN(sizeof(DRWInterface), "DRWInterface");

	interface->model = GPU_shader_get_uniform(shader, "ModelMatrix");
	interface->modelview = GPU_shader_get_uniform(shader, "ModelViewMatrix");
	interface->projection = GPU_shader_get_uniform(shader, "ProjectionMatrix");
	interface->view = GPU_shader_get_uniform(shader, "ViewMatrix");
	interface->viewinverse = GPU_shader_get_uniform(shader, "ViewMatrixInverse");
	interface->viewprojection = GPU_shader_get_uniform(shader, "ViewProjectionMatrix");
	interface->modelviewprojection = GPU_shader_get_uniform(shader, "ModelViewProjectionMatrix");
	interface->normal = GPU_shader_get_uniform(shader, "NormalMatrix");
	interface->worldnormal = GPU_shader_get_uniform(shader, "WorldNormalMatrix");
	interface->eye = GPU_shader_get_uniform(shader, "eye");
	interface->instance_count = 0;
	interface->attribs_count = 0;
	interface->attribs_stride = 0;
	interface->instance_vbo = 0;

	memset(&interface->vbo_format, 0, sizeof(VertexFormat));

	BLI_listbase_clear(&interface->uniforms);
	BLI_listbase_clear(&interface->attribs);

	return interface;
}

#ifdef USE_GPU_SELECT
static DRWInterface *DRW_interface_duplicate(DRWInterface *interface_src)
{
	DRWInterface *interface_dst = MEM_dupallocN(interface_src);
	BLI_duplicatelist(&interface_dst->uniforms, &interface_src->uniforms);
	BLI_duplicatelist(&interface_dst->attribs, &interface_src->attribs);
	return interface_dst;
}
#endif

static void DRW_interface_uniform(DRWShadingGroup *shgroup, const char *name,
                                  DRWUniformType type, const void *value, int length, int arraysize, int bindloc)
{
	DRWUniform *uni = MEM_mallocN(sizeof(DRWUniform), "DRWUniform");

	if (type == DRW_UNIFORM_BLOCK) {
		uni->location = GPU_shader_get_uniform_block(shgroup->shader, name);
	}
	else {
		uni->location = GPU_shader_get_uniform(shgroup->shader, name);
	}

	uni->type = type;
	uni->value = value;
	uni->length = length;
	uni->arraysize = arraysize;
	uni->bindloc = bindloc; /* for textures */

	if (uni->location == -1) {
		if (G.debug & G_DEBUG)
			fprintf(stderr, "Uniform '%s' not found!\n", name);

		MEM_freeN(uni);
		return;
	}

	BLI_addtail(&shgroup->interface->uniforms, uni);
}

static void DRW_interface_attrib(DRWShadingGroup *shgroup, const char *name, DRWAttribType type, int size)
{
	DRWAttrib *attrib = MEM_mallocN(sizeof(DRWAttrib), "DRWAttrib");
	GLuint program = GPU_shader_get_program(shgroup->shader);

	attrib->location = glGetAttribLocation(program, name);
	attrib->type = type;
	attrib->size = size;

	if (attrib->location == -1) {
		if (G.debug & G_DEBUG)
			fprintf(stderr, "Attribute '%s' not found!\n", name);

		MEM_freeN(attrib);
		return;
	}

	BLI_assert(BLI_strnlen(name, 32) < 32);
	BLI_strncpy(attrib->name, name, 32);

	shgroup->interface->attribs_count += 1;

	BLI_addtail(&shgroup->interface->attribs, attrib);
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Shading Group (DRW_shgroup)
 * \{ */

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWShadingGroup *shgroup = MEM_mallocN(sizeof(DRWShadingGroup), "DRWShadingGroup");

	shgroup->type = DRW_SHG_NORMAL;
	shgroup->shader = shader;
	shgroup->interface = DRW_interface_create(shader);
	shgroup->state_extra = 0;
	shgroup->batch_geom = NULL;
	shgroup->instance_geom = NULL;

	BLI_addtail(&pass->shgroups, shgroup);
	BLI_listbase_clear(&shgroup->calls);

#ifdef USE_GPU_SELECT
	shgroup->pass_parent = pass;
#endif

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_material_create(struct GPUMaterial *material, DRWPass *pass)
{
	double time = 0.0; /* TODO make time variable */
	const int max_tex = GPU_max_textures() - 1;

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
			GPUTexture *tex = GPU_texture_from_blender(input->ima, input->iuser, input->textarget, input->image_isdata, time, 1);

			if (input->bindtex) {
				/* TODO maybe track texture slot usage to avoid clash with engine textures */
				DRW_shgroup_uniform_texture(grp, input->shadername, tex, max_tex - input->texid);
			}
		}
		/* Color Ramps */
		else if (input->tex) {
			DRW_shgroup_uniform_texture(grp, input->shadername, input->tex, max_tex - input->texid);
		}
		/* Floats */
		else {
			switch (input->type) {
				case 1:
					DRW_shgroup_uniform_float(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case 2:
					DRW_shgroup_uniform_vec2(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case 3:
					DRW_shgroup_uniform_vec3(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case 4:
					DRW_shgroup_uniform_vec4(grp, input->shadername, (float *)input->dynamicvec, 1);
					break;
				case 9:
					DRW_shgroup_uniform_mat3(grp, input->shadername, (float *)input->dynamicvec);
					break;
				case 16:
					DRW_shgroup_uniform_mat4(grp, input->shadername, (float *)input->dynamicvec);
					break;
				default:
					break;
			}
		}
	}

	return grp;
}

DRWShadingGroup *DRW_shgroup_material_instance_create(struct GPUMaterial *material, DRWPass *pass, Batch *geom)
{
	DRWShadingGroup *shgroup = DRW_shgroup_material_create(material, pass);

	if (shgroup) {
		shgroup->type = DRW_SHG_INSTANCE;
		shgroup->instance_geom = geom;
	}

	return shgroup;
}

DRWShadingGroup *DRW_shgroup_instance_create(struct GPUShader *shader, DRWPass *pass, Batch *geom)
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

void DRW_shgroup_free(struct DRWShadingGroup *shgroup)
{
	BLI_freelistN(&shgroup->calls);
	BLI_freelistN(&shgroup->interface->uniforms);
	BLI_freelistN(&shgroup->interface->attribs);

	if (shgroup->interface->instance_vbo) {
		glDeleteBuffers(1, &shgroup->interface->instance_vbo);
	}

	MEM_freeN(shgroup->interface);

	BATCH_DISCARD_ALL_SAFE(shgroup->batch_geom);
}

void DRW_shgroup_call_add(DRWShadingGroup *shgroup, Batch *geom, float (*obmat)[4])
{
	BLI_assert(geom != NULL);

	DRWCall *call = MEM_callocN(sizeof(DRWCall), "DRWCall");

	call->obmat = obmat;
	call->geometry = geom;

#ifdef USE_GPU_SELECT
	call->select_id = g_DRW_select_id;
#endif

	BLI_addtail(&shgroup->calls, call);
}

void DRW_shgroup_call_dynamic_add_array(DRWShadingGroup *shgroup, const void *attr[], unsigned int attr_len)
{
	DRWInterface *interface = shgroup->interface;

#ifdef USE_GPU_SELECT
	if ((G.f & G_PICKSEL) && (interface->instance_count > 0)) {
		shgroup = MEM_dupallocN(shgroup);
		BLI_listbase_clear(&shgroup->calls);

		shgroup->interface = interface = DRW_interface_duplicate(interface);
		interface->instance_count = 0;

		BLI_addtail(&shgroup->pass_parent->shgroups, shgroup);
	}
#endif

	unsigned int data_size = sizeof(void *) * interface->attribs_count;
	int size = sizeof(DRWCallDynamic) + data_size;

	DRWCallDynamic *call = MEM_callocN(size, "DRWCallDynamic");

	BLI_assert(attr_len == interface->attribs_count);

#ifdef USE_GPU_SELECT
	call->select_id = g_DRW_select_id;
#endif

	memcpy((void *)call->data, attr, data_size);

	interface->instance_count += 1;

	BLI_addtail(&shgroup->calls, call);
}

/**
 * State is added to #Pass.state while drawing.
 * Use to temporarily enable draw options.
 *
 * Currently there is no way to disable (could add if needed).
 */
void DRW_shgroup_state_enable(DRWShadingGroup *shgroup, DRWState state)
{
	shgroup->state_extra |= state;
}

void DRW_shgroup_attrib_float(DRWShadingGroup *shgroup, const char *name, int size)
{
	DRW_interface_attrib(shgroup, name, DRW_ATTRIB_FLOAT, size);
}

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex, int loc)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_TEXTURE, tex, 0, 0, loc);
}

void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup, const char *name, const GPUUniformBuffer *ubo, int loc)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BLOCK, ubo, 0, 0, loc);
}

void DRW_shgroup_uniform_buffer(DRWShadingGroup *shgroup, const char *name, GPUTexture **tex, int loc)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BUFFER, tex, 0, 0, loc);
}

void DRW_shgroup_uniform_bool(DRWShadingGroup *shgroup, const char *name, const bool *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BOOL, value, 1, arraysize, 0);
}

void DRW_shgroup_uniform_float(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 1, arraysize, 0);
}

void DRW_shgroup_uniform_vec2(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 2, arraysize, 0);
}

void DRW_shgroup_uniform_vec3(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 3, arraysize, 0);
}

void DRW_shgroup_uniform_vec4(DRWShadingGroup *shgroup, const char *name, const float *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_FLOAT, value, 4, arraysize, 0);
}

void DRW_shgroup_uniform_short(DRWShadingGroup *shgroup, const char *name, const short *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_SHORT, value, 1, arraysize, 0);
}

void DRW_shgroup_uniform_int(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_INT, value, 1, arraysize, 0);
}

void DRW_shgroup_uniform_ivec2(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_INT, value, 2, arraysize, 0);
}

void DRW_shgroup_uniform_ivec3(DRWShadingGroup *shgroup, const char *name, const int *value, int arraysize)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_INT, value, 3, arraysize, 0);
}

void DRW_shgroup_uniform_mat3(DRWShadingGroup *shgroup, const char *name, const float *value)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_MAT3, value, 9, 1, 0);
}

void DRW_shgroup_uniform_mat4(DRWShadingGroup *shgroup, const char *name, const float *value)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_MAT4, value, 16, 1, 0);
}

/* Creates a VBO containing OGL primitives for all DRWCallDynamic */
static void shgroup_dynamic_batch(DRWShadingGroup *shgroup)
{
	DRWInterface *interface = shgroup->interface;
	int nbr = interface->instance_count;

	PrimitiveType type = (shgroup->type == DRW_SHG_POINT_BATCH) ? PRIM_POINTS : PRIM_LINES;

	if (nbr == 0)
		return;

	/* Upload Data */
	if (interface->vbo_format.attrib_ct == 0) {
		for (DRWAttrib *attrib = interface->attribs.first; attrib; attrib = attrib->next) {
			BLI_assert(attrib->size <= 4); /* matrices have no place here for now */
			if (attrib->type == DRW_ATTRIB_FLOAT) {
				attrib->format_id = VertexFormat_add_attrib(
				        &interface->vbo_format, attrib->name, COMP_F32, attrib->size, KEEP_FLOAT);
			}
			else if (attrib->type == DRW_ATTRIB_INT) {
				attrib->format_id = VertexFormat_add_attrib(
				        &interface->vbo_format, attrib->name, COMP_I8, attrib->size, KEEP_INT);
			}
			else {
				BLI_assert(false);
			}
		}
	}

	VertexBuffer *vbo = VertexBuffer_create_with_format(&interface->vbo_format);
	VertexBuffer_allocate_data(vbo, nbr);

	int j = 0;
	for (DRWCallDynamic *call = shgroup->calls.first; call; call = call->next, j++) {
		int i = 0;
		for (DRWAttrib *attrib = interface->attribs.first; attrib; attrib = attrib->next, i++) {
			VertexBuffer_set_attrib(vbo, attrib->format_id, j, call->data[i]);
		}
	}

	/* TODO make the batch dynamic instead of freeing it every times */
	if (shgroup->batch_geom)
		Batch_discard_all(shgroup->batch_geom);

	shgroup->batch_geom = Batch_create(type, vbo, NULL);
}

static void shgroup_dynamic_instance(DRWShadingGroup *shgroup)
{
	int i = 0;
	int offset = 0;
	DRWInterface *interface = shgroup->interface;
	int vert_nbr = interface->instance_count;
	int buffer_size = 0;

	if (vert_nbr == 0) {
		if (interface->instance_vbo) {
			glDeleteBuffers(1, &interface->instance_vbo);
			interface->instance_vbo = 0;
		}
		return;
	}

	/* only once */
	if (interface->attribs_stride == 0) {
		for (DRWAttrib *attrib = interface->attribs.first; attrib; attrib = attrib->next, i++) {
			BLI_assert(attrib->type == DRW_ATTRIB_FLOAT); /* Only float for now */
			interface->attribs_stride += attrib->size;
			interface->attribs_size[i] = attrib->size;
			interface->attribs_loc[i] = attrib->location;
		}
	}

	/* Gather Data */
	buffer_size = sizeof(float) * interface->attribs_stride * vert_nbr;
	float *data = MEM_mallocN(buffer_size, "Instance VBO data");

	for (DRWCallDynamic *call = shgroup->calls.first; call; call = call->next) {
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
	if ((shgroup->interface->instance_vbo || shgroup->batch_geom) &&
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
	DRWPass *pass = MEM_callocN(sizeof(DRWPass), name);
	pass->state = state;
	BLI_strncpy(pass->name, name, MAX_PASS_NAME);

	BLI_listbase_clear(&pass->shgroups);

	return pass;
}

void DRW_pass_free(DRWPass *pass)
{
	for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
		DRW_shgroup_free(shgroup);
	}

	glDeleteQueries(2, pass->timer_queries);
	BLI_freelistN(&pass->shgroups);
}

void DRW_pass_foreach_shgroup(DRWPass *pass, void (*callback)(void *userData, DRWShadingGroup *shgrp), void *userData)
{
	for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
		callback(userData, shgroup);
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
		        DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER,
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
		if ((test = CHANGED_TO(DRW_STATE_BLEND))) {
			if (test == 1) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			else {
				glDisable(GL_BLEND);
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

static void draw_geometry(DRWShadingGroup *shgroup, Batch *geom, const float (*obmat)[4])
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;
	DRWInterface *interface = shgroup->interface;

	float mvp[4][4], mv[4][4], n[3][3], wn[3][3];
	float eye[3] = { 0.0f, 0.0f, 1.0f }; /* looking into the screen */

	bool do_mvp = (interface->modelviewprojection != -1);
	bool do_mv = (interface->modelview != -1);
	bool do_n = (interface->normal != -1);
	bool do_wn = (interface->worldnormal != -1);
	bool do_eye = (interface->eye != -1);

	if (do_mvp) {
		mul_m4_m4m4(mvp, rv3d->persmat, obmat);
	}
	if (do_mv || do_n || do_eye) {
		mul_m4_m4m4(mv, rv3d->viewmat, obmat);
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

	/* Should be really simple */
	/* step 1 : bind object dependent matrices */
	if (interface->model != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->model, 16, 1, (float *)obmat);
	}
	if (interface->modelviewprojection != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->modelviewprojection, 16, 1, (float *)mvp);
	}
	if (interface->viewinverse != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->viewinverse, 16, 1, (float *)rv3d->viewinv);
	}
	if (interface->viewprojection != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->viewprojection, 16, 1, (float *)rv3d->persmat);
	}
	if (interface->projection != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->projection, 16, 1, (float *)rv3d->winmat);
	}
	if (interface->view != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->view, 16, 1, (float *)rv3d->viewmat);
	}
	if (interface->modelview != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->modelview, 16, 1, (float *)mv);
	}
	if (interface->normal != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->normal, 9, 1, (float *)n);
	}
	if (interface->worldnormal != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->worldnormal, 9, 1, (float *)wn);
	}
	if (interface->eye != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->eye, 3, 1, (float *)eye);
	}

	/* step 2 : bind vertex array & draw */
	Batch_set_program(geom, GPU_shader_get_program(shgroup->shader), GPU_shader_get_interface(shgroup->shader));
	if (interface->instance_vbo) {
		Batch_draw_stupid_instanced(geom, interface->instance_vbo, interface->instance_count, interface->attribs_count,
		                            interface->attribs_stride, interface->attribs_size, interface->attribs_loc);
	}
	else {
		Batch_draw_stupid(geom);
	}
}

static void draw_shgroup(DRWShadingGroup *shgroup, DRWState pass_state)
{
	BLI_assert(shgroup->shader);
	BLI_assert(shgroup->interface);

	DRWInterface *interface = shgroup->interface;
	GPUTexture *tex;
	int val;

	if (DST.shader != shgroup->shader) {
		if (DST.shader) GPU_shader_unbind();
		GPU_shader_bind(shgroup->shader);
		DST.shader = shgroup->shader;
	}

	if (shgroup->type != DRW_SHG_NORMAL) {
		shgroup_dynamic_batch_from_calls(shgroup);
	}

	DRW_state_set(pass_state | shgroup->state_extra);

	/* Binding Uniform */
	/* Don't check anything, Interface should already contain the least uniform as possible */
	for (DRWUniform *uni = interface->uniforms.first; uni; uni = uni->next) {
		DRWBoundTexture *bound_tex;

		switch (uni->type) {
			case DRW_UNIFORM_SHORT:
				val = (int)*((short *)uni->value);
				GPU_shader_uniform_vector_int(
				        shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)&val);
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
				GPU_texture_bind(tex, uni->bindloc);

				bound_tex = MEM_callocN(sizeof(DRWBoundTexture), "DRWBoundTexture");
				bound_tex->tex = tex;
				BLI_addtail(&DST.bound_texs, bound_tex);

				GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
				break;
			case DRW_UNIFORM_BUFFER:
				if (!DRW_state_is_fbo()) {
					break;
				}
				tex = *((GPUTexture **)uni->value);
				GPU_texture_bind(tex, uni->bindloc);
				GPU_texture_compare_mode(tex, false);
				GPU_texture_filter_mode(tex, false);

				bound_tex = MEM_callocN(sizeof(DRWBoundTexture), "DRWBoundTexture");
				bound_tex->tex = tex;
				BLI_addtail(&DST.bound_texs, bound_tex);

				GPU_shader_uniform_texture(shgroup->shader, uni->location, tex);
				break;
			case DRW_UNIFORM_BLOCK:
				GPU_uniformbuffer_bind((GPUUniformBuffer *)uni->value, uni->bindloc);
				GPU_shader_uniform_buffer(shgroup->shader, uni->location, (GPUUniformBuffer *)uni->value);
				break;
		}
	}

#ifdef USE_GPU_SELECT
	/* use the first item because of selection we only ever add one */
#  define GPU_SELECT_LOAD_IF_PICKSEL(_call) \
	if ((G.f & G_PICKSEL) && (_call)) { \
		GPU_select_load_id((_call)->select_id); \
	} ((void)0)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(_call_ls) \
	if ((G.f & G_PICKSEL) && (_call_ls)->first) { \
		BLI_assert(BLI_listbase_is_single(_call_ls)); \
		GPU_select_load_id(((DRWCall *)(_call_ls)->first)->select_id); \
	} ((void)0)
#else
#  define GPU_SELECT_LOAD_IF_PICKSEL(call)
#  define GPU_SELECT_LOAD_IF_PICKSEL_LIST(call)
#endif

	/* Rendering Calls */
	if (shgroup->type != DRW_SHG_NORMAL) {
		/* Replacing multiple calls with only one */
		float obmat[4][4];
		unit_m4(obmat);

		if (shgroup->type == DRW_SHG_INSTANCE && interface->instance_count > 0) {
			GPU_SELECT_LOAD_IF_PICKSEL_LIST(&shgroup->calls);
			draw_geometry(shgroup, shgroup->instance_geom, obmat);
		}
		else {
			/* Some dynamic batch can have no geom (no call to aggregate) */
			if (shgroup->batch_geom) {
				GPU_SELECT_LOAD_IF_PICKSEL_LIST(&shgroup->calls);
				draw_geometry(shgroup, shgroup->batch_geom, obmat);
			}
		}
	}
	else {
		for (DRWCall *call = shgroup->calls.first; call; call = call->next) {
			GPU_SELECT_LOAD_IF_PICKSEL(call);
			draw_geometry(shgroup, call->geometry, call->obmat);
		}
	}

	DRW_state_reset();
}

void DRW_draw_pass(DRWPass *pass)
{
	/* Start fresh */
	DST.shader = NULL;
	DST.tex_bind_id = 0;

	DRW_state_set(pass->state);
	BLI_listbase_clear(&DST.bound_texs);

	pass->wasdrawn = true;

	/* Init Timer queries */
	if (pass->timer_queries[0] == 0) {
		pass->front_idx = 0;
		pass->back_idx = 1;

		glGenQueries(2, pass->timer_queries);

		/* dummy query, avoid gl error */
		glBeginQuery(GL_TIME_ELAPSED, pass->timer_queries[pass->front_idx]);
		glEndQuery(GL_TIME_ELAPSED);
	}
	else {
		/* swap indices */
		unsigned int tmp = pass->back_idx;
		pass->back_idx = pass->front_idx;
		pass->front_idx = tmp;
	}

	/* issue query for the next frame */
	glBeginQuery(GL_TIME_ELAPSED, pass->timer_queries[pass->back_idx]);

	for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
		draw_shgroup(shgroup, pass->state);
	}

	/* Clear Bound textures */
	for (DRWBoundTexture *bound_tex = DST.bound_texs.first; bound_tex; bound_tex = bound_tex->next) {
		GPU_texture_unbind(bound_tex->tex);
	}
	DST.tex_bind_id = 0;
	BLI_freelistN(&DST.bound_texs);

	if (DST.shader) {
		GPU_shader_unbind();
		DST.shader = NULL;
	}

	glEndQuery(GL_TIME_ELAPSED);
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
	DRW_state_reset_ex(DRW_STATE_DEFAULT);
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

bool DRW_is_object_renderable(Object *ob)
{
	Scene *scene = DST.draw_ctx.scene;
	Object *obedit = scene->obedit;

	if (ob->type == OB_MESH) {
		if (ob == obedit) {
			IDProperty *props = BKE_object_collection_engine_get(ob, COLLECTION_MODE_EDIT, "");
			bool do_occlude_wire = BKE_collection_engine_property_value_get_bool(props, "show_occlude_wire");

			if (do_occlude_wire)
				return false;
		}
	}

	return true;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Framebuffers (DRW_framebuffer)
 * \{ */

static GPUTextureFormat convert_tex_format(int fbo_format, int *channels, bool *is_depth)
{
	*is_depth = ELEM(fbo_format, DRW_BUF_DEPTH_16, DRW_BUF_DEPTH_24);

	switch (fbo_format) {
		case DRW_BUF_RG_16:    *channels = 2; return GPU_RG16F;
		case DRW_BUF_RGBA_8:   *channels = 4; return GPU_RGBA8;
		case DRW_BUF_RGBA_16:  *channels = 4; return GPU_RGBA16F;
		case DRW_BUF_DEPTH_24: *channels = 1; return GPU_DEPTH_COMPONENT24;
		default:
			BLI_assert(false);
			*channels = 4; return GPU_RGBA8;
	}
}

void DRW_framebuffer_init(
        struct GPUFrameBuffer **fb, int width, int height,
        DRWFboTexture textures[MAX_FBO_TEX], int textures_len)
{
	BLI_assert(textures_len <= MAX_FBO_TEX);

	if (!*fb) {
		int color_attachment = -1;
		*fb = GPU_framebuffer_create();

		for (int i = 0; i < textures_len; ++i) {
			int channels;
			bool is_depth;

			DRWFboTexture fbotex = textures[i];
			GPUTextureFormat gpu_format = convert_tex_format(fbotex.format, &channels, &is_depth);

			if (!*fbotex.tex) {
				*fbotex.tex = GPU_texture_create_2D_custom(width, height, channels, gpu_format, NULL, NULL);
				drw_texture_set_parameters(*fbotex.tex, fbotex.flag);
			}

			if (!is_depth) {
				++color_attachment;
			}

			GPU_framebuffer_texture_attach(*fb, *fbotex.tex, color_attachment, 0);
		}

		if (!GPU_framebuffer_check_valid(*fb, NULL)) {
			printf("Error invalid framebuffer\n");
		}

		GPU_framebuffer_bind(DST.default_framebuffer);
	}
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

void DRW_framebuffer_texture_detach(GPUTexture *tex)
{
	GPU_framebuffer_texture_detach(tex);
}

void DRW_framebuffer_blit(struct GPUFrameBuffer *fb_read, struct GPUFrameBuffer *fb_write, bool depth)
{
	GPU_framebuffer_blit(fb_read, 0, fb_write, 0, depth);
}

void DRW_framebuffer_viewport_size(struct GPUFrameBuffer *UNUSED(fb_read), int w, int h)
{
	glViewport(0, 0, w, h);
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
	}
	else {
		DST.size[0] = 0;
		DST.size[1] = 0;

		DST.default_framebuffer = NULL;
	}
	/* Refresh DST.screenvecs */
	copy_v3_v3(DST.screenvecs[0], rv3d->viewinv[0]);
	copy_v3_v3(DST.screenvecs[1], rv3d->viewinv[1]);
	normalize_v3(DST.screenvecs[0]);
	normalize_v3(DST.screenvecs[1]);

	/* Refresh DST.pixelsize */
	DST.pixsize = rv3d->pixsize;
}

void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type)
{
	RegionView3D *rv3d = DST.draw_ctx.rv3d;

	switch (type) {
		case DRW_MAT_PERS:
			copy_m4_m4(mat, rv3d->persmat);
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
		default:
			BLI_assert(!"Matrix type invalid");
			break;
	}
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

/** \} */


/* -------------------------------------------------------------------- */

/** \name Objects (DRW_object)
 * \{ */

typedef struct ObjectEngineData {
	struct ObjectEngineData *next, *prev;
	DrawEngineType *engine_type;
	void *storage;
} ObjectEngineData;

void **DRW_object_engine_data_get(Object *ob, DrawEngineType *engine_type)
{
	ObjectEngineData *oed;

	for (oed = ob->drawdata.first; oed; oed = oed->next) {
		if (oed->engine_type == engine_type) {
			return &oed->storage;
		}
	}

	oed = MEM_callocN(sizeof(ObjectEngineData), "ObjectEngineData");
	oed->engine_type = engine_type;
	BLI_addtail(&ob->drawdata, oed);

	return &oed->storage;
}

void DRW_object_engine_data_free(Object *ob)
{
	for (ObjectEngineData *oed = ob->drawdata.first; oed; oed = oed->next) {
		if (oed->storage) {
			MEM_freeN(oed->storage);
		}
	}

	BLI_freelistN(&ob->drawdata);
}

LampEngineData *DRW_lamp_engine_data_get(Object *ob, RenderEngineType *engine_type)
{
	BLI_assert(ob->type == OB_LAMP);

	Scene *scene = DST.draw_ctx.scene;

	/* TODO Dupliobjects */
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

#define TIMER_FALLOFF 0.1f

static void DRW_engines_init(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		double stime = PIL_check_seconds_timer();

		if (engine->engine_init) {
			engine->engine_init(data);
		}

		double ftime = (PIL_check_seconds_timer() - stime) * 1e3;
		data->init_time = data->init_time * (1.0f - TIMER_FALLOFF) + ftime * TIMER_FALLOFF; /* exp average */
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

		double stime = PIL_check_seconds_timer();
		data->cache_time = 0.0;

		if (engine->cache_init) {
			engine->cache_init(data);
		}

		data->cache_time += (PIL_check_seconds_timer() - stime) * 1e3;
	}
}

static void DRW_engines_cache_populate(Object *ob)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		double stime = PIL_check_seconds_timer();

		if (engine->cache_populate) {
			engine->cache_populate(data, ob);
		}

		data->cache_time += (PIL_check_seconds_timer() - stime) * 1e3;
	}
}

static void DRW_engines_cache_finish(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		double stime = PIL_check_seconds_timer();

		if (engine->cache_finish) {
			engine->cache_finish(data);
		}

		data->cache_time += (PIL_check_seconds_timer() - stime) * 1e3;
	}
}

static void DRW_engines_draw_background(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		double stime = PIL_check_seconds_timer();

		if (engine->draw_background) {
			engine->draw_background(data);
			return;
		}

		double ftime = (PIL_check_seconds_timer() - stime) * 1e3;
		data->background_time = data->background_time * (1.0f - TIMER_FALLOFF) + ftime * TIMER_FALLOFF; /* exp average */
	}

	/* No draw_background found, doing default background */
	DRW_draw_background();
}

static void DRW_engines_draw_scene(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		double stime = PIL_check_seconds_timer();

		if (engine->draw_scene) {
			engine->draw_scene(data);
		}

		double ftime = (PIL_check_seconds_timer() - stime) * 1e3;
		data->render_time = data->render_time * (1.0f - TIMER_FALLOFF) + ftime * TIMER_FALLOFF; /* exp average */
	}
}

static void DRW_engines_draw_text(void)
{
	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		double stime = PIL_check_seconds_timer();

		if (data->text_draw_cache) {
			DRW_text_cache_draw(data->text_draw_cache, DST.draw_ctx.v3d, DST.draw_ctx.ar, false);
		}

		double ftime = (PIL_check_seconds_timer() - stime) * 1e3;
		data->render_time = data->render_time * (1.0f - TIMER_FALLOFF) + ftime * TIMER_FALLOFF; /* exp average */
	}
}

static void use_drw_engine(DrawEngineType *engine)
{
	LinkData *ld = MEM_callocN(sizeof(LinkData), "enabled engine link data");
	ld->data = engine;
	BLI_addtail(&DST.enabled_engines, ld);
}

/* TODO revisit this when proper layering is implemented */
/* Gather all draw engines needed and store them in DST.enabled_engines
 * That also define the rendering order of engines */
static void DRW_engines_enable_from_engine(const Scene *scene)
{
	/* TODO layers */
	RenderEngineType *type = RE_engines_find(scene->r.engine);
	if (type->draw_engine != NULL) {
		use_drw_engine(type->draw_engine);
	}

	if ((type->flag & RE_INTERNAL) == 0) {
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

static void DRW_engines_enable(const Scene *scene, SceneLayer *sl)
{
	const int mode = CTX_data_mode_enum_ex(scene->obedit, OBACT_NEW);
	DRW_engines_enable_from_engine(scene);
	DRW_engines_enable_from_object_mode();
	DRW_engines_enable_from_mode(mode);
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
	                       rect->ymax - (3 + v++) * U.widget_unit, 0.0f,
	                       txt, size);
}

/* CPU stats */
static void DRW_debug_cpu_stats(void)
{
	int u, v;
	double cache_tot_time = 0.0, init_tot_time = 0.0, background_tot_time = 0.0, render_tot_time = 0.0, tot_time = 0.0;
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
	sprintf(col_label, "Cache");
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

		cache_tot_time += data->cache_time;
		sprintf(time_to_txt, "%.2fms", data->cache_time);
		draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));

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
	sprintf(time_to_txt, "%.2fms", cache_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", init_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", background_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", render_tot_time);
	draw_stat(&rect, u++, v, time_to_txt, sizeof(time_to_txt));
	sprintf(time_to_txt, "%.2fms", tot_time);
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

	char time_to_txt[16];
	char pass_name[MAX_PASS_NAME + 8];
	int v = BLI_listbase_count(&DST.enabled_engines) + 3;
	GLuint64 tot_time = 0;

	for (LinkData *link = DST.enabled_engines.first; link; link = link->next) {
		GLuint64 engine_time = 0;
		DrawEngineType *engine = link->data;
		ViewportEngineData *data = DRW_viewport_engine_data_get(engine);
		int vsta = v;

		draw_stat(&rect, 0, v, engine->idname, sizeof(engine->idname));
		v++;

		for (int i = 0; i < engine->vedata_size->psl_len; ++i) {
			DRWPass *pass = data->psl->passes[i];
			if (pass != NULL) {
				GLuint64 time;
				glGetQueryObjectui64v(pass->timer_queries[pass->front_idx], GL_QUERY_RESULT, &time);
				tot_time += time;
				engine_time += time;

				sprintf(pass_name, "   |--> %s", pass->name);
				draw_stat(&rect, 0, v, pass_name, sizeof(pass_name));

				if (pass->wasdrawn)
					sprintf(time_to_txt, "%.2fms", time / 1000000.0);
				else
					sprintf(time_to_txt, "Not drawn");
				draw_stat(&rect, 2, v++, time_to_txt, sizeof(time_to_txt));

				pass->wasdrawn = false;
			}
		}
		/* engine total time */
		sprintf(time_to_txt, "%.2fms", engine_time / 1000000.0);
		draw_stat(&rect, 2, vsta, time_to_txt, sizeof(time_to_txt));
		v++;
	}

	sprintf(pass_name, "Total GPU time %.2fms (%.1f fps)", tot_time / 1000000.0, 1000000000.0 / tot_time);
	draw_stat(&rect, 0, v, pass_name, sizeof(pass_name));
}


/* -------------------------------------------------------------------- */

/** \name Main Draw Loops (DRW_draw)
 * \{ */

/* Everything starts here.
 * This function takes care of calling all cache and rendering functions
 * for each relevant engine / mode engine. */
void DRW_draw_view(const bContext *C)
{
	struct Depsgraph *graph = CTX_data_depsgraph(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = CTX_wm_view3d(C);

	DST.draw_ctx.evil_C = C;

	DRW_draw_render_loop(graph, ar, v3d);
}

/**
 * Used for both regular and off-screen drawing.
 */
void DRW_draw_render_loop(
        struct Depsgraph *graph,
        ARegion *ar, View3D *v3d)
{
	Scene *scene = DAG_get_scene(graph);
	SceneLayer *sl = DAG_get_scene_layer(graph);
	RegionView3D *rv3d = ar->regiondata;

	bool cache_is_dirty;
	DST.viewport = rv3d->viewport;
	v3d->zbuf = true;

	/* Get list of enabled engines */
	DRW_engines_enable(scene, sl);

	/* Setup viewport */
	cache_is_dirty = GPU_viewport_cache_validate(DST.viewport, DRW_engines_get_hash());

	DST.draw_ctx = (DRWContextState){
		ar, rv3d, v3d, scene, sl,
		/* reuse if caller sets */
		DST.draw_ctx.evil_C,
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

		DEG_OBJECT_ITER(graph, ob);
		{
			DRW_engines_cache_populate(ob);
		}
		DEG_OBJECT_ITER_END

		DRW_engines_cache_finish();
	}

	/* Start Drawing */
	DRW_state_reset();
	DRW_engines_draw_background();

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

	/* needed so manipulator isn't obscured */
	glClear(GL_DEPTH_BUFFER_BIT);

	if (DST.draw_ctx.evil_C) {
		DRW_draw_manipulator();

		DRW_draw_region_info();
	}

	if (G.debug_value > 20) {
		DRW_debug_cpu_stats();
		DRW_debug_gpu_stats();
	}

	DRW_state_reset();
	DRW_engines_disable();

	/* avoid accidental reuse */
	memset(&DST, 0x0, sizeof(DST));
}

void DRW_draw_render_loop_offscreen(
        struct Depsgraph *graph,
        ARegion *ar, View3D *v3d, GPUOffScreen *ofs)
{
	RegionView3D *rv3d = ar->regiondata;

	/* backup */
	void *backup_viewport = rv3d->viewport;
	{
		/* backup (_never_ use rv3d->viewport) */
		rv3d->viewport = GPU_viewport_create_from_offscreen(ofs);
	}

	DST.draw_ctx.evil_C = NULL;

	DRW_draw_render_loop(graph, ar, v3d);

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
	Scene *scene = DAG_get_scene(graph);
	SceneLayer *sl = DAG_get_scene_layer(graph);
#ifndef USE_GPU_SELECT
	UNUSED_VARS(vc, scene, sl, v3d, ar, rect);
#else
	RegionView3D *rv3d = ar->regiondata;

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
		ar, rv3d, v3d, scene, sl, (bContext *)NULL,
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
			DEG_OBJECT_ITER(graph, ob)
			{
				if ((ob->base_flag & BASE_SELECTABLED) != 0) {
					DRW_select_load_id(ob->base_selection_color);
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

	/* avoid accidental reuse */
	memset(&DST, 0x0, sizeof(DST));

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
	Scene *scene = DAG_get_scene(graph);
	SceneLayer *sl = DAG_get_scene_layer(graph);
	RegionView3D *rv3d = ar->regiondata;

	/* backup (_never_ use rv3d->viewport) */
	void *backup_viewport = rv3d->viewport;
	rv3d->viewport = NULL;

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
		ar, rv3d, v3d, scene, sl, (bContext *)NULL,
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

		DEG_OBJECT_ITER(graph, ob)
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

	/* avoid accidental reuse */
	memset(&DST, 0x0, sizeof(DST));

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
 * Should text draw in this mode?
 */
bool DRW_state_show_text(void)
{
	return (DST.options.is_select) == 0 &&
	       (DST.options.is_depth) == 0;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Context State (DRW_context_state)
 * \{ */

void DRW_context_state_init(const bContext *C, DRWContextState *r_draw_ctx)
{
	r_draw_ctx->ar = CTX_wm_region(C);
	r_draw_ctx->rv3d = CTX_wm_region_view3d(C);
	r_draw_ctx->v3d = CTX_wm_view3d(C);

	r_draw_ctx->scene = CTX_data_scene(C);
	r_draw_ctx->sl = CTX_data_scene_layer(C);

	/* grr, cant avoid! */
	r_draw_ctx->evil_C = C;
}


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

		BKE_curve_batch_cache_dirty_cb = DRW_curve_batch_cache_dirty;
		BKE_curve_batch_cache_free_cb = DRW_curve_batch_cache_free;

		BKE_mesh_batch_cache_dirty_cb = DRW_mesh_batch_cache_dirty;
		BKE_mesh_batch_cache_free_cb = DRW_mesh_batch_cache_free;

		BKE_lattice_batch_cache_dirty_cb = DRW_lattice_batch_cache_dirty;
		BKE_lattice_batch_cache_free_cb = DRW_lattice_batch_cache_free;
	}
}

extern struct GPUUniformBuffer *globals_ubo; /* draw_common.c */
void DRW_engines_free(void)
{
	DRW_shape_cache_free();

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

#ifdef WITH_CLAY_ENGINE
	BLI_remlink(&R_engines, &DRW_engine_viewport_clay_type);
#endif
}

/** \} */

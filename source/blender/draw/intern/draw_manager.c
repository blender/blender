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

#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"

#include "BIF_glutil.h"

#include "BKE_global.h"

#include "BLT_translation.h"

#include "DRW_engine.h"
#include "DRW_render.h"

#include "DNA_view3d_types.h"

#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"

#include "RE_engine.h"

#include "UI_resources.h"

#include "object_mode.h"
#include "edit_armature_mode.h"
#include "edit_mesh_mode.h"
#include "clay.h"

#define MAX_ATTRIB_NAME 32

extern char datatoc_gpu_shader_2D_vert_glsl[];
extern char datatoc_gpu_shader_3D_vert_glsl[];
extern char datatoc_gpu_shader_basic_vert_glsl[];

/* Structures */
typedef enum {
	DRW_UNIFORM_BOOL,
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
	int modelview;
	int projection;
	int view;
	int modelviewprojection;
	int viewprojection;
	int normal;
	int eye;
	/* Dynamic batch */
	GLuint instance_vbo;
	int instance_count;
	VertexFormat vbo_format;
};

struct DRWPass {
	ListBase shgroups; /* DRWShadingGroup */
	DRWState state;
	float state_param; /* Line / Point width */
};

typedef struct DRWCall {
	struct DRWCall *next, *prev;
	Batch *geometry;
	float (*obmat)[4];
} DRWCall;

typedef struct DRWDynamicCall {
	struct DRWDynamicCall *next, *prev;
	const void *data[];
} DRWDynamicCall;

struct DRWShadingGroup {
	struct DRWShadingGroup *next, *prev;

	struct GPUShader *shader;        /* Shader to bind */
	struct DRWInterface *interface;  /* Uniforms pointers */
	ListBase calls;                  /* DRWCall or DRWDynamicCall depending of type*/
	int state;                       /* State changes for this batch only */
	int type;

	Batch *instance_geom;  /* Geometry to instance */
	Batch *batch_geom;     /* Result of call batching */
};

/* Used by DRWShadingGroup.type */
enum {
	DRW_SHG_NORMAL,
	DRW_SHG_POINT_BATCH,
	DRW_SHG_LINE_BATCH,
	DRW_SHG_INSTANCE,
};

/* Render State */
static struct DRWGlobalState {
	GPUShader *shader;
	struct GPUFrameBuffer *default_framebuffer;
	FramebufferList *fbl, *fbl_mode;
	TextureList *txl, *txl_mode;
	PassList *psl, *psl_mode;
	StorageList *stl, *stl_mode;
	ListBase bound_texs;
	int tex_bind_id;
	float size[2];
	float screenvecs[2][3];
	float pixsize;
	/* Current rendering context set by DRW_viewport_init */
	const struct bContext *context;
} DST = {NULL};

/* ***************************************** TEXTURES ******************************************/
static void drw_texture_get_format(DRWTextureFormat format, GPUTextureFormat *data_type, int *channels)
{
	switch (format) {
		case DRW_TEX_RGBA_8: *data_type = GPU_RGBA8; break;
		case DRW_TEX_RGBA_16: *data_type = GPU_RGBA16F; break;
		case DRW_TEX_RG_16: *data_type = GPU_RG16F; break;
		case DRW_TEX_RG_32: *data_type = GPU_RG32F; break;
		case DRW_TEX_R_8: *data_type = GPU_R8; break;
#if 0
		case DRW_TEX_RGBA_32: *data_type = GPU_RGBA32F; break;
		case DRW_TEX_RGB_8: *data_type = GPU_RGB8; break;
		case DRW_TEX_RGB_16: *data_type = GPU_RGB16F; break;
		case DRW_TEX_RGB_32: *data_type = GPU_RGB32F; break;
		case DRW_TEX_RG_8: *data_type = GPU_RG8; break;
		case DRW_TEX_R_16: *data_type = GPU_R16F; break;
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

/* TODO make use of format */
GPUTexture *DRW_texture_create_2D_array(int w, int h, int d, DRWTextureFormat UNUSED(format), DRWTextureFlag flags, const float *fpixels)
{
	GPUTexture *tex;

	tex = GPU_texture_create_2D_array(w, h, d, fpixels, NULL);
	drw_texture_set_parameters(tex, flags);

	return tex;
}

void DRW_texture_free(GPUTexture *tex)
{
	GPU_texture_free(tex);
}


/* ************************************ UNIFORM BUFFER OBJECT **********************************/

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

/* ****************************************** SHADERS ******************************************/

GPUShader *DRW_shader_create(const char *vert, const char *geom, const char *frag, const char *defines)
{
	return GPU_shader_create(vert, frag, geom, NULL, defines, 0, 0, 0);
}

GPUShader *DRW_shader_create_2D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_2D_vert_glsl, frag, NULL, NULL, defines, 0, 0, 0);
}

GPUShader *DRW_shader_create_3D(const char *frag, const char *defines)
{
	return GPU_shader_create(datatoc_gpu_shader_3D_vert_glsl, frag, NULL, NULL, defines, 0, 0, 0);
}

GPUShader *DRW_shader_create_3D_depth_only(void)
{
	return GPU_shader_get_builtin_shader(GPU_SHADER_3D_DEPTH_ONLY);
}

void DRW_shader_free(GPUShader *shader)
{
	GPU_shader_free(shader);
}

/* ***************************************** INTERFACE ******************************************/

static DRWInterface *DRW_interface_create(GPUShader *shader)
{
	DRWInterface *interface = MEM_mallocN(sizeof(DRWInterface), "DRWInterface");

	interface->modelview = GPU_shader_get_uniform(shader, "ModelViewMatrix");
	interface->projection = GPU_shader_get_uniform(shader, "ProjectionMatrix");
	interface->view = GPU_shader_get_uniform(shader, "ViewMatrix");
	interface->viewprojection = GPU_shader_get_uniform(shader, "ViewProjectionMatrix");
	interface->modelviewprojection = GPU_shader_get_uniform(shader, "ModelViewProjectionMatrix");
	interface->normal = GPU_shader_get_uniform(shader, "NormalMatrix");
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

void DRW_get_dfdy_factors(float dfdyfac[2])
{
	GPU_get_dfdy_factors(dfdyfac);
}

/* ***************************************** SHADING GROUP ******************************************/

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWShadingGroup *shgroup = MEM_mallocN(sizeof(DRWShadingGroup), "DRWShadingGroup");

	shgroup->type = DRW_SHG_NORMAL;
	shgroup->shader = shader;
	shgroup->interface = DRW_interface_create(shader);
	shgroup->state = 0;
	shgroup->batch_geom = NULL;
	shgroup->instance_geom = NULL;

	BLI_addtail(&pass->shgroups, shgroup);
	BLI_listbase_clear(&shgroup->calls);

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

	if (shgroup->batch_geom) {
		Batch_discard_all(shgroup->batch_geom);
	}
}

void DRW_shgroup_call_add(DRWShadingGroup *shgroup, Batch *geom, float (*obmat)[4])
{
	BLI_assert(geom != NULL);

	DRWCall *call = MEM_callocN(sizeof(DRWCall), "DRWCall");

	call->obmat = obmat;
	call->geometry = geom;

	BLI_addtail(&shgroup->calls, call);
}

void DRW_shgroup_dynamic_call_add(DRWShadingGroup *shgroup, ...)
{
	va_list params;
	int i;
	DRWInterface *interface = shgroup->interface;
	int size = sizeof(ListBase) + sizeof(void *) * interface->attribs_count;

	DRWDynamicCall *call = MEM_callocN(size, "DRWDynamicCall");

	va_start(params, shgroup);
	for (i = 0; i < interface->attribs_count; ++i) {
		call->data[i] = va_arg(params, void *);
	}
	va_end(params);

	interface->instance_count += 1;

	BLI_addtail(&shgroup->calls, call);
}

/* Make sure you know what you do when using this,
 * State is not revert back at the end of the shgroup */
void DRW_shgroup_state_set(DRWShadingGroup *shgroup, DRWState state)
{
	shgroup->state = state;
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

/* Creates a VBO containing OGL primitives for all DRWDynamicCall */
static void shgroup_dynamic_batch(DRWShadingGroup *shgroup)
{
	DRWInterface *interface = shgroup->interface;
	int nbr = interface->instance_count;

	GLenum type = (shgroup->type == DRW_SHG_POINT_BATCH) ? GL_POINTS : GL_LINES;

	if (nbr == 0)
		return;

	/* Upload Data */
	if (interface->vbo_format.attrib_ct == 0) {
		for (DRWAttrib *attrib = interface->attribs.first; attrib; attrib = attrib->next) {
			BLI_assert(attrib->size <= 4); /* matrices have no place here for now */
			if (attrib->type == DRW_ATTRIB_FLOAT) {
				attrib->format_id = add_attrib(&interface->vbo_format, attrib->name, GL_FLOAT, attrib->size, KEEP_FLOAT);
			}
			else if (attrib->type == DRW_ATTRIB_INT) {
				attrib->format_id = add_attrib(&interface->vbo_format, attrib->name, GL_BYTE, attrib->size, KEEP_INT);
			}
			else {
				BLI_assert(false);
			}
		}
	}

	VertexBuffer *vbo = VertexBuffer_create_with_format(&interface->vbo_format);
	VertexBuffer_allocate_data(vbo, nbr);

	int j = 0;
	for (DRWDynamicCall *call = shgroup->calls.first; call; call = call->next, j++) {
		int i = 0;
		for (DRWAttrib *attrib = interface->attribs.first; attrib; attrib = attrib->next, i++) {
			setAttrib(vbo, attrib->format_id, j, call->data[i]);
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

	for (DRWDynamicCall *call = shgroup->calls.first; call; call = call->next) {
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

/* ***************************************** PASSES ******************************************/

DRWPass *DRW_pass_create(const char *name, DRWState state)
{
	DRWPass *pass = MEM_callocN(sizeof(DRWPass), name);
	pass->state = state;

	BLI_listbase_clear(&pass->shgroups);

	return pass;
}

void DRW_pass_free(DRWPass *pass)
{
	for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
		DRW_shgroup_free(shgroup);
	}
	BLI_freelistN(&pass->shgroups);
}

/* ****************************************** DRAW ******************************************/

#ifdef WITH_CLAY_ENGINE
/* Only alter the state (does not reset it like set_state() ) */
static void shgroup_set_state(DRWShadingGroup *shgroup)
{
	if (shgroup->state) {
		/* Blend */
		if (shgroup->state & DRW_STATE_BLEND) {
			glEnable(GL_BLEND);
		}

		/* Wire width */
		if (shgroup->state & DRW_STATE_WIRE) {
			glLineWidth(1.0f);
		}
		else if (shgroup->state & DRW_STATE_WIRE_LARGE) {
			glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
		}

		/* Line Stipple */
		if (shgroup->state & DRW_STATE_STIPPLE_2) {
			setlinestyle(2);
		}
		else if (shgroup->state & DRW_STATE_STIPPLE_3) {
			setlinestyle(3);
		}
		else if (shgroup->state & DRW_STATE_STIPPLE_4) {
			setlinestyle(4);
		}

		if (shgroup->state & DRW_STATE_POINT) {
			GPU_enable_program_point_size();
			glPointSize(5.0f);
		}
	}
}

typedef struct DRWBoundTexture {
	struct DRWBoundTexture *next, *prev;
	GPUTexture *tex;
} DRWBoundTexture;

static void draw_geometry(DRWShadingGroup *shgroup, Batch *geom, const float (*obmat)[4])
{
	RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);
	DRWInterface *interface = shgroup->interface;
	
	float mvp[4][4], mv[4][4], n[3][3];
	float eye[3] = { 0.0f, 0.0f, 1.0f }; /* looking into the screen */

	bool do_mvp = (interface->modelviewprojection != -1);
	bool do_mv = (interface->modelview != -1);
	bool do_n = (interface->normal != -1);
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
	if (do_eye) {
		/* Used by orthographic wires */
		float tmp[3][3];
		invert_m3_m3(tmp, n);
		/* set eye vector, transformed to object coords */
		mul_m3_v3(tmp, eye);
	}

	/* Should be really simple */
	/* step 1 : bind object dependent matrices */
	if (interface->modelviewprojection != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->modelviewprojection, 16, 1, (float *)mvp);
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
	if (interface->eye != -1) {
		GPU_shader_uniform_vector(shgroup->shader, interface->eye, 3, 1, (float *)eye);
	}

	/* step 2 : bind vertex array & draw */
	Batch_set_program(geom, GPU_shader_get_program(shgroup->shader));
	if (interface->instance_vbo) {
		Batch_draw_stupid_instanced(geom, interface->instance_vbo, interface->instance_count, interface->attribs_count,
		                            interface->attribs_stride, interface->attribs_size, interface->attribs_loc);
	}
	else {
		Batch_draw_stupid(geom);
	}
}

static void draw_shgroup(DRWShadingGroup *shgroup)
{
	BLI_assert(shgroup->shader);
	BLI_assert(shgroup->interface);

	DRWInterface *interface = shgroup->interface;
	GPUTexture *tex;

	if (DST.shader != shgroup->shader) {
		if (DST.shader) GPU_shader_unbind();
		GPU_shader_bind(shgroup->shader);
		DST.shader = shgroup->shader;
	}

	if (shgroup->type != DRW_SHG_NORMAL) {
		shgroup_dynamic_batch_from_calls(shgroup);
	}

	shgroup_set_state(shgroup);

	/* Binding Uniform */
	/* Don't check anything, Interface should already contain the least uniform as possible */
	for (DRWUniform *uni = interface->uniforms.first; uni; uni = uni->next) {
		DRWBoundTexture *bound_tex;

		switch (uni->type) {
			case DRW_UNIFORM_BOOL:
			case DRW_UNIFORM_INT:
				GPU_shader_uniform_vector_int(shgroup->shader, uni->location, uni->length, uni->arraysize, (int *)uni->value);
				break;
			case DRW_UNIFORM_FLOAT:
			case DRW_UNIFORM_MAT3:
			case DRW_UNIFORM_MAT4:
				GPU_shader_uniform_vector(shgroup->shader, uni->location, uni->length, uni->arraysize, (float *)uni->value);
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

	/* Rendering Calls */
	if (shgroup->type != DRW_SHG_NORMAL) {
		/* Replacing multiple calls with only one */
		float obmat[4][4];
		unit_m4(obmat);

		if (shgroup->type == DRW_SHG_INSTANCE && interface->instance_count > 0) {
			draw_geometry(shgroup, shgroup->instance_geom, obmat);
		}
		else {
			/* Some dynamic batch can have no geom (no call to aggregate) */
			if (shgroup->batch_geom) {
				draw_geometry(shgroup, shgroup->batch_geom, obmat);
			}
		}
	}
	else {
		for (DRWCall *call = shgroup->calls.first; call; call = call->next) {
			draw_geometry(shgroup, call->geometry, call->obmat);
		}
	}
}

static void set_state(short flag)
{
	/* TODO Keep track of the state and only revert what is needed */

	/* Depth Write */
	if (flag & DRW_STATE_WRITE_DEPTH)
		glDepthMask(GL_TRUE);
	else
		glDepthMask(GL_FALSE);

	/* Color Write */
	if (flag & DRW_STATE_WRITE_COLOR)
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	else
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	/* Backface Culling */
	if (flag & DRW_STATE_CULL_BACK ||
	    flag & DRW_STATE_CULL_FRONT)
	{

		glEnable(GL_CULL_FACE);

		if (flag & DRW_STATE_CULL_BACK)
			glCullFace(GL_BACK);
		else if (flag & DRW_STATE_CULL_FRONT)
			glCullFace(GL_FRONT);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	/* Depht Test */
	if (flag & (DRW_STATE_DEPTH_LESS | DRW_STATE_DEPTH_EQUAL | DRW_STATE_DEPTH_GREATER))
	{

		glEnable(GL_DEPTH_TEST);

		if (flag & DRW_STATE_DEPTH_LESS)
			glDepthFunc(GL_LEQUAL);
		else if (flag & DRW_STATE_DEPTH_EQUAL)
			glDepthFunc(GL_EQUAL);
		else if (flag & DRW_STATE_DEPTH_GREATER)
			glDepthFunc(GL_GREATER);
	}
	else {
		glDisable(GL_DEPTH_TEST);
	}

	/* Wire Width */
	if (flag & DRW_STATE_WIRE) {
		glLineWidth(1.0f);
	}
	else if (flag & DRW_STATE_WIRE_LARGE) {
		glLineWidth(UI_GetThemeValuef(TH_OUTLINE_WIDTH) * 2.0f);
	}

	/* Points Size */
	if (flag & DRW_STATE_POINT) {
		GPU_enable_program_point_size();
		glPointSize(5.0f);
	}
	else {
		GPU_disable_program_point_size();
	}

	/* Blending (all buffer) */
	if (flag & DRW_STATE_BLEND) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}
	else {
		glDisable(GL_BLEND);
	}

	/* Line Stipple */
	if (flag & DRW_STATE_STIPPLE_2) {
		setlinestyle(2);
	}
	else if (flag & DRW_STATE_STIPPLE_3) {
		setlinestyle(3);
	}
	else if (flag & DRW_STATE_STIPPLE_4) {
		setlinestyle(4);
	}
	else {
		setlinestyle(0);
	}
}

void DRW_draw_pass(DRWPass *pass)
{
	/* Start fresh */
	DST.shader = NULL;
	DST.tex_bind_id = 0;

	set_state(pass->state);
	BLI_listbase_clear(&DST.bound_texs);

	for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
		draw_shgroup(shgroup);
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
}

/* Reset state to not interfer with other UI drawcall */
void DRW_state_reset(void)
{
	DRWState state = 0;
	state |= DRW_STATE_WRITE_DEPTH;
	state |= DRW_STATE_WRITE_COLOR;
	state |= DRW_STATE_DEPTH_LESS;
	set_state(state);
}

void DRW_draw_mode_overlays(void)
{
	const bContext *C = DRW_get_context();
	int mode = CTX_data_mode_enum(C);

	DRW_draw_grid();

	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			EDIT_MESH_draw();
			break;
		case CTX_MODE_EDIT_ARMATURE:
			EDIT_ARMATURE_draw();
			break;
		case CTX_MODE_OBJECT:
			OBJECT_draw();
			break;
	}

	DRW_draw_manipulator();
	DRW_draw_region_info();
}
#else
void DRW_draw_pass(DRWPass *UNUSED(pass))
{
}

#endif

/* ******************************************* Mode Engine Cache ****************************************** */

void DRW_mode_init(void)
{
	const bContext *C = DRW_get_context();
	int mode = CTX_data_mode_enum(C);

	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			EDIT_MESH_init();
			break;
		case CTX_MODE_EDIT_ARMATURE:
			break;
		case CTX_MODE_OBJECT:
			break;
	}
}

void DRW_mode_cache_init(void)
{
	const bContext *C = DRW_get_context();
	int mode = CTX_data_mode_enum(C);

	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			EDIT_MESH_cache_init();
			break;
		case CTX_MODE_EDIT_ARMATURE:
			EDIT_ARMATURE_cache_init();
			break;
		case CTX_MODE_OBJECT:
			OBJECT_cache_init();
			break;
	}
}

void DRW_mode_cache_populate(Object *ob)
{
	const bContext *C = DRW_get_context();
	int mode = CTX_data_mode_enum(C);

	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			EDIT_MESH_cache_populate(ob);
			break;
		case CTX_MODE_EDIT_ARMATURE:
			EDIT_ARMATURE_cache_populate(ob);
			break;
		case CTX_MODE_OBJECT:
			OBJECT_cache_populate(ob);
			break;
	}
}

void DRW_mode_cache_finish(void)
{
	const bContext *C = DRW_get_context();
	int mode = CTX_data_mode_enum(C);

	switch (mode) {
		case CTX_MODE_EDIT_MESH:
			EDIT_MESH_cache_finish();
			break;
		case CTX_MODE_EDIT_ARMATURE:
			EDIT_ARMATURE_cache_finish();
			break;
		case CTX_MODE_OBJECT:
			OBJECT_cache_finish();
			break;
	}
}

/* ****************************************** Settings ******************************************/
void *DRW_material_settings_get(Material *ma, const char *engine_name)
{
	MaterialEngineSettings *ms = NULL;

	ms = BLI_findstring(&ma->engines_settings, engine_name, offsetof(MaterialEngineSettings, name));

#ifdef WITH_CLAY_ENGINE
	/* If the settings does not exists yet, create it */
	if (ms == NULL) {
		ms = MEM_callocN(sizeof(RenderEngineSettings), "RenderEngineSettings");

		BLI_strncpy(ms->name, engine_name, 32);

		/* TODO make render_settings_create a polymorphic function */
		if (STREQ(engine_name, RE_engine_id_BLENDER_CLAY)) {
			ms->data = CLAY_material_settings_create();
		}
		else {
			/* No engine matched */
			BLI_assert(false);
		}

		BLI_addtail(&ma->engines_settings, ms);
	}
#else
	return NULL;
#endif

	return ms->data;
}

/* If scene is NULL, use context scene */
void *DRW_render_settings_get(Scene *scene, const char *engine_name)
{
	RenderEngineSettings *rs = NULL;

	if (scene == NULL)
		scene = CTX_data_scene(DST.context);

	rs = BLI_findstring(&scene->engines_settings, engine_name, offsetof(RenderEngineSettings, name));

#ifdef WITH_CLAY_ENGINE
	/* If the settings does not exists yet, create it */
	if (rs == NULL) {
		rs = MEM_callocN(sizeof(RenderEngineSettings), "RenderEngineSettings");

		BLI_strncpy(rs->name, engine_name, 32);

		/* TODO make render_settings_create a polymorphic function */
		if (STREQ(engine_name, RE_engine_id_BLENDER_CLAY)) {
			rs->data = CLAY_render_settings_create();
		}
		else {
			/* No engine matched */
			BLI_assert(false);
		}

		BLI_addtail(&scene->engines_settings, rs);
	}
#else
	return NULL;
#endif

	return rs->data;
}
/* ****************************************** Framebuffers ******************************************/

void DRW_framebuffer_init(struct GPUFrameBuffer **fb, int width, int height, DRWFboTexture textures[MAX_FBO_TEX],
                          int texnbr)
{
	BLI_assert(texnbr <= MAX_FBO_TEX);

	if (!*fb) {
		int color_attachment = -1;
		*fb = GPU_framebuffer_create();

		for (int i = 0; i < texnbr; ++i) {
			DRWFboTexture fbotex = textures[i];
			
			if (!*fbotex.tex) {
				/* TODO refine to opengl formats */
				if (fbotex.format == DRW_BUF_DEPTH_16 ||
				    fbotex.format == DRW_BUF_DEPTH_24)
				{
					*fbotex.tex = GPU_texture_create_depth(width, height, NULL);
					GPU_texture_compare_mode(*fbotex.tex, false);
					GPU_texture_filter_mode(*fbotex.tex, false);
				}
				else {
					*fbotex.tex = GPU_texture_create_2D(width, height, NULL, NULL);
					++color_attachment;
				}
			}
			
			GPU_framebuffer_texture_attach(*fb, *fbotex.tex, color_attachment);
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

void DRW_framebuffer_clear(bool color, bool depth, float clear_col[4])
{
	if (color) {
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClearColor(clear_col[0], clear_col[1], clear_col[2], clear_col[4]);
	}
	if (depth) {
		glDepthMask(GL_TRUE);
	}
	glClear(((color) ? GL_COLOR_BUFFER_BIT : 0) |
	        ((depth) ? GL_DEPTH_BUFFER_BIT : 0));
}

void DRW_framebuffer_texture_attach(struct GPUFrameBuffer *fb, GPUTexture *tex, int slot)
{
	GPU_framebuffer_texture_attach(fb, tex, slot);
}

void DRW_framebuffer_texture_detach(GPUTexture *tex)
{
	GPU_framebuffer_texture_detach(tex);
}

void DRW_framebuffer_blit(struct GPUFrameBuffer *fb_read, struct GPUFrameBuffer *fb_write, bool depth)
{
	GPU_framebuffer_blit(fb_read, 0, fb_write, 0, depth);
}

/* ****************************************** Viewport ******************************************/

float *DRW_viewport_size_get(void)
{
	return &DST.size[0];
}

float *DRW_viewport_screenvecs_get(void)
{
	return &DST.screenvecs[0][0];
}

float *DRW_viewport_pixelsize_get(void)
{
	return &DST.pixsize;
}

void DRW_viewport_init(const bContext *C)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	GPUViewport *viewport = rv3d->viewport;

	GPU_viewport_get_engine_data(viewport, &DST.fbl, &DST.txl, &DST.psl, &DST.stl);
	GPU_viewport_get_mode_data(viewport, &DST.fbl_mode, &DST.txl_mode, &DST.psl_mode, &DST.stl_mode);

	/* Refresh DST.size */
	DefaultTextureList *txl = (DefaultTextureList *)DST.txl;
	DST.size[0] = (float)GPU_texture_width(txl->color);
	DST.size[1] = (float)GPU_texture_height(txl->color);

	DefaultFramebufferList *fbl = (DefaultFramebufferList *)DST.fbl;
	DST.default_framebuffer = fbl->default_fb;

	/* Refresh DST.screenvecs */
	copy_v3_v3(DST.screenvecs[0], rv3d->viewinv[0]);
	copy_v3_v3(DST.screenvecs[1], rv3d->viewinv[1]);
	normalize_v3(DST.screenvecs[0]);
	normalize_v3(DST.screenvecs[1]);

	/* Refresh DST.pixelsize */
	DST.pixsize = rv3d->pixsize;

	/* Save context for all later needs */
	DST.context = C;

	DRW_update_global_values();
}

void DRW_viewport_matrix_get(float mat[4][4], DRWViewportMatrixType type)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);

	if (type == DRW_MAT_PERS)
		copy_m4_m4(mat, rv3d->persmat);
	else if (type == DRW_MAT_WIEW)
		copy_m4_m4(mat, rv3d->viewmat);
	else if (type == DRW_MAT_WIN)
		copy_m4_m4(mat, rv3d->winmat);
}

bool DRW_viewport_is_persp_get(void)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);
	return rv3d->is_persp;
}

bool DRW_viewport_cache_is_dirty(void)
{
	/* TODO Use a dirty flag */
	return (DST.psl->passes[0] == NULL);
}

/* ****************************************** OTHER ***************************************** */

const bContext *DRW_get_context(void)
{
	return DST.context;
}

void *DRW_engine_pass_list_get(void)
{
	return DST.psl;
}

void *DRW_engine_storage_list_get(void)
{
	return DST.stl;
}

void *DRW_engine_texture_list_get(void)
{
	return DST.txl;
}

void *DRW_engine_framebuffer_list_get(void)
{
	return DST.fbl;
}

void *DRW_mode_pass_list_get(void)
{
	return DST.psl_mode;
}

void *DRW_mode_storage_list_get(void)
{
	return DST.stl_mode;
}

void *DRW_mode_texture_list_get(void)
{
	return DST.txl_mode;
}

void *DRW_mode_framebuffer_list_get(void)
{
	return DST.fbl_mode;
}

/* ****************************************** INIT ***************************************** */

void DRW_engines_init(void)
{
#ifdef WITH_CLAY_ENGINE
	RE_engines_register(NULL, &viewport_clay_type);
#endif
}

extern struct GPUUniformBuffer *globals_ubo; /* draw_mode_pass.c */
void DRW_engines_free(void)
{
#ifdef WITH_CLAY_ENGINE
	CLAY_engine_free();

	EDIT_MESH_engine_free();

	DRW_shape_cache_free();

	if (globals_ubo)
		GPU_uniformbuffer_free(globals_ubo);

	BLI_remlink(&R_engines, &viewport_clay_type);
#endif
}

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

/** \file blender/draw/draw_manager.c
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

#include "GPU_basic_shader.h"
#include "GPU_batch.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"
#include "GPU_framebuffer.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniformbuffer.h"
#include "GPU_viewport.h"

#include "RE_engine.h"

#include "UI_resources.h"

#include "clay.h"

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

struct DRWUniform {
	struct DRWUniform *next, *prev;
	DRWUniformType type;
	int location;
	int length;
	int arraysize;
	int bindloc;
	const void *value;
};

struct DRWInterface {
	ListBase uniforms;
	/* matrices locations */
	int modelview;
	int projection;
	int modelviewprojection;
	int viewprojection;
	int normal;
	int eye;
};

struct DRWPass {
	ListBase shgroups;
	DRWState state;
	float state_param; /* Line / Point width */
};

typedef struct DRWCall {
	struct DRWCall *next, *prev;
	Batch *geometry;
	float(*obmat)[4];
} DRWCall;

struct DRWShadingGroup {
	struct DRWShadingGroup *next, *prev;
	struct GPUShader *shader;        /* Shader to bind */
	struct DRWInterface *interface;  /* Uniforms pointers */
	ListBase calls;                  /* List with all geometry and transforms */
	int state;                       /* State changes for this batch only */
	short dyntype;                   /* Dynamic Batch type, 0 is normal */
	Batch *dyngeom;                  /* Dynamic batch */
	GLuint instance_vbo;             /* Dynamic batch VBO storing Model Matrices */
	int instance_count;                /* Dynamic batch Number of instance to render */
};

/* Render State */
static struct DRWGlobalState{
	GPUShader *shader;
	struct GPUFrameBuffer *default_framebuffer;
	FramebufferList *current_fbl;
	TextureList *current_txl;
	PassList *current_psl;
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
	interface->viewprojection = GPU_shader_get_uniform(shader, "ViewProjectionMatrix");
	interface->modelviewprojection = GPU_shader_get_uniform(shader, "ModelViewProjectionMatrix");
	interface->normal = GPU_shader_get_uniform(shader, "NormalMatrix");
	interface->eye = GPU_shader_get_uniform(shader, "eye");

	BLI_listbase_clear(&interface->uniforms);

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

void DRW_get_dfdy_factors(float dfdyfac[2])
{
	GPU_get_dfdy_factors(dfdyfac);
}

/* ***************************************** SHADING GROUP ******************************************/

DRWShadingGroup *DRW_shgroup_create(struct GPUShader *shader, DRWPass *pass)
{
	DRWShadingGroup *shgroup = MEM_callocN(sizeof(DRWShadingGroup), "DRWShadingGroup");

	shgroup->shader = shader;
	shgroup->interface = DRW_interface_create(shader);
	shgroup->state = 0;
	shgroup->dyntype = 0;
	shgroup->dyngeom = NULL;

	BLI_addtail(&pass->shgroups, shgroup);

	return shgroup;
}

void DRW_shgroup_free(struct DRWShadingGroup *shgroup)
{
	BLI_freelistN(&shgroup->calls);
	BLI_freelistN(&shgroup->interface->uniforms);
	MEM_freeN(shgroup->interface);

	if (shgroup->dyngeom)
		Batch_discard_all(shgroup->dyngeom);
}

/* Later use VBO */
void DRW_shgroup_call_add(DRWShadingGroup *shgroup, Batch *geom, float (*obmat)[4])
{
	if (geom) {
		DRWCall *call = MEM_callocN(sizeof(DRWCall), "DRWCall");

		call->obmat = obmat;
		call->geometry = geom;

		BLI_addtail(&shgroup->calls, call);
	}
}

/* Make sure you know what you do when using this,
 * State is not revert back at the end of the shgroup */
void DRW_shgroup_state_set(DRWShadingGroup *shgroup, DRWState state)
{
	shgroup->state = state;
}

void DRW_shgroup_dyntype_set(DRWShadingGroup *shgroup, int type)
{
	shgroup->dyntype = type;
}

void DRW_shgroup_uniform_texture(DRWShadingGroup *shgroup, const char *name, const GPUTexture *tex, int loc)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_TEXTURE, tex, 0, 0, loc);
}

void DRW_shgroup_uniform_block(DRWShadingGroup *shgroup, const char *name, const GPUUniformBuffer *ubo, int loc)
{
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BLOCK, ubo, 0, 0, loc);
}

void DRW_shgroup_uniform_buffer(DRWShadingGroup *shgroup, const char *name, const int value, int loc)
{
	/* we abuse the lenght attrib to store the buffer index */
	DRW_interface_uniform(shgroup, name, DRW_UNIFORM_BUFFER, NULL, value, 0, loc);
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

/* Creates OGL primitives based on DRWCall.obmat position list */
static void shgroup_dynamic_batch_primitives(DRWShadingGroup *shgroup)
{
	int i = 0;
	int nbr = BLI_listbase_count(&shgroup->calls);
	GLenum type;

	if (nbr == 0) {
		if (shgroup->dyngeom) {
			Batch_discard(shgroup->dyngeom);
			shgroup->dyngeom = NULL;
		}
		return;
	}

	/* Gather Data */
	float *data = MEM_mallocN(sizeof(float) * 3 * nbr , "Object Center Batch data");

	for (DRWCall *call = shgroup->calls.first; call; call = call->next, i++) {
		copy_v3_v3(&data[i*3], call->obmat[3]);
	}

	/* Upload Data */
	static VertexFormat format = { 0 };
	static unsigned pos_id;
	if (format.attrib_ct == 0) {
		pos_id = add_attrib(&format, "pos", GL_FLOAT, 3, KEEP_FLOAT);
	}

	VertexBuffer *vbo = VertexBuffer_create_with_format(&format);
	VertexBuffer_allocate_data(vbo, nbr);

	fillAttrib(vbo, pos_id, data);

	if (shgroup->dyntype == DRW_DYN_POINTS)
		type = GL_POINTS;
	else
		type = GL_LINES;

	/* TODO make the batch dynamic instead of freeing it every times */
	if (shgroup->dyngeom)
		Batch_discard_all(shgroup->dyngeom);

	shgroup->dyngeom = Batch_create(type, vbo, NULL);

	MEM_freeN(data);
}

static void shgroup_dynamic_batch_instance(DRWShadingGroup *shgroup)
{
	int i = 0;
	int nbr = BLI_listbase_count(&shgroup->calls);

	shgroup->instance_count = nbr;

	if (nbr == 0) {
		if (shgroup->instance_vbo) {
			glDeleteBuffers(1, &shgroup->instance_vbo);
			shgroup->instance_vbo = 0;
		}
		return;
	}

	/* Gather Data */
	float *data = MEM_mallocN(sizeof(float) * 4 * 4 * nbr , "Instance Model Matrix");

	for (DRWCall *call = shgroup->calls.first; call; call = call->next, i++) {
		copy_m4_m4((float (*)[4])&data[i*16], call->obmat);
	}

	/* TODO poke mike to add this to gawain */
	if (shgroup->instance_vbo) {
		glDeleteBuffers(1, &shgroup->instance_vbo);
		shgroup->instance_vbo = 0;
	}

	glGenBuffers(1, &shgroup->instance_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, shgroup->instance_vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4 * nbr, data, GL_STATIC_DRAW);

	MEM_freeN(data);
}

static void shgroup_dynamic_batch_from_calls(DRWShadingGroup *shgroup)
{
#ifdef WITH_VIEWPORT_CACHE_TEST
	if (shgroup->dyngeom) return;
#endif
	if (shgroup->dyntype == DRW_DYN_INSTANCE) {
		shgroup_dynamic_batch_instance(shgroup);
	}
	else {
		shgroup_dynamic_batch_primitives(shgroup);
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

/* TODO this is slow we should not have to use this (better store shgroup pointer somewhere) */
DRWShadingGroup *DRW_pass_nth_shgroup_get(DRWPass *pass, int n)
{
	int i = 0;
	for (DRWShadingGroup *shgroup = pass->shgroups.first; shgroup; shgroup = shgroup->next) {
		if (i == n)
			return shgroup;
		i++;
	}

	return NULL;
}

/* ****************************************** DRAW ******************************************/

void DRW_draw_background(void)
{
	/* Just to make sure */
	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (UI_GetThemeValue(TH_SHOW_BACK_GRAD)) {
		/* Gradient background Color */
		gpuMatrixBegin3D(); /* TODO: finish 2D API */

		glClear(GL_DEPTH_BUFFER_BIT);

		VertexFormat *format = immVertexFormat();
		unsigned pos = add_attrib(format, "pos", COMP_F32, 2, KEEP_FLOAT);
		unsigned color = add_attrib(format, "color", COMP_U8, 3, NORMALIZE_INT_TO_FLOAT);
		unsigned char col_hi[3], col_lo[3];

		immBindBuiltinProgram(GPU_SHADER_2D_SMOOTH_COLOR);

		UI_GetThemeColor3ubv(TH_LOW_GRAD, col_lo);
		UI_GetThemeColor3ubv(TH_HIGH_GRAD, col_hi);

		immBegin(GL_QUADS, 4);
		immAttrib3ubv(color, col_lo);
		immVertex2f(pos, -1.0f, -1.0f);
		immVertex2f(pos, 1.0f, -1.0f);

		immAttrib3ubv(color, col_hi);
		immVertex2f(pos, 1.0f, 1.0f);
		immVertex2f(pos, -1.0f, 1.0f);
		immEnd();

		immUnbindProgram();

		gpuMatrixEnd();
	}
	else {
		/* Solid background Color */
		UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
}
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

static void draw_geometry(DRWShadingGroup *shgroup, DRWInterface *interface, Batch *geom,
                          unsigned int instance_vbo, int instance_count, const float (*obmat)[4])
{
	RegionView3D *rv3d = CTX_wm_region_view3d(DST.context);
	
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
	if (instance_vbo) {
		Batch_draw_stupid_instanced(geom, instance_vbo, instance_count);
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

	if (DST.shader != shgroup->shader) {
		if (DST.shader) GPU_shader_unbind();
		GPU_shader_bind(shgroup->shader);
		DST.shader = shgroup->shader;
	}

	if (shgroup->dyntype != 0) {
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
				GPU_texture_bind((GPUTexture *)uni->value, uni->bindloc);

				bound_tex = MEM_callocN(sizeof(DRWBoundTexture), "DRWBoundTexture");
				bound_tex->tex = (GPUTexture *)uni->value;
				BLI_addtail(&DST.bound_texs, bound_tex);

				GPU_shader_uniform_texture(shgroup->shader, uni->location, (GPUTexture *)uni->value);
				break;
			case DRW_UNIFORM_BUFFER:
				/* restore index from lenght we abused */
				GPU_texture_bind(DST.current_txl->textures[uni->length], uni->bindloc);
				GPU_texture_compare_mode(DST.current_txl->textures[uni->length], false);
				GPU_texture_filter_mode(DST.current_txl->textures[uni->length], false);
				
				bound_tex = MEM_callocN(sizeof(DRWBoundTexture), "DRWBoundTexture");
				bound_tex->tex = DST.current_txl->textures[uni->length];
				BLI_addtail(&DST.bound_texs, bound_tex);

				GPU_shader_uniform_texture(shgroup->shader, uni->location, DST.current_txl->textures[uni->length]);
				break;
			case DRW_UNIFORM_BLOCK:
				GPU_uniformbuffer_bind((GPUUniformBuffer *)uni->value, uni->bindloc);
				GPU_shader_uniform_buffer(shgroup->shader, uni->location, (GPUUniformBuffer *)uni->value);
				break;
		}
	}

	/* Rendering Calls */
	if (shgroup->dyntype != 0) {
		/* Replacing multiple calls with only one */
		float obmat[4][4];
		unit_m4(obmat);

		if (shgroup->dyntype == DRW_DYN_INSTANCE && shgroup->instance_count > 0) {
			DRWCall *call = shgroup->calls.first;
			draw_geometry(shgroup, interface, call->geometry, shgroup->instance_vbo, shgroup->instance_count, obmat);
		}
		else {
			/* Some dynamic batch can have no geom (no call to aggregate) */
			if (shgroup->dyngeom) {
				draw_geometry(shgroup, interface, shgroup->dyngeom, 0, 1, obmat);
			}
		}
	}
	else {
		for (DRWCall *call = shgroup->calls.first; call; call = call->next) {
			draw_geometry(shgroup, interface, call->geometry, 0, 1, call->obmat);
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
	    flag & DRW_STATE_CULL_FRONT) {

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
	if (flag & DRW_STATE_DEPTH_LESS ||
	    flag & DRW_STATE_DEPTH_EQUAL) {

		glEnable(GL_DEPTH_TEST);

		if (flag & DRW_STATE_DEPTH_LESS)
			glDepthFunc(GL_LEQUAL);
		else if (flag & DRW_STATE_DEPTH_EQUAL)
			glDepthFunc(GL_EQUAL);
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
#endif
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
	if (!*fb) {
		int color_attachment = -1;
		*fb = GPU_framebuffer_create();

		for (int i = 0; i < texnbr; ++i)
		{
			DRWFboTexture fbotex = textures[i];
			
			if (!*fbotex.tex) {
				/* TODO refine to opengl formats */
				if (fbotex.format == DRW_BUF_DEPTH_16 ||
					fbotex.format == DRW_BUF_DEPTH_24) {
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

void DRW_framebuffer_texture_attach(struct GPUFrameBuffer *fb, GPUTexture *tex, int slot)
{
	GPU_framebuffer_texture_attach(fb, tex, slot);
}

void DRW_framebuffer_texture_detach(GPUTexture *tex)
{
	GPU_framebuffer_texture_detach(tex);
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

void DRW_viewport_init(const bContext *C, void **buffers, void **textures, void **passes, void **storage)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	GPUViewport *viewport = rv3d->viewport;

	GPU_viewport_get_engine_data(viewport, buffers, textures, passes, storage);

	/* Refresh DST.size */
	DefaultTextureList *txl = (DefaultTextureList *)*textures;
	DST.size[0] = (float)GPU_texture_width(txl->color);
	DST.size[1] = (float)GPU_texture_height(txl->color);

	DefaultFramebufferList *fbl = (DefaultFramebufferList *)*buffers;
	DST.default_framebuffer = fbl->default_fb;

	DST.current_txl = (TextureList *)*textures;
	DST.current_fbl = (FramebufferList *)*buffers;
	DST.current_psl = (PassList *)*passes;

	/* Refresh DST.screenvecs */
	copy_v3_v3(DST.screenvecs[0], rv3d->viewinv[0]);
	copy_v3_v3(DST.screenvecs[1], rv3d->viewinv[1]);
	normalize_v3(DST.screenvecs[0]);
	normalize_v3(DST.screenvecs[1]);

	/* Refresh DST.pixelsize */
	DST.pixsize = rv3d->pixsize;

	/* Save context for all later needs */
	DST.context = C;
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
	return (DST.current_psl->passes[0] == NULL);
}

/* ****************************************** INIT ******************************************/

void DRW_engines_init(void)
{
#ifdef WITH_CLAY_ENGINE
	RE_engines_register(NULL, &viewport_clay_type);
#endif
}

void DRW_engines_free(void)
{
#ifdef WITH_CLAY_ENGINE
	clay_engine_free();

	DRW_shape_cache_free();

	BLI_remlink(&R_engines, &viewport_clay_type);
#endif
}
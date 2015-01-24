/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_material.h
 *  \ingroup gpu
 */

#ifndef __GPU_MATERIAL_H__
#define __GPU_MATERIAL_H__

#include "DNA_customdata_types.h" /* for CustomDataType */
#include "DNA_listBase.h"

#include "BLI_sys_types.h" /* for bool */

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImageUser;
struct Material;
struct Object;
struct Lamp;
struct Image;
struct bNode;
struct LinkNode;
struct Scene;
struct SceneRenderLayer;
struct GPUVertexAttribs;
struct GPUNode;
struct GPUNodeLink;
struct GPUNodeStack;
struct GPUMaterial;
struct GPUTexture;
struct GPULamp;
struct PreviewImage;
struct World;

typedef struct GPUNode GPUNode;
typedef struct GPUNodeLink GPUNodeLink;
typedef struct GPUMaterial GPUMaterial;
typedef struct GPULamp GPULamp;

/* Functions to create GPU Materials nodes */

typedef enum GPUType {
	/* The value indicates the number of elements in each type */
	GPU_NONE = 0,
	GPU_FLOAT = 1,
	GPU_VEC2 = 2,
	GPU_VEC3 = 3,
	GPU_VEC4 = 4,
	GPU_MAT3 = 9,
	GPU_MAT4 = 16,

	GPU_TEX2D = 1002,
	GPU_SHADOW2D = 1003,
	GPU_ATTRIB = 3001
} GPUType;

typedef enum GPUBuiltin {
	GPU_VIEW_MATRIX = 1,
	GPU_OBJECT_MATRIX = 2,
	GPU_INVERSE_VIEW_MATRIX = 4,
	GPU_INVERSE_OBJECT_MATRIX = 8,
	GPU_VIEW_POSITION = 16,
	GPU_VIEW_NORMAL = 32,
	GPU_OBCOLOR = 64,
	GPU_AUTO_BUMPSCALE = 128,
} GPUBuiltin;

typedef enum GPUOpenGLBuiltin {
	GPU_MATCAP_NORMAL = 1,
	GPU_COLOR = 2,
} GPUOpenGLBuiltin;

typedef enum GPUMatType {
	GPU_MATERIAL_TYPE_MESH  = 1,
	GPU_MATERIAL_TYPE_WORLD = 2,	
} GPUMatType;


typedef enum GPUBlendMode {
	GPU_BLEND_SOLID = 0,
	GPU_BLEND_ADD = 1,
	GPU_BLEND_ALPHA = 2,
	GPU_BLEND_CLIP = 4,
	GPU_BLEND_ALPHA_SORT = 8
} GPUBlendMode;

typedef struct GPUNodeStack {
	GPUType type;
	const char *name;
	float vec[4];
	struct GPUNodeLink *link;
	bool hasinput;
	bool hasoutput;
	short sockettype;
} GPUNodeStack;

typedef enum GPUDynamicType {
	GPU_DYNAMIC_NONE = 0,
	GPU_DYNAMIC_OBJECT_VIEWMAT = 1,
	GPU_DYNAMIC_OBJECT_MAT = 2,
	GPU_DYNAMIC_OBJECT_VIEWIMAT = 3,
	GPU_DYNAMIC_OBJECT_IMAT = 4,
	GPU_DYNAMIC_OBJECT_COLOR = 5,
	GPU_DYNAMIC_OBJECT_AUTOBUMPSCALE = 15,

	GPU_DYNAMIC_LAMP_FIRST = 6,
	GPU_DYNAMIC_LAMP_DYNVEC = 6,
	GPU_DYNAMIC_LAMP_DYNCO = 7,
	GPU_DYNAMIC_LAMP_DYNIMAT = 8,
	GPU_DYNAMIC_LAMP_DYNPERSMAT = 9,
	GPU_DYNAMIC_LAMP_DYNENERGY = 10,
	GPU_DYNAMIC_LAMP_DYNCOL = 11,
	GPU_DYNAMIC_LAMP_LAST = 11,
	GPU_DYNAMIC_SAMPLER_2DBUFFER = 12,
	GPU_DYNAMIC_SAMPLER_2DIMAGE = 13,
	GPU_DYNAMIC_SAMPLER_2DSHADOW = 14,
	GPU_DYNAMIC_LAMP_DISTANCE = 16,
	GPU_DYNAMIC_LAMP_ATT1 = 17,
	GPU_DYNAMIC_LAMP_ATT2 = 18,
	GPU_DYNAMIC_LAMP_SPOTSIZE = 19,
	GPU_DYNAMIC_LAMP_SPOTBLEND = 20,
} GPUDynamicType;

GPUNodeLink *GPU_attribute(CustomDataType type, const char *name);
GPUNodeLink *GPU_uniform(float *num);
GPUNodeLink *GPU_dynamic_uniform(float *num, GPUDynamicType dynamictype, void *data);
GPUNodeLink *GPU_image(struct Image *ima, struct ImageUser *iuser, bool is_data);
GPUNodeLink *GPU_image_preview(struct PreviewImage *prv);
GPUNodeLink *GPU_texture(int size, float *pixels);
GPUNodeLink *GPU_dynamic_texture(struct GPUTexture *tex, GPUDynamicType dynamictype, void *data);
GPUNodeLink *GPU_builtin(GPUBuiltin builtin);
GPUNodeLink *GPU_opengl_builtin(GPUOpenGLBuiltin builtin);

bool GPU_link(GPUMaterial *mat, const char *name, ...);
bool GPU_stack_link(GPUMaterial *mat, const char *name, GPUNodeStack *in, GPUNodeStack *out, ...);

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_enable_alpha(GPUMaterial *material);
GPUBlendMode GPU_material_alpha_blend(GPUMaterial *material, float obcol[4]);

/* High level functions to create and use GPU materials */
GPUMaterial *GPU_material_world(struct Scene *scene, struct World *wo);

GPUMaterial *GPU_material_from_blender(struct Scene *scene, struct Material *ma);
GPUMaterial *GPU_material_matcap(struct Scene *scene, struct Material *ma);
void GPU_material_free(struct ListBase *gpumaterial);

void GPU_materials_free(void);

bool GPU_lamp_override_visible(GPULamp *lamp, struct SceneRenderLayer *srl, struct Material *ma);
void GPU_material_bind(GPUMaterial *material, int oblay, int viewlay, double time, int mipmap, float viewmat[4][4], float viewinv[4][4], bool scenelock);
void GPU_material_bind_uniforms(GPUMaterial *material, float obmat[4][4], float obcol[4], float autobumpscale);
void GPU_material_unbind(GPUMaterial *material);
int GPU_material_bound(GPUMaterial *material);
struct Scene *GPU_material_scene(GPUMaterial *material);
GPUMatType GPU_Material_get_type(GPUMaterial *material);

void GPU_material_vertex_attributes(GPUMaterial *material,
	struct GPUVertexAttribs *attrib);

bool GPU_material_do_color_management(GPUMaterial *mat);
bool GPU_material_use_new_shading_nodes(GPUMaterial *mat);

/* Exported shading */

typedef struct GPUShadeInput {
	GPUMaterial *gpumat;
	struct Material *mat;

	GPUNodeLink *rgb, *specrgb, *vn, *view, *vcol, *ref;
	GPUNodeLink *alpha, *refl, *spec, *emit, *har, *amb;
	GPUNodeLink *spectra;
} GPUShadeInput;

typedef struct GPUShadeResult {
	GPUNodeLink *diff, *spec, *combined, *alpha;
} GPUShadeResult;

void GPU_shadeinput_set(GPUMaterial *mat, struct Material *ma, GPUShadeInput *shi);
void GPU_shaderesult_set(GPUShadeInput *shi, GPUShadeResult *shr);

/* Export GLSL shader */

typedef enum GPUDataType {
	GPU_DATA_NONE = 0,
	GPU_DATA_1I = 1,	// 1 integer
	GPU_DATA_1F = 2,
	GPU_DATA_2F = 3,
	GPU_DATA_3F = 4,
	GPU_DATA_4F = 5,
	GPU_DATA_9F = 6,
	GPU_DATA_16F = 7,
	GPU_DATA_4UB = 8,
} GPUDataType;

/* this structure gives information of each uniform found in the shader */
typedef struct GPUInputUniform {
	struct GPUInputUniform *next, *prev;
	char varname[32];		/* name of uniform in shader */
	GPUDynamicType type;	/* type of uniform, data format and calculation derive from it */
	GPUDataType datatype;	/* type of uniform data */
	struct Object *lamp;	/* when type=GPU_DYNAMIC_LAMP_... or GPU_DYNAMIC_SAMPLER_2DSHADOW */
	struct Image *image;	/* when type=GPU_DYNAMIC_SAMPLER_2DIMAGE */
	int texnumber;			/* when type=GPU_DYNAMIC_SAMPLER, texture number: 0.. */
	unsigned char *texpixels;	/* for internally generated texture, pixel data in RGBA format */
	int texsize;			/* size in pixel of the texture in texpixels buffer: for 2D textures, this is S and T size (square texture) */
} GPUInputUniform;

typedef struct GPUInputAttribute {
	struct GPUInputAttribute *next, *prev;
	char varname[32];	/* name of attribute in shader */
	int type;			/* from CustomData.type, data type derives from it */
	GPUDataType datatype;	/* type of attribute data */
	const char *name;	/* layer name */
	int number;			/* generic attribute number */
} GPUInputAttribute;

typedef struct GPUShaderExport {
	ListBase uniforms;
	ListBase attributes;
	char *vertex;
	char *fragment;
} GPUShaderExport;

GPUShaderExport *GPU_shader_export(struct Scene *scene, struct Material *ma);
void GPU_free_shader_export(GPUShaderExport *shader);

/* Lamps */

GPULamp *GPU_lamp_from_blender(struct Scene *scene, struct Object *ob, struct Object *par);
void GPU_lamp_free(struct Object *ob);

bool GPU_lamp_has_shadow_buffer(GPULamp *lamp);
void GPU_lamp_update_buffer_mats(GPULamp *lamp);
void GPU_lamp_shadow_buffer_bind(GPULamp *lamp, float viewmat[4][4], int *winsize, float winmat[4][4]);
void GPU_lamp_shadow_buffer_unbind(GPULamp *lamp);
int GPU_lamp_shadow_buffer_type(GPULamp *lamp);

void GPU_lamp_update(GPULamp *lamp, int lay, int hide, float obmat[4][4]);
void GPU_lamp_update_colors(GPULamp *lamp, float r, float g, float b, float energy);
void GPU_lamp_update_distance(GPULamp *lamp, float distance, float att1, float att2);
void GPU_lamp_update_spot(GPULamp *lamp, float spotsize, float spotblend);
int GPU_lamp_shadow_layer(GPULamp *lamp);
GPUNodeLink *GPU_lamp_get_data(GPUMaterial *mat, GPULamp *lamp, GPUNodeLink **col, GPUNodeLink **lv, GPUNodeLink **dist, GPUNodeLink **shadow);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_MATERIAL_H__*/


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
struct Image;
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
typedef struct GPUParticleInfo GPUParticleInfo;

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
	GPU_TEXCUBE = 1004,
	GPU_ATTRIB = 3001
} GPUType;

typedef enum GPUBuiltin {
	GPU_VIEW_MATRIX =           (1 << 0),
	GPU_OBJECT_MATRIX =         (1 << 1),
	GPU_INVERSE_VIEW_MATRIX =   (1 << 2),
	GPU_INVERSE_OBJECT_MATRIX = (1 << 3),
	GPU_VIEW_POSITION =         (1 << 4),
	GPU_VIEW_NORMAL =           (1 << 5),
	GPU_OBCOLOR =               (1 << 6),
	GPU_AUTO_BUMPSCALE =        (1 << 7),
	GPU_CAMERA_TEXCO_FACTORS =  (1 << 8),
	GPU_PARTICLE_SCALAR_PROPS = (1 << 9),
	GPU_PARTICLE_LOCATION =	    (1 << 10),
	GPU_PARTICLE_VELOCITY =     (1 << 11),
	GPU_PARTICLE_ANG_VELOCITY = (1 << 12),
	GPU_LOC_TO_VIEW_MATRIX =    (1 << 13),
	GPU_INVERSE_LOC_TO_VIEW_MATRIX = (1 << 14),
	GPU_OBJECT_INFO =           (1 << 15)
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
	GPU_BLEND_ALPHA_SORT = 8,
	GPU_BLEND_ALPHA_TO_COVERAGE = 16
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


#define GPU_DYNAMIC_GROUP_FROM_TYPE(f) ((f) & 0xFFFF0000)

#define GPU_DYNAMIC_GROUP_MISC     0x00010000
#define GPU_DYNAMIC_GROUP_LAMP     0x00020000
#define GPU_DYNAMIC_GROUP_OBJECT   0x00030000
#define GPU_DYNAMIC_GROUP_SAMPLER  0x00040000
#define GPU_DYNAMIC_GROUP_MIST     0x00050000
#define GPU_DYNAMIC_GROUP_WORLD    0x00060000
#define GPU_DYNAMIC_GROUP_MAT      0x00070000

typedef enum GPUDynamicType {

	GPU_DYNAMIC_NONE                 = 0,

	GPU_DYNAMIC_OBJECT_VIEWMAT       = 1  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_MAT           = 2  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_VIEWIMAT      = 3  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_IMAT          = 4  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_COLOR         = 5  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_AUTOBUMPSCALE = 6  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_LOCTOVIEWMAT  = 7  | GPU_DYNAMIC_GROUP_OBJECT,
	GPU_DYNAMIC_OBJECT_LOCTOVIEWIMAT = 8  | GPU_DYNAMIC_GROUP_OBJECT,

	GPU_DYNAMIC_LAMP_DYNVEC          = 1  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_DYNCO           = 2  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_DYNIMAT         = 3  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_DYNPERSMAT      = 4  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_DYNENERGY       = 5  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_DYNCOL          = 6  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_DISTANCE        = 7  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_ATT1            = 8  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_ATT2            = 9  | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_SPOTSIZE        = 10 | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_SPOTBLEND       = 11 | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_SPOTSCALE       = 12 | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_COEFFCONST      = 13 | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_COEFFLIN        = 14 | GPU_DYNAMIC_GROUP_LAMP,
	GPU_DYNAMIC_LAMP_COEFFQUAD       = 15 | GPU_DYNAMIC_GROUP_LAMP,

	GPU_DYNAMIC_SAMPLER_2DBUFFER     = 1  | GPU_DYNAMIC_GROUP_SAMPLER,
	GPU_DYNAMIC_SAMPLER_2DIMAGE      = 2  | GPU_DYNAMIC_GROUP_SAMPLER,
	GPU_DYNAMIC_SAMPLER_2DSHADOW     = 3  | GPU_DYNAMIC_GROUP_SAMPLER,

	GPU_DYNAMIC_MIST_ENABLE          = 1  | GPU_DYNAMIC_GROUP_MIST,
	GPU_DYNAMIC_MIST_START           = 2  | GPU_DYNAMIC_GROUP_MIST,
	GPU_DYNAMIC_MIST_DISTANCE        = 3  | GPU_DYNAMIC_GROUP_MIST,
	GPU_DYNAMIC_MIST_INTENSITY       = 4  | GPU_DYNAMIC_GROUP_MIST,
	GPU_DYNAMIC_MIST_TYPE            = 5  | GPU_DYNAMIC_GROUP_MIST,
	GPU_DYNAMIC_MIST_COLOR           = 6  | GPU_DYNAMIC_GROUP_MIST,

	GPU_DYNAMIC_HORIZON_COLOR        = 1  | GPU_DYNAMIC_GROUP_WORLD,
	GPU_DYNAMIC_AMBIENT_COLOR        = 2  | GPU_DYNAMIC_GROUP_WORLD,
	GPU_DYNAMIC_ZENITH_COLOR         = 3  | GPU_DYNAMIC_GROUP_WORLD,

	GPU_DYNAMIC_MAT_DIFFRGB          = 1  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_REF              = 2  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_SPECRGB          = 3  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_SPEC             = 4  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_HARD             = 5  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_EMIT             = 6  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_AMB              = 7  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_ALPHA            = 8  | GPU_DYNAMIC_GROUP_MAT,
	GPU_DYNAMIC_MAT_MIR              = 9  | GPU_DYNAMIC_GROUP_MAT
} GPUDynamicType;

GPUNodeLink *GPU_attribute(CustomDataType type, const char *name);
GPUNodeLink *GPU_uniform(float *num);
GPUNodeLink *GPU_dynamic_uniform(float *num, GPUDynamicType dynamictype, void *data);
GPUNodeLink *GPU_image(struct Image *ima, struct ImageUser *iuser, bool is_data);
GPUNodeLink *GPU_cube_map(struct Image *ima, struct ImageUser *iuser, bool is_data);
GPUNodeLink *GPU_image_preview(struct PreviewImage *prv);
GPUNodeLink *GPU_texture(int size, float *pixels);
GPUNodeLink *GPU_dynamic_texture(struct GPUTexture *tex, GPUDynamicType dynamictype, void *data);
GPUNodeLink *GPU_builtin(GPUBuiltin builtin);
GPUNodeLink *GPU_opengl_builtin(GPUOpenGLBuiltin builtin);
void GPU_node_link_set_type(GPUNodeLink *link, GPUType type);

bool GPU_link(GPUMaterial *mat, const char *name, ...);
bool GPU_stack_link(GPUMaterial *mat, const char *name, GPUNodeStack *in, GPUNodeStack *out, ...);

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_enable_alpha(GPUMaterial *material);
GPUBuiltin GPU_get_material_builtins(GPUMaterial *material);
GPUBlendMode GPU_material_alpha_blend(GPUMaterial *material, float obcol[4]);

/* High level functions to create and use GPU materials */
GPUMaterial *GPU_material_world(struct Scene *scene, struct World *wo);

GPUMaterial *GPU_material_from_blender(struct Scene *scene, struct Material *ma, bool use_opensubdiv);
GPUMaterial *GPU_material_matcap(struct Scene *scene, struct Material *ma, bool use_opensubdiv);
void GPU_material_free(struct ListBase *gpumaterial);

void GPU_materials_free(void);

bool GPU_lamp_visible(GPULamp *lamp, struct SceneRenderLayer *srl, struct Material *ma);
void GPU_material_bind(
        GPUMaterial *material, int oblay, int viewlay, double time, int mipmap,
        float viewmat[4][4], float viewinv[4][4], float cameraborder[4], bool scenelock);
void GPU_material_bind_uniforms(
        GPUMaterial *material, float obmat[4][4], float viewmat[4][4], float obcol[4],
        float autobumpscale, GPUParticleInfo *pi, float object_info[3]);
void GPU_material_unbind(GPUMaterial *material);
bool GPU_material_bound(GPUMaterial *material);
struct Scene *GPU_material_scene(GPUMaterial *material);
GPUMatType GPU_Material_get_type(GPUMaterial *material);

void GPU_material_vertex_attributes(GPUMaterial *material,
	struct GPUVertexAttribs *attrib);

bool GPU_material_do_color_management(GPUMaterial *mat);
bool GPU_material_use_new_shading_nodes(GPUMaterial *mat);
bool GPU_material_use_world_space_shading(GPUMaterial *mat);

/* Exported shading */

typedef struct GPUShadeInput {
	GPUMaterial *gpumat;
	struct Material *mat;

	GPUNodeLink *rgb, *specrgb, *vn, *view, *vcol, *ref;
	GPUNodeLink *alpha, *refl, *spec, *emit, *har, *amb;
	GPUNodeLink *spectra, *mir, *refcol;
} GPUShadeInput;

typedef struct GPUShadeResult {
	GPUNodeLink *diff, *spec, *combined, *alpha;
} GPUShadeResult;

void GPU_shadeinput_set(GPUMaterial *mat, struct Material *ma, GPUShadeInput *shi);
void GPU_shaderesult_set(GPUShadeInput *shi, GPUShadeResult *shr);

/* Export GLSL shader */

typedef enum GPUDataType {
	GPU_DATA_NONE = 0,
	GPU_DATA_1I = 1,   /* 1 integer */
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
	char varname[32];         /* name of uniform in shader */
	GPUDynamicType type;      /* type of uniform, data format and calculation derive from it */
	GPUDataType datatype;     /* type of uniform data */
	struct Object *lamp;      /* when type=GPU_DYNAMIC_LAMP_... or GPU_DYNAMIC_SAMPLER_2DSHADOW */
	struct Image *image;      /* when type=GPU_DYNAMIC_SAMPLER_2DIMAGE */
	struct Material *material;/* when type=GPU_DYNAMIC_MAT_... */
	int texnumber;            /* when type=GPU_DYNAMIC_SAMPLER, texture number: 0.. */
	unsigned char *texpixels; /* for internally generated texture, pixel data in RGBA format */
	int texsize;              /* size in pixel of the texture in texpixels buffer:
	                           * for 2D textures, this is S and T size (square texture) */
} GPUInputUniform;

typedef struct GPUInputAttribute {
	struct GPUInputAttribute *next, *prev;
	char varname[32];     /* name of attribute in shader */
	int type;             /* from CustomData.type, data type derives from it */
	GPUDataType datatype; /* type of attribute data */
	const char *name;     /* layer name */
	int number;           /* generic attribute number */
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
int GPU_lamp_shadow_bind_code(GPULamp *lamp);
float *GPU_lamp_dynpersmat(GPULamp *lamp);

void GPU_lamp_update(GPULamp *lamp, int lay, int hide, float obmat[4][4]);
void GPU_lamp_update_colors(GPULamp *lamp, float r, float g, float b, float energy);
void GPU_lamp_update_distance(GPULamp *lamp, float distance, float att1, float att2,
                              float coeff_const, float coeff_lin, float coeff_quad);
void GPU_lamp_update_spot(GPULamp *lamp, float spotsize, float spotblend);
int GPU_lamp_shadow_layer(GPULamp *lamp);
GPUNodeLink *GPU_lamp_get_data(
        GPUMaterial *mat, GPULamp *lamp,
        GPUNodeLink **r_col, GPUNodeLink **r_lv, GPUNodeLink **r_dist, GPUNodeLink **r_shadow, GPUNodeLink **r_energy);

/* World */
void GPU_mist_update_enable(short enable);
void GPU_mist_update_values(int type, float start, float dist, float inten, float color[3]);
void GPU_horizon_update_color(float color[3]);
void GPU_ambient_update_color(float color[3]);
void GPU_zenith_update_color(float color[3]);

struct GPUParticleInfo
{
	float scalprops[4];
	float location[4];
	float velocity[3];
	float angular_velocity[3];
};

#ifdef WITH_OPENSUBDIV
struct DerivedMesh;
void GPU_material_update_fvar_offset(GPUMaterial *gpu_material,
                                     struct DerivedMesh *dm);
#endif

#ifdef __cplusplus
}
#endif

#endif /*__GPU_MATERIAL_H__*/

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
struct ListBase;
struct Main;
struct Material;
struct Object;
struct Scene;
struct GPUVertexAttribs;
struct GPUNode;
struct GPUNodeLink;
struct GPUNodeStack;
struct GPUMaterial;
struct GPUTexture;
struct GPUUniformBuffer;
struct PreviewImage;
struct World;
struct bNode;
struct bNodeTree;

typedef struct GPUNode GPUNode;
typedef struct GPUNodeLink GPUNodeLink;
typedef struct GPUMaterial GPUMaterial;

typedef struct GPUParticleInfo GPUParticleInfo;

/* Functions to create GPU Materials nodes */

typedef enum GPUType {
	/* Keep in sync with GPU_DATATYPE_STR */
	/* The value indicates the number of elements in each type */
	GPU_NONE = 0,
	GPU_FLOAT = 1,
	GPU_VEC2 = 2,
	GPU_VEC3 = 3,
	GPU_VEC4 = 4,
	GPU_MAT3 = 9,
	GPU_MAT4 = 16,

	/* Values not in GPU_DATATYPE_STR */
	GPU_TEX1D_ARRAY = 1001,
	GPU_TEX2D = 1002,
	GPU_TEX3D = 1003,
	GPU_SHADOW2D = 1004,
	GPU_TEXCUBE = 1005,

	/* GLSL Struct types */
	GPU_CLOSURE = 1006,

	/* Opengl Attributes */
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
	GPU_PARTICLE_LOCATION =     (1 << 10),
	GPU_PARTICLE_VELOCITY =     (1 << 11),
	GPU_PARTICLE_ANG_VELOCITY = (1 << 12),
	GPU_LOC_TO_VIEW_MATRIX =    (1 << 13),
	GPU_INVERSE_LOC_TO_VIEW_MATRIX = (1 << 14),
	GPU_OBJECT_INFO =           (1 << 15),
	GPU_VOLUME_DENSITY =        (1 << 16),
	GPU_VOLUME_FLAME =          (1 << 17),
	GPU_VOLUME_TEMPERATURE =    (1 << 18)
} GPUBuiltin;

typedef enum GPUOpenGLBuiltin {
	GPU_MATCAP_NORMAL = 1,
	GPU_COLOR = 2,
} GPUOpenGLBuiltin;

typedef enum GPUMatType {
	GPU_MATERIAL_TYPE_MESH  = 1,
	GPU_MATERIAL_TYPE_WORLD = 2,
} GPUMatType;

typedef enum GPUMatFlag {
	GPU_MATFLAG_DIFFUSE       = (1 << 0),
	GPU_MATFLAG_GLOSSY        = (1 << 1),
	GPU_MATFLAG_REFRACT       = (1 << 2),
	GPU_MATFLAG_SSS           = (1 << 3),
} GPUMatFlag;

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
	float vec[4];
	struct GPUNodeLink *link;
	bool hasinput;
	bool hasoutput;
	short sockettype;
	bool end;
} GPUNodeStack;

typedef enum GPUMaterialStatus {
	GPU_MAT_FAILED = 0,
	GPU_MAT_QUEUED,
	GPU_MAT_SUCCESS,
} GPUMaterialStatus;

#define GPU_DYNAMIC_GROUP_FROM_TYPE(f) ((f) & 0xFFFF0000)

#define GPU_DYNAMIC_GROUP_MISC     0x00010000
#define GPU_DYNAMIC_GROUP_LAMP     0x00020000
#define GPU_DYNAMIC_GROUP_OBJECT   0x00030000
#define GPU_DYNAMIC_GROUP_SAMPLER  0x00040000
#define GPU_DYNAMIC_GROUP_MIST     0x00050000
#define GPU_DYNAMIC_GROUP_WORLD    0x00060000
#define GPU_DYNAMIC_GROUP_MAT      0x00070000
#define GPU_DYNAMIC_UBO            0x00080000

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
GPUNodeLink *GPU_uniform_buffer(float *num, GPUType gputype);
GPUNodeLink *GPU_image(struct Image *ima, struct ImageUser *iuser, bool is_data);
GPUNodeLink *GPU_cube_map(struct Image *ima, struct ImageUser *iuser, bool is_data);
GPUNodeLink *GPU_image_preview(struct PreviewImage *prv);
GPUNodeLink *GPU_texture_ramp(GPUMaterial *mat, int size, float *pixels, float *layer);
GPUNodeLink *GPU_dynamic_texture(struct GPUTexture *tex, GPUDynamicType dynamictype, void *data);
GPUNodeLink *GPU_builtin(GPUBuiltin builtin);
GPUNodeLink *GPU_opengl_builtin(GPUOpenGLBuiltin builtin);
void GPU_node_link_set_type(GPUNodeLink *link, GPUType type);

bool GPU_link(GPUMaterial *mat, const char *name, ...);
bool GPU_stack_link(GPUMaterial *mat, struct bNode *node, const char *name, GPUNodeStack *in, GPUNodeStack *out, ...);
GPUNodeLink *GPU_uniformbuffer_link_out(
        struct GPUMaterial *mat, struct bNode *node,
        struct GPUNodeStack *stack, const int index);

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link);
GPUBuiltin GPU_get_material_builtins(GPUMaterial *material);

void GPU_material_sss_profile_create(GPUMaterial *material, float radii[3], short *falloff_type, float *sharpness);
struct GPUUniformBuffer *GPU_material_sss_profile_get(
        GPUMaterial *material, int sample_len, struct GPUTexture **tex_profile);

/* High level functions to create and use GPU materials */
GPUMaterial *GPU_material_from_nodetree_find(
        struct ListBase *gpumaterials, const void *engine_type, int options);
GPUMaterial *GPU_material_from_nodetree(
        struct Scene *scene, struct bNodeTree *ntree, struct ListBase *gpumaterials, const void *engine_type, int options,
        const char *vert_code, const char *geom_code, const char *frag_lib, const char *defines, const char *name);
void GPU_material_compile(GPUMaterial *mat);
void GPU_material_free(struct ListBase *gpumaterial);

void GPU_materials_free(struct Main *bmain);

struct Scene *GPU_material_scene(GPUMaterial *material);
GPUMatType GPU_Material_get_type(GPUMaterial *material);
struct GPUPass *GPU_material_get_pass(GPUMaterial *material);
struct ListBase *GPU_material_get_inputs(GPUMaterial *material);
GPUMaterialStatus GPU_material_status(GPUMaterial *mat);

struct GPUUniformBuffer *GPU_material_uniform_buffer_get(GPUMaterial *material);
void GPU_material_uniform_buffer_create(GPUMaterial *material, ListBase *inputs);

void GPU_material_vertex_attributes(
        GPUMaterial *material,
        struct GPUVertexAttribs *attrib);

bool GPU_material_do_color_management(GPUMaterial *mat);
bool GPU_material_use_domain_surface(GPUMaterial *mat);
bool GPU_material_use_domain_volume(GPUMaterial *mat);

void GPU_material_flag_set(GPUMaterial *mat, GPUMatFlag flag);
bool GPU_material_flag_get(GPUMaterial *mat, GPUMatFlag flag);

void GPU_pass_cache_init(void);
void GPU_pass_cache_garbage_collect(void);
void GPU_pass_cache_free(void);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_MATERIAL_H__*/

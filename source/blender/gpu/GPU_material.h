/*
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
 */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "DNA_customdata_types.h" /* for CustomDataType */
#include "DNA_image_types.h"
#include "DNA_listBase.h"

#include "BLI_sys_types.h" /* for bool */

#include "GPU_texture.h" /* for eGPUSamplerState */

#ifdef __cplusplus
extern "C" {
#endif

struct GHash;
struct GPUMaterial;
struct GPUNode;
struct GPUNodeLink;
struct GPUNodeStack;
struct GPUTexture;
struct GPUUniformBuf;
struct Image;
struct ImageUser;
struct ListBase;
struct Main;
struct Material;
struct Scene;
struct bNode;
struct bNodeTree;

typedef struct GPUMaterial GPUMaterial;
typedef struct GPUNode GPUNode;
typedef struct GPUNodeLink GPUNodeLink;

/* Functions to create GPU Materials nodes. */

typedef enum eGPUType {
  /* Keep in sync with GPU_DATATYPE_STR */
  /* The value indicates the number of elements in each type */
  GPU_NONE = 0,
  GPU_FLOAT = 1,
  GPU_VEC2 = 2,
  GPU_VEC3 = 3,
  GPU_VEC4 = 4,
  GPU_MAT3 = 9,
  GPU_MAT4 = 16,
  GPU_MAX_CONSTANT_DATA = GPU_MAT4,

  /* Values not in GPU_DATATYPE_STR */
  GPU_TEX1D_ARRAY = 1001,
  GPU_TEX2D = 1002,
  GPU_TEX2D_ARRAY = 1003,
  GPU_TEX3D = 1004,
  GPU_SHADOW2D = 1005,
  GPU_TEXCUBE = 1006,

  /* GLSL Struct types */
  GPU_CLOSURE = 1007,

  /* Opengl Attributes */
  GPU_ATTR = 3001,
} eGPUType;

typedef enum eGPUBuiltin {
  GPU_VIEW_MATRIX = (1 << 0),
  GPU_OBJECT_MATRIX = (1 << 1),
  GPU_INVERSE_VIEW_MATRIX = (1 << 2),
  GPU_INVERSE_OBJECT_MATRIX = (1 << 3),
  GPU_VIEW_POSITION = (1 << 4),
  GPU_VIEW_NORMAL = (1 << 5),
  GPU_OBJECT_COLOR = (1 << 6),
  GPU_AUTO_BUMPSCALE = (1 << 7),
  GPU_CAMERA_TEXCO_FACTORS = (1 << 8),
  GPU_PARTICLE_SCALAR_PROPS = (1 << 9),
  GPU_PARTICLE_LOCATION = (1 << 10),
  GPU_PARTICLE_VELOCITY = (1 << 11),
  GPU_PARTICLE_ANG_VELOCITY = (1 << 12),
  GPU_LOC_TO_VIEW_MATRIX = (1 << 13),
  GPU_INVERSE_LOC_TO_VIEW_MATRIX = (1 << 14),
  GPU_OBJECT_INFO = (1 << 15),
  GPU_BARYCENTRIC_TEXCO = (1 << 16),
  GPU_BARYCENTRIC_DIST = (1 << 17),
  GPU_WORLD_NORMAL = (1 << 18),
} eGPUBuiltin;

typedef enum eGPUMatFlag {
  GPU_MATFLAG_DIFFUSE = (1 << 0),
  GPU_MATFLAG_GLOSSY = (1 << 1),
  GPU_MATFLAG_REFRACT = (1 << 2),
  GPU_MATFLAG_SSS = (1 << 3),
  GPU_MATFLAG_BARYCENTRIC = (1 << 4),
} eGPUMatFlag;

typedef struct GPUNodeStack {
  eGPUType type;
  float vec[4];
  struct GPUNodeLink *link;
  bool hasinput;
  bool hasoutput;
  short sockettype;
  bool end;
} GPUNodeStack;

typedef enum eGPUMaterialStatus {
  GPU_MAT_FAILED = 0,
  GPU_MAT_QUEUED,
  GPU_MAT_SUCCESS,
} eGPUMaterialStatus;

typedef enum eGPUVolumeDefaultValue {
  GPU_VOLUME_DEFAULT_0,
  GPU_VOLUME_DEFAULT_1,
} eGPUVolumeDefaultValue;

typedef void (*GPUMaterialEvalCallbackFn)(GPUMaterial *mat,
                                          int options,
                                          const char **vert_code,
                                          const char **geom_code,
                                          const char **frag_lib,
                                          const char **defines);

GPUNodeLink *GPU_constant(const float *num);
GPUNodeLink *GPU_uniform(const float *num);
GPUNodeLink *GPU_attribute(GPUMaterial *mat, CustomDataType type, const char *name);
GPUNodeLink *GPU_uniform_attribute(GPUMaterial *mat, const char *name, bool use_dupli);
GPUNodeLink *GPU_image(GPUMaterial *mat,
                       struct Image *ima,
                       struct ImageUser *iuser,
                       eGPUSamplerState sampler_state);
GPUNodeLink *GPU_image_tiled(GPUMaterial *mat,
                             struct Image *ima,
                             struct ImageUser *iuser,
                             eGPUSamplerState sampler_state);
GPUNodeLink *GPU_image_tiled_mapping(GPUMaterial *mat, struct Image *ima, struct ImageUser *iuser);
GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *row);
GPUNodeLink *GPU_volume_grid(GPUMaterial *mat,
                             const char *name,
                             eGPUVolumeDefaultValue default_value);
GPUNodeLink *GPU_builtin(eGPUBuiltin builtin);

bool GPU_link(GPUMaterial *mat, const char *name, ...);
bool GPU_stack_link(GPUMaterial *mat,
                    struct bNode *node,
                    const char *name,
                    GPUNodeStack *in,
                    GPUNodeStack *out,
                    ...);
GPUNodeLink *GPU_uniformbuf_link_out(struct GPUMaterial *mat,
                                     struct bNode *node,
                                     struct GPUNodeStack *stack,
                                     const int index);

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_add_output_link_aov(GPUMaterial *material, GPUNodeLink *link, int hash);

void GPU_material_sss_profile_create(GPUMaterial *material, float radii[3]);
struct GPUUniformBuf *GPU_material_sss_profile_get(GPUMaterial *material,
                                                   int sample_len,
                                                   struct GPUTexture **tex_profile);

/**
 * High level functions to create and use GPU materials.
 */
GPUMaterial *GPU_material_from_nodetree_find(struct ListBase *gpumaterials,
                                             const void *engine_type,
                                             int options);
/**
 * \note Caller must use #GPU_material_from_nodetree_find to re-use existing materials,
 * This is enforced since constructing other arguments to this function may be expensive
 * so only do this when they are needed.
 */
GPUMaterial *GPU_material_from_nodetree(struct Scene *scene,
                                        struct Material *ma,
                                        struct bNodeTree *ntree,
                                        struct ListBase *gpumaterials,
                                        const void *engine_type,
                                        const int options,
                                        const bool is_volume_shader,
                                        const char *vert_code,
                                        const char *geom_code,
                                        const char *frag_lib,
                                        const char *defines,
                                        const char *name,
                                        GPUMaterialEvalCallbackFn callback);
void GPU_material_compile(GPUMaterial *mat);
void GPU_material_free(struct ListBase *gpumaterial);

void GPU_materials_free(struct Main *bmain);

struct Scene *GPU_material_scene(GPUMaterial *material);
struct GPUPass *GPU_material_get_pass(GPUMaterial *material);
struct GPUShader *GPU_material_get_shader(GPUMaterial *material);
/**
 * Return can be NULL if it's a world material.
 */
struct Material *GPU_material_get_material(GPUMaterial *material);
/**
 * Return true if the material compilation has not yet begin or begin.
 */
eGPUMaterialStatus GPU_material_status(GPUMaterial *mat);

struct GPUUniformBuf *GPU_material_uniform_buffer_get(GPUMaterial *material);
/**
 * Create dynamic UBO from parameters
 *
 * \param inputs: Items are #LinkData, data is #GPUInput (`BLI_genericNodeN(GPUInput)`).
 */
void GPU_material_uniform_buffer_create(GPUMaterial *material, ListBase *inputs);
struct GPUUniformBuf *GPU_material_create_sss_profile_ubo(void);

bool GPU_material_has_surface_output(GPUMaterial *mat);
bool GPU_material_has_volume_output(GPUMaterial *mat);

bool GPU_material_is_volume_shader(GPUMaterial *mat);

void GPU_material_flag_set(GPUMaterial *mat, eGPUMatFlag flag);
bool GPU_material_flag_get(GPUMaterial *mat, eGPUMatFlag flag);

void GPU_pass_cache_init(void);
void GPU_pass_cache_garbage_collect(void);
void GPU_pass_cache_free(void);

/* Requested Material Attributes and Textures */

typedef struct GPUMaterialAttribute {
  struct GPUMaterialAttribute *next, *prev;
  int type;      /* CustomDataType */
  char name[64]; /* MAX_CUSTOMDATA_LAYER_NAME */
  eGPUType gputype;
  int id;
  int users;
} GPUMaterialAttribute;

typedef struct GPUMaterialTexture {
  struct GPUMaterialTexture *next, *prev;
  struct Image *ima;
  struct ImageUser iuser;
  bool iuser_available;
  struct GPUTexture **colorband;
  char sampler_name[32];       /* Name of sampler in GLSL. */
  char tiled_mapping_name[32]; /* Name of tile mapping sampler in GLSL. */
  int users;
  int sampler_state; /* eGPUSamplerState */
} GPUMaterialTexture;

typedef struct GPUMaterialVolumeGrid {
  struct GPUMaterialVolumeGrid *next, *prev;
  char *name;
  eGPUVolumeDefaultValue default_value;
  char sampler_name[32];   /* Name of sampler in GLSL. */
  char transform_name[32]; /* Name of 4x4 matrix in GLSL. */
  int users;
} GPUMaterialVolumeGrid;

ListBase GPU_material_attributes(GPUMaterial *material);
ListBase GPU_material_textures(GPUMaterial *material);
ListBase GPU_material_volume_grids(GPUMaterial *material);

typedef struct GPUUniformAttr {
  struct GPUUniformAttr *next, *prev;

  /* Meaningful part of the attribute set key. */
  char name[64]; /* MAX_CUSTOMDATA_LAYER_NAME */
  bool use_dupli;

  /* Helper fields used by code generation. */
  short id;
  int users;
} GPUUniformAttr;

typedef struct GPUUniformAttrList {
  ListBase list; /* GPUUniformAttr */

  /* List length and hash code precomputed for fast lookup and comparison. */
  unsigned int count, hash_code;
} GPUUniformAttrList;

GPUUniformAttrList *GPU_material_uniform_attributes(GPUMaterial *material);

struct GHash *GPU_uniform_attr_list_hash_new(const char *info);
void GPU_uniform_attr_list_copy(GPUUniformAttrList *dest, GPUUniformAttrList *src);
void GPU_uniform_attr_list_free(GPUUniformAttrList *set);

#ifdef __cplusplus
}
#endif

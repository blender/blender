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

#ifndef __GPU_MATERIAL_H__
#define __GPU_MATERIAL_H__

#include "DNA_customdata_types.h" /* for CustomDataType */
#include "DNA_listBase.h"

#include "BLI_sys_types.h" /* for bool */

#ifdef __cplusplus
extern "C" {
#endif

struct GPUMaterial;
struct GPUNode;
struct GPUNodeLink;
struct GPUNodeStack;
struct GPUTexture;
struct GPUUniformBuffer;
struct GPUVertAttrLayers;
struct Image;
struct ImageUser;
struct ListBase;
struct Main;
struct Material;
struct Object;
struct PreviewImage;
struct Scene;
struct World;
struct bNode;
struct bNodeTree;

typedef struct GPUMaterial GPUMaterial;
typedef struct GPUNode GPUNode;
typedef struct GPUNodeLink GPUNodeLink;

typedef struct GPUParticleInfo GPUParticleInfo;

/* Functions to create GPU Materials nodes */

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

  /* Values not in GPU_DATATYPE_STR */
  GPU_TEX1D_ARRAY = 1001,
  GPU_TEX2D = 1002,
  GPU_TEX3D = 1003,
  GPU_SHADOW2D = 1004,
  GPU_TEXCUBE = 1005,

  /* GLSL Struct types */
  GPU_CLOSURE = 1006,

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
  GPU_OBCOLOR = (1 << 6),
  GPU_AUTO_BUMPSCALE = (1 << 7),
  GPU_CAMERA_TEXCO_FACTORS = (1 << 8),
  GPU_PARTICLE_SCALAR_PROPS = (1 << 9),
  GPU_PARTICLE_LOCATION = (1 << 10),
  GPU_PARTICLE_VELOCITY = (1 << 11),
  GPU_PARTICLE_ANG_VELOCITY = (1 << 12),
  GPU_LOC_TO_VIEW_MATRIX = (1 << 13),
  GPU_INVERSE_LOC_TO_VIEW_MATRIX = (1 << 14),
  GPU_OBJECT_INFO = (1 << 15),
  GPU_VOLUME_DENSITY = (1 << 16),
  GPU_VOLUME_FLAME = (1 << 17),
  GPU_VOLUME_TEMPERATURE = (1 << 18),
  GPU_BARYCENTRIC_TEXCO = (1 << 19),
  GPU_BARYCENTRIC_DIST = (1 << 20),
  GPU_WORLD_NORMAL = (1 << 21),
} eGPUBuiltin;

typedef enum eGPUMatFlag {
  GPU_MATFLAG_DIFFUSE = (1 << 0),
  GPU_MATFLAG_GLOSSY = (1 << 1),
  GPU_MATFLAG_REFRACT = (1 << 2),
  GPU_MATFLAG_SSS = (1 << 3),
} eGPUMatFlag;

typedef enum eGPUBlendMode {
  GPU_BLEND_SOLID = 0,
  GPU_BLEND_ADD = 1,
  GPU_BLEND_ALPHA = 2,
  GPU_BLEND_CLIP = 4,
  GPU_BLEND_ALPHA_SORT = 8,
  GPU_BLEND_ALPHA_TO_COVERAGE = 16,
} eGPUBlendMode;

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

GPUNodeLink *GPU_attribute(CustomDataType type, const char *name);
GPUNodeLink *GPU_constant(float *num);
GPUNodeLink *GPU_uniform(float *num);
GPUNodeLink *GPU_image(struct Image *ima, struct ImageUser *iuser);
GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *layer);
GPUNodeLink *GPU_builtin(eGPUBuiltin builtin);

bool GPU_link(GPUMaterial *mat, const char *name, ...);
bool GPU_stack_link(GPUMaterial *mat,
                    struct bNode *node,
                    const char *name,
                    GPUNodeStack *in,
                    GPUNodeStack *out,
                    ...);
GPUNodeLink *GPU_uniformbuffer_link_out(struct GPUMaterial *mat,
                                        struct bNode *node,
                                        struct GPUNodeStack *stack,
                                        const int index);

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link);
eGPUBuiltin GPU_get_material_builtins(GPUMaterial *material);

void GPU_material_sss_profile_create(GPUMaterial *material,
                                     float radii[3],
                                     short *falloff_type,
                                     float *sharpness);
struct GPUUniformBuffer *GPU_material_sss_profile_get(GPUMaterial *material,
                                                      int sample_len,
                                                      struct GPUTexture **tex_profile);

/* High level functions to create and use GPU materials */
GPUMaterial *GPU_material_from_nodetree_find(struct ListBase *gpumaterials,
                                             const void *engine_type,
                                             int options);
GPUMaterial *GPU_material_from_nodetree(struct Scene *scene,
                                        struct bNodeTree *ntree,
                                        struct ListBase *gpumaterials,
                                        const void *engine_type,
                                        int options,
                                        const char *vert_code,
                                        const char *geom_code,
                                        const char *frag_lib,
                                        const char *defines,
                                        const char *name);
void GPU_material_compile(GPUMaterial *mat);
void GPU_material_free(struct ListBase *gpumaterial);

void GPU_materials_free(struct Main *bmain);

struct Scene *GPU_material_scene(GPUMaterial *material);
struct GPUPass *GPU_material_get_pass(GPUMaterial *material);
struct ListBase *GPU_material_get_inputs(GPUMaterial *material);
eGPUMaterialStatus GPU_material_status(GPUMaterial *mat);

struct GPUUniformBuffer *GPU_material_uniform_buffer_get(GPUMaterial *material);
void GPU_material_uniform_buffer_create(GPUMaterial *material, ListBase *inputs);
struct GPUUniformBuffer *GPU_material_create_sss_profile_ubo(void);

void GPU_material_vertex_attrs(GPUMaterial *material, struct GPUVertAttrLayers *attrs);

bool GPU_material_use_domain_surface(GPUMaterial *mat);
bool GPU_material_use_domain_volume(GPUMaterial *mat);

void GPU_material_flag_set(GPUMaterial *mat, eGPUMatFlag flag);
bool GPU_material_flag_get(GPUMaterial *mat, eGPUMatFlag flag);

void GPU_pass_cache_init(void);
void GPU_pass_cache_garbage_collect(void);
void GPU_pass_cache_free(void);

#ifdef __cplusplus
}
#endif

#endif /*__GPU_MATERIAL_H__*/

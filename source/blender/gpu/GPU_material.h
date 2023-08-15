/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "DNA_customdata_types.h" /* for eCustomDataType */
#include "DNA_image_types.h"
#include "DNA_listBase.h"

#include "BLI_sys_types.h" /* for bool */

#include "GPU_shader.h"  /* for GPUShaderCreateInfo */
#include "GPU_texture.h" /* for GPUSamplerState */

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

  /* GLSL Struct types */
  GPU_CLOSURE = 1007,

  /* Opengl Attributes */
  GPU_ATTR = 3001,
} eGPUType;

typedef enum eGPUMaterialFlag {
  GPU_MATFLAG_DIFFUSE = (1 << 0),
  GPU_MATFLAG_SUBSURFACE = (1 << 1),
  GPU_MATFLAG_GLOSSY = (1 << 2),
  GPU_MATFLAG_REFRACT = (1 << 3),
  GPU_MATFLAG_EMISSION = (1 << 4),
  GPU_MATFLAG_TRANSPARENT = (1 << 5),
  GPU_MATFLAG_HOLDOUT = (1 << 6),
  GPU_MATFLAG_SHADER_TO_RGBA = (1 << 7),
  GPU_MATFLAG_AO = (1 << 8),
  GPU_MATFLAG_CLEARCOAT = (1 << 9),

  GPU_MATFLAG_OBJECT_INFO = (1 << 10),
  GPU_MATFLAG_AOV = (1 << 11),

  GPU_MATFLAG_BARYCENTRIC = (1 << 20),

  /* Optimization to only add the branches of the principled shader that are necessary. */
  GPU_MATFLAG_PRINCIPLED_CLEARCOAT = (1 << 21),
  GPU_MATFLAG_PRINCIPLED_METALLIC = (1 << 22),
  GPU_MATFLAG_PRINCIPLED_DIELECTRIC = (1 << 23),
  GPU_MATFLAG_PRINCIPLED_GLASS = (1 << 24),
  GPU_MATFLAG_PRINCIPLED_ANY = (1 << 25),

  /* Tells the render engine the material was just compiled or updated. */
  GPU_MATFLAG_UPDATED = (1 << 29),

  /* HACK(fclem) Tells the environment texture node to not bail out if empty. */
  GPU_MATFLAG_LOOKDEV_HACK = (1 << 30),
} eGPUMaterialFlag;

ENUM_OPERATORS(eGPUMaterialFlag, GPU_MATFLAG_LOOKDEV_HACK);

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
  GPU_MAT_CREATED,
  GPU_MAT_QUEUED,
  GPU_MAT_SUCCESS,
} eGPUMaterialStatus;

/* GPU_MAT_OPTIMIZATION_SKIP for cases where we do not
 * plan to perform optimization on a given material. */
typedef enum eGPUMaterialOptimizationStatus {
  GPU_MAT_OPTIMIZATION_SKIP = 0,
  GPU_MAT_OPTIMIZATION_READY,
  GPU_MAT_OPTIMIZATION_QUEUED,
  GPU_MAT_OPTIMIZATION_SUCCESS,
} eGPUMaterialOptimizationStatus;

typedef enum eGPUDefaultValue {
  GPU_DEFAULT_0 = 0,
  GPU_DEFAULT_1,
} eGPUDefaultValue;

typedef struct GPUCodegenOutput {
  char *attr_load;
  /* Node-tree functions calls. */
  char *displacement;
  char *surface;
  char *volume;
  char *thickness;
  char *composite;
  char *material_functions;

  GPUShaderCreateInfo *create_info;
} GPUCodegenOutput;

typedef void (*GPUCodegenCallbackFn)(void *thunk, GPUMaterial *mat, GPUCodegenOutput *codegen);

GPUNodeLink *GPU_constant(const float *num);
GPUNodeLink *GPU_uniform(const float *num);
GPUNodeLink *GPU_attribute(GPUMaterial *mat, eCustomDataType type, const char *name);
/**
 * Add a GPU attribute that refers to the default color attribute on a geometry.
 * The name, type, and domain are unknown and do not depend on the material.
 */
GPUNodeLink *GPU_attribute_default_color(GPUMaterial *mat);
/**
 * Add a GPU attribute that refers to the approximate length of curves/hairs.
 */
GPUNodeLink *GPU_attribute_hair_length(GPUMaterial *mat);
GPUNodeLink *GPU_attribute_with_default(GPUMaterial *mat,
                                        eCustomDataType type,
                                        const char *name,
                                        eGPUDefaultValue default_value);
GPUNodeLink *GPU_uniform_attribute(GPUMaterial *mat,
                                   const char *name,
                                   bool use_dupli,
                                   uint32_t *r_hash);
GPUNodeLink *GPU_layer_attribute(GPUMaterial *mat, const char *name);
GPUNodeLink *GPU_image(GPUMaterial *mat,
                       struct Image *ima,
                       struct ImageUser *iuser,
                       GPUSamplerState sampler_state);
void GPU_image_tiled(GPUMaterial *mat,
                     struct Image *ima,
                     struct ImageUser *iuser,
                     GPUSamplerState sampler_state,
                     GPUNodeLink **r_image_tiled_link,
                     GPUNodeLink **r_image_tiled_mapping_link);
GPUNodeLink *GPU_image_sky(GPUMaterial *mat,
                           int width,
                           int height,
                           const float *pixels,
                           float *layer,
                           GPUSamplerState sampler_state);
GPUNodeLink *GPU_color_band(GPUMaterial *mat, int size, float *pixels, float *row);

/**
 * Create an implementation defined differential calculation of a float function.
 * The given function should return a float.
 * The result will be a vec2 containing dFdx and dFdy result of that function.
 */
GPUNodeLink *GPU_differentiate_float_function(const char *function_name);

bool GPU_link(GPUMaterial *mat, const char *name, ...);
bool GPU_stack_link(GPUMaterial *mat,
                    const struct bNode *node,
                    const char *name,
                    GPUNodeStack *in,
                    GPUNodeStack *out,
                    ...);

void GPU_material_output_surface(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_output_volume(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_output_displacement(GPUMaterial *material, GPUNodeLink *link);
void GPU_material_output_thickness(GPUMaterial *material, GPUNodeLink *link);

void GPU_material_add_output_link_aov(GPUMaterial *material, GPUNodeLink *link, int hash);

void GPU_material_add_output_link_composite(GPUMaterial *material, GPUNodeLink *link);

/**
 * Wrap a part of the material graph into a function. You need then need to call the function by
 * using something like #GPU_differentiate_float_function.
 * \note This replace the link by a constant to break the link with the main graph.
 * \param return_type: sub function return type. Output is cast to this type.
 * \param link: link to use as the sub function output.
 * \return the name of the generated function.
 */
char *GPU_material_split_sub_function(GPUMaterial *material,
                                      eGPUType return_type,
                                      GPUNodeLink **link);

bool GPU_material_sss_profile_create(GPUMaterial *material, float radii[3]);
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
                                        const char *name,
                                        uint64_t shader_uuid,
                                        bool is_volume_shader,
                                        bool is_lookdev,
                                        GPUCodegenCallbackFn callback,
                                        void *thunk);

void GPU_material_compile(GPUMaterial *mat);
void GPU_material_free_single(GPUMaterial *material);
void GPU_material_free(struct ListBase *gpumaterial);

void GPU_material_acquire(GPUMaterial *mat);
void GPU_material_release(GPUMaterial *mat);

void GPU_materials_free(struct Main *bmain);

struct Scene *GPU_material_scene(GPUMaterial *material);
struct GPUPass *GPU_material_get_pass(GPUMaterial *material);
/** Return the most optimal shader configuration for the given material. */
struct GPUShader *GPU_material_get_shader(GPUMaterial *material);
/** Return the base un-optimized shader. */
struct GPUShader *GPU_material_get_shader_base(GPUMaterial *material);
const char *GPU_material_get_name(GPUMaterial *material);

/**
 * Material Optimization.
 * \note Compiles optimal version of shader graph, populating mat->optimized_pass.
 * This operation should always be deferred until existing compilations have completed.
 * Default un-optimized materials will still exist for interactive material editing performance.
 */
void GPU_material_optimize(GPUMaterial *mat);

/**
 * Return can be NULL if it's a world material.
 */
struct Material *GPU_material_get_material(GPUMaterial *material);
/**
 * Return true if the material compilation has not yet begin or begin.
 */
eGPUMaterialStatus GPU_material_status(GPUMaterial *mat);
void GPU_material_status_set(GPUMaterial *mat, eGPUMaterialStatus status);

/**
 * Return status for asynchronous optimization jobs.
 */
eGPUMaterialOptimizationStatus GPU_material_optimization_status(GPUMaterial *mat);
void GPU_material_optimization_status_set(GPUMaterial *mat, eGPUMaterialOptimizationStatus status);
bool GPU_material_optimization_ready(GPUMaterial *mat);

/**
 * Store reference to a similar default material for asynchronous PSO cache warming.
 *
 * This function expects `material` to have not yet been compiled and for `default_material` to be
 * ready. When compiling `material` as part of an asynchronous shader compilation job, use existing
 * PSO descriptors from `default_material`'s shader to also compile PSOs for this new material
 * asynchronously, rather than at runtime.
 *
 * The default_material `options` should match this new materials options in order
 * for PSO descriptors to match those needed by the new `material`.
 *
 * NOTE: `default_material` must exist when `GPU_material_compile(..)` is called for
 * `material`.
 *
 * See `GPU_shader_warm_cache(..)` for more information.
 */
void GPU_material_set_default(GPUMaterial *material, GPUMaterial *default_material);

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

void GPU_material_flag_set(GPUMaterial *mat, eGPUMaterialFlag flag);
bool GPU_material_flag_get(const GPUMaterial *mat, eGPUMaterialFlag flag);
eGPUMaterialFlag GPU_material_flag(const GPUMaterial *mat);
bool GPU_material_recalc_flag_get(GPUMaterial *mat);
uint64_t GPU_material_uuid_get(GPUMaterial *mat);

void GPU_pass_cache_init(void);
void GPU_pass_cache_garbage_collect(void);
void GPU_pass_cache_free(void);

/* Requested Material Attributes and Textures */

typedef struct GPUMaterialAttribute {
  struct GPUMaterialAttribute *next, *prev;
  int type;                /* eCustomDataType */
  char name[68];           /* MAX_CUSTOMDATA_LAYER_NAME */
  char input_name[12 + 1]; /* GPU_MAX_SAFE_ATTR_NAME + 1 */
  eGPUType gputype;
  eGPUDefaultValue default_value; /* Only for volumes attributes. */
  int id;
  int users;
  /**
   * If true, the corresponding attribute is the specified default color attribute on the mesh,
   * if it exists. In that case the type and name data can vary per geometry, so it will not be
   * valid here.
   */
  bool is_default_color;
  /**
   * If true, the attribute is the length of hair particles and curves.
   */
  bool is_hair_length;
} GPUMaterialAttribute;

typedef struct GPUMaterialTexture {
  struct GPUMaterialTexture *next, *prev;
  struct Image *ima;
  struct ImageUser iuser;
  bool iuser_available;
  struct GPUTexture **colorband;
  struct GPUTexture **sky;
  char sampler_name[32];       /* Name of sampler in GLSL. */
  char tiled_mapping_name[32]; /* Name of tile mapping sampler in GLSL. */
  int users;
  GPUSamplerState sampler_state;
} GPUMaterialTexture;

ListBase GPU_material_attributes(GPUMaterial *material);
ListBase GPU_material_textures(GPUMaterial *material);

typedef struct GPUUniformAttr {
  struct GPUUniformAttr *next, *prev;

  /* Meaningful part of the attribute set key. */
  char name[68]; /* MAX_CUSTOMDATA_LAYER_NAME */
  /** Hash of name[68] + use_dupli. */
  uint32_t hash_code;
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

const GPUUniformAttrList *GPU_material_uniform_attributes(const GPUMaterial *material);

struct GHash *GPU_uniform_attr_list_hash_new(const char *info);
void GPU_uniform_attr_list_copy(GPUUniformAttrList *dest, const GPUUniformAttrList *src);
void GPU_uniform_attr_list_free(GPUUniformAttrList *set);

typedef struct GPULayerAttr {
  struct GPULayerAttr *next, *prev;

  /* Meaningful part of the attribute set key. */
  char name[68]; /* MAX_CUSTOMDATA_LAYER_NAME */
  /** Hash of name[68]. */
  uint32_t hash_code;

  /* Helper fields used by code generation. */
  int users;
} GPULayerAttr;

const ListBase *GPU_material_layer_attributes(const GPUMaterial *material);

/* A callback passed to GPU_material_from_callbacks to construct the material graph by adding and
 * linking the necessary GPU material nodes. */
typedef void (*ConstructGPUMaterialFn)(void *thunk, GPUMaterial *material);

/* Construct a GPU material from a set of callbacks. See the callback types for more information.
 * The given thunk will be passed as the first parameter of each callback. */
GPUMaterial *GPU_material_from_callbacks(ConstructGPUMaterialFn construct_function_cb,
                                         GPUCodegenCallbackFn generate_code_function_cb,
                                         void *thunk);

#ifdef __cplusplus
}
#endif

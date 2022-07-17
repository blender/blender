/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup gpu
 *
 * Manages materials, lights and textures.
 */

#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_node.h"

#include "NOD_shader.h"

#include "GPU_material.h"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "DRW_engine.h"

#include "gpu_codegen.h"
#include "gpu_node_graph.h"

#include "atomic_ops.h"

/* Structs */
#define MAX_COLOR_BAND 128

typedef struct GPUColorBandBuilder {
  float pixels[MAX_COLOR_BAND][CM_TABLE + 1][4];
  int current_layer;
} GPUColorBandBuilder;

struct GPUMaterial {
  /* Contains GPUShader and source code for deferred compilation.
   * Can be shared between similar material (i.e: sharing same nodetree topology). */
  GPUPass *pass;
  /** UBOs for this material parameters. */
  GPUUniformBuf *ubo;
  /** Compilation status. Do not use if shader is not GPU_MAT_SUCCESS. */
  eGPUMaterialStatus status;
  /** Some flags about the nodetree & the needed resources. */
  eGPUMaterialFlag flag;
  /* Identify shader variations (shadow, probe, world background...).
   * Should be unique even across render engines. */
  uint64_t uuid;
  /* Number of generated function. */
  int generated_function_len;
  /** Object type for attribute fetching. */
  bool is_volume_shader;

  /** DEPRECATED Currently only used for deferred compilation. */
  Scene *scene;
  /** Source material, might be null. */
  Material *ma;
  /** 1D Texture array containing all color bands. */
  GPUTexture *coba_tex;
  /** Builder for coba_tex. */
  GPUColorBandBuilder *coba_builder;
  /* Low level node graph(s). Also contains resources needed by the material. */
  GPUNodeGraph graph;

  /** DEPRECATED: To remove. */
  bool has_surface_output;
  bool has_volume_output;
  /** DEPRECATED: To remove. */
  GPUUniformBuf *sss_profile;  /* UBO containing SSS profile. */
  GPUTexture *sss_tex_profile; /* Texture containing SSS profile. */
  bool sss_enabled;
  float sss_radii[3];
  int sss_samples;
  bool sss_dirty;

  uint32_t refcount;

#ifndef NDEBUG
  char name[64];
#endif
};

/* Functions */

GPUTexture **gpu_material_ramp_texture_row_set(GPUMaterial *mat,
                                               int size,
                                               float *pixels,
                                               float *row)
{
  /* In order to put all the color-bands into one 1D array texture,
   * we need them to be the same size. */
  BLI_assert(size == CM_TABLE + 1);
  UNUSED_VARS_NDEBUG(size);

  if (mat->coba_builder == NULL) {
    mat->coba_builder = MEM_mallocN(sizeof(GPUColorBandBuilder), "GPUColorBandBuilder");
    mat->coba_builder->current_layer = 0;
  }

  int layer = mat->coba_builder->current_layer;
  *row = (float)layer;

  if (*row == MAX_COLOR_BAND) {
    printf("Too many color band in shader! Remove some Curve, Black Body or Color Ramp Node.\n");
  }
  else {
    float *dst = (float *)mat->coba_builder->pixels[layer];
    memcpy(dst, pixels, sizeof(float) * (CM_TABLE + 1) * 4);
    mat->coba_builder->current_layer += 1;
  }

  return &mat->coba_tex;
}

static void gpu_material_ramp_texture_build(GPUMaterial *mat)
{
  if (mat->coba_builder == NULL) {
    return;
  }

  GPUColorBandBuilder *builder = mat->coba_builder;

  mat->coba_tex = GPU_texture_create_1d_array(
      "mat_ramp", CM_TABLE + 1, builder->current_layer, 1, GPU_RGBA16F, (float *)builder->pixels);

  MEM_freeN(builder);
  mat->coba_builder = NULL;
}

static void gpu_material_free_single(GPUMaterial *material)
{
  bool do_free = atomic_sub_and_fetch_uint32(&material->refcount, 1) == 0;
  if (!do_free) {
    return;
  }

  gpu_node_graph_free(&material->graph);

  if (material->pass != NULL) {
    GPU_pass_release(material->pass);
  }
  if (material->ubo != NULL) {
    GPU_uniformbuf_free(material->ubo);
  }
  if (material->coba_tex != NULL) {
    GPU_texture_free(material->coba_tex);
  }
  if (material->sss_profile != NULL) {
    GPU_uniformbuf_free(material->sss_profile);
  }
  if (material->sss_tex_profile != NULL) {
    GPU_texture_free(material->sss_tex_profile);
  }
  MEM_freeN(material);
}

void GPU_material_free(ListBase *gpumaterial)
{
  LISTBASE_FOREACH (LinkData *, link, gpumaterial) {
    GPUMaterial *material = link->data;
    DRW_deferred_shader_remove(material);
    gpu_material_free_single(material);
  }
  BLI_freelistN(gpumaterial);
}

Scene *GPU_material_scene(GPUMaterial *material)
{
  return material->scene;
}

GPUPass *GPU_material_get_pass(GPUMaterial *material)
{
  return material->pass;
}

GPUShader *GPU_material_get_shader(GPUMaterial *material)
{
  return material->pass ? GPU_pass_shader_get(material->pass) : NULL;
}

Material *GPU_material_get_material(GPUMaterial *material)
{
  return material->ma;
}

GPUUniformBuf *GPU_material_uniform_buffer_get(GPUMaterial *material)
{
  return material->ubo;
}

void GPU_material_uniform_buffer_create(GPUMaterial *material, ListBase *inputs)
{
#ifndef NDEBUG
  const char *name = material->name;
#else
  const char *name = "Material";
#endif
  material->ubo = GPU_uniformbuf_create_from_list(inputs, name);
}

ListBase GPU_material_attributes(GPUMaterial *material)
{
  return material->graph.attributes;
}

ListBase GPU_material_textures(GPUMaterial *material)
{
  return material->graph.textures;
}

GPUUniformAttrList *GPU_material_uniform_attributes(GPUMaterial *material)
{
  GPUUniformAttrList *attrs = &material->graph.uniform_attrs;
  return attrs->count > 0 ? attrs : NULL;
}

#if 1 /* End of life code. */
/* Eevee Subsurface scattering. */
/* Based on Separable SSS. by Jorge Jimenez and Diego Gutierrez */

#  define SSS_SAMPLES 65
#  define SSS_EXPONENT 2.0f /* Importance sampling exponent */

typedef struct GPUSssKernelData {
  float kernel[SSS_SAMPLES][4];
  float param[3], max_radius;
  float avg_inv_radius;
  int samples;
  int pad[2];
} GPUSssKernelData;

BLI_STATIC_ASSERT_ALIGN(GPUSssKernelData, 16)

static void sss_calculate_offsets(GPUSssKernelData *kd, int count, float exponent)
{
  float step = 2.0f / (float)(count - 1);
  for (int i = 0; i < count; i++) {
    float o = ((float)i) * step - 1.0f;
    float sign = (o < 0.0f) ? -1.0f : 1.0f;
    float ofs = sign * fabsf(powf(o, exponent));
    kd->kernel[i][3] = ofs;
  }
}

#  define BURLEY_TRUNCATE 16.0f
#  define BURLEY_TRUNCATE_CDF 0.9963790093708328f  // cdf(BURLEY_TRUNCATE)
static float burley_profile(float r, float d)
{
  float exp_r_3_d = expf(-r / (3.0f * d));
  float exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
  return (exp_r_d + exp_r_3_d) / (4.0f * d);
}

static float eval_profile(float r, float param)
{
  r = fabsf(r);
  return burley_profile(r, param) / BURLEY_TRUNCATE_CDF;
}

/* Resolution for each sample of the precomputed kernel profile */
#  define INTEGRAL_RESOLUTION 32
static float eval_integral(float x0, float x1, float param)
{
  const float range = x1 - x0;
  const float step = range / INTEGRAL_RESOLUTION;
  float integral = 0.0f;

  for (int i = 0; i < INTEGRAL_RESOLUTION; i++) {
    float x = x0 + range * ((float)i + 0.5f) / (float)INTEGRAL_RESOLUTION;
    float y = eval_profile(x, param);
    integral += y * step;
  }

  return integral;
}
#  undef INTEGRAL_RESOLUTION

static void compute_sss_kernel(GPUSssKernelData *kd, const float radii[3], int sample_len)
{
  float rad[3];
  /* Minimum radius */
  rad[0] = MAX2(radii[0], 1e-15f);
  rad[1] = MAX2(radii[1], 1e-15f);
  rad[2] = MAX2(radii[2], 1e-15f);

  kd->avg_inv_radius = 3.0f / (rad[0] + rad[1] + rad[2]);

  /* Christensen-Burley fitting */
  float l[3], d[3];

  mul_v3_v3fl(l, rad, 0.25f * M_1_PI);
  const float A = 1.0f;
  const float s = 1.9f - A + 3.5f * (A - 0.8f) * (A - 0.8f);
  /* XXX 0.6f Out of nowhere to match cycles! Empirical! Can be tweak better. */
  mul_v3_v3fl(d, l, 0.6f / s);
  mul_v3_v3fl(rad, d, BURLEY_TRUNCATE);
  kd->max_radius = MAX3(rad[0], rad[1], rad[2]);

  copy_v3_v3(kd->param, d);

  /* Compute samples locations on the 1d kernel [-1..1] */
  sss_calculate_offsets(kd, sample_len, SSS_EXPONENT);

  /* Weights sum for normalization */
  float sum[3] = {0.0f, 0.0f, 0.0f};

  /* Compute integral of each sample footprint */
  for (int i = 0; i < sample_len; i++) {
    float x0, x1;

    if (i == 0) {
      x0 = kd->kernel[0][3] - fabsf(kd->kernel[0][3] - kd->kernel[1][3]) / 2.0f;
    }
    else {
      x0 = (kd->kernel[i - 1][3] + kd->kernel[i][3]) / 2.0f;
    }

    if (i == sample_len - 1) {
      x1 = kd->kernel[sample_len - 1][3] +
           fabsf(kd->kernel[sample_len - 2][3] - kd->kernel[sample_len - 1][3]) / 2.0f;
    }
    else {
      x1 = (kd->kernel[i][3] + kd->kernel[i + 1][3]) / 2.0f;
    }

    x0 *= kd->max_radius;
    x1 *= kd->max_radius;

    kd->kernel[i][0] = eval_integral(x0, x1, kd->param[0]);
    kd->kernel[i][1] = eval_integral(x0, x1, kd->param[1]);
    kd->kernel[i][2] = eval_integral(x0, x1, kd->param[2]);

    sum[0] += kd->kernel[i][0];
    sum[1] += kd->kernel[i][1];
    sum[2] += kd->kernel[i][2];
  }

  for (int i = 0; i < 3; i++) {
    if (sum[i] > 0.0f) {
      /* Normalize */
      for (int j = 0; j < sample_len; j++) {
        kd->kernel[j][i] /= sum[i];
      }
    }
    else {
      /* Avoid 0 kernel sum. */
      kd->kernel[sample_len / 2][i] = 1.0f;
    }
  }

  /* Put center sample at the start of the array (to sample first) */
  float tmpv[4];
  copy_v4_v4(tmpv, kd->kernel[sample_len / 2]);
  for (int i = sample_len / 2; i > 0; i--) {
    copy_v4_v4(kd->kernel[i], kd->kernel[i - 1]);
  }
  copy_v4_v4(kd->kernel[0], tmpv);

  kd->samples = sample_len;
}

#  define INTEGRAL_RESOLUTION 512
static void compute_sss_translucence_kernel(const GPUSssKernelData *kd,
                                            int resolution,
                                            float **output)
{
  float(*texels)[4];
  texels = MEM_callocN(sizeof(float[4]) * resolution, "compute_sss_translucence_kernel");
  *output = (float *)texels;

  /* Last texel should be black, hence the - 1. */
  for (int i = 0; i < resolution - 1; i++) {
    /* Distance from surface. */
    float d = kd->max_radius * ((float)i + 0.00001f) / ((float)resolution);

    /* For each distance d we compute the radiance incoming from an hypothetical parallel plane. */
    /* Compute radius of the footprint on the hypothetical plane. */
    float r_fp = sqrtf(kd->max_radius * kd->max_radius - d * d);
    float r_step = r_fp / INTEGRAL_RESOLUTION;
    float area_accum = 0.0f;
    for (float r = 0.0f; r < r_fp; r += r_step) {
      /* Compute distance to the "shading" point through the medium. */
      /* r_step * 0.5f to put sample between the area borders */
      float dist = hypotf(r + r_step * 0.5f, d);

      float profile[3];
      profile[0] = eval_profile(dist, kd->param[0]);
      profile[1] = eval_profile(dist, kd->param[1]);
      profile[2] = eval_profile(dist, kd->param[2]);

      /* Since the profile and configuration are radially symmetrical we
       * can just evaluate it once and weight it accordingly */
      float r_next = r + r_step;
      float disk_area = (M_PI * r_next * r_next) - (M_PI * r * r);

      mul_v3_fl(profile, disk_area);
      add_v3_v3(texels[i], profile);
      area_accum += disk_area;
    }
    /* Normalize over the disk. */
    mul_v3_fl(texels[i], 1.0f / (area_accum));
  }

  /* Normalize */
  for (int j = resolution - 2; j > 0; j--) {
    texels[j][0] /= (texels[0][0] > 0.0f) ? texels[0][0] : 1.0f;
    texels[j][1] /= (texels[0][1] > 0.0f) ? texels[0][1] : 1.0f;
    texels[j][2] /= (texels[0][2] > 0.0f) ? texels[0][2] : 1.0f;
  }

  /* First texel should be white */
  texels[0][0] = (texels[0][0] > 0.0f) ? 1.0f : 0.0f;
  texels[0][1] = (texels[0][1] > 0.0f) ? 1.0f : 0.0f;
  texels[0][2] = (texels[0][2] > 0.0f) ? 1.0f : 0.0f;

  /* dim the last few texels for smoother transition */
  mul_v3_fl(texels[resolution - 2], 0.25f);
  mul_v3_fl(texels[resolution - 3], 0.5f);
  mul_v3_fl(texels[resolution - 4], 0.75f);
}
#  undef INTEGRAL_RESOLUTION

bool GPU_material_sss_profile_create(GPUMaterial *material, float radii[3])
{
  /* Enable only once. */
  if (material->sss_enabled) {
    return false;
  }
  copy_v3_v3(material->sss_radii, radii);
  material->sss_dirty = true;
  material->sss_enabled = true;

  /* Update / Create UBO */
  if (material->sss_profile == NULL) {
    material->sss_profile = GPU_uniformbuf_create(sizeof(GPUSssKernelData));
  }
  return true;
}

struct GPUUniformBuf *GPU_material_sss_profile_get(GPUMaterial *material,
                                                   int sample_len,
                                                   GPUTexture **tex_profile)
{
  if (!material->sss_enabled) {
    return NULL;
  }

  if (material->sss_dirty || (material->sss_samples != sample_len)) {
    GPUSssKernelData kd;

    compute_sss_kernel(&kd, material->sss_radii, sample_len);

    /* Update / Create UBO */
    GPU_uniformbuf_update(material->sss_profile, &kd);

    /* Update / Create Tex */
    float *translucence_profile;
    compute_sss_translucence_kernel(&kd, 64, &translucence_profile);

    if (material->sss_tex_profile != NULL) {
      GPU_texture_free(material->sss_tex_profile);
    }

    material->sss_tex_profile = GPU_texture_create_1d(
        "sss_tex_profile", 64, 1, GPU_RGBA16F, translucence_profile);

    MEM_freeN(translucence_profile);

    material->sss_samples = sample_len;
    material->sss_dirty = false;
  }

  if (tex_profile != NULL) {
    *tex_profile = material->sss_tex_profile;
  }
  return material->sss_profile;
}

struct GPUUniformBuf *GPU_material_create_sss_profile_ubo(void)
{
  return GPU_uniformbuf_create(sizeof(GPUSssKernelData));
}

#  undef SSS_EXPONENT
#  undef SSS_SAMPLES
#endif

void GPU_material_output_surface(GPUMaterial *material, GPUNodeLink *link)
{
  if (!material->graph.outlink_surface) {
    material->graph.outlink_surface = link;
    material->has_surface_output = true;
  }
}

void GPU_material_output_volume(GPUMaterial *material, GPUNodeLink *link)
{
  if (!material->graph.outlink_volume) {
    material->graph.outlink_volume = link;
    material->has_volume_output = true;
  }
}

void GPU_material_output_displacement(GPUMaterial *material, GPUNodeLink *link)
{
  if (!material->graph.outlink_displacement) {
    material->graph.outlink_displacement = link;
  }
}

void GPU_material_output_thickness(GPUMaterial *material, GPUNodeLink *link)
{
  if (!material->graph.outlink_thickness) {
    material->graph.outlink_thickness = link;
  }
}

void GPU_material_add_output_link_aov(GPUMaterial *material, GPUNodeLink *link, int hash)
{
  GPUNodeGraphOutputLink *aov_link = MEM_callocN(sizeof(GPUNodeGraphOutputLink), __func__);
  aov_link->outlink = link;
  aov_link->hash = hash;
  BLI_addtail(&material->graph.outlink_aovs, aov_link);
}

char *GPU_material_split_sub_function(GPUMaterial *material,
                                      eGPUType return_type,
                                      GPUNodeLink **link)
{
  /* Force cast to return type. */
  switch (return_type) {
    case GPU_FLOAT:
      GPU_link(material, "set_value", *link, link);
      break;
    case GPU_VEC3:
      GPU_link(material, "set_rgb", *link, link);
      break;
    case GPU_VEC4:
      GPU_link(material, "set_rgba", *link, link);
      break;
    default:
      BLI_assert(0);
      break;
  }

  GPUNodeGraphFunctionLink *func_link = MEM_callocN(sizeof(GPUNodeGraphFunctionLink), __func__);
  func_link->outlink = *link;
  SNPRINTF(func_link->name, "ntree_fn%d", material->generated_function_len++);
  BLI_addtail(&material->graph.material_functions, func_link);

  /* Set value to break the link with the main graph. */
  switch (return_type) {
    case GPU_FLOAT:
      GPU_link(material, "set_value_one", link);
      break;
    case GPU_VEC3:
      GPU_link(material, "set_rgb_one", link);
      break;
    case GPU_VEC4:
      GPU_link(material, "set_rgba_one", link);
      break;
    default:
      BLI_assert(0);
      break;
  }
  return func_link->name;
}

GPUNodeGraph *gpu_material_node_graph(GPUMaterial *material)
{
  return &material->graph;
}

eGPUMaterialStatus GPU_material_status(GPUMaterial *mat)
{
  return mat->status;
}

void GPU_material_status_set(GPUMaterial *mat, eGPUMaterialStatus status)
{
  mat->status = status;
}

/* Code generation */

bool GPU_material_has_surface_output(GPUMaterial *mat)
{
  return mat->has_surface_output;
}

bool GPU_material_has_volume_output(GPUMaterial *mat)
{
  return mat->has_volume_output;
}

void GPU_material_flag_set(GPUMaterial *mat, eGPUMaterialFlag flag)
{
  mat->flag |= flag;
}

bool GPU_material_flag_get(const GPUMaterial *mat, eGPUMaterialFlag flag)
{
  return (mat->flag & flag) != 0;
}

eGPUMaterialFlag GPU_material_flag(const GPUMaterial *mat)
{
  return mat->flag;
}

/* NOTE: Consumes the flags. */
bool GPU_material_recalc_flag_get(GPUMaterial *mat)
{
  bool updated = (mat->flag & GPU_MATFLAG_UPDATED) != 0;
  mat->flag &= ~GPU_MATFLAG_UPDATED;
  return updated;
}

uint64_t GPU_material_uuid_get(GPUMaterial *mat)
{
  return mat->uuid;
}

GPUMaterial *GPU_material_from_nodetree(Scene *scene,
                                        Material *ma,
                                        bNodeTree *ntree,
                                        ListBase *gpumaterials,
                                        const char *name,
                                        uint64_t shader_uuid,
                                        bool is_volume_shader,
                                        bool is_lookdev,
                                        GPUCodegenCallbackFn callback,
                                        void *thunk)
{
  /* Search if this material is not already compiled. */
  LISTBASE_FOREACH (LinkData *, link, gpumaterials) {
    GPUMaterial *mat = (GPUMaterial *)link->data;
    if (mat->uuid == shader_uuid) {
      return mat;
    }
  }

  GPUMaterial *mat = MEM_callocN(sizeof(GPUMaterial), "GPUMaterial");
  mat->ma = ma;
  mat->scene = scene;
  mat->uuid = shader_uuid;
  mat->flag = GPU_MATFLAG_UPDATED;
  mat->status = GPU_MAT_CREATED;
  mat->is_volume_shader = is_volume_shader;
  mat->graph.used_libraries = BLI_gset_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "GPUNodeGraph.used_libraries");
  mat->refcount = 1;
#ifndef NDEBUG
  STRNCPY(mat->name, name);
#else
  UNUSED_VARS(name);
#endif
  if (is_lookdev) {
    mat->flag |= GPU_MATFLAG_LOOKDEV_HACK;
  }

  /* Localize tree to create links for reroute and mute. */
  bNodeTree *localtree = ntreeLocalize(ntree);
  ntreeGPUMaterialNodes(localtree, mat);

  gpu_material_ramp_texture_build(mat);

  {
    /* Create source code and search pass cache for an already compiled version. */
    mat->pass = GPU_generate_pass(mat, &mat->graph, callback, thunk);

    if (mat->pass == NULL) {
      /* We had a cache hit and the shader has already failed to compile. */
      mat->status = GPU_MAT_FAILED;
      gpu_node_graph_free(&mat->graph);
    }
    else {
      GPUShader *sh = GPU_pass_shader_get(mat->pass);
      if (sh != NULL) {
        /* We had a cache hit and the shader is already compiled. */
        mat->status = GPU_MAT_SUCCESS;
        gpu_node_graph_free_nodes(&mat->graph);
      }
    }
  }

  /* Only free after GPU_pass_shader_get where GPUUniformBuf read data from the local tree. */
  ntreeFreeLocalTree(localtree);
  BLI_assert(!localtree->id.py_instance); /* Or call #BKE_libblock_free_data_py. */
  MEM_freeN(localtree);

  /* Note that even if building the shader fails in some way, we still keep
   * it to avoid trying to compile again and again, and simply do not use
   * the actual shader on drawing. */
  LinkData *link = MEM_callocN(sizeof(LinkData), "GPUMaterialLink");
  link->data = mat;
  BLI_addtail(gpumaterials, link);

  return mat;
}

void GPU_material_acquire(GPUMaterial *mat)
{
  atomic_add_and_fetch_uint32(&mat->refcount, 1);
}

void GPU_material_release(GPUMaterial *mat)
{
  gpu_material_free_single(mat);
}

void GPU_material_compile(GPUMaterial *mat)
{
  bool success;

  BLI_assert(ELEM(mat->status, GPU_MAT_QUEUED, GPU_MAT_CREATED));
  BLI_assert(mat->pass);

  /* NOTE: The shader may have already been compiled here since we are
   * sharing GPUShader across GPUMaterials. In this case it's a no-op. */
#ifndef NDEBUG
  success = GPU_pass_compile(mat->pass, mat->name);
#else
  success = GPU_pass_compile(mat->pass, __func__);
#endif

  mat->flag |= GPU_MATFLAG_UPDATED;

  if (success) {
    GPUShader *sh = GPU_pass_shader_get(mat->pass);
    if (sh != NULL) {
      mat->status = GPU_MAT_SUCCESS;
      gpu_node_graph_free_nodes(&mat->graph);
    }
    else {
      mat->status = GPU_MAT_FAILED;
    }
  }
  else {
    mat->status = GPU_MAT_FAILED;
    GPU_pass_release(mat->pass);
    mat->pass = NULL;
    gpu_node_graph_free(&mat->graph);
  }
}

void GPU_materials_free(Main *bmain)
{
  LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
    GPU_material_free(&ma->gpumaterial);
  }

  LISTBASE_FOREACH (World *, wo, &bmain->worlds) {
    GPU_material_free(&wo->gpumaterial);
  }

  // BKE_world_defaults_free_gpu();
  BKE_material_defaults_free_gpu();
}

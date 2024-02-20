/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * Manages materials, lights and textures.
 */

#include <algorithm>
#include <cmath>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_time.h"
#include "BLI_utildefines.h"

#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_node.hh"

#include "NOD_shader.h"

#include "GPU_material.hh"
#include "GPU_shader.h"
#include "GPU_texture.h"
#include "GPU_uniform_buffer.h"

#include "DRW_engine.hh"

#include "gpu_codegen.h"
#include "gpu_node_graph.h"

#include "atomic_ops.h"

/* Structs */
#define MAX_COLOR_BAND 128
#define MAX_GPU_SKIES 8

/**
 * Whether the optimized variant of the GPUPass should be created asynchronously.
 * Usage of this depends on whether there are possible threading challenges of doing so.
 * Currently, the overhead of GPU_generate_pass is relatively small in comparison to shader
 * compilation, though this option exists in case any potential scenarios for material graph
 * optimization cause a slow down on the main thread.
 *
 * NOTE: The actual shader program for the optimized pass will always be compiled asynchronously,
 * this flag controls whether shader node graph source serialization happens on the compilation
 * worker thread as well. */
#define ASYNC_OPTIMIZED_PASS_CREATION 0

struct GPUColorBandBuilder {
  float pixels[MAX_COLOR_BAND][CM_TABLE + 1][4];
  int current_layer;
};

struct GPUSkyBuilder {
  float pixels[MAX_GPU_SKIES][GPU_SKY_WIDTH * GPU_SKY_HEIGHT][4];
  int current_layer;
};

struct GPUMaterial {
  /* Contains #GPUShader and source code for deferred compilation.
   * Can be shared between similar material (i.e: sharing same node-tree topology). */
  GPUPass *pass;
  /* Optimized GPUPass, situationally compiled after initial pass for optimal realtime performance.
   * This shader variant bakes dynamic uniform data as constant. This variant will not use
   * the ubo, and instead bake constants directly into the shader source. */
  GPUPass *optimized_pass;
  /* Optimization status.
   * We also use this status to determine whether this material should be considered for
   * optimization. Only sufficiently complex shaders benefit from constant-folding optimizations.
   *   `GPU_MAT_OPTIMIZATION_READY` -> shader should be optimized and is ready for optimization.
   *   `GPU_MAT_OPTIMIZATION_SKIP` -> Shader should not be optimized as it would not benefit
   * performance to do so, based on the heuristic.
   */
  eGPUMaterialOptimizationStatus optimization_status;
  double creation_time;
#if ASYNC_OPTIMIZED_PASS_CREATION == 1
  struct DeferredOptimizePass {
    GPUCodegenCallbackFn callback;
    void *thunk;
  } DeferredOptimizePass;
  struct DeferredOptimizePass optimize_pass_info;
#endif

  /** UBOs for this material parameters. */
  GPUUniformBuf *ubo;
  /** Compilation status. Do not use if shader is not GPU_MAT_SUCCESS. */
  eGPUMaterialStatus status;
  /** Some flags about the nodetree & the needed resources. */
  eGPUMaterialFlag flag;
  /** The engine type this material is compiled for. */
  eGPUMaterialEngine engine;
  /* Identify shader variations (shadow, probe, world background...) */
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
  /** 2D Texture array containing all sky textures. */
  GPUTexture *sky_tex;
  /** Builder for sky_tex. */
  GPUSkyBuilder *sky_builder;
  /* Low level node graph(s). Also contains resources needed by the material. */
  GPUNodeGraph graph;

  /** Default material reference used for PSO cache warming. Default materials may perform
   * different operations, but the permutation will frequently share the same input PSO
   * descriptors. This enables asynchronous PSO compilation as part of the deferred compilation
   * pass, reducing runtime stuttering and responsiveness while compiling materials. */
  GPUMaterial *default_mat;

  /** DEPRECATED: To remove. */
  bool has_surface_output;
  bool has_volume_output;
  bool has_displacement_output;
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
#else
  char name[16];
#endif
};

/* Functions */

GPUTexture **gpu_material_sky_texture_layer_set(
    GPUMaterial *mat, int width, int height, const float *pixels, float *row)
{
  /* In order to put all sky textures into one 2D array texture,
   * we need them to be the same size. */
  BLI_assert(width == GPU_SKY_WIDTH);
  BLI_assert(height == GPU_SKY_HEIGHT);
  UNUSED_VARS_NDEBUG(width, height);

  if (mat->sky_builder == nullptr) {
    mat->sky_builder = static_cast<GPUSkyBuilder *>(
        MEM_mallocN(sizeof(GPUSkyBuilder), "GPUSkyBuilder"));
    mat->sky_builder->current_layer = 0;
  }

  int layer = mat->sky_builder->current_layer;
  *row = float(layer);

  if (*row == MAX_GPU_SKIES) {
    printf("Too many sky textures in shader!\n");
  }
  else {
    float *dst = (float *)mat->sky_builder->pixels[layer];
    memcpy(dst, pixels, sizeof(float) * GPU_SKY_WIDTH * GPU_SKY_HEIGHT * 4);
    mat->sky_builder->current_layer += 1;
  }

  return &mat->sky_tex;
}

GPUTexture **gpu_material_ramp_texture_row_set(GPUMaterial *mat,
                                               int size,
                                               float *pixels,
                                               float *row)
{
  /* In order to put all the color-bands into one 1D array texture,
   * we need them to be the same size. */
  BLI_assert(size == CM_TABLE + 1);
  UNUSED_VARS_NDEBUG(size);

  if (mat->coba_builder == nullptr) {
    mat->coba_builder = static_cast<GPUColorBandBuilder *>(
        MEM_mallocN(sizeof(GPUColorBandBuilder), "GPUColorBandBuilder"));
    mat->coba_builder->current_layer = 0;
  }

  int layer = mat->coba_builder->current_layer;
  *row = float(layer);

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
  if (mat->coba_builder == nullptr) {
    return;
  }

  GPUColorBandBuilder *builder = mat->coba_builder;

  mat->coba_tex = GPU_texture_create_1d_array("mat_ramp",
                                              CM_TABLE + 1,
                                              builder->current_layer,
                                              1,
                                              GPU_RGBA16F,
                                              GPU_TEXTURE_USAGE_SHADER_READ,
                                              (float *)builder->pixels);

  MEM_freeN(builder);
  mat->coba_builder = nullptr;
}

static void gpu_material_sky_texture_build(GPUMaterial *mat)
{
  if (mat->sky_builder == nullptr) {
    return;
  }

  mat->sky_tex = GPU_texture_create_2d_array("mat_sky",
                                             GPU_SKY_WIDTH,
                                             GPU_SKY_HEIGHT,
                                             mat->sky_builder->current_layer,
                                             1,
                                             GPU_RGBA32F,
                                             GPU_TEXTURE_USAGE_SHADER_READ,
                                             (float *)mat->sky_builder->pixels);

  MEM_freeN(mat->sky_builder);
  mat->sky_builder = nullptr;
}

void GPU_material_free_single(GPUMaterial *material)
{
  bool do_free = atomic_sub_and_fetch_uint32(&material->refcount, 1) == 0;
  if (!do_free) {
    return;
  }

  gpu_node_graph_free(&material->graph);

  if (material->optimized_pass != nullptr) {
    GPU_pass_release(material->optimized_pass);
  }
  if (material->pass != nullptr) {
    GPU_pass_release(material->pass);
  }
  if (material->ubo != nullptr) {
    GPU_uniformbuf_free(material->ubo);
  }
  if (material->coba_tex != nullptr) {
    GPU_texture_free(material->coba_tex);
  }
  if (material->sky_tex != nullptr) {
    GPU_texture_free(material->sky_tex);
  }
  if (material->sss_profile != nullptr) {
    GPU_uniformbuf_free(material->sss_profile);
  }
  if (material->sss_tex_profile != nullptr) {
    GPU_texture_free(material->sss_tex_profile);
  }
  MEM_freeN(material);
}

void GPU_material_free(ListBase *gpumaterial)
{
  LISTBASE_FOREACH (LinkData *, link, gpumaterial) {
    GPUMaterial *material = static_cast<GPUMaterial *>(link->data);
    DRW_deferred_shader_remove(material);
    GPU_material_free_single(material);
  }
  BLI_freelistN(gpumaterial);
}

Scene *GPU_material_scene(GPUMaterial *material)
{
  return material->scene;
}

GPUPass *GPU_material_get_pass(GPUMaterial *material)
{
  /* If an optimized pass variant is available, and optimization is
   * flagged as complete, we use this one instead. */
  return ((GPU_material_optimization_status(material) == GPU_MAT_OPTIMIZATION_SUCCESS) &&
          material->optimized_pass) ?
             material->optimized_pass :
             material->pass;
}

GPUShader *GPU_material_get_shader(GPUMaterial *material)
{
  /* If an optimized material shader variant is available, and optimization is
   * flagged as complete, we use this one instead. */
  GPUShader *shader = ((GPU_material_optimization_status(material) ==
                        GPU_MAT_OPTIMIZATION_SUCCESS) &&
                       material->optimized_pass) ?
                          GPU_pass_shader_get(material->optimized_pass) :
                          nullptr;
  return (shader) ? shader : ((material->pass) ? GPU_pass_shader_get(material->pass) : nullptr);
}

GPUShader *GPU_material_get_shader_base(GPUMaterial *material)
{
  return (material->pass) ? GPU_pass_shader_get(material->pass) : nullptr;
}

const char *GPU_material_get_name(GPUMaterial *material)
{
  return material->name;
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
  material->ubo = GPU_uniformbuf_create_from_list(inputs, material->name);
}

ListBase GPU_material_attributes(const GPUMaterial *material)
{
  return material->graph.attributes;
}

ListBase GPU_material_textures(GPUMaterial *material)
{
  return material->graph.textures;
}

const GPUUniformAttrList *GPU_material_uniform_attributes(const GPUMaterial *material)
{
  const GPUUniformAttrList *attrs = &material->graph.uniform_attrs;
  return attrs->count > 0 ? attrs : nullptr;
}

const ListBase *GPU_material_layer_attributes(const GPUMaterial *material)
{
  const ListBase *attrs = &material->graph.layer_attrs;
  return !BLI_listbase_is_empty(attrs) ? attrs : nullptr;
}

#if 1 /* End of life code. */
/* Eevee Subsurface scattering. */
/* Based on Separable SSS. by Jorge Jimenez and Diego Gutierrez */

#  define SSS_SAMPLES 65
#  define SSS_EXPONENT 2.0f /* Importance sampling exponent */

struct GPUSssKernelData {
  float kernel[SSS_SAMPLES][4];
  float param[3], max_radius;
  float avg_inv_radius;
  int samples;
  int pad[2];
};

BLI_STATIC_ASSERT_ALIGN(GPUSssKernelData, 16)

static void sss_calculate_offsets(GPUSssKernelData *kd, int count, float exponent)
{
  float step = 2.0f / float(count - 1);
  for (int i = 0; i < count; i++) {
    float o = float(i) * step - 1.0f;
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
    float x = x0 + range * (float(i) + 0.5f) / float(INTEGRAL_RESOLUTION);
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
  rad[0] = std::max(radii[0], 1e-15f);
  rad[1] = std::max(radii[1], 1e-15f);
  rad[2] = std::max(radii[2], 1e-15f);

  kd->avg_inv_radius = 3.0f / (rad[0] + rad[1] + rad[2]);

  /* Christensen-Burley fitting */
  float l[3], d[3];

  mul_v3_v3fl(l, rad, 0.25f * M_1_PI);
  const float A = 1.0f;
  const float s = 1.9f - A + 3.5f * (A - 0.8f) * (A - 0.8f);
  /* XXX 0.6f Out of nowhere to match cycles! Empirical! Can be tweak better. */
  mul_v3_v3fl(d, l, 0.6f / s);
  mul_v3_v3fl(rad, d, BURLEY_TRUNCATE);
  kd->max_radius = std::max({rad[0], rad[1], rad[2]});

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
  texels = static_cast<float(*)[4]>(
      MEM_callocN(sizeof(float[4]) * resolution, "compute_sss_translucence_kernel"));
  *output = (float *)texels;

  /* Last texel should be black, hence the - 1. */
  for (int i = 0; i < resolution - 1; i++) {
    /* Distance from surface. */
    float d = kd->max_radius * (float(i) + 0.00001f) / float(resolution);

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
  if (material->sss_profile == nullptr) {
    material->sss_profile = GPU_uniformbuf_create(sizeof(GPUSssKernelData));
  }
  return true;
}

GPUUniformBuf *GPU_material_sss_profile_get(GPUMaterial *material,
                                            int sample_len,
                                            GPUTexture **tex_profile)
{
  if (!material->sss_enabled) {
    return nullptr;
  }

  if (material->sss_dirty || (material->sss_samples != sample_len)) {
    GPUSssKernelData kd;

    compute_sss_kernel(&kd, material->sss_radii, sample_len);

    /* Update / Create UBO */
    GPU_uniformbuf_update(material->sss_profile, &kd);

    /* Update / Create Tex */
    float *translucence_profile;
    compute_sss_translucence_kernel(&kd, 64, &translucence_profile);

    if (material->sss_tex_profile != nullptr) {
      GPU_texture_free(material->sss_tex_profile);
    }

    material->sss_tex_profile = GPU_texture_create_1d("sss_tex_profile",
                                                      64,
                                                      1,
                                                      GPU_RGBA16F,
                                                      GPU_TEXTURE_USAGE_SHADER_READ,
                                                      translucence_profile);

    MEM_freeN(translucence_profile);

    material->sss_samples = sample_len;
    material->sss_dirty = false;
  }

  if (tex_profile != nullptr) {
    *tex_profile = material->sss_tex_profile;
  }
  return material->sss_profile;
}

GPUUniformBuf *GPU_material_create_sss_profile_ubo()
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
    material->has_displacement_output = true;
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
  GPUNodeGraphOutputLink *aov_link = static_cast<GPUNodeGraphOutputLink *>(
      MEM_callocN(sizeof(GPUNodeGraphOutputLink), __func__));
  aov_link->outlink = link;
  aov_link->hash = hash;
  BLI_addtail(&material->graph.outlink_aovs, aov_link);
}

void GPU_material_add_output_link_composite(GPUMaterial *material, GPUNodeLink *link)
{
  GPUNodeGraphOutputLink *compositor_link = static_cast<GPUNodeGraphOutputLink *>(
      MEM_callocN(sizeof(GPUNodeGraphOutputLink), __func__));
  compositor_link->outlink = link;
  BLI_addtail(&material->graph.outlink_compositor, compositor_link);
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

  GPUNodeGraphFunctionLink *func_link = static_cast<GPUNodeGraphFunctionLink *>(
      MEM_callocN(sizeof(GPUNodeGraphFunctionLink), __func__));
  func_link->outlink = *link;
  SNPRINTF(func_link->name, "ntree_fn%d", material->generated_function_len++);
  BLI_addtail(&material->graph.material_functions, func_link);

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

eGPUMaterialOptimizationStatus GPU_material_optimization_status(GPUMaterial *mat)
{
  return mat->optimization_status;
}

void GPU_material_optimization_status_set(GPUMaterial *mat, eGPUMaterialOptimizationStatus status)
{
  mat->optimization_status = status;
  if (mat->optimization_status == GPU_MAT_OPTIMIZATION_READY) {
    /* Reset creation timer to delay optimization pass. */
    mat->creation_time = BLI_time_now_seconds();
  }
}

bool GPU_material_optimization_ready(GPUMaterial *mat)
{
  /* Timer threshold before optimizations will be queued.
   * When materials are frequently being modified, optimization
   * can incur CPU overhead from excessive compilation.
   *
   * As the optimization is entirely asynchronous, it is still beneficial
   * to do this quickly to avoid build-up and improve runtime performance.
   * The threshold just prevents compilations being queued frame after frame. */
  const double optimization_time_threshold_s = 1.2;
  return ((BLI_time_now_seconds() - mat->creation_time) >= optimization_time_threshold_s);
}

void GPU_material_set_default(GPUMaterial *material, GPUMaterial *default_material)
{
  if (material != default_material) {
    material->default_mat = default_material;
  }
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

bool GPU_material_has_displacement_output(GPUMaterial *mat)
{
  return mat->has_displacement_output;
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

bool GPU_material_recalc_flag_get(GPUMaterial *mat)
{
  /* NOTE: Consumes the flags. */

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
                                        eGPUMaterialEngine engine,
                                        uint64_t shader_uuid,
                                        bool is_volume_shader,
                                        bool is_lookdev,
                                        GPUCodegenCallbackFn callback,
                                        void *thunk)
{
  /* Search if this material is not already compiled. */
  LISTBASE_FOREACH (LinkData *, link, gpumaterials) {
    GPUMaterial *mat = (GPUMaterial *)link->data;
    if (mat->uuid == shader_uuid && mat->engine == engine) {
      return mat;
    }
  }

  GPUMaterial *mat = static_cast<GPUMaterial *>(MEM_callocN(sizeof(GPUMaterial), "GPUMaterial"));
  mat->ma = ma;
  mat->scene = scene;
  mat->engine = engine;
  mat->uuid = shader_uuid;
  mat->flag = GPU_MATFLAG_UPDATED;
  mat->status = GPU_MAT_CREATED;
  mat->default_mat = nullptr;
  mat->is_volume_shader = is_volume_shader;
  mat->graph.used_libraries = BLI_gset_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "GPUNodeGraph.used_libraries");
  mat->refcount = 1;
  STRNCPY(mat->name, name);
  if (is_lookdev) {
    mat->flag |= GPU_MATFLAG_LOOKDEV_HACK;
  }

  /* Localize tree to create links for reroute and mute. */
  bNodeTree *localtree = ntreeLocalize(ntree);
  ntreeGPUMaterialNodes(localtree, mat);

  gpu_material_ramp_texture_build(mat);
  gpu_material_sky_texture_build(mat);

  {
    /* Create source code and search pass cache for an already compiled version. */
    mat->pass = GPU_generate_pass(mat, &mat->graph, engine, callback, thunk, false);

    if (mat->pass == nullptr) {
      /* We had a cache hit and the shader has already failed to compile. */
      mat->status = GPU_MAT_FAILED;
      gpu_node_graph_free(&mat->graph);
    }
    else {
      /* Determine whether we should generate an optimized variant of the graph.
       * Heuristic is based on complexity of default material pass and shader node graph. */
      if (GPU_pass_should_optimize(mat->pass)) {
        GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_READY);
      }

      GPUShader *sh = GPU_pass_shader_get(mat->pass);
      if (sh != nullptr) {
        /* We had a cache hit and the shader is already compiled. */
        mat->status = GPU_MAT_SUCCESS;

        if (mat->optimization_status == GPU_MAT_OPTIMIZATION_SKIP) {
          gpu_node_graph_free_nodes(&mat->graph);
        }
      }

      /* Generate optimized pass. */
      if (mat->optimization_status == GPU_MAT_OPTIMIZATION_READY) {
#if ASYNC_OPTIMIZED_PASS_CREATION == 1
        mat->optimized_pass = nullptr;
        mat->optimize_pass_info.callback = callback;
        mat->optimize_pass_info.thunk = thunk;
#else
        mat->optimized_pass = GPU_generate_pass(mat, &mat->graph, engine, callback, thunk, true);
        if (mat->optimized_pass == nullptr) {
          /* Failed to create optimized pass. */
          gpu_node_graph_free_nodes(&mat->graph);
          GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_SKIP);
        }
        else {
          GPUShader *optimized_sh = GPU_pass_shader_get(mat->optimized_pass);
          if (optimized_sh != nullptr) {
            /* Optimized shader already available. */
            gpu_node_graph_free_nodes(&mat->graph);
            GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_SUCCESS);
          }
        }
#endif
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
  LinkData *link = static_cast<LinkData *>(MEM_callocN(sizeof(LinkData), "GPUMaterialLink"));
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
  GPU_material_free_single(mat);
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
    if (sh != nullptr) {

      /** Perform asynchronous Render Pipeline State Object (PSO) compilation.
       *
       * Warm PSO cache within asynchronous compilation thread using default material as source.
       * GPU_shader_warm_cache(..) performs the API-specific PSO compilation using the assigned
       * parent shader's cached PSO descriptors as an input.
       *
       * This is only applied if the given material has a specified default reference
       * material available, and the default material is already compiled.
       *
       * As PSOs do not always match for default shaders, we limit warming for PSO
       * configurations to ensure compile time remains fast, as these first
       * entries will be the most commonly used PSOs. As not all PSOs are necessarily
       * required immediately, this limit should remain low (1-3 at most). */
      if (!ELEM(mat->default_mat, nullptr, mat)) {
        if (mat->default_mat->pass != nullptr) {
          GPUShader *parent_sh = GPU_pass_shader_get(mat->default_mat->pass);
          if (parent_sh) {
            /* Skip warming if cached pass is identical to the default material. */
            if (mat->default_mat->pass != mat->pass && parent_sh != sh) {
              GPU_shader_set_parent(sh, parent_sh);
              GPU_shader_warm_cache(sh, 1);
            }
          }
        }
      }

      /* Flag success. */
      mat->status = GPU_MAT_SUCCESS;
      if (mat->optimization_status == GPU_MAT_OPTIMIZATION_SKIP) {
        /* Only free node graph nodes if not required by secondary optimization pass. */
        gpu_node_graph_free_nodes(&mat->graph);
      }
    }
    else {
      mat->status = GPU_MAT_FAILED;
    }
  }
  else {
    mat->status = GPU_MAT_FAILED;
    GPU_pass_release(mat->pass);
    mat->pass = nullptr;
    gpu_node_graph_free(&mat->graph);
  }
}

void GPU_material_optimize(GPUMaterial *mat)
{
  /* If shader is flagged for skipping optimization or has already been successfully
   * optimized, skip. */
  if (ELEM(mat->optimization_status, GPU_MAT_OPTIMIZATION_SKIP, GPU_MAT_OPTIMIZATION_SUCCESS)) {
    return;
  }

  /* If original shader has not been fully compiled, we are not
   * ready to perform optimization. */
  if (mat->status != GPU_MAT_SUCCESS) {
    /* Reset optimization status. */
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_READY);
    return;
  }

#if ASYNC_OPTIMIZED_PASS_CREATION == 1
  /* If the optimized pass is not valid, first generate optimized pass.
   * NOTE(Threading): Need to verify if GPU_generate_pass can cause side-effects, especially when
   * used with "thunk". So far, this appears to work, and deferring optimized pass creation is more
   * optimal, as these do not benefit from caching, due to baked constants. However, this could
   * possibly be cause for concern for certain cases. */
  if (!mat->optimized_pass) {
    mat->optimized_pass = GPU_generate_pass(mat,
                                            &mat->graph,
                                            mat->engine,
                                            mat->optimize_pass_info.callback,
                                            mat->optimize_pass_info.thunk,
                                            true);
    BLI_assert(mat->optimized_pass);
  }
#else
  if (!mat->optimized_pass) {
    /* Optimized pass has not been created, skip future optimization attempts. */
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_SKIP);
    return;
  }
#endif

  bool success;
/* NOTE: The shader may have already been compiled here since we are
 * sharing GPUShader across GPUMaterials. In this case it's a no-op. */
#ifndef NDEBUG
  success = GPU_pass_compile(mat->optimized_pass, mat->name);
#else
  success = GPU_pass_compile(mat->optimized_pass, __func__);
#endif

  if (success) {
    GPUShader *sh = GPU_pass_shader_get(mat->optimized_pass);
    if (sh != nullptr) {
      /** Perform asynchronous Render Pipeline State Object (PSO) compilation.
       *
       * Warm PSO cache within asynchronous compilation thread for optimized materials.
       * This setup assigns the original unoptimized shader as a "parent" shader
       * for the optimized version. This then allows the associated GPU backend to
       * compile PSOs within this asynchronous pass, using the identical PSO descriptors of the
       * parent shader.
       *
       * This eliminates all run-time stuttering associated with material optimization and ensures
       * realtime material editing and animation remains seamless, while retaining optimal realtime
       * performance. */
      GPUShader *parent_sh = GPU_pass_shader_get(mat->pass);
      if (parent_sh) {
        GPU_shader_set_parent(sh, parent_sh);
        GPU_shader_warm_cache(sh, -1);
      }

      /* Mark as complete. */
      GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_SUCCESS);
    }
    else {
      /* Optimized pass failed to compile. Disable any future optimization attempts. */
      GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_SKIP);
    }
  }
  else {
    /* Optimization pass generation failed. Disable future attempts to optimize. */
    GPU_pass_release(mat->optimized_pass);
    mat->optimized_pass = nullptr;
    GPU_material_optimization_status_set(mat, GPU_MAT_OPTIMIZATION_SKIP);
  }

  /* Release node graph as no longer needed. */
  gpu_node_graph_free_nodes(&mat->graph);
}

void GPU_materials_free(Main *bmain)
{
  LISTBASE_FOREACH (Material *, ma, &bmain->materials) {
    GPU_material_free(&ma->gpumaterial);
  }

  LISTBASE_FOREACH (World *, wo, &bmain->worlds) {
    GPU_material_free(&wo->gpumaterial);
  }

  BKE_material_defaults_free_gpu();
}

GPUMaterial *GPU_material_from_callbacks(eGPUMaterialEngine engine,
                                         ConstructGPUMaterialFn construct_function_cb,
                                         GPUCodegenCallbackFn generate_code_function_cb,
                                         void *thunk)
{
  /* Allocate a new material and its material graph, and initialize its reference count. */
  GPUMaterial *material = static_cast<GPUMaterial *>(
      MEM_callocN(sizeof(GPUMaterial), "GPUMaterial"));
  material->graph.used_libraries = BLI_gset_new(
      BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "GPUNodeGraph.used_libraries");
  material->refcount = 1;
  material->optimization_status = GPU_MAT_OPTIMIZATION_SKIP;
  material->optimized_pass = nullptr;
  material->default_mat = nullptr;
  material->engine = engine;

  /* Construct the material graph by adding and linking the necessary GPU material nodes. */
  construct_function_cb(thunk, material);

  /* Create and initialize the texture storing color bands used by Ramp and Curve nodes. */
  gpu_material_ramp_texture_build(material);

  /* Lookup an existing pass in the cache or generate a new one. */
  material->pass = GPU_generate_pass(
      material, &material->graph, material->engine, generate_code_function_cb, thunk, false);
  material->optimized_pass = nullptr;

  /* The pass already exists in the pass cache but its shader already failed to compile. */
  if (material->pass == nullptr) {
    material->status = GPU_MAT_FAILED;
    gpu_node_graph_free(&material->graph);
    return material;
  }

  /* The pass already exists in the pass cache and its shader is already compiled. */
  GPUShader *shader = GPU_pass_shader_get(material->pass);
  if (shader != nullptr) {
    material->status = GPU_MAT_SUCCESS;
    if (material->optimization_status == GPU_MAT_OPTIMIZATION_SKIP) {
      /* Only free node graph if not required by secondary optimization pass. */
      gpu_node_graph_free_nodes(&material->graph);
    }
    return material;
  }

  /* The material was created successfully but still needs to be compiled. */
  material->status = GPU_MAT_CREATED;
  return material;
}

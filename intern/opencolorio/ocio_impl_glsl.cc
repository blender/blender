/*
 * Adapted from OpenColorIO with this license:
 *
 * Copyright (c) 2003-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modifications Copyright 2013, Blender Foundation.
 */

#include <limits>
#include <list>
#include <sstream>
#include <string.h>
#include <vector>

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4251 4275)
#endif
#include <OpenColorIO/OpenColorIO.h>
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

#include "GPU_immediate.h"
#include "GPU_shader.h"
#include "GPU_uniform_buffer.h"

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"

extern "C" char datatoc_gpu_shader_display_transform_glsl[];
extern "C" char datatoc_gpu_shader_display_transform_vertex_glsl[];

/* **** OpenGL drawing routines using GLSL for color space transform ***** */

enum OCIO_GPUTextureSlots {
  TEXTURE_SLOT_IMAGE = 0,
  TEXTURE_SLOT_OVERLAY = 1,
  TEXTURE_SLOT_CURVE_MAPPING = 2,
  TEXTURE_SLOT_LUTS_OFFSET = 3,
};

/* Curve mapping parameters
 *
 * See documentation for OCIO_CurveMappingSettings to get fields descriptions.
 * (this ones pretty much copies stuff from C structure.)
 */
struct OCIO_GPUCurveMappingParameters {
  float curve_mapping_mintable[4];
  float curve_mapping_range[4];
  float curve_mapping_ext_in_x[4];
  float curve_mapping_ext_in_y[4];
  float curve_mapping_ext_out_x[4];
  float curve_mapping_ext_out_y[4];
  float curve_mapping_first_x[4];
  float curve_mapping_first_y[4];
  float curve_mapping_last_x[4];
  float curve_mapping_last_y[4];
  float curve_mapping_black[4];
  float curve_mapping_bwmul[4];
  int curve_mapping_lut_size;
  int curve_mapping_use_extend_extrapolate;
  int _pad[2];
  /** WARNING: Needs to be 16byte aligned. Used as UBO data. */
};

struct OCIO_GPUShader {
  /* GPU shader. */
  struct GPUShader *shader = nullptr;

  /** Uniform locations. */
  int scale_loc = 0;
  int exponent_loc = 0;
  int dither_loc = 0;
  int overlay_loc = 0;
  int predivide_loc = 0;
  int ubo_bind = 0;

  /* Destructor. */
  ~OCIO_GPUShader()
  {
    if (shader) {
      GPU_shader_free(shader);
    }
  }
};

struct OCIO_GPULutTexture {
  GPUTexture *texture = nullptr;
  std::string sampler_name;
};

struct OCIO_GPUUniform {
  GpuShaderDesc::UniformData data;
  std::string name;
};

struct OCIO_GPUTextures {
  /** LUT Textures */
  std::vector<OCIO_GPULutTexture> luts;

  /* Dummy in case of no overlay. */
  GPUTexture *dummy = nullptr;

  /* Uniforms */
  std::vector<OCIO_GPUUniform> uniforms;

  /* Destructor. */
  ~OCIO_GPUTextures()
  {
    for (OCIO_GPULutTexture &lut : luts) {
      GPU_texture_free(lut.texture);
    }
    if (dummy) {
      GPU_texture_free(dummy);
    }
  }
};

struct OCIO_GPUCurveMappping {
  /** GPU Uniform Buffer handle. 0 if not allocated. */
  GPUUniformBuf *buffer = nullptr;
  /** OpenGL Texture handles. 0 if not allocated. */
  GPUTexture *texture = nullptr;
  /* To detect when to update the uniforms and textures. */
  size_t cache_id = 0;

  /* Destructor. */
  ~OCIO_GPUCurveMappping()
  {
    if (texture) {
      GPU_texture_free(texture);
    }
    if (buffer) {
      GPU_uniformbuf_free(buffer);
    }
  }
};

struct OCIO_GPUDisplayShader {
  OCIO_GPUShader shader;
  OCIO_GPUTextures textures;
  OCIO_GPUCurveMappping curvemap;

  /* Cache variables. */
  std::string input;
  std::string view;
  std::string display;
  std::string look;
  bool use_curve_mapping = false;

  /** Error checking. */
  bool valid = false;
};

static const int SHADER_CACHE_MAX_SIZE = 4;
std::list<OCIO_GPUDisplayShader> SHADER_CACHE;

/* -------------------------------------------------------------------- */
/** \name Shader
 * \{ */

static bool createGPUShader(OCIO_GPUShader &shader,
                            OCIO_GPUTextures &textures,
                            const GpuShaderDescRcPtr &shaderdesc_to_scene_linear,
                            const GpuShaderDescRcPtr &shaderdesc_to_display,
                            const bool use_curve_mapping)
{
  std::ostringstream os;
  {
    /* Fragment shader */

    /* Work around OpenColorIO not supporting latest GLSL yet. */
    os << "#define texture2D texture\n";
    os << "#define texture3D texture\n";

    if (use_curve_mapping) {
      os << "#define USE_CURVE_MAPPING\n";
    }

    os << shaderdesc_to_scene_linear->getShaderText() << "\n";
    os << shaderdesc_to_display->getShaderText() << "\n";

    os << datatoc_gpu_shader_display_transform_glsl;
  }

  shader.shader = GPU_shader_create(datatoc_gpu_shader_display_transform_vertex_glsl,
                                    os.str().c_str(),
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    "OCIOShader");

  if (shader.shader == nullptr) {
    return false;
  }

  shader.scale_loc = GPU_shader_get_uniform(shader.shader, "scale");
  shader.exponent_loc = GPU_shader_get_uniform(shader.shader, "exponent");
  shader.dither_loc = GPU_shader_get_uniform(shader.shader, "dither");
  shader.overlay_loc = GPU_shader_get_uniform(shader.shader, "overlay");
  shader.predivide_loc = GPU_shader_get_uniform(shader.shader, "predivide");
  shader.ubo_bind = GPU_shader_get_uniform_block_binding(shader.shader,
                                                         "OCIO_GPUCurveMappingParameters");

  GPU_shader_bind(shader.shader);

  /* Set texture bind point uniform once. This is saved by the shader. */
  GPUShader *sh = shader.shader;
  GPU_shader_uniform_int(sh, GPU_shader_get_uniform(sh, "image_texture"), TEXTURE_SLOT_IMAGE);
  GPU_shader_uniform_int(sh, GPU_shader_get_uniform(sh, "overlay_texture"), TEXTURE_SLOT_OVERLAY);

  if (use_curve_mapping) {
    GPU_shader_uniform_int(
        sh, GPU_shader_get_uniform(sh, "curve_mapping_texture"), TEXTURE_SLOT_CURVE_MAPPING);
  }

  /* Set LUT textures. */
  for (int i = 0; i < textures.luts.size(); i++) {
    GPU_shader_uniform_int(sh,
                           GPU_shader_get_uniform(sh, textures.luts[i].sampler_name.c_str()),
                           TEXTURE_SLOT_LUTS_OFFSET + i);
  }

  /* Set uniforms. */
  for (OCIO_GPUUniform &uniform : textures.uniforms) {
    const GpuShaderDesc::UniformData &data = uniform.data;
    const char *name = uniform.name.c_str();

    if (data.m_getDouble) {
      GPU_shader_uniform_1f(sh, name, (float)data.m_getDouble());
    }
    else if (data.m_getBool) {
      GPU_shader_uniform_1f(sh, name, (float)(data.m_getBool() ? 1.0f : 0.0f));
    }
    else if (data.m_getFloat3) {
      GPU_shader_uniform_3f(sh,
                            name,
                            (float)data.m_getFloat3()[0],
                            (float)data.m_getFloat3()[1],
                            (float)data.m_getFloat3()[2]);
    }
    else if (data.m_vectorFloat.m_getSize && data.m_vectorFloat.m_getVector) {
      GPU_shader_uniform_vector(sh,
                                GPU_shader_get_uniform(sh, name),
                                (int)data.m_vectorFloat.m_getSize(),
                                1,
                                (float *)data.m_vectorFloat.m_getVector());
    }
    else if (data.m_vectorInt.m_getSize && data.m_vectorInt.m_getVector) {
      GPU_shader_uniform_vector_int(sh,
                                    GPU_shader_get_uniform(sh, name),
                                    (int)data.m_vectorInt.m_getSize(),
                                    1,
                                    (int *)data.m_vectorInt.m_getVector());
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Textures
 * \{ */

static bool addGPUUniform(OCIO_GPUTextures &textures,
                          const GpuShaderDescRcPtr &shader_desc,
                          int index)
{
  OCIO_GPUUniform uniform;
  uniform.name = shader_desc->getUniform(index, uniform.data);
  if (uniform.data.m_type == UNIFORM_UNKNOWN) {
    return false;
  }

  textures.uniforms.push_back(uniform);
  return true;
}

static bool addGPULut2D(OCIO_GPUTextures &textures,
                        const GpuShaderDescRcPtr &shader_desc,
                        int index)
{
  const char *texture_name = nullptr;
  const char *sampler_name = nullptr;
  unsigned int width = 0;
  unsigned int height = 0;
  GpuShaderCreator::TextureType channel = GpuShaderCreator::TEXTURE_RGB_CHANNEL;
  Interpolation interpolation = INTERP_LINEAR;
  shader_desc->getTexture(
      index, texture_name, sampler_name, width, height, channel, interpolation);

  const float *values;
  shader_desc->getTextureValues(index, values);
  if (texture_name == nullptr || sampler_name == nullptr || width == 0 || height == 0 ||
      values == nullptr) {
    return false;
  }

  eGPUTextureFormat format = (channel == GpuShaderCreator::TEXTURE_RGB_CHANNEL) ? GPU_RGB16F :
                                                                                  GPU_R16F;

  OCIO_GPULutTexture lut;
  lut.texture = GPU_texture_create_2d(texture_name, width, height, 0, format, values);
  if (lut.texture == nullptr) {
    return false;
  }

  GPU_texture_filter_mode(lut.texture, interpolation != INTERP_NEAREST);
  GPU_texture_wrap_mode(lut.texture, false, true);

  lut.sampler_name = sampler_name;

  textures.luts.push_back(lut);
  return true;
}

static bool addGPULut3D(OCIO_GPUTextures &textures,
                        const GpuShaderDescRcPtr &shader_desc,
                        int index)
{
  const char *texture_name = nullptr;
  const char *sampler_name = nullptr;
  unsigned int edgelen = 0;
  Interpolation interpolation = INTERP_LINEAR;
  shader_desc->get3DTexture(index, texture_name, sampler_name, edgelen, interpolation);

  const float *values;
  shader_desc->get3DTextureValues(index, values);
  if (texture_name == nullptr || sampler_name == nullptr || edgelen == 0 || values == nullptr) {
    return false;
  }

  OCIO_GPULutTexture lut;
  lut.texture = GPU_texture_create_3d(
      texture_name, edgelen, edgelen, edgelen, 0, GPU_RGB16F, GPU_DATA_FLOAT, values);
  if (lut.texture == nullptr) {
    return false;
  }

  GPU_texture_filter_mode(lut.texture, interpolation != INTERP_NEAREST);
  GPU_texture_wrap_mode(lut.texture, false, true);

  lut.sampler_name = sampler_name;

  textures.luts.push_back(lut);
  return true;
}

static bool createGPUTextures(OCIO_GPUTextures &textures,
                              const GpuShaderDescRcPtr &shaderdesc_to_scene_linear,
                              const GpuShaderDescRcPtr &shaderdesc_to_display)
{
  textures.dummy = GPU_texture_create_error(2, false);

  textures.luts.clear();
  textures.uniforms.clear();

  for (int index = 0; index < shaderdesc_to_scene_linear->getNumUniforms(); index++) {
    if (!addGPUUniform(textures, shaderdesc_to_scene_linear, index)) {
      return false;
    }
  }
  for (int index = 0; index < shaderdesc_to_scene_linear->getNumTextures(); index++) {
    if (!addGPULut2D(textures, shaderdesc_to_scene_linear, index)) {
      return false;
    }
  }
  for (int index = 0; index < shaderdesc_to_scene_linear->getNum3DTextures(); index++) {
    if (!addGPULut3D(textures, shaderdesc_to_scene_linear, index)) {
      return false;
    }
  }
  for (int index = 0; index < shaderdesc_to_display->getNumUniforms(); index++) {
    if (!addGPUUniform(textures, shaderdesc_to_display, index)) {
      return false;
    }
  }
  for (int index = 0; index < shaderdesc_to_display->getNumTextures(); index++) {
    if (!addGPULut2D(textures, shaderdesc_to_display, index)) {
      return false;
    }
  }
  for (int index = 0; index < shaderdesc_to_display->getNum3DTextures(); index++) {
    if (!addGPULut3D(textures, shaderdesc_to_display, index)) {
      return false;
    }
  }

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curve Mapping
 * \{ */

static bool createGPUCurveMapping(OCIO_GPUCurveMappping &curvemap,
                                  OCIO_CurveMappingSettings *curve_mapping_settings)
{
  if (curve_mapping_settings) {
    int lut_size = curve_mapping_settings->lut_size;

    curvemap.texture = GPU_texture_create_1d("OCIOCurveMap", lut_size, 1, GPU_RGBA16F, nullptr);
    GPU_texture_filter_mode(curvemap.texture, false);
    GPU_texture_wrap_mode(curvemap.texture, false, true);

    curvemap.buffer = GPU_uniformbuf_create(sizeof(OCIO_GPUCurveMappingParameters));

    if (curvemap.texture == nullptr || curvemap.buffer == nullptr) {
      return false;
    }
  }

  return true;
}

static void updateGPUCurveMapping(OCIO_GPUCurveMappping &curvemap,
                                  OCIO_CurveMappingSettings *curve_mapping_settings)
{
  /* Test if we need to update. The caller ensures the curve_mapping_settings
   * changes when its contents changes. */
  if (curve_mapping_settings == nullptr || curvemap.cache_id == curve_mapping_settings->cache_id) {
    return;
  }

  curvemap.cache_id = curve_mapping_settings->cache_id;

  /* Update texture. */
  int offset[3] = {0, 0, 0};
  int extent[3] = {curve_mapping_settings->lut_size, 0, 0};
  const float *pixels = curve_mapping_settings->lut;
  GPU_texture_update_sub(
      curvemap.texture, GPU_DATA_FLOAT, pixels, UNPACK3(offset), UNPACK3(extent));

  /* Update uniforms. */
  OCIO_GPUCurveMappingParameters data;
  for (int i = 0; i < 4; i++) {
    data.curve_mapping_range[i] = curve_mapping_settings->range[i];
    data.curve_mapping_mintable[i] = curve_mapping_settings->mintable[i];
    data.curve_mapping_ext_in_x[i] = curve_mapping_settings->ext_in_x[i];
    data.curve_mapping_ext_in_y[i] = curve_mapping_settings->ext_in_y[i];
    data.curve_mapping_ext_out_x[i] = curve_mapping_settings->ext_out_x[i];
    data.curve_mapping_ext_out_y[i] = curve_mapping_settings->ext_out_y[i];
    data.curve_mapping_first_x[i] = curve_mapping_settings->first_x[i];
    data.curve_mapping_first_y[i] = curve_mapping_settings->first_y[i];
    data.curve_mapping_last_x[i] = curve_mapping_settings->last_x[i];
    data.curve_mapping_last_y[i] = curve_mapping_settings->last_y[i];
  }
  for (int i = 0; i < 3; i++) {
    data.curve_mapping_black[i] = curve_mapping_settings->black[i];
    data.curve_mapping_bwmul[i] = curve_mapping_settings->bwmul[i];
  }
  data.curve_mapping_lut_size = curve_mapping_settings->lut_size;
  data.curve_mapping_use_extend_extrapolate = curve_mapping_settings->use_extend_extrapolate;

  GPU_uniformbuf_update(curvemap.buffer, &data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name OCIO GPU Shader Implementation
 * \{ */

bool OCIOImpl::supportGPUShader()
{
  /* Minimum supported version 3.3 does meet all requirements. */
  return true;
}

static OCIO_GPUDisplayShader &getGPUDisplayShader(
    OCIO_ConstConfigRcPtr *config,
    const char *input,
    const char *view,
    const char *display,
    const char *look,
    OCIO_CurveMappingSettings *curve_mapping_settings)
{
  /* Find existing shader in cache. */
  const bool use_curve_mapping = (curve_mapping_settings != nullptr);
  for (std::list<OCIO_GPUDisplayShader>::iterator it = SHADER_CACHE.begin();
       it != SHADER_CACHE.end();
       it++) {
    if (it->input == input && it->view == view && it->display == display && it->look == look &&
        it->use_curve_mapping == use_curve_mapping) {
      /* Move to front of the cache to mark as most recently used. */
      if (it != SHADER_CACHE.begin()) {
        SHADER_CACHE.splice(SHADER_CACHE.begin(), SHADER_CACHE, it);
      }

      return *it;
    }
  }

  /* Remove least recently used element from cache. */
  if (SHADER_CACHE.size() >= SHADER_CACHE_MAX_SIZE) {
    SHADER_CACHE.pop_back();
  }

  /* Create GPU shader. */
  SHADER_CACHE.emplace_front();
  OCIO_GPUDisplayShader &display_shader = SHADER_CACHE.front();

  display_shader.input = input;
  display_shader.view = view;
  display_shader.display = display;
  display_shader.look = look;
  display_shader.use_curve_mapping = use_curve_mapping;
  display_shader.valid = false;

  /* Create Processors.
   *
   * Scale and exponent are handled outside of OCIO shader so we can handle them
   * as uniforms at the binding stage. OCIO would otherwise bake them into the
   * shader code, requiring slow recompiles when interactively adjusting them.
   *
   * Note that OCIO does have the concept of dynamic properties, however there
   * is no dynamic gamma and exposure is part of more expensive operations only.
   *
   * Since exposure must happen in scene linear, we use two processors. The input
   * is usually scene linear already and so that conversion is often a no-op.
   */
  OCIO_ConstProcessorRcPtr *processor_to_scene_linear = OCIO_configGetProcessorWithNames(
      config, input, ROLE_SCENE_LINEAR);
  OCIO_ConstProcessorRcPtr *processor_to_display = OCIO_createDisplayProcessor(
      config, ROLE_SCENE_LINEAR, view, display, look, 1.0f, 1.0f);

  /* Create shader descriptions. */
  if (processor_to_scene_linear && processor_to_display) {
    GpuShaderDescRcPtr shaderdesc_to_scene_linear = GpuShaderDesc::CreateShaderDesc();
    shaderdesc_to_scene_linear->setLanguage(GPU_LANGUAGE_GLSL_1_3);
    shaderdesc_to_scene_linear->setFunctionName("OCIO_to_scene_linear");
    (*(ConstProcessorRcPtr *)processor_to_scene_linear)
        ->getDefaultGPUProcessor()
        ->extractGpuShaderInfo(shaderdesc_to_scene_linear);
    shaderdesc_to_scene_linear->finalize();

    GpuShaderDescRcPtr shaderdesc_to_display = GpuShaderDesc::CreateShaderDesc();
    shaderdesc_to_display->setLanguage(GPU_LANGUAGE_GLSL_1_3);
    shaderdesc_to_display->setFunctionName("OCIO_to_display");
    (*(ConstProcessorRcPtr *)processor_to_display)
        ->getDefaultGPUProcessor()
        ->extractGpuShaderInfo(shaderdesc_to_display);
    shaderdesc_to_display->finalize();

    /* Create GPU shader and textures. */
    if (createGPUTextures(
            display_shader.textures, shaderdesc_to_scene_linear, shaderdesc_to_display) &&
        createGPUCurveMapping(display_shader.curvemap, curve_mapping_settings) &&
        createGPUShader(display_shader.shader,
                        display_shader.textures,
                        shaderdesc_to_scene_linear,
                        shaderdesc_to_display,
                        use_curve_mapping)) {
      display_shader.valid = true;
    }
  }

  /* Free processors. */
  if (processor_to_scene_linear) {
    OCIO_processorRelease(processor_to_scene_linear);
  }
  if (processor_to_display) {
    OCIO_processorRelease(processor_to_display);
  }

  return display_shader;
}

/**
 * Setup GPU contexts for a transform defined by processor using GLSL.
 * All LUT allocating baking and shader compilation happens here.
 *
 * Once this function is called, callee could start drawing images
 * using regular 2D texture.
 *
 * When all drawing is finished, gpuDisplayShaderUnbind must be called to
 * restore GPU context to its previous state.
 */
bool OCIOImpl::gpuDisplayShaderBind(OCIO_ConstConfigRcPtr *config,
                                    const char *input,
                                    const char *view,
                                    const char *display,
                                    const char *look,
                                    OCIO_CurveMappingSettings *curve_mapping_settings,
                                    const float scale,
                                    const float exponent,
                                    const float dither,
                                    const bool use_predivide,
                                    const bool use_overlay)
{
  /* Get GPU shader from cache or create new one. */
  OCIO_GPUDisplayShader &display_shader = getGPUDisplayShader(
      config, input, view, display, look, curve_mapping_settings);
  if (!display_shader.valid) {
    return false;
  }

  /* Verify the shader is valid. */
  OCIO_GPUTextures &textures = display_shader.textures;
  OCIO_GPUShader &shader = display_shader.shader;
  OCIO_GPUCurveMappping &curvemap = display_shader.curvemap;

  /* Update and bind curve mapping data. */
  if (curve_mapping_settings) {
    updateGPUCurveMapping(curvemap, curve_mapping_settings);
    GPU_uniformbuf_bind(curvemap.buffer, shader.ubo_bind);
    GPU_texture_bind(curvemap.texture, TEXTURE_SLOT_CURVE_MAPPING);
  }

  /* Bind textures to sampler units. Texture 0 is set by caller.
   * Uniforms have already been set for texture bind points.*/
  if (!use_overlay) {
    /* Avoid missing binds. */
    GPU_texture_bind(textures.dummy, TEXTURE_SLOT_OVERLAY);
  }
  for (int i = 0; i < textures.luts.size(); i++) {
    GPU_texture_bind(textures.luts[i].texture, TEXTURE_SLOT_LUTS_OFFSET + i);
  }

  /* TODO(fclem): remove remains of IMM. */
  immBindShader(shader.shader);

  /* Bind Shader and set uniforms. */
  // GPU_shader_bind(shader.shader);
  GPU_shader_uniform_float(shader.shader, shader.scale_loc, scale);
  GPU_shader_uniform_float(shader.shader, shader.exponent_loc, exponent);
  GPU_shader_uniform_float(shader.shader, shader.dither_loc, dither);
  GPU_shader_uniform_int(shader.shader, shader.overlay_loc, use_overlay);
  GPU_shader_uniform_int(shader.shader, shader.predivide_loc, use_predivide);

  return true;
}

void OCIOImpl::gpuDisplayShaderUnbind()
{
  immUnbindProgram();
}

void OCIOImpl::gpuCacheFree()
{
  SHADER_CACHE.clear();
}

/** \} */

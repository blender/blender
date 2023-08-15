/* SPDX-FileCopyrightText: 2003-2010 Sony Pictures Imageworks Inc., et al. All Rights Reserved.
 *                         (BSD-3-Clause).
 * SPDX-FileCopyrightText: 2013 Blender Foundation (GPL-2.0-or-later).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later AND BSD-3-Clause */

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

#include "gpu_shader_create_info.hh"

using namespace OCIO_NAMESPACE;

#include "MEM_guardedalloc.h"

#include "ocio_impl.h"
#include "ocio_shader_shared.hh"

/* **** OpenGL drawing routines using GLSL for color space transform ***** */

enum OCIO_GPUTextureSlots {
  TEXTURE_SLOT_IMAGE = 0,
  TEXTURE_SLOT_OVERLAY = 1,
  TEXTURE_SLOT_CURVE_MAPPING = 2,
  TEXTURE_SLOT_LUTS_OFFSET = 3,
};

enum OCIO_GPUUniformBufSlots {
  UNIFORMBUF_SLOT_DISPLAY = 0,
  UNIFORMBUF_SLOT_CURVEMAP = 1,
  UNIFORMBUF_SLOT_LUTS = 2,
};

struct OCIO_GPUShader {
  /* GPU shader. */
  struct GPUShader *shader = nullptr;

  /** Uniform parameters. */
  OCIO_GPUParameters parameters = {};
  GPUUniformBuf *parameters_buffer = nullptr;

  /* Destructor. */
  ~OCIO_GPUShader()
  {
    if (shader) {
      GPU_shader_free(shader);
    }
    if (parameters_buffer) {
      GPU_uniformbuf_free(parameters_buffer);
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
  GPUUniformBuf *uniforms_buffer = nullptr;

  /* Destructor. */
  ~OCIO_GPUTextures()
  {
    for (OCIO_GPULutTexture &lut : luts) {
      GPU_texture_free(lut.texture);
    }
    if (dummy) {
      GPU_texture_free(dummy);
    }
    if (uniforms_buffer) {
      GPU_uniformbuf_free(uniforms_buffer);
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
  using namespace blender::gpu::shader;

  std::string source;
  source += shaderdesc_to_scene_linear->getShaderText();
  source += "\n";
  source += shaderdesc_to_display->getShaderText();
  source += "\n";

  {
    /* Replace all uniform declarations by a comment.
     * This avoids double declarations from the backend. */
    size_t index = 0;
    while (true) {
      index = source.find("uniform ", index);
      if (index == -1) {
        break;
      }
      source.replace(index, 2, "//");
      index += 2;
    }
  }

  StageInterfaceInfo iface("OCIO_Interface", "");
  iface.smooth(Type::VEC2, "texCoord_interp");

  ShaderCreateInfo info("OCIO_Display");
  /* Work around OpenColorIO not supporting latest GLSL yet. */
  info.define("texture1D", "texture");
  info.define("texture2D", "texture");
  info.define("texture3D", "texture");
  info.typedef_source("ocio_shader_shared.hh");
  info.sampler(TEXTURE_SLOT_IMAGE, ImageType::FLOAT_2D, "image_texture");
  info.sampler(TEXTURE_SLOT_OVERLAY, ImageType::FLOAT_2D, "overlay_texture");
  info.uniform_buf(UNIFORMBUF_SLOT_DISPLAY, "OCIO_GPUParameters", "parameters");
  info.push_constant(Type::MAT4, "ModelViewProjectionMatrix");
  info.vertex_in(0, Type::VEC2, "pos");
  info.vertex_in(1, Type::VEC2, "texCoord");
  info.vertex_out(iface);
  info.fragment_out(0, Type::VEC4, "fragColor");
  info.vertex_source("gpu_shader_display_transform_vert.glsl");
  info.fragment_source("gpu_shader_display_transform_frag.glsl");
  info.fragment_source_generated = source;

  /* #96502: Work around for incorrect OCIO GLSL code generation when using
   * GradingPrimaryTransform. Should be reevaluated when changing to a next version of OCIO.
   * (currently v2.1.1). */
  info.define("inf 1e32");

  if (use_curve_mapping) {
    info.define("USE_CURVE_MAPPING");
    info.uniform_buf(UNIFORMBUF_SLOT_CURVEMAP, "OCIO_GPUCurveMappingParameters", "curve_mapping");
    info.sampler(TEXTURE_SLOT_CURVE_MAPPING, ImageType::FLOAT_1D, "curve_mapping_texture");
  }

  /* Set LUT textures. */
  int slot = TEXTURE_SLOT_LUTS_OFFSET;
  for (OCIO_GPULutTexture &texture : textures.luts) {
    const int dimensions = GPU_texture_dimensions(texture.texture);
    ImageType type = (dimensions == 1) ? ImageType::FLOAT_1D :
                     (dimensions == 2) ? ImageType::FLOAT_2D :
                                         ImageType::FLOAT_3D;

    info.sampler(slot++, type, texture.sampler_name.c_str());
  }

  /* Set LUT uniforms. */
  if (!textures.uniforms.empty()) {
    /* NOTE: For simplicity, we pad everything to size of vec4 avoiding sorting and alignment
     * issues. It is unlikely that this becomes a real issue. */
    size_t ubo_size = textures.uniforms.size() * sizeof(float) * 4;
    void *ubo_data_buf = malloc(ubo_size);

    uint32_t *ubo_data = reinterpret_cast<uint32_t *>(ubo_data_buf);

    std::stringstream ss;
    ss << "struct OCIO_GPULutParameters {\n";

    int index = 0;
    for (OCIO_GPUUniform &uniform : textures.uniforms) {
      index += 1;
      const GpuShaderDesc::UniformData &data = uniform.data;
      const char *name = uniform.name.c_str();
      char prefix = ' ';
      int vec_len;
      switch (data.m_type) {
        case UNIFORM_DOUBLE: {
          vec_len = 1;
          float value = float(data.m_getDouble());
          memcpy(ubo_data, &value, sizeof(float));
          break;
        }
        case UNIFORM_BOOL: {
          prefix = 'b';
          vec_len = 1;
          int value = int(data.m_getBool());
          memcpy(ubo_data, &value, sizeof(int));
          break;
        }
        case UNIFORM_FLOAT3:
          vec_len = 3;
          memcpy(ubo_data, data.m_getFloat3().data(), sizeof(float) * 3);
          break;
        case UNIFORM_VECTOR_FLOAT:
          vec_len = data.m_vectorFloat.m_getSize();
          memcpy(ubo_data, data.m_vectorFloat.m_getVector(), sizeof(float) * vec_len);
          break;
        case UNIFORM_VECTOR_INT:
          prefix = 'i';
          vec_len = data.m_vectorInt.m_getSize();
          memcpy(ubo_data, data.m_vectorInt.m_getVector(), sizeof(int) * vec_len);
          break;
        default:
          continue;
      }
      /* Align every member to 16bytes. */
      ubo_data += 4;
      /* Use a generic variable name because some GLSL compilers can interpret the preprocessor
       * define as recursive. */
      ss << "  " << prefix << "vec4 var" << index << ";\n";
      /* Use a define to keep the generated code working. */
      blender::StringRef suffix = blender::StringRefNull("xyzw").substr(0, vec_len);
      ss << "#define " << name << " lut_parameters.var" << index << "." << suffix << "\n";
    }
    ss << "};\n";
    info.typedef_source_generated = ss.str();

    info.uniform_buf(UNIFORMBUF_SLOT_LUTS, "OCIO_GPULutParameters", "lut_parameters");

    textures.uniforms_buffer = GPU_uniformbuf_create_ex(
        ubo_size, ubo_data_buf, "OCIO_LutParameters");

    free(ubo_data_buf);
  }

  shader.shader = GPU_shader_create_from_info(reinterpret_cast<GPUShaderCreateInfo *>(&info));

  return (shader.shader != nullptr);
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

static bool addGPULut1D2D(OCIO_GPUTextures &textures,
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
      values == nullptr)
  {
    return false;
  }

  eGPUTextureFormat format = (channel == GpuShaderCreator::TEXTURE_RGB_CHANNEL) ? GPU_RGB16F :
                                                                                  GPU_R16F;

  OCIO_GPULutTexture lut;
  /* There does not appear to be an explicit way to check if a texture is 1D or 2D.
   * It depends on more than height. So check instead by looking at the source. */
  std::string sampler1D_name = std::string("sampler1D ") + sampler_name;
  if (strstr(shader_desc->getShaderText(), sampler1D_name.c_str()) != nullptr) {
    lut.texture = GPU_texture_create_1d(
        texture_name, width, 1, format, GPU_TEXTURE_USAGE_SHADER_READ, values);
  }
  else {
    lut.texture = GPU_texture_create_2d(
        texture_name, width, height, 1, format, GPU_TEXTURE_USAGE_SHADER_READ, values);
  }
  if (lut.texture == nullptr) {
    return false;
  }

  GPU_texture_filter_mode(lut.texture, interpolation != INTERP_NEAREST);
  GPU_texture_extend_mode(lut.texture, GPU_SAMPLER_EXTEND_MODE_EXTEND);

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
  lut.texture = GPU_texture_create_3d(texture_name,
                                      edgelen,
                                      edgelen,
                                      edgelen,
                                      1,
                                      GPU_RGB16F,
                                      GPU_TEXTURE_USAGE_SHADER_READ,
                                      values);
  if (lut.texture == nullptr) {
    return false;
  }

  GPU_texture_filter_mode(lut.texture, interpolation != INTERP_NEAREST);
  GPU_texture_extend_mode(lut.texture, GPU_SAMPLER_EXTEND_MODE_EXTEND);

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
    if (!addGPULut1D2D(textures, shaderdesc_to_scene_linear, index)) {
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
    if (!addGPULut1D2D(textures, shaderdesc_to_display, index)) {
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

    curvemap.texture = GPU_texture_create_1d(
        "OCIOCurveMap", lut_size, 1, GPU_RGBA16F, GPU_TEXTURE_USAGE_SHADER_READ, nullptr);
    GPU_texture_filter_mode(curvemap.texture, false);
    GPU_texture_extend_mode(curvemap.texture, GPU_SAMPLER_EXTEND_MODE_EXTEND);

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
    data.range[i] = curve_mapping_settings->range[i];
    data.mintable[i] = curve_mapping_settings->mintable[i];
    data.ext_in_x[i] = curve_mapping_settings->ext_in_x[i];
    data.ext_in_y[i] = curve_mapping_settings->ext_in_y[i];
    data.ext_out_x[i] = curve_mapping_settings->ext_out_x[i];
    data.ext_out_y[i] = curve_mapping_settings->ext_out_y[i];
    data.first_x[i] = curve_mapping_settings->first_x[i];
    data.first_y[i] = curve_mapping_settings->first_y[i];
    data.last_x[i] = curve_mapping_settings->last_x[i];
    data.last_y[i] = curve_mapping_settings->last_y[i];
  }
  for (int i = 0; i < 3; i++) {
    data.black[i] = curve_mapping_settings->black[i];
    data.bwmul[i] = curve_mapping_settings->bwmul[i];
  }
  data.lut_size = curve_mapping_settings->lut_size;
  data.use_extend_extrapolate = curve_mapping_settings->use_extend_extrapolate;

  GPU_uniformbuf_update(curvemap.buffer, &data);
}

static void updateGPUDisplayParameters(OCIO_GPUShader &shader,
                                       float scale,
                                       float exponent,
                                       float dither,
                                       bool use_predivide,
                                       bool use_overlay,
                                       bool use_hdr)
{
  bool do_update = false;
  if (shader.parameters_buffer == nullptr) {
    shader.parameters_buffer = GPU_uniformbuf_create(sizeof(OCIO_GPUParameters));
    do_update = true;
  }
  OCIO_GPUParameters &data = shader.parameters;
  if (data.scale != scale) {
    data.scale = scale;
    do_update = true;
  }
  if (data.exponent != exponent) {
    data.exponent = exponent;
    do_update = true;
  }
  if (data.dither != dither) {
    data.dither = dither;
    do_update = true;
  }
  if (bool(data.use_predivide) != use_predivide) {
    data.use_predivide = use_predivide;
    do_update = true;
  }
  if (bool(data.use_overlay) != use_overlay) {
    data.use_overlay = use_overlay;
    do_update = true;
  }
  if (bool(data.use_hdr) != use_hdr) {
    data.use_hdr = use_hdr;
    do_update = true;
  }
  if (do_update) {
    GPU_uniformbuf_update(shader.parameters_buffer, &data);
  }
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
       it++)
  {
    if (it->input == input && it->view == view && it->display == display && it->look == look &&
        it->use_curve_mapping == use_curve_mapping)
    {
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
      config, ROLE_SCENE_LINEAR, view, display, look, 1.0f, 1.0f, false);

  /* Create shader descriptions. */
  if (processor_to_scene_linear && processor_to_display) {
    GpuShaderDescRcPtr shaderdesc_to_scene_linear = GpuShaderDesc::CreateShaderDesc();
    shaderdesc_to_scene_linear->setLanguage(GPU_LANGUAGE_GLSL_1_3);
    shaderdesc_to_scene_linear->setFunctionName("OCIO_to_scene_linear");
    shaderdesc_to_scene_linear->setResourcePrefix("to_scene");
    (*(ConstProcessorRcPtr *)processor_to_scene_linear)
        ->getDefaultGPUProcessor()
        ->extractGpuShaderInfo(shaderdesc_to_scene_linear);
    shaderdesc_to_scene_linear->finalize();

    GpuShaderDescRcPtr shaderdesc_to_display = GpuShaderDesc::CreateShaderDesc();
    shaderdesc_to_display->setLanguage(GPU_LANGUAGE_GLSL_1_3);
    shaderdesc_to_display->setFunctionName("OCIO_to_display");
    shaderdesc_to_display->setResourcePrefix("to_display");
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
                        use_curve_mapping))
    {
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
                                    const bool use_overlay,
                                    const bool use_hdr)
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
    GPU_uniformbuf_bind(curvemap.buffer, UNIFORMBUF_SLOT_CURVEMAP);
    GPU_texture_bind(curvemap.texture, TEXTURE_SLOT_CURVE_MAPPING);
  }

  /* Bind textures to sampler units. Texture 0 is set by caller.
   * Uniforms have already been set for texture bind points. */
  if (!use_overlay) {
    /* Avoid missing binds. */
    GPU_texture_bind(textures.dummy, TEXTURE_SLOT_OVERLAY);
  }
  for (int i = 0; i < textures.luts.size(); i++) {
    GPU_texture_bind(textures.luts[i].texture, TEXTURE_SLOT_LUTS_OFFSET + i);
  }

  if (textures.uniforms_buffer) {
    GPU_uniformbuf_bind(textures.uniforms_buffer, UNIFORMBUF_SLOT_LUTS);
  }

  updateGPUDisplayParameters(shader, scale, exponent, dither, use_predivide, use_overlay, use_hdr);
  GPU_uniformbuf_bind(shader.parameters_buffer, UNIFORMBUF_SLOT_DISPLAY);

  /* TODO(fclem): remove remains of IMM. */
  immBindShader(shader.shader);

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

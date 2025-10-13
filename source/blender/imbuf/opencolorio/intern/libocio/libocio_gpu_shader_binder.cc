/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "libocio_gpu_shader_binder.hh"

#if defined(WITH_OPENCOLORIO)

#  include "GPU_texture.hh"

#  include "error_handling.hh"
#  include "libocio_config.hh"
#  include "libocio_display_processor.hh"
#  include "libocio_processor.hh"

#  include "../gpu_shader_binder_internal.hh"

namespace blender::ocio {

namespace {

using namespace OCIO_NAMESPACE;

static ConstProcessorRcPtr create_to_scene_linear_processor(
    const ConstConfigRcPtr &ocio_config, const internal::GPUDisplayShader &display_shader)
{
  return create_ocio_processor(
      ocio_config, display_shader.from_colorspace.c_str(), ROLE_SCENE_LINEAR);
}

static ConstProcessorRcPtr create_to_display_processor(
    const LibOCIOConfig &config, const internal::GPUDisplayShader &display_shader)
{
  DisplayParameters display_parameters;
  display_parameters.from_colorspace = ROLE_SCENE_LINEAR;
  display_parameters.view = display_shader.view;
  display_parameters.display = display_shader.display;
  display_parameters.look = display_shader.look;
  display_parameters.use_hdr_buffer = display_shader.use_hdr_buffer;
  display_parameters.use_display_emulation = display_shader.use_display_emulation;
  return create_ocio_display_processor(config, display_parameters);
}

static bool add_gpu_uniform(internal::GPUTextures &textures,
                            const GpuShaderDescRcPtr &shader_desc,
                            const int index)
{
  internal::GPUUniform uniform;
  uniform.name = shader_desc->getUniform(index, uniform.data);
  if (uniform.data.m_type == UNIFORM_UNKNOWN) {
    return false;
  }

  textures.uniforms.append(uniform);
  return true;
}

static bool add_gpu_lut_1D2D(internal::GPUTextures &textures,
                             const GpuShaderDescRcPtr &shader_desc,
                             const int index)
{
  const char *texture_name = nullptr;
  const char *sampler_name = nullptr;
  uint width = 0;
  uint height = 0;

  GpuShaderCreator::TextureType channel = GpuShaderCreator::TEXTURE_RGB_CHANNEL;
  Interpolation interpolation = INTERP_LINEAR;

  /* Always use 2D textures in OpenColorIO 2.3, simpler and same performance. */
  static_assert(OCIO_VERSION_HEX >= 0x02030000);
  GpuShaderDesc::TextureDimensions dimensions = GpuShaderDesc::TEXTURE_2D;
  shader_desc->getTexture(
      index, texture_name, sampler_name, width, height, channel, dimensions, interpolation);

  const float *values;
  shader_desc->getTextureValues(index, values);
  if (texture_name == nullptr || sampler_name == nullptr || width == 0 || height == 0 ||
      values == nullptr)
  {
    return false;
  }

  blender::gpu::TextureFormat format = (channel == GpuShaderCreator::TEXTURE_RGB_CHANNEL) ?
                                           blender::gpu::TextureFormat::SFLOAT_16_16_16 :
                                           blender::gpu::TextureFormat::SFLOAT_16;

  internal::GPULutTexture lut;
  /* There does not appear to be an explicit way to check if a texture is 1D or 2D.
   * It depends on more than height. So check instead by looking at the source.
   * The Blender default config does not use 1D textures, but for example
   * studio-config-v3.0.0_aces-v2.0_ocio-v2.4.ocio needs this code. */
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

  textures.luts.append(lut);

  return true;
}

static bool add_gpu_lut_3D(internal::GPUTextures &textures,
                           const GpuShaderDescRcPtr &shader_desc,
                           const int index)
{
  const char *texture_name = nullptr;
  const char *sampler_name = nullptr;
  uint edgelen = 0;
  Interpolation interpolation = INTERP_LINEAR;
  shader_desc->get3DTexture(index, texture_name, sampler_name, edgelen, interpolation);

  const float *values;
  shader_desc->get3DTextureValues(index, values);
  if (texture_name == nullptr || sampler_name == nullptr || edgelen == 0 || values == nullptr) {
    return false;
  }

  internal::GPULutTexture lut;
  lut.texture = GPU_texture_create_3d(texture_name,
                                      edgelen,
                                      edgelen,
                                      edgelen,
                                      1,
                                      blender::gpu::TextureFormat::SFLOAT_16_16_16,
                                      GPU_TEXTURE_USAGE_SHADER_READ,
                                      values);
  if (lut.texture == nullptr) {
    return false;
  }

  GPU_texture_filter_mode(lut.texture, interpolation != INTERP_NEAREST);
  GPU_texture_extend_mode(lut.texture, GPU_SAMPLER_EXTEND_MODE_EXTEND);

  lut.sampler_name = sampler_name;

  textures.luts.append(lut);
  return true;
}

static bool create_gpu_textures(internal::GPUTextures &textures,
                                const GpuShaderDescRcPtr &shader_desc)
{
  for (int index = 0; index < shader_desc->getNumUniforms(); index++) {
    if (!add_gpu_uniform(textures, shader_desc, index)) {
      return false;
    }
  }
  for (int index = 0; index < shader_desc->getNumTextures(); index++) {
    if (!add_gpu_lut_1D2D(textures, shader_desc, index)) {
      return false;
    }
  }
  for (int index = 0; index < shader_desc->getNum3DTextures(); index++) {
    if (!add_gpu_lut_3D(textures, shader_desc, index)) {
      return false;
    }
  }

  return true;
}

}  // namespace

void LibOCIOGPUShaderBinder::construct_shader_for_processors(
    internal::GPUDisplayShader &display_shader,
    const ConstProcessorRcPtr &processor_to_scene_linear,
    const ConstProcessorRcPtr &processor_to_display,
    const Span<std::array<StringRefNull, 2>> additional_defines) const
{
  std::string fragment_source;

  GpuShaderDescRcPtr shaderdesc_to_scene_linear;
  if (processor_to_scene_linear) {
    shaderdesc_to_scene_linear = GpuShaderDesc::CreateShaderDesc();
    shaderdesc_to_scene_linear->setLanguage(GPU_LANGUAGE_GLSL_1_3);
    shaderdesc_to_scene_linear->setFunctionName("OCIO_to_scene_linear");
    shaderdesc_to_scene_linear->setResourcePrefix("to_scene");
    processor_to_scene_linear->getDefaultGPUProcessor()->extractGpuShaderInfo(
        shaderdesc_to_scene_linear);
    shaderdesc_to_scene_linear->finalize();

    if (!create_gpu_textures(display_shader.textures, shaderdesc_to_scene_linear)) {
      display_shader.is_valid = false;
      return;
    }

    fragment_source += shaderdesc_to_scene_linear->getShaderText();
    fragment_source += "\n";
  }

  GpuShaderDescRcPtr shaderdesc_to_display;
  if (processor_to_display) {
    shaderdesc_to_display = GpuShaderDesc::CreateShaderDesc();
    shaderdesc_to_display->setLanguage(GPU_LANGUAGE_GLSL_1_3);
    shaderdesc_to_display->setFunctionName("OCIO_to_display");
    shaderdesc_to_display->setResourcePrefix("to_display");
    processor_to_display->getDefaultGPUProcessor()->extractGpuShaderInfo(shaderdesc_to_display);
    shaderdesc_to_display->finalize();

    if (!create_gpu_textures(display_shader.textures, shaderdesc_to_display)) {
      display_shader.is_valid = false;
      return;
    }

    fragment_source += shaderdesc_to_display->getShaderText();
    fragment_source += "\n";
  }

  if (!create_gpu_shader(display_shader, fragment_source, additional_defines)) {
    display_shader.is_valid = false;
    return;
  }

  display_shader.is_valid = true;
}

void LibOCIOGPUShaderBinder::construct_display_shader(
    internal::GPUDisplayShader &display_shader) const
{
  const LibOCIOConfig &config = static_cast<const LibOCIOConfig &>(config_);
  const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config = config.get_ocio_config();

  ConstProcessorRcPtr processor_to_scene_linear = create_to_scene_linear_processor(ocio_config,
                                                                                   display_shader);
  ConstProcessorRcPtr processor_to_display = create_to_display_processor(config, display_shader);

  if (!processor_to_scene_linear || !processor_to_display) {
    display_shader.is_valid = false;
    return;
  }

  construct_shader_for_processors(
      display_shader, processor_to_scene_linear, processor_to_display, {});
}

void LibOCIOGPUShaderBinder::construct_scene_linear_shader(
    internal::GPUDisplayShader &display_shader) const
{
  const LibOCIOConfig &config = static_cast<const LibOCIOConfig &>(config_);
  const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config = config.get_ocio_config();

  ConstProcessorRcPtr processor_to_scene_linear = create_to_scene_linear_processor(ocio_config,
                                                                                   display_shader);
  if (!processor_to_scene_linear) {
    display_shader.is_valid = false;
    return;
  }

  construct_shader_for_processors(
      display_shader,
      processor_to_scene_linear,
      nullptr,
      {{"USE_TO_SCENE_LINEAR_ONLY", ""}, {"OUTPUT_PREMULTIPLIED", ""}});
}

}  // namespace blender::ocio

#endif

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <cstdint>
#include <memory>
#include <string>

#include "BLI_assert.h"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "GPU_capabilities.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "gpu_shader_create_info.hh"

#include "COM_context.hh"
#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_result.hh"

#include "libocio_display_processor.hh"

#include "CLG_log.h"

#if defined(WITH_OPENCOLORIO)
#  include <OpenColorIO/OpenColorIO.h>
#endif

namespace blender::compositor {

static CLG_LogRef LOG = {"compositor.gpu"};

/* --------------------------------------------------------------------
 * GPU Shader Creator.
 */

#if defined(WITH_OPENCOLORIO)

namespace OCIO = OCIO_NAMESPACE;
using namespace blender::gpu::shader;

/* A subclass of OCIO::GpuShaderCreator that constructs the shader using a ShaderCreateInfo. The
 * Create method should be used to construct the creator, then the extractGpuShaderInfo() method of
 * the appropriate OCIO::GPUProcessor should be called passing in the creator. After construction,
 * the constructed compute shader can be used by calling the bind_shader_and_resources() method,
 * followed by binding the input texture and output image using their names input_sampler_name()
 * and output_image_name(), following by dispatching the shader on the domain of the input, and
 * finally calling the unbind_shader_and_resources() method.
 *
 * Upon calling the extractGpuShaderInfo(), all the transforms in the GPU processor will add their
 * needed resources by calling the respective addUniform() and add[3D]Texture() methods. Then, the
 * shader code of all transforms will be generated and passed to the createShaderText() method,
 * generating the full code of the processor. Finally, the finalize() method will be called to
 * finally create the shader. */
class GPUShaderCreator : public OCIO::GpuShaderCreator {
 public:
  static std::shared_ptr<GPUShaderCreator> Create(ResultPrecision precision)
  {
    std::shared_ptr<GPUShaderCreator> instance = std::make_shared<GPUShaderCreator>();
    instance->setLanguage(OCIO::GPU_LANGUAGE_GLSL_4_0);
    instance->precision_ = precision;
    return instance;
  }

  /* Not used, but needs to be overridden, so return a nullptr. */
  OCIO::GpuShaderCreatorRcPtr clone() const override
  {
    return OCIO::GpuShaderCreatorRcPtr();
  }

  /* This is ignored since we query using our own GPU capabilities system. */
  void setTextureMaxWidth(uint /*max_width*/) override {}

  uint getTextureMaxWidth() const noexcept override
  {
    return GPU_max_texture_size();
  }

#  if OCIO_VERSION_HEX >= 0x02030000
  void setAllowTexture1D(bool allowed) override
  {
    allow_texture_1D_ = allowed;
  }

  bool getAllowTexture1D() const override
  {
    return allow_texture_1D_;
  }
#  endif

  bool addUniform(const char *name, const DoubleGetter &get_double) override
  {
    /* Check if a resource exists with the same name and assert if it is the case, returning false
     * indicates failure to add the uniform for the shader creator. */
    if (!resource_names_.add(std::make_unique<std::string>(name))) {
      BLI_assert_unreachable();
      return false;
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    std::string &resource_name = *resource_names_[resource_names_.size() - 1];
    shader_create_info_.push_constant(Type::float_t, resource_name);

    float_uniforms_.add(resource_name, get_double);

    return true;
  }

  bool addUniform(const char *name, const BoolGetter &get_bool) override
  {
    /* Check if a resource exists with the same name and assert if it is the case, returning false
     * indicates failure to add the uniform for the shader creator. */
    if (!resource_names_.add(std::make_unique<std::string>(name))) {
      BLI_assert_unreachable();
      return false;
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    const std::string &resource_name = *resource_names_[resource_names_.size() - 1];
    shader_create_info_.push_constant(Type::bool_t, resource_name);

    boolean_uniforms_.add(name, get_bool);

    return true;
  }

  bool addUniform(const char *name, const Float3Getter &get_float3) override
  {
    /* Check if a resource exists with the same name and assert if it is the case, returning false
     * indicates failure to add the uniform for the shader creator. */
    if (!resource_names_.add(std::make_unique<std::string>(name))) {
      BLI_assert_unreachable();
      return false;
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    std::string &resource_name = *resource_names_[resource_names_.size() - 1];
    shader_create_info_.push_constant(Type::float3_t, resource_name);

    vector_uniforms_.add(resource_name, get_float3);

    return true;
  }

  bool addUniform(const char *name,
                  const SizeGetter &get_size,
                  const VectorFloatGetter &get_vector_float
#  if OCIO_VERSION_HEX >= 0x02050000
                  ,
                  const unsigned /*maxSize*/
#  endif
                  ) override
  {
    /* Check if a resource exists with the same name and assert if it is the case, returning false
     * indicates failure to add the uniform for the shader creator. */
    if (!resource_names_.add(std::make_unique<std::string>(name))) {
      BLI_assert_unreachable();
      return false;
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    std::string &resource_name = *resource_names_[resource_names_.size() - 1];
    shader_create_info_.uniform_buf(buffers_sizes_.size(), "float", resource_name);

    float_buffers_.add(resource_name, get_vector_float);
    buffers_sizes_.add(resource_name, get_size);

    return true;
  }

  bool addUniform(const char *name,
                  const SizeGetter &get_size,
                  const VectorIntGetter &get_vector_int
#  if OCIO_VERSION_HEX >= 0x02050000
                  ,
                  const unsigned /*maxSize*/
#  endif
                  ) override
  {
    /* Check if a resource exists with the same name and assert if it is the case, returning false
     * indicates failure to add the uniform for the shader creator. */
    if (!resource_names_.add(std::make_unique<std::string>(name))) {
      BLI_assert_unreachable();
      return false;
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    std::string &resource_name = *resource_names_[resource_names_.size() - 1];
    shader_create_info_.uniform_buf(buffers_sizes_.size(), "int", resource_name);

    int_buffers_.add(name, get_vector_int);
    buffers_sizes_.add(name, get_size);

    return true;
  }

#  if OCIO_VERSION_HEX >= 0x02050000
  unsigned
#  else
  void
#  endif
  addTexture(const char *texture_name,
             const char *sampler_name,
             uint width,
             uint height,
             TextureType channel,
#  if OCIO_VERSION_HEX >= 0x02030000
             OCIO::GpuShaderDesc::TextureDimensions dimensions,
#  endif
             OCIO::Interpolation interpolation,
             const float *values) override
  {
    /* Check if a resource exists with the same name and assert if it is the case. */
    if (!resource_names_.add(std::make_unique<std::string>(sampler_name))) {
      BLI_assert_unreachable();
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    const std::string &resource_name = *resource_names_[resource_names_.size() - 1];

    blender::gpu::Texture *texture;
    const blender::gpu::TextureFormat base_format =
        (channel == TEXTURE_RGB_CHANNEL) ? blender::gpu::TextureFormat::SFLOAT_32_32_32 :
                                           blender::gpu::TextureFormat::SFLOAT_32;
    const blender::gpu::TextureFormat texture_format = Result::gpu_texture_format(base_format,
                                                                                  precision_);
    /* A height of 1 indicates a 1D texture according to the OCIO API. */
#  if OCIO_VERSION_HEX >= 0x02030000
    if (dimensions == OCIO::GpuShaderDesc::TEXTURE_1D)
#  else
    if (height == 1)
#  endif
    {
      texture = GPU_texture_create_1d(
          texture_name, width, 1, texture_format, GPU_TEXTURE_USAGE_SHADER_READ, values);
      shader_create_info_.sampler(textures_.size() + 1, ImageType::Float1D, resource_name);
    }
    else {
      texture = GPU_texture_create_2d(
          texture_name, width, height, 1, texture_format, GPU_TEXTURE_USAGE_SHADER_READ, values);
      shader_create_info_.sampler(textures_.size() + 1, ImageType::Float2D, resource_name);
    }
    GPU_texture_filter_mode(texture, interpolation != OCIO::INTERP_NEAREST);

    textures_.add(sampler_name, texture);
#  if OCIO_VERSION_HEX >= 0x02050000
    return textures_.size() - 1;
#  endif
  }

#  if OCIO_VERSION_HEX >= 0x02050000
  unsigned
#  else
  void
#  endif
  add3DTexture(const char *texture_name,
               const char *sampler_name,
               uint size,
               OCIO::Interpolation interpolation,
               const float *values) override
  {
    /* Check if a resource exists with the same name and assert if it is the case. */
    if (!resource_names_.add(std::make_unique<std::string>(sampler_name))) {
      BLI_assert_unreachable();
    }

    /* Don't use the name argument directly since ShaderCreateInfo only stores references to
     * resource names, instead, use the name that is stored in resource_names_. */
    const std::string &resource_name = *resource_names_[resource_names_.size() - 1];
    shader_create_info_.sampler(textures_.size() + 1, ImageType::Float3D, resource_name);

    blender::gpu::Texture *texture = GPU_texture_create_3d(
        texture_name,
        size,
        size,
        size,
        1,
        Result::gpu_texture_format(blender::gpu::TextureFormat::SFLOAT_32_32_32, precision_),
        GPU_TEXTURE_USAGE_SHADER_READ,
        values);
    GPU_texture_filter_mode(texture, interpolation != OCIO::INTERP_NEAREST);

    textures_.add(sampler_name, texture);
#  if OCIO_VERSION_HEX >= 0x02050000
    return textures_.size() - 1;
#  endif
  }

  /* This gets called before the finalize() method to construct the shader code. We just
   * concatenate the code except for the declarations section. That's because the ShaderCreateInfo
   * will add the declaration itself. */
  void createShaderText(const char * /*parameter_declarations*/,
#  if OCIO_VERSION_HEX >= 0x02050000
                        const char * /*texture_declarations*/,
#  endif
                        const char *helper_methods,
                        const char *function_header,
                        const char *function_body,
                        const char *function_footer) override
  {
    shader_code_ += helper_methods;
    shader_code_ += function_header;
    shader_code_ += function_body;
    shader_code_ += function_footer;
  }

  /* This gets called when all resources were added using the respective addUniform() or
   * add[3D]Texture() methods and the shader code was generated using the createShaderText()
   * method. That is, we are ready to complete the ShaderCreateInfo and create a shader from it. */
  void finalize() override
  {
    GpuShaderCreator::finalize();

    shader_create_info_.local_group_size(16, 16);
    shader_create_info_.sampler(0, ImageType::Float2D, input_sampler_name());
    shader_create_info_.builtins(BuiltinBits::GLOBAL_INVOCATION_ID);
    shader_create_info_.image(0,
                              Result::gpu_texture_format(ResultType::Color, precision_),
                              Qualifier::write,
                              ImageReadWriteType::Float2D,
                              output_image_name());
    shader_create_info_.compute_source("gpu_shader_compositor_ocio_processor.glsl");
    shader_create_info_.compute_source_generated += GPU_shader_preprocess_source(shader_code_);

    GPUShaderCreateInfo *info = reinterpret_cast<GPUShaderCreateInfo *>(&shader_create_info_);
    shader_ = GPU_shader_create_from_info(info);
  }

  gpu::Shader *bind_shader_and_resources()
  {
    if (!shader_) {
      return nullptr;
    }

    GPU_shader_bind(shader_);

    for (auto item : float_uniforms_.items()) {
      GPU_shader_uniform_1f(shader_, item.key.c_str(), item.value());
    }

    for (auto item : boolean_uniforms_.items()) {
      GPU_shader_uniform_1b(shader_, item.key.c_str(), item.value());
    }

    for (auto item : vector_uniforms_.items()) {
      GPU_shader_uniform_3fv(shader_, item.key.c_str(), item.value().data());
    }

    for (auto item : float_buffers_.items()) {
      gpu::UniformBuf *buffer = GPU_uniformbuf_create_ex(
          buffers_sizes_.lookup(item.key)(), item.value(), item.key.c_str());
      const int ubo_location = GPU_shader_get_ubo_binding(shader_, item.key.c_str());
      GPU_uniformbuf_bind(buffer, ubo_location);
      uniform_buffers_.append(buffer);
    }

    for (auto item : int_buffers_.items()) {
      gpu::UniformBuf *buffer = GPU_uniformbuf_create_ex(
          buffers_sizes_.lookup(item.key)(), item.value(), item.key.c_str());
      const int ubo_location = GPU_shader_get_ubo_binding(shader_, item.key.c_str());
      GPU_uniformbuf_bind(buffer, ubo_location);
      uniform_buffers_.append(buffer);
    }

    for (auto item : textures_.items()) {
      const int texture_image_unit = GPU_shader_get_sampler_binding(shader_, item.key.c_str());
      GPU_texture_bind(item.value, texture_image_unit);
    }

    return shader_;
  }

  void unbind_shader_and_resources()
  {
    for (gpu::UniformBuf *buffer : uniform_buffers_) {
      GPU_uniformbuf_unbind(buffer);
      GPU_uniformbuf_free(buffer);
    }

    for (blender::gpu::Texture *texture : textures_.values()) {
      GPU_texture_unbind(texture);
    }

    GPU_shader_unbind();
  }

  const char *input_sampler_name()
  {
    return "input_tx";
  }

  const char *output_image_name()
  {
    return "output_img";
  }

  ~GPUShaderCreator() override
  {
    for (blender::gpu::Texture *texture : textures_.values()) {
      GPU_texture_free(texture);
    }

    GPU_shader_free(shader_);
  }

 private:
  /* The processor shader and the ShaderCreateInfo used to construct it. Constructed and
   * initialized in the finalize() method. */
  gpu::Shader *shader_ = nullptr;
  ShaderCreateInfo shader_create_info_ = ShaderCreateInfo("OCIO Processor");

  /* Stores the generated OCIOMain function as well as a number of helper functions. Initialized in
   * the createShaderText() method. */
  std::string shader_code_;

  /* Maps that associates the name of a uniform with a getter function that returns its value.
   * Initialized in the respective addUniform() methods. */
  Map<std::string, DoubleGetter> float_uniforms_;
  Map<std::string, BoolGetter> boolean_uniforms_;
  Map<std::string, Float3Getter> vector_uniforms_;

  /* Maps that associates the name of uniform buffer objects with a getter function that returns
   * its values. Initialized in the respective addUniform() methods. */
  Map<std::string, VectorFloatGetter> float_buffers_;
  Map<std::string, VectorIntGetter> int_buffers_;

  /* A map that associates the name of uniform buffer objects with a getter functions that returns
   * its number of elements. Initialized in the respective addUniform() methods. */
  Map<std::string, SizeGetter> buffers_sizes_;

  /* A map that associates the name of a sampler with its corresponding texture. Initialized in the
   * addTexture() and add3DTexture() methods. */
  Map<std::string, blender::gpu::Texture *> textures_;

  /* A vector set that stores the names of all the resources used by the shader. This is used to:
   *   1. Check for name collisions when adding new resources.
   *   2. Store the resource names throughout the construction of the shader since the
   *      ShaderCreateInfo class only stores references to resources names. */
  VectorSet<std::unique_ptr<std::string>> resource_names_;

  /* A vectors that stores the created uniform buffers when bind_shader_and_resources() is called,
   * so that they can be properly unbound and freed in the unbind_shader_and_resources() method. */
  Vector<gpu::UniformBuf *> uniform_buffers_;

#  if OCIO_VERSION_HEX >= 0x02030000
  /* Allow creating 1D textures, or only use 2D textures. */
  bool allow_texture_1D_ = true;
#  endif

  /* The precision of the OCIO resources as well as the output image. */
  ResultPrecision precision_;
};

#else

/* A stub implementation in case OCIO is disabled at build time. */
class GPUShaderCreator {
 public:
  static std::shared_ptr<GPUShaderCreator> Create(ResultPrecision /*precision*/)
  {
    return std::make_shared<GPUShaderCreator>();
  }

  gpu::Shader *bind_shader_and_resources()
  {
    return nullptr;
  }

  void unbind_shader_and_resources() {}

  const char *input_sampler_name()
  {
    return nullptr;
  }

  const char *output_image_name()
  {
    return nullptr;
  }
};

#endif

/* ------------------------------------------------------------------------------------------------
 * OCIO Color Space Conversion Shader Key.
 */

OCIOColorSpaceConversionShaderKey::OCIOColorSpaceConversionShaderKey(
    const std::string &source, const std::string &target, const std::string &config_cache_id)
    : source(source), target(target), config_cache_id(config_cache_id)
{
}

uint64_t OCIOColorSpaceConversionShaderKey::hash() const
{
  return get_default_hash(source, target, config_cache_id);
}

bool operator==(const OCIOColorSpaceConversionShaderKey &a,
                const OCIOColorSpaceConversionShaderKey &b)
{
  return a.source == b.source && a.target == b.target && a.config_cache_id == b.config_cache_id;
}

/* --------------------------------------------------------------------
 * OCIO Color Space Conversion Shader.
 */

OCIOColorSpaceConversionShader::OCIOColorSpaceConversionShader(Context &context,
                                                               std::string source,
                                                               std::string target)
{
  /* Create a GPU shader creator and construct it based on the transforms in the default GPU
   * processor. */
  shader_creator_ = GPUShaderCreator::Create(context.get_precision());

#if defined(WITH_OPENCOLORIO)
  /* Get a GPU processor that transforms the source color space to the target color space. */
  try {
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();
    OCIO::ConstProcessorRcPtr processor = config->getProcessor(source.c_str(), target.c_str());
    OCIO::ConstGPUProcessorRcPtr gpu_processor = processor->getDefaultGPUProcessor();

    auto ocio_shader_creator = std::static_pointer_cast<OCIO::GpuShaderCreator>(shader_creator_);
    gpu_processor->extractGpuShaderInfo(ocio_shader_creator);
  }
  catch (const OCIO::Exception &e) {
    CLOG_ERROR(&LOG, "Failed to create OpenColorIO shader: %s", e.what());
  }
#else
  UNUSED_VARS(source, target);
  UNUSED_VARS(LOG);
#endif
}

gpu::Shader *OCIOColorSpaceConversionShader::bind_shader_and_resources()
{
  return shader_creator_->bind_shader_and_resources();
}

void OCIOColorSpaceConversionShader::unbind_shader_and_resources()
{
  shader_creator_->unbind_shader_and_resources();
}

const char *OCIOColorSpaceConversionShader::input_sampler_name()
{
  return shader_creator_->input_sampler_name();
}

const char *OCIOColorSpaceConversionShader::output_image_name()
{
  return shader_creator_->output_image_name();
}

/* --------------------------------------------------------------------
 * OCIO Color Space Conversion Shader Container.
 */

void OCIOColorSpaceConversionShaderContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to track
   * their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

OCIOColorSpaceConversionShader &OCIOColorSpaceConversionShaderContainer::get(Context &context,
                                                                             std::string source,
                                                                             std::string target)
{
#if defined(WITH_OPENCOLORIO)
  /* Use the config cache ID in the cache key in case the configuration changed at runtime. */
  std::string config_cache_id = OCIO::GetCurrentConfig()->getCacheID();
#else
  std::string config_cache_id;
#endif

  const OCIOColorSpaceConversionShaderKey key(source, target, config_cache_id);

  OCIOColorSpaceConversionShader &shader = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<OCIOColorSpaceConversionShader>(context, source, target);
  });

  shader.needed = true;
  return shader;
}

/* ------------------------------------------------------------------------------------------------
 * OCIO To Display Shader Key.
 */

OCIOToDisplayShaderKey::OCIOToDisplayShaderKey(const ColorManagedDisplaySettings &display_settings,
                                               const ColorManagedViewSettings &view_settings,
                                               const bool inverse,
                                               const std::string &config_cache_id)
    : display_device(display_settings.display_device),
      view_transform(view_settings.view_transform),
      look(view_settings.look),
      inverse(inverse),
      config_cache_id(config_cache_id)
{
}

uint64_t OCIOToDisplayShaderKey::hash() const
{
  return get_default_hash(
      get_default_hash(display_device, view_transform, look, (inverse) ? "inverse" : "forward"),
      config_cache_id);
}

bool operator==(const OCIOToDisplayShaderKey &a, const OCIOToDisplayShaderKey &b)
{
  return a.display_device == b.display_device && a.view_transform == b.view_transform &&
         a.look == b.look && a.inverse == b.inverse && a.config_cache_id == b.config_cache_id;
}

/* --------------------------------------------------------------------
 * OCIO To Display Shader.
 */

OCIOToDisplayShader::OCIOToDisplayShader(Context &context,
                                         const ColorManagedDisplaySettings &display_settings,
                                         const ColorManagedViewSettings &view_settings,
                                         const bool inverse)
{
  /* Create a GPU shader creator and construct it based on the transforms in the default GPU
   * processor. */
  shader_creator_ = GPUShaderCreator::Create(context.get_precision());

#if defined(WITH_OPENCOLORIO)
  /* Get a GPU processor that transforms the display_device color space to the view_transform color
   * space. */
  try {
    OCIO::ConstConfigRcPtr config = OCIO::GetCurrentConfig();

    OCIO::TransformRcPtr group = ocio::create_ocio_display_transform(
        config,
        display_settings.display_device,
        view_settings.view_transform,
        view_settings.look,
        "scene_linear");

    if (inverse) {
      group->setDirection(OCIO::TRANSFORM_DIR_INVERSE);
    }

    OCIO::ConstProcessorRcPtr processor = config->getProcessor(group);
    OCIO::ConstGPUProcessorRcPtr gpu_processor = processor->getDefaultGPUProcessor();

    auto ocio_shader_creator = std::static_pointer_cast<OCIO::GpuShaderCreator>(shader_creator_);
    gpu_processor->extractGpuShaderInfo(ocio_shader_creator);
  }
  catch (const OCIO::Exception &e) {
    CLOG_ERROR(&LOG, "Failed to create OpenColorIO shader: %s", e.what());
  }
#else
  UNUSED_VARS(display_settings, view_settings, inverse);
#endif
}

gpu::Shader *OCIOToDisplayShader::bind_shader_and_resources()
{
  return shader_creator_->bind_shader_and_resources();
}

void OCIOToDisplayShader::unbind_shader_and_resources()
{
  shader_creator_->unbind_shader_and_resources();
}

const char *OCIOToDisplayShader::input_sampler_name()
{
  return shader_creator_->input_sampler_name();
}

const char *OCIOToDisplayShader::output_image_name()
{
  return shader_creator_->output_image_name();
}

/* --------------------------------------------------------------------
 * OCIO To Display Shader Container.
 */

void OCIOToDisplayShaderContainer::reset()
{
  /* First, delete all resources that are no longer needed. */
  map_.remove_if([](auto item) { return !item.value->needed; });

  /* Second, reset the needed status of the remaining resources to false to ready them to
   * track their needed status for the next evaluation. */
  for (auto &value : map_.values()) {
    value->needed = false;
  }
}

OCIOToDisplayShader &OCIOToDisplayShaderContainer::get(
    Context &context,
    const ColorManagedDisplaySettings &display_settings,
    const ColorManagedViewSettings &view_settings,
    const bool inverse)
{
#if defined(WITH_OPENCOLORIO)
  /* Use the config cache ID in the cache key in case the configuration changed at runtime. */
  std::string config_cache_id = OCIO::GetCurrentConfig()->getCacheID();
#else
  std::string config_cache_id;
#endif

  const OCIOToDisplayShaderKey key(display_settings, view_settings, inverse, config_cache_id);

  OCIOToDisplayShader &shader = *map_.lookup_or_add_cb(key, [&]() {
    return std::make_unique<OCIOToDisplayShader>(
        context, display_settings, view_settings, inverse);
  });

  shader.needed = true;
  return shader;
}

}  // namespace blender::compositor

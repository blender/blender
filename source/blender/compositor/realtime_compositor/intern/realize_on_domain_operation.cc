/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_float3x3.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_result.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

RealizeOnDomainOperation::RealizeOnDomainOperation(Context &context,
                                                   Domain domain,
                                                   ResultType type)
    : SimpleOperation(context), domain_(domain)
{
  InputDescriptor input_descriptor;
  input_descriptor.type = type;
  declare_input_descriptor(input_descriptor);
  populate_result(Result(type, texture_pool()));
}

void RealizeOnDomainOperation::execute()
{
  Result &input = get_input();
  Result &result = get_result();

  result.allocate_texture(domain_);

  GPUShader *shader = get_realization_shader();
  GPU_shader_bind(shader);

  /* Transform the input space into the domain space. */
  const float3x3 local_transformation = domain_.transformation.inverted() *
                                        input.domain().transformation;

  /* Set the origin of the transformation to be the center of the domain. */
  const float3x3 transformation = float3x3::from_origin_transformation(
      local_transformation, float2(domain_.size) / 2.0f);

  /* Invert the transformation because the shader transforms the domain coordinates instead of the
   * input image itself and thus expect the inverse. */
  const float3x3 inverse_transformation = transformation.inverted();

  GPU_shader_uniform_mat3_as_mat4(shader, "inverse_transformation", inverse_transformation.ptr());

  /* The texture sampler should use bilinear interpolation for both the bilinear and bicubic
   * cases, as the logic used by the bicubic realization shader expects textures to use bilinear
   * interpolation. */
  const bool use_bilinear = ELEM(input.get_realization_options().interpolation,
                                 Interpolation::Bilinear,
                                 Interpolation::Bicubic);
  GPU_texture_filter_mode(input.texture(), use_bilinear);

  /* Make out-of-bound texture access return zero by clamping to border color. And make texture
   * wrap appropriately if the input repeats. */
  const bool repeats = input.get_realization_options().repeat_x ||
                       input.get_realization_options().repeat_y;
  GPU_texture_wrap_mode(input.texture(), repeats, false);

  input.bind_as_texture(shader, "input_tx");
  result.bind_as_image(shader, "domain_img");

  compute_dispatch_threads_at_least(shader, domain_.size);

  input.unbind_as_texture();
  result.unbind_as_image();
  GPU_shader_unbind();
}

GPUShader *RealizeOnDomainOperation::get_realization_shader()
{
  switch (get_result().type()) {
    case ResultType::Color:
      return shader_manager().get("compositor_realize_on_domain_color");
    case ResultType::Vector:
      return shader_manager().get("compositor_realize_on_domain_vector");
    case ResultType::Float:
      return shader_manager().get("compositor_realize_on_domain_float");
  }

  BLI_assert_unreachable();
  return nullptr;
}

Domain RealizeOnDomainOperation::compute_domain()
{
  return domain_;
}

SimpleOperation *RealizeOnDomainOperation::construct_if_needed(
    Context &context,
    const Result &input_result,
    const InputDescriptor &input_descriptor,
    const Domain &operation_domain)
{
  /* This input wants to skip realization, the operation is not needed. */
  if (input_descriptor.skip_realization) {
    return nullptr;
  }

  /* The input expects a single value and if no single value is provided, it will be ignored and a
   * default value will be used, so no need to realize it and the operation is not needed. */
  if (input_descriptor.expects_single_value) {
    return nullptr;
  }

  /* Input result is a single value and does not need realization, the operation is not needed. */
  if (input_result.is_single_value()) {
    return nullptr;
  }

  /* The input have an identical domain to the operation domain, so no need to realize it and the
   * operation is not needed. */
  if (input_result.domain() == operation_domain) {
    return nullptr;
  }

  /* Otherwise, realization is needed. */
  return new RealizeOnDomainOperation(context, operation_domain, input_descriptor.type);
}

}  // namespace blender::realtime_compositor

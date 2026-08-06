/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sequencer
 */

#include <optional>

#include "BLI_math_rotation.hh"

#include "BKE_node_runtime.hh"

#include "PRF_profile.hh"

#include "COM_algorithm_parallel_reduction.hh"
#include "COM_ocio_color_space_conversion_shader.hh"
#include "COM_realize_on_domain_operation.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

#include "GPU_state.hh"
#include "GPU_texture_pool.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "NOD_compositor_nodes_srna.hh"

#include "RNA_access.hh"

#include "compositor.hh"

namespace blender::seq {

template<typename T>
static void set_float_array(PointerRNA *input_props_ptr, compositor::Result &result)
{
  T value;
  RNA_float_get_array(input_props_ptr, "value", value);
  result.set_single_value(value);
}

template<typename T>
static void set_int_array(PointerRNA *input_props_ptr, compositor::Result &result)
{
  T value;
  RNA_int_get_array(input_props_ptr, "value", value);
  result.set_single_value(value);
}

static void set_single_input_from_rna_value(PointerRNA *input_props_ptr,
                                            const eNodeSocketDatatype socket_type,
                                            compositor::Result &result,
                                            const std::optional<int> dimensions)
{
  using namespace nodes;

  /* Only consider inputs explicitly set to Value type. */
  if (CompositorNodesInputType(RNA_enum_get(input_props_ptr, "type")) !=
      CompositorNodesInputType::Value)
  {
    return;
  }

  switch (socket_type) {
    case SOCK_FLOAT: {
      const float value = RNA_float_get(input_props_ptr, "value");
      result.set_single_value(value);
      break;
    }
    case SOCK_VECTOR: {
      switch (dimensions.value_or(3)) {
        case 2: {
          set_float_array<float2>(input_props_ptr, result);
          break;
        }
        case 3: {
          set_float_array<float3>(input_props_ptr, result);
          break;
        }
        case 4: {
          set_float_array<float4>(input_props_ptr, result);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
      break;
    }
    case SOCK_RGBA: {
      ColorGeometry4f value;
      RNA_float_get_array(input_props_ptr, "value", value);
      result.set_single_value(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = RNA_boolean_get(input_props_ptr, "value");
      result.set_single_value(value);
      break;
    }
    case SOCK_INT: {
      const int value = RNA_int_get(input_props_ptr, "value");
      result.set_single_value(value);
      break;
    }
    case SOCK_ROTATION: {
      float3 value_euler;
      RNA_float_get_array(input_props_ptr, "value", value_euler);
      math::Quaternion value_rotation = math::to_quaternion(math::EulerXYZ(value_euler));
      result.set_single_value(value_rotation);
      break;
    }
    case SOCK_MENU: {
      const MenuValue value = MenuValue(RNA_enum_get(input_props_ptr, "value"));
      result.set_single_value(value);
      break;
    }
    case SOCK_STRING: {
      const std::string value = RNA_string_get(input_props_ptr, "value");
      result.set_single_value(value);
      break;
    }
    case SOCK_INT_VECTOR: {
      switch (dimensions.value_or(2)) {
        case 2: {
          set_int_array<int2>(input_props_ptr, result);
          break;
        }
        case 3: {
          set_int_array<int3>(input_props_ptr, result);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
      break;
    }
    case SOCK_OBJECT: {
      Object *value = RNA_pointer_get(input_props_ptr, "value").data_as<Object>();
      result.set_single_value(value);
      break;
    }
    case SOCK_FONT: {
      VFont *value = RNA_pointer_get(input_props_ptr, "value").data_as<VFont>();
      result.set_single_value(value);
      break;
    }
    case SOCK_IMAGE:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_MATERIAL:
    case SOCK_SCENE:
    case SOCK_TEXT_ID:
    case SOCK_MASK:
    case SOCK_SOUND:
    case SOCK_GEOMETRY:
    case SOCK_MATRIX:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
    case SOCK_SHADER:
    case SOCK_CUSTOM:
      break;
  }
}

static std::optional<int> get_socket_dimension(const bNodeTreeInterfaceSocket *socket,
                                               const eNodeSocketDatatype socket_type)
{
  if (socket_type == SOCK_VECTOR) {
    return static_cast<bNodeSocketValueVector *>(socket->socket_data)->dimensions;
  }
  if (socket_type == SOCK_INT_VECTOR) {
    return static_cast<bNodeSocketValueIntVector *>(socket->socket_data)->dimensions;
  }
  return {};
}

void set_input_result_from_rna(PointerRNA &inputs_ptr,
                               const bNodeTreeInterfaceSocket &socket,
                               const eNodeSocketDatatype socket_type,
                               compositor::Result &result)
{
  PointerRNA input_props_ptr = RNA_pointer_get(&inputs_ptr, socket.identifier);
  result.allocate_single_value();
  set_single_input_from_rna_value(
      &input_props_ptr, socket_type, result, get_socket_dimension(&socket, socket_type));
}

compositor::ResultPrecision CompositorContext::get_precision() const
{
  switch (this->render_data_.scene->r.compositor_precision) {
    case SCE_COMPOSITOR_PRECISION_AUTO:
      /* Auto uses full precision for final renders and half precision otherwise. */
      return this->render_data_.render ? compositor::ResultPrecision::Full :
                                         compositor::ResultPrecision::Half;
    case SCE_COMPOSITOR_PRECISION_FULL:
      return compositor::ResultPrecision::Full;
  }
  BLI_assert_unreachable();
  return compositor::ResultPrecision::Half;
}

void CompositorContext::create_result_from_input(compositor::Result &result, ImBuf &input)
{
  PRF_scope_with_name("SeqCreateCompInput", ProfileCategory::Draw);
  const bool gpu = this->use_gpu();
  const int2 size = int2(input.x, input.y);
  if (!gpu) {
    /* CPU path: ensure input is linear float. */
    ensure_ibuf_is_linear_space(&input, true);
    BLI_assert(input.float_data());
    result.share_data(input.float_data(), size);
    return;
  }

  /* GPU path: do necessary color space conversions (if any) to linear space on the GPU. */
  const bool input_is_byte = input.float_data() == nullptr;
  const char *input_colorspace = input_is_byte ? IMB_colormanagement_get_byte_colorspace(&input) :
                                                 IMB_colormanagement_get_float_colorspace(&input);
  const char *linear_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);

  bool use_fallback = true;

  if (input_is_byte || !STREQ(input_colorspace, linear_colorspace)) {
    /* Need to convert data format or colorspace: upload input into temporary texture,
     * convert into compositor result. */

    /* Get the conversion shader. */
    compositor::OCIOColorSpaceConversionShader &ocio_shader =
        this->cache_manager().ocio_color_space_conversion_shaders.get(
            *this, input_colorspace, linear_colorspace);
    gpu::Shader *shader = ocio_shader.bind_shader_and_resources();
    if (shader) {

      /* Upload input image into a GPU texture. */
      gpu::TexturePool &pool = gpu::TexturePool::get();
      gpu::Texture *input_tex = pool.acquire_texture_2d(size,
                                                        1,
                                                        input_is_byte ?
                                                            gpu::TextureFormat::UNORM_8_8_8_8 :
                                                            gpu::TextureFormat::SFLOAT_32_32_32_32,
                                                        GPU_TEXTURE_USAGE_SHADER_READ,
                                                        "seq_comp_input");
      if (input_tex) {
        if (input_is_byte) {
          GPU_texture_update(input_tex, GPU_DATA_UBYTE, input.byte_data());
        }
        else {
          GPU_texture_update(input_tex, GPU_DATA_FLOAT, input.float_data());
        }

        /* Allocate compositor result texture. We use global compositor precision even
         * for byte inputs. In theory Half precision should be enough, but that leads to potential
         * small differences between CPU & GPU paths. */
        result.set_precision(get_precision());
        result.allocate_texture(size);

        /* Convert input texture into the compositor result texture. */
        GPU_texture_bind(input_tex,
                         GPU_shader_get_sampler_binding(shader, ocio_shader.input_sampler_name()));
        result.bind_as_image(shader, ocio_shader.output_image_name());

        GPU_shader_uniform_1b(shader, "premultiply_output", input_is_byte);

        compositor::compute_dispatch_threads_at_least(shader, size);

        GPU_texture_unbind(input_tex);
        result.unbind_as_image();
        pool.release_texture(input_tex);

        use_fallback = false;
      }
      ocio_shader.unbind_shader_and_resources();
    }
  }

  /* Colorspace conversion was not needed or failed: upload input float data into
   * compositor result. */
  if (use_fallback) {
    /* This is a no-op if input is already linear float; otherwise this step might be needed
     * if conversion above has failed. */
    ensure_ibuf_is_linear_space(&input, true);
    result.allocate_texture(size);
    GPU_texture_update(result, GPU_DATA_FLOAT, input.float_data());
  }
}

void CompositorContext::write_viewer_impl(const compositor::Result &result, ImBuf &image)
{
  using namespace compositor;

  /* Realize the transforms if needed. */
  const InputDescriptor input_descriptor = {ResultType::Color,
                                            InputRealizationMode::OperationDomain};
  SimpleOperation *realization_operation = RealizeOnDomainOperation::construct_if_needed(
      *this, result, input_descriptor, result.domain());

  if (!realization_operation) {
    this->write_output(result, image);
    return;
  }

  Result realize_input = this->create_result(ResultType::Color, result.precision());
  realize_input.share_data(result);
  realization_operation->map_input_to_result(&realize_input);
  realization_operation->evaluate();

  Result &realized_result = realization_operation->get_result();
  this->write_output(realized_result, image);
  realized_result.release();
  delete realization_operation;
}

void CompositorContext::write_output(const compositor::Result &result, ImBuf &image)
{
  PRF_scope_with_name("SeqCompWriteOutput", ProfileCategory::Draw);

  if (result.is_single_value()) {
    compositor::Color color = result.get_single_value<compositor::Color>();
    IMB_rectfill(&image, color);
    image.color_mode = color.a < 1.0f ? ImColorMode::RGBA : ImColorMode::RGB;
    return;
  }

  result_translation_ = result.domain().transformation.location();
  const int output_size_x = result.domain().data_size.x;
  const int output_size_y = result.domain().data_size.y;
  if (output_size_x != image.x || output_size_y != image.y || !image.float_buffer.data) {
    /* Output size is different (e.g. image is blurred with expanded bounds);
     * need to allocate appropriately sized buffer. */
    IMB_free_all_data(&image);
    image.x = output_size_x;
    image.y = output_size_y;
  }

  compositor::Color min_color = compositor::minimum_color(*this, result);
  image.color_mode = min_color.a < 1.0f ? ImColorMode::RGBA : ImColorMode::RGB;

  if (this->use_gpu()) {
    PRF_scope_with_name("SeqCompositorGPUReadback", ProfileCategory::Draw);
    GPU_memory_barrier(GPU_BARRIER_TEXTURE_UPDATE);
    IMB_alloc_float_pixels(&image, 4, false);
    GPU_texture_read(result.gpu_texture(), GPU_DATA_FLOAT, 0, image.float_data_for_write());
  }
  else if (result.sharing_info()) {
    image.channels = 4;
    image.float_buffer = ImBufFloatBuffer{
        .data = static_cast<const float *>(result.cpu_data().data()),
        .sharing_info = result.sharing_info(),
        .colorspace = nullptr};
  }
  else if (result.cpu_data().data() != image.float_data()) {
    IMB_alloc_float_pixels(&image, 4, false);
    std::memcpy(image.float_data_for_write(),
                result.cpu_data().data(),
                IMB_get_pixel_count(&image) * sizeof(float) * 4);
  }
  const char *to_colorspace = IMB_colormanagement_role_colorspace_name_get(
      COLOR_ROLE_SCENE_LINEAR);
  IMB_colormanagement_assign_float_colorspace(&image, to_colorspace);
}

void CompositorContext::write_outputs(const bNodeTree &node_group,
                                      compositor::NodeGroupOperation &node_group_operation,
                                      ImBuf &output_image)
{
  using namespace compositor;
  for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
    Result &output_result = node_group_operation.get_result(output_socket->identifier);
    if (!output_result.should_compute()) {
      continue;
    }

    this->write_viewer_impl(output_result, output_image);
    output_result.release();
  }
}

void CompositorContext::set_output_refcount(const bNodeTree &node_group,
                                            compositor::NodeGroupOperation &node_group_operation)
{
  using namespace compositor;

  /* If the node group has no viewer node in the active context or the base context, and the
   * context requires a viewer output, we use the group output as a viewer. */
  const bke::DataBlockComputeContext base_compute_context(nullptr, this->get_scene().id);
  const bool has_viewer =
      has_viewer_node(node_group, base_compute_context, base_compute_context.hash()) ||
      has_viewer_node(node_group, base_compute_context, this->get_active_compute_context_hash());
  const bool needs_viewer_output = flag_is_set(this->needed_outputs(),
                                               NodeGroupOutputTypes::ViewerNode);
  const bool use_group_output_as_viewer = (!has_viewer && needs_viewer_output);

  const bool is_group_output_needed = render_data_.render || use_group_output_as_viewer;

  /* Set the reference count for the outputs, only the first color output is actually needed,
   * while the rest are ignored. */
  node_group.ensure_interface_cache();
  for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
    const bool is_first_output = output_socket == node_group.interface_outputs().first();
    Result &output_result = node_group_operation.get_result(output_socket->identifier);
    const bool is_color = output_result.type() == ResultType::Color;
    const bool is_needed = is_group_output_needed && is_first_output && is_color;
    output_result.set_reference_count(is_needed ? 1 : 0);
  }
}

}  // namespace blender::seq

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>
#include <string>

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"

#include "DNA_customdata_types.h"

#include "GPU_context.hh"
#include "GPU_debug.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "gpu_shader_create_info.hh"

#include "NOD_derived_node_tree.hh"

#include "COM_context.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_node.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

#include <sstream>

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

ShaderOperation::ShaderOperation(Context &context,
                                 PixelCompileUnit &compile_unit,
                                 const Schedule &schedule)
    : PixelOperation(context, compile_unit, schedule)
{
  material_ = GPU_material_from_callbacks(
      GPU_MAT_COMPOSITOR, &construct_material, &generate_code, this);
}

ShaderOperation::~ShaderOperation()
{
  GPU_material_free_single(material_);
}

void ShaderOperation::execute()
{
  GPU_debug_group_begin("ShaderOperation");
  const Domain domain = compute_domain();
  for (StringRef identifier : output_sockets_to_output_identifiers_map_.values()) {
    Result &result = get_result(identifier);
    result.allocate_texture(domain);
  }

  gpu::Shader *shader = GPU_material_get_shader(material_);
  GPU_shader_bind(shader);

  bind_material_resources(shader);
  bind_inputs(shader);
  bind_outputs(shader);

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_texture_unbind_all();
  GPU_texture_image_unbind_all();
  GPU_uniformbuf_debug_unbind_all();
  GPU_shader_unbind();
  GPU_debug_group_end();
}

void ShaderOperation::bind_material_resources(gpu::Shader *shader)
{
  /* Bind the uniform buffer of the material if it exists. It may not exist if the GPU material has
   * no uniforms. */
  gpu::UniformBuf *ubo = GPU_material_uniform_buffer_get(material_);
  if (ubo) {
    GPU_uniformbuf_bind(ubo, GPU_shader_get_ubo_binding(shader, GPU_UBO_BLOCK_NAME));
  }

  /* Bind color band textures needed by curve and ramp nodes. */
  ListBase textures = GPU_material_textures(material_);
  LISTBASE_FOREACH (GPUMaterialTexture *, texture, &textures) {
    if (texture->colorband) {
      const int texture_image_unit = GPU_shader_get_sampler_binding(shader, texture->sampler_name);
      GPU_texture_bind(*texture->colorband, texture_image_unit);
    }
  }
}

void ShaderOperation::bind_inputs(gpu::Shader *shader)
{
  /* Attributes represents the inputs of the operation and their names match those of the inputs of
   * the operation as well as the corresponding texture samples in the shader. */
  ListBase attributes = GPU_material_attributes(material_);
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    get_input(attribute->name).bind_as_texture(shader, attribute->name);
  }
}

void ShaderOperation::bind_outputs(gpu::Shader *shader)
{
  for (StringRefNull output_identifier : output_sockets_to_output_identifiers_map_.values()) {
    get_result(output_identifier).bind_as_image(shader, output_identifier.c_str());
  }
}

void ShaderOperation::construct_material(void *thunk, GPUMaterial *material)
{
  ShaderOperation *operation = static_cast<ShaderOperation *>(thunk);
  operation->material_ = material;
  for (DNode node : operation->compile_unit_) {
    operation->shader_nodes_.add_new(node, std::make_unique<ShaderNode>(node));

    operation->link_node_inputs(node);

    operation->shader_nodes_.lookup(node)->compile(material);

    operation->populate_results_for_node(node);
  }
}

void ShaderOperation::link_node_inputs(DNode node)
{
  for (int i = 0; i < node->input_sockets().size(); i++) {
    const DInputSocket input{node.context(), node->input_sockets()[i]};

    /* The input is unavailable and unused, but it still needs to be linked as this is what the GPU
     * material compiler expects. */
    if (!is_socket_available(input.bsocket())) {
      this->link_node_input_unavailable(input);
      continue;
    }

    /* The origin socket is an input, that means the input is unlinked and . */
    const DSocket origin = get_input_origin_socket(input);
    if (origin->is_input()) {
      const InputDescriptor origin_descriptor = input_descriptor_from_input_socket(
          origin.bsocket());

      if (origin_descriptor.implicit_input == ImplicitInput::None) {
        /* No implicit input, so link a constant setter node for it that holds the input value. */
        this->link_node_input_constant(input, DInputSocket(origin));
      }
      else {
        this->link_node_input_implicit(input, DInputSocket(origin));
      }
      continue;
    }

    /* Otherwise, the origin socket is an output, which means it is linked. */
    const DOutputSocket output = DOutputSocket(origin);

    /* If the origin node is part of the shader operation, then the link is internal to the GPU
     * material graph and is linked appropriately. */
    if (compile_unit_.contains(output.node())) {
      this->link_node_input_internal(input, output);
      continue;
    }

    /* Otherwise, the origin node is not part of the shader operation, then the link is external to
     * the GPU material graph and an input to the shader operation must be declared and linked to
     * the node input. */
    this->link_node_input_external(input, output);
  }
}

void ShaderOperation::link_node_input_unavailable(const DInputSocket input)
{
  ShaderNode &node = *shader_nodes_.lookup(input.node());
  GPUNodeStack &stack = node.get_input(input->identifier);

  /* Create a constant link with some zero value. The value is arbitrary and ignored. See the
   * method description. */
  zero_v4(stack.vec);
  GPUNodeLink *link = GPU_constant(stack.vec);

  GPU_link(material_, "set_float", link, &stack.link);
}

/* Initializes the vector value of the given GPU node stack from the default value of the given
 * input socket. */
static void initialize_input_stack_value(const DInputSocket input, GPUNodeStack &stack)
{
  switch (input->type) {
    case SOCK_FLOAT: {
      const float value = input->default_value_typed<bNodeSocketValueFloat>()->value;
      stack.vec[0] = value;
      break;
    }
    case SOCK_INT: {
      /* GPUMaterial doesn't support int, so it is stored as a float. */
      const int value = input->default_value_typed<bNodeSocketValueInt>()->value;
      stack.vec[0] = int(value);
      break;
    }
    case SOCK_BOOLEAN: {
      /* GPUMaterial doesn't support bool, so it is stored as a float. */
      const bool value = input->default_value_typed<bNodeSocketValueBoolean>()->value;
      stack.vec[0] = float(value);
      break;
    }
    case SOCK_VECTOR: {
      const float4 value = float4(input->default_value_typed<bNodeSocketValueVector>()->value);
      copy_v4_v4(stack.vec, value);
      break;
    }
    case SOCK_RGBA: {
      const Color value = Color(input->default_value_typed<bNodeSocketValueRGBA>()->value);
      copy_v4_v4(stack.vec, value);
      break;
    }
    case SOCK_MENU: {
      /* GPUMaterial doesn't support int, so it is stored as a float. */
      const int32_t value = input->default_value_typed<bNodeSocketValueMenu>()->value;
      stack.vec[0] = int(value);
      break;
    }
    case SOCK_STRING:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(get_node_socket_result_type(input.bsocket())));
      BLI_assert_unreachable();
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
}

static const char *get_set_function_name(const ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "set_float";
    case ResultType::Float2:
      return "set_float2";
    case ResultType::Float3:
      return "set_float3";
    case ResultType::Float4:
      return "set_float4";
    case ResultType::Color:
      return "set_color";
    case ResultType::Int:
      /* GPUMaterial doesn't support int, so it is passed as a float. */
      return "set_float";
    case ResultType::Int2:
      /* GPUMaterial doesn't support int2, so it is passed as a float2. */
      return "set_float2";
    case ResultType::Bool:
      /* GPUMaterial doesn't support bool, so it is passed as a float. */
      return "set_float";
    case ResultType::Menu:
      /* GPUMaterial doesn't support int, so it is passed as a float. */
      return "set_float";
    case ResultType::String:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void ShaderOperation::link_node_input_constant(const DInputSocket input, const DInputSocket origin)
{
  ShaderNode &node = *shader_nodes_.lookup(input.node());
  GPUNodeStack &stack = node.get_input(input->identifier);

  /* Create a constant or a uniform link that carry the value of the origin. Use a constant for
   * socket types that rarely change like booleans and menus, while use a uniform for socket type
   * that might change a lot to avoid excessive shader recompilation. */
  initialize_input_stack_value(origin, stack);
  const bool use_as_constant = ELEM(origin->type, SOCK_BOOLEAN, SOCK_MENU);
  GPUNodeLink *link = use_as_constant ? GPU_constant(stack.vec) : GPU_uniform(stack.vec);

  const ResultType type = get_node_socket_result_type(origin.bsocket());
  const char *function_name = get_set_function_name(type);
  GPU_link(material_, function_name, link, &stack.link);
}

void ShaderOperation::link_node_input_implicit(const DInputSocket input, const DInputSocket origin)
{
  ShaderNode &node = *shader_nodes_.lookup(input.node());
  GPUNodeStack &stack = node.get_input(input->identifier);

  const InputDescriptor origin_descriptor = input_descriptor_from_input_socket(origin.bsocket());
  const ImplicitInput implicit_input = origin_descriptor.implicit_input;

  /* Inherit the type and implicit input of the origin input since doing implicit conversion inside
   * the shader operation is much cheaper. */
  InputDescriptor input_descriptor = input_descriptor_from_input_socket(input.bsocket());
  input_descriptor.type = origin_descriptor.type;
  input_descriptor.implicit_input = implicit_input;

  /* An input was already declared for that implicit input, so no need to declare it again and we
   * just link it. */
  if (implicit_input_to_material_attribute_map_.contains(implicit_input)) {
    /* But first we update the domain priority of the input descriptor to be the higher priority of
     * the existing descriptor and the descriptor of the new input socket. That's because the same
     * implicit input might be used in inputs inside the shader operation which have different
     * priorities. */
    InputDescriptor &existing_input_descriptor = this->get_input_descriptor(
        implicit_inputs_to_input_identifiers_map_.lookup(implicit_input));
    existing_input_descriptor.domain_priority = math::min(
        existing_input_descriptor.domain_priority, input_descriptor.domain_priority);

    /* Link the attribute representing the shader operation input corresponding to the implicit
     * input. */
    stack.link = implicit_input_to_material_attribute_map_.lookup(implicit_input);
    return;
  }

  const int implicit_input_index = implicit_inputs_to_input_identifiers_map_.size();
  const std::string input_identifier = "implicit_input" + std::to_string(implicit_input_index);
  declare_input_descriptor(input_identifier, input_descriptor);

  /* Map the implicit input to the identifier of the operation input that was declared for it. */
  implicit_inputs_to_input_identifiers_map_.add_new(implicit_input, input_identifier);

  /* Add a new GPU attribute representing an input to the GPU material. Instead of using the
   * attribute directly, we link it to an appropriate set function and use its output link instead.
   * This is needed because the `gputype` member of the attribute is only initialized if it is
   * linked to a GPU node. */
  GPUNodeLink *attribute_link;
  GPU_link(material_,
           get_set_function_name(input_descriptor.type),
           GPU_attribute(material_, CD_AUTO_FROM_NAME, input_identifier.c_str()),
           &attribute_link);

  /* Map the implicit input to the attribute that was created for it. */
  implicit_input_to_material_attribute_map_.add(implicit_input, attribute_link);

  /* Link the attribute representing the shader operation input corresponding to the implicit
   * input. */
  stack.link = attribute_link;
}

void ShaderOperation::link_node_input_internal(DInputSocket input_socket,
                                               DOutputSocket output_socket)
{
  ShaderNode &output_node = *shader_nodes_.lookup(output_socket.node());
  GPUNodeStack &output_stack = output_node.get_output(output_socket->identifier);

  ShaderNode &input_node = *shader_nodes_.lookup(input_socket.node());
  GPUNodeStack &input_stack = input_node.get_input(input_socket->identifier);

  input_stack.link = output_stack.link;
}

void ShaderOperation::link_node_input_external(DInputSocket input_socket,
                                               DOutputSocket output_socket)
{

  ShaderNode &node = *shader_nodes_.lookup(input_socket.node());
  GPUNodeStack &stack = node.get_input(input_socket->identifier);

  if (!output_to_material_attribute_map_.contains(output_socket)) {
    /* No input was declared for that output yet, so declare it. */
    declare_operation_input(input_socket, output_socket);
  }
  else {
    /* An input was already declared for that same output socket, so no need to declare it again.
     * But we update the domain priority of the input descriptor to be the higher priority of the
     * existing descriptor and the descriptor of the new input socket. That's because the same
     * output might be connected to multiple inputs inside the shader operation which have
     * different priorities. */
    const std::string input_identifier = outputs_to_declared_inputs_map_.lookup(output_socket);
    InputDescriptor &input_descriptor = this->get_input_descriptor(input_identifier);
    input_descriptor.domain_priority = math::min(
        input_descriptor.domain_priority,
        input_descriptor_from_input_socket(input_socket.bsocket()).domain_priority);

    /* Increment the input's reference count. */
    inputs_to_reference_counts_map_.lookup(input_identifier)++;
  }

  /* Link the attribute representing the shader operation input corresponding to the given output
   * socket. */
  stack.link = output_to_material_attribute_map_.lookup(output_socket);
}

void ShaderOperation::declare_operation_input(DInputSocket input_socket,
                                              DOutputSocket output_socket)
{
  const int input_index = output_to_material_attribute_map_.size();
  std::string input_identifier = "input" + std::to_string(input_index);

  /* Declare the input descriptor for this input and prefer to declare its type to be the same as
   * the type of the output socket because doing type conversion in the shader is much cheaper. */
  InputDescriptor input_descriptor = input_descriptor_from_input_socket(input_socket.bsocket());
  input_descriptor.type = get_node_socket_result_type(output_socket.bsocket());
  declare_input_descriptor(input_identifier, input_descriptor);

  /* Add a new GPU attribute representing an input to the GPU material. Instead of using the
   * attribute directly, we link it to an appropriate set function and use its output link instead.
   * This is needed because the `gputype` member of the attribute is only initialized if it is
   * linked to a GPU node. */
  GPUNodeLink *attribute_link;
  GPU_link(material_,
           get_set_function_name(input_descriptor.type),
           GPU_attribute(material_, CD_AUTO_FROM_NAME, input_identifier.c_str()),
           &attribute_link);

  /* Map the output socket to the attribute that was created for it. */
  output_to_material_attribute_map_.add(output_socket, attribute_link);

  /* Map the identifier of the operation input to the output socket it is linked to. */
  inputs_to_linked_outputs_map_.add_new(input_identifier, output_socket);

  /* Map the output socket to the identifier of the operation input that was declared for it. */
  outputs_to_declared_inputs_map_.add_new(output_socket, input_identifier);

  /* Map the identifier of the operation input to a reference count of 1, this will later be
   * incremented if that same output was referenced again. */
  inputs_to_reference_counts_map_.add_new(input_identifier, 1);
}

void ShaderOperation::populate_results_for_node(DNode node)
{
  const DOutputSocket preview_output = find_preview_output_socket(node);

  for (const bNodeSocket *output : node->output_sockets()) {
    const DOutputSocket doutput{node.context(), output};

    if (!is_socket_available(output)) {
      continue;
    }

    /* If any of the nodes linked to the output are not part of the shader operation but are part
     * of the execution schedule, then an output result needs to be populated for it. */
    const bool is_operation_output = is_output_linked_to_node_conditioned(
        doutput,
        [&](DNode node) { return schedule_.contains(node) && !compile_unit_.contains(node); });

    /* If the output is used as the node preview, then an output result needs to be populated for
     * it, and we additionally keep track of that output to later compute the previews from. */
    const bool is_preview_output = doutput == preview_output;
    if (is_preview_output) {
      preview_outputs_.add(doutput);
    }

    if (is_operation_output || is_preview_output) {
      populate_operation_result(doutput);
    }
  }
}

static const char *get_store_function_name(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "node_compositor_store_output_float";
    case ResultType::Float2:
      return "node_compositor_store_output_float2";
    case ResultType::Float3:
      return "node_compositor_store_output_float3";
    case ResultType::Float4:
      return "node_compositor_store_output_float4";
    case ResultType::Color:
      return "node_compositor_store_output_color";
    case ResultType::Int:
      return "node_compositor_store_output_int";
    case ResultType::Int2:
      return "node_compositor_store_output_int2";
    case ResultType::Bool:
      return "node_compositor_store_output_bool";
    case ResultType::Menu:
      return "node_compositor_store_output_menu";
    case ResultType::String:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void ShaderOperation::populate_operation_result(DOutputSocket output_socket)
{
  const uint output_id = output_sockets_to_output_identifiers_map_.size();
  std::string output_identifier = "output" + std::to_string(output_id);

  const ResultType result_type = get_node_socket_result_type(output_socket.bsocket());
  const Result result = context().create_result(result_type);
  populate_result(output_identifier, result);

  /* Map the output socket to the identifier of the newly populated result. */
  output_sockets_to_output_identifiers_map_.add_new(output_socket, output_identifier);

  ShaderNode &node = *shader_nodes_.lookup(output_socket.node());
  GPUNodeLink *output_link = node.get_output(output_socket->identifier).link;

  /* Link the output node stack to an output storer storing in the appropriate result. The result
   * is identified by its index in the operation and the index is encoded as a float to be passed
   * to the GPU function. Additionally, create an output link from the storer node to declare as an
   * output to the GPU material. This storer output link is a dummy link in the sense that its
   * value is ignored since it is already written in the output, but it is used to track nodes that
   * contribute to the output of the compositor node tree. */
  GPUNodeLink *storer_output_link;
  GPUNodeLink *id_link = GPU_constant((float *)&output_id);
  const char *store_function_name = get_store_function_name(result_type);
  GPU_link(material_, store_function_name, id_link, output_link, &storer_output_link);

  /* Declare the output link of the storer node as an output of the GPU material to help the GPU
   * code generator to track the nodes that contribute to the output of the shader. */
  GPU_material_add_output_link_composite(material_, storer_output_link);
}

using namespace gpu::shader;

void ShaderOperation::generate_code(void *thunk,
                                    GPUMaterial *material,
                                    GPUCodegenOutput *code_generator_output)
{
  ShaderOperation *operation = static_cast<ShaderOperation *>(thunk);
  ShaderCreateInfo &shader_create_info = *reinterpret_cast<ShaderCreateInfo *>(
      code_generator_output->create_info);

  shader_create_info.local_group_size(16, 16);

  /* Add implementation for the functions inserted by the code generator.. */
  shader_create_info.typedef_source("gpu_shader_compositor_code_generation.glsl");

  /* The source shader is a compute shader with a main function that calls the dynamically
   * generated evaluate function. The evaluate function includes the serialized GPU material graph
   * preceded by code that initialized the inputs of the operation. Additionally, the storer
   * functions that writes the outputs are defined outside the evaluate function. */
  shader_create_info.compute_source("gpu_shader_compositor_main.glsl");

  std::string store_code = operation->generate_code_for_outputs(shader_create_info);
  shader_create_info.generated_sources.append(
      {"gpu_shader_compositor_store.glsl", {}, store_code});

  std::string eval_code;
  eval_code += "void evaluate()\n{\n";

  eval_code += operation->generate_code_for_inputs(material, shader_create_info);

  eval_code += code_generator_output->composite.serialized;

  eval_code += "}\n";

  shader_create_info.generated_sources.append({"gpu_shader_compositor_eval.glsl",
                                               code_generator_output->composite.dependencies,
                                               eval_code});
}

/* Texture storers in the shader always take a [i]vec4 as an argument, so encode each type in an
 * [i]vec4 appropriately. */
static const char *glsl_store_expression_from_result_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "vec4(value)";
    case ResultType::Float2:
      return "vec4(value, 0.0f, 0.0f)";
    case ResultType::Float3:
      return "vec4(value, 0.0f)";
    case ResultType::Float4:
      return "value";
    case ResultType::Color:
      return "value";
    case ResultType::Int:
      /* GPUMaterial doesn't support int, so it is passed as a float, and we need to convert it
       * back to int before writing it. */
      return "ivec4(int(value))";
    case ResultType::Int2:
      /* GPUMaterial doesn't support int2, so it is passed as a float2, and we need to convert it
       * back to int2 before writing it. */
      return "ivec4(ivec2(value), 0, 0)";
    case ResultType::Bool:
      /* GPUMaterial doesn't support bool, so it is passed as a float and stored as an int, and we
       * need to convert it back to bool and then to an int before writing it. */
      return "ivec4(bool(value))";
    case ResultType::Menu:
      /* GPUMaterial doesn't support int, so it is passed as a float, and we need to convert it
       * back to int before writing it. */
      return "ivec4(int(value))";
    case ResultType::String:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

static ImageType gpu_image_type_from_result_type(const ResultType type)
{
  switch (type) {
    case ResultType::Float:
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Color:
    case ResultType::Float4:
      return ImageType::Float2D;
    case ResultType::Int:
    case ResultType::Int2:
    case ResultType::Bool:
    case ResultType::Menu:
      return ImageType::Int2D;
    case ResultType::String:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return ImageType::Float2D;
}

std::string ShaderOperation::generate_code_for_outputs(ShaderCreateInfo &shader_create_info)
{
  const std::string store_float_function_header = "void store_float(const uint id, float value)";
  const std::string store_float2_function_header = "void store_float2(const uint id, vec2 value)";
  const std::string store_float3_function_header = "void store_float3(const uint id, vec3 value)";
  const std::string store_float4_function_header = "void store_float4(const uint id, vec4 value)";
  const std::string store_color_function_header = "void store_color(const uint id, vec4 value)";
  /* GPUMaterial doesn't support int, so it is passed as a float. */
  const std::string store_int_function_header = "void store_int(const uint id, float value)";
  /* GPUMaterial doesn't support int2, so it is passed as a float2. */
  const std::string store_int2_function_header = "void store_int2(const uint id, vec2 value)";
  /* GPUMaterial doesn't support bool, so it is passed as a float. */
  const std::string store_bool_function_header = "void store_bool(const uint id, float value)";
  /* GPUMaterial doesn't support int, so it is passed as a float. */
  const std::string store_menu_function_header = "void store_menu(const uint id, float value)";

  /* Each of the store functions is essentially a single switch case on the given ID, so start by
   * opening the function with a curly bracket followed by opening a switch statement in each of
   * the functions. */
  std::stringstream store_float_function;
  std::stringstream store_float2_function;
  std::stringstream store_float3_function;
  std::stringstream store_float4_function;
  std::stringstream store_color_function;
  std::stringstream store_int_function;
  std::stringstream store_int2_function;
  std::stringstream store_bool_function;
  std::stringstream store_menu_function;
  const std::string store_function_start = "\n{\n  switch (id) {\n";
  store_float_function << store_float_function_header << store_function_start;
  store_float2_function << store_float2_function_header << store_function_start;
  store_float3_function << store_float3_function_header << store_function_start;
  store_float4_function << store_float4_function_header << store_function_start;
  store_color_function << store_color_function_header << store_function_start;
  store_int_function << store_int_function_header << store_function_start;
  store_int2_function << store_int2_function_header << store_function_start;
  store_bool_function << store_bool_function_header << store_function_start;
  store_menu_function << store_menu_function_header << store_function_start;

  shader_create_info.builtins(BuiltinBits::GLOBAL_INVOCATION_ID);

  int output_index = 0;
  for (StringRefNull output_identifier : output_sockets_to_output_identifiers_map_.values()) {
    const Result &result = get_result(output_identifier);

    /* Add a write-only image for this output where its values will be written. */
    shader_create_info.image(output_index,
                             result.get_gpu_texture_format(),
                             Qualifier::write,
                             ImageReadWriteType(gpu_image_type_from_result_type(result.type())),
                             output_identifier,
                             Frequency::PASS);
    output_index++;

    /* Add a case for the index of this output followed by a break statement. */
    std::stringstream case_code;
    const std::string store_expression = glsl_store_expression_from_result_type(result.type());
    const std::string texel = ", ivec2(gl_GlobalInvocationID.xy), ";
    case_code << "    case " << StringRef(output_identifier).drop_known_prefix("output") << ":\n"
              << "      imageStore(" << output_identifier << texel << store_expression << ");\n"
              << "      break;\n";

    /* Only add the case to the function with the matching type. */
    switch (result.type()) {
      case ResultType::Float:
        store_float_function << case_code.str();
        break;
      case ResultType::Float2:
        store_float2_function << case_code.str();
        break;
      case ResultType::Float3:
        store_float3_function << case_code.str();
        break;
      case ResultType::Float4:
        store_float4_function << case_code.str();
        break;
      case ResultType::Color:
        store_color_function << case_code.str();
        break;
      case ResultType::Int:
        store_int_function << case_code.str();
        break;
      case ResultType::Int2:
        store_int2_function << case_code.str();
        break;
      case ResultType::Bool:
        store_bool_function << case_code.str();
        break;
      case ResultType::Menu:
        store_menu_function << case_code.str();
        break;
      case ResultType::String:
        /* Single only types do not support GPU code path. */
        BLI_assert(Result::is_single_value_only_type(result.type()));
        BLI_assert_unreachable();
        break;
    }
  }

  /* Close the previously opened switch statement as well as the function itself. */
  const std::string store_function_end = "  }\n}\n\n";
  store_float_function << store_function_end;
  store_float2_function << store_function_end;
  store_float3_function << store_function_end;
  store_float4_function << store_function_end;
  store_color_function << store_function_end;
  store_int_function << store_function_end;
  store_int2_function << store_function_end;
  store_bool_function << store_function_end;
  store_menu_function << store_function_end;

  return store_float_function.str() + store_float2_function.str() + store_float3_function.str() +
         store_float4_function.str() + store_color_function.str() + store_int_function.str() +
         store_int2_function.str() + store_bool_function.str() + store_menu_function.str();
}

static const char *glsl_type_from_result_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "float";
    case ResultType::Float2:
      return "vec2";
    case ResultType::Float3:
      return "vec3";
    case ResultType::Float4:
      return "vec4";
    case ResultType::Color:
      return "vec4";
    case ResultType::Int:
      /* GPUMaterial doesn't support int, so it is passed as a float. */
      return "float";
    case ResultType::Int2:
      /* GPUMaterial doesn't support int2, so it is passed as a float2. */
      return "vec2";
    case ResultType::Bool:
      /* GPUMaterial doesn't support bool, so it is passed as a float. */
      return "float";
    case ResultType::Menu:
      /* GPUMaterial doesn't support int, so it is passed as a float. */
      return "float";
    case ResultType::String:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

/* Texture loaders in the shader always return an [i]vec4, so a swizzle is needed to retrieve the
 * actual value for each type. */
static const char *glsl_swizzle_from_result_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "x";
    case ResultType::Float2:
      return "xy";
    case ResultType::Float3:
      return "xyz";
    case ResultType::Float4:
      return "xyzw";
    case ResultType::Color:
      return "rgba";
    case ResultType::Int:
      return "x";
    case ResultType::Int2:
      /* GPUMaterial doesn't support float2, so it is passed as a float2. */
      return "xy";
    case ResultType::Bool:
      return "x";
    case ResultType::Menu:
      return "x";
    case ResultType::String:
      /* Single only types do not support GPU code path. */
      BLI_assert(Result::is_single_value_only_type(type));
      BLI_assert_unreachable();
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

std::string ShaderOperation::generate_code_for_inputs(GPUMaterial *material,
                                                      ShaderCreateInfo &shader_create_info)
{
  /* The attributes of the GPU material represents the inputs of the operation. */
  ListBase attributes = GPU_material_attributes(material);

  if (BLI_listbase_is_empty(&attributes)) {
    return "";
  }

  std::string code;

  /* Add a texture sampler for each of the inputs with the same name as the attribute, we start
   * counting the sampler slot location from the number of textures in the material, since some
   * sampler slots may be reserved for things like color band textures. */
  const ListBase textures = GPU_material_textures(material);
  int input_slot_location = BLI_listbase_count(&textures);
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    const InputDescriptor &input_descriptor = get_input_descriptor(attribute->name);
    shader_create_info.sampler(input_slot_location,
                               gpu_image_type_from_result_type(input_descriptor.type),
                               attribute->name,
                               Frequency::PASS);
    input_slot_location++;
  }

  /* Declare a struct called var_attrs that includes an appropriately typed member for each of the
   * inputs. The names of the members should be the letter v followed by the ID of the attribute
   * corresponding to the input. Such names are expected by the code generator. */
  std::stringstream declare_attributes;
  declare_attributes << "struct {\n";
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    const InputDescriptor &input_descriptor = get_input_descriptor(attribute->name);
    const std::string type = glsl_type_from_result_type(input_descriptor.type);
    declare_attributes << "  " << type << " v" << attribute->id << ";\n";
  }
  declare_attributes << "} var_attrs;\n\n";

  code += declare_attributes.str();

  /* The texture loader utilities are needed to sample the input textures and initialize the
   * attributes. */
  shader_create_info.typedef_source("gpu_shader_compositor_texture_utilities.glsl");

  /* Initialize each member of the previously declared struct by loading its corresponding texture
   * with an appropriate swizzle and cast for its type. */
  std::stringstream initialize_attributes;
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    const InputDescriptor &input_descriptor = get_input_descriptor(attribute->name);
    const std::string swizzle = glsl_swizzle_from_result_type(input_descriptor.type);
    const std::string type = glsl_type_from_result_type(input_descriptor.type);
    initialize_attributes << "var_attrs.v" << attribute->id << " = " << type << "("
                          << "texture_load(" << attribute->name
                          << ", ivec2(gl_GlobalInvocationID.xy))." << swizzle << ")"
                          << ";\n";
  }
  initialize_attributes << "\n";

  code += initialize_attributes.str();

  return code;
}

}  // namespace blender::compositor

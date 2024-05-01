/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>
#include <string>

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"

#include "GPU_context.hh"
#include "GPU_material.hh"
#include "GPU_shader.hh"
#include "GPU_texture.hh"
#include "GPU_uniform_buffer.hh"

#include "gpu_shader_create_info.hh"

#include "NOD_derived_node_tree.hh"
#include "NOD_node_declaration.hh"

#include "COM_context.hh"
#include "COM_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_shader_node.hh"
#include "COM_shader_operation.hh"
#include "COM_utilities.hh"

#include <sstream>

namespace blender::realtime_compositor {

using namespace nodes::derived_node_tree_types;

ShaderOperation::ShaderOperation(Context &context,
                                 ShaderCompileUnit &compile_unit,
                                 const Schedule &schedule)
    : Operation(context), schedule_(schedule), compile_unit_(compile_unit)
{
  material_ = GPU_material_from_callbacks(
      GPU_MAT_COMPOSITOR, &construct_material, &generate_code, this);
  GPU_material_status_set(material_, GPU_MAT_QUEUED);
  GPU_material_compile(material_);
}

ShaderOperation::~ShaderOperation()
{
  GPU_material_free_single(material_);
}

void ShaderOperation::execute()
{
  const Domain domain = compute_domain();
  for (StringRef identifier : output_sockets_to_output_identifiers_map_.values()) {
    Result &result = get_result(identifier);
    result.allocate_texture(domain);
  }

  GPUShader *shader = GPU_material_get_shader(material_);
  GPU_shader_bind(shader);

  bind_material_resources(shader);
  bind_inputs(shader);
  bind_outputs(shader);

  compute_dispatch_threads_at_least(shader, domain.size);

  GPU_texture_unbind_all();
  GPU_texture_image_unbind_all();
  GPU_uniformbuf_debug_unbind_all();
  GPU_shader_unbind();
}

void ShaderOperation::compute_preview()
{
  for (const DOutputSocket &output : preview_outputs_) {
    Result &result = get_result(get_output_identifier_from_output_socket(output));
    compute_preview_from_result(context(), output.node(), result);
    result.release();
  }
}

StringRef ShaderOperation::get_output_identifier_from_output_socket(DOutputSocket output_socket)
{
  return output_sockets_to_output_identifiers_map_.lookup(output_socket);
}

Map<std::string, DOutputSocket> &ShaderOperation::get_inputs_to_linked_outputs_map()
{
  return inputs_to_linked_outputs_map_;
}

void ShaderOperation::compute_results_reference_counts(const Schedule &schedule)
{
  for (const auto item : output_sockets_to_output_identifiers_map_.items()) {
    int reference_count = number_of_inputs_linked_to_output_conditioned(
        item.key, [&](DInputSocket input) {
          /* We only consider inputs that are not part of the shader operations, because inputs
           * that are part of the shader operations are internal and do not deal with the result
           * directly. */
          return schedule.contains(input.node()) && !compile_unit_.contains(input.node());
        });

    if (preview_outputs_.contains(item.key)) {
      reference_count++;
    }

    get_result(item.value).set_initial_reference_count(reference_count);
  }
}

void ShaderOperation::bind_material_resources(GPUShader *shader)
{
  /* Bind the uniform buffer of the material if it exists. It may not exist if the GPU material has
   * no uniforms. */
  GPUUniformBuf *ubo = GPU_material_uniform_buffer_get(material_);
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

void ShaderOperation::bind_inputs(GPUShader *shader)
{
  /* Attributes represents the inputs of the operation and their names match those of the inputs of
   * the operation as well as the corresponding texture samples in the shader. */
  ListBase attributes = GPU_material_attributes(material_);
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    get_input(attribute->name).bind_as_texture(shader, attribute->name);
  }
}

void ShaderOperation::bind_outputs(GPUShader *shader)
{
  for (StringRefNull output_identifier : output_sockets_to_output_identifiers_map_.values()) {
    get_result(output_identifier).bind_as_image(shader, output_identifier.c_str());
  }
}

void ShaderOperation::construct_material(void *thunk, GPUMaterial *material)
{
  ShaderOperation *operation = static_cast<ShaderOperation *>(thunk);
  for (DNode node : operation->compile_unit_) {
    ShaderNode *shader_node = node->typeinfo->get_compositor_shader_node(node);
    operation->shader_nodes_.add_new(node, std::unique_ptr<ShaderNode>(shader_node));

    operation->link_node_inputs(node, material);

    shader_node->compile(material);

    operation->populate_results_for_node(node, material);
  }
}

void ShaderOperation::link_node_inputs(DNode node, GPUMaterial *material)
{
  for (const bNodeSocket *input : node->input_sockets()) {
    const DInputSocket dinput{node.context(), input};

    /* Get the output linked to the input. If it is null, that means the input is unlinked.
     * Unlinked inputs are linked by the node compile method, so skip this here. */
    const DOutputSocket doutput = get_output_linked_to_input(dinput);
    if (!doutput) {
      continue;
    }

    /* If the origin node is part of the shader operation, then the link is internal to the GPU
     * material graph and is linked appropriately. */
    if (compile_unit_.contains(doutput.node())) {
      link_node_input_internal(dinput, doutput);
      continue;
    }

    /* Otherwise, the origin node is not part of the shader operation, then the link is external to
     * the GPU material graph and an input to the shader operation must be declared and linked to
     * the node input. */
    link_node_input_external(dinput, doutput, material);
  }
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
                                               DOutputSocket output_socket,
                                               GPUMaterial *material)
{

  ShaderNode &node = *shader_nodes_.lookup(input_socket.node());
  GPUNodeStack &stack = node.get_input(input_socket->identifier);

  /* An input was already declared for that same output socket, so no need to declare it again. */
  if (!output_to_material_attribute_map_.contains(output_socket)) {
    declare_operation_input(input_socket, output_socket, material);
  }

  /* Link the attribute representing the shader operation input corresponding to the given output
   * socket. */
  stack.link = output_to_material_attribute_map_.lookup(output_socket);
}

static const char *get_set_function_name(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "set_value";
    case ResultType::Vector:
      return "set_rgb";
    case ResultType::Color:
      return "set_rgba";
    default:
      /* Other types are internal and needn't be handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void ShaderOperation::declare_operation_input(DInputSocket input_socket,
                                              DOutputSocket output_socket,
                                              GPUMaterial *material)
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
  GPU_link(material,
           get_set_function_name(input_descriptor.type),
           GPU_attribute(material, CD_AUTO_FROM_NAME, input_identifier.c_str()),
           &attribute_link);

  /* Map the output socket to the attribute that was created for it. */
  output_to_material_attribute_map_.add(output_socket, attribute_link);

  /* Map the identifier of the operation input to the output socket it is linked to. */
  inputs_to_linked_outputs_map_.add_new(input_identifier, output_socket);
}

void ShaderOperation::populate_results_for_node(DNode node, GPUMaterial *material)
{
  const DOutputSocket preview_output = find_preview_output_socket(node);

  for (const bNodeSocket *output : node->output_sockets()) {
    const DOutputSocket doutput{node.context(), output};

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
      populate_operation_result(doutput, material);
    }
  }
}

static const char *get_store_function_name(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "node_compositor_store_output_float";
    case ResultType::Vector:
      return "node_compositor_store_output_vector";
    case ResultType::Color:
      return "node_compositor_store_output_color";
    default:
      /* Other types are internal and needn't be handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void ShaderOperation::populate_operation_result(DOutputSocket output_socket, GPUMaterial *material)
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
  GPU_link(material, store_function_name, id_link, output_link, &storer_output_link);

  /* Declare the output link of the storer node as an output of the GPU material to help the GPU
   * code generator to track the nodes that contribute to the output of the shader. */
  GPU_material_add_output_link_composite(material, storer_output_link);
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

  /* The resources are added without explicit locations, so make sure it is done by the
   * shader creator. */
  shader_create_info.auto_resource_location(true);

  /* Add implementation for implicit conversion operations inserted by the code generator. This
   * file should include the functions [float|vec3|vec4]_from_[float|vec3|vec4]. */
  shader_create_info.typedef_source("gpu_shader_compositor_type_conversion.glsl");

  /* The source shader is a compute shader with a main function that calls the dynamically
   * generated evaluate function. The evaluate function includes the serialized GPU material graph
   * preceded by code that initialized the inputs of the operation. Additionally, the storer
   * functions that writes the outputs are defined outside the evaluate function. */
  shader_create_info.compute_source("gpu_shader_compositor_main.glsl");

  /* The main function is emitted in the shader before the evaluate function, so the evaluate
   * function needs to be forward declared here.
   * NOTE(Metal): Metal does not require forward declarations. */
  if (GPU_backend_get_type() != GPU_BACKEND_METAL) {
    shader_create_info.typedef_source_generated += "void evaluate();\n";
  }

  operation->generate_code_for_outputs(shader_create_info);

  shader_create_info.compute_source_generated += "void evaluate()\n{\n";

  operation->generate_code_for_inputs(material, shader_create_info);

  shader_create_info.compute_source_generated += code_generator_output->composite;

  shader_create_info.compute_source_generated += "}\n";
}

/* Texture storers in the shader always take a vec4 as an argument, so encode each type in a vec4
 * appropriately. */
static const char *glsl_store_expression_from_result_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "vec4(value)";
    case ResultType::Vector:
      return "vec4(vector, 0.0)";
    case ResultType::Color:
      return "color";
    default:
      /* Other types are internal and needn't be handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void ShaderOperation::generate_code_for_outputs(ShaderCreateInfo &shader_create_info)
{
  const std::string store_float_function_header = "void store_float(const uint id, float value)";
  const std::string store_vector_function_header = "void store_vector(const uint id, vec3 vector)";
  const std::string store_color_function_header = "void store_color(const uint id, vec4 color)";

  /* The store functions are used by the node_compositor_store_output_[float|vector|color]
   * functions but are only defined later as part of the compute source, so they need to be forward
   * declared.
   * NOTE(Metal): Metal does not require forward declarations. */
  if (GPU_backend_get_type() != GPU_BACKEND_METAL) {
    shader_create_info.typedef_source_generated += store_float_function_header + ";\n";
    shader_create_info.typedef_source_generated += store_vector_function_header + ";\n";
    shader_create_info.typedef_source_generated += store_color_function_header + ";\n";
  }

  /* Each of the store functions is essentially a single switch case on the given ID, so start by
   * opening the function with a curly bracket followed by opening a switch statement in each of
   * the functions. */
  std::stringstream store_float_function;
  std::stringstream store_vector_function;
  std::stringstream store_color_function;
  const std::string store_function_start = "\n{\n  switch (id) {\n";
  store_float_function << store_float_function_header << store_function_start;
  store_vector_function << store_vector_function_header << store_function_start;
  store_color_function << store_color_function_header << store_function_start;

  for (StringRefNull output_identifier : output_sockets_to_output_identifiers_map_.values()) {
    const Result &result = get_result(output_identifier);

    /* Add a write-only image for this output where its values will be written. */
    shader_create_info.image(0,
                             result.get_texture_format(),
                             Qualifier::WRITE,
                             ImageType::FLOAT_2D,
                             output_identifier,
                             Frequency::PASS);

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
      case ResultType::Vector:
        store_vector_function << case_code.str();
        break;
      case ResultType::Color:
        store_color_function << case_code.str();
        break;
      default:
        /* Other types are internal and needn't be handled by operations. */
        BLI_assert_unreachable();
        break;
    }
  }

  /* Close the previously opened switch statement as well as the function itself. */
  const std::string store_function_end = "  }\n}\n\n";
  store_float_function << store_function_end;
  store_vector_function << store_function_end;
  store_color_function << store_function_end;

  shader_create_info.compute_source_generated += store_float_function.str() +
                                                 store_vector_function.str() +
                                                 store_color_function.str();
}

static const char *glsl_type_from_result_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "float";
    case ResultType::Vector:
      return "vec3";
    case ResultType::Color:
      return "vec4";
    default:
      /* Other types are internal and needn't be handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

/* Texture loaders in the shader always return a vec4, so a swizzle is needed to retrieve the
 * actual value for each type. */
static const char *glsl_swizzle_from_result_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return "x";
    case ResultType::Vector:
      return "xyz";
    case ResultType::Color:
      return "rgba";
    default:
      /* Other types are internal and needn't be handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

void ShaderOperation::generate_code_for_inputs(GPUMaterial *material,
                                               ShaderCreateInfo &shader_create_info)
{
  /* The attributes of the GPU material represents the inputs of the operation. */
  ListBase attributes = GPU_material_attributes(material);

  if (BLI_listbase_is_empty(&attributes)) {
    return;
  }

  /* Add a texture sampler for each of the inputs with the same name as the attribute. */
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    shader_create_info.sampler(0, ImageType::FLOAT_2D, attribute->name, Frequency::PASS);
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

  shader_create_info.compute_source_generated += declare_attributes.str();

  /* The texture loader utilities are needed to sample the input textures and initialize the
   * attributes. */
  shader_create_info.typedef_source("gpu_shader_compositor_texture_utilities.glsl");

  /* Initialize each member of the previously declared struct by loading its corresponding texture
   * with an appropriate swizzle for its type. */
  std::stringstream initialize_attributes;
  LISTBASE_FOREACH (GPUMaterialAttribute *, attribute, &attributes) {
    const InputDescriptor &input_descriptor = get_input_descriptor(attribute->name);
    const std::string swizzle = glsl_swizzle_from_result_type(input_descriptor.type);
    initialize_attributes << "var_attrs.v" << attribute->id << " = "
                          << "texture_load(" << attribute->name
                          << ", ivec2(gl_GlobalInvocationID.xy))." << swizzle << ";\n";
  }
  initialize_attributes << "\n";

  shader_create_info.compute_source_generated += initialize_attributes.str();
}

}  // namespace blender::realtime_compositor

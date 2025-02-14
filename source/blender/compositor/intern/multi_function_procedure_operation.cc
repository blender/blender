/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>
#include <string>

#include "BLI_assert.h"
#include "BLI_cpp_type.hh"
#include "BLI_generic_span.hh"
#include "BLI_index_mask.hh"
#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "FN_multi_function.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_context.hh"
#include "FN_multi_function_data_type.hh"
#include "FN_multi_function_procedure.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"
#include "FN_multi_function_procedure_optimization.hh"

#include "DNA_node_types.h"

#include "NOD_derived_node_tree.hh"
#include "NOD_multi_function.hh"

#include "COM_context.hh"
#include "COM_domain.hh"
#include "COM_input_descriptor.hh"
#include "COM_multi_function_procedure_operation.hh"
#include "COM_pixel_operation.hh"
#include "COM_result.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"
#include "COM_utilities_type_conversion.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

MultiFunctionProcedureOperation::MultiFunctionProcedureOperation(Context &context,
                                                                 PixelCompileUnit &compile_unit,
                                                                 const Schedule &schedule)
    : PixelOperation(context, compile_unit, schedule), procedure_builder_(procedure_)
{
  this->build_procedure();
  procedure_executor_ = std::make_unique<mf::ProcedureExecutor>(procedure_);
}

static const CPPType &get_cpp_type(ResultType type)
{
  switch (type) {
    case ResultType::Float:
      return CPPType::get<float>();
    case ResultType::Int:
      return CPPType::get<int>();
    case ResultType::Vector:
    case ResultType::Color:
      return CPPType::get<float4>();
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Int2:
      /* Those types are internal and needn't be handled by operations. */
      break;
  }

  BLI_assert_unreachable();
  return CPPType::get<float>();
}

/* Adds the single value input parameter of the given input to the given parameter_builder. */
static void add_single_value_input_parameter(mf::ParamsBuilder &parameter_builder,
                                             const Result &input)
{
  BLI_assert(input.is_single_value());
  switch (input.type()) {
    case ResultType::Float:
      parameter_builder.add_readonly_single_input_value(input.get_single_value<float>());
      return;
    case ResultType::Int:
      parameter_builder.add_readonly_single_input_value(input.get_single_value<int>());
      return;
    case ResultType::Color:
      parameter_builder.add_readonly_single_input_value(input.get_single_value<float4>());
      return;
    case ResultType::Vector:
      parameter_builder.add_readonly_single_input_value(input.get_single_value<float4>());
      return;
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Int2:
      /* Those types are internal and needn't be handled by operations. */
      BLI_assert_unreachable();
      break;
  }
}

/* Adds the single value output parameter of the given output to the given parameter_builder. */
static void add_single_value_output_parameter(mf::ParamsBuilder &parameter_builder, Result &output)
{
  output.allocate_single_value();
  switch (output.type()) {
    case ResultType::Float:
      parameter_builder.add_uninitialized_single_output(&output.get_single_value<float>());
      return;
    case ResultType::Int:
      parameter_builder.add_uninitialized_single_output(&output.get_single_value<int>());
      return;
    case ResultType::Color:
      parameter_builder.add_uninitialized_single_output(&output.get_single_value<float4>());
      return;
    case ResultType::Vector:
      parameter_builder.add_uninitialized_single_output(&output.get_single_value<float4>());
      return;
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Int2:
      /* Those types are internal and needn't be handled by operations. */
      BLI_assert_unreachable();
      break;
  }
}

/* Upload the single value output value to the GPU. The set_single_value method already does that,
 * so we can call it on its own value. */
static void upload_single_value_output_to_gpu(Result &output)
{
  switch (output.type()) {
    case ResultType::Float:
      output.set_single_value(output.get_single_value<float>());
      return;
    case ResultType::Int:
      output.set_single_value(output.get_single_value<int>());
      return;
    case ResultType::Color:
      output.set_single_value(output.get_single_value<float4>());
      return;
    case ResultType::Vector:
      output.set_single_value(output.get_single_value<float4>());
      return;
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Int2:
      /* Those types are internal and needn't be handled by operations. */
      BLI_assert_unreachable();
      break;
  }
}

void MultiFunctionProcedureOperation::execute()
{
  const Domain domain = compute_domain();
  const int64_t size = int64_t(domain.size.x) * domain.size.y;
  const IndexMask mask = IndexMask(size);
  mf::ParamsBuilder parameter_builder{*procedure_executor_, &mask};

  const bool is_single_value = this->is_single_value_operation();

  /* For each of the parameters, either add an input or an output depending on its interface type,
   * allocating the outputs when needed. */
  for (int i = 0; i < procedure_.params().size(); i++) {
    if (procedure_.params()[i].type == mf::ParamType::InterfaceType::Input) {
      Result &input = get_input(parameter_identifiers_[i]);
      if (input.is_single_value()) {
        add_single_value_input_parameter(parameter_builder, input);
      }
      else {
        const GSpan span{get_cpp_type(input.type()), input.data(), size};
        parameter_builder.add_readonly_single_input(span);
      }
    }
    else {
      Result &output = get_result(parameter_identifiers_[i]);
      if (is_single_value) {
        add_single_value_output_parameter(parameter_builder, output);
      }
      else {
        output.allocate_texture(domain);
        const GMutableSpan span{get_cpp_type(output.type()), output.data(), size};
        parameter_builder.add_uninitialized_single_output(span);
      }
    }
  }

  mf::ContextBuilder context_builder;
  procedure_executor_->call_auto(mask, parameter_builder, context_builder);

  /* In case of single value GPU execution, the single values need to be uploaded to the GPU. */
  if (is_single_value && this->context().use_gpu()) {
    for (int i = 0; i < procedure_.params().size(); i++) {
      if (procedure_.params()[i].type == mf::ParamType::InterfaceType::Output) {
        Result &output = get_result(parameter_identifiers_[i]);
        upload_single_value_output_to_gpu(output);
      }
    }
  }
}

void MultiFunctionProcedureOperation::build_procedure()
{
  for (DNode node : compile_unit_) {
    /* Get the multi-function of the node. */
    auto &multi_function_builder = *node_multi_functions_.lookup_or_add_cb(node, [&]() {
      return std::make_unique<nodes::NodeMultiFunctionBuilder>(*node.bnode(),
                                                               node.context()->btree());
    });
    node->typeinfo->build_multi_function(multi_function_builder);
    const mf::MultiFunction &multi_function = multi_function_builder.function();

    /* Get the variables of the inputs of the node, creating inputs to the operation/procedure if
     * needed. */
    Vector<mf::Variable *> input_variables = this->get_input_variables(node);

    /* Call the node multi-function, getting the variables for its outputs. */
    Vector<mf::Variable *> output_variables = procedure_builder_.add_call(multi_function,
                                                                          input_variables);

    /* Assign the output variables to the node's respective outputs, creating outputs for the
     * operation/procedure if needed. */
    this->assign_output_variables(node, output_variables);
  }

  /* Add destructor calls for the variables. */
  for (const auto &item : output_to_variable_map_.items()) {
    /* Variables that are used by the outputs should not be destructed. */
    if (!output_sockets_to_output_identifiers_map_.contains(item.key)) {
      procedure_builder_.add_destruct(*item.value);
    }
  }
  for (mf::Variable *variable : implicit_variables_) {
    procedure_builder_.add_destruct(*variable);
  }

  mf::ReturnInstruction &return_instruction = procedure_builder_.add_return();
  mf::procedure_optimization::move_destructs_up(procedure_, return_instruction);
  BLI_assert(procedure_.validate());
}

Vector<mf::Variable *> MultiFunctionProcedureOperation::get_input_variables(DNode node)
{
  Vector<mf::Variable *> input_variables;
  for (int i = 0; i < node->input_sockets().size(); i++) {
    const DInputSocket input{node.context(), node->input_sockets()[i]};

    if (!input->is_available()) {
      continue;
    }

    /* The origin socket is an input, that means the input is unlinked and we generate a constant
     * variable for it. */
    const DSocket origin = get_input_origin_socket(input);
    if (origin->is_input()) {
      input_variables.append(this->get_constant_input_variable(DInputSocket(origin)));
    }
    else {
      /* Otherwise, the origin socket is an output, which means it is linked. */
      const DOutputSocket output = DOutputSocket(origin);

      /* If the origin node is part of the multi-function procedure operation, then the output has
       * an existing variable for it. */
      if (compile_unit_.contains(output.node())) {
        input_variables.append(output_to_variable_map_.lookup(output));
      }
      else {
        /* Otherwise, the origin node is not part of the multi-function procedure operation, and a
         * variable that represents an input to the multi-function procedure operation is used. */
        input_variables.append(this->get_multi_function_input_variable(input, output));
      }
    }

    /* Implicitly convert the variable type if needed by adding a call to an implicit conversion
     * function. */
    input_variables.last() = this->do_variable_implicit_conversion(
        input, origin, input_variables.last());
  }

  return input_variables;
}

mf::Variable *MultiFunctionProcedureOperation::get_constant_input_variable(DInputSocket input)
{
  const mf::MultiFunction *constant_function = nullptr;
  switch (input->type) {
    case SOCK_FLOAT: {
      const float value = input->default_value_typed<bNodeSocketValueFloat>()->value;
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float>>(value);
      break;
    }
    case SOCK_INT: {
      const int value = input->default_value_typed<bNodeSocketValueInt>()->value;
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<int>>(value);
      break;
    }
    case SOCK_VECTOR: {
      const float3 value = float3(input->default_value_typed<bNodeSocketValueVector>()->value);
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float4>>(
          float4(value, 0.0f));
      break;
    }
    case SOCK_RGBA: {
      const float4 value = float4(input->default_value_typed<bNodeSocketValueRGBA>()->value);
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float4>>(value);
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  mf::Variable *constant_variable = procedure_builder_.add_call<1>(*constant_function)[0];
  implicit_variables_.append(constant_variable);
  return constant_variable;
}

mf::Variable *MultiFunctionProcedureOperation::get_multi_function_input_variable(
    DInputSocket input_socket, DOutputSocket output_socket)
{
  /* An input was already declared for that same output socket, so no need to declare it again and
   * we just return its variable.  */
  if (output_to_variable_map_.contains(output_socket)) {
    /* But first we update the domain priority of the input descriptor to be the higher priority of
     * the existing descriptor and the descriptor of the new input socket. That's because the same
     * output might be connected to multiple inputs inside the multi-function procedure operation
     * which have different priorities. */
    const std::string input_identifier = outputs_to_declared_inputs_map_.lookup(output_socket);
    InputDescriptor &input_descriptor = this->get_input_descriptor(input_identifier);
    input_descriptor.domain_priority = math::min(
        input_descriptor.domain_priority,
        input_descriptor_from_input_socket(input_socket.bsocket()).domain_priority);

    /* Increment the input's reference count. */
    inputs_to_reference_counts_map_.lookup(input_identifier)++;

    return output_to_variable_map_.lookup(output_socket);
  }

  const int input_index = inputs_to_linked_outputs_map_.size();
  const std::string input_identifier = "input" + std::to_string(input_index);

  /* Declare the input descriptor for this input and prefer to declare its type to be the same as
   * the type of the output socket because doing type conversion in the multi-function procedure is
   * cheaper. */
  InputDescriptor input_descriptor = input_descriptor_from_input_socket(input_socket.bsocket());
  input_descriptor.type = get_node_socket_result_type(output_socket.bsocket());
  declare_input_descriptor(input_identifier, input_descriptor);

  mf::Variable &variable = procedure_builder_.add_input_parameter(
      mf::DataType::ForSingle(get_cpp_type(input_descriptor.type)), input_identifier);
  parameter_identifiers_.append(input_identifier);

  /* Map the output socket to the variable that was created for it. */
  output_to_variable_map_.add(output_socket, &variable);

  /* Map the identifier of the operation input to the output socket it is linked to. */
  inputs_to_linked_outputs_map_.add_new(input_identifier, output_socket);

  /* Map the output socket to the identifier of the operation input that was declared for it. */
  outputs_to_declared_inputs_map_.add_new(output_socket, input_identifier);

  /* Map the identifier of the operation input to a reference count of 1, this will later be
   * incremented if that same output was referenced again. */
  inputs_to_reference_counts_map_.add_new(input_identifier, 1);

  return &variable;
}

/* Returns a multi-function that implicitly converts from the given variable type to the given
 * expected type. nullptr will be returned if no conversion is needed. */
static mf::MultiFunction *get_conversion_function(const ResultType variable_type,
                                                  const ResultType expected_type)
{
  static auto float_to_int_function = mf::build::SI1_SO<float, int>(
      "Float To Int", float_to_int, mf::build::exec_presets::AllSpanOrSingle());
  static auto float_to_vector_function = mf::build::SI1_SO<float, float4>(
      "Float To Vector", float_to_vector, mf::build::exec_presets::AllSpanOrSingle());
  static auto float_to_color_function = mf::build::SI1_SO<float, float4>(
      "Float To Color", float_to_color, mf::build::exec_presets::AllSpanOrSingle());

  static auto int_to_float_function = mf::build::SI1_SO<int, float>(
      "Int To Float", int_to_float, mf::build::exec_presets::AllSpanOrSingle());
  static auto int_to_vector_function = mf::build::SI1_SO<int, float4>(
      "Int To Vector", int_to_vector, mf::build::exec_presets::AllSpanOrSingle());
  static auto int_to_color_function = mf::build::SI1_SO<int, float4>(
      "Int To Color", int_to_color, mf::build::exec_presets::AllSpanOrSingle());

  static auto vector_to_float_function = mf::build::SI1_SO<float4, float>(
      "Vector To Float", vector_to_float, mf::build::exec_presets::AllSpanOrSingle());
  static auto vector_to_int_function = mf::build::SI1_SO<float4, int>(
      "Vector To Int", vector_to_int, mf::build::exec_presets::AllSpanOrSingle());
  static auto vector_to_color_function = mf::build::SI1_SO<float4, float4>(
      "Vector To Color", vector_to_color, mf::build::exec_presets::AllSpanOrSingle());

  static auto color_to_float_function = mf::build::SI1_SO<float4, float>(
      "Color To Float", color_to_float, mf::build::exec_presets::AllSpanOrSingle());
  static auto color_to_int_function = mf::build::SI1_SO<float4, int>(
      "Color To Int", color_to_int, mf::build::exec_presets::AllSpanOrSingle());
  static auto color_to_vector_function = mf::build::SI1_SO<float4, float4>(
      "Color To Vector", color_to_vector, mf::build::exec_presets::AllSpanOrSingle());

  switch (variable_type) {
    case ResultType::Float:
      switch (expected_type) {
        case ResultType::Int:
          return &float_to_int_function;
        case ResultType::Vector:
          return &float_to_vector_function;
        case ResultType::Color:
          return &float_to_color_function;
        case ResultType::Float:
          /* Same type, no conversion needed. */
          return nullptr;
        case ResultType::Float2:
        case ResultType::Float3:
        case ResultType::Int2:
          /* Types are not user facing, so we needn't implement them. */
          break;
      }
      break;
    case ResultType::Int:
      switch (expected_type) {
        case ResultType::Float:
          return &int_to_float_function;
        case ResultType::Vector:
          return &int_to_vector_function;
        case ResultType::Color:
          return &int_to_color_function;
        case ResultType::Int:
          /* Same type, no conversion needed. */
          return nullptr;
        case ResultType::Float2:
        case ResultType::Float3:
        case ResultType::Int2:
          /* Types are not user facing, so we needn't implement them. */
          break;
      }
      break;
    case ResultType::Vector:
      switch (expected_type) {
        case ResultType::Float:
          return &vector_to_float_function;
        case ResultType::Int:
          return &vector_to_int_function;
        case ResultType::Color:
          return &vector_to_color_function;
        case ResultType::Vector:
          /* Same type, no conversion needed. */
          return nullptr;
        case ResultType::Float2:
        case ResultType::Float3:
        case ResultType::Int2:
          /* Types are not user facing, so we needn't implement them. */
          break;
      }
      break;
    case ResultType::Color:
      switch (expected_type) {
        case ResultType::Float:
          return &color_to_float_function;
        case ResultType::Int:
          return &color_to_int_function;
        case ResultType::Vector:
          return &color_to_vector_function;
        case ResultType::Color:
          /* Same type, no conversion needed. */
          return nullptr;
        case ResultType::Float2:
        case ResultType::Float3:
        case ResultType::Int2:
          /* Types are not user facing, so we needn't implement them. */
          break;
      }
      break;
    case ResultType::Float2:
    case ResultType::Float3:
    case ResultType::Int2:
      /* Types are not user facing, so we needn't implement them. */
      break;
  }

  BLI_assert_unreachable();
  return nullptr;
}

mf::Variable *MultiFunctionProcedureOperation::do_variable_implicit_conversion(
    DInputSocket input_socket, DSocket origin_socket, mf::Variable *variable)
{
  const ResultType expected_type = get_node_socket_result_type(input_socket.bsocket());
  const ResultType variable_type = get_node_socket_result_type(origin_socket.bsocket());

  const mf::MultiFunction *function = get_conversion_function(variable_type, expected_type);
  if (!function) {
    return variable;
  }

  mf::Variable *converted_variable = procedure_builder_.add_call<1>(*function, {variable})[0];
  implicit_variables_.append(converted_variable);
  return converted_variable;
}

void MultiFunctionProcedureOperation::assign_output_variables(DNode node,
                                                              Vector<mf::Variable *> &variables)
{
  const DOutputSocket preview_output = find_preview_output_socket(node);

  for (int i = 0; i < node->output_sockets().size(); i++) {
    const DOutputSocket output{node.context(), node->output_sockets()[i]};

    output_to_variable_map_.add_new(output, variables[i]);

    /* If any of the nodes linked to the output are not part of the multi-function procedure
     * operation but are part of the execution schedule, then an output result needs to be
     * populated for it. */
    const bool is_operation_output = is_output_linked_to_node_conditioned(output, [&](DNode node) {
      return schedule_.contains(node) && !compile_unit_.contains(node);
    });

    /* If the output is used as the node preview, then an output result needs to be populated for
     * it, and we additionally keep track of that output to later compute the previews from. */
    const bool is_preview_output = output == preview_output;
    if (is_preview_output) {
      preview_outputs_.add(output);
    }

    if (is_operation_output || is_preview_output) {
      this->populate_operation_result(output, variables[i]);
    }
  }
}

void MultiFunctionProcedureOperation::populate_operation_result(DOutputSocket output_socket,
                                                                mf::Variable *variable)
{
  const uint output_id = output_sockets_to_output_identifiers_map_.size();
  const std::string output_identifier = "output" + std::to_string(output_id);

  const ResultType result_type = get_node_socket_result_type(output_socket.bsocket());
  const Result result = context().create_result(result_type);
  populate_result(output_identifier, result);

  /* Map the output socket to the identifier of the newly populated result. */
  output_sockets_to_output_identifiers_map_.add_new(output_socket, output_identifier);

  procedure_builder_.add_output_parameter(*variable);
  parameter_identifiers_.append(output_identifier);
}

bool MultiFunctionProcedureOperation::is_single_value_operation()
{
  /* Return true if all inputs are single values. */
  for (int i = 0; i < procedure_.params().size(); i++) {
    if (procedure_.params()[i].type == mf::ParamType::InterfaceType::Input) {
      Result &input = this->get_input(parameter_identifiers_[i]);
      if (!input.is_single_value()) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace blender::compositor

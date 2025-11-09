/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>
#include <string>

#include "BLI_assert.h"
#include "BLI_color.hh"
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

#include "BKE_type_conversions.hh"

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
      const Result &input = get_input(parameter_identifiers_[i]);
      if (input.is_single_value()) {
        parameter_builder.add_readonly_single_input(input.single_value());
      }
      else {
        parameter_builder.add_readonly_single_input(input.cpu_data());
      }
    }
    else {
      Result &output = get_result(parameter_identifiers_[i]);
      if (is_single_value) {
        output.allocate_single_value();
        parameter_builder.add_uninitialized_single_output(
            GMutableSpan(output.get_cpp_type(), output.single_value().get(), 1));
      }
      else {
        output.allocate_texture(domain);
        parameter_builder.add_uninitialized_single_output(output.cpu_data());
      }
    }
  }

  mf::ContextBuilder context_builder;
  procedure_executor_->call_auto(mask, parameter_builder, context_builder);

  /* In case of single value execution, update single value data. */
  if (is_single_value) {
    for (int i = 0; i < procedure_.params().size(); i++) {
      if (procedure_.params()[i].type == mf::ParamType::InterfaceType::Output) {
        Result &output = get_result(parameter_identifiers_[i]);
        output.update_single_value_data();
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
    Vector<mf::Variable *> input_variables = this->get_input_variables(node, multi_function);

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
    if (!output_variables_.contains(item.value)) {
      procedure_builder_.add_destruct(*item.value);
    }
  }
  for (mf::Variable *variable : implicit_variables_) {
    /* Variables that are used by the outputs should not be destructed. */
    if (!output_variables_.contains(variable)) {
      procedure_builder_.add_destruct(*variable);
    }
  }
  for (mf::Variable *variable : implicit_input_to_variable_map_.values()) {
    procedure_builder_.add_destruct(*variable);
  }

  mf::ReturnInstruction &return_instruction = procedure_builder_.add_return();
  mf::procedure_optimization::move_destructs_up(procedure_, return_instruction);
  BLI_assert(procedure_.validate());
}

Vector<mf::Variable *> MultiFunctionProcedureOperation::get_input_variables(
    DNode node, const mf::MultiFunction &multi_function)
{
  int available_inputs_index = 0;
  Vector<mf::Variable *> input_variables;
  for (int i = 0; i < node->input_sockets().size(); i++) {
    const DInputSocket input{node.context(), node->input_sockets()[i]};

    if (!is_socket_available(input.bsocket())) {
      continue;
    }

    /* The origin socket is an input, that means the input is unlinked. */
    const DSocket origin = get_input_origin_socket(input);
    if (origin->is_input()) {
      const InputDescriptor origin_descriptor = input_descriptor_from_input_socket(
          origin.bsocket());

      if (origin_descriptor.implicit_input == ImplicitInput::None) {
        /* No implicit input, so get a constant variable that holds the socket value. */
        input_variables.append(this->get_constant_input_variable(DInputSocket(origin)));
      }
      else {
        input_variables.append(this->get_implicit_input_variable(input, DInputSocket(origin)));
      }
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

    /* Implicitly convert the variable type to the expected parameter type if needed. */
    const mf::ParamType parameter_type = multi_function.param_type(available_inputs_index);
    input_variables.last() = this->convert_variable(input_variables.last(),
                                                    parameter_type.data_type());

    available_inputs_index++;
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
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<int32_t>>(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = input->default_value_typed<bNodeSocketValueBoolean>()->value;
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<bool>>(value);
      break;
    }
    case SOCK_VECTOR: {
      switch (input->default_value_typed<bNodeSocketValueVector>()->dimensions) {
        case 2: {
          const float2 value = float2(input->default_value_typed<bNodeSocketValueVector>()->value);
          constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float2>>(value);
          break;
        }
        case 3: {
          const float3 value = float3(input->default_value_typed<bNodeSocketValueVector>()->value);
          constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float3>>(value);
          break;
        }
        case 4: {
          const float4 value = float4(input->default_value_typed<bNodeSocketValueVector>()->value);
          constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float4>>(value);
          break;
        }
        default:
          BLI_assert_unreachable();
          break;
      }
      break;
    }
    case SOCK_RGBA: {
      const Color value = Color(input->default_value_typed<bNodeSocketValueRGBA>()->value);
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<float4>>(value);
      break;
    }
    case SOCK_MENU: {
      const int32_t value = input->default_value_typed<bNodeSocketValueMenu>()->value;
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<nodes::MenuValue>>(
          value);
      break;
    }
    case SOCK_STRING: {
      const std::string value = input->default_value_typed<bNodeSocketValueString>()->value;
      constant_function = &procedure_.construct_function<mf::CustomMF_Constant<std::string>>(
          value);
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

mf::Variable *MultiFunctionProcedureOperation::get_implicit_input_variable(
    const DInputSocket input, const DInputSocket origin)
{
  const InputDescriptor origin_descriptor = input_descriptor_from_input_socket(origin.bsocket());
  const ImplicitInput implicit_input = origin_descriptor.implicit_input;

  /* Inherit the type and implicit input of the origin input since doing implicit conversion inside
   * the multi-function operation is much cheaper. */
  InputDescriptor input_descriptor = input_descriptor_from_input_socket(input.bsocket());
  input_descriptor.type = origin_descriptor.type;
  input_descriptor.implicit_input = implicit_input;

  /* An input was already declared for that implicit input, so no need to declare it again and we
   * just return its variable. */
  if (implicit_input_to_variable_map_.contains(implicit_input)) {
    /* But first we update the domain priority of the input descriptor to be the higher priority of
     * the existing descriptor and the descriptor of the new input socket. That's because the same
     * implicit input might be used in inputs inside the multi-function procedure operation which
     * have different priorities. */
    InputDescriptor &existing_input_descriptor = this->get_input_descriptor(
        implicit_inputs_to_input_identifiers_map_.lookup(implicit_input));
    existing_input_descriptor.domain_priority = math::min(
        existing_input_descriptor.domain_priority, input_descriptor.domain_priority);

    return implicit_input_to_variable_map_.lookup(implicit_input);
  }

  const int implicit_input_index = implicit_inputs_to_input_identifiers_map_.size();
  const std::string input_identifier = "implicit_input" + std::to_string(implicit_input_index);
  declare_input_descriptor(input_identifier, input_descriptor);

  /* Map the implicit input to the identifier of the operation input that was declared for it. */
  implicit_inputs_to_input_identifiers_map_.add_new(implicit_input, input_identifier);

  mf::Variable &variable = procedure_builder_.add_input_parameter(
      mf::DataType::ForSingle(Result::cpp_type(input_descriptor.type)), input_identifier);
  parameter_identifiers_.append(input_identifier);

  /* Map the implicit input to the variable that was created for it. */
  implicit_input_to_variable_map_.add(implicit_input, &variable);

  return &variable;
}

mf::Variable *MultiFunctionProcedureOperation::get_multi_function_input_variable(
    DInputSocket input_socket, DOutputSocket output_socket)
{
  /* An input was already declared for that same output socket, so no need to declare it again and
   * we just return its variable. */
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
      mf::DataType::ForSingle(Result::cpp_type(input_descriptor.type)), input_identifier);
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

void MultiFunctionProcedureOperation::assign_output_variables(DNode node,
                                                              Vector<mf::Variable *> &variables)
{
  const DOutputSocket preview_output = find_preview_output_socket(node);

  int available_outputs_index = 0;
  for (int i = 0; i < node->output_sockets().size(); i++) {
    const DOutputSocket output{node.context(), node->output_sockets()[i]};

    if (!is_socket_available(output.bsocket())) {
      continue;
    }

    mf::Variable *output_variable = variables[available_outputs_index];
    output_to_variable_map_.add_new(output, output_variable);

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
      this->populate_operation_result(output, output_variable);
    }

    available_outputs_index++;
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

  /* Implicitly convert the variable type to the expected result type if needed. */
  const mf::DataType expected_type = mf::DataType::ForSingle(result.get_cpp_type());
  mf::Variable *converted_variable = this->convert_variable(variable, expected_type);

  procedure_builder_.add_output_parameter(*converted_variable);
  output_variables_.add_new(converted_variable);
  parameter_identifiers_.append(output_identifier);
}

mf::Variable *MultiFunctionProcedureOperation::convert_variable(mf::Variable *variable,
                                                                const mf::DataType expected_type)
{
  /* Conversion not needed. */
  const mf::DataType variable_type = variable->data_type();
  if (variable_type == expected_type) {
    return variable;
  }

  const bke::DataTypeConversions &conversion_table = bke::get_implicit_type_conversions();
  const mf::MultiFunction *function = conversion_table.get_conversion_multi_function(
      variable_type, expected_type);

  /* Conversion is not possible, return a default variable instead. */
  if (!function) {
    const mf::MultiFunction &constant_function =
        procedure_.construct_function<mf::CustomMF_GenericConstant>(
            expected_type.single_type(), expected_type.single_type().default_value(), false);
    mf::Variable *constant_variable = procedure_builder_.add_call<1>(constant_function)[0];
    implicit_variables_.append(constant_variable);
    return constant_variable;
  }

  mf::Variable *converted_variable = procedure_builder_.add_call<1>(*function, {variable})[0];
  implicit_variables_.append(converted_variable);
  return converted_variable;
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

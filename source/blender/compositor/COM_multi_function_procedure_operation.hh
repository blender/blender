/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "BLI_vector.hh"

#include "FN_multi_function_procedure.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"

#include "NOD_derived_node_tree.hh"
#include "NOD_multi_function.hh"

#include "COM_context.hh"
#include "COM_input_descriptor.hh"
#include "COM_pixel_operation.hh"
#include "COM_scheduler.hh"

namespace blender::compositor {

using namespace nodes::derived_node_tree_types;

/* ------------------------------------------------------------------------------------------------
 * Multi-Function Procedure Operation
 *
 * A pixel operation that evaluates a multi-function procedure built from the pixel compile unit
 * using the multi-function procedure builder, see FN_multi_function_procedure_builder.hh for more
 * information. Also see the PixelOperation class for more information on pixel operations. */
class MultiFunctionProcedureOperation : public PixelOperation {
 private:
  /* The multi-function procedure, its builder, and executor that are backing the operation. This
   * is created and compiled during construction. */
  mf::Procedure procedure_;
  mf::ProcedureBuilder procedure_builder_;
  std::unique_ptr<mf::ProcedureExecutor> procedure_executor_;
  /* A map that associates each node in the compile unit with an instance of its multi-function
   * builder. */
  Map<DNode, std::unique_ptr<nodes::NodeMultiFunctionBuilder>> node_multi_functions_;
  /* A map that associates the output sockets of each node to the variables that were created for
   * them. */
  Map<DOutputSocket, mf::Variable *> output_to_variable_map_;
  /* A map that associates implicit inputs to the variables that were created for them. */
  Map<ImplicitInput, mf::Variable *> implicit_input_to_variable_map_;
  /* A vector that stores the intermediate variables that were implicitly created for the procedure
   * but are not associated with a node output. Those variables are for such multi-functions like
   * constant inputs and implicit conversion. */
  Vector<mf::Variable *> implicit_variables_;
  /* A set that stores the variables that are used as the outputs of the procedure. */
  Set<mf::Variable *> output_variables_;
  /* A vector that stores the identifiers of the parameters of the multi-function procedure in
   * order. The parameters include both inputs and outputs. This is used to retrieve the input and
   * output results for each of the parameters in the procedure. Note that parameters have no
   * identifiers and are identified solely by their order. */
  Vector<std::string> parameter_identifiers_;

 public:
  /* Build a multi-function procedure as well as an executor for it from the given pixel compile
   * unit and execution schedule. */
  MultiFunctionProcedureOperation(Context &context,
                                  PixelCompileUnit &compile_unit,
                                  const Schedule &schedule);

  /* Calls the multi-function procedure executor on the domain of the operator passing in the
   * inputs and outputs as parameters. */
  void execute() override;

 private:
  /* Builds the procedure by going over the nodes in the compile unit, calling their
   * multi-functions and creating any necessary inputs or outputs to the operation/procedure. */
  void build_procedure();

  /* Get the variables corresponding to the inputs of the given node. The variables can be those
   * that were returned by a previous call to a multi-function, those that were generated as
   * constants for unlinked inputs, or those that were added as inputs to the operation/procedure
   * itself. The variables are implicitly converted to the type expected by the multi-function. */
  Vector<mf::Variable *> get_input_variables(DNode node, const mf::MultiFunction &multi_function);

  /* Returns a constant variable that was created by calling a constant function carrying the value
   * of the given input socket. */
  mf::Variable *get_constant_input_variable(DInputSocket input);

  /* Given an unlinked input with an implicit input. Declare an input to the operation/procedure
   * for that implicit input if not done already and return a variable that represent that implicit
   * input. The implicit input and type are taken from the given origin input, which will be equal
   * to the input in most cases, but can also be an unlinked input of a group node */
  mf::Variable *get_implicit_input_variable(const DInputSocket input, const DInputSocket origin);

  /* Given an input in a node that is part of the compile unit that is connected to an output that
   * is in a non that is not part of the compile unit. Declare an input to the operation/procedure
   * for that output if not done already and return a variable that represent that input. */
  mf::Variable *get_multi_function_input_variable(DInputSocket input_socket,
                                                  DOutputSocket output_socket);

  /* Given the variables that were returned by calling the multi-function for the given node,
   * assign the variables to their corresponding outputs. And if an output is connected to a node
   * outside of the compile unit or is used as the preview of the node, declare an output to the
   * operation/procedure for it. */
  void assign_output_variables(DNode node, Vector<mf::Variable *> &variables);

  /* Populate an output to the operator/procedure for the given output socket whose value is stored
   * in the given variable. The variable is implicitly converted to the type expected by the
   * socket. */
  void populate_operation_result(DOutputSocket output_socket, mf::Variable *variable);

  /* Convert the given variable to the given expected type. This is done by adding an implicit
   * conversion function whose output variable will be returned. If no conversion is needed, the
   * given variable is returned as is. If conversion is not possible, a fallback default variable
   * will b returned. */
  mf::Variable *convert_variable(mf::Variable *variable, const mf::DataType expected_type);

  /* Returns true if the operation operates on single values, that is, all of its inputs are single
   * values. Assumes the procedure is already build. */
  bool is_single_value_operation();
};

}  // namespace blender::compositor

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_procedure_builder.hh"

namespace blender::fn::multi_function {

void ProcedureBuilder::add_destruct(Variable &variable)
{
  DestructInstruction &instruction = procedure_->new_destruct_instruction();
  instruction.set_variable(&variable);
  this->link_to_cursors(&instruction);
  cursors_ = {InstructionCursor{instruction}};
}

void ProcedureBuilder::add_destruct(Span<Variable *> variables)
{
  for (Variable *variable : variables) {
    this->add_destruct(*variable);
  }
}

ReturnInstruction &ProcedureBuilder::add_return()
{
  ReturnInstruction &instruction = procedure_->new_return_instruction();
  this->link_to_cursors(&instruction);
  cursors_ = {};
  return instruction;
}

CallInstruction &ProcedureBuilder::add_call_with_no_variables(const MultiFunction &fn)
{
  CallInstruction &instruction = procedure_->new_call_instruction(fn);
  this->link_to_cursors(&instruction);
  cursors_ = {InstructionCursor{instruction}};
  return instruction;
}

CallInstruction &ProcedureBuilder::add_call_with_all_variables(const MultiFunction &fn,
                                                               Span<Variable *> param_variables)
{
  CallInstruction &instruction = this->add_call_with_no_variables(fn);
  instruction.set_params(param_variables);
  return instruction;
}

Vector<Variable *> ProcedureBuilder::add_call(const MultiFunction &fn,
                                              Span<Variable *> input_and_mutable_variables)
{
  Vector<Variable *> output_variables;
  CallInstruction &instruction = this->add_call_with_no_variables(fn);
  for (const int param_index : fn.param_indices()) {
    const ParamType param_type = fn.param_type(param_index);
    switch (param_type.interface_type()) {
      case ParamType::Input:
      case ParamType::Mutable: {
        Variable *variable = input_and_mutable_variables.first();
        instruction.set_param_variable(param_index, variable);
        input_and_mutable_variables = input_and_mutable_variables.drop_front(1);
        break;
      }
      case ParamType::Output: {
        Variable &variable = procedure_->new_variable(param_type.data_type(),
                                                      fn.param_name(param_index));
        instruction.set_param_variable(param_index, &variable);
        output_variables.append(&variable);
        break;
      }
    }
  }
  /* All passed in variables should have been dropped in the loop above. */
  BLI_assert(input_and_mutable_variables.is_empty());
  return output_variables;
}

ProcedureBuilder::Branch ProcedureBuilder::add_branch(Variable &condition)
{
  BranchInstruction &instruction = procedure_->new_branch_instruction();
  instruction.set_condition(&condition);
  this->link_to_cursors(&instruction);
  /* Clear cursors because this builder ends here. */
  cursors_.clear();

  Branch branch{*procedure_, *procedure_};
  branch.branch_true.set_cursor(InstructionCursor{instruction, true});
  branch.branch_false.set_cursor(InstructionCursor{instruction, false});
  return branch;
}

ProcedureBuilder::Loop ProcedureBuilder::add_loop()
{
  DummyInstruction &loop_begin = procedure_->new_dummy_instruction();
  DummyInstruction &loop_end = procedure_->new_dummy_instruction();
  this->link_to_cursors(&loop_begin);
  cursors_ = {InstructionCursor{loop_begin}};

  Loop loop;
  loop.begin = &loop_begin;
  loop.end = &loop_end;

  return loop;
}

void ProcedureBuilder::add_loop_continue(Loop &loop)
{
  this->link_to_cursors(loop.begin);
  /* Clear cursors because this builder ends here. */
  cursors_.clear();
}

void ProcedureBuilder::add_loop_break(Loop &loop)
{
  this->link_to_cursors(loop.end);
  /* Clear cursors because this builder ends here. */
  cursors_.clear();
}

}  // namespace blender::fn::multi_function

/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_procedure_builder.hh"

namespace blender::fn {

void MFProcedureBuilder::add_destruct(MFVariable &variable)
{
  MFDestructInstruction &instruction = procedure_->new_destruct_instruction();
  instruction.set_variable(&variable);
  this->link_to_cursors(&instruction);
  cursors_ = {MFInstructionCursor{instruction}};
}

void MFProcedureBuilder::add_destruct(Span<MFVariable *> variables)
{
  for (MFVariable *variable : variables) {
    this->add_destruct(*variable);
  }
}

MFReturnInstruction &MFProcedureBuilder::add_return()
{
  MFReturnInstruction &instruction = procedure_->new_return_instruction();
  this->link_to_cursors(&instruction);
  cursors_ = {};
  return instruction;
}

MFCallInstruction &MFProcedureBuilder::add_call_with_no_variables(const MultiFunction &fn)
{
  MFCallInstruction &instruction = procedure_->new_call_instruction(fn);
  this->link_to_cursors(&instruction);
  cursors_ = {MFInstructionCursor{instruction}};
  return instruction;
}

MFCallInstruction &MFProcedureBuilder::add_call_with_all_variables(
    const MultiFunction &fn, Span<MFVariable *> param_variables)
{
  MFCallInstruction &instruction = this->add_call_with_no_variables(fn);
  instruction.set_params(param_variables);
  return instruction;
}

Vector<MFVariable *> MFProcedureBuilder::add_call(const MultiFunction &fn,
                                                  Span<MFVariable *> input_and_mutable_variables)
{
  Vector<MFVariable *> output_variables;
  MFCallInstruction &instruction = this->add_call_with_no_variables(fn);
  for (const int param_index : fn.param_indices()) {
    const MFParamType param_type = fn.param_type(param_index);
    switch (param_type.interface_type()) {
      case MFParamType::Input:
      case MFParamType::Mutable: {
        MFVariable *variable = input_and_mutable_variables.first();
        instruction.set_param_variable(param_index, variable);
        input_and_mutable_variables = input_and_mutable_variables.drop_front(1);
        break;
      }
      case MFParamType::Output: {
        MFVariable &variable = procedure_->new_variable(param_type.data_type(),
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

MFProcedureBuilder::Branch MFProcedureBuilder::add_branch(MFVariable &condition)
{
  MFBranchInstruction &instruction = procedure_->new_branch_instruction();
  instruction.set_condition(&condition);
  this->link_to_cursors(&instruction);
  /* Clear cursors because this builder ends here. */
  cursors_.clear();

  Branch branch{*procedure_, *procedure_};
  branch.branch_true.set_cursor(MFInstructionCursor{instruction, true});
  branch.branch_false.set_cursor(MFInstructionCursor{instruction, false});
  return branch;
}

MFProcedureBuilder::Loop MFProcedureBuilder::add_loop()
{
  MFDummyInstruction &loop_begin = procedure_->new_dummy_instruction();
  MFDummyInstruction &loop_end = procedure_->new_dummy_instruction();
  this->link_to_cursors(&loop_begin);
  cursors_ = {MFInstructionCursor{loop_begin}};

  Loop loop;
  loop.begin = &loop_begin;
  loop.end = &loop_end;

  return loop;
}

void MFProcedureBuilder::add_loop_continue(Loop &loop)
{
  this->link_to_cursors(loop.begin);
  /* Clear cursors because this builder ends here. */
  cursors_.clear();
}

void MFProcedureBuilder::add_loop_break(Loop &loop)
{
  this->link_to_cursors(loop.end);
  /* Clear cursors because this builder ends here. */
  cursors_.clear();
}

}  // namespace blender::fn

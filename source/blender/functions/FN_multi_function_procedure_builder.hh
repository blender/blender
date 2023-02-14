/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_multi_function_procedure.hh"

namespace blender::fn::multi_function {

/**
 * Utility class to build a #Procedure.
 */
class ProcedureBuilder {
 private:
  /** Procedure that is being build. */
  Procedure *procedure_ = nullptr;
  /** Cursors where the next instruction should be inserted. */
  Vector<InstructionCursor> cursors_;

 public:
  struct Branch;
  struct Loop;

  ProcedureBuilder(Procedure &procedure,
                   InstructionCursor initial_cursor = InstructionCursor::ForEntry());

  ProcedureBuilder(Span<ProcedureBuilder *> builders);

  ProcedureBuilder(Branch &branch);

  void set_cursor(const InstructionCursor &cursor);
  void set_cursor(Span<InstructionCursor> cursors);
  void set_cursor(Span<ProcedureBuilder *> builders);
  void set_cursor_after_branch(Branch &branch);
  void set_cursor_after_loop(Loop &loop);

  void add_destruct(Variable &variable);
  void add_destruct(Span<Variable *> variables);

  ReturnInstruction &add_return();

  Branch add_branch(Variable &condition);

  Loop add_loop();
  void add_loop_continue(Loop &loop);
  void add_loop_break(Loop &loop);

  CallInstruction &add_call_with_no_variables(const MultiFunction &fn);
  CallInstruction &add_call_with_all_variables(const MultiFunction &fn,
                                               Span<Variable *> param_variables);

  Vector<Variable *> add_call(const MultiFunction &fn,
                              Span<Variable *> input_and_mutable_variables = {});

  template<int OutputN>
  std::array<Variable *, OutputN> add_call(const MultiFunction &fn,
                                           Span<Variable *> input_and_mutable_variables = {});

  void add_parameter(ParamType::InterfaceType interface_type, Variable &variable);
  Variable &add_parameter(ParamType param_type, std::string name = "");

  Variable &add_input_parameter(DataType data_type, std::string name = "");
  template<typename T> Variable &add_single_input_parameter(std::string name = "");
  template<typename T> Variable &add_single_mutable_parameter(std::string name = "");

  void add_output_parameter(Variable &variable);

 private:
  void link_to_cursors(Instruction *instruction);
};

struct ProcedureBuilder::Branch {
  ProcedureBuilder branch_true;
  ProcedureBuilder branch_false;
};

struct ProcedureBuilder::Loop {
  Instruction *begin = nullptr;
  DummyInstruction *end = nullptr;
};

/* --------------------------------------------------------------------
 * ProcedureBuilder inline methods.
 */

inline ProcedureBuilder::ProcedureBuilder(Branch &branch)
    : ProcedureBuilder(*branch.branch_true.procedure_)
{
  this->set_cursor_after_branch(branch);
}

inline ProcedureBuilder::ProcedureBuilder(Procedure &procedure, InstructionCursor initial_cursor)
    : procedure_(&procedure), cursors_({initial_cursor})
{
}

inline ProcedureBuilder::ProcedureBuilder(Span<ProcedureBuilder *> builders)
    : ProcedureBuilder(*builders[0]->procedure_)
{
  this->set_cursor(builders);
}

inline void ProcedureBuilder::set_cursor(const InstructionCursor &cursor)
{
  cursors_ = {cursor};
}

inline void ProcedureBuilder::set_cursor(Span<InstructionCursor> cursors)
{
  cursors_ = cursors;
}

inline void ProcedureBuilder::set_cursor_after_branch(Branch &branch)
{
  this->set_cursor({&branch.branch_false, &branch.branch_true});
}

inline void ProcedureBuilder::set_cursor_after_loop(Loop &loop)
{
  this->set_cursor(InstructionCursor{*loop.end});
}

inline void ProcedureBuilder::set_cursor(Span<ProcedureBuilder *> builders)
{
  cursors_.clear();
  for (ProcedureBuilder *builder : builders) {
    cursors_.extend(builder->cursors_);
  }
}

template<int OutputN>
inline std::array<Variable *, OutputN> ProcedureBuilder::add_call(
    const MultiFunction &fn, Span<Variable *> input_and_mutable_variables)
{
  Vector<Variable *> output_variables = this->add_call(fn, input_and_mutable_variables);
  BLI_assert(output_variables.size() == OutputN);

  std::array<Variable *, OutputN> output_array;
  initialized_copy_n(output_variables.data(), OutputN, output_array.data());
  return output_array;
}

inline void ProcedureBuilder::add_parameter(ParamType::InterfaceType interface_type,
                                            Variable &variable)
{
  procedure_->add_parameter(interface_type, variable);
}

inline Variable &ProcedureBuilder::add_parameter(ParamType param_type, std::string name)
{
  Variable &variable = procedure_->new_variable(param_type.data_type(), std::move(name));
  this->add_parameter(param_type.interface_type(), variable);
  return variable;
}

inline Variable &ProcedureBuilder::add_input_parameter(DataType data_type, std::string name)
{
  return this->add_parameter(ParamType(ParamType::Input, data_type), std::move(name));
}

template<typename T>
inline Variable &ProcedureBuilder::add_single_input_parameter(std::string name)
{
  return this->add_parameter(ParamType::ForSingleInput(CPPType::get<T>()), std::move(name));
}

template<typename T>
inline Variable &ProcedureBuilder::add_single_mutable_parameter(std::string name)
{
  return this->add_parameter(ParamType::ForMutableSingle(CPPType::get<T>()), std::move(name));
}

inline void ProcedureBuilder::add_output_parameter(Variable &variable)
{
  this->add_parameter(ParamType::Output, variable);
}

inline void ProcedureBuilder::link_to_cursors(Instruction *instruction)
{
  for (InstructionCursor &cursor : cursors_) {
    cursor.set_next(*procedure_, instruction);
  }
}

}  // namespace blender::fn::multi_function

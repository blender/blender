/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_multi_function.hh"

namespace blender::fn::multi_function {

class Variable;
class Instruction;
class CallInstruction;
class BranchInstruction;
class DestructInstruction;
class DummyInstruction;
class ReturnInstruction;
class Procedure;

/** Every instruction has exactly one of these types. */
enum class InstructionType {
  Call,
  Branch,
  Destruct,
  Dummy,
  Return,
};

/**
 * An #InstructionCursor points to a position in a multi-function procedure, where an instruction
 * can be inserted.
 */
class InstructionCursor {
 public:
  enum Type {
    None,
    Entry,
    Call,
    Destruct,
    Branch,
    Dummy,
  };

 private:
  Type type_ = None;
  Instruction *instruction_ = nullptr;
  /* Only used when it is a branch instruction. */
  bool branch_output_ = false;

 public:
  InstructionCursor() = default;
  InstructionCursor(CallInstruction &instruction);
  InstructionCursor(DestructInstruction &instruction);
  InstructionCursor(BranchInstruction &instruction, bool branch_output);
  InstructionCursor(DummyInstruction &instruction);

  static InstructionCursor ForEntry();

  Instruction *next(Procedure &procedure) const;
  void set_next(Procedure &procedure, Instruction *new_instruction) const;

  Instruction *instruction() const;

  Type type() const;

  friend bool operator==(const InstructionCursor &a, const InstructionCursor &b)
  {
    return a.type_ == b.type_ && a.instruction_ == b.instruction_ &&
           a.branch_output_ == b.branch_output_;
  }

  friend bool operator!=(const InstructionCursor &a, const InstructionCursor &b)
  {
    return !(a == b);
  }
};

/**
 * A variable is similar to a virtual register in other libraries. During evaluation, every is
 * either uninitialized or contains a value for every index (remember, a multi-function procedure
 * is always evaluated for many indices at the same time).
 */
class Variable : NonCopyable, NonMovable {
 private:
  DataType data_type_;
  Vector<Instruction *> users_;
  std::string name_;
  int index_in_graph_;

  friend Procedure;
  friend CallInstruction;
  friend BranchInstruction;
  friend DestructInstruction;

 public:
  DataType data_type() const;
  Span<Instruction *> users();

  StringRefNull name() const;
  void set_name(std::string name);

  int index_in_procedure() const;
};

/** Base class for all instruction types. */
class Instruction : NonCopyable, NonMovable {
 protected:
  InstructionType type_;
  Vector<InstructionCursor> prev_;

  friend Procedure;
  friend CallInstruction;
  friend BranchInstruction;
  friend DestructInstruction;
  friend DummyInstruction;
  friend ReturnInstruction;

 public:
  InstructionType type() const;

  /**
   * Other instructions that come before this instruction. There can be multiple previous
   * instructions when branching is used in the procedure.
   */
  Span<InstructionCursor> prev() const;
};

/**
 * References a multi-function that is evaluated when the instruction is executed. It also
 * references the variables whose data will be passed into the multi-function.
 */
class CallInstruction : public Instruction {
 private:
  const MultiFunction *fn_ = nullptr;
  Instruction *next_ = nullptr;
  MutableSpan<Variable *> params_;

  friend Procedure;

 public:
  const MultiFunction &fn() const;

  Instruction *next();
  const Instruction *next() const;
  void set_next(Instruction *instruction);

  void set_param_variable(int param_index, Variable *variable);
  void set_params(Span<Variable *> variables);

  Span<Variable *> params();
  Span<const Variable *> params() const;
};

/**
 * What makes a branch instruction special is that it has two successor instructions. One that will
 * be used when a condition variable was true, and one otherwise.
 */
class BranchInstruction : public Instruction {
 private:
  Variable *condition_ = nullptr;
  Instruction *branch_true_ = nullptr;
  Instruction *branch_false_ = nullptr;

  friend Procedure;

 public:
  Variable *condition();
  const Variable *condition() const;
  void set_condition(Variable *variable);

  Instruction *branch_true();
  const Instruction *branch_true() const;
  void set_branch_true(Instruction *instruction);

  Instruction *branch_false();
  const Instruction *branch_false() const;
  void set_branch_false(Instruction *instruction);
};

/**
 * A destruct instruction destructs a single variable. So the variable value will be uninitialized
 * after this instruction. All variables that are not output variables of the procedure, have to be
 * destructed before the procedure ends. Destructing early is generally a good thing, because it
 * might help with memory buffer reuse, which decreases memory-usage and increases performance.
 */
class DestructInstruction : public Instruction {
 private:
  Variable *variable_ = nullptr;
  Instruction *next_ = nullptr;

  friend Procedure;

 public:
  Variable *variable();
  const Variable *variable() const;
  void set_variable(Variable *variable);

  Instruction *next();
  const Instruction *next() const;
  void set_next(Instruction *instruction);
};

/**
 * This instruction does nothing, it just exists to building a procedure simpler in some cases.
 */
class DummyInstruction : public Instruction {
 private:
  Instruction *next_ = nullptr;

  friend Procedure;

 public:
  Instruction *next();
  const Instruction *next() const;
  void set_next(Instruction *instruction);
};

/**
 * This instruction ends the procedure.
 */
class ReturnInstruction : public Instruction {
};

/**
 * Inputs and outputs of the entire procedure network.
 */
struct Parameter {
  ParamType::InterfaceType type;
  Variable *variable;
};

struct ConstParameter {
  ParamType::InterfaceType type;
  const Variable *variable;
};

/**
 * A multi-function procedure allows composing multi-functions in arbitrary ways. It consists of
 * variables and instructions that operate on those variables. Branching and looping within the
 * procedure is supported as well.
 *
 * Typically, a #Procedure should be constructed using a #ProcedureBuilder, which has many more
 * utility methods for common use cases.
 */
class Procedure : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<CallInstruction *> call_instructions_;
  Vector<BranchInstruction *> branch_instructions_;
  Vector<DestructInstruction *> destruct_instructions_;
  Vector<DummyInstruction *> dummy_instructions_;
  Vector<ReturnInstruction *> return_instructions_;
  Vector<Variable *> variables_;
  Vector<Parameter> params_;
  Vector<destruct_ptr<MultiFunction>> owned_functions_;
  Instruction *entry_ = nullptr;

  friend class ProcedureDotExport;

 public:
  Procedure() = default;
  ~Procedure();

  Variable &new_variable(DataType data_type, std::string name = "");
  CallInstruction &new_call_instruction(const MultiFunction &fn);
  BranchInstruction &new_branch_instruction();
  DestructInstruction &new_destruct_instruction();
  DummyInstruction &new_dummy_instruction();
  ReturnInstruction &new_return_instruction();

  void add_parameter(ParamType::InterfaceType interface_type, Variable &variable);
  Span<ConstParameter> params() const;

  template<typename T, typename... Args> const MultiFunction &construct_function(Args &&...args);

  Instruction *entry();
  const Instruction *entry() const;
  void set_entry(Instruction &entry);

  Span<Variable *> variables();
  Span<const Variable *> variables() const;

  std::string to_dot() const;

  bool validate() const;

 private:
  bool validate_all_instruction_pointers_set() const;
  bool validate_all_params_provided() const;
  bool validate_same_variables_in_one_call() const;
  bool validate_parameters() const;
  bool validate_initialization() const;

  struct InitState {
    bool can_be_initialized = false;
    bool can_be_uninitialized = false;
  };

  InitState find_initialization_state_before_instruction(const Instruction &target_instruction,
                                                         const Variable &variable) const;
};

/* -------------------------------------------------------------------- */
/** \name #InstructionCursor Inline Methods
 * \{ */

inline InstructionCursor::InstructionCursor(CallInstruction &instruction)
    : type_(Call), instruction_(&instruction)
{
}

inline InstructionCursor::InstructionCursor(DestructInstruction &instruction)
    : type_(Destruct), instruction_(&instruction)
{
}

inline InstructionCursor::InstructionCursor(BranchInstruction &instruction, bool branch_output)
    : type_(Branch), instruction_(&instruction), branch_output_(branch_output)
{
}

inline InstructionCursor::InstructionCursor(DummyInstruction &instruction)
    : type_(Dummy), instruction_(&instruction)
{
}

inline InstructionCursor InstructionCursor::ForEntry()
{
  InstructionCursor cursor;
  cursor.type_ = Type::Entry;
  return cursor;
}

inline Instruction *InstructionCursor::instruction() const
{
  /* This isn't really const correct unfortunately, because to make it correct we'll need a const
   * version of #InstructionCursor. */
  return instruction_;
}

inline InstructionCursor::Type InstructionCursor::type() const
{
  return type_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Variable Inline Methods
 * \{ */

inline DataType Variable::data_type() const
{
  return data_type_;
}

inline Span<Instruction *> Variable::users()
{
  return users_;
}

inline StringRefNull Variable::name() const
{
  return name_;
}

inline int Variable::index_in_procedure() const
{
  return index_in_graph_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Instruction Inline Methods
 * \{ */

inline InstructionType Instruction::type() const
{
  return type_;
}

inline Span<InstructionCursor> Instruction::prev() const
{
  return prev_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #CallInstruction Inline Methods
 * \{ */

inline const MultiFunction &CallInstruction::fn() const
{
  return *fn_;
}

inline Instruction *CallInstruction::next()
{
  return next_;
}

inline const Instruction *CallInstruction::next() const
{
  return next_;
}

inline Span<Variable *> CallInstruction::params()
{
  return params_;
}

inline Span<const Variable *> CallInstruction::params() const
{
  return params_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #BranchInstruction Inline Methods
 * \{ */

inline Variable *BranchInstruction::condition()
{
  return condition_;
}

inline const Variable *BranchInstruction::condition() const
{
  return condition_;
}

inline Instruction *BranchInstruction::branch_true()
{
  return branch_true_;
}

inline const Instruction *BranchInstruction::branch_true() const
{
  return branch_true_;
}

inline Instruction *BranchInstruction::branch_false()
{
  return branch_false_;
}

inline const Instruction *BranchInstruction::branch_false() const
{
  return branch_false_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #DestructInstruction Inline Methods
 * \{ */

inline Variable *DestructInstruction::variable()
{
  return variable_;
}

inline const Variable *DestructInstruction::variable() const
{
  return variable_;
}

inline Instruction *DestructInstruction::next()
{
  return next_;
}

inline const Instruction *DestructInstruction::next() const
{
  return next_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #DummyInstruction Inline Methods
 * \{ */

inline Instruction *DummyInstruction::next()
{
  return next_;
}

inline const Instruction *DummyInstruction::next() const
{
  return next_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #Procedure Inline Methods
 * \{ */

inline Span<ConstParameter> Procedure::params() const
{
  static_assert(sizeof(Parameter) == sizeof(ConstParameter));
  return params_.as_span().cast<ConstParameter>();
}

inline Instruction *Procedure::entry()
{
  return entry_;
}

inline const Instruction *Procedure::entry() const
{
  return entry_;
}

inline Span<Variable *> Procedure::variables()
{
  return variables_;
}

inline Span<const Variable *> Procedure::variables() const
{
  return variables_;
}

template<typename T, typename... Args>
inline const MultiFunction &Procedure::construct_function(Args &&...args)
{
  destruct_ptr<T> fn = allocator_.construct<T>(std::forward<Args>(args)...);
  const MultiFunction &fn_ref = *fn;
  owned_functions_.append(std::move(fn));
  return fn_ref;
}

/** \} */

}  // namespace blender::fn::multi_function

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup fn
 */

#include "FN_multi_function.hh"

namespace blender::fn {

class MFVariable;
class MFInstruction;
class MFCallInstruction;
class MFBranchInstruction;
class MFDestructInstruction;
class MFDummyInstruction;
class MFReturnInstruction;
class MFProcedure;

/** Every instruction has exactly one of these types. */
enum class MFInstructionType {
  Call,
  Branch,
  Destruct,
  Dummy,
  Return,
};

/**
 * An #MFInstructionCursor points to a position in a multi-function procedure, where an instruction
 * can be inserted.
 */
class MFInstructionCursor {
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
  MFInstruction *instruction_ = nullptr;
  /* Only used when it is a branch instruction. */
  bool branch_output_ = false;

 public:
  MFInstructionCursor() = default;
  MFInstructionCursor(MFCallInstruction &instruction);
  MFInstructionCursor(MFDestructInstruction &instruction);
  MFInstructionCursor(MFBranchInstruction &instruction, bool branch_output);
  MFInstructionCursor(MFDummyInstruction &instruction);

  static MFInstructionCursor ForEntry();

  MFInstruction *next(MFProcedure &procedure) const;
  void set_next(MFProcedure &procedure, MFInstruction *new_instruction) const;

  MFInstruction *instruction() const;

  Type type() const;

  friend bool operator==(const MFInstructionCursor &a, const MFInstructionCursor &b)
  {
    return a.type_ == b.type_ && a.instruction_ == b.instruction_ &&
           a.branch_output_ == b.branch_output_;
  }

  friend bool operator!=(const MFInstructionCursor &a, const MFInstructionCursor &b)
  {
    return !(a == b);
  }
};

/**
 * A variable is similar to a virtual register in other libraries. During evaluation, every is
 * either uninitialized or contains a value for every index (remember, a multi-function procedure
 * is always evaluated for many indices at the same time).
 */
class MFVariable : NonCopyable, NonMovable {
 private:
  MFDataType data_type_;
  Vector<MFInstruction *> users_;
  std::string name_;
  int id_;

  friend MFProcedure;
  friend MFCallInstruction;
  friend MFBranchInstruction;
  friend MFDestructInstruction;

 public:
  MFDataType data_type() const;
  Span<MFInstruction *> users();

  StringRefNull name() const;
  void set_name(std::string name);

  int id() const;
};

/** Base class for all instruction types. */
class MFInstruction : NonCopyable, NonMovable {
 protected:
  MFInstructionType type_;
  Vector<MFInstructionCursor> prev_;

  friend MFProcedure;
  friend MFCallInstruction;
  friend MFBranchInstruction;
  friend MFDestructInstruction;
  friend MFDummyInstruction;
  friend MFReturnInstruction;

 public:
  MFInstructionType type() const;

  /**
   * Other instructions that come before this instruction. There can be multiple previous
   * instructions when branching is used in the procedure.
   */
  Span<MFInstructionCursor> prev() const;
};

/**
 * References a multi-function that is evaluated when the instruction is executed. It also
 * references the variables whose data will be passed into the multi-function.
 */
class MFCallInstruction : public MFInstruction {
 private:
  const MultiFunction *fn_ = nullptr;
  MFInstruction *next_ = nullptr;
  MutableSpan<MFVariable *> params_;

  friend MFProcedure;

 public:
  const MultiFunction &fn() const;

  MFInstruction *next();
  const MFInstruction *next() const;
  void set_next(MFInstruction *instruction);

  void set_param_variable(int param_index, MFVariable *variable);
  void set_params(Span<MFVariable *> variables);

  Span<MFVariable *> params();
  Span<const MFVariable *> params() const;
};

/**
 * What makes a branch instruction special is that it has two successor instructions. One that will
 * be used when a condition variable was true, and one otherwise.
 */
class MFBranchInstruction : public MFInstruction {
 private:
  MFVariable *condition_ = nullptr;
  MFInstruction *branch_true_ = nullptr;
  MFInstruction *branch_false_ = nullptr;

  friend MFProcedure;

 public:
  MFVariable *condition();
  const MFVariable *condition() const;
  void set_condition(MFVariable *variable);

  MFInstruction *branch_true();
  const MFInstruction *branch_true() const;
  void set_branch_true(MFInstruction *instruction);

  MFInstruction *branch_false();
  const MFInstruction *branch_false() const;
  void set_branch_false(MFInstruction *instruction);
};

/**
 * A destruct instruction destructs a single variable. So the variable value will be uninitialized
 * after this instruction. All variables that are not output variables of the procedure, have to be
 * destructed before the procedure ends. Destructing early is generally a good thing, because it
 * might help with memory buffer reuse, which decreases memory-usage and increases performance.
 */
class MFDestructInstruction : public MFInstruction {
 private:
  MFVariable *variable_ = nullptr;
  MFInstruction *next_ = nullptr;

  friend MFProcedure;

 public:
  MFVariable *variable();
  const MFVariable *variable() const;
  void set_variable(MFVariable *variable);

  MFInstruction *next();
  const MFInstruction *next() const;
  void set_next(MFInstruction *instruction);
};

/**
 * This instruction does nothing, it just exists to building a procedure simpler in some cases.
 */
class MFDummyInstruction : public MFInstruction {
 private:
  MFInstruction *next_ = nullptr;

  friend MFProcedure;

 public:
  MFInstruction *next();
  const MFInstruction *next() const;
  void set_next(MFInstruction *instruction);
};

/**
 * This instruction ends the procedure.
 */
class MFReturnInstruction : public MFInstruction {
};

/**
 * Inputs and outputs of the entire procedure network.
 */
struct MFParameter {
  MFParamType::InterfaceType type;
  MFVariable *variable;
};

struct ConstMFParameter {
  MFParamType::InterfaceType type;
  const MFVariable *variable;
};

/**
 * A multi-function procedure allows composing multi-functions in arbitrary ways. It consists of
 * variables and instructions that operate on those variables. Branching and looping within the
 * procedure is supported as well.
 *
 * Typically, a #MFProcedure should be constructed using a #MFProcedureBuilder, which has many more
 * utility methods for common use cases.
 */
class MFProcedure : NonCopyable, NonMovable {
 private:
  LinearAllocator<> allocator_;
  Vector<MFCallInstruction *> call_instructions_;
  Vector<MFBranchInstruction *> branch_instructions_;
  Vector<MFDestructInstruction *> destruct_instructions_;
  Vector<MFDummyInstruction *> dummy_instructions_;
  Vector<MFReturnInstruction *> return_instructions_;
  Vector<MFVariable *> variables_;
  Vector<MFParameter> params_;
  Vector<destruct_ptr<MultiFunction>> owned_functions_;
  MFInstruction *entry_ = nullptr;

  friend class MFProcedureDotExport;

 public:
  MFProcedure() = default;
  ~MFProcedure();

  MFVariable &new_variable(MFDataType data_type, std::string name = "");
  MFCallInstruction &new_call_instruction(const MultiFunction &fn);
  MFBranchInstruction &new_branch_instruction();
  MFDestructInstruction &new_destruct_instruction();
  MFDummyInstruction &new_dummy_instruction();
  MFReturnInstruction &new_return_instruction();

  void add_parameter(MFParamType::InterfaceType interface_type, MFVariable &variable);
  Span<ConstMFParameter> params() const;

  template<typename T, typename... Args> const MultiFunction &construct_function(Args &&...args);

  MFInstruction *entry();
  const MFInstruction *entry() const;
  void set_entry(MFInstruction &entry);

  Span<MFVariable *> variables();
  Span<const MFVariable *> variables() const;

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

  InitState find_initialization_state_before_instruction(const MFInstruction &target_instruction,
                                                         const MFVariable &variable) const;
};

namespace multi_function_procedure_types {
using MFVariable = fn::MFVariable;
using MFInstruction = fn::MFInstruction;
using MFCallInstruction = fn::MFCallInstruction;
using MFBranchInstruction = fn::MFBranchInstruction;
using MFDestructInstruction = fn::MFDestructInstruction;
using MFProcedure = fn::MFProcedure;
}  // namespace multi_function_procedure_types

/* -------------------------------------------------------------------- */
/** \name #MFInstructionCursor Inline Methods
 * \{ */

inline MFInstructionCursor::MFInstructionCursor(MFCallInstruction &instruction)
    : type_(Call), instruction_(&instruction)
{
}

inline MFInstructionCursor::MFInstructionCursor(MFDestructInstruction &instruction)
    : type_(Destruct), instruction_(&instruction)
{
}

inline MFInstructionCursor::MFInstructionCursor(MFBranchInstruction &instruction,
                                                bool branch_output)
    : type_(Branch), instruction_(&instruction), branch_output_(branch_output)
{
}

inline MFInstructionCursor::MFInstructionCursor(MFDummyInstruction &instruction)
    : type_(Dummy), instruction_(&instruction)
{
}

inline MFInstructionCursor MFInstructionCursor::ForEntry()
{
  MFInstructionCursor cursor;
  cursor.type_ = Type::Entry;
  return cursor;
}

inline MFInstruction *MFInstructionCursor::instruction() const
{
  /* This isn't really const correct unfortunately, because to make it correct we'll need a const
   * version of #MFInstructionCursor. */
  return instruction_;
}

inline MFInstructionCursor::Type MFInstructionCursor::type() const
{
  return type_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFVariable Inline Methods
 * \{ */

inline MFDataType MFVariable::data_type() const
{
  return data_type_;
}

inline Span<MFInstruction *> MFVariable::users()
{
  return users_;
}

inline StringRefNull MFVariable::name() const
{
  return name_;
}

inline int MFVariable::id() const
{
  return id_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFInstruction Inline Methods
 * \{ */

inline MFInstructionType MFInstruction::type() const
{
  return type_;
}

inline Span<MFInstructionCursor> MFInstruction::prev() const
{
  return prev_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFCallInstruction Inline Methods
 * \{ */

inline const MultiFunction &MFCallInstruction::fn() const
{
  return *fn_;
}

inline MFInstruction *MFCallInstruction::next()
{
  return next_;
}

inline const MFInstruction *MFCallInstruction::next() const
{
  return next_;
}

inline Span<MFVariable *> MFCallInstruction::params()
{
  return params_;
}

inline Span<const MFVariable *> MFCallInstruction::params() const
{
  return params_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFBranchInstruction Inline Methods
 * \{ */

inline MFVariable *MFBranchInstruction::condition()
{
  return condition_;
}

inline const MFVariable *MFBranchInstruction::condition() const
{
  return condition_;
}

inline MFInstruction *MFBranchInstruction::branch_true()
{
  return branch_true_;
}

inline const MFInstruction *MFBranchInstruction::branch_true() const
{
  return branch_true_;
}

inline MFInstruction *MFBranchInstruction::branch_false()
{
  return branch_false_;
}

inline const MFInstruction *MFBranchInstruction::branch_false() const
{
  return branch_false_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFDestructInstruction Inline Methods
 * \{ */

inline MFVariable *MFDestructInstruction::variable()
{
  return variable_;
}

inline const MFVariable *MFDestructInstruction::variable() const
{
  return variable_;
}

inline MFInstruction *MFDestructInstruction::next()
{
  return next_;
}

inline const MFInstruction *MFDestructInstruction::next() const
{
  return next_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFDummyInstruction Inline Methods
 * \{ */

inline MFInstruction *MFDummyInstruction::next()
{
  return next_;
}

inline const MFInstruction *MFDummyInstruction::next() const
{
  return next_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #MFProcedure Inline Methods
 * \{ */

inline Span<ConstMFParameter> MFProcedure::params() const
{
  static_assert(sizeof(MFParameter) == sizeof(ConstMFParameter));
  return params_.as_span().cast<ConstMFParameter>();
}

inline MFInstruction *MFProcedure::entry()
{
  return entry_;
}

inline const MFInstruction *MFProcedure::entry() const
{
  return entry_;
}

inline Span<MFVariable *> MFProcedure::variables()
{
  return variables_;
}

inline Span<const MFVariable *> MFProcedure::variables() const
{
  return variables_;
}

template<typename T, typename... Args>
inline const MultiFunction &MFProcedure::construct_function(Args &&...args)
{
  destruct_ptr<T> fn = allocator_.construct<T>(std::forward<Args>(args)...);
  const MultiFunction &fn_ref = *fn;
  owned_functions_.append(std::move(fn));
  return fn_ref;
}

/** \} */

}  // namespace blender::fn

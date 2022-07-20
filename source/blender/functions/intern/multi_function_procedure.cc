/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_procedure.hh"

#include "BLI_dot_export.hh"
#include "BLI_stack.hh"

namespace blender::fn {

void MFInstructionCursor::set_next(MFProcedure &procedure, MFInstruction *new_instruction) const
{
  switch (type_) {
    case Type::None: {
      break;
    }
    case Type::Entry: {
      procedure.set_entry(*new_instruction);
      break;
    }
    case Type::Call: {
      static_cast<MFCallInstruction *>(instruction_)->set_next(new_instruction);
      break;
    }
    case Type::Branch: {
      MFBranchInstruction &branch_instruction = *static_cast<MFBranchInstruction *>(instruction_);
      if (branch_output_) {
        branch_instruction.set_branch_true(new_instruction);
      }
      else {
        branch_instruction.set_branch_false(new_instruction);
      }
      break;
    }
    case Type::Destruct: {
      static_cast<MFDestructInstruction *>(instruction_)->set_next(new_instruction);
      break;
    }
    case Type::Dummy: {
      static_cast<MFDummyInstruction *>(instruction_)->set_next(new_instruction);
      break;
    }
  }
}

MFInstruction *MFInstructionCursor::next(MFProcedure &procedure) const
{
  switch (type_) {
    case Type::None:
      return nullptr;
    case Type::Entry:
      return procedure.entry();
    case Type::Call:
      return static_cast<MFCallInstruction *>(instruction_)->next();
    case Type::Branch: {
      MFBranchInstruction &branch_instruction = *static_cast<MFBranchInstruction *>(instruction_);
      if (branch_output_) {
        return branch_instruction.branch_true();
      }
      return branch_instruction.branch_false();
    }
    case Type::Destruct:
      return static_cast<MFDestructInstruction *>(instruction_)->next();
    case Type::Dummy:
      return static_cast<MFDummyInstruction *>(instruction_)->next();
  }
  return nullptr;
}

void MFVariable::set_name(std::string name)
{
  name_ = std::move(name);
}

void MFCallInstruction::set_next(MFInstruction *instruction)
{
  if (next_ != nullptr) {
    next_->prev_.remove_first_occurrence_and_reorder(*this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(*this);
  }
  next_ = instruction;
}

void MFCallInstruction::set_param_variable(int param_index, MFVariable *variable)
{
  if (params_[param_index] != nullptr) {
    params_[param_index]->users_.remove_first_occurrence_and_reorder(this);
  }
  if (variable != nullptr) {
    BLI_assert(fn_->param_type(param_index).data_type() == variable->data_type());
    variable->users_.append(this);
  }
  params_[param_index] = variable;
}

void MFCallInstruction::set_params(Span<MFVariable *> variables)
{
  BLI_assert(variables.size() == params_.size());
  for (const int i : variables.index_range()) {
    this->set_param_variable(i, variables[i]);
  }
}

void MFBranchInstruction::set_condition(MFVariable *variable)
{
  if (condition_ != nullptr) {
    condition_->users_.remove_first_occurrence_and_reorder(this);
  }
  if (variable != nullptr) {
    variable->users_.append(this);
  }
  condition_ = variable;
}

void MFBranchInstruction::set_branch_true(MFInstruction *instruction)
{
  if (branch_true_ != nullptr) {
    branch_true_->prev_.remove_first_occurrence_and_reorder({*this, true});
  }
  if (instruction != nullptr) {
    instruction->prev_.append({*this, true});
  }
  branch_true_ = instruction;
}

void MFBranchInstruction::set_branch_false(MFInstruction *instruction)
{
  if (branch_false_ != nullptr) {
    branch_false_->prev_.remove_first_occurrence_and_reorder({*this, false});
  }
  if (instruction != nullptr) {
    instruction->prev_.append({*this, false});
  }
  branch_false_ = instruction;
}

void MFDestructInstruction::set_variable(MFVariable *variable)
{
  if (variable_ != nullptr) {
    variable_->users_.remove_first_occurrence_and_reorder(this);
  }
  if (variable != nullptr) {
    variable->users_.append(this);
  }
  variable_ = variable;
}

void MFDestructInstruction::set_next(MFInstruction *instruction)
{
  if (next_ != nullptr) {
    next_->prev_.remove_first_occurrence_and_reorder(*this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(*this);
  }
  next_ = instruction;
}

void MFDummyInstruction::set_next(MFInstruction *instruction)
{
  if (next_ != nullptr) {
    next_->prev_.remove_first_occurrence_and_reorder(*this);
  }
  if (instruction != nullptr) {
    instruction->prev_.append(*this);
  }
  next_ = instruction;
}

MFVariable &MFProcedure::new_variable(MFDataType data_type, std::string name)
{
  MFVariable &variable = *allocator_.construct<MFVariable>().release();
  variable.name_ = std::move(name);
  variable.data_type_ = data_type;
  variable.index_in_graph_ = variables_.size();
  variables_.append(&variable);
  return variable;
}

MFCallInstruction &MFProcedure::new_call_instruction(const MultiFunction &fn)
{
  MFCallInstruction &instruction = *allocator_.construct<MFCallInstruction>().release();
  instruction.type_ = MFInstructionType::Call;
  instruction.fn_ = &fn;
  instruction.params_ = allocator_.allocate_array<MFVariable *>(fn.param_amount());
  instruction.params_.fill(nullptr);
  call_instructions_.append(&instruction);
  return instruction;
}

MFBranchInstruction &MFProcedure::new_branch_instruction()
{
  MFBranchInstruction &instruction = *allocator_.construct<MFBranchInstruction>().release();
  instruction.type_ = MFInstructionType::Branch;
  branch_instructions_.append(&instruction);
  return instruction;
}

MFDestructInstruction &MFProcedure::new_destruct_instruction()
{
  MFDestructInstruction &instruction = *allocator_.construct<MFDestructInstruction>().release();
  instruction.type_ = MFInstructionType::Destruct;
  destruct_instructions_.append(&instruction);
  return instruction;
}

MFDummyInstruction &MFProcedure::new_dummy_instruction()
{
  MFDummyInstruction &instruction = *allocator_.construct<MFDummyInstruction>().release();
  instruction.type_ = MFInstructionType::Dummy;
  dummy_instructions_.append(&instruction);
  return instruction;
}

MFReturnInstruction &MFProcedure::new_return_instruction()
{
  MFReturnInstruction &instruction = *allocator_.construct<MFReturnInstruction>().release();
  instruction.type_ = MFInstructionType::Return;
  return_instructions_.append(&instruction);
  return instruction;
}

void MFProcedure::add_parameter(MFParamType::InterfaceType interface_type, MFVariable &variable)
{
  params_.append({interface_type, &variable});
}

void MFProcedure::set_entry(MFInstruction &entry)
{
  if (entry_ != nullptr) {
    entry_->prev_.remove_first_occurrence_and_reorder(MFInstructionCursor::ForEntry());
  }
  entry_ = &entry;
  entry_->prev_.append(MFInstructionCursor::ForEntry());
}

MFProcedure::~MFProcedure()
{
  for (MFCallInstruction *instruction : call_instructions_) {
    instruction->~MFCallInstruction();
  }
  for (MFBranchInstruction *instruction : branch_instructions_) {
    instruction->~MFBranchInstruction();
  }
  for (MFDestructInstruction *instruction : destruct_instructions_) {
    instruction->~MFDestructInstruction();
  }
  for (MFDummyInstruction *instruction : dummy_instructions_) {
    instruction->~MFDummyInstruction();
  }
  for (MFReturnInstruction *instruction : return_instructions_) {
    instruction->~MFReturnInstruction();
  }
  for (MFVariable *variable : variables_) {
    variable->~MFVariable();
  }
}

bool MFProcedure::validate() const
{
  if (entry_ == nullptr) {
    return false;
  }
  if (!this->validate_all_instruction_pointers_set()) {
    return false;
  }
  if (!this->validate_all_params_provided()) {
    return false;
  }
  if (!this->validate_same_variables_in_one_call()) {
    return false;
  }
  if (!this->validate_parameters()) {
    return false;
  }
  if (!this->validate_initialization()) {
    return false;
  }
  return true;
}

bool MFProcedure::validate_all_instruction_pointers_set() const
{
  for (const MFCallInstruction *instruction : call_instructions_) {
    if (instruction->next_ == nullptr) {
      return false;
    }
  }
  for (const MFDestructInstruction *instruction : destruct_instructions_) {
    if (instruction->next_ == nullptr) {
      return false;
    }
  }
  for (const MFBranchInstruction *instruction : branch_instructions_) {
    if (instruction->branch_true_ == nullptr) {
      return false;
    }
    if (instruction->branch_false_ == nullptr) {
      return false;
    }
  }
  for (const MFDummyInstruction *instruction : dummy_instructions_) {
    if (instruction->next_ == nullptr) {
      return false;
    }
  }
  return true;
}

bool MFProcedure::validate_all_params_provided() const
{
  for (const MFCallInstruction *instruction : call_instructions_) {
    const MultiFunction &fn = instruction->fn();
    for (const int param_index : fn.param_indices()) {
      const MFParamType param_type = fn.param_type(param_index);
      if (param_type.category() == MFParamCategory::SingleOutput) {
        /* Single outputs are optional. */
        continue;
      }
      const MFVariable *variable = instruction->params_[param_index];
      if (variable == nullptr) {
        return false;
      }
    }
  }
  for (const MFBranchInstruction *instruction : branch_instructions_) {
    if (instruction->condition_ == nullptr) {
      return false;
    }
  }
  for (const MFDestructInstruction *instruction : destruct_instructions_) {
    if (instruction->variable_ == nullptr) {
      return false;
    }
  }
  return true;
}

bool MFProcedure::validate_same_variables_in_one_call() const
{
  for (const MFCallInstruction *instruction : call_instructions_) {
    const MultiFunction &fn = *instruction->fn_;
    for (const int param_index : fn.param_indices()) {
      const MFParamType param_type = fn.param_type(param_index);
      const MFVariable *variable = instruction->params_[param_index];
      if (variable == nullptr) {
        continue;
      }
      for (const int other_param_index : fn.param_indices()) {
        if (other_param_index == param_index) {
          continue;
        }
        const MFVariable *other_variable = instruction->params_[other_param_index];
        if (other_variable != variable) {
          continue;
        }
        if (ELEM(param_type.interface_type(), MFParamType::Mutable, MFParamType::Output)) {
          /* When a variable is used as mutable or output parameter, it can only be used once. */
          return false;
        }
        const MFParamType other_param_type = fn.param_type(other_param_index);
        /* A variable is allowed to be used as input more than once. */
        if (other_param_type.interface_type() != MFParamType::Input) {
          return false;
        }
      }
    }
  }
  return true;
}

bool MFProcedure::validate_parameters() const
{
  Set<const MFVariable *> variables;
  for (const MFParameter &param : params_) {
    /* One variable cannot be used as multiple parameters. */
    if (!variables.add(param.variable)) {
      return false;
    }
  }
  return true;
}

bool MFProcedure::validate_initialization() const
{
  /* TODO: Issue warning when it maybe wrongly initialized. */
  for (const MFDestructInstruction *instruction : destruct_instructions_) {
    const MFVariable &variable = *instruction->variable_;
    const InitState state = this->find_initialization_state_before_instruction(*instruction,
                                                                               variable);
    if (!state.can_be_initialized) {
      return false;
    }
  }
  for (const MFBranchInstruction *instruction : branch_instructions_) {
    const MFVariable &variable = *instruction->condition_;
    const InitState state = this->find_initialization_state_before_instruction(*instruction,
                                                                               variable);
    if (!state.can_be_initialized) {
      return false;
    }
  }
  for (const MFCallInstruction *instruction : call_instructions_) {
    const MultiFunction &fn = *instruction->fn_;
    for (const int param_index : fn.param_indices()) {
      const MFParamType param_type = fn.param_type(param_index);
      /* If the parameter was an unneeded output, it could be null. */
      if (!instruction->params_[param_index]) {
        continue;
      }
      const MFVariable &variable = *instruction->params_[param_index];
      const InitState state = this->find_initialization_state_before_instruction(*instruction,
                                                                                 variable);
      switch (param_type.interface_type()) {
        case MFParamType::Input:
        case MFParamType::Mutable: {
          if (!state.can_be_initialized) {
            return false;
          }
          break;
        }
        case MFParamType::Output: {
          if (!state.can_be_uninitialized) {
            return false;
          }
          break;
        }
      }
    }
  }
  Set<const MFVariable *> variables_that_should_be_initialized_on_return;
  for (const MFParameter &param : params_) {
    if (ELEM(param.type, MFParamType::Mutable, MFParamType::Output)) {
      variables_that_should_be_initialized_on_return.add_new(param.variable);
    }
  }
  for (const MFReturnInstruction *instruction : return_instructions_) {
    for (const MFVariable *variable : variables_) {
      const InitState init_state = this->find_initialization_state_before_instruction(*instruction,
                                                                                      *variable);
      if (variables_that_should_be_initialized_on_return.contains(variable)) {
        if (!init_state.can_be_initialized) {
          return false;
        }
      }
      else {
        if (!init_state.can_be_uninitialized) {
          return false;
        }
      }
    }
  }
  return true;
}

MFProcedure::InitState MFProcedure::find_initialization_state_before_instruction(
    const MFInstruction &target_instruction, const MFVariable &target_variable) const
{
  InitState state;

  auto check_entry_instruction = [&]() {
    bool caller_initialized_variable = false;
    for (const MFParameter &param : params_) {
      if (param.variable == &target_variable) {
        if (ELEM(param.type, MFParamType::Input, MFParamType::Mutable)) {
          caller_initialized_variable = true;
          break;
        }
      }
    }
    if (caller_initialized_variable) {
      state.can_be_initialized = true;
    }
    else {
      state.can_be_uninitialized = true;
    }
  };

  if (&target_instruction == entry_) {
    check_entry_instruction();
  }

  Set<const MFInstruction *> checked_instructions;
  Stack<const MFInstruction *> instructions_to_check;
  for (const MFInstructionCursor &cursor : target_instruction.prev_) {
    if (cursor.instruction() != nullptr) {
      instructions_to_check.push(cursor.instruction());
    }
  }

  while (!instructions_to_check.is_empty()) {
    const MFInstruction &instruction = *instructions_to_check.pop();
    if (!checked_instructions.add(&instruction)) {
      /* Skip if the instruction has been checked already. */
      continue;
    }
    bool state_modified = false;
    switch (instruction.type_) {
      case MFInstructionType::Call: {
        const MFCallInstruction &call_instruction = static_cast<const MFCallInstruction &>(
            instruction);
        const MultiFunction &fn = *call_instruction.fn_;
        for (const int param_index : fn.param_indices()) {
          if (call_instruction.params_[param_index] == &target_variable) {
            const MFParamType param_type = fn.param_type(param_index);
            if (param_type.interface_type() == MFParamType::Output) {
              state.can_be_initialized = true;
              state_modified = true;
              break;
            }
          }
        }
        break;
      }
      case MFInstructionType::Destruct: {
        const MFDestructInstruction &destruct_instruction =
            static_cast<const MFDestructInstruction &>(instruction);
        if (destruct_instruction.variable_ == &target_variable) {
          state.can_be_uninitialized = true;
          state_modified = true;
        }
        break;
      }
      case MFInstructionType::Branch:
      case MFInstructionType::Dummy:
      case MFInstructionType::Return: {
        /* These instruction types don't change the initialization state of variables. */
        break;
      }
    }

    if (!state_modified) {
      if (&instruction == entry_) {
        check_entry_instruction();
      }
      for (const MFInstructionCursor &cursor : instruction.prev_) {
        if (cursor.instruction() != nullptr) {
          instructions_to_check.push(cursor.instruction());
        }
      }
    }
  }

  return state;
}

class MFProcedureDotExport {
 private:
  const MFProcedure &procedure_;
  dot::DirectedGraph digraph_;
  Map<const MFInstruction *, dot::Node *> dot_nodes_by_begin_;
  Map<const MFInstruction *, dot::Node *> dot_nodes_by_end_;

 public:
  MFProcedureDotExport(const MFProcedure &procedure) : procedure_(procedure)
  {
  }

  std::string generate()
  {
    this->create_nodes();
    this->create_edges();
    return digraph_.to_dot_string();
  }

  void create_nodes()
  {
    Vector<const MFInstruction *> all_instructions;
    auto add_instructions = [&](auto instructions) {
      all_instructions.extend(instructions.begin(), instructions.end());
    };
    add_instructions(procedure_.call_instructions_);
    add_instructions(procedure_.branch_instructions_);
    add_instructions(procedure_.destruct_instructions_);
    add_instructions(procedure_.dummy_instructions_);
    add_instructions(procedure_.return_instructions_);

    Set<const MFInstruction *> handled_instructions;

    for (const MFInstruction *representative : all_instructions) {
      if (handled_instructions.contains(representative)) {
        continue;
      }
      Vector<const MFInstruction *> block_instructions = this->get_instructions_in_block(
          *representative);
      std::stringstream ss;
      ss << "<";

      for (const MFInstruction *current : block_instructions) {
        handled_instructions.add_new(current);
        switch (current->type()) {
          case MFInstructionType::Call: {
            this->instruction_to_string(*static_cast<const MFCallInstruction *>(current), ss);
            break;
          }
          case MFInstructionType::Destruct: {
            this->instruction_to_string(*static_cast<const MFDestructInstruction *>(current), ss);
            break;
          }
          case MFInstructionType::Dummy: {
            this->instruction_to_string(*static_cast<const MFDummyInstruction *>(current), ss);
            break;
          }
          case MFInstructionType::Return: {
            this->instruction_to_string(*static_cast<const MFReturnInstruction *>(current), ss);
            break;
          }
          case MFInstructionType::Branch: {
            this->instruction_to_string(*static_cast<const MFBranchInstruction *>(current), ss);
            break;
          }
        }
        ss << R"(<br align="left" />)";
      }
      ss << ">";

      dot::Node &dot_node = digraph_.new_node(ss.str());
      dot_node.set_shape(dot::Attr_shape::Rectangle);
      dot_nodes_by_begin_.add_new(block_instructions.first(), &dot_node);
      dot_nodes_by_end_.add_new(block_instructions.last(), &dot_node);
    }
  }

  void create_edges()
  {
    auto create_edge = [&](dot::Node &from_node,
                           const MFInstruction *to_instruction) -> dot::DirectedEdge & {
      if (to_instruction == nullptr) {
        dot::Node &to_node = digraph_.new_node("missing");
        to_node.set_shape(dot::Attr_shape::Diamond);
        return digraph_.new_edge(from_node, to_node);
      }
      dot::Node &to_node = *dot_nodes_by_begin_.lookup(to_instruction);
      return digraph_.new_edge(from_node, to_node);
    };

    for (auto item : dot_nodes_by_end_.items()) {
      const MFInstruction &from_instruction = *item.key;
      dot::Node &from_node = *item.value;
      switch (from_instruction.type()) {
        case MFInstructionType::Call: {
          const MFInstruction *to_instruction =
              static_cast<const MFCallInstruction &>(from_instruction).next();
          create_edge(from_node, to_instruction);
          break;
        }
        case MFInstructionType::Destruct: {
          const MFInstruction *to_instruction =
              static_cast<const MFDestructInstruction &>(from_instruction).next();
          create_edge(from_node, to_instruction);
          break;
        }
        case MFInstructionType::Dummy: {
          const MFInstruction *to_instruction =
              static_cast<const MFDummyInstruction &>(from_instruction).next();
          create_edge(from_node, to_instruction);
          break;
        }
        case MFInstructionType::Return: {
          break;
        }
        case MFInstructionType::Branch: {
          const MFBranchInstruction &branch_instruction = static_cast<const MFBranchInstruction &>(
              from_instruction);
          const MFInstruction *to_true_instruction = branch_instruction.branch_true();
          const MFInstruction *to_false_instruction = branch_instruction.branch_false();
          create_edge(from_node, to_true_instruction).attributes.set("color", "#118811");
          create_edge(from_node, to_false_instruction).attributes.set("color", "#881111");
          break;
        }
      }
    }

    dot::Node &entry_node = this->create_entry_node();
    create_edge(entry_node, procedure_.entry());
  }

  bool has_to_be_block_begin(const MFInstruction &instruction)
  {
    if (instruction.prev().size() != 1) {
      return true;
    }
    if (ELEM(instruction.prev()[0].type(),
             MFInstructionCursor::Type::Branch,
             MFInstructionCursor::Type::Entry)) {
      return true;
    }
    return false;
  }

  const MFInstruction &get_first_instruction_in_block(const MFInstruction &representative)
  {
    const MFInstruction *current = &representative;
    while (!this->has_to_be_block_begin(*current)) {
      current = current->prev()[0].instruction();
      if (current == &representative) {
        /* There is a loop without entry or exit, just break it up here. */
        break;
      }
    }
    return *current;
  }

  const MFInstruction *get_next_instruction_in_block(const MFInstruction &instruction,
                                                     const MFInstruction &block_begin)
  {
    const MFInstruction *next = nullptr;
    switch (instruction.type()) {
      case MFInstructionType::Call: {
        next = static_cast<const MFCallInstruction &>(instruction).next();
        break;
      }
      case MFInstructionType::Destruct: {
        next = static_cast<const MFDestructInstruction &>(instruction).next();
        break;
      }
      case MFInstructionType::Dummy: {
        next = static_cast<const MFDummyInstruction &>(instruction).next();
        break;
      }
      case MFInstructionType::Return:
      case MFInstructionType::Branch: {
        break;
      }
    }
    if (next == nullptr) {
      return nullptr;
    }
    if (next == &block_begin) {
      return nullptr;
    }
    if (this->has_to_be_block_begin(*next)) {
      return nullptr;
    }
    return next;
  }

  Vector<const MFInstruction *> get_instructions_in_block(const MFInstruction &representative)
  {
    Vector<const MFInstruction *> instructions;
    const MFInstruction &begin = this->get_first_instruction_in_block(representative);
    for (const MFInstruction *current = &begin; current != nullptr;
         current = this->get_next_instruction_in_block(*current, begin)) {
      instructions.append(current);
    }
    return instructions;
  }

  void variable_to_string(const MFVariable *variable, std::stringstream &ss)
  {
    if (variable == nullptr) {
      ss << "null";
    }
    else {
      ss << "$" << variable->index_in_procedure();
      if (!variable->name().is_empty()) {
        ss << "(" << variable->name() << ")";
      }
    }
  }

  void instruction_name_format(StringRef name, std::stringstream &ss)
  {
    ss << name;
  }

  void instruction_to_string(const MFCallInstruction &instruction, std::stringstream &ss)
  {
    const MultiFunction &fn = instruction.fn();
    this->instruction_name_format(fn.debug_name() + ": ", ss);
    for (const int param_index : fn.param_indices()) {
      const MFParamType param_type = fn.param_type(param_index);
      const MFVariable *variable = instruction.params()[param_index];
      ss << R"(<font color="grey30">)";
      switch (param_type.interface_type()) {
        case MFParamType::Input: {
          ss << "in";
          break;
        }
        case MFParamType::Mutable: {
          ss << "mut";
          break;
        }
        case MFParamType::Output: {
          ss << "out";
          break;
        }
      }
      ss << " </font> ";
      variable_to_string(variable, ss);
      if (param_index < fn.param_amount() - 1) {
        ss << ", ";
      }
    }
  }

  void instruction_to_string(const MFDestructInstruction &instruction, std::stringstream &ss)
  {
    instruction_name_format("Destruct ", ss);
    variable_to_string(instruction.variable(), ss);
  }

  void instruction_to_string(const MFDummyInstruction &UNUSED(instruction), std::stringstream &ss)
  {
    instruction_name_format("Dummy ", ss);
  }

  void instruction_to_string(const MFReturnInstruction &UNUSED(instruction), std::stringstream &ss)
  {
    instruction_name_format("Return ", ss);

    Vector<ConstMFParameter> outgoing_parameters;
    for (const ConstMFParameter &param : procedure_.params()) {
      if (ELEM(param.type, MFParamType::Mutable, MFParamType::Output)) {
        outgoing_parameters.append(param);
      }
    }
    for (const int param_index : outgoing_parameters.index_range()) {
      const ConstMFParameter &param = outgoing_parameters[param_index];
      variable_to_string(param.variable, ss);
      if (param_index < outgoing_parameters.size() - 1) {
        ss << ", ";
      }
    }
  }

  void instruction_to_string(const MFBranchInstruction &instruction, std::stringstream &ss)
  {
    instruction_name_format("Branch ", ss);
    variable_to_string(instruction.condition(), ss);
  }

  dot::Node &create_entry_node()
  {
    std::stringstream ss;
    ss << "Entry: ";
    Vector<ConstMFParameter> incoming_parameters;
    for (const ConstMFParameter &param : procedure_.params()) {
      if (ELEM(param.type, MFParamType::Input, MFParamType::Mutable)) {
        incoming_parameters.append(param);
      }
    }
    for (const int param_index : incoming_parameters.index_range()) {
      const ConstMFParameter &param = incoming_parameters[param_index];
      variable_to_string(param.variable, ss);
      if (param_index < incoming_parameters.size() - 1) {
        ss << ", ";
      }
    }

    dot::Node &node = digraph_.new_node(ss.str());
    node.set_shape(dot::Attr_shape::Ellipse);
    return node;
  }
};

std::string MFProcedure::to_dot() const
{
  MFProcedureDotExport dot_export{*this};
  return dot_export.generate();
}

}  // namespace blender::fn

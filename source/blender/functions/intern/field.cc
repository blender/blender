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

#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_vector_set.hh"

#include "FN_field.hh"
#include "FN_multi_function_procedure.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"
#include "FN_multi_function_procedure_optimization.hh"

namespace blender::fn {

/* --------------------------------------------------------------------
 * Field Evaluation.
 */

struct FieldTreeInfo {
  /**
   * When fields are built, they only have references to the fields that they depend on. This map
   * allows traversal of fields in the opposite direction. So for every field it stores the other
   * fields that depend on it directly.
   */
  MultiValueMap<GFieldRef, GFieldRef> field_users;
  /**
   * The same field input may exist in the field tree as as separate nodes due to the way
   * the tree is constructed. This set contains every different input only once.
   */
  VectorSet<std::reference_wrapper<const FieldInput>> deduplicated_field_inputs;
};

/**
 * Collects some information from the field tree that is required by later steps.
 */
static FieldTreeInfo preprocess_field_tree(Span<GFieldRef> entry_fields)
{
  FieldTreeInfo field_tree_info;

  Stack<GFieldRef> fields_to_check;
  Set<GFieldRef> handled_fields;

  for (GFieldRef field : entry_fields) {
    if (handled_fields.add(field)) {
      fields_to_check.push(field);
    }
  }

  while (!fields_to_check.is_empty()) {
    GFieldRef field = fields_to_check.pop();
    const FieldNode &field_node = field.node();
    switch (field_node.node_type()) {
      case FieldNodeType::Input: {
        const FieldInput &field_input = static_cast<const FieldInput &>(field_node);
        field_tree_info.deduplicated_field_inputs.add(field_input);
        break;
      }
      case FieldNodeType::Operation: {
        const FieldOperation &operation = static_cast<const FieldOperation &>(field_node);
        for (const GFieldRef operation_input : operation.inputs()) {
          field_tree_info.field_users.add(operation_input, field);
          if (handled_fields.add(operation_input)) {
            fields_to_check.push(operation_input);
          }
        }
        break;
      }
      case FieldNodeType::Constant: {
        /* Nothing to do. */
        break;
      }
    }
  }
  return field_tree_info;
}

/**
 * Retrieves the data from the context that is passed as input into the field.
 */
static Vector<GVArray> get_field_context_inputs(
    ResourceScope &scope,
    const IndexMask mask,
    const FieldContext &context,
    const Span<std::reference_wrapper<const FieldInput>> field_inputs)
{
  Vector<GVArray> field_context_inputs;
  for (const FieldInput &field_input : field_inputs) {
    GVArray varray = context.get_varray_for_input(field_input, mask, scope);
    if (!varray) {
      const CPPType &type = field_input.cpp_type();
      varray = GVArray::ForSingleDefault(type, mask.min_array_size());
    }
    field_context_inputs.append(varray);
  }
  return field_context_inputs;
}

/**
 * \return A set that contains all fields from the field tree that depend on an input that varies
 * for different indices.
 */
static Set<GFieldRef> find_varying_fields(const FieldTreeInfo &field_tree_info,
                                          Span<GVArray> field_context_inputs)
{
  Set<GFieldRef> found_fields;
  Stack<GFieldRef> fields_to_check;

  /* The varying fields are the ones that depend on inputs that are not constant. Therefore we
   * start the tree search at the non-constant input fields and traverse through all fields that
   * depend on them. */
  for (const int i : field_context_inputs.index_range()) {
    const GVArray &varray = field_context_inputs[i];
    if (varray.is_single()) {
      continue;
    }
    const FieldInput &field_input = field_tree_info.deduplicated_field_inputs[i];
    const GFieldRef field_input_field{field_input, 0};
    const Span<GFieldRef> users = field_tree_info.field_users.lookup(field_input_field);
    for (const GFieldRef &field : users) {
      if (found_fields.add(field)) {
        fields_to_check.push(field);
      }
    }
  }
  while (!fields_to_check.is_empty()) {
    GFieldRef field = fields_to_check.pop();
    const Span<GFieldRef> users = field_tree_info.field_users.lookup(field);
    for (GFieldRef field : users) {
      if (found_fields.add(field)) {
        fields_to_check.push(field);
      }
    }
  }
  return found_fields;
}

/**
 * Builds the #procedure so that it computes the the fields.
 */
static void build_multi_function_procedure_for_fields(MFProcedure &procedure,
                                                      ResourceScope &scope,
                                                      const FieldTreeInfo &field_tree_info,
                                                      Span<GFieldRef> output_fields)
{
  MFProcedureBuilder builder{procedure};
  /* Every input, intermediate and output field corresponds to a variable in the procedure. */
  Map<GFieldRef, MFVariable *> variable_by_field;

  /* Start by adding the field inputs as parameters to the procedure. */
  for (const FieldInput &field_input : field_tree_info.deduplicated_field_inputs) {
    MFVariable &variable = builder.add_input_parameter(
        MFDataType::ForSingle(field_input.cpp_type()), field_input.debug_name());
    variable_by_field.add_new({field_input, 0}, &variable);
  }

  /* Utility struct that is used to do proper depth first search traversal of the tree below. */
  struct FieldWithIndex {
    GFieldRef field;
    int current_input_index = 0;
  };

  for (GFieldRef field : output_fields) {
    /* We start a new stack for each output field to make sure that a field pushed later to the
     * stack does never depend on a field that was pushed before. */
    Stack<FieldWithIndex> fields_to_check;
    fields_to_check.push({field, 0});
    while (!fields_to_check.is_empty()) {
      FieldWithIndex &field_with_index = fields_to_check.peek();
      const GFieldRef &field = field_with_index.field;
      if (variable_by_field.contains(field)) {
        /* The field has been handled already. */
        fields_to_check.pop();
        continue;
      }
      const FieldNode &field_node = field.node();
      switch (field_node.node_type()) {
        case FieldNodeType::Input: {
          /* Field inputs should already be handled above. */
          break;
        }
        case FieldNodeType::Operation: {
          const FieldOperation &operation_node = static_cast<const FieldOperation &>(field.node());
          const Span<GField> operation_inputs = operation_node.inputs();

          if (field_with_index.current_input_index < operation_inputs.size()) {
            /* Not all inputs are handled yet. Push the next input field to the stack and increment
             * the input index. */
            fields_to_check.push({operation_inputs[field_with_index.current_input_index]});
            field_with_index.current_input_index++;
          }
          else {
            /* All inputs variables are ready, now gather all variables that are used by the
             * function and call it. */
            const MultiFunction &multi_function = operation_node.multi_function();
            Vector<MFVariable *> variables(multi_function.param_amount());

            int param_input_index = 0;
            int param_output_index = 0;
            for (const int param_index : multi_function.param_indices()) {
              const MFParamType param_type = multi_function.param_type(param_index);
              const MFParamType::InterfaceType interface_type = param_type.interface_type();
              if (interface_type == MFParamType::Input) {
                const GField &input_field = operation_inputs[param_input_index];
                variables[param_index] = variable_by_field.lookup(input_field);
                param_input_index++;
              }
              else if (interface_type == MFParamType::Output) {
                const GFieldRef output_field{operation_node, param_output_index};
                const bool output_is_ignored =
                    field_tree_info.field_users.lookup(output_field).is_empty() &&
                    !output_fields.contains(output_field);
                if (output_is_ignored) {
                  /* Ignored outputs don't need a variable. */
                  variables[param_index] = nullptr;
                }
                else {
                  /* Create a new variable for used outputs. */
                  MFVariable &new_variable = procedure.new_variable(param_type.data_type());
                  variables[param_index] = &new_variable;
                  variable_by_field.add_new(output_field, &new_variable);
                }
                param_output_index++;
              }
              else {
                BLI_assert_unreachable();
              }
            }
            builder.add_call_with_all_variables(multi_function, variables);
          }
          break;
        }
        case FieldNodeType::Constant: {
          const FieldConstant &constant_node = static_cast<const FieldConstant &>(field_node);
          const MultiFunction &fn = procedure.construct_function<CustomMF_GenericConstant>(
              constant_node.type(), constant_node.value().get(), false);
          MFVariable &new_variable = *builder.add_call<1>(fn)[0];
          variable_by_field.add_new(field, &new_variable);
          break;
        }
      }
    }
  }

  /* Add output parameters to the procedure. */
  Set<MFVariable *> already_output_variables;
  for (const GFieldRef &field : output_fields) {
    MFVariable *variable = variable_by_field.lookup(field);
    if (!already_output_variables.add(variable)) {
      /* One variable can be output at most once. To output the same value twice, we have to make
       * a copy first. */
      const MultiFunction &copy_fn = scope.construct<CustomMF_GenericCopy>(variable->data_type());
      variable = builder.add_call<1>(copy_fn, {variable})[0];
    }
    builder.add_output_parameter(*variable);
  }

  /* Remove the variables that should not be destructed from the map. */
  for (const GFieldRef &field : output_fields) {
    variable_by_field.remove(field);
  }
  /* Add destructor calls for the remaining variables. */
  for (MFVariable *variable : variable_by_field.values()) {
    builder.add_destruct(*variable);
  }

  MFReturnInstruction &return_instr = builder.add_return();

  procedure_optimization::move_destructs_up(procedure, return_instr);

  // std::cout << procedure.to_dot() << "\n";
  BLI_assert(procedure.validate());
}

Vector<GVArray> evaluate_fields(ResourceScope &scope,
                                Span<GFieldRef> fields_to_evaluate,
                                IndexMask mask,
                                const FieldContext &context,
                                Span<GVMutableArray> dst_varrays)
{
  Vector<GVArray> r_varrays(fields_to_evaluate.size());
  Array<bool> is_output_written_to_dst(fields_to_evaluate.size(), false);
  const int array_size = mask.min_array_size();

  if (mask.is_empty()) {
    for (const int i : fields_to_evaluate.index_range()) {
      const CPPType &type = fields_to_evaluate[i].cpp_type();
      r_varrays[i] = GVArray::ForEmpty(type);
    }
    return r_varrays;
  }

  /* Destination arrays are optional. Create a small utility method to access them. */
  auto get_dst_varray = [&](int index) -> GVMutableArray {
    if (dst_varrays.is_empty()) {
      return {};
    }
    const GVMutableArray &varray = dst_varrays[index];
    if (!varray) {
      return {};
    }
    BLI_assert(varray.size() >= array_size);
    return varray;
  };

  /* Traverse the field tree and prepare some data that is used in later steps. */
  FieldTreeInfo field_tree_info = preprocess_field_tree(fields_to_evaluate);

  /* Get inputs that will be passed into the field when evaluated. */
  Vector<GVArray> field_context_inputs = get_field_context_inputs(
      scope, mask, context, field_tree_info.deduplicated_field_inputs);

  /* Finish fields that don't need any processing directly. */
  for (const int out_index : fields_to_evaluate.index_range()) {
    const GFieldRef &field = fields_to_evaluate[out_index];
    const FieldNode &field_node = field.node();
    switch (field_node.node_type()) {
      case FieldNodeType::Input: {
        const FieldInput &field_input = static_cast<const FieldInput &>(field.node());
        const int field_input_index = field_tree_info.deduplicated_field_inputs.index_of(
            field_input);
        const GVArray &varray = field_context_inputs[field_input_index];
        r_varrays[out_index] = varray;
        break;
      }
      case FieldNodeType::Constant: {
        const FieldConstant &field_constant = static_cast<const FieldConstant &>(field.node());
        r_varrays[out_index] = GVArray::ForSingleRef(
            field_constant.type(), mask.min_array_size(), field_constant.value().get());
        break;
      }
      case FieldNodeType::Operation: {
        break;
      }
    }
  }

  Set<GFieldRef> varying_fields = find_varying_fields(field_tree_info, field_context_inputs);

  /* Separate fields into two categories. Those that are constant and need to be evaluated only
   * once, and those that need to be evaluated for every index. */
  Vector<GFieldRef> varying_fields_to_evaluate;
  Vector<int> varying_field_indices;
  Vector<GFieldRef> constant_fields_to_evaluate;
  Vector<int> constant_field_indices;
  for (const int i : fields_to_evaluate.index_range()) {
    if (r_varrays[i]) {
      /* Already done. */
      continue;
    }
    GFieldRef field = fields_to_evaluate[i];
    if (varying_fields.contains(field)) {
      varying_fields_to_evaluate.append(field);
      varying_field_indices.append(i);
    }
    else {
      constant_fields_to_evaluate.append(field);
      constant_field_indices.append(i);
    }
  }

  /* Evaluate varying fields if necessary. */
  if (!varying_fields_to_evaluate.is_empty()) {
    /* Build the procedure for those fields. */
    MFProcedure procedure;
    build_multi_function_procedure_for_fields(
        procedure, scope, field_tree_info, varying_fields_to_evaluate);
    MFProcedureExecutor procedure_executor{procedure};

    MFParamsBuilder mf_params{procedure_executor, &mask};
    MFContextBuilder mf_context;

    /* Provide inputs to the procedure executor. */
    for (const GVArray &varray : field_context_inputs) {
      mf_params.add_readonly_single_input(varray);
    }

    for (const int i : varying_fields_to_evaluate.index_range()) {
      const GFieldRef &field = varying_fields_to_evaluate[i];
      const CPPType &type = field.cpp_type();
      const int out_index = varying_field_indices[i];

      /* Try to get an existing virtual array that the result should be written into. */
      GVMutableArray dst_varray = get_dst_varray(out_index);
      void *buffer;
      if (!dst_varray || !dst_varray.is_span()) {
        /* Allocate a new buffer for the computed result. */
        buffer = scope.linear_allocator().allocate(type.size() * array_size, type.alignment());

        if (!type.is_trivially_destructible()) {
          /* Destruct values in the end. */
          scope.add_destruct_call(
              [buffer, mask, &type]() { type.destruct_indices(buffer, mask); });
        }

        r_varrays[out_index] = GVArray::ForSpan({type, buffer, array_size});
      }
      else {
        /* Write the result into the existing span. */
        buffer = dst_varray.get_internal_span().data();

        r_varrays[out_index] = dst_varray;
        is_output_written_to_dst[out_index] = true;
      }

      /* Pass output buffer to the procedure executor. */
      const GMutableSpan span{type, buffer, array_size};
      mf_params.add_uninitialized_single_output(span);
    }

    procedure_executor.call_auto(mask, mf_params, mf_context);
  }

  /* Evaluate constant fields if necessary. */
  if (!constant_fields_to_evaluate.is_empty()) {
    /* Build the procedure for those fields. */
    MFProcedure procedure;
    build_multi_function_procedure_for_fields(
        procedure, scope, field_tree_info, constant_fields_to_evaluate);
    MFProcedureExecutor procedure_executor{procedure};
    MFParamsBuilder mf_params{procedure_executor, 1};
    MFContextBuilder mf_context;

    /* Provide inputs to the procedure executor. */
    for (const GVArray &varray : field_context_inputs) {
      mf_params.add_readonly_single_input(varray);
    }

    for (const int i : constant_fields_to_evaluate.index_range()) {
      const GFieldRef &field = constant_fields_to_evaluate[i];
      const CPPType &type = field.cpp_type();
      /* Allocate memory where the computed value will be stored in. */
      void *buffer = scope.linear_allocator().allocate(type.size(), type.alignment());

      if (!type.is_trivially_destructible()) {
        /* Destruct value in the end. */
        scope.add_destruct_call([buffer, &type]() { type.destruct(buffer); });
      }

      /* Pass output buffer to the procedure executor. */
      mf_params.add_uninitialized_single_output({type, buffer, 1});

      /* Create virtual array that can be used after the procedure has been executed below. */
      const int out_index = constant_field_indices[i];
      r_varrays[out_index] = GVArray::ForSingleRef(type, array_size, buffer);
    }

    procedure_executor.call(IndexRange(1), mf_params, mf_context);
  }

  /* Copy data to supplied destination arrays if necessary. In some cases the evaluation above
   * has written the computed data in the right place already. */
  if (!dst_varrays.is_empty()) {
    for (const int out_index : fields_to_evaluate.index_range()) {
      GVMutableArray dst_varray = get_dst_varray(out_index);
      if (!dst_varray) {
        /* Caller did not provide a destination for this output. */
        continue;
      }
      const GVArray &computed_varray = r_varrays[out_index];
      BLI_assert(computed_varray.type() == dst_varray.type());
      if (is_output_written_to_dst[out_index]) {
        /* The result has been written into the destination provided by the caller already. */
        continue;
      }
      /* Still have to copy over the data in the destination provided by the caller. */
      if (dst_varray.is_span()) {
        /* Materialize into a span. */
        computed_varray.materialize_to_uninitialized(mask, dst_varray.get_internal_span().data());
      }
      else {
        /* Slower materialize into a different structure. */
        const CPPType &type = computed_varray.type();
        BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
        for (const int i : mask) {
          computed_varray.get_to_uninitialized(i, buffer);
          dst_varray.set_by_relocate(i, buffer);
        }
      }
      r_varrays[out_index] = dst_varray;
    }
  }
  return r_varrays;
}

void evaluate_constant_field(const GField &field, void *r_value)
{
  if (field.node().depends_on_input()) {
    const CPPType &type = field.cpp_type();
    type.copy_construct(type.default_value(), r_value);
    return;
  }

  ResourceScope scope;
  FieldContext context;
  Vector<GVArray> varrays = evaluate_fields(scope, {field}, IndexRange(1), context);
  varrays[0].get_to_uninitialized(0, r_value);
}

GField make_field_constant_if_possible(GField field)
{
  if (field.node().depends_on_input()) {
    return field;
  }
  const CPPType &type = field.cpp_type();
  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
  evaluate_constant_field(field, buffer);
  GField new_field = make_constant_field(type, buffer);
  type.destruct(buffer);
  return new_field;
}

GField make_constant_field(const CPPType &type, const void *value)
{
  auto constant_node = std::make_shared<FieldConstant>(type, value);
  return GField{std::move(constant_node)};
}

GVArray FieldContext::get_varray_for_input(const FieldInput &field_input,
                                           IndexMask mask,
                                           ResourceScope &scope) const
{
  /* By default ask the field input to create the varray. Another field context might overwrite
   * the context here. */
  return field_input.get_varray_for_context(*this, mask, scope);
}

IndexFieldInput::IndexFieldInput() : FieldInput(CPPType::get<int>(), "Index")
{
  category_ = Category::Generated;
}

GVArray IndexFieldInput::get_index_varray(IndexMask mask)
{
  auto index_func = [](int i) { return i; };
  return VArray<int>::ForFunc(mask.min_array_size(), index_func);
}

GVArray IndexFieldInput::get_varray_for_context(const fn::FieldContext &UNUSED(context),
                                                IndexMask mask,
                                                ResourceScope &UNUSED(scope)) const
{
  /* TODO: Investigate a similar method to IndexRange::as_span() */
  return get_index_varray(mask);
}

uint64_t IndexFieldInput::hash() const
{
  /* Some random constant hash. */
  return 128736487678;
}

bool IndexFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  return dynamic_cast<const IndexFieldInput *>(&other) != nullptr;
}

/* --------------------------------------------------------------------
 * FieldOperation.
 */

FieldOperation::FieldOperation(std::shared_ptr<const MultiFunction> function,
                               Vector<GField> inputs)
    : FieldOperation(*function, std::move(inputs))
{
  owned_function_ = std::move(function);
}

/**
 * Returns the field inputs used by all the provided fields.
 * This tries to reuse an existing #FieldInputs whenever possible to avoid copying it.
 */
static std::shared_ptr<const FieldInputs> combine_field_inputs(Span<GField> fields)
{
  /* The #FieldInputs that we try to reuse if possible. */
  const std::shared_ptr<const FieldInputs> *field_inputs_candidate = nullptr;
  for (const GField &field : fields) {
    const std::shared_ptr<const FieldInputs> &field_inputs = field.node().field_inputs();
    /* Only try to reuse non-empty #FieldInputs. */
    if (field_inputs && !field_inputs->nodes.is_empty()) {
      if (field_inputs_candidate == nullptr) {
        field_inputs_candidate = &field_inputs;
      }
      else if ((*field_inputs_candidate)->nodes.size() < field_inputs->nodes.size()) {
        /* Always try to reuse the #FieldInputs that has the most nodes already. */
        field_inputs_candidate = &field_inputs;
      }
    }
  }
  if (field_inputs_candidate == nullptr) {
    /* None of the field depends on an input. */
    return {};
  }
  /* Check if all inputs are in the candidate. */
  Vector<const FieldInput *> inputs_not_in_candidate;
  for (const GField &field : fields) {
    const std::shared_ptr<const FieldInputs> &field_inputs = field.node().field_inputs();
    if (!field_inputs) {
      continue;
    }
    if (&field_inputs == field_inputs_candidate) {
      continue;
    }
    for (const FieldInput *field_input : field_inputs->nodes) {
      if (!(*field_inputs_candidate)->nodes.contains(field_input)) {
        inputs_not_in_candidate.append(field_input);
      }
    }
  }
  if (inputs_not_in_candidate.is_empty()) {
    /* The existing #FieldInputs can be reused, because no other field has additional inputs. */
    return *field_inputs_candidate;
  }
  /* Create new #FieldInputs that contains all of the inputs that the fields depend on. */
  std::shared_ptr<FieldInputs> new_field_inputs = std::make_shared<FieldInputs>(
      **field_inputs_candidate);
  for (const FieldInput *field_input : inputs_not_in_candidate) {
    new_field_inputs->nodes.add(field_input);
    new_field_inputs->deduplicated_nodes.add(*field_input);
  }
  return new_field_inputs;
}

FieldOperation::FieldOperation(const MultiFunction &function, Vector<GField> inputs)
    : FieldNode(FieldNodeType::Operation), function_(&function), inputs_(std::move(inputs))
{
  field_inputs_ = combine_field_inputs(inputs_);
}

/* --------------------------------------------------------------------
 * FieldInput.
 */

FieldInput::FieldInput(const CPPType &type, std::string debug_name)
    : FieldNode(FieldNodeType::Input), type_(&type), debug_name_(std::move(debug_name))
{
  std::shared_ptr<FieldInputs> field_inputs = std::make_shared<FieldInputs>();
  field_inputs->nodes.add_new(this);
  field_inputs->deduplicated_nodes.add_new(*this);
  field_inputs_ = std::move(field_inputs);
}

/* --------------------------------------------------------------------
 * FieldConstant.
 */

FieldConstant::FieldConstant(const CPPType &type, const void *value)
    : FieldNode(FieldNodeType::Constant), type_(type)
{
  value_ = MEM_mallocN_aligned(type.size(), type.alignment(), __func__);
  type.copy_construct(value, value_);
}

FieldConstant::~FieldConstant()
{
  type_.destruct(value_);
  MEM_freeN(value_);
}

const CPPType &FieldConstant::output_cpp_type(int output_index) const
{
  BLI_assert(output_index == 0);
  UNUSED_VARS_NDEBUG(output_index);
  return type_;
}

const CPPType &FieldConstant::type() const
{
  return type_;
}

GPointer FieldConstant::value() const
{
  return {type_, value_};
}

/* --------------------------------------------------------------------
 * FieldEvaluator.
 */

static Vector<int64_t> indices_from_selection(IndexMask mask, const VArray<bool> &selection)
{
  /* If the selection is just a single value, it's best to avoid calling this
   * function when constructing an IndexMask and use an IndexRange instead. */
  BLI_assert(!selection.is_single());

  Vector<int64_t> indices;
  if (selection.is_span()) {
    Span<bool> span = selection.get_internal_span();
    for (const int64_t i : mask) {
      if (span[i]) {
        indices.append(i);
      }
    }
  }
  else {
    for (const int i : mask) {
      if (selection[i]) {
        indices.append(i);
      }
    }
  }
  return indices;
}

int FieldEvaluator::add_with_destination(GField field, GVMutableArray dst)
{
  const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
  dst_varrays_.append(dst);
  output_pointer_infos_.append({});
  return field_index;
}

int FieldEvaluator::add_with_destination(GField field, GMutableSpan dst)
{
  return this->add_with_destination(std::move(field), GVMutableArray::ForSpan(dst));
}

int FieldEvaluator::add(GField field, GVArray *varray_ptr)
{
  const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
  dst_varrays_.append(nullptr);
  output_pointer_infos_.append(OutputPointerInfo{
      varray_ptr, [](void *dst, const GVArray &varray, ResourceScope &UNUSED(scope)) {
        *(GVArray *)dst = varray;
      }});
  return field_index;
}

int FieldEvaluator::add(GField field)
{
  const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
  dst_varrays_.append(nullptr);
  output_pointer_infos_.append({});
  return field_index;
}

static IndexMask evaluate_selection(const Field<bool> &selection_field,
                                    const FieldContext &context,
                                    IndexMask full_mask,
                                    ResourceScope &scope)
{
  if (selection_field) {
    VArray<bool> selection =
        evaluate_fields(scope, {selection_field}, full_mask, context)[0].typed<bool>();
    if (selection.is_single()) {
      if (selection.get_internal_single()) {
        return full_mask;
      }
      return IndexRange(0);
    }
    return scope.add_value(indices_from_selection(full_mask, selection)).as_span();
  }
  return full_mask;
}

void FieldEvaluator::evaluate()
{
  BLI_assert_msg(!is_evaluated_, "Cannot evaluate fields twice.");

  selection_mask_ = evaluate_selection(selection_field_, context_, mask_, scope_);

  Array<GFieldRef> fields(fields_to_evaluate_.size());
  for (const int i : fields_to_evaluate_.index_range()) {
    fields[i] = fields_to_evaluate_[i];
  }
  evaluated_varrays_ = evaluate_fields(scope_, fields, selection_mask_, context_, dst_varrays_);
  BLI_assert(fields_to_evaluate_.size() == evaluated_varrays_.size());
  for (const int i : fields_to_evaluate_.index_range()) {
    OutputPointerInfo &info = output_pointer_infos_[i];
    if (info.dst != nullptr) {
      info.set(info.dst, evaluated_varrays_[i], scope_);
    }
  }
  is_evaluated_ = true;
}

IndexMask FieldEvaluator::get_evaluated_as_mask(const int field_index)
{
  VArray<bool> varray = this->get_evaluated(field_index).typed<bool>();

  if (varray.is_single()) {
    if (varray.get_internal_single()) {
      return IndexRange(varray.size());
    }
    return IndexRange(0);
  }

  return scope_.add_value(indices_from_selection(mask_, varray)).as_span();
}

IndexMask FieldEvaluator::get_evaluated_selection_as_mask()
{
  BLI_assert(is_evaluated_);
  return selection_mask_;
}

}  // namespace blender::fn

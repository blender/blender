/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_vector_set.hh"

#include "FN_field_evaluation.hh"
#include "FN_multi_function.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_procedure.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"
#include "FN_multi_function_procedure_optimization.hh"

namespace blender::fn {

/* -------------------------------------------------------------------- */
/** \name Field Evaluation
 * \{ */

struct FieldTreeInfo {
  FieldHashDeep deep_hashes;
  /**
   * When fields are built, they only have references to the fields that they depend on. This map
   * allows traversal of fields in the opposite direction. So for every field it stores the other
   * fields that depend on it directly.
   */
  MultiValueMap<UniqueHash, UniqueHash> field_users;
  /**
   * The same field input may exist in the field tree as separate nodes due to the way
   * the tree is constructed. This set contains every different input only once.
   */
  VectorSet<UniqueHash> deduplicated_input_hashes;
  Vector<GFieldRef> deduplicated_inputs;
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
    const GFieldRef &field = fields_to_check.pop();
    const GFieldRef::Variant &field_variant = field.variant();
    const UniqueHash hash = field_tree_info.deep_hashes.ensure(field);
    std::visit(
        [&]<typename T>(const T &v) {
          if constexpr (std::is_same_v<T, GFieldRef::Input>) {
            if (field_tree_info.deduplicated_input_hashes.add(hash)) {
              field_tree_info.deduplicated_inputs.append(field);
            }
          }
          else if constexpr (std::is_same_v<T, GFieldRef::MultiFn>) {
            for (const GField &input_field : v.node->inputs()) {
              const UniqueHash input_hash = field_tree_info.deep_hashes.lookup(input_field);
              field_tree_info.field_users.add(input_hash, hash);
              if (handled_fields.add(input_field)) {
                fields_to_check.push(input_field);
              }
            }
          }
          else if constexpr (std::is_same_v<T, GFieldRef::Value>) {
            /* Nothing to do. */
          }
          else {
            /* Ensure all cases handled. */
            static_assert(sizeof(T) == 0);
          }
        },
        field_variant);
  }
  return field_tree_info;
}

/**
 * Retrieves the data from the context that is passed as input into the field.
 */
static Vector<GVArray> get_field_context_inputs(ResourceScope &scope,
                                                const IndexMask &mask,
                                                const FieldContext &context,
                                                const Span<GFieldRef> field_inputs)
{
  Vector<GVArray> field_context_inputs;
  for (const GFieldRef &input_field : field_inputs) {
    const FieldInput &field_input = *std::get<GFieldRef::Input>(input_field.variant()).node;
    GVArray varray = context.get_varray_for_input(field_input, mask, scope);
    if (!varray) {
      const CPPType &type = field_input.cpp_type();
      varray = GVArray::from_single_default(type, mask.min_array_size());
    }
    field_context_inputs.append(std::move(varray));
  }
  return field_context_inputs;
}

/**
 * \return A set that contains all fields from the field tree that depend on an input that varies
 * for different indices.
 */
static Set<UniqueHash> find_varying_fields(const FieldTreeInfo &field_tree_info,
                                           const Span<GVArray> field_context_inputs)
{
  Set<UniqueHash> found_fields;
  Stack<UniqueHash> fields_to_check;

  /* The varying fields are the ones that depend on inputs that are not constant. Therefore we
   * start the tree search at the non-constant input fields and traverse through all fields that
   * depend on them. */
  for (const int input_i : field_tree_info.deduplicated_inputs.index_range()) {
    const GVArray &varray = field_context_inputs[input_i];
    if (varray.is_single()) {
      continue;
    }
    const UniqueHash &field = field_tree_info.deduplicated_input_hashes[input_i];
    for (const UniqueHash &user : field_tree_info.field_users.lookup(field)) {
      if (found_fields.add(user)) {
        fields_to_check.push(user);
      }
    }
  }
  while (!fields_to_check.is_empty()) {
    const UniqueHash &field = fields_to_check.pop();
    for (const UniqueHash &user : field_tree_info.field_users.lookup(field)) {
      if (found_fields.add(user)) {
        fields_to_check.push(user);
      }
    }
  }
  return found_fields;
}

/**
 * Builds the #procedure so that it computes the fields.
 */
static void build_multi_function_procedure_for_fields(mf::Procedure &procedure,
                                                      ResourceScope &scope,
                                                      const FieldTreeInfo &field_tree_info,
                                                      Span<GFieldRef> output_fields)
{
  mf::ProcedureBuilder builder{procedure};
  /* Every input, intermediate and output field corresponds to a variable in the procedure. */
  Map<UniqueHash, mf::Variable *> variable_by_field;

  /* Start by adding the field inputs as parameters to the procedure. */
  for (const GFieldRef &input_field : field_tree_info.deduplicated_inputs) {
    const UniqueHash input_hash = field_tree_info.deep_hashes.lookup(input_field);
    const FieldInput &field_input = *std::get<GFieldRef::Input>(input_field.variant()).node;
    mf::Variable &variable = builder.add_input_parameter(
        mf::DataType::ForSingle(field_input.cpp_type()), field_input.debug_name());
    variable_by_field.add_new(input_hash, &variable);
  }

  /* Utility struct that is used to do proper depth first search traversal of the tree below. */
  struct FieldWithIndex {
    GFieldRef field;
    int current_input_index = 0;
  };

  for (GFieldRef field : output_fields) {
    /* We start a new stack for each output field to make sure that a field pushed later to the
     * stack never depends on a field that was pushed before. */
    Stack<FieldWithIndex> fields_to_check;
    fields_to_check.push({field, 0});
    while (!fields_to_check.is_empty()) {
      FieldWithIndex &field_with_index = fields_to_check.peek();
      const GFieldRef &field = field_with_index.field;
      const UniqueHash field_hash = field_tree_info.deep_hashes.lookup(field);
      if (variable_by_field.contains(field_hash)) {
        /* The field has been handled already. */
        fields_to_check.pop();
        continue;
      }
      const GFieldRef::Variant &field_variant = field.variant();
      std::visit(
          [&]<typename T>(const T &v) {
            if constexpr (std::is_same_v<T, GFieldRef::Input>) {
              /* Variables for inputs are added above. */
            }
            else if constexpr (std::is_same_v<T, GFieldRef::MultiFn>) {
              const FieldOperation &field_multi_fn = *v.node;
              const Span<GField> fn_inputs = field_multi_fn.inputs();

              if (field_with_index.current_input_index < fn_inputs.size()) {
                /* Not all inputs are handled yet. Push the next input field to the stack and
                 * increment the input index. */
                fields_to_check.push({fn_inputs[field_with_index.current_input_index]});
                field_with_index.current_input_index++;
              }
              else {
                /* All inputs variables are ready, now gather all variables that are used by the
                 * function and call it. */
                const mf::MultiFunction &multi_function = field_multi_fn.multi_function();
                Array<mf::Variable *, 8> variables(multi_function.param_amount());

                int param_input_index = 0;
                int param_output_index = 0;
                for (const int param_index : multi_function.param_indices()) {
                  const mf::ParamType param_type = multi_function.param_type(param_index);
                  const mf::ParamType::InterfaceType interface_type = param_type.interface_type();
                  if (interface_type == mf::ParamType::Input) {
                    const GField &input_field = fn_inputs[param_input_index];
                    const UniqueHash input_hash = field_tree_info.deep_hashes.lookup(input_field);
                    variables[param_index] = variable_by_field.lookup(input_hash);
                    param_input_index++;
                  }
                  else if (interface_type == mf::ParamType::Output) {
                    const GFieldRef output_field{field_multi_fn, param_output_index};
                    /* NOTE: This abuses the deep hash cache as a set of the fields in the tree. At
                     * the cost of either hashing this output field or building a separate set of
                     * visisted GFieldRefs, we wouldn't have to use the cache in this way. */
                    if (!field_tree_info.deep_hashes.contains(output_field)) {
                      /* Ignored outputs don't need a variable. */
                      variables[param_index] = nullptr;
                    }
                    else {
                      /* Create a new variable for used outputs. */
                      mf::Variable &new_variable = procedure.new_variable(param_type.data_type());
                      variables[param_index] = &new_variable;
                      const UniqueHash output_hash = field_tree_info.deep_hashes.lookup(
                          output_field);
                      variable_by_field.add_new(output_hash, &new_variable);
                    }
                    param_output_index++;
                  }
                  else {
                    BLI_assert_unreachable();
                  }
                }
                builder.add_call_with_all_variables(multi_function, variables);
              }
            }
            else if constexpr (std::is_same_v<T, GFieldRef::Value>) {
              const mf::MultiFunction &fn =
                  procedure.construct_function<mf::CustomMF_GenericConstant>(
                      *v.type, v.value, false);
              mf::Variable &new_variable = *builder.add_call<1>(fn)[0];
              variable_by_field.add_new(field_hash, &new_variable);
            }
            else {
              /* Ensure all cases handled. */
              static_assert(sizeof(T) == 0);
            }
          },
          field_variant);
    }
  }

  /* Add output parameters to the procedure. */
  Set<mf::Variable *> output_variables;
  for (const GFieldRef &field : output_fields) {
    const UniqueHash field_hash = field_tree_info.deep_hashes.lookup(field);
    mf::Variable *variable = variable_by_field.lookup(field_hash);
    if (!output_variables.add(variable)) {
      /* One variable can be output at most once. To output the same value twice, we have to make
       * a copy first. */
      const mf::MultiFunction &copy_fn = scope.construct<mf::CustomMF_GenericCopy>(
          variable->data_type());
      variable = builder.add_call<1>(copy_fn, {variable})[0];
      output_variables.add(variable);
    }
    builder.add_output_parameter(*variable);
  }

  for (mf::Variable *variable : procedure.variables()) {
    if (!output_variables.contains(variable)) {
      builder.add_destruct(*variable);
    }
  }

  mf::ReturnInstruction &return_instr = builder.add_return();

  mf::procedure_optimization::move_destructs_up(procedure, return_instr);

  procedure.prepare_for_execution();

  // std::cout << procedure.to_dot() << "\n";
  BLI_assert(procedure.validate());
}

Vector<GVArray> evaluate_fields(ResourceScope &scope,
                                Span<GFieldRef> fields_to_evaluate,
                                const IndexMask &mask,
                                const FieldContext &context,
                                Span<GVMutableArray> dst_varrays)
{
  Vector<GVArray> varrays(fields_to_evaluate.size());
  Array<bool> is_output_written_to_dst(fields_to_evaluate.size(), false);
  const int array_size = mask.min_array_size();

  if (mask.is_empty()) {
    for (const int i : fields_to_evaluate.index_range()) {
      const CPPType &type = fields_to_evaluate[i].cpp_type();
      varrays[i] = GVArray::from_empty(type);
    }
    return varrays;
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
      scope, mask, context, field_tree_info.deduplicated_inputs);

  Set<UniqueHash> varying_fields = find_varying_fields(field_tree_info, field_context_inputs);

  /* Process fields that can output a VArray directly, and separate the rest of the fields into
   * two categories: those that are constant and need to be evaluated only once, and those that
   * need to be evaluated for every index. */
  Vector<GFieldRef> varying_fields_to_evaluate;
  Vector<int> varying_field_indices;
  Vector<GFieldRef> constant_fields_to_evaluate;
  Vector<int> constant_field_indices;
  for (const int out_index : fields_to_evaluate.index_range()) {
    const GFieldRef &field = fields_to_evaluate[out_index];
    const GFieldRef::Variant &field_variant = field.variant();
    std::visit(
        [&]<typename T>(const T &v) {
          if constexpr (std::is_same_v<T, GFieldRef::Input>) {
            const UniqueHash hash = field_tree_info.deep_hashes.lookup(field);
            const int input_i = field_tree_info.deduplicated_input_hashes.index_of(hash);
            const GVArray &varray = field_context_inputs[input_i];
            varrays[out_index] = varray;
          }
          else if constexpr (std::is_same_v<T, GFieldRef::MultiFn>) {
            const UniqueHash hash = field_tree_info.deep_hashes.lookup(field);
            if (varying_fields.contains(hash)) {
              varying_fields_to_evaluate.append(field);
              varying_field_indices.append(out_index);
            }
            else {
              constant_fields_to_evaluate.append(field);
              constant_field_indices.append(out_index);
            }
          }
          else if constexpr (std::is_same_v<T, GFieldRef::Value>) {
            varrays[out_index] = GVArray::from_single_ref(*v.type, mask.min_array_size(), v.value);
          }
          else {
            /* Ensure all cases handled. */
            static_assert(sizeof(T) == 0);
          }
        },
        field_variant);
  }

  /* Evaluate varying fields if necessary. */
  if (!varying_fields_to_evaluate.is_empty()) {
    /* Build the procedure for those fields. */
    mf::Procedure procedure;
    build_multi_function_procedure_for_fields(
        procedure, scope, field_tree_info, varying_fields_to_evaluate);
    mf::ProcedureExecutor procedure_executor{procedure};

    mf::ParamsBuilder mf_params{procedure_executor, &mask};
    mf::ContextBuilder mf_context;

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
        buffer = scope.allocator().allocate_array(type, array_size);

        if (!type.is_trivially_destructible) {
          /* Destruct values in the end. */
          scope.add_destruct_call(
              [buffer, mask, &type]() { type.destruct_indices(buffer, mask); });
        }

        varrays[out_index] = GVArray::from_span({type, buffer, array_size});
      }
      else {
        /* Write the result into the existing span. */
        buffer = dst_varray.get_internal_span().data();

        varrays[out_index] = dst_varray;
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
    mf::Procedure procedure;
    build_multi_function_procedure_for_fields(
        procedure, scope, field_tree_info, constant_fields_to_evaluate);
    mf::ProcedureExecutor procedure_executor{procedure};
    const IndexMask mask(1);
    mf::ParamsBuilder mf_params{procedure_executor, &mask};
    mf::ContextBuilder mf_context;

    /* Provide inputs to the procedure executor. */
    for (const GVArray &varray : field_context_inputs) {
      mf_params.add_readonly_single_input(varray);
    }

    for (const int i : constant_fields_to_evaluate.index_range()) {
      const GFieldRef &field = constant_fields_to_evaluate[i];
      const CPPType &type = field.cpp_type();
      /* Allocate memory where the computed value will be stored in. */
      void *buffer = scope.allocate_owned(type);

      /* Pass output buffer to the procedure executor. */
      mf_params.add_uninitialized_single_output({type, buffer, 1});

      /* Create virtual array that can be used after the procedure has been executed below. */
      const int out_index = constant_field_indices[i];
      varrays[out_index] = GVArray::from_single_ref(type, array_size, buffer);
    }

    procedure_executor.call(mask, mf_params, mf_context);
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
      const GVArray &computed_varray = varrays[out_index];
      BLI_assert(computed_varray.type() == dst_varray.type());
      if (is_output_written_to_dst[out_index]) {
        /* The result has been written into the destination provided by the caller already. */
        continue;
      }
      /* Still have to copy over the data in the destination provided by the caller. */
      if (dst_varray.is_span()) {
        array_utils::copy(computed_varray,
                          mask,
                          dst_varray.get_internal_span().take_front(mask.min_array_size()));
      }
      else {
        /* Slower materialize into a different structure. */
        const CPPType &type = computed_varray.type();
        threading::parallel_for(mask.index_range(), 2048, [&](const IndexRange range) {
          BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
          mask.slice(range).foreach_segment([&](auto segment) {
            for (const int i : segment) {
              computed_varray.get_to_uninitialized(i, buffer);
              dst_varray.set_by_relocate(i, buffer);
            }
          });
        });
      }
      varrays[out_index] = dst_varray;
    }
  }
  return varrays;
}

void evaluate_constant_field(const GField &field, void *r_value)
{
  if (field.depends_on_input()) {
    const CPPType &type = field.cpp_type();
    type.value_initialize(r_value);
    return;
  }

  AlignedBuffer<512, 64> local_buffer;
  ResourceScope scope(local_buffer);
  FieldContext context;
  Vector<GVArray> varrays = evaluate_fields(scope, {field}, IndexRange(1), context);
  varrays[0].get_to_uninitialized(0, r_value);
}

GField make_field_constant_if_possible(GField field)
{
  if (field.depends_on_input()) {
    return field;
  }
  const CPPType &type = field.cpp_type();
  BUFFER_FOR_CPP_TYPE_VALUE(type, buffer);
  evaluate_constant_field(field, buffer);
  GField new_field = GField::from_constant(type, buffer);
  type.destruct(buffer);
  return new_field;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldEvaluator
 * \{ */

static IndexMask index_mask_from_selection(const IndexMask full_mask,
                                           const VArray<bool> &selection,
                                           ResourceScope &scope)
{
  return IndexMask::from_bools(full_mask, selection, scope.construct<IndexMaskMemory>());
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
  return this->add_with_destination(std::move(field), GVMutableArray::from_span(dst));
}

int FieldEvaluator::add(GField field, GVArray *varray_ptr)
{
  const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
  dst_varrays_.append(nullptr);
  output_pointer_infos_.append(OutputPointerInfo{
      varray_ptr, [](void *dst, const GVArray &varray, ResourceScope & /*scope*/) {
        *static_cast<GVArray *>(dst) = varray;
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
                                    const IndexMask &full_mask,
                                    ResourceScope &scope)
{
  VArray<bool> selection =
      evaluate_fields(scope, {selection_field}, full_mask, context)[0].typed<bool>();
  return index_mask_from_selection(full_mask, selection, scope);
}

void FieldEvaluator::evaluate()
{
  BLI_assert_msg(!is_evaluated_, "Cannot evaluate fields twice.");

  selection_mask_ = selection_field_ ?
                        evaluate_selection(*selection_field_, context_, mask_, scope_) :
                        mask_;

  Vector<GFieldRef> fields;
  fields.reserve(fields_to_evaluate_.size());
  for (const int i : fields_to_evaluate_.index_range()) {
    fields.append(fields_to_evaluate_[i]);
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
  return index_mask_from_selection(mask_, varray, scope_);
}

IndexMask FieldEvaluator::get_evaluated_selection_as_mask() const
{
  BLI_assert(is_evaluated_);
  return selection_mask_;
}

/** \} */

}  // namespace blender::fn

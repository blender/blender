/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_list.hh"

#include "list_function_eval.hh"

namespace blender::nodes {

class ListFieldContext : public FieldContext {
 public:
  ListFieldContext() = default;

  GVArray get_varray_for_input(const FieldInput &field_input,
                               const IndexMask &mask,
                               ResourceScope & /*scope*/) const override
  {
    const bke::IDAttributeFieldInput *id_field_input =
        dynamic_cast<const bke::IDAttributeFieldInput *>(&field_input);

    const fn::IndexFieldInput *index_field_input = dynamic_cast<const fn::IndexFieldInput *>(
        &field_input);

    if (id_field_input == nullptr && index_field_input == nullptr) {
      return {};
    }

    return fn::IndexFieldInput::get_index_varray(mask);
  }
};

ListPtr evaluate_field_to_list(GField field, const int64_t count)
{
  const CPPType &cpp_type = field.cpp_type();
  List::ArrayData array_data = List::ArrayData::ForConstructed(cpp_type, count);
  GMutableSpan span(cpp_type, array_data.data, count);

  ListFieldContext context{};
  fn::FieldEvaluator evaluator{context, count};
  evaluator.add_with_destination(std::move(field), span);
  evaluator.evaluate();

  return List::create(cpp_type, std::move(array_data), count);
}

static ListPtr create_repeated_list(ListPtr list, const int64_t dst_size)
{
  if (list->size() >= dst_size) {
    return list;
  }
  if (const auto *data = std::get_if<nodes::List::ArrayData>(&list->data())) {
    const int64_t size = list->size();
    BLI_assert(size > 0);
    const CPPType &cpp_type = list->cpp_type();
    List::ArrayData new_data = List::ArrayData::ForUninitialized(cpp_type, dst_size);
    const int64_t chunks = dst_size / size;
    for (const int64_t i : IndexRange(chunks)) {
      const int64_t offset = cpp_type.size * i * size;
      cpp_type.copy_construct_n(data->data, POINTER_OFFSET(new_data.data, offset), size);
    }
    const int64_t last_chunk_size = dst_size % size;
    if (last_chunk_size > 0) {
      const int64_t offset = cpp_type.size * chunks * size;
      cpp_type.copy_construct_n(
          data->data, POINTER_OFFSET(new_data.data, offset), last_chunk_size);
    }

    return List::create(cpp_type, std::move(new_data), dst_size);
  }
  if (const auto *data = std::get_if<nodes::List::SingleData>(&list->data())) {
    const CPPType &cpp_type = list->cpp_type();
    return List::create(cpp_type, *data, dst_size);
  }
  BLI_assert_unreachable();
  return {};
}

static void add_list_to_params(mf::ParamsBuilder &params,
                               const mf::ParamType &param_type,
                               const List &list)
{
  const CPPType &cpp_type = param_type.data_type().single_type();
  BLI_assert(cpp_type == list.cpp_type());
  if (const auto *array_data = std::get_if<nodes::List::ArrayData>(&list.data())) {
    params.add_readonly_single_input(GSpan(cpp_type, array_data->data, list.size()));
  }
  else if (const auto *single_data = std::get_if<nodes::List::SingleData>(&list.data())) {
    params.add_readonly_single_input(GPointer(cpp_type, single_data->value));
  }
}

void execute_multi_function_on_value_variant__list(const MultiFunction &fn,
                                                   const Span<SocketValueVariant *> input_values,
                                                   const Span<SocketValueVariant *> output_values,
                                                   GeoNodesUserData *user_data)
{
  int64_t max_size = 0;
  for (const int i : input_values.index_range()) {
    SocketValueVariant &input_variant = *input_values[i];
    if (input_variant.is_list()) {
      if (ListPtr list = input_variant.get<ListPtr>()) {
        max_size = std::max(max_size, list->size());
      }
    }
  }

  const IndexMask mask(max_size);
  mf::ParamsBuilder params{fn, &mask};
  mf::ContextBuilder context;
  context.user_data(user_data);

  Array<ListPtr, 8> input_lists(input_values.size());
  for (const int i : input_values.index_range()) {
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &cpp_type = param_type.data_type().single_type();
    SocketValueVariant &input_variant = *input_values[i];
    if (input_variant.is_single()) {
      const void *value = input_variant.get_single_ptr_raw();
      params.add_readonly_single_input(GPointer(cpp_type, value));
    }
    else if (input_variant.is_list()) {
      ListPtr list_ptr = input_variant.get<ListPtr>();
      if (!list_ptr || list_ptr->size() == 0) {
        params.add_readonly_single_input(GPointer(cpp_type, cpp_type.default_value()));
        continue;
      }
      input_lists[i] = create_repeated_list(std::move(list_ptr), max_size);
      add_list_to_params(params, param_type, *input_lists[i]);
    }
    else if (input_variant.is_context_dependent_field()) {
      fn::GField field = input_variant.extract<fn::GField>();
      input_lists[i] = evaluate_field_to_list(std::move(field), max_size);
      add_list_to_params(params, param_type, *input_lists[i]);
    }
    else {
      /* This function should not be called when there are other types like grids in the inputs. */
      BLI_assert_unreachable();
      params.add_readonly_single_input(GPointer(cpp_type, cpp_type.default_value()));
    }
  }
  for (const int i : output_values.index_range()) {
    if (output_values[i] == nullptr) {
      params.add_ignored_single_output("");
      continue;
    }
    SocketValueVariant &output_variant = *output_values[i];
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &cpp_type = param_type.data_type().single_type();
    List::ArrayData array_data = List::ArrayData::ForUninitialized(cpp_type, max_size);

    params.add_uninitialized_single_output(GMutableSpan(cpp_type, array_data.data, max_size));
    output_variant.set(List::create(cpp_type, std::move(array_data), max_size));
  }
  fn.call(mask, params, context);
}

}  // namespace blender::nodes

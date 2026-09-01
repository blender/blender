/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "NOD_geometry_exec.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_geometry_nodes_list.hh"
#include "NOD_geometry_nodes_values.hh"

#include "list_function_eval.hh"

namespace blender::nodes {

GVArray ListFieldContext::get_varray_for_input(const FieldInput &field_input,
                                               const IndexMask &mask,
                                               ResourceScope & /*scope*/) const
{
  const auto *id_field_input = dynamic_cast<const bke::IDAttributeFieldInput *>(&field_input);

  const auto *index_field_input = dynamic_cast<const fn::IndexFieldInput *>(&field_input);

  if (id_field_input == nullptr && index_field_input == nullptr) {
    return {};
  }

  return fn::IndexFieldInput::get_index_varray(mask);
}

GListPtr evaluate_field_to_list(GField field, const int64_t count)
{
  const CPPType &cpp_type = field.cpp_type();
  GArray array(cpp_type, count);

  ListFieldContext context{};
  fn::FieldEvaluator evaluator{context, count};
  evaluator.add_with_destination(std::move(field), array);
  evaluator.evaluate();

  return GList::from_garray(std::move(array));
}

SampleIndexFunction::SampleIndexFunction(GListPtr list) : list_(std::move(list))
{
  mf::SignatureBuilder builder{"Sample Index", signature_};
  builder.single_input<int>("Index");
  builder.single_output("Value", list_->cpp_type());
  this->set_signature(&signature_);
}

void SampleIndexFunction::call(const IndexMask &mask,
                               mf::Params params,
                               mf::Context /*context*/) const
{
  const VArraySpan<int> indices = params.readonly_single_input<int>(0, "Index");
  GMutableSpan dst = params.uninitialized_single_output(1, "Value");

  IndexMaskMemory memory;
  const IndexMask valid_indices = array_utils::indices_in_range(
      mask, indices, IndexRange(list_->size()), memory);

  if (valid_indices.size() != mask.size()) {
    const IndexMask invalid_indices = valid_indices.complement(mask, memory);
    list_->cpp_type().fill_construct_indices(
        list_->cpp_type().default_value(), dst.data(), invalid_indices);
  }

  const GList::DataVariant &data = list_->data();
  if (const auto *array_data = std::get_if<nodes::GList::ArrayData>(&data)) {
    const GSpan src(list_->cpp_type(), array_data->data, list_->size());
    valid_indices.foreach_index(
        [&](const int i) { list_->cpp_type().copy_construct(src[indices[i]], dst[i]); });
  }
  else if (const auto *single_data = std::get_if<nodes::GList::SingleData>(&data)) {
    list_->cpp_type().fill_construct_indices(single_data->value, dst.data(), valid_indices);
  }
}

void SampleIndexFunction::hash_unique(UniqueHashBytes &hash) const
{
  static constexpr int8_t id = 0;
  hash.add(&id);
  hash.add(list_.get());
}

static GListPtr create_repeated_list(GListPtr list, const int64_t dst_size)
{
  if (list->size() >= dst_size) {
    return list;
  }
  if (const auto *data = std::get_if<nodes::GList::ArrayData>(&list->data())) {
    const int64_t size = list->size();
    BLI_assert(size > 0);
    const CPPType &cpp_type = list->cpp_type();
    GArray new_data(cpp_type, dst_size, NoInitialization{});
    const int64_t chunks = dst_size / size;
    for (const int64_t i : IndexRange(chunks)) {
      cpp_type.copy_construct_n(data->data, new_data[i * size], size);
    }
    const int64_t last_chunk_size = dst_size % size;
    if (last_chunk_size > 0) {
      cpp_type.copy_construct_n(data->data, new_data[chunks * size], last_chunk_size);
    }

    return GList::from_garray(std::move(new_data));
  }
  if (const auto *data = std::get_if<nodes::GList::SingleData>(&list->data())) {
    const CPPType &cpp_type = list->cpp_type();
    return GList::create(cpp_type, *data, dst_size);
  }
  BLI_assert_unreachable();
  return {};
}

static void add_list_to_params(mf::ParamsBuilder &params,
                               const mf::ParamType &param_type,
                               const GList &list)
{
  const CPPType &cpp_type = param_type.data_type().single_type();
  BLI_assert(cpp_type == list.cpp_type());
  if (const auto *array_data = std::get_if<nodes::GList::ArrayData>(&list.data())) {
    params.add_readonly_single_input(GSpan(cpp_type, array_data->data, list.size()));
  }
  else if (const auto *single_data = std::get_if<nodes::GList::SingleData>(&list.data())) {
    params.add_readonly_single_input(GPointer(cpp_type, single_data->value));
  }
}

[[nodiscard]] static bool execute_multi_function_on_value_variant__list_individual(
    const int64_t output_size,
    const MultiFunction &fn,
    const std::shared_ptr<MultiFunction> &owned_fn,
    const Span<SocketValueVariant *> input_values,
    const Span<SocketValueVariant *> output_values,
    GeoNodesUserData *user_data,
    std::string &r_error_message)
{
  /* Prepare output arrays. */
  Array<Array<SocketValueVariant>, 8> output_lists(output_values.size());
  for (const int output_i : output_values.index_range()) {
    output_lists[output_i].reinitialize(output_size);
  }
  /* Evaluate field inputs. */
  Array<GListPtr, 8> fields_as_lists(input_values.size());
  for (const int input_i : input_values.index_range()) {
    const SocketValueVariant &input_variant = *input_values[input_i];
    if (input_variant.is_context_dependent_field()) {
      fields_as_lists[input_i] = evaluate_field_to_list(input_variant.get<fn::GField>(),
                                                        output_size);
    }
  }

  std::atomic<bool> error_occurred = false;
  Mutex error_mutex;

  /* Evaluate each list index individually, potentially in parallel. */
  threading::parallel_for(IndexRange(output_size), 64, [&](const IndexRange range) {
    Array<SocketValueVariant, 8> inputs(input_values.size());
    Array<SocketValueVariant *, 8> input_ptrs(input_values.size());
    for (const int input_i : input_values.index_range()) {
      input_ptrs[input_i] = &inputs[input_i];
    }
    for (const int iter_i : range) {
      if (error_occurred.load()) {
        return;
      }
      /* Prepare inputs. */
      for (const int input_i : input_values.index_range()) {
        const mf::ParamType param_type = fn.param_type(input_i);
        const CPPType &cpp_type = param_type.data_type().single_type();
        const SocketValueVariant &input_variant = *input_values[input_i];
        const eNodeSocketDatatype socket_type =
            bke::geo_nodes_base_cpp_type_to_socket_type(cpp_type).value();
        SocketValueVariant &elem_input = inputs[input_i];

        auto handle_input_list = [&](const GListPtr &list) {
          if (list) {
            if (list->size() == 0) {
              elem_input.store_single(socket_type, cpp_type.default_value());
            }
            else if (list->cpp_type().is<SocketValueVariant>()) {
              elem_input = list->typed<SocketValueVariant>().varray()[iter_i % list->size()];
            }
            else if (list->cpp_type() == cpp_type) {
              void *ptr = elem_input.allocate_single(socket_type);
              list->varray().get_to_uninitialized(iter_i % list->size(), ptr);
            }
            else {
              elem_input.store_single(socket_type, cpp_type.default_value());
              BLI_assert_unreachable();
            }
          }
          else {
            elem_input.store_single(socket_type, cpp_type.default_value());
          }
        };

        if (input_variant.is_list()) {
          handle_input_list(input_variant.get<GListPtr>());
        }
        else if (input_variant.is_context_dependent_field()) {
          handle_input_list(fields_as_lists[input_i]);
        }
        else if (input_variant.is_single()) {
          elem_input = input_variant;
        }
        else {
          BLI_assert_unreachable();
        }
      }
      Array<SocketValueVariant *, 8> output_ptrs(output_values.size());
      for (const int output_i : output_values.index_range()) {
        if (output_values[output_i]) {
          output_ptrs[output_i] = &output_lists[output_i][iter_i];
        }
        else {
          /* This output is ignored. */
          output_ptrs[output_i] = nullptr;
        }
      }
      std::string sub_error_message;
      if (!execute_multi_function_on_value_variant(
              fn, owned_fn, input_ptrs, output_ptrs, user_data, sub_error_message))
      {
        std::lock_guard lock{error_mutex};
        error_occurred.store(true);
        r_error_message = sub_error_message;
        return;
      }
    }
  });
  if (error_occurred.load()) {
    return false;
  }
  for (const int output_i : output_values.index_range()) {
    if (output_values[output_i]) {
      output_values[output_i]->set(GList::from_container(std::move(output_lists[output_i])));
    }
  }
  return true;
}

[[nodiscard]] bool execute_multi_function_on_value_variant__list(
    const MultiFunction &fn,
    const std::shared_ptr<MultiFunction> &owned_fn,
    const Span<SocketValueVariant *> input_values,
    const Span<SocketValueVariant *> output_values,
    GeoNodesUserData *user_data,
    std::string &r_error_message)
{
  int64_t max_size = 0;
  bool evaluate_individual = false;
  for (const int i : input_values.index_range()) {
    SocketValueVariant &input_variant = *input_values[i];
    const mf::ParamType param_type = fn.param_type(i);
    const CPPType &cpp_type = param_type.data_type().single_type();
    if (input_variant.is_list()) {
      if (GListPtr list = input_variant.get<GListPtr>()) {
        max_size = std::max(max_size, list->size());
        if (list->cpp_type() != cpp_type) {
          evaluate_individual = true;
        }
      }
    }
  }

  if (evaluate_individual) {
    /* Can't use a single MultiFunction evaluation, because a list contains more complex types. */
    return execute_multi_function_on_value_variant__list_individual(
        max_size, fn, owned_fn, input_values, output_values, user_data, r_error_message);
  }

  const IndexMask mask(max_size);
  mf::ParamsBuilder params{fn, &mask};
  mf::ContextBuilder context;
  context.user_data(user_data);

  Array<GListPtr, 8> input_lists(input_values.size());
  for (const int i : input_values.index_range()) {
    const mf::ParamType param_type = fn.param_type(params.next_param_index());
    const CPPType &cpp_type = param_type.data_type().single_type();
    SocketValueVariant &input_variant = *input_values[i];
    if (input_variant.is_single()) {
      const void *value = input_variant.get_single_ptr_raw();
      params.add_readonly_single_input(GPointer(cpp_type, value));
    }
    else if (input_variant.is_list()) {
      GListPtr list_ptr = input_variant.get<GListPtr>();
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
    GArray array(cpp_type, max_size, NoInitialization{});

    params.add_uninitialized_single_output(GMutableSpan(cpp_type, array.data(), max_size));
    output_variant.set(GList::from_garray(std::move(array)));
  }
  fn.call(mask, params, context);
  return true;
}

}  // namespace blender::nodes

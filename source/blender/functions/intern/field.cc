/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_set.hh"
#include "BLI_stack.hh"

#include "FN_field.hh"
#include "FN_multi_function_registry.hh"

#include <xxhash.h>

namespace blender::fn {

FieldInput::FieldInput(const CPPType &type, std::string debug_name)
    : type_(&type), debug_name_(std::move(debug_name))
{
}

GField GField::from_constant(const CPPType &type, const void *value)
{
  if (TrivialInlineConstant::cpp_type_supported(type)) {
    TrivialInlineConstant constant;
    constant.type = &type;
    type.copy_construct(value, constant.value.ptr());
    return GField(constant);
  }
  void *new_value = MEM_new_uninitialized_aligned(type.size, type.alignment, __func__);
  type.copy_construct(value, new_value);
  return GField(OwnedConstant{&type, new_value});
}

bool operator==(const GField &a, const GField &b)
{
  const GField &a_ref = a.deref_field_ref();
  const GField &b_ref = b.deref_field_ref();

  return std::visit(
      [&]<typename T>(const T &v_a) -> bool {
        if constexpr (std::is_same_v<T, GField::Input>) {
          if (const auto *v_b = std::get_if<GField::Input>(&b_ref.variant_)) {
            return v_a.node == v_b->node;
          }
          return false;
        }
        else if constexpr (std::is_same_v<T, GField::MultiFn>) {
          if (const auto *v_b = std::get_if<GField::MultiFn>(&b_ref.variant_)) {
            return v_a.node == v_b->node && v_a.output_i == v_b->output_i;
          }
          return false;
        }
        else if constexpr (std::is_same_v<T, GField::FieldRef>) {
          /* Should not exist due to #deref_field_ref above. */
          BLI_assert_unreachable();
          return false;
        }
        else if constexpr (GField::is_constant_value_v<T>) {
          const CPPType &type_a = *v_a.type;
          const void *constant_a = v_a.value;
          return std::visit(
              [&]<typename U>(const U &v_b) -> bool {
                if constexpr (GField::is_constant_value_v<U>) {
                  const CPPType &type_b = *v_b.type;
                  if (type_a != type_b) {
                    return false;
                  }
                  const void *constant_b = v_b.value;
                  return type_a.is_equal_or_false(constant_a, constant_b);
                }
                else {
                  return false;
                }
              },
              b_ref.variant_);
        }
        return false;
      },
      a_ref.variant_);
}

uint64_t GField::hash() const
{
  const GField &ref = this->deref_field_ref();
  return std::visit(
      [&]<typename T>(const T &v) -> uint64_t {
        if constexpr (std::is_same_v<T, Input>) {
          return get_default_hash(v.node);
        }
        else if constexpr (std::is_same_v<T, MultiFn>) {
          return get_default_hash(v.node, v.output_i);
        }
        else if constexpr (std::is_same_v<T, FieldRef>) {
          /* Should not exist due to #deref_field_ref above. */
          BLI_assert_unreachable();
          return 0;
        }
        else if constexpr (is_constant_value_v<T>) {
          return v.type->hash_or_fallback(v.value, uint64_t(v.type));
        }
      },
      ref.variant_);
}

UniqueHash FieldHashDeep::ensure(const GFieldRef &field)
{
  if (const UniqueHash *cached = cache.lookup_ptr(field)) {
    return *cached;
  }

  /* With a post-order DFS traversal, push each node twice. On the first pop (not yet in
   * `visited`), push a field's children. On the second pop (already in `visited`), all children
   * will be in `cache`, so compute and store the hash. Checking the cache for a hash avoids
   * duplicate work when the same sub-field is reached via multiple paths (e.g. diamond-shaped
   * graphs). */
  Set<GFieldRef, 8> visited;
  Stack<GFieldRef, 16> stack;
  stack.push(field);
  while (!stack.is_empty()) {
    GFieldRef current = stack.pop();
    if (cache.contains(current)) {
      continue;
    }
    if (visited.contains(current)) {
      UniqueHashBytes hash_context;
      std::visit(
          [&]<typename T>(const T &v) {
            if constexpr (std::is_same_v<T, GFieldRef::Value>) {
              v.type->hash_unique(v.value, hash_context);
              hash_context.add(v.type);
            }
            else if constexpr (std::is_same_v<T, GFieldRef::Input>) {
              v.node->hash_unique(hash_context, *this);
            }
            else if constexpr (std::is_same_v<T, GFieldRef::MultiFn>) {
              v.node->multi_function().hash_unique(hash_context);
              hash_context.add(v.output_i);
              for (const GField &input_field : v.node->inputs()) {
                hash_context.add(cache.lookup(input_field));
              }
            }
          },
          current.variant());
      const Span bytes = hash_context.data.as_span();
      UniqueHash hash;
      const XXH128_hash_t xxhash = XXH3_128bits(bytes.data(), bytes.size());
      static_assert(sizeof(UniqueHash) == sizeof(xxhash));
      memcpy(static_cast<void *>(&hash), &xxhash, sizeof(xxhash));
      cache.add_new(current, hash);
      continue;
    }
    visited.add(current);
    stack.push(current);
    if (const auto *multi_fn = std::get_if<GFieldRef::MultiFn>(&current.variant())) {
      for (const GField &input : multi_fn->node->inputs()) {
        stack.push(input);
      }
    }
  }

  return cache.lookup(field);
}

const FieldInputsPtr &FieldInput::field_inputs() const
{
  field_inputs_mutex_.ensure([&]() {
    FieldInputs *inputs = MEM_new<FieldInputs>(__func__);
    inputs->inputs.add(*this);
    field_inputs_ = FieldInputsPtr(inputs);
  });
  return field_inputs_;
}

uint64_t FieldInput::hash() const
{
  UniqueHashBytes hash_context;
  FieldHashDeep deep_hash_cache;
  this->hash_unique(hash_context, deep_hash_cache);
  return get_default_hash(hash_context.data);
}

FieldInput::~FieldInput() = default;

void FieldInput::foreach_recursive_field(FunctionRef<void(const GField &)> /*fn*/) const {}

void FieldInput::hash_unique(UniqueHashBytes &hash, FieldHashDeep & /*deep_hash_cache*/) const
{
  hash.add(this);
}

void FieldInput::delete_self()
{
  MEM_delete(this);
}

void FieldOperation::delete_self()
{
  MEM_delete(this);
}

void FieldInputs::delete_self()
{
  MEM_delete(this);
}

FieldOperationPtr FieldOperation::from(std::shared_ptr<const mf::MultiFunction> fn,
                                       Vector<GField> inputs)
{
  return FieldOperationPtr(MEM_new<FieldOperation>(__func__, std::move(fn), std::move(inputs)));
}

FieldOperationPtr FieldOperation::from(const mf::MultiFunction &fn, Vector<GField> inputs)
{
  return FieldOperationPtr(MEM_new<FieldOperation>(__func__, fn, std::move(inputs)));
}

/**
 * Combine the field inputs from multiple fields. If possible, nothing new is allocated.
 */
static FieldInputsPtr combine_field_inputs(const Span<GField> &fields)
{
  /* Try to find an existing #FieldInputsPtr that covers all given fields. */
  bool candidate_valid = true;
  const FieldInputsPtr *candidate = nullptr;
  for (const GField &field : fields) {
    const FieldInputsPtr &field_inputs_ptr = field.field_inputs();
    if (!field_inputs_ptr) {
      continue;
    }
    if (!candidate) {
      candidate = &field_inputs_ptr;
      continue;
    }
    if (field_inputs_ptr == *candidate) {
      continue;
    }
    const FieldInputsPtr *smaller_candidate = candidate;
    const FieldInputsPtr *larger_candidate = &field_inputs_ptr;
    if ((*smaller_candidate)->inputs.size() > (*larger_candidate)->inputs.size()) {
      std::swap(smaller_candidate, larger_candidate);
    }
    /* Check if the smaller candidate is fully contained in the larger one. */
    for (const FieldInput &field_input : (*smaller_candidate)->inputs) {
      if (!(*larger_candidate)->inputs.contains(field_input)) {
        candidate_valid = false;
        break;
      }
    }
    if (!candidate_valid) {
      break;
    }
    candidate = larger_candidate;
  }
  if (candidate_valid) {
    if (candidate) {
      return *candidate;
    }
    return {};
  }
  /* None of the existing #FieldInputs can be reused, create a new #FieldInputs and add all the
   * inputs to it. */
  FieldInputs *new_field_inputs = MEM_new<FieldInputs>(__func__);
  for (const GField &field : fields) {
    const FieldInputsPtr &field_inputs_ptr = field.field_inputs();
    if (!field_inputs_ptr) {
      continue;
    }
    for (const FieldInput &field_input : field_inputs_ptr->inputs) {
      new_field_inputs->inputs.add(field_input);
    }
  }
  return FieldInputsPtr(new_field_inputs);
}

GField::GField(const GField &other) : variant_(other.variant_)
{
  std::visit(
      [&]<typename T>(T &v) {
        if constexpr (std::is_same_v<T, OwnedConstant>) {
          void *new_value = MEM_new_uninitialized_aligned(
              v.type->size, v.type->alignment, __func__);
          v.type->copy_construct(v.value, new_value);
          v.value = new_value;
        }
      },
      variant_);
}

GField::GField(GField &&other) noexcept : variant_(std::move(other.variant_))
{
  const CPPType &type = this->cpp_type();
  other.variant_ = ConstantRef{&type, type.default_value()};
}

GField &GField::operator=(const GField &other)
{
  if (this == &other) {
    return *this;
  }
  this->~GField();
  new (this) GField(other);
  return *this;
}

GField &GField::operator=(GField &&other) noexcept
{
  if (this == &other) {
    return *this;
  }
  this->~GField();
  new (this) GField(std::move(other));
  return *this;
}

GField::~GField()
{
  std::visit(
      [&]<typename T>(T &v) {
        if constexpr (std::is_same_v<T, OwnedConstant>) {
          v.type->destruct(v.value);
          MEM_delete_void(v.value);
        }
      },
      variant_);
}

GFieldRef::GFieldRef(const GField &field)
    : variant_(std::visit(
          []<typename T>(const T &v) -> Variant {
            if constexpr (std::is_same_v<T, GField::Input>) {
              return Input{v.node.get()};
            }
            else if constexpr (std::is_same_v<T, GField::MultiFn>) {
              return MultiFn{v.node.get(), v.output_i};
            }
            else if constexpr (std::is_same_v<T, GField::FieldRef>) {
              /* Should not exist due to #deref_field_ref. */
              BLI_assert_unreachable();
              return Value{};
            }
            else if constexpr (GField::is_constant_value_v<T>) {
              return Value{v.type, v.value};
            }
          },
          field.deref_field_ref().variant()))
{
}

const FieldInputsPtr &GFieldRef::field_inputs() const
{
  static const ImplicitSharingPtr<FieldInputs> empty_inputs;
  return std::visit(
      [&]<typename T>(const T &v) -> const FieldInputsPtr & {
        if constexpr (std::is_same_v<T, Input>) {
          return v.node->field_inputs();
        }
        else if constexpr (std::is_same_v<T, MultiFn>) {
          return v.node->field_inputs();
        }
        else if constexpr (std::is_same_v<T, Value>) {
          return empty_inputs;
        }
      },
      variant_);
}

bool operator==(const GFieldRef &a, const GFieldRef &b)
{
  return std::visit(
      [&]<typename T>(const T &v_a) -> bool {
        if constexpr (std::is_same_v<T, GFieldRef::Value>) {
          if (const auto *v_b = std::get_if<GFieldRef::Value>(&b.variant())) {
            if (v_a.type != v_b->type) {
              return false;
            }
            return v_a.type->is_equal_or_false(v_a.value, v_b->value);
          }
          return false;
        }
        else if constexpr (std::is_same_v<T, GFieldRef::Input>) {
          if (const auto *v_b = std::get_if<GFieldRef::Input>(&b.variant())) {
            return v_a.node == v_b->node;
          }
          return false;
        }
        else if constexpr (std::is_same_v<T, GFieldRef::MultiFn>) {
          if (const auto *v_b = std::get_if<GFieldRef::MultiFn>(&b.variant())) {
            return v_a.node == v_b->node && v_a.output_i == v_b->output_i;
          }
          return false;
        }
      },
      a.variant());
}

uint64_t GFieldRef::hash() const
{
  return std::visit(
      [&]<typename T>(const T &v) -> uint64_t {
        if constexpr (std::is_same_v<T, Value>) {
          return v.type->hash_or_fallback(v.value, uint64_t(v.type));
        }
        else if constexpr (std::is_same_v<T, Input>) {
          return get_default_hash(v.node);
        }
        else if constexpr (std::is_same_v<T, MultiFn>) {
          return get_default_hash(v.node, v.output_i);
        }
      },
      variant_);
}

FieldOperation::FieldOperation(std::shared_ptr<const mf::MultiFunction> fn, Vector<GField> inputs)
    : FieldOperation(*fn, std::move(inputs))
{
  owned_fn_ = std::move(fn);
}

FieldOperation::FieldOperation(const mf::MultiFunction &fn, Vector<GField> inputs)
    : inputs_(inputs), fn_(&fn)
{
  field_inputs_ = combine_field_inputs(inputs_);
}

const CPPType &FieldOperation::output_cpp_type(const int output_i) const
{
  int count = 0;
  for (const int param_index : fn_->param_indices()) {
    const mf::ParamType param_type = fn_->param_type(param_index);
    if (param_type.is_output()) {
      if (count == output_i) {
        return param_type.data_type().single_type();
      }
      count++;
    }
  }
  BLI_assert_unreachable();
  return CPPType::get<float>();
}

const FieldInputsPtr &GField::field_inputs() const
{
  static const ImplicitSharingPtr<FieldInputs> empty_inputs;
  return std::visit(
      []<typename T>(const T &v) -> const FieldInputsPtr & {
        if constexpr (is_same_any_v<T, Input, MultiFn>) {
          return v.node->field_inputs();
        }
        else if constexpr (std::is_same_v<T, FieldRef>) {
          return v.field_ref->field_inputs();
        }
        else if constexpr (is_same_any_v<T, ConstantRef, TrivialInlineConstant, OwnedConstant>) {
          return empty_inputs;
        }
      },
      this->variant_);
}

GVArray FieldContext::get_varray_for_input(const FieldInput &field_input,
                                           const IndexMask &mask,
                                           ResourceScope &scope) const
{
  /* By default ask the field input to create the varray. Another field context might overwrite
   * the context here. */
  return field_input.get_varray_for_context(*this, mask, scope);
}

IndexFieldInput::IndexFieldInput() : FieldInput(CPPType::get<int>(), "Index") {}

GVArray IndexFieldInput::get_index_varray(const IndexMask &mask)
{
  auto index_func = [](int i) { return i; };
  return VArray<int>::from_func(mask.min_array_size(), index_func);
}

GVArray IndexFieldInput::get_varray_for_context(const fn::FieldContext & /*context*/,
                                                const IndexMask &mask,
                                                ResourceScope & /*scope*/) const
{
  /* TODO: Investigate a similar method to IndexRange::as_span() */
  return get_index_varray(mask);
}

void IndexFieldInput::hash_unique(UniqueHashBytes &hash,
                                  fn::FieldHashDeep & /*deep_hash_cache*/) const
{
  static constexpr int8_t id = 0;
  hash.add(&id);
}

const Field<int> &IndexFieldInput::get_field()
{
  static const Field<int> field = Field<int>::from_input<IndexFieldInput>();
  static const Field<int> field_ref = Field<int>::from_non_owning_ref(field);
  return field_ref;
}

Field<bool> invert_boolean_field(const Field<bool> &field)
{
  const mf::MultiFunction &not_fn = fn::multi_function::registry::lookup("!bool"_ustr);
  auto not_op = FieldOperation::from(not_fn, {field});
  return GField(not_op, 0).typed<bool>();
}

}  // namespace blender::fn

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "FN_multi_function_procedure_executor.hh"

#include "BLI_stack.hh"

namespace blender::fn::multi_function {

ProcedureExecutor::ProcedureExecutor(const Procedure &procedure) : procedure_(procedure)
{
  SignatureBuilder builder("Procedure Executor", signature_);

  for (const ConstParameter &param : procedure.params()) {
    builder.add("Parameter", ParamType(param.type, param.variable->data_type()));
  }

  this->set_signature(&signature_);
}

using IndicesSplitVectors = std::array<Vector<int64_t>, 2>;

namespace {
enum class ValueType {
  GVArray = 0,
  Span = 1,
  GVVectorArray = 2,
  GVectorArray = 3,
  OneSingle = 4,
  OneVector = 5,
};
constexpr int tot_variable_value_types = 6;
}  // namespace

/**
 * During evaluation, a variable may be stored in various different forms, depending on what
 * instructions do with the variables.
 */
struct VariableValue {
  ValueType type;

  VariableValue(ValueType type) : type(type) {}
};

/* This variable is the unmodified virtual array from the caller. */
struct VariableValue_GVArray : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::GVArray;
  const GVArray &data;

  VariableValue_GVArray(const GVArray &data) : VariableValue(static_type), data(data)
  {
    BLI_assert(data);
  }
};

/* This variable has a different value for every index. Some values may be uninitialized. The span
 * may be owned by the caller. */
struct VariableValue_Span : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::Span;
  void *data;
  bool owned;

  VariableValue_Span(void *data, bool owned) : VariableValue(static_type), data(data), owned(owned)
  {
  }
};

/* This variable is the unmodified virtual vector array from the caller. */
struct VariableValue_GVVectorArray : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::GVVectorArray;
  const GVVectorArray &data;

  VariableValue_GVVectorArray(const GVVectorArray &data) : VariableValue(static_type), data(data)
  {
  }
};

/* This variable has a different vector for every index. */
struct VariableValue_GVectorArray : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::GVectorArray;
  GVectorArray &data;
  bool owned;

  VariableValue_GVectorArray(GVectorArray &data, bool owned)
      : VariableValue(static_type), data(data), owned(owned)
  {
  }
};

/* This variable has the same value for every index. */
struct VariableValue_OneSingle : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::OneSingle;
  void *data;
  bool is_initialized = false;

  VariableValue_OneSingle(void *data) : VariableValue(static_type), data(data) {}
};

/* This variable has the same vector for every index. */
struct VariableValue_OneVector : public VariableValue {
  static inline constexpr ValueType static_type = ValueType::OneVector;
  GVectorArray &data;

  VariableValue_OneVector(GVectorArray &data) : VariableValue(static_type), data(data) {}
};

static_assert(std::is_trivially_destructible_v<VariableValue_GVArray>);
static_assert(std::is_trivially_destructible_v<VariableValue_Span>);
static_assert(std::is_trivially_destructible_v<VariableValue_GVVectorArray>);
static_assert(std::is_trivially_destructible_v<VariableValue_GVectorArray>);
static_assert(std::is_trivially_destructible_v<VariableValue_OneSingle>);
static_assert(std::is_trivially_destructible_v<VariableValue_OneVector>);

class VariableState;

/**
 * The #ValueAllocator is responsible for providing memory for variables and their values. It also
 * manages the reuse of buffers to improve performance.
 */
class ValueAllocator : NonCopyable, NonMovable {
 private:
  /**
   * Allocate with 64 byte alignment for better reusability of buffers and improved cache
   * performance.
   */
  static constexpr inline int min_alignment = 64;

  /** All buffers in the free-lists below have been allocated with this allocator. */
  LinearAllocator<> &linear_allocator_;

  /**
   * Use stacks so that the most recently used buffers are reused first. This improves cache
   * efficiency.
   */
  std::array<Stack<VariableValue *>, tot_variable_value_types> variable_value_free_lists_;

  /**
   * The integer key is the size of one element (e.g. 4 for an integer buffer). All buffers are
   * aligned to #min_alignment bytes.
   */
  Stack<void *> small_span_buffers_free_list_;
  Map<int, Stack<void *>> span_buffers_free_lists_;

  /** Cache buffers for single values of different types. */
  static constexpr inline int small_value_max_size = 16;
  static constexpr inline int small_value_max_alignment = 8;
  Stack<void *> small_single_value_free_list_;
  Map<const CPPType *, Stack<void *>> single_value_free_lists_;

 public:
  ValueAllocator(LinearAllocator<> &linear_allocator) : linear_allocator_(linear_allocator) {}

  VariableValue_GVArray *obtain_GVArray(const GVArray &varray)
  {
    return this->obtain<VariableValue_GVArray>(varray);
  }

  VariableValue_GVVectorArray *obtain_GVVectorArray(const GVVectorArray &varray)
  {
    return this->obtain<VariableValue_GVVectorArray>(varray);
  }

  VariableValue_Span *obtain_Span_not_owned(void *buffer)
  {
    return this->obtain<VariableValue_Span>(buffer, false);
  }

  VariableValue_Span *obtain_Span(const CPPType &type, int size)
  {
    void *buffer = nullptr;

    const int64_t element_size = type.size();
    const int64_t alignment = type.alignment();

    if (alignment > min_alignment) {
      /* In this rare case we fallback to not reusing existing buffers. */
      buffer = linear_allocator_.allocate(element_size * size, alignment);
    }
    else {
      Stack<void *> *stack = type.can_exist_in_buffer(small_value_max_size,
                                                      small_value_max_alignment) ?
                                 &small_span_buffers_free_list_ :
                                 span_buffers_free_lists_.lookup_ptr(element_size);
      if (stack == nullptr || stack->is_empty()) {
        buffer = linear_allocator_.allocate(
            std::max<int64_t>(element_size, small_value_max_size) * size, min_alignment);
      }
      else {
        /* Reuse existing buffer. */
        buffer = stack->pop();
      }
    }

    return this->obtain<VariableValue_Span>(buffer, true);
  }

  VariableValue_GVectorArray *obtain_GVectorArray_not_owned(GVectorArray &data)
  {
    return this->obtain<VariableValue_GVectorArray>(data, false);
  }

  VariableValue_GVectorArray *obtain_GVectorArray(const CPPType &type, int size)
  {
    GVectorArray *vector_array = new GVectorArray(type, size);
    return this->obtain<VariableValue_GVectorArray>(*vector_array, true);
  }

  VariableValue_OneSingle *obtain_OneSingle(const CPPType &type)
  {
    const bool is_small = type.can_exist_in_buffer(small_value_max_size,
                                                   small_value_max_alignment);
    Stack<void *> &stack = is_small ? small_single_value_free_list_ :
                                      single_value_free_lists_.lookup_or_add_default(&type);
    void *buffer;
    if (stack.is_empty()) {
      buffer = linear_allocator_.allocate(
          std::max<int>(small_value_max_size, type.size()),
          std::max<int>(small_value_max_alignment, type.alignment()));
    }
    else {
      buffer = stack.pop();
    }
    return this->obtain<VariableValue_OneSingle>(buffer);
  }

  VariableValue_OneVector *obtain_OneVector(const CPPType &type)
  {
    GVectorArray *vector_array = new GVectorArray(type, 1);
    return this->obtain<VariableValue_OneVector>(*vector_array);
  }

  void release_value(VariableValue *value, const DataType &data_type)
  {
    switch (value->type) {
      case ValueType::GVArray: {
        break;
      }
      case ValueType::Span: {
        auto *value_typed = static_cast<VariableValue_Span *>(value);
        if (value_typed->owned) {
          const CPPType &type = data_type.single_type();
          /* Assumes all values in the buffer are uninitialized already. */
          Stack<void *> &buffers = type.can_exist_in_buffer(small_value_max_size,
                                                            small_value_max_alignment) ?
                                       small_span_buffers_free_list_ :
                                       span_buffers_free_lists_.lookup_or_add_default(type.size());
          buffers.push(value_typed->data);
        }
        break;
      }
      case ValueType::GVVectorArray: {
        break;
      }
      case ValueType::GVectorArray: {
        auto *value_typed = static_cast<VariableValue_GVectorArray *>(value);
        if (value_typed->owned) {
          delete &value_typed->data;
        }
        break;
      }
      case ValueType::OneSingle: {
        auto *value_typed = static_cast<VariableValue_OneSingle *>(value);
        const CPPType &type = data_type.single_type();
        if (value_typed->is_initialized) {
          type.destruct(value_typed->data);
        }
        const bool is_small = type.can_exist_in_buffer(small_value_max_size,
                                                       small_value_max_alignment);
        if (is_small) {
          small_single_value_free_list_.push(value_typed->data);
        }
        else {
          single_value_free_lists_.lookup_or_add_default(&type).push(value_typed->data);
        }
        break;
      }
      case ValueType::OneVector: {
        auto *value_typed = static_cast<VariableValue_OneVector *>(value);
        delete &value_typed->data;
        break;
      }
    }

    Stack<VariableValue *> &stack = variable_value_free_lists_[int(value->type)];
    stack.push(value);
  }

 private:
  template<typename T, typename... Args> T *obtain(Args &&...args)
  {
    static_assert(std::is_base_of_v<VariableValue, T>);
    Stack<VariableValue *> &stack = variable_value_free_lists_[int(T::static_type)];
    if (stack.is_empty()) {
      void *buffer = linear_allocator_.allocate(sizeof(T), alignof(T));
      return new (buffer) T(std::forward<Args>(args)...);
    }
    return new (stack.pop()) T(std::forward<Args>(args)...);
  }
};

/**
 * This class keeps track of a single variable during evaluation.
 */
class VariableState : NonCopyable, NonMovable {
 public:
  /** The current value of the variable. The storage format may change over time. */
  VariableValue *value_ = nullptr;
  /** Number of indices that are currently initialized in this variable. */
  int tot_initialized_ = 0;
  /* This a non-owning pointer to either span buffer or #GVectorArray or null. */
  void *caller_provided_storage_ = nullptr;

  void destruct_value(ValueAllocator &value_allocator, const DataType &data_type)
  {
    value_allocator.release_value(value_, data_type);
    value_ = nullptr;
  }

  /* True if this contains only one value for all indices, i.e. the value for all indices is
   * the same. */
  bool is_one() const
  {
    if (value_ == nullptr) {
      return true;
    }
    switch (value_->type) {
      case ValueType::GVArray:
        return this->value_as<VariableValue_GVArray>()->data.is_single();
      case ValueType::Span:
        return tot_initialized_ == 0;
      case ValueType::GVVectorArray:
        return this->value_as<VariableValue_GVVectorArray>()->data.is_single_vector();
      case ValueType::GVectorArray:
        return tot_initialized_ == 0;
      case ValueType::OneSingle:
        return true;
      case ValueType::OneVector:
        return true;
    }
    BLI_assert_unreachable();
    return false;
  }

  bool is_fully_initialized(const IndexMask &full_mask)
  {
    return tot_initialized_ == full_mask.size();
  }

  bool is_fully_uninitialized(const IndexMask &full_mask)
  {
    UNUSED_VARS(full_mask);
    return tot_initialized_ == 0;
  }

  void add_as_input(ParamsBuilder &params, const IndexMask &mask, const DataType &data_type) const
  {
    /* Sanity check to make sure that enough values are initialized. */
    BLI_assert(mask.size() <= tot_initialized_);
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::GVArray: {
        params.add_readonly_single_input(this->value_as<VariableValue_GVArray>()->data);
        break;
      }
      case ValueType::Span: {
        const void *data = this->value_as<VariableValue_Span>()->data;
        const GSpan span{data_type.single_type(), data, mask.min_array_size()};
        params.add_readonly_single_input(span);
        break;
      }
      case ValueType::GVVectorArray: {
        params.add_readonly_vector_input(this->value_as<VariableValue_GVVectorArray>()->data);
        break;
      }
      case ValueType::GVectorArray: {
        params.add_readonly_vector_input(this->value_as<VariableValue_GVectorArray>()->data);
        break;
      }
      case ValueType::OneSingle: {
        const auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        const GPointer gpointer{data_type.single_type(), value_typed->data};
        params.add_readonly_single_input(gpointer);
        break;
      }
      case ValueType::OneVector: {
        params.add_readonly_vector_input(this->value_as<VariableValue_OneVector>()->data[0]);
        break;
      }
    }
  }

  void ensure_is_mutable(const IndexMask &full_mask,
                         const DataType &data_type,
                         ValueAllocator &value_allocator)
  {
    if (value_ != nullptr && ELEM(value_->type, ValueType::Span, ValueType::GVectorArray)) {
      return;
    }

    const int array_size = full_mask.min_array_size();

    switch (data_type.category()) {
      case DataType::Single: {
        const CPPType &type = data_type.single_type();
        VariableValue_Span *new_value = nullptr;
        if (caller_provided_storage_ == nullptr) {
          new_value = value_allocator.obtain_Span(type, array_size);
        }
        else {
          /* Reuse the storage provided caller when possible. */
          new_value = value_allocator.obtain_Span_not_owned(caller_provided_storage_);
        }
        if (value_ != nullptr) {
          if (value_->type == ValueType::GVArray) {
            /* Fill new buffer with data from virtual array. */
            this->value_as<VariableValue_GVArray>()->data.materialize_to_uninitialized(
                full_mask, new_value->data);
          }
          else if (value_->type == ValueType::OneSingle) {
            auto *old_value_typed_ = this->value_as<VariableValue_OneSingle>();
            if (old_value_typed_->is_initialized) {
              /* Fill the buffer with a single value. */
              type.fill_construct_indices(old_value_typed_->data, new_value->data, full_mask);
            }
          }
          else {
            BLI_assert_unreachable();
          }
          value_allocator.release_value(value_, data_type);
        }
        value_ = new_value;
        break;
      }
      case DataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        VariableValue_GVectorArray *new_value = nullptr;
        if (caller_provided_storage_ == nullptr) {
          new_value = value_allocator.obtain_GVectorArray(type, array_size);
        }
        else {
          new_value = value_allocator.obtain_GVectorArray_not_owned(
              *static_cast<GVectorArray *>(caller_provided_storage_));
        }
        if (value_ != nullptr) {
          if (value_->type == ValueType::GVVectorArray) {
            /* Fill new vector array with data from virtual vector array. */
            new_value->data.extend(full_mask, this->value_as<VariableValue_GVVectorArray>()->data);
          }
          else if (value_->type == ValueType::OneVector) {
            /* Fill all indices with the same value. */
            const GSpan vector = this->value_as<VariableValue_OneVector>()->data[0];
            new_value->data.extend(full_mask, GVVectorArray_For_SingleGSpan{vector, array_size});
          }
          else {
            BLI_assert_unreachable();
          }
          value_allocator.release_value(value_, data_type);
        }
        value_ = new_value;
        break;
      }
    }
  }

  void add_as_mutable(ParamsBuilder &params,
                      const IndexMask &mask,
                      const IndexMask &full_mask,
                      const DataType &data_type,
                      ValueAllocator &value_allocator)
  {
    /* Sanity check to make sure that enough values are initialized. */
    BLI_assert(mask.size() <= tot_initialized_);

    this->ensure_is_mutable(full_mask, data_type, value_allocator);
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::Span: {
        void *data = this->value_as<VariableValue_Span>()->data;
        const GMutableSpan span{data_type.single_type(), data, mask.min_array_size()};
        params.add_single_mutable(span);
        break;
      }
      case ValueType::GVectorArray: {
        params.add_vector_mutable(this->value_as<VariableValue_GVectorArray>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::GVVectorArray:
      case ValueType::OneSingle:
      case ValueType::OneVector: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void add_as_output(ParamsBuilder &params,
                     const IndexMask &mask,
                     const IndexMask &full_mask,
                     const DataType &data_type,
                     ValueAllocator &value_allocator)
  {
    /* Sanity check to make sure that enough values are not initialized. */
    BLI_assert(mask.size() <= full_mask.size() - tot_initialized_);
    this->ensure_is_mutable(full_mask, data_type, value_allocator);
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::Span: {
        void *data = this->value_as<VariableValue_Span>()->data;
        const GMutableSpan span{data_type.single_type(), data, mask.min_array_size()};
        params.add_uninitialized_single_output(span);
        break;
      }
      case ValueType::GVectorArray: {
        params.add_vector_output(this->value_as<VariableValue_GVectorArray>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::GVVectorArray:
      case ValueType::OneSingle:
      case ValueType::OneVector: {
        BLI_assert_unreachable();
        break;
      }
    }

    tot_initialized_ += mask.size();
  }

  void add_as_input__one(ParamsBuilder &params, const DataType &data_type) const
  {
    BLI_assert(this->is_one());
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::GVArray: {
        params.add_readonly_single_input(this->value_as<VariableValue_GVArray>()->data);
        break;
      }
      case ValueType::GVVectorArray: {
        params.add_readonly_vector_input(this->value_as<VariableValue_GVVectorArray>()->data);
        break;
      }
      case ValueType::OneSingle: {
        const auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        GPointer ptr{data_type.single_type(), value_typed->data};
        params.add_readonly_single_input(ptr);
        break;
      }
      case ValueType::OneVector: {
        params.add_readonly_vector_input(this->value_as<VariableValue_OneVector>()->data);
        break;
      }
      case ValueType::Span:
      case ValueType::GVectorArray: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void ensure_is_mutable__one(const DataType &data_type, ValueAllocator &value_allocator)
  {
    BLI_assert(this->is_one());
    if (value_ != nullptr && ELEM(value_->type, ValueType::OneSingle, ValueType::OneVector)) {
      return;
    }

    switch (data_type.category()) {
      case DataType::Single: {
        const CPPType &type = data_type.single_type();
        VariableValue_OneSingle *new_value = value_allocator.obtain_OneSingle(type);
        if (value_ != nullptr) {
          if (value_->type == ValueType::GVArray) {
            this->value_as<VariableValue_GVArray>()->data.get_internal_single_to_uninitialized(
                new_value->data);
            new_value->is_initialized = true;
          }
          else if (value_->type == ValueType::Span) {
            BLI_assert(tot_initialized_ == 0);
            /* Nothing to do, the single value is uninitialized already. */
          }
          else {
            BLI_assert_unreachable();
          }
          value_allocator.release_value(value_, data_type);
        }
        value_ = new_value;
        break;
      }
      case DataType::Vector: {
        const CPPType &type = data_type.vector_base_type();
        VariableValue_OneVector *new_value = value_allocator.obtain_OneVector(type);
        if (value_ != nullptr) {
          if (value_->type == ValueType::GVVectorArray) {
            const GVVectorArray &old_vector_array =
                this->value_as<VariableValue_GVVectorArray>()->data;
            new_value->data.extend(IndexRange(1), old_vector_array);
          }
          else if (value_->type == ValueType::GVectorArray) {
            BLI_assert(tot_initialized_ == 0);
            /* Nothing to do. */
          }
          else {
            BLI_assert_unreachable();
          }
          value_allocator.release_value(value_, data_type);
        }
        value_ = new_value;
        break;
      }
    }
  }

  void add_as_mutable__one(ParamsBuilder &params,
                           const DataType &data_type,
                           ValueAllocator &value_allocator)
  {
    BLI_assert(this->is_one());
    this->ensure_is_mutable__one(data_type, value_allocator);
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        params.add_single_mutable(GMutableSpan{data_type.single_type(), value_typed->data, 1});
        break;
      }
      case ValueType::OneVector: {
        params.add_vector_mutable(this->value_as<VariableValue_OneVector>()->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::Span:
      case ValueType::GVVectorArray:
      case ValueType::GVectorArray: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  void add_as_output__one(ParamsBuilder &params,
                          const IndexMask &mask,
                          const DataType &data_type,
                          ValueAllocator &value_allocator)
  {
    BLI_assert(this->is_one());
    this->ensure_is_mutable__one(data_type, value_allocator);
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(!value_typed->is_initialized);
        params.add_uninitialized_single_output(
            GMutableSpan{data_type.single_type(), value_typed->data, 1});
        /* It becomes initialized when the multi-function is called. */
        value_typed->is_initialized = true;
        break;
      }
      case ValueType::OneVector: {
        auto *value_typed = this->value_as<VariableValue_OneVector>();
        BLI_assert(value_typed->data[0].is_empty());
        params.add_vector_output(value_typed->data);
        break;
      }
      case ValueType::GVArray:
      case ValueType::Span:
      case ValueType::GVVectorArray:
      case ValueType::GVectorArray: {
        BLI_assert_unreachable();
        break;
      }
    }

    tot_initialized_ += mask.size();
  }

  /**
   * Destruct the masked elements in this variable.
   * \return True when all elements of this variable are initialized and the variable state can be
   *  released.
   */
  bool destruct(const IndexMask &mask,
                const IndexMask &full_mask,
                const DataType &data_type,
                ValueAllocator &value_allocator)
  {
    BLI_assert(value_ != nullptr);
    int new_tot_initialized = tot_initialized_ - mask.size();

    /* Sanity check to make sure that enough indices can be destructed. */
    BLI_assert(new_tot_initialized >= 0);

    switch (value_->type) {
      case ValueType::GVArray: {
        if (mask.size() < full_mask.size()) {
          /* Not all elements are destructed. Since we can't work on the original array, we have to
           * create a copy first. */
          this->ensure_is_mutable(full_mask, data_type, value_allocator);
          BLI_assert(value_->type == ValueType::Span);
          const CPPType &type = data_type.single_type();
          type.destruct_indices(this->value_as<VariableValue_Span>()->data, mask);
        }
        break;
      }
      case ValueType::Span: {
        const CPPType &type = data_type.single_type();
        type.destruct_indices(this->value_as<VariableValue_Span>()->data, mask);
        break;
      }
      case ValueType::GVVectorArray: {
        if (mask.size() < full_mask.size()) {
          /* Not all elements are cleared. Since we can't work on the original vector array, we
           * have to create a copy first. A possible future optimization is to create the partial
           * copy directly. */
          this->ensure_is_mutable(full_mask, data_type, value_allocator);
          BLI_assert(value_->type == ValueType::GVectorArray);
          this->value_as<VariableValue_GVectorArray>()->data.clear(mask);
        }
        break;
      }
      case ValueType::GVectorArray: {
        this->value_as<VariableValue_GVectorArray>()->data.clear(mask);
        break;
      }
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        UNUSED_VARS_NDEBUG(value_typed);
        if (mask.size() == tot_initialized_) {
          const CPPType &type = data_type.single_type();
          type.destruct(value_typed->data);
          value_typed->is_initialized = false;
        }
        break;
      }
      case ValueType::OneVector: {
        auto *value_typed = this->value_as<VariableValue_OneVector>();
        if (mask.size() == tot_initialized_) {
          value_typed->data.clear(IndexRange(1));
        }
        break;
      }
    }

    tot_initialized_ = new_tot_initialized;

    const bool should_self_destruct = new_tot_initialized == 0 &&
                                      caller_provided_storage_ == nullptr;
    return should_self_destruct;
  }

  void indices_split(const IndexMask &mask, IndicesSplitVectors &r_indices)
  {
    BLI_assert(mask.size() <= tot_initialized_);
    BLI_assert(value_ != nullptr);

    switch (value_->type) {
      case ValueType::GVArray: {
        const VArray<bool> varray = this->value_as<VariableValue_GVArray>()->data.typed<bool>();
        mask.foreach_index([&](const int64_t i) { r_indices[varray[i]].append(i); });
        break;
      }
      case ValueType::Span: {
        const Span<bool> span(
            static_cast<const bool *>(this->value_as<VariableValue_Span>()->data),
            mask.min_array_size());
        mask.foreach_index([&](const int64_t i) { r_indices[span[i]].append(i); });
        break;
      }
      case ValueType::OneSingle: {
        auto *value_typed = this->value_as<VariableValue_OneSingle>();
        BLI_assert(value_typed->is_initialized);
        const bool condition = *static_cast<const bool *>(value_typed->data);
        Vector<int64_t> &indices = r_indices[condition];
        indices.reserve(indices.size() + mask.size());
        mask.foreach_index_optimized<int64_t>([&](const int64_t i) { indices.append(i); });
        break;
      }
      case ValueType::GVVectorArray:
      case ValueType::GVectorArray:
      case ValueType::OneVector: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  template<typename T> T *value_as()
  {
    BLI_assert(value_ != nullptr);
    BLI_assert(value_->type == T::static_type);
    return static_cast<T *>(value_);
  }

  template<typename T> const T *value_as() const
  {
    BLI_assert(value_ != nullptr);
    BLI_assert(value_->type == T::static_type);
    return static_cast<T *>(value_);
  }
};

/** Keeps track of the states of all variables during evaluation. */
class VariableStates {
 private:
  ValueAllocator value_allocator_;
  const Procedure &procedure_;
  /** The state of every variable, indexed by #Variable::index_in_procedure(). */
  Array<VariableState> variable_states_;
  const IndexMask &full_mask_;

 public:
  VariableStates(LinearAllocator<> &linear_allocator,
                 const Procedure &procedure,
                 const IndexMask &full_mask)
      : value_allocator_(linear_allocator),
        procedure_(procedure),
        variable_states_(procedure.variables().size()),
        full_mask_(full_mask)
  {
  }

  ~VariableStates()
  {
    for (const int variable_i : procedure_.variables().index_range()) {
      VariableState &state = variable_states_[variable_i];
      if (state.value_ != nullptr) {
        const Variable *variable = procedure_.variables()[variable_i];
        state.destruct_value(value_allocator_, variable->data_type());
      }
    }
  }

  ValueAllocator &value_allocator()
  {
    return value_allocator_;
  }

  const IndexMask &full_mask() const
  {
    return full_mask_;
  }

  void add_initial_variable_states(const ProcedureExecutor &fn,
                                   const Procedure &procedure,
                                   Params &params)
  {
    for (const int param_index : fn.param_indices()) {
      ParamType param_type = fn.param_type(param_index);
      const Variable *variable = procedure.params()[param_index].variable;

      auto add_state = [&](VariableValue *value,
                           bool input_is_initialized,
                           void *caller_provided_storage = nullptr) {
        const int tot_initialized = input_is_initialized ? full_mask_.size() : 0;
        const int variable_i = variable->index_in_procedure();
        VariableState &variable_state = variable_states_[variable_i];
        BLI_assert(variable_state.value_ == nullptr);
        variable_state.value_ = value;
        variable_state.tot_initialized_ = tot_initialized;
        variable_state.caller_provided_storage_ = caller_provided_storage;
      };

      switch (param_type.category()) {
        case ParamCategory::SingleInput: {
          const GVArray &data = params.readonly_single_input(param_index);
          add_state(value_allocator_.obtain_GVArray(data), true);
          break;
        }
        case ParamCategory::VectorInput: {
          const GVVectorArray &data = params.readonly_vector_input(param_index);
          add_state(value_allocator_.obtain_GVVectorArray(data), true);
          break;
        }
        case ParamCategory::SingleOutput: {
          GMutableSpan data = params.uninitialized_single_output(param_index);
          add_state(value_allocator_.obtain_Span_not_owned(data.data()), false, data.data());
          break;
        }
        case ParamCategory::VectorOutput: {
          GVectorArray &data = params.vector_output(param_index);
          add_state(value_allocator_.obtain_GVectorArray_not_owned(data), false, &data);
          break;
        }
        case ParamCategory::SingleMutable: {
          GMutableSpan data = params.single_mutable(param_index);
          add_state(value_allocator_.obtain_Span_not_owned(data.data()), true, data.data());
          break;
        }
        case ParamCategory::VectorMutable: {
          GVectorArray &data = params.vector_mutable(param_index);
          add_state(value_allocator_.obtain_GVectorArray_not_owned(data), true, &data);
          break;
        }
      }
    }
  }

  void add_as_param(VariableState &variable_state,
                    ParamsBuilder &params,
                    const ParamType &param_type,
                    const IndexMask &mask)
  {
    const DataType data_type = param_type.data_type();
    switch (param_type.interface_type()) {
      case ParamType::Input: {
        variable_state.add_as_input(params, mask, data_type);
        break;
      }
      case ParamType::Mutable: {
        variable_state.add_as_mutable(params, mask, full_mask_, data_type, value_allocator_);
        break;
      }
      case ParamType::Output: {
        variable_state.add_as_output(params, mask, full_mask_, data_type, value_allocator_);
        break;
      }
    }
  }

  void add_as_param__one(VariableState &variable_state,
                         ParamsBuilder &params,
                         const ParamType &param_type,
                         const IndexMask &mask)
  {
    const DataType data_type = param_type.data_type();
    switch (param_type.interface_type()) {
      case ParamType::Input: {
        variable_state.add_as_input__one(params, data_type);
        break;
      }
      case ParamType::Mutable: {
        variable_state.add_as_mutable__one(params, data_type, value_allocator_);
        break;
      }
      case ParamType::Output: {
        variable_state.add_as_output__one(params, mask, data_type, value_allocator_);
        break;
      }
    }
  }

  void destruct(const Variable &variable, const IndexMask &mask)
  {
    VariableState &variable_state = this->get_variable_state(variable);
    if (variable_state.destruct(mask, full_mask_, variable.data_type(), value_allocator_)) {
      variable_state.destruct_value(value_allocator_, variable.data_type());
    }
  }

  VariableState &get_variable_state(const Variable &variable)
  {
    const int variable_i = variable.index_in_procedure();
    VariableState &variable_state = variable_states_[variable_i];
    return variable_state;
  }
};

static bool evaluate_as_one(Span<VariableState *> param_variable_states,
                            const IndexMask &mask,
                            const IndexMask &full_mask)
{
  if (mask.size() < full_mask.size()) {
    return false;
  }
  for (VariableState *state : param_variable_states) {
    if (state != nullptr && state->value_ != nullptr && !state->is_one()) {
      return false;
    }
  }
  return true;
}

static void gather_parameter_variable_states(const MultiFunction &fn,
                                             const CallInstruction &instruction,
                                             VariableStates &variable_states,
                                             MutableSpan<VariableState *> r_param_variable_states)
{
  for (const int param_index : fn.param_indices()) {
    const Variable *variable = instruction.params()[param_index];
    if (variable == nullptr) {
      r_param_variable_states[param_index] = nullptr;
    }
    else {
      VariableState &variable_state = variable_states.get_variable_state(*variable);
      r_param_variable_states[param_index] = &variable_state;
    }
  }
}

static void fill_params__one(const MultiFunction &fn,
                             const IndexMask &mask,
                             ParamsBuilder &params,
                             VariableStates &variable_states,
                             const Span<VariableState *> param_variable_states)
{
  for (const int param_index : fn.param_indices()) {
    const ParamType param_type = fn.param_type(param_index);
    VariableState *variable_state = param_variable_states[param_index];
    if (variable_state == nullptr) {
      params.add_ignored_single_output();
    }
    else {
      variable_states.add_as_param__one(*variable_state, params, param_type, mask);
    }
  }
}

static void fill_params(const MultiFunction &fn,
                        const IndexMask &mask,
                        ParamsBuilder &params,
                        VariableStates &variable_states,
                        const Span<VariableState *> param_variable_states)
{
  for (const int param_index : fn.param_indices()) {
    const ParamType param_type = fn.param_type(param_index);
    VariableState *variable_state = param_variable_states[param_index];
    if (variable_state == nullptr) {
      params.add_ignored_single_output();
    }
    else {
      variable_states.add_as_param(*variable_state, params, param_type, mask);
    }
  }
}

static void execute_call_instruction(const CallInstruction &instruction,
                                     const IndexMask &mask,
                                     VariableStates &variable_states,
                                     const Context &context)
{
  const MultiFunction &fn = instruction.fn();

  Vector<VariableState *> param_variable_states;
  param_variable_states.resize(fn.param_amount());
  gather_parameter_variable_states(fn, instruction, variable_states, param_variable_states);

  /* If all inputs to the function are constant, it's enough to call the function only once instead
   * of for every index. */
  if (evaluate_as_one(param_variable_states, mask, variable_states.full_mask())) {
    static const IndexMask one_mask(1);
    ParamsBuilder params(fn, &one_mask);
    fill_params__one(fn, mask, params, variable_states, param_variable_states);

    try {
      fn.call(one_mask, params, context);
    }
    catch (...) {
      /* Multi-functions must not throw exceptions. */
      BLI_assert_unreachable();
    }
  }
  else {
    ParamsBuilder params(fn, &mask);
    fill_params(fn, mask, params, variable_states, param_variable_states);

    try {
      fn.call_auto(mask, params, context);
    }
    catch (...) {
      /* Multi-functions must not throw exceptions. */
      BLI_assert_unreachable();
    }
  }
}

/** An index mask, that might own the indices if necessary. */
struct InstructionIndices {
  std::unique_ptr<IndexMaskMemory> memory;
  IndexMask referenced_indices;

  const IndexMask &mask() const
  {
    return this->referenced_indices;
  }
};

/** Contains information about the next instruction that should be executed. */
struct NextInstructionInfo {
  const Instruction *instruction = nullptr;
  InstructionIndices indices;

  const IndexMask &mask() const
  {
    return this->indices.mask();
  }

  operator bool() const
  {
    return this->instruction != nullptr;
  }
};

/**
 * Keeps track of the next instruction for all indices and decides in which order instructions are
 * evaluated.
 */
class InstructionScheduler {
 private:
  Stack<NextInstructionInfo> next_instructions_;

 public:
  InstructionScheduler() = default;

  void add_referenced_indices(const Instruction &instruction, const IndexMask &mask)
  {
    if (mask.is_empty()) {
      return;
    }
    InstructionIndices new_indices;
    new_indices.referenced_indices = mask;
    next_instructions_.push({&instruction, std::move(new_indices)});
  }

  void add_owned_indices(const Instruction &instruction, Vector<int64_t> indices)
  {
    if (indices.is_empty()) {
      return;
    }

    InstructionIndices new_indices;
    new_indices.memory = std::make_unique<IndexMaskMemory>();
    new_indices.referenced_indices = IndexMask::from_indices<int64_t>(indices,
                                                                      *new_indices.memory);
    next_instructions_.push({&instruction, std::move(new_indices)});
  }

  bool is_done() const
  {
    return next_instructions_.is_empty();
  }

  const NextInstructionInfo &peek() const
  {
    BLI_assert(!this->is_done());
    return next_instructions_.peek();
  }

  void update_instruction_pointer(const Instruction &instruction)
  {
    next_instructions_.peek().instruction = &instruction;
  }

  NextInstructionInfo pop()
  {
    return next_instructions_.pop();
  }
};

void ProcedureExecutor::call(const IndexMask &full_mask, Params params, Context context) const
{
  BLI_assert(procedure_.validate());

  AlignedBuffer<512, 64> local_buffer;
  LinearAllocator<> linear_allocator;
  linear_allocator.provide_buffer(local_buffer);

  VariableStates variable_states{linear_allocator, procedure_, full_mask};
  variable_states.add_initial_variable_states(*this, procedure_, params);

  InstructionScheduler scheduler;
  scheduler.add_referenced_indices(*procedure_.entry(), full_mask);

  /* Loop until all indices got to a return instruction. */
  while (!scheduler.is_done()) {
    const NextInstructionInfo &instr_info = scheduler.peek();
    const Instruction &instruction = *instr_info.instruction;
    switch (instruction.type()) {
      case InstructionType::Call: {
        const CallInstruction &call_instruction = static_cast<const CallInstruction &>(
            instruction);
        execute_call_instruction(call_instruction, instr_info.mask(), variable_states, context);
        scheduler.update_instruction_pointer(*call_instruction.next());
        break;
      }
      case InstructionType::Branch: {
        const BranchInstruction &branch_instruction = static_cast<const BranchInstruction &>(
            instruction);
        const Variable *condition_var = branch_instruction.condition();
        VariableState &variable_state = variable_states.get_variable_state(*condition_var);

        IndicesSplitVectors new_indices;
        variable_state.indices_split(instr_info.mask(), new_indices);
        scheduler.pop();
        scheduler.add_owned_indices(*branch_instruction.branch_false(), new_indices[false]);
        scheduler.add_owned_indices(*branch_instruction.branch_true(), new_indices[true]);
        break;
      }
      case InstructionType::Destruct: {
        const DestructInstruction &destruct_instruction = static_cast<const DestructInstruction &>(
            instruction);
        const Variable *variable = destruct_instruction.variable();
        variable_states.destruct(*variable, instr_info.mask());
        scheduler.update_instruction_pointer(*destruct_instruction.next());
        break;
      }
      case InstructionType::Dummy: {
        const DummyInstruction &dummy_instruction = static_cast<const DummyInstruction &>(
            instruction);
        scheduler.update_instruction_pointer(*dummy_instruction.next());
        break;
      }
      case InstructionType::Return: {
        /* Don't insert the indices back into the scheduler. */
        scheduler.pop();
        break;
      }
    }
  }

  for (const int param_index : this->param_indices()) {
    const ParamType param_type = this->param_type(param_index);
    const Variable *variable = procedure_.params()[param_index].variable;
    VariableState &variable_state = variable_states.get_variable_state(*variable);
    switch (param_type.interface_type()) {
      case ParamType::Input: {
        /* Input variables must be destructed in the end. */
        BLI_assert(variable_state.is_fully_uninitialized(full_mask));
        break;
      }
      case ParamType::Mutable:
      case ParamType::Output: {
        /* Mutable and output variables must be initialized in the end. */
        BLI_assert(variable_state.is_fully_initialized(full_mask));
        /* Make sure that the data is in the memory provided by the caller. */
        variable_state.ensure_is_mutable(
            full_mask, param_type.data_type(), variable_states.value_allocator());
        break;
      }
    }
  }
}

MultiFunction::ExecutionHints ProcedureExecutor::get_execution_hints() const
{
  ExecutionHints hints;
  hints.allocates_array = true;
  hints.min_grain_size = 10000;
  return hints;
}

}  // namespace blender::fn::multi_function

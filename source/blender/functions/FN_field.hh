/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup fn
 *
 * A #Field represents a function that outputs a value based on an arbitrary number of inputs. The
 * inputs for a specific field evaluation are provided by a #FieldContext.
 *
 * A typical example is a field that computes a displacement vector for every vertex on a mesh
 * based on its position.
 *
 * Fields can be built, composed and evaluated at run-time. They are stored in a directed tree
 * graph data structure, whereby each node is a #FieldNode and edges are dependencies. A #FieldNode
 * has an arbitrary number of inputs and at least one output and a #Field references a specific
 * output of a #FieldNode. The inputs of a #FieldNode are other fields.
 *
 * There are two different types of field nodes:
 *  - #FieldInput: Has no input and exactly one output. It represents an input to the entire field
 *    when it is evaluated. During evaluation, the value of this input is based on a #FieldContext.
 *  - #FieldOperation: Has an arbitrary number of field inputs and at least one output. Its main
 *    use is to compose multiple existing fields into new fields.
 *
 * When fields are evaluated, they are converted into a multi-function procedure which allows
 * efficient computation. In the future, we might support different field evaluation mechanisms for
 * e.g. the following scenarios:
 *  - Latency of a single evaluation is more important than throughput.
 *  - Evaluation should happen on other hardware like GPUs.
 *
 * Whenever possible, multiple fields should be evaluated together to avoid duplicate work when
 * they share common sub-fields and a common context.
 */

#include <iostream>

#include "BLI_function_ref.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_vector_set.hh"

#include "FN_multi_function.hh"

namespace blender::fn {

class FieldInput;
struct FieldInputs;

/**
 * Have a fixed set of base node types, because all code that works with field nodes has to
 * understand those.
 */
enum class FieldNodeType {
  Input,
  Operation,
  Constant,
};

/**
 * A node in a field-tree. It has at least one output that can be referenced by fields.
 */
class FieldNode {
 private:
  FieldNodeType node_type_;

 protected:
  /**
   * Keeps track of the inputs that this node depends on. This avoids recomputing it every time the
   * data is required. It is a shared pointer, because very often multiple nodes depend on the same
   * inputs.
   * Might contain null.
   */
  std::shared_ptr<const FieldInputs> field_inputs_;

 public:
  FieldNode(FieldNodeType node_type);
  virtual ~FieldNode();

  virtual const CPPType &output_cpp_type(int output_index) const = 0;

  FieldNodeType node_type() const;
  bool depends_on_input() const;

  const std::shared_ptr<const FieldInputs> &field_inputs() const;

  virtual uint64_t hash() const;
  virtual bool is_equal_to(const FieldNode &other) const;

  /**
   * Calls the callback for every field input that the current field depends on. This is recursive,
   * so if a field input depends on other field inputs, those are taken into account as well.
   */
  virtual void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const;
};

/**
 * Common base class for fields to avoid declaring the same methods for #GField and #GFieldRef.
 */
template<typename NodePtr> class GFieldBase {
 protected:
  NodePtr node_ = nullptr;
  int node_output_index_ = 0;

  GFieldBase(NodePtr node, const int node_output_index)
      : node_(std::move(node)), node_output_index_(node_output_index)
  {
  }

 public:
  GFieldBase() = default;

  operator bool() const
  {
    return node_ != nullptr;
  }

  friend bool operator==(const GFieldBase &a, const GFieldBase &b)
  {
    /* Two nodes can compare equal even when their pointer is not the same. For example, two
     * "Position" nodes are the same. */
    return *a.node_ == *b.node_ && a.node_output_index_ == b.node_output_index_;
  }

  uint64_t hash() const
  {
    return get_default_hash(*node_, node_output_index_);
  }

  const CPPType &cpp_type() const
  {
    return node_->output_cpp_type(node_output_index_);
  }

  const FieldNode &node() const
  {
    return *node_;
  }

  int node_output_index() const
  {
    return node_output_index_;
  }
};

/**
 * A field whose output type is only known at run-time.
 */
class GField : public GFieldBase<std::shared_ptr<FieldNode>> {
 public:
  GField() = default;

  GField(std::shared_ptr<FieldNode> node, const int node_output_index = 0)
      : GFieldBase<std::shared_ptr<FieldNode>>(std::move(node), node_output_index)
  {
  }
};

/**
 * Same as #GField but is cheaper to copy/move around, because it does not contain a
 * #std::shared_ptr.
 */
class GFieldRef : public GFieldBase<const FieldNode *> {
 public:
  GFieldRef() = default;

  GFieldRef(const GField &field)
      : GFieldBase<const FieldNode *>(&field.node(), field.node_output_index())
  {
  }

  GFieldRef(const FieldNode &node, const int node_output_index = 0)
      : GFieldBase<const FieldNode *>(&node, node_output_index)
  {
  }
};

namespace detail {
/* Utility class to make #is_field_v work. */
struct TypedFieldBase {};
}  // namespace detail

/**
 * A typed version of #GField. It has the same memory layout as #GField.
 */
template<typename T> class Field : public GField, detail::TypedFieldBase {
 public:
  using base_type = T;

  Field() = default;

  Field(GField field) : GField(std::move(field))
  {
    BLI_assert(this->cpp_type().template is<T>());
  }

  /**
   * Generally, the constructor above would be sufficient, but this additional constructor ensures
   * that trying to create e.g. a `Field<int>` from a `Field<float>` does not compile (instead of
   * only failing at run-time).
   */
  template<typename U> Field(Field<U> field) : GField(std::move(field))
  {
    static_assert(std::is_same_v<T, U>);
  }

  Field(std::shared_ptr<FieldNode> node, const int node_output_index = 0)
      : Field(GField(std::move(node), node_output_index))
  {
  }
};

/** True when T is any Field<...> type. */
template<typename T>
static constexpr bool is_field_v = std::is_base_of_v<detail::TypedFieldBase, T> &&
                                   !std::is_same_v<detail::TypedFieldBase, T>;

/**
 * A #FieldNode that allows composing existing fields into new fields.
 */
class FieldOperation : public FieldNode {
  /**
   * The multi-function used by this node. It is optionally owned.
   * Multi-functions with mutable or vector parameters are not supported currently.
   */
  std::shared_ptr<const mf::MultiFunction> owned_function_;
  const mf::MultiFunction *function_;

  /** Inputs to the operation. */
  blender::Vector<GField> inputs_;

 public:
  FieldOperation(std::shared_ptr<const mf::MultiFunction> function, Vector<GField> inputs = {});
  FieldOperation(const mf::MultiFunction &function, Vector<GField> inputs = {});
  ~FieldOperation();

  Span<GField> inputs() const;
  const mf::MultiFunction &multi_function() const;

  const CPPType &output_cpp_type(int output_index) const override;

  static std::shared_ptr<FieldOperation> Create(std::shared_ptr<const mf::MultiFunction> function,
                                                Vector<GField> inputs = {})
  {
    return std::make_shared<FieldOperation>(FieldOperation(std::move(function), inputs));
  }
  static std::shared_ptr<FieldOperation> Create(const mf::MultiFunction &function,
                                                Vector<GField> inputs = {})
  {
    return std::make_shared<FieldOperation>(FieldOperation(function, inputs));
  }
};

class FieldContext;

/**
 * A #FieldNode that represents an input to the entire field-tree.
 */
class FieldInput : public FieldNode {
 public:
  /* The order is also used for sorting in socket inspection. */
  enum class Category {
    NamedAttribute = 0,
    Generated = 1,
    AnonymousAttribute = 2,
    Unknown,
  };

 protected:
  const CPPType *type_;
  std::string debug_name_;
  Category category_ = Category::Unknown;

 public:
  FieldInput(const CPPType &type, std::string debug_name = "");
  ~FieldInput();

  /**
   * Get the value of this specific input based on the given context. The returned virtual array,
   * should live at least as long as the passed in #scope. May return null.
   */
  virtual GVArray get_varray_for_context(const FieldContext &context,
                                         const IndexMask &mask,
                                         ResourceScope &scope) const = 0;

  virtual std::string socket_inspection_name() const;
  blender::StringRef debug_name() const;
  const CPPType &cpp_type() const;
  Category category() const;

  const CPPType &output_cpp_type(int output_index) const override;
};

class FieldConstant : public FieldNode {
 private:
  const CPPType &type_;
  void *value_;

 public:
  FieldConstant(const CPPType &type, const void *value);
  ~FieldConstant();

  const CPPType &output_cpp_type(int output_index) const override;
  const CPPType &type() const;
  GPointer value() const;
};

/**
 * Keeps track of the inputs of a field.
 */
struct FieldInputs {
  /** All #FieldInput nodes that a field (possibly indirectly) depends on. */
  VectorSet<const FieldInput *> nodes;
  /**
   * Same as above but the inputs are deduplicated. For example, when there are two separate index
   * input nodes, only one will show up in this list.
   */
  VectorSet<std::reference_wrapper<const FieldInput>> deduplicated_nodes;
};

/**
 * Provides inputs for a specific field evaluation.
 */
class FieldContext {
 public:
  virtual ~FieldContext() = default;

  virtual GVArray get_varray_for_input(const FieldInput &field_input,
                                       const IndexMask &mask,
                                       ResourceScope &scope) const;
};

/**
 * Utility class that makes it easier to evaluate fields.
 */
class FieldEvaluator : NonMovable, NonCopyable {
  struct OutputPointerInfo {
    void *dst = nullptr;
    /* When a destination virtual array is provided for an input, this is
     * unnecessary, otherwise this is used to construct the required virtual array. */
    void (*set)(void *dst, const GVArray &varray, ResourceScope &scope) = nullptr;
  };

  ResourceScope scope_;
  const FieldContext &context_;
  const IndexMask &mask_;
  Vector<GField> fields_to_evaluate_;
  Vector<GVMutableArray> dst_varrays_;
  Vector<GVArray> evaluated_varrays_;
  Vector<OutputPointerInfo> output_pointer_infos_;
  bool is_evaluated_ = false;

  Field<bool> selection_field_;
  IndexMask selection_mask_;

 public:
  /** Takes #mask by pointer because the mask has to live longer than the evaluator. */
  FieldEvaluator(const FieldContext &context, const IndexMask *mask)
      : context_(context), mask_(*mask)
  {
  }

  /** Construct a field evaluator for all indices less than #size. */
  FieldEvaluator(const FieldContext &context, const int64_t size)
      : context_(context), mask_(scope_.construct<IndexMask>(size))
  {
  }

  ~FieldEvaluator()
  {
    /* While this assert isn't strictly necessary, and could be replaced with a warning,
     * it will catch cases where someone forgets to call #evaluate(). */
    BLI_assert(is_evaluated_);
  }

  /**
   * The selection field is evaluated first to determine which indices of the other fields should
   * be evaluated. Calling this method multiple times will just replace the previously set
   * selection field. Only the elements selected by both this selection and the selection provided
   * in the constructor are calculated. If no selection field is set, it is assumed that all
   * indices passed to the constructor are selected.
   */
  void set_selection(Field<bool> selection)
  {
    selection_field_ = std::move(selection);
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable virtual array that the evaluated result for this field is be written into.
   */
  int add_with_destination(GField field, GVMutableArray dst);

  /** Same as #add_with_destination but typed. */
  template<typename T> int add_with_destination(Field<T> field, VMutableArray<T> dst)
  {
    return this->add_with_destination(GField(std::move(field)), GVMutableArray(std::move(dst)));
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable span that the evaluated result for this field is be written into.
   * \note When the output may only be used as a single value, the version of this function with
   * a virtual array result array should be used.
   */
  int add_with_destination(GField field, GMutableSpan dst);

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable span that the evaluated result for this field is be written into.
   * \note When the output may only be used as a single value, the version of this function with
   * a virtual array result array should be used.
   */
  template<typename T> int add_with_destination(Field<T> field, MutableSpan<T> dst)
  {
    return this->add_with_destination(std::move(field), VMutableArray<T>::ForSpan(dst));
  }

  int add(GField field, GVArray *varray_ptr);

  /**
   * \param field: Field to add to the evaluator.
   * \param varray_ptr: Once #evaluate is called, the resulting virtual array will be will be
   *   assigned to the given position.
   * \return Index of the field in the evaluator which can be used in the #get_evaluated methods.
   */
  template<typename T> int add(Field<T> field, VArray<T> *varray_ptr)
  {
    const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
    dst_varrays_.append({});
    output_pointer_infos_.append(OutputPointerInfo{
        varray_ptr, [](void *dst, const GVArray &varray, ResourceScope & /*scope*/) {
          *(VArray<T> *)dst = varray.typed<T>();
        }});
    return field_index;
  }

  /**
   * \return Index of the field in the evaluator which can be used in the #get_evaluated methods.
   */
  int add(GField field);

  /**
   * Evaluate all fields on the evaluator. This can only be called once.
   */
  void evaluate();

  const GVArray &get_evaluated(const int field_index) const
  {
    BLI_assert(is_evaluated_);
    return evaluated_varrays_[field_index];
  }

  template<typename T> VArray<T> get_evaluated(const int field_index)
  {
    return this->get_evaluated(field_index).typed<T>();
  }

  IndexMask get_evaluated_selection_as_mask();

  /**
   * Retrieve the output of an evaluated boolean field and convert it to a mask, which can be used
   * to avoid calculations for unnecessary elements later on. The evaluator will own the indices in
   * some cases, so it must live at least as long as the returned mask.
   */
  IndexMask get_evaluated_as_mask(int field_index);
};

/**
 * Evaluate fields in the given context. If possible, multiple fields should be evaluated together,
 * because that can be more efficient when they share common sub-fields.
 *
 * \param scope: The resource scope that owns data that makes up the output virtual arrays. Make
 *   sure the scope is not destructed when the output virtual arrays are still used.
 * \param fields_to_evaluate: The fields that should be evaluated together.
 * \param mask: Determines which indices are computed. The mask may be referenced by the returned
 *   virtual arrays. So the underlying indices (if applicable) should live longer then #scope.
 * \param context: The context that the field is evaluated in. Used to retrieve data from each
 *   #FieldInput in the field network.
 * \param dst_varrays: If provided, the computed data will be written into those virtual arrays
 *   instead of into newly created ones. That allows making the computed data live longer than
 *   #scope and is more efficient when the data will be written into those virtual arrays
 *   later anyway.
 * \return The computed virtual arrays for each provided field. If #dst_varrays is passed, the
 *   provided virtual arrays are returned.
 */
Vector<GVArray> evaluate_fields(ResourceScope &scope,
                                Span<GFieldRef> fields_to_evaluate,
                                const IndexMask &mask,
                                const FieldContext &context,
                                Span<GVMutableArray> dst_varrays = {});

/* -------------------------------------------------------------------- */
/** \name Utility functions for simple field creation and evaluation
 * \{ */

void evaluate_constant_field(const GField &field, void *r_value);

template<typename T> T evaluate_constant_field(const Field<T> &field)
{
  T value;
  value.~T();
  evaluate_constant_field(field, &value);
  return value;
}

Field<bool> invert_boolean_field(const Field<bool> &field);

GField make_constant_field(const CPPType &type, const void *value);

template<typename T> Field<T> make_constant_field(T value)
{
  return make_constant_field(CPPType::get<T>(), &value);
}

/**
 * If the field depends on some input, the same field is returned.
 * Otherwise the field is evaluated and a new field is created that just computes this constant.
 *
 * Making the field constant has two benefits:
 * - The field-tree becomes a single node, which is more efficient when the field is evaluated many
 *   times.
 * - Memory of the input fields may be freed.
 */
GField make_field_constant_if_possible(GField field);

class IndexFieldInput final : public FieldInput {
 public:
  IndexFieldInput();

  static GVArray get_index_varray(const IndexMask &mask);

  GVArray get_varray_for_context(const FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const final;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldNode Inline Methods
 * \{ */

inline FieldNode::FieldNode(const FieldNodeType node_type) : node_type_(node_type) {}

inline FieldNodeType FieldNode::node_type() const
{
  return node_type_;
}

inline bool FieldNode::depends_on_input() const
{
  return field_inputs_ && !field_inputs_->nodes.is_empty();
}

inline const std::shared_ptr<const FieldInputs> &FieldNode::field_inputs() const
{
  return field_inputs_;
}

inline uint64_t FieldNode::hash() const
{
  return get_default_hash(this);
}

inline bool FieldNode::is_equal_to(const FieldNode &other) const
{
  return this == &other;
}

inline bool operator==(const FieldNode &a, const FieldNode &b)
{
  return a.is_equal_to(b);
}

inline bool operator!=(const FieldNode &a, const FieldNode &b)
{
  return !(a == b);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldOperation Inline Methods
 * \{ */

inline Span<GField> FieldOperation::inputs() const
{
  return inputs_;
}

inline const mf::MultiFunction &FieldOperation::multi_function() const
{
  return *function_;
}

inline const CPPType &FieldOperation::output_cpp_type(int output_index) const
{
  int output_counter = 0;
  for (const int param_index : function_->param_indices()) {
    mf::ParamType param_type = function_->param_type(param_index);
    if (param_type.is_output()) {
      if (output_counter == output_index) {
        return param_type.data_type().single_type();
      }
      output_counter++;
    }
  }
  BLI_assert_unreachable();
  return CPPType::get<float>();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldInput Inline Methods
 * \{ */

inline std::string FieldInput::socket_inspection_name() const
{
  return debug_name_;
}

inline StringRef FieldInput::debug_name() const
{
  return debug_name_;
}

inline const CPPType &FieldInput::cpp_type() const
{
  return *type_;
}

inline FieldInput::Category FieldInput::category() const
{
  return category_;
}

inline const CPPType &FieldInput::output_cpp_type(int output_index) const
{
  BLI_assert(output_index == 0);
  UNUSED_VARS_NDEBUG(output_index);
  return *type_;
}

/** \} */

}  // namespace blender::fn

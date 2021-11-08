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
 *
 * A #Field represents a function that outputs a value based on an arbitrary number of inputs. The
 * inputs for a specific field evaluation are provided by a #FieldContext.
 *
 * A typical example is a field that computes a displacement vector for every vertex on a mesh
 * based on its position.
 *
 * Fields can be build, composed and evaluated at run-time. They are stored in a directed tree
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

#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "FN_generic_virtual_array.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_procedure.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"

namespace blender::fn {

class FieldInput;

/**
 * A node in a field-tree. It has at least one output that can be referenced by fields.
 */
class FieldNode {
 private:
  bool is_input_;
  /**
   * True when this node is a #FieldInput or (potentially indirectly) depends on one. This could
   * always be derived again later by traversing the field-tree, but keeping track of it while the
   * field is built is cheaper.
   *
   * If this is false, the field is constant. Note that even when this is true, the field may be
   * constant when all inputs are constant.
   */
  bool depends_on_input_;

 public:
  FieldNode(bool is_input, bool depends_on_input);

  virtual ~FieldNode() = default;

  virtual const CPPType &output_cpp_type(int output_index) const = 0;

  bool is_input() const;
  bool is_operation() const;
  bool depends_on_input() const;

  /**
   * Invoke callback for every field input. It might be called multiple times for the same input.
   * The caller is responsible for deduplication if required.
   */
  virtual void foreach_field_input(FunctionRef<void(const FieldInput &)> foreach_fn) const = 0;

  virtual uint64_t hash() const;
  virtual bool is_equal_to(const FieldNode &other) const;
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
    return get_default_hash_2(*node_, node_output_index_);
  }

  const fn::CPPType &cpp_type() const
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

/**
 * A typed version of #GField. It has the same memory layout as #GField.
 */
template<typename T> class Field : public GField {
 public:
  Field() = default;

  Field(GField field) : GField(std::move(field))
  {
    BLI_assert(this->cpp_type().template is<T>());
  }

  Field(std::shared_ptr<FieldNode> node, const int node_output_index = 0)
      : Field(GField(std::move(node), node_output_index))
  {
  }
};

/**
 * A #FieldNode that allows composing existing fields into new fields.
 */
class FieldOperation : public FieldNode {
  /**
   * The multi-function used by this node. It is optionally owned.
   * Multi-functions with mutable or vector parameters are not supported currently.
   */
  std::shared_ptr<const MultiFunction> owned_function_;
  const MultiFunction *function_;

  /** Inputs to the operation. */
  blender::Vector<GField> inputs_;

 public:
  FieldOperation(std::shared_ptr<const MultiFunction> function, Vector<GField> inputs = {});
  FieldOperation(const MultiFunction &function, Vector<GField> inputs = {});

  Span<GField> inputs() const;
  const MultiFunction &multi_function() const;

  const CPPType &output_cpp_type(int output_index) const override;
  void foreach_field_input(FunctionRef<void(const FieldInput &)> foreach_fn) const override;
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

  /**
   * Get the value of this specific input based on the given context. The returned virtual array,
   * should live at least as long as the passed in #scope. May return null.
   */
  virtual const GVArray *get_varray_for_context(const FieldContext &context,
                                                IndexMask mask,
                                                ResourceScope &scope) const = 0;

  virtual std::string socket_inspection_name() const;
  blender::StringRef debug_name() const;
  const CPPType &cpp_type() const;
  Category category() const;

  const CPPType &output_cpp_type(int output_index) const override;
  void foreach_field_input(FunctionRef<void(const FieldInput &)> foreach_fn) const override;
};

/**
 * Provides inputs for a specific field evaluation.
 */
class FieldContext {
 public:
  ~FieldContext() = default;

  virtual const GVArray *get_varray_for_input(const FieldInput &field_input,
                                              IndexMask mask,
                                              ResourceScope &scope) const;
};

/**
 * Utility class that makes it easier to evaluate fields.
 */
class FieldEvaluator : NonMovable, NonCopyable {
 private:
  struct OutputPointerInfo {
    void *dst = nullptr;
    /* When a destination virtual array is provided for an input, this is
     * unnecessary, otherwise this is used to construct the required virtual array. */
    void (*set)(void *dst, const GVArray &varray, ResourceScope &scope) = nullptr;
  };

  ResourceScope scope_;
  const FieldContext &context_;
  const IndexMask mask_;
  Vector<GField> fields_to_evaluate_;
  Vector<GVMutableArray *> dst_varrays_;
  Vector<const GVArray *> evaluated_varrays_;
  Vector<OutputPointerInfo> output_pointer_infos_;
  bool is_evaluated_ = false;

 public:
  /** Takes #mask by pointer because the mask has to live longer than the evaluator. */
  FieldEvaluator(const FieldContext &context, const IndexMask *mask)
      : context_(context), mask_(*mask)
  {
  }

  /** Construct a field evaluator for all indices less than #size. */
  FieldEvaluator(const FieldContext &context, const int64_t size) : context_(context), mask_(size)
  {
  }

  ~FieldEvaluator()
  {
    /* While this assert isn't strictly necessary, and could be replaced with a warning,
     * it will catch cases where someone forgets to call #evaluate(). */
    BLI_assert(is_evaluated_);
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable virtual array that the evaluated result for this field is be written into.
   */
  int add_with_destination(GField field, GVMutableArray &dst);

  /** Same as #add_with_destination but typed. */
  template<typename T> int add_with_destination(Field<T> field, VMutableArray<T> &dst)
  {
    GVMutableArray &varray = scope_.construct<GVMutableArray_For_VMutableArray<T>>(dst);
    return this->add_with_destination(GField(std::move(field)), varray);
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable span that the evaluated result for this field is be written into.
   * \note: When the output may only be used as a single value, the version of this function with
   * a virtual array result array should be used.
   */
  int add_with_destination(GField field, GMutableSpan dst);

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable span that the evaluated result for this field is be written into.
   * \note: When the output may only be used as a single value, the version of this function with
   * a virtual array result array should be used.
   */
  template<typename T> int add_with_destination(Field<T> field, MutableSpan<T> dst)
  {
    GVMutableArray &varray = scope_.construct<GVMutableArray_For_MutableSpan<T>>(dst);
    return this->add_with_destination(std::move(field), varray);
  }

  int add(GField field, const GVArray **varray_ptr);

  /**
   * \param field: Field to add to the evaluator.
   * \param varray_ptr: Once #evaluate is called, the resulting virtual array will be will be
   *   assigned to the given position.
   * \return Index of the field in the evaluator which can be used in the #get_evaluated methods.
   */
  template<typename T> int add(Field<T> field, const VArray<T> **varray_ptr)
  {
    const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
    dst_varrays_.append(nullptr);
    output_pointer_infos_.append(
        OutputPointerInfo{varray_ptr, [](void *dst, const GVArray &varray, ResourceScope &scope) {
                            *(const VArray<T> **)dst = &*scope.construct<GVArray_Typed<T>>(varray);
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
    return *evaluated_varrays_[field_index];
  }

  template<typename T> const VArray<T> &get_evaluated(const int field_index)
  {
    const GVArray &varray = this->get_evaluated(field_index);
    GVArray_Typed<T> &typed_varray = scope_.construct<GVArray_Typed<T>>(varray);
    return *typed_varray;
  }

  /**
   * Retrieve the output of an evaluated boolean field and convert it to a mask, which can be used
   * to avoid calculations for unnecessary elements later on. The evaluator will own the indices in
   * some cases, so it must live at least as long as the returned mask.
   */
  IndexMask get_evaluated_as_mask(const int field_index);
};

Vector<const GVArray *> evaluate_fields(ResourceScope &scope,
                                        Span<GFieldRef> fields_to_evaluate,
                                        IndexMask mask,
                                        const FieldContext &context,
                                        Span<GVMutableArray *> dst_varrays = {});

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

template<typename T> Field<T> make_constant_field(T value)
{
  auto constant_fn = std::make_unique<fn::CustomMF_Constant<T>>(std::forward<T>(value));
  auto operation = std::make_shared<FieldOperation>(std::move(constant_fn));
  return Field<T>{GField{std::move(operation), 0}};
}

GField make_field_constant_if_possible(GField field);

class IndexFieldInput final : public FieldInput {
 public:
  IndexFieldInput();

  static GVArray *get_index_varray(IndexMask mask, ResourceScope &scope);

  const GVArray *get_varray_for_context(const FieldContext &context,
                                        IndexMask mask,
                                        ResourceScope &scope) const final;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name #FieldNode Inline Methods
 * \{ */

inline FieldNode::FieldNode(bool is_input, bool depends_on_input)
    : is_input_(is_input), depends_on_input_(depends_on_input)
{
}

inline bool FieldNode::is_input() const
{
  return is_input_;
}

inline bool FieldNode::is_operation() const
{
  return !is_input_;
}

inline bool FieldNode::depends_on_input() const
{
  return depends_on_input_;
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

inline const MultiFunction &FieldOperation::multi_function() const
{
  return *function_;
}

inline const CPPType &FieldOperation::output_cpp_type(int output_index) const
{
  int output_counter = 0;
  for (const int param_index : function_->param_indices()) {
    MFParamType param_type = function_->param_type(param_index);
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

/* SPDX-FileCopyrightText: 2026 Blender Authors
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
 * graph data structure. A field may generally depend on other fields.
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

#include "BLI_cache_mutex.hh"
#include "BLI_implicit_sharing_ptr.hh"

#include "FN_multi_function.hh"

namespace blender::fn {

class GField;
class FieldInput;
class FieldOperation;
class FieldInputs;
class FieldContext;

using FieldInputPtr = ImplicitSharingPtr<FieldInput>;
using FieldOperationPtr = ImplicitSharingPtr<FieldOperation>;
using FieldInputsPtr = ImplicitSharingPtr<FieldInputs>;
template<typename T> class Field;

/**
 * A field with a type that is only known at runtime which can be accessed through the #cpp_type
 * method. If the type is known at compile time, it is recommended to use #Field<T> instead.
 *
 * It is designed to support various internal storage representations to avoid unnecessary
 * allocations or reference counting in many common cases.
 */
class GField {
 public:
  struct Input {
    FieldInputPtr node;
  };

  struct MultiFn {
    FieldOperationPtr node;
    int output_i = 0;
  };

  /**
   * Allows referencing another field without owning it. This helps with fields that are highly
   * reused like the position field because it avoids reference counting..
   */
  struct FieldRef {
    const GField *field_ref = nullptr;
  };

  struct ConstantRef {
    const CPPType *type = nullptr;
    /** This value is not owned. Typically it has static lifetime. */
    const void *value = nullptr;
  };

  /**
   * Allows storing constants inside of #GField without any additional memory allocation.
   */
  struct TrivialInlineConstant {
    static constexpr int64_t inline_size = 16;
    static constexpr int64_t inline_alignment = 8;

    template<typename T>
    static constexpr bool type_supported_v = std::is_trivially_destructible_v<T> &&
                                             std::is_trivially_copyable_v<T> &&
                                             sizeof(T) <= inline_size &&
                                             alignof(T) <= inline_alignment;

    static bool cpp_type_supported(const CPPType &type);

    const CPPType *type = nullptr;
    AlignedBuffer<inline_size, inline_alignment> value;
  };

  /** Used for storing constants that can't be inlined. */
  struct OwnedConstant {
    const CPPType *type = nullptr;
    /* This value is owned by the #GField. */
    void *value = nullptr;
  };

  template<typename T>
  static constexpr bool is_constant_value_v =
      is_same_any_v<T, ConstantRef, TrivialInlineConstant, OwnedConstant>;

  using Variant =
      std::variant<Input, MultiFn, FieldRef, ConstantRef, TrivialInlineConstant, OwnedConstant>;

 private:
  Variant variant_;

 public:
  /**
   * #GField is expected to always have a valid #CPPType. Therefore, it can't be default
   * constructed.
   */
  GField() = delete;
  /** Construct a field that just outputs the default value of the given type. */
  explicit GField(const CPPType &type) noexcept;
  /** Construct a field owning a field input. */
  explicit GField(FieldInputPtr node) noexcept;
  /** Construct a field that owns a field operation and outputs one of its outputs. */
  explicit GField(FieldOperationPtr node, int output_i = 0) noexcept;
  /** Construct directly from a #Variant, mostly for internal use. */
  explicit GField(Variant variant) noexcept;

  /**
   * Wraps the given field in a new field. This is used to avoid reference counting for some field
   * fields which have static lifetime.
   */
  static GField from_non_owning_ref(const GField &field);

  /** Construct a field that just outputs the given constant value. */
  static GField from_constant(const CPPType &type, const void *value);

  /** Construct a field that just outputs the given constant value without owning it. */
  static GField from_non_owning_constant(const CPPType &type, const void *value);

  /** Build a new #FieldInput with the given arguments. */
  template<typename InputT, typename... Args> static GField from_input(Args &&...args);

  /**
   * #GField requires manual memory management due to inlined values and to support move semantics
   * without making #GField nullable.
   */
  GField(const GField &other);
  GField(GField &&other) noexcept;
  GField &operator=(const GField &other);
  GField &operator=(GField &&other) noexcept;
  ~GField();

  /** The value type the field outputs for each element, e.g. float. */
  const CPPType &cpp_type() const;

  /** Root #FieldInput nodes that this field depends on. */
  const FieldInputsPtr &field_inputs() const;

  /**
   * This "normalizes" the field. Specifically, if this field is just a non-owning reference to
   * some other field, the referenced field is returned.
   */
  const GField &deref_field_ref() const;

  /** Get the underlying #Variant. */
  const Variant &variant() const;

  /** Returns true when the field depends on some input. */
  bool depends_on_input() const;

  /** Utility to access a specific input type if this field is just an input. */
  template<typename InputT> const InputT *get_input_if() const;

  /**
   * This only implements shallow comparison. A more deep comparison could reveal that two fields
   * are semantically the same even if this comparison is false. Deep comparison is much more
   * expensive though.
   */
  friend bool operator==(const GField &a, const GField &b);
  uint64_t hash() const;

  /**
   * Get a typed reference to this field. Note that #Field<T> happens to be identical to #GField on
   * a bit-level. So this is just a cast.
   */
  template<typename T> const Field<T> &typed() const;
  template<typename T> Field<T> &typed();
};

/** A version of #GField that should be used when the field type is known at compile time. */
template<typename T> class Field {
 public:
  using base_type = T;
  using generic_type = GField;

 private:
  /**
   * #Field<T> just stores a #GField. This makes converting between the two types easy.
   */
  GField field_;

  friend GField;

 public:
  /**
   * Unlike #GField, default construction is allowed here, because the type is known without extra
   * arguments.
   */
  Field();

  /** Same as corresponding #GField constructors. */
  explicit Field(FieldInputPtr node);
  explicit Field(FieldOperationPtr node, int output_i = 0);

  /** Construct a field that just outputs the given value. */
  explicit Field(T value);

  /** This is implicitly cast to #GField which is always valid. */
  operator const GField &() const;

  /** These are the same as the corresponding #GField methods. */
  bool depends_on_input() const;
  template<typename InputT, typename... Args> static Field from_input(Args &&...args);
  template<typename InputT> const InputT *get_input_if() const;
  uint64_t hash() const;
  static Field from_non_owning_ref(const Field &field);
};

/**
 * A version of #GField that only references data from other fields but does not own any data
 * itself. This allows it to be smaller and trivially copyable making it more efficient in some
 * contexts. This is mainly used during field evaluation.
 */
class GFieldRef {
 public:
  struct Value {
    const CPPType *type = nullptr;
    const void *value = nullptr;
  };
  struct Input {
    const FieldInput *node = nullptr;
  };
  struct MultiFn {
    const FieldOperation *node = nullptr;
    int output_i = 0;
  };

  using Variant = std::variant<Value, Input, MultiFn>;

 private:
  Variant variant_;

 public:
  /**
   * Create a reference to the given fields. The caller is responsible for making sure that the
   * referenced data stays valid.
   */
  GFieldRef(const GField &field);
  template<typename T> GFieldRef(const Field<T> &field);
  explicit GFieldRef(const FieldInput &field_input);
  explicit GFieldRef(const FieldOperation &field_multi_fn, int output_i = 0);

  /** Get access to the underlying #Variant. */
  const Variant &variant() const;

  /** These are the same as the corresponding #GField methods. */
  const CPPType &cpp_type() const;
  const FieldInputsPtr &field_inputs() const;
  uint64_t hash() const;
};

/**
 * A field is always evaluated in some context. This context determines the value of the field
 * inputs.
 */
class FieldContext {
 public:
  virtual ~FieldContext() = default;

  virtual GVArray get_varray_for_input(const FieldInput &field_input,
                                       const IndexMask &mask,
                                       ResourceScope &scope) const;
};

/**
 * "Deep" hashing for fields that considers the operation and inputs semantically, rather than
 * just the shallow data (i.e. memory address) of the field data, like the default "hash()"
 * implementation. Because common field reuse would give this potentially exponential cost, this
 * struct caches the hashes of intermediate fields.
 */
struct FieldHashDeep {
  Map<GFieldRef, UniqueHash> cache;
  UniqueHash ensure(const GFieldRef &field);
  UniqueHash lookup(const GFieldRef &field) const
  {
    return this->cache.lookup(field);
  }
  bool contains(const GFieldRef &field) const
  {
    return this->cache.contains(field);
  }
};

/**
 * Cache of field inputs. This is used quite often and is therefore computed eagerly for
 * intermediate operations. Otherwise one would have to parse the field tree every time the set of
 * inputs is required. Since many fields share the same set of inputs, this is often shared.
 */
class FieldInputs : public ImplicitSharingMixin {
 public:
  /** Deduplicated set of field inputs. */
  VectorSet<std::reference_wrapper<const FieldInput>> inputs;

  void delete_self() override;
};

/**
 * This is an abstract class which concrete field inputs have to derive from. When a field is
 * evaluated, this can provide values based on the provided context.
 *
 * Since there is no better way yet, #FieldInput is also often used to process the output of
 * intermediate fields, in which case this is not technically an "input".
 */
class FieldInput : public ImplicitSharingMixin {
 protected:
  const CPPType *type_;
  std::string debug_name_;

  /**
   * Field inputs are initialized lazily because it can't be done in the constructor because the
   * derived class constructor has not run yet.
   */
  mutable CacheMutex field_inputs_mutex_;
  mutable FieldInputsPtr field_inputs_;

 public:
  FieldInput(const CPPType &type, std::string debug_name = "");
  ~FieldInput() override;

  StringRefNull debug_name() const;
  virtual std::string socket_inspection_name() const;

  const CPPType &cpp_type() const;

  const FieldInputsPtr &field_inputs() const;

  uint64_t hash() const;
  virtual void hash_unique(UniqueHashBytes &hash, FieldHashDeep &deep_hash_cache) const;

  /**
   * If this #FieldInput depends on other fields, this function should be overridden.
   */
  virtual void foreach_recursive_field(FunctionRef<void(const GField &)> fn) const;

  /**
   * Output a virtual array for the given index mask in the given context.
   */
  virtual GVArray get_varray_for_context(const FieldContext &context,
                                         const IndexMask &mask,
                                         ResourceScope &scope) const = 0;

  void delete_self() override;
};

/**
 * This is an intermediate node in a field tree which executes a #MultiFunction on each value. The
 * #MultiFunction can either be owned or just referenced.
 *
 * It also stores a #GField for every input of the multi-function. Other fields may reference
 * individual outputs.
 */
class FieldOperation : public ImplicitSharingMixin {
 private:
  /** One #GField for every input of the multi-function. */
  Vector<GField> inputs_;

  /** Optionally owned multi-function. */
  std::shared_ptr<const mf::MultiFunction> owned_fn_;
  const mf::MultiFunction *fn_;

  /** Cached field inputs. */
  FieldInputsPtr field_inputs_;

 public:
  /** Prefer `from*` constructor functions instead. */
  FieldOperation(std::shared_ptr<const mf::MultiFunction> fn, Vector<GField> inputs);
  FieldOperation(const mf::MultiFunction &fn, Vector<GField> inputs);

  static FieldOperationPtr from(std::shared_ptr<const mf::MultiFunction> fn,
                                Vector<GField> inputs);
  static FieldOperationPtr from(const mf::MultiFunction &fn, Vector<GField> inputs);

  /** Get the type of a specific output. */
  const CPPType &output_cpp_type(int output_i) const;

  const mf::MultiFunction &multi_function() const;
  const FieldInputsPtr &field_inputs() const;
  Span<GField> inputs() const;

  void delete_self() override;
};

bool operator==(const GField &a, const GField &b);
bool operator==(const GFieldRef &a, const GFieldRef &b);

/** Type trait to detect field types. */
template<typename T> constexpr bool is_field_v = false;
template<typename T> constexpr bool is_field_v<Field<T>> = true;

Field<bool> invert_boolean_field(const Field<bool> &field);

class IndexFieldInput final : public FieldInput {
 public:
  IndexFieldInput();

  static GVArray get_index_varray(const IndexMask &mask);

  GVArray get_varray_for_context(const FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const final;

  void hash_unique(UniqueHashBytes &hash, FieldHashDeep &deep_hash_cache) const override;

  /** Cached index field to avoid allocating a new one every time. */
  static const Field<int> &get_field();
};

/* -------------------------------------------------------------------- */
/** \name Inline Methods
 * \{ */

inline GField::GField(const CPPType &type) noexcept
    : variant_(ConstantRef{&type, type.default_value()})
{
}
inline GField::GField(FieldInputPtr node) noexcept : variant_(Input{std::move(node)}) {}
inline GField::GField(Variant variant) noexcept : variant_(std::move(variant)) {}
inline GField::GField(FieldOperationPtr node, const int output_i) noexcept
    : variant_(MultiFn{std::move(node), output_i})
{
}

inline GField GField::from_non_owning_ref(const GField &field)
{
  return GField(FieldRef{&field});
}

inline bool GField::TrivialInlineConstant::cpp_type_supported(const CPPType &type)
{
  return type.is_trivial && type.size <= TrivialInlineConstant::inline_size &&
         type.alignment <= TrivialInlineConstant::inline_alignment;
}

inline GField GField::from_non_owning_constant(const CPPType &type, const void *value)
{
  return GField(ConstantRef{&type, value});
}

template<typename T> inline Field<T> Field<T>::from_non_owning_ref(const Field &field)
{
  return GField::from_non_owning_ref(field).template typed<T>();
}

template<typename InputT, typename... Args> inline GField GField::from_input(Args &&...args)
{
  FieldInputPtr input{MEM_new<InputT>(__func__, std::forward<Args>(args)...)};
  return GField(Input{std::move(input)});
}

template<typename T>
template<typename InputT, typename... Args>
inline Field<T> Field<T>::from_input(Args &&...args)
{
  return GField::from_input<InputT>(std::forward<Args>(args)...).template typed<T>();
}

template<typename T>
inline Field<T>::Field(T value)
    : field_([&]() {
        const CPPType &type = CPPType::get<T>();
        if constexpr (GField::TrivialInlineConstant::type_supported_v<T>) {
          GField::TrivialInlineConstant constant;
          constant.type = &type;
          new (constant.value.ptr()) T(std::move(value));
          return GField(constant);
        }
        else {
          T *new_value = MEM_new<T>(__func__, std::move(new_value));
          return GField(GField::OwnedConstant{&type, new_value});
        }
      }())
{
}

template<typename T> inline bool Field<T>::depends_on_input() const
{
  return field_.depends_on_input();
}

inline const CPPType &GField::cpp_type() const
{
  return std::visit(
      []<typename T>(const T &v) -> const CPPType & {
        if constexpr (std::is_same_v<T, Input>) {
          return v.node->cpp_type();
        }
        else if constexpr (std::is_same_v<T, MultiFn>) {
          return v.node->output_cpp_type(v.output_i);
        }
        else if constexpr (std::is_same_v<T, FieldRef>) {
          return v.field_ref->cpp_type();
        }
        else if constexpr (is_same_any_v<T, ConstantRef, TrivialInlineConstant, OwnedConstant>) {
          return *v.type;
        }
      },
      this->variant_);
}

inline const GField &GField::deref_field_ref() const
{
  if (const auto *field_ref = std::get_if<FieldRef>(&this->variant_)) {
    return field_ref->field_ref->deref_field_ref();
  }
  return *this;
}

template<typename T> inline bool operator==(const Field<T> &a, const Field<T> &b)
{
  return static_cast<const GField &>(a) == static_cast<const GField &>(b);
}

template<typename T> inline uint64_t Field<T>::hash() const
{
  return field_.hash();
}

inline const CPPType &FieldInput::cpp_type() const
{
  return *this->type_;
}

inline const FieldInputsPtr &FieldOperation::field_inputs() const
{
  return field_inputs_;
}

inline StringRefNull FieldInput::debug_name() const
{
  return debug_name_;
}

inline std::string FieldInput::socket_inspection_name() const
{
  return debug_name_;
}

template<typename T> inline Field<T>::operator const GField &() const
{
  return field_;
}

template<typename T> inline const Field<T> &GField::typed() const
{
  static_assert(sizeof(GField) == sizeof(Field<T>));
  BLI_assert(this->cpp_type().is<T>());
  return reinterpret_cast<const Field<T> &>(*this);
}

template<typename T> inline Field<T> &GField::typed()
{
  static_assert(sizeof(GField) == sizeof(Field<T>));
  BLI_assert(this->cpp_type().is<T>());
  return reinterpret_cast<Field<T> &>(*this);
}

inline const GField::Variant &GField::variant() const
{
  return variant_;
}

template<typename T> inline Field<T>::Field() : field_(CPPType::get<T>()) {}

template<typename T> inline Field<T>::Field(FieldInputPtr node) : field_(GField(std::move(node)))
{
}
template<typename T>
inline Field<T>::Field(FieldOperationPtr node, const int output_i)
    : field_(GField(std::move(node), output_i))
{
}

inline bool GField::depends_on_input() const
{
  const FieldInputsPtr &inputs = this->field_inputs();
  if (!inputs) {
    return false;
  }
  return !inputs->inputs.is_empty();
}

template<typename InputT> inline const InputT *GField::get_input_if() const
{
  const GField &deref_field = this->deref_field_ref();
  if (const auto *input = std::get_if<Input>(&deref_field.variant())) {
    return dynamic_cast<const InputT *>(input->node.get());
  }
  return nullptr;
}

template<typename T> template<typename InputT> inline const InputT *Field<T>::get_input_if() const
{
  return field_.get_input_if<InputT>();
}

inline Span<GField> FieldOperation::inputs() const
{
  return inputs_;
}

inline GFieldRef::GFieldRef(const FieldInput &field_input) : variant_(Input{&field_input}) {}

inline GFieldRef::GFieldRef(const FieldOperation &field_multi_fn, int output_i)
    : variant_(MultiFn{&field_multi_fn, output_i})
{
}

template<typename T>
inline GFieldRef::GFieldRef(const Field<T> &field) : GFieldRef(static_cast<const GField &>(field))
{
}

inline const GFieldRef::Variant &GFieldRef::variant() const
{
  return variant_;
}

inline const CPPType &GFieldRef::cpp_type() const
{
  return std::visit(
      []<typename T>(const T &v) -> const CPPType & {
        if constexpr (std::is_same_v<T, Value>) {
          return *v.type;
        }
        else if constexpr (std::is_same_v<T, Input>) {
          return v.node->cpp_type();
        }
        else if constexpr (std::is_same_v<T, MultiFn>) {
          return v.node->output_cpp_type(v.output_i);
        }
      },
      variant_);
}

inline bool operator==(const FieldInput &a, const FieldInput &b)
{
  return &a == &b;
}

inline const mf::MultiFunction &FieldOperation::multi_function() const
{
  return *this->fn_;
}

/** \} */

}  // namespace blender::fn

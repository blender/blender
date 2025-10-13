/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <variant>

#include "BLI_function_ref.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_memory_counter_fwd.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector_set.hh"

#include "DNA_attribute_types.h"

struct BlendDataReader;
struct BlendWriter;
struct IDTypeForeachColorFunctionCallback;
namespace blender {
class GPointer;
class CPPType;
class ResourceScope;
}  // namespace blender

namespace blender::bke {

enum class AttrDomain : int8_t;
enum class AttrType : int16_t;
enum class AttrStorageType : int8_t;

/** Data and metadata for a single geometry attribute. */
class Attribute {
 public:
  /**
   * Data for an attribute stored as a full contiguous array with a data type exactly matching the
   * attribute's type. The array referenced must match the size of the domain and the data type.
   */
  struct ArrayData {
    /* NOTE: Since the shared data pointed to by `sharing_info` knows how to free itself, it often
     * stores the size and type itself. It may be possible to make use of that fact to avoid
     * storing it here, or even vice versa. */
    void *data;
    /** The number of elements in the array. */
    int64_t size;
    ImplicitSharingPtr<> sharing_info;
    static ArrayData from_value(const GPointer &value, int64_t domain_size);
    static ArrayData from_default_value(const CPPType &type, int64_t domain_size);
    static ArrayData from_uninitialized(const CPPType &type, int64_t domain_size);
    static ArrayData from_constructed(const CPPType &type, int64_t domain_size);
  };
  /** Data for an attribute stored as a single value for the entire domain. */
  struct SingleData {
    /* NOTE: For simplicity and to avoid a bit of redundancy, the domain size isn't stored here.
     * It's not necessary to manage a single value. */
    void *value;
    ImplicitSharingPtr<> sharing_info;
    static SingleData from_value(const GPointer &value);
    static SingleData from_default_value(const CPPType &type);
  };
  using DataVariant = std::variant<ArrayData, SingleData>;
  friend AttributeStorage;

 private:
  /**
   * Because it's used as the custom ID for the attributes vector set, the name cannot be changed
   * without removing and adding the attribute.
   */
  std::string name_;
  AttrDomain domain_;
  AttrType type_;

  DataVariant data_;

 public:
  /**
   * Unique name across all domains.
   * \note Compared to #CustomData, which doesn't enforce uniqueness, across domains on its own,
   * this is enforced by asserts when adding attributes. See #unique_name_calc() (which is also
   * called during the conversion process).
   */
  StringRefNull name() const;

  /** Which part of a geometry the attribute corresponds to. */
  AttrDomain domain() const;

  /**
   * The data type exposed to the user. Depending on the storage type, the actual internal values
   * may not be the same type.
   */
  AttrType data_type() const;

  /**
   * The method used to store the data. This gives flexibility to optimize the internal storage
   * even though conceptually the attribute is an array of values.
   */
  AttrStorageType storage_type() const;

  /**
   * Low level access to the data stored for the attribute. The variant's type will correspond to
   * the storage type.
   */
  const DataVariant &data() const;

  /**
   * The same as #data(), but if the attribute data is shared initially, it will be unshared and
   * made mutable.
   */
  DataVariant &data_for_write();

  /** Replace the attribute's data without first making the existing data mutable. */
  void assign_data(DataVariant &&data);
};

class AttributeStorageRuntime {
  friend AttributeStorage;
  struct AttributeNameGetter {
    StringRef operator()(const std::unique_ptr<Attribute> &value) const
    {
      return value->name();
    }
  };
  /**
   * For quick access, the attributes are stored in a vector set, keyed by their name. Attributes
   * can still be reordered by rebuilding the vector set from scratch. Each attribute is allocated
   * to give pointer stability across additions and removals.
   */
  CustomIDVectorSet<std::unique_ptr<Attribute>, AttributeNameGetter> attributes;
};

class AttributeStorage : public ::AttributeStorage {
 public:
  AttributeStorage();
  AttributeStorage(const AttributeStorage &other);
  AttributeStorage(AttributeStorage &&other);
  AttributeStorage &operator=(const AttributeStorage &other);
  AttributeStorage &operator=(AttributeStorage &&other);
  ~AttributeStorage();

  /**
   * Iterate over all attributes, with the order defined by the order of insertion. It is not safe
   * to add or remove attributes while iterating.
   */
  void foreach(FunctionRef<void(Attribute &)> fn);
  void foreach(FunctionRef<void(const Attribute &)> fn) const;
  void foreach_with_stop(FunctionRef<bool(Attribute &)> fn);
  void foreach_with_stop(FunctionRef<bool(const Attribute &)> fn) const;

  /** Return the number of attributes. */
  int count() const;

  /** Return the attribute at the given index. */
  Attribute &at_index(int index);
  const Attribute &at_index(int index) const;

  /** Return the index of the attribute with the given name, or -1 if not found. */
  int index_of(StringRef name) const;

  /**
   * Try to find the attribute with a given name. The non-const overload does not make the
   * attribute data itself mutable.
   */
  Attribute *lookup(StringRef name);
  const Attribute *lookup(StringRef name) const;

  /**
   * Attempt to remove the attribute with the given name, returning `true` if successful. Should
   * not be called while iterating over attributes.
   */
  bool remove(StringRef name);

  /**
   * Add an attribute with the given name, which must not already be used by an existing attribute
   * or this will invoke undefined behavior.
   */
  Attribute &add(std::string name,
                 bke::AttrDomain domain,
                 bke::AttrType data_type,
                 Attribute::DataVariant data);

  /** Return a possibly changed version of the input name that is unique within existing names. */
  std::string unique_name_calc(StringRef name) const;

  /** Change the name of a single existing attribute. */
  void rename(StringRef old_name, std::string new_name);

  /**
   * Resize the data for a given domain. New values will be default initialized (meaning no zero
   * initialization for trivial types).
   */
  void resize(AttrDomain domain, int64_t new_size);

  /**
   * Read data owned by the #AttributeStorage struct. This works by converting the DNA-specific
   * types stored in the files to the runtime data structures.
   */
  void blend_read(BlendDataReader &reader);
  /**
   * Temporary data used to write a #AttributeStorage struct embedded in another struct. See
   * #attribute_storage_blend_write_prepare for more information.
   */
  struct BlendWriteData {
    ResourceScope &scope;
    Vector<::Attribute, 16> &attributes;
    explicit BlendWriteData(ResourceScope &scope);
  };
  /**
   * Write the prepared data and the data stored in the DNA fields in
   * the #AttributeStorage struct.
   */
  void blend_write(BlendWriter &writer, const BlendWriteData &write_data);

  /**
   * Iterate over every color to change it to another colorspace.
   */
  void foreach_working_space_color(const IDTypeForeachColorFunctionCallback &fn);

  void count_memory(MemoryCounter &memory) const;
};

/** The C++ wrapper needs to be the same size as the DNA struct. */
static_assert(sizeof(AttributeStorage) == sizeof(::AttributeStorage));

inline StringRefNull Attribute::name() const
{
  return name_;
}

inline AttrDomain Attribute::domain() const
{
  return domain_;
}

inline AttrType Attribute::data_type() const
{
  return type_;
}

inline const Attribute::DataVariant &Attribute::data() const
{
  return data_;
}

inline void Attribute::assign_data(DataVariant &&data)
{
  data_ = std::move(data);
}

}  // namespace blender::bke

inline blender::bke::AttributeStorage &AttributeStorage::wrap()
{
  return *reinterpret_cast<blender::bke::AttributeStorage *>(this);
}
inline const blender::bke::AttributeStorage &AttributeStorage::wrap() const
{
  return *reinterpret_cast<const blender::bke::AttributeStorage *>(this);
}

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
 * \ingroup bli
 *
 * An abstraction layer for serialization formats.
 *
 * Allowing to read/write data to a serialization format like JSON.
 *
 *
 *
 * # Supported data types
 *
 * The abstraction layer has a limited set of data types it supports.
 * There are specific classes that builds up the data structure that
 * can be (de)serialized.
 *
 * - StringValue: for strings
 * - IntValue: for integer values
 * - DoubleValue: for double precision floating point numbers
 * - BooleanValue: for boolean values
 * - ArrayValue: An array of any supported value.
 * - ObjectValue: A key value pair where keys are std::string.
 * - NullValue: for null values.
 *
 * # Basic usage
 *
 * ## Serializing
 *
 * - Construct a structure that needs to be serialized using the `*Value` classes.
 * - Construct the formatter you want to use
 * - Invoke the formatter.serialize method passing an output stream and the value.
 *
 * The next example would format an integer value (42) as JSON the result will
 * be stored inside `out`.
 *
 * \code{.cc}
 * JsonFormatter json;
 * std::stringstream out;
 * IntValue test_value(42);
 * json.serialize(out, test_value);
 * \endcode
 *
 * ## Deserializing
 *
 * \code{.cc}
 * std::stringstream is("42");
 * JsonFormatter json;
 * std::unique_ptr<Value> value = json.deserialize(is);
 * \endcode
 *
 * # Adding a new formatter
 *
 * To add a new formatter a new sub-class of `Formatter` must be created and the
 * `serialize`/`deserialize` methods should be implemented.
 *
 */

#include <ostream>

#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

namespace blender::io::serialize {

/**
 * Enumeration containing all sub-classes of Value. It is used as for type checking.
 *
 * \see #Value::type()
 */
enum class eValueType {
  String,
  Int,
  Array,
  Null,
  Boolean,
  Double,
  Object,
};

class Value;
class StringValue;
class ObjectValue;
template<typename T, eValueType V> class PrimitiveValue;
using IntValue = PrimitiveValue<int64_t, eValueType::Int>;
using DoubleValue = PrimitiveValue<double, eValueType::Double>;
using BooleanValue = PrimitiveValue<bool, eValueType::Boolean>;

template<typename Container, typename ContainerItem, eValueType V> class ContainerValue;
/* ArrayValue stores its items as shared pointer as it shares data with a lookup table that can
 * be created by calling `create_lookup`. */
using ArrayValue =
    ContainerValue<Vector<std::shared_ptr<Value>>, std::shared_ptr<Value>, eValueType::Array>;

/**
 * Class containing a (de)serializable value.
 *
 * To serialize from or to a specific format the Value will be used as an intermediate container
 * holding the values. Value class is abstract. There are concrete classes to for different data
 * types.
 *
 * - `StringValue`: contains a string.
 * - `IntValue`: contains an integer.
 * - `ArrayValue`: contains an array of elements. Elements don't need to be the same type.
 * - `NullValue`: represents nothing (null pointer or optional).
 * - `BooleanValue`: contains a boolean (true/false).
 * - `DoubleValue`: contains a double precision floating point number.
 * - `ObjectValue`: represents an object (key value pairs where keys are strings and values can be
 *   of different types.
 *
 */
class Value {
 private:
  eValueType type_;

 protected:
  Value() = delete;
  explicit Value(eValueType type) : type_(type)
  {
  }

 public:
  virtual ~Value() = default;
  const eValueType type() const
  {
    return type_;
  }

  /**
   * Casts to a StringValue.
   * Will return nullptr when it is a different type.
   */
  const StringValue *as_string_value() const;

  /**
   * Casts to an IntValue.
   * Will return nullptr when it is a different type.
   */
  const IntValue *as_int_value() const;

  /**
   * Casts to a DoubleValue.
   * Will return nullptr when it is a different type.
   */
  const DoubleValue *as_double_value() const;

  /**
   * Casts to a BooleanValue.
   * Will return nullptr when it is a different type.
   */
  const BooleanValue *as_boolean_value() const;

  /**
   * Casts to an ArrayValue.
   * Will return nullptr when it is a different type.
   */
  const ArrayValue *as_array_value() const;

  /**
   * Casts to an ObjectValue.
   * Will return nullptr when it is a different type.
   */
  const ObjectValue *as_object_value() const;
};

/**
 * For generating value types that represent types that are typically known processor data types.
 */
template<
    /** Wrapped c/cpp data type that is used to store the value. */
    typename T,
    /** Value type of the class. */
    eValueType V>
class PrimitiveValue : public Value {
 private:
  T inner_value_{};

 public:
  explicit PrimitiveValue(const T value) : Value(V), inner_value_(value)
  {
  }

  const T value() const
  {
    return inner_value_;
  }
};

class NullValue : public Value {
 public:
  NullValue() : Value(eValueType::Null)
  {
  }
};

class StringValue : public Value {
 private:
  std::string string_;

 public:
  StringValue(const StringRef string) : Value(eValueType::String), string_(string)
  {
  }

  const std::string &value() const
  {
    return string_;
  }
};

/**
 * Template for arrays and objects.
 *
 * Both ArrayValue and ObjectValue store their values in an array.
 */
template<
    /** The container type where the elements are stored in. */
    typename Container,

    /** Type of the data inside the container. */
    typename ContainerItem,

    /** ValueType representing the value (object/array). */
    eValueType V>
class ContainerValue : public Value {
 public:
  using Items = Container;
  using Item = ContainerItem;

 private:
  Container inner_value_;

 public:
  ContainerValue() : Value(V)
  {
  }

  const Container &elements() const
  {
    return inner_value_;
  }

  Container &elements()
  {
    return inner_value_;
  }
};

/**
 * Internal storage type for ObjectValue.
 *
 * The elements are stored as an key value pair. The value is a shared pointer so it can be shared
 * when using `ObjectValue::create_lookup`.
 */
using ObjectElementType = std::pair<std::string, std::shared_ptr<Value>>;

/**
 * Object is a key-value container where the key must be a std::string.
 * Internally it is stored in a blender::Vector to ensure the order of keys.
 */
class ObjectValue
    : public ContainerValue<Vector<ObjectElementType>, ObjectElementType, eValueType::Object> {
 public:
  using LookupValue = std::shared_ptr<Value>;
  using Lookup = Map<std::string, LookupValue>;

  /**
   * Return a lookup map to quickly lookup by key.
   *
   * The lookup is owned by the caller.
   */
  const Lookup create_lookup() const
  {
    Lookup result;
    for (const Item &item : elements()) {
      result.add_as(item.first, item.second);
    }
    return result;
  }
};

/**
 * Interface for any provided Formatter.
 */
class Formatter {
 public:
  virtual ~Formatter() = default;

  /** Serialize the value to the given stream. */
  virtual void serialize(std::ostream &os, const Value &value) = 0;

  /** Deserialize the stream. */
  virtual std::unique_ptr<Value> deserialize(std::istream &is) = 0;
};

/**
 * Formatter to (de)serialize a JSON formatted stream.
 */
class JsonFormatter : public Formatter {
 public:
  /**
   * The indentation level to use.
   * Typically number of chars. Set to 0 to not use indentation.
   */
  int8_t indentation_len = 0;

 public:
  void serialize(std::ostream &os, const Value &value) override;
  std::unique_ptr<Value> deserialize(std::istream &is) override;
};

}  // namespace blender::io::serialize

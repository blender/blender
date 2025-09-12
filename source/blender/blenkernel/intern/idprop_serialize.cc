/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <optional>

#include "DNA_ID.h"

#include "BKE_idprop.hh"

#include "BLI_listbase.h"
#include "BLI_serialize.hh"

namespace blender::bke::idprop {
using namespace blender::io::serialize;

/* Forward declarations */
class IDPropertySerializer;
struct DictionaryEntryParser;
static IDProperty *idprop_from_value(const DictionaryValue &value);
static const IDPropertySerializer &serializer_for(eIDPropertyType property_type);
static const IDPropertySerializer &serializer_for(StringRef idprop_typename);

/* -------------------------------------------------------------------- */
/** \name ID property serialization.
 * \{ */

/* Definitions */
static constexpr StringRef IDP_KEY_NAME("name");
static constexpr StringRef IDP_KEY_TYPE("type");
static constexpr StringRef IDP_KEY_SUBTYPE("subtype");
static constexpr StringRef IDP_KEY_VALUE("value");

static constexpr StringRef IDP_PROPERTY_TYPENAME_STRING("IDP_STRING");
static constexpr StringRef IDP_PROPERTY_TYPENAME_BOOL("IDP_BOOL");
static constexpr StringRef IDP_PROPERTY_TYPENAME_INT("IDP_INT");
static constexpr StringRef IDP_PROPERTY_TYPENAME_FLOAT("IDP_FLOAT");
static constexpr StringRef IDP_PROPERTY_TYPENAME_DOUBLE("IDP_DOUBLE");
static constexpr StringRef IDP_PROPERTY_TYPENAME_ARRAY("IDP_ARRAY");
static constexpr StringRef IDP_PROPERTY_TYPENAME_GROUP("IDP_GROUP");
static constexpr StringRef IDP_PROPERTY_TYPENAME_UNKNOWN("IDP_UNKNOWN");

/**
 * \brief Base class for (de)serializing IDProperties.
 *
 * Has a subclass for supported IDProperties and one for unsupported IDProperties.
 */
class IDPropertySerializer {
 public:
  constexpr IDPropertySerializer() = default;

  /**
   * \brief return the type name for (de)serializing.
   * Type name is stored in the `type` or `subtype` attribute of the serialized id_property.
   */
  virtual std::string type_name() const = 0;

  /**
   * \brief return the IDPropertyType for (de)serializing.
   */
  virtual std::optional<eIDPropertyType> property_type() const = 0;

  /**
   * \brief create dictionary containing the given id_property.
   */
  virtual std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const = 0;

  /**
   * \brief convert the entry to an id property.
   */
  virtual std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const = 0;

  /**
   * \brief Can the serializer be used?
   *
   * IDP_ID and IDP_IDPARRAY aren't supported for serialization.
   */
  virtual bool supports_serializing() const
  {
    return true;
  }

 protected:
  /**
   * \brief Create a new DictionaryValue instance.
   *
   * Only fill the dictionary with common attributes (name, type).
   */
  std::shared_ptr<DictionaryValue> create_dictionary(const IDProperty *id_property) const
  {
    std::shared_ptr<DictionaryValue> result = std::make_shared<DictionaryValue>();
    result->append_str(IDP_KEY_NAME, id_property->name);
    result->append_str(IDP_KEY_TYPE, this->type_name());
    return result;
  }
};

/**
 * \brief Helper class for parsing DictionaryValues.
 */
struct DictionaryEntryParser {
  DictionaryValue::Lookup lookup;

 public:
  explicit DictionaryEntryParser(const DictionaryValue &value) : lookup(value.create_lookup()) {}

  std::optional<eIDPropertyType> get_type() const
  {
    return get_id_property_type(IDP_KEY_TYPE);
  }

  std::optional<eIDPropertyType> get_subtype() const
  {
    return get_id_property_type(IDP_KEY_SUBTYPE);
  }

  std::optional<std::string> get_name() const
  {
    return get_string(IDP_KEY_NAME);
  }

  std::optional<std::string> get_string_value() const
  {
    return get_string(IDP_KEY_VALUE);
  }

  std::optional<bool> get_bool_value() const
  {
    return get_bool(IDP_KEY_VALUE);
  }

  std::optional<int32_t> get_int_value() const
  {
    return get_int(IDP_KEY_VALUE);
  }

  std::optional<float> get_float_value() const
  {
    return get_float(IDP_KEY_VALUE);
  }

  std::optional<double> get_double_value() const
  {
    return get_double(IDP_KEY_VALUE);
  }

  std::optional<int> get_enum_value() const
  {
    return get_enum(IDP_KEY_VALUE);
  }

  const ArrayValue *get_array_value() const
  {
    return get_array(IDP_KEY_VALUE);
  }

  std::optional<Vector<int32_t>> get_array_int_value() const
  {
    return get_array_primitive<int32_t, IntValue>(IDP_KEY_VALUE);
  }

  std::optional<Vector<float>> get_array_float_value() const
  {
    return get_array_primitive<float, DoubleValue>(IDP_KEY_VALUE);
  }

  std::optional<Vector<double>> get_array_double_value() const
  {
    return get_array_primitive<double, DoubleValue>(IDP_KEY_VALUE);
  }

 private:
  std::optional<std::string> get_string(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (value->get()->type() != eValueType::String) {
      return std::nullopt;
    }
    return value->get()->as_string_value()->value();
  }

  const ArrayValue *get_array(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return nullptr;
    }
    if (value->get()->type() != eValueType::Array) {
      return nullptr;
    }
    return value->get()->as_array_value();
  }

  std::optional<bool> get_bool(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (value->get()->type() != eValueType::Boolean) {
      return std::nullopt;
    }
    return value->get()->as_boolean_value()->value();
  }

  std::optional<int32_t> get_int(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (value->get()->type() != eValueType::Int) {
      return std::nullopt;
    }
    return value->get()->as_int_value()->value();
  }

  std::optional<int32_t> get_enum(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (value->get()->type() != eValueType::Int) {
      return std::nullopt;
    }
    return value->get()->as_int_value()->value();
  }

  std::optional<double> get_double(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (value->get()->type() != eValueType::Double) {
      return std::nullopt;
    }
    return value->get()->as_double_value()->value();
  }

  std::optional<float> get_float(StringRef key) const
  {
    return static_cast<std::optional<float>>(get_double(key));
  }

  template<typename PrimitiveType, typename ValueType>
  std::optional<Vector<PrimitiveType>> get_array_primitive(StringRef key) const
  {
    const std::shared_ptr<Value> *value = lookup.lookup_ptr(key);
    if (value == nullptr) {
      return std::nullopt;
    }
    if (value->get()->type() != eValueType::Array) {
      return std::nullopt;
    }

    Vector<PrimitiveType> result;
    for (const std::shared_ptr<Value> &element : value->get()->as_array_value()->elements()) {
      const ValueType *value_type = static_cast<const ValueType *>(element.get());
      PrimitiveType primitive_value = value_type->value();
      result.append_as(primitive_value);
    }

    return result;
  }

  std::optional<eIDPropertyType> get_id_property_type(StringRef key) const
  {
    std::optional<std::string> string_value = get_string(key);
    if (!string_value.has_value()) {
      return std::nullopt;
    }
    const IDPropertySerializer &serializer = serializer_for(*string_value);
    return serializer.property_type();
  }
};

/** \brief IDPSerializer for IDP_STRING. */
class IDPStringSerializer : public IDPropertySerializer {
 public:
  constexpr IDPStringSerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_STRING;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_STRING;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);
    result->append_str(IDP_KEY_VALUE, IDP_string_get(id_property));
    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_STRING);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<std::string> string_value = entry_reader.get_string_value();
    if (!string_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), string_value->c_str());
  }
};

/** \brief IDPSerializer for IDP_INT. */
class IDPBoolSerializer : public IDPropertySerializer {
 public:
  constexpr IDPBoolSerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_BOOL;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_BOOLEAN;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);
    result->append(IDP_KEY_VALUE, std::make_shared<BooleanValue>(IDP_bool_get(id_property) != 0));
    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_BOOLEAN);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<bool> extracted_value = entry_reader.get_bool_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create_bool(name->c_str(), *extracted_value);
  }
};

/** \brief IDPSerializer for IDP_INT. */
class IDPIntSerializer : public IDPropertySerializer {
 public:
  constexpr IDPIntSerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_INT;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_INT;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);
    result->append_int(IDP_KEY_VALUE, IDP_int_get(id_property));
    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_INT);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<int32_t> extracted_value = entry_reader.get_int_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), *extracted_value);
  }
};

/** \brief IDPSerializer for IDP_FLOAT. */
class IDPFloatSerializer : public IDPropertySerializer {
 public:
  constexpr IDPFloatSerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_FLOAT;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_FLOAT;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);
    result->append_double(IDP_KEY_VALUE, IDP_float_get(id_property));
    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_FLOAT);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<float> extracted_value = entry_reader.get_float_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), *extracted_value);
  }
};

/** \brief IDPSerializer for IDP_DOUBLE. */
class IDPDoubleSerializer : public IDPropertySerializer {
 public:
  constexpr IDPDoubleSerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_DOUBLE;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_DOUBLE;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);
    result->append_double(IDP_KEY_VALUE, IDP_double_get(id_property));
    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_DOUBLE);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<double> extracted_value = entry_reader.get_double_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), *extracted_value);
  }
};

/** \brief IDPSerializer for IDP_ARRAY. */
class IDPArraySerializer : public IDPropertySerializer {
 public:
  constexpr IDPArraySerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_ARRAY;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_ARRAY;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);
    const IDPropertySerializer &subtype_serializer = serializer_for(
        static_cast<eIDPropertyType>(id_property->subtype));
    result->append_str(IDP_KEY_SUBTYPE, subtype_serializer.type_name());

    ArrayValue &array = *result->append_array(IDP_KEY_VALUE);
    switch (static_cast<eIDPropertyType>(id_property->subtype)) {
      case IDP_INT: {
        int32_t *values = IDP_array_int_get(id_property);
        add_values<int32_t, IntValue>(array, Span<int32_t>(values, id_property->len));
        break;
      }
      case IDP_FLOAT: {
        float *values = IDP_array_float_get(id_property);
        add_values<float, DoubleValue>(array, Span<float>(values, id_property->len));
        break;
      }
      case IDP_DOUBLE: {
        double *values = IDP_array_double_get(id_property);
        add_values<double, DoubleValue>(array, Span<double>(values, id_property->len));
        break;
      }
      case IDP_GROUP: {
        IDProperty *values = static_cast<IDProperty *>(IDP_array_voidp_get(id_property));
        add_values(array, Span<IDProperty>(values, id_property->len));
        break;
      }
      default: {
        /* IDP_ARRAY only supports IDP_INT, IDP_FLOAT, IDP_DOUBLE and IDP_GROUP. */
        BLI_assert_unreachable();
        break;
      }
    }

    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_ARRAY);
    std::optional<eIDPropertyType> property_subtype = entry_reader.get_subtype();
    if (!property_subtype.has_value()) {
      return nullptr;
    }

    switch (*property_subtype) {
      case IDP_INT:
        return idprop_array_int_from_value(entry_reader);
      case IDP_FLOAT:
        return idprop_array_float_from_value(entry_reader);
      case IDP_DOUBLE:
        return idprop_array_double_from_value(entry_reader);
      default:
        return nullptr;
    }
  }

 private:
  /** Add the given values to array. */
  template</* C-primitive type of the values to add. Possible types are `float`, `int32_t` or
            * `double`. */
           typename PrimitiveType,
           /* Type of value that can store the PrimitiveType in the Array. */
           typename ValueType>
  void add_values(ArrayValue &array, Span<PrimitiveType> values) const
  {
    for (PrimitiveType value : values) {
      array.append(std::make_shared<ValueType>(value));
    }
  }

  void add_values(ArrayValue &array, Span<IDProperty> values) const
  {
    for (const IDProperty &id_property : values) {
      const IDPropertySerializer &value_serializer = serializer_for(
          static_cast<eIDPropertyType>(id_property.type));
      if (!value_serializer.supports_serializing()) {
        continue;
      }
      array.append(value_serializer.idprop_to_dictionary(&id_property));
    }
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> idprop_array_int_from_value(
      DictionaryEntryParser &entry_reader) const
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_ARRAY);
    BLI_assert(*(entry_reader.get_subtype()) == IDP_INT);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<Vector<int32_t>> extracted_value = entry_reader.get_array_int_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), *extracted_value);
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> idprop_array_float_from_value(
      DictionaryEntryParser &entry_reader) const
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_ARRAY);
    BLI_assert(*(entry_reader.get_subtype()) == IDP_FLOAT);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<Vector<float>> extracted_value = entry_reader.get_array_float_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), *extracted_value);
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> idprop_array_double_from_value(
      DictionaryEntryParser &entry_reader) const
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_ARRAY);
    BLI_assert(*(entry_reader.get_subtype()) == IDP_DOUBLE);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }
    std::optional<Vector<double>> extracted_value = entry_reader.get_array_double_value();
    if (!extracted_value.has_value()) {
      return nullptr;
    }
    return create(name->c_str(), *extracted_value);
  }
};

/** \brief IDPSerializer for IDP_GROUP. */
class IDPGroupSerializer : public IDPropertySerializer {
 public:
  constexpr IDPGroupSerializer() = default;

  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_GROUP;
  }

  std::optional<eIDPropertyType> property_type() const override
  {
    return IDP_GROUP;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty *id_property) const override
  {
    std::shared_ptr<DictionaryValue> result = create_dictionary(id_property);

    std::shared_ptr<ArrayValue> array = std::make_shared<ArrayValue>();
    LISTBASE_FOREACH (IDProperty *, sub_property, &id_property->data.group) {
      const IDPropertySerializer &sub_property_serializer = serializer_for(
          static_cast<eIDPropertyType>(sub_property->type));
      array->append(sub_property_serializer.idprop_to_dictionary(sub_property));
    }

    result->append(IDP_KEY_VALUE, std::move(array));
    return result;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser &entry_reader) const override
  {
    BLI_assert(*(entry_reader.get_type()) == IDP_GROUP);
    std::optional<std::string> name = entry_reader.get_name();
    if (!name.has_value()) {
      return nullptr;
    }

    const ArrayValue *array = entry_reader.get_array_value();
    if (array == nullptr) {
      return nullptr;
    }

    std::unique_ptr<IDProperty, IDPropertyDeleter> result = create_group(name->c_str());
    for (const std::shared_ptr<Value> &element : array->elements()) {
      if (element->type() != eValueType::Dictionary) {
        continue;
      }
      const DictionaryValue *subobject = element->as_dictionary_value();
      IDProperty *subproperty = idprop_from_value(*subobject);
      IDP_AddToGroup(result.get(), subproperty);
    }

    return result;
  }
};

/**
 * \brief Dummy serializer for unknown and unsupported types.
 */
class IDPUnknownSerializer : public IDPropertySerializer {
 public:
  constexpr IDPUnknownSerializer() = default;
  std::string type_name() const override
  {
    return IDP_PROPERTY_TYPENAME_UNKNOWN;
  }
  std::optional<eIDPropertyType> property_type() const override
  {
    return std::nullopt;
  }

  std::shared_ptr<DictionaryValue> idprop_to_dictionary(
      const IDProperty * /*id_property*/) const override
  {
    BLI_assert_unreachable();
    return nullptr;
  }

  bool supports_serializing() const override
  {
    return false;
  }

  std::unique_ptr<IDProperty, IDPropertyDeleter> entry_to_idprop(
      DictionaryEntryParser & /*entry_reader*/) const override
  {
    return nullptr;
  }
};

/* Serializers are constructed statically to remove construction/destruction. */
static constexpr IDPStringSerializer IDP_SERIALIZER_STRING;
static constexpr IDPBoolSerializer IDP_SERIALIZER_BOOL;
static constexpr IDPIntSerializer IDP_SERIALIZER_INT;
static constexpr IDPFloatSerializer IDP_SERIALIZER_FLOAT;
static constexpr IDPDoubleSerializer IDP_SERIALIZER_DOUBLE;
static constexpr IDPArraySerializer IDP_SERIALIZER_ARRAY;
static constexpr IDPGroupSerializer IDP_SERIALIZER_GROUP;
static constexpr IDPUnknownSerializer IDP_SERIALIZER_UNKNOWN;

/** \brief get the serializer for the given property type. */
static const IDPropertySerializer &serializer_for(eIDPropertyType property_type)
{
  switch (property_type) {
    case IDP_STRING:
      return IDP_SERIALIZER_STRING;

    case IDP_BOOLEAN:
      return IDP_SERIALIZER_BOOL;

    case IDP_INT:
      return IDP_SERIALIZER_INT;

    case IDP_FLOAT:
      return IDP_SERIALIZER_FLOAT;

    case IDP_DOUBLE:
      return IDP_SERIALIZER_DOUBLE;

    case IDP_ARRAY:
      return IDP_SERIALIZER_ARRAY;

    case IDP_GROUP:
      return IDP_SERIALIZER_GROUP;

    default:
      BLI_assert_msg(false, "Trying to convert an unsupported/unknown property type to a string");
      return IDP_SERIALIZER_UNKNOWN;
  }
}

/** \brief get serializer for the given typename. */
static const IDPropertySerializer &serializer_for(StringRef idprop_typename)
{
  if (idprop_typename == IDP_PROPERTY_TYPENAME_STRING) {
    return IDP_SERIALIZER_STRING;
  }
  if (idprop_typename == IDP_PROPERTY_TYPENAME_BOOL) {
    return IDP_SERIALIZER_BOOL;
  }
  if (idprop_typename == IDP_PROPERTY_TYPENAME_INT) {
    return IDP_SERIALIZER_INT;
  }
  if (idprop_typename == IDP_PROPERTY_TYPENAME_FLOAT) {
    return IDP_SERIALIZER_FLOAT;
  }
  if (idprop_typename == IDP_PROPERTY_TYPENAME_DOUBLE) {
    return IDP_SERIALIZER_DOUBLE;
  }
  if (idprop_typename == IDP_PROPERTY_TYPENAME_ARRAY) {
    return IDP_SERIALIZER_ARRAY;
  }
  if (idprop_typename == IDP_PROPERTY_TYPENAME_GROUP) {
    return IDP_SERIALIZER_GROUP;
  }
  return IDP_SERIALIZER_UNKNOWN;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name IDProperty to Value
 * \{ */

std::unique_ptr<ArrayValue> convert_to_serialize_values(const IDProperty *properties)
{
  BLI_assert(properties != nullptr);
  std::unique_ptr<ArrayValue> result = std::make_unique<ArrayValue>();
  const IDProperty *current_property = properties;
  while (current_property != nullptr) {
    const IDPropertySerializer &serializer = serializer_for(
        static_cast<eIDPropertyType>(current_property->type));
    if (serializer.supports_serializing()) {
      result->append(serializer.idprop_to_dictionary(current_property));
    }
    current_property = current_property->next;
  }

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name IDProperty from Value
 * \{ */

static IDProperty *idprop_from_value(const DictionaryValue &value)
{
  DictionaryEntryParser entry_reader(value);
  std::optional<eIDPropertyType> property_type = entry_reader.get_type();
  if (!property_type.has_value()) {
    return nullptr;
  }

  const IDPropertySerializer &serializer = serializer_for(*property_type);
  return serializer.entry_to_idprop(entry_reader).release();
}

static IDProperty *idprop_from_value(const ArrayValue &value)
{
  IDProperty *result = nullptr;
  IDProperty *previous_added = nullptr;

  for (const std::shared_ptr<Value> &element : value.elements()) {
    if (element->type() != eValueType::Dictionary) {
      continue;
    }
    const DictionaryValue *object_value = element->as_dictionary_value();
    IDProperty *last_created = idprop_from_value(*object_value);
    if (last_created == nullptr) {
      continue;
    }

    if (result == nullptr) {
      result = last_created;
    }

    if (previous_added) {
      previous_added->next = last_created;
    }
    last_created->prev = previous_added;
    previous_added = last_created;
  }

  return result;
}

IDProperty *convert_from_serialize_value(const Value &value)
{
  if (value.type() != eValueType::Array) {
    return nullptr;
  }

  return idprop_from_value(*value.as_array_value());
}

/** \} */

}  // namespace blender::bke::idprop

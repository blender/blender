/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_listbase.h"
#include "BLI_serialize.hh"

#include "DNA_ID.h"

#include "BKE_idprop.hh"

namespace blender::bke::idprop::tests {

using namespace blender::io::serialize;

static void check_container_value(ArrayValue *value)
{
  ASSERT_NE(value, nullptr);
  ASSERT_EQ(value->type(), eValueType::Array);
  const Span<std::shared_ptr<Value>> elements = value->elements();
  EXPECT_FALSE(elements.is_empty());
  EXPECT_EQ(elements.size(), 1);

  const std::shared_ptr<Value> &item = value->elements()[0];
  ASSERT_EQ(item->type(), eValueType::Dictionary);
}

static void check_object_attribute(const DictionaryValue::Lookup &lookup,
                                   const std::string expected_key,
                                   const std::string expected_value)
{
  EXPECT_TRUE(lookup.contains(expected_key));
  const std::shared_ptr<Value> &element = *lookup.lookup_ptr(expected_key);
  ASSERT_EQ(element->type(), eValueType::String);
  EXPECT_EQ(element->as_string_value()->value(), expected_value);
}

static void check_object_attribute(const DictionaryValue::Lookup &lookup,
                                   const std::string expected_key,
                                   const int32_t expected_value)
{
  EXPECT_TRUE(lookup.contains(expected_key));
  const std::shared_ptr<Value> &element = *lookup.lookup_ptr(expected_key);
  ASSERT_EQ(element->type(), eValueType::Int);
  EXPECT_EQ(element->as_int_value()->value(), expected_value);
}

static void check_object_attribute(const DictionaryValue::Lookup &lookup,
                                   const std::string expected_key,
                                   const float expected_value)
{
  EXPECT_TRUE(lookup.contains(expected_key));
  const std::shared_ptr<Value> &element = *lookup.lookup_ptr(expected_key);
  ASSERT_EQ(element->type(), eValueType::Double);
  EXPECT_EQ(element->as_double_value()->value(), expected_value);
}

static void check_object_attribute(const DictionaryValue::Lookup &lookup,
                                   const std::string expected_key,
                                   const double expected_value)
{
  EXPECT_TRUE(lookup.contains(expected_key));
  const std::shared_ptr<Value> &element = *lookup.lookup_ptr(expected_key);
  ASSERT_EQ(element->type(), eValueType::Double);
  EXPECT_EQ(element->as_double_value()->value(), expected_value);
}

static void test_string_to_value(const StringRefNull prop_name, const StringRefNull prop_content)
{
  std::unique_ptr<IDProperty, IDPropertyDeleter> property = create(prop_name, prop_content);

  std::unique_ptr<ArrayValue> value = convert_to_serialize_values(property.get());
  check_container_value(value.get());
  const std::shared_ptr<Value> &item = value->elements()[0];
  const DictionaryValue *object = item->as_dictionary_value();
  const DictionaryValue::Lookup lookup = object->create_lookup();

  EXPECT_EQ(lookup.size(), 3);
  check_object_attribute(lookup, "name", prop_name);
  check_object_attribute(lookup, "type", "IDP_STRING");
  check_object_attribute(lookup, "value", prop_content);
}

TEST(idprop, convert_idp_string_to_value)
{
  test_string_to_value("mykey", "mycontent");
}

static void test_int_to_value(const StringRefNull prop_name, int32_t prop_content)
{
  std::unique_ptr<IDProperty, IDPropertyDeleter> property = create(prop_name, prop_content);

  std::unique_ptr<ArrayValue> value = convert_to_serialize_values(property.get());
  check_container_value(value.get());
  const std::shared_ptr<Value> &item = value->elements()[0];
  const DictionaryValue *object = item->as_dictionary_value();
  const DictionaryValue::Lookup lookup = object->create_lookup();

  EXPECT_EQ(lookup.size(), 3);
  check_object_attribute(lookup, "name", prop_name);
  check_object_attribute(lookup, "type", "IDP_INT");
  check_object_attribute(lookup, "value", prop_content);
}

TEST(idprop, convert_idp_int_to_value)
{
  test_int_to_value("mykey", 0);
}

static void test_float_to_value(const StringRefNull prop_name, float prop_content)
{
  std::unique_ptr<IDProperty, IDPropertyDeleter> property = create(prop_name, prop_content);

  std::unique_ptr<ArrayValue> value = convert_to_serialize_values(property.get());
  check_container_value(value.get());
  const std::shared_ptr<Value> &item = value->elements()[0];
  const DictionaryValue *object = item->as_dictionary_value();
  const DictionaryValue::Lookup lookup = object->create_lookup();

  EXPECT_EQ(lookup.size(), 3);
  check_object_attribute(lookup, "name", prop_name);
  check_object_attribute(lookup, "type", "IDP_FLOAT");
  check_object_attribute(lookup, "value", prop_content);
}

TEST(idprop, convert_idp_float_to_value)
{
  test_float_to_value("mykey", 0.2f);
}

static void test_double_to_value(const StringRefNull prop_name, double prop_content)
{
  std::unique_ptr<IDProperty, IDPropertyDeleter> property = create(prop_name, prop_content);

  std::unique_ptr<ArrayValue> value = convert_to_serialize_values(property.get());
  check_container_value(value.get());
  const std::shared_ptr<Value> &item = value->elements()[0];
  const DictionaryValue *object = item->as_dictionary_value();
  const DictionaryValue::Lookup lookup = object->create_lookup();

  EXPECT_EQ(lookup.size(), 3);
  check_object_attribute(lookup, "name", prop_name);
  check_object_attribute(lookup, "type", "IDP_DOUBLE");
  check_object_attribute(lookup, "value", prop_content);
}

TEST(idprop, convert_idp_double_to_value)
{
  test_double_to_value("mykey", 0.2);
}

template<typename PrimitiveType, typename ValueType>
static void test_array_to_value(const StringRefNull prop_name, Vector<PrimitiveType> prop_content)
{
  std::unique_ptr<IDProperty, IDPropertyDeleter> property = create(prop_name, prop_content);
  std::unique_ptr<ArrayValue> value = convert_to_serialize_values(property.get());

  check_container_value(value.get());
  const std::shared_ptr<Value> &item = value->elements()[0];
  const DictionaryValue *object = item->as_dictionary_value();
  const DictionaryValue::Lookup lookup = object->create_lookup();

  EXPECT_EQ(lookup.size(), 4);
  check_object_attribute(lookup, "name", prop_name);
  check_object_attribute(lookup, "type", "IDP_ARRAY");

  const std::shared_ptr<Value> &element = *lookup.lookup_ptr("value");
  const ArrayValue *subvalues = element->as_array_value();
  ASSERT_NE(subvalues, nullptr);
  const Span<std::shared_ptr<Value>> subitems = subvalues->elements();
  ASSERT_EQ(subitems.size(), prop_content.size());

  for (size_t i = 0; i < prop_content.size(); i++) {
    EXPECT_EQ(static_cast<ValueType *>(subitems[i].get())->value(), prop_content[i]);
  }
}

TEST(idprop, convert_idp_int_array_to_value)
{
  test_array_to_value<int32_t, IntValue>("my_integer_array",
                                         {-16, -8, -4, -2, -1, 0, 1, 2, 4, 8, 16});
}

TEST(idprop, convert_idp_float_array_to_value)
{
  test_array_to_value<float, DoubleValue>(
      "my_float_array", {-16.8f, -8.4f, -4.2f, -2.1f, -1.0f, 0.0f, 1.0f, 2.1f, 4.2f, 8.4f, 16.8f});
}

TEST(idprop, convert_idp_double_array_to_value)
{
  test_array_to_value<double, DoubleValue>(
      "my_double_array", {-16.8, -8.4, -4.2, -2.1, -1.0, 0.0, 1.0, 2.1, 4.2, 8.4, 16.8});
}

static std::unique_ptr<Value> parse_json(StringRef input)
{
  std::stringstream is(input);
  JsonFormatter json;
  std::unique_ptr<Value> value = json.deserialize(is);
  return value;
}

static std::string to_json(const Value &value)
{
  std::stringstream out;
  JsonFormatter json;
  json.serialize(out, value);
  return out.str();
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        StringRef expected_value)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_STRING);
  EXPECT_EQ(id_property->name, expected_name);
  EXPECT_EQ(IDP_string_get(id_property), expected_value);
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        int32_t expected_value)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_INT);
  EXPECT_EQ(id_property->name, expected_name);
  EXPECT_EQ(IDP_int_get(id_property), expected_value);
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        float expected_value)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_FLOAT);
  EXPECT_EQ(id_property->name, expected_name);
  EXPECT_EQ(IDP_float_get(id_property), expected_value);
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        double expected_value)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_DOUBLE);
  EXPECT_EQ(id_property->name, expected_name);
  EXPECT_EQ(IDP_double_get(id_property), expected_value);
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        const Span<int32_t> values)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_ARRAY);
  EXPECT_EQ(id_property->subtype, IDP_INT);
  EXPECT_EQ(id_property->len, values.size());
  EXPECT_EQ(id_property->name, expected_name);
  int32_t *idprop_values = IDP_array_int_get(id_property);
  for (int i = 0; i < values.size(); i++) {
    EXPECT_EQ(idprop_values[i], values[i]);
  }
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        const Span<float> values)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_ARRAY);
  EXPECT_EQ(id_property->subtype, IDP_FLOAT);
  EXPECT_EQ(id_property->len, values.size());
  EXPECT_EQ(id_property->name, expected_name);
  float *idprop_values = IDP_array_float_get(id_property);
  for (int i = 0; i < values.size(); i++) {
    EXPECT_EQ(idprop_values[i], values[i]);
  }
}

static void test_idprop(const IDProperty *id_property,
                        StringRef expected_name,
                        const Span<double> values)
{
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_ARRAY);
  EXPECT_EQ(id_property->subtype, IDP_DOUBLE);
  EXPECT_EQ(id_property->len, values.size());
  EXPECT_EQ(id_property->name, expected_name);
  double *idprop_values = IDP_array_double_get(id_property);
  for (int i = 0; i < values.size(); i++) {
    EXPECT_EQ(idprop_values[i], values[i]);
  }
}

template<typename Type>
static void test_convert_idprop_from_value(StringRef input,
                                           StringRef expected_name,
                                           Type expected_value)
{
  std::unique_ptr<Value> value = parse_json(input);
  IDProperty *id_property = convert_from_serialize_value(*value);
  test_idprop(id_property, expected_name, expected_value);
  IDP_FreeProperty(id_property);
}

TEST(idprop, convert_idp_string_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyStringName","type":"IDP_STRING","value":"MyString"}])",
      "MyStringName",
      "MyString");
}

TEST(idprop, convert_idp_int_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyIntegerName","type":"IDP_INT","value":42}])", "MyIntegerName", 42);
}

TEST(idprop, convert_idp_float_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyFloatName","type":"IDP_FLOAT","value":42.24}])", "MyFloatName", 42.24f);
}

TEST(idprop, convert_idp_double_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyDoubleName","type":"IDP_DOUBLE","value":42.24}])", "MyDoubleName", 42.24);
}

TEST(idprop, convert_idp_array_int_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyArrayName","type":"IDP_ARRAY","subtype":"IDP_INT","value":[42, 24, 35]}])",
      "MyArrayName",
      Vector<int32_t>{42, 24, 35});
}

TEST(idprop, convert_idp_array_float_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyArrayName","type":"IDP_ARRAY","subtype":"IDP_FLOAT","value":[42.0, 24.4, 35.2]}])",
      "MyArrayName",
      Vector<float>{42.0f, 24.4f, 35.2f});
}

TEST(idprop, convert_idp_array_double_from_value)
{
  test_convert_idprop_from_value(
      R"([{"name":"MyArrayName","type":"IDP_ARRAY","subtype":"IDP_DOUBLE","value":[42.43,24.5,35.8]}])",
      "MyArrayName",
      Vector<double>{42.43, 24.5, 35.8});
}

TEST(idprop, convert_idp_multiple_from_value)
{
  static const std::string input_json =
      R"([{"name":"MyIntegerName","type":"IDP_INT","value":42},{"name":"MyStringName","type":"IDP_STRING","value":"MyString"},{"name":"MyFloatName","type":"IDP_FLOAT","value":42.24},{"name":"MyDoubleName","type":"IDP_DOUBLE","value":42.24}])";
  std::unique_ptr<Value> value = parse_json(input_json);

  IDProperty *id_property = convert_from_serialize_value(*value);
  IDProperty *id_property_1 = id_property;
  ASSERT_NE(id_property_1, nullptr);
  IDProperty *id_property_2 = id_property_1->next;
  ASSERT_NE(id_property_2, nullptr);
  IDProperty *id_property_3 = id_property_2->next;
  ASSERT_NE(id_property_3, nullptr);
  IDProperty *id_property_4 = id_property_3->next;
  ASSERT_NE(id_property_4, nullptr);

  EXPECT_EQ(id_property_1->prev, nullptr);
  EXPECT_EQ(id_property_2->prev, id_property_1);
  EXPECT_EQ(id_property_3->prev, id_property_2);
  EXPECT_EQ(id_property_4->prev, id_property_3);
  EXPECT_EQ(id_property_4->next, nullptr);

  test_idprop(id_property_1, "MyIntegerName", 42);
  test_idprop(id_property_2, "MyStringName", "MyString");
  test_idprop(id_property_3, "MyFloatName", 42.24f);
  test_idprop(id_property_4, "MyDoubleName", 42.24);

  IDP_FreeProperty(id_property_1);
  IDP_FreeProperty(id_property_2);
  IDP_FreeProperty(id_property_3);
  IDP_FreeProperty(id_property_4);
}

TEST(idprop, convert_idp_multiple_roundtrip)
{
  static const std::string input_json =
      R"([{"name":"MyIntegerName","type":"IDP_INT","value":42},{"name":"MyStringName","type":"IDP_STRING","value":"MyString"},{"name":"MyFloatName","type":"IDP_FLOAT","value":42.2400016784668},{"name":"MyDoubleName","type":"IDP_DOUBLE","value":42.24}])";
  std::unique_ptr<Value> value = parse_json(input_json);

  IDProperty *id_property = convert_from_serialize_value(*value);
  IDProperty *id_property_1 = id_property;
  ASSERT_NE(id_property_1, nullptr);
  IDProperty *id_property_2 = id_property_1->next;
  ASSERT_NE(id_property_2, nullptr);
  IDProperty *id_property_3 = id_property_2->next;
  ASSERT_NE(id_property_3, nullptr);
  IDProperty *id_property_4 = id_property_3->next;
  ASSERT_NE(id_property_4, nullptr);

  std::unique_ptr<Value> value_from_id_properties = convert_to_serialize_values(id_property);
  std::string output_json = to_json(*value_from_id_properties);
  EXPECT_EQ(input_json, output_json);

  IDP_FreeProperty(id_property_1);
  IDP_FreeProperty(id_property_2);
  IDP_FreeProperty(id_property_3);
  IDP_FreeProperty(id_property_4);
}

TEST(idprop, convert_idp_group_from_value)
{
  static const std::string input_json =
      R"([{"name":"AssetMetaData.properties","type":"IDP_GROUP","value":[{"name":"dimensions","type":"IDP_ARRAY","subtype":"IDP_FLOAT","value":[2.0,2.0,2.0]}]}])";
  std::unique_ptr<Value> value = parse_json(input_json);

  IDProperty *id_property = convert_from_serialize_value(*value);
  ASSERT_NE(id_property, nullptr);
  EXPECT_EQ(id_property->type, IDP_GROUP);
  EXPECT_EQ(BLI_listbase_count(&id_property->data.group), 1);

  test_idprop(static_cast<IDProperty *>(id_property->data.group.first),
              "dimensions",
              Vector<float>{2.0f, 2.0f, 2.0f});

  IDP_FreeProperty(id_property);
}

}  // namespace blender::bke::idprop::tests

/* SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_serialize.hh"

/* -------------------------------------------------------------------- */
/* tests */

namespace blender::io::serialize::json::testing {

TEST(serialize, string_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  StringValue test_value("Hello JSON");
  json.serialize(out, test_value);
  EXPECT_EQ(out.str(), "\"Hello JSON\"");
}

static void test_int_to_json(int64_t value, StringRef expected)
{
  JsonFormatter json;
  std::stringstream out;
  IntValue test_value(value);
  json.serialize(out, test_value);
  EXPECT_EQ(out.str(), expected);
}

TEST(serialize, int_to_json)
{
  test_int_to_json(42, "42");
  test_int_to_json(-42, "-42");
  test_int_to_json(std::numeric_limits<int32_t>::max(), "2147483647");
  test_int_to_json(std::numeric_limits<int32_t>::min(), "-2147483648");
  test_int_to_json(std::numeric_limits<int64_t>::max(), "9223372036854775807");
  test_int_to_json(std::numeric_limits<int64_t>::min(), "-9223372036854775808");
}

TEST(serialize, double_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  DoubleValue test_value(42.31);
  json.serialize(out, test_value);
  EXPECT_EQ(out.str(), "42.31");
}

TEST(serialize, null_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  NullValue test_value;
  json.serialize(out, test_value);
  EXPECT_EQ(out.str(), "null");
}

TEST(serialize, false_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  BooleanValue value(false);
  json.serialize(out, value);
  EXPECT_EQ(out.str(), "false");
}

TEST(serialize, true_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  BooleanValue value(true);
  json.serialize(out, value);
  EXPECT_EQ(out.str(), "true");
}

TEST(serialize, array_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  ArrayValue value_array;
  value_array.append_int(42);
  value_array.append_str("Hello JSON");
  value_array.append_null();
  value_array.append_bool(false);
  value_array.append_bool(true);

  json.serialize(out, value_array);
  EXPECT_EQ(out.str(), "[42,\"Hello JSON\",null,false,true]");
}

TEST(serialize, object_to_json)
{
  JsonFormatter json;
  std::stringstream out;
  DictionaryValue value_object;
  value_object.append_int("best_number", 42);

  json.serialize(out, value_object);
  EXPECT_EQ(out.str(), "{\"best_number\":42}");
}

TEST(serialize, json_roundtrip_ordering)
{
  const std::string input =
      "[{\"_id\":\"614ada7c476c472ecbd0ecbb\",\"index\":0,\"guid\":\"d5b81381-cef8-4327-923d-"
      "41e57ff79326\",\"isActive\":false,\"balance\":\"$2,062.25\",\"picture\":\"http://"
      "placehold.it/32x32\",\"age\":26,\"eyeColor\":\"brown\",\"name\":\"Geneva "
      "Vega\",\"gender\":\"female\",\"company\":\"SLOGANAUT\",\"email\":\"genevavega@sloganaut."
      "com\",\"phone\":\"+1 (993) 432-2805\",\"address\":\"943 Christopher Avenue, Northchase, "
      "Alabama, 5769\",\"about\":\"Eu cillum qui eu fugiat sit nulla eu duis. Aliqua nulla aliqua "
      "ea tempor dolor fugiat sint consectetur exercitation ipsum magna ex. Aute laborum esse "
      "magna nostrud in cillum et mollit proident. Deserunt ex minim adipisicing incididunt "
      "incididunt dolore velit aliqua.\\r\\n\",\"registered\":\"2014-06-02T06:29:33 "
      "-02:00\",\"latitude\":-66.003108,\"longitude\":44.038986,\"tags\":[\"exercitation\","
      "\"laborum\",\"velit\",\"magna\",\"officia\",\"aliqua\",\"laboris\"],\"friends\":[{\"id\":0,"
      "\"name\":\"Daniel Stuart\"},{\"id\":1,\"name\":\"Jackson "
      "Velez\"},{\"id\":2,\"name\":\"Browning Boyd\"}],\"greeting\":\"Hello, Geneva Vega! You "
      "have 8 unread "
      "messages.\",\"favoriteFruit\":\"strawberry\"},{\"_id\":\"614ada7cf28685063c6722af\","
      "\"index\":1,\"guid\":\"e157edf3-a86d-4984-b18d-e2fe568a9915\",\"isActive\":false,"
      "\"balance\":\"$3,550.44\",\"picture\":\"http://placehold.it/"
      "32x32\",\"age\":40,\"eyeColor\":\"blue\",\"name\":\"Lamb "
      "Lowe\",\"gender\":\"male\",\"company\":\"PROXSOFT\",\"email\":\"lamblowe@proxsoft.com\","
      "\"phone\":\"+1 (999) 573-2855\",\"address\":\"632 Rockwell Place, Diaperville, "
      "Pennsylvania, 5050\",\"about\":\"Anim dolor deserunt esse quis velit adipisicing aute "
      "nostrud velit minim culpa aute et tempor. Dolor aliqua reprehenderit anim voluptate. "
      "Consequat proident ut culpa reprehenderit qui. Nisi proident velit cillum voluptate. "
      "Ullamco id sunt quis aute adipisicing cupidatat consequat "
      "aliquip.\\r\\n\",\"registered\":\"2014-09-06T06:13:36 "
      "-02:00\",\"latitude\":-44.550228,\"longitude\":-80.893356,\"tags\":[\"anim\",\"id\","
      "\"irure\",\"do\",\"officia\",\"irure\",\"Lorem\"],\"friends\":[{\"id\":0,\"name\":"
      "\"Faulkner Watkins\"},{\"id\":1,\"name\":\"Cecile Schneider\"},{\"id\":2,\"name\":\"Burt "
      "Lester\"}],\"greeting\":\"Hello, Lamb Lowe! You have 1 unread "
      "messages.\",\"favoriteFruit\":\"strawberry\"},{\"_id\":\"614ada7c235335fc56bc2f78\","
      "\"index\":2,\"guid\":\"8206bad1-8274-49fd-9223-d727589f22ca\",\"isActive\":false,"
      "\"balance\":\"$2,548.34\",\"picture\":\"http://placehold.it/"
      "32x32\",\"age\":37,\"eyeColor\":\"blue\",\"name\":\"Sallie "
      "Chase\",\"gender\":\"female\",\"company\":\"FLEETMIX\",\"email\":\"salliechase@fleetmix."
      "com\",\"phone\":\"+1 (953) 453-3388\",\"address\":\"865 Irving Place, Chelsea, Utah, "
      "9777\",\"about\":\"In magna exercitation incididunt exercitation dolor anim. Consectetur "
      "dolore commodo elit cillum dolor reprehenderit magna minim et ex labore pariatur. Nulla "
      "ullamco officia velit in aute proident nostrud. Duis deserunt et labore Lorem aliqua "
      "eiusmod commodo sunt.\\r\\n\",\"registered\":\"2017-03-16T08:54:53 "
      "-01:00\",\"latitude\":-78.481939,\"longitude\":-149.820215,\"tags\":[\"Lorem\",\"ipsum\","
      "\"in\",\"tempor\",\"consectetur\",\"voluptate\",\"elit\"],\"friends\":[{\"id\":0,\"name\":"
      "\"Gibson Garner\"},{\"id\":1,\"name\":\"Anna Frank\"},{\"id\":2,\"name\":\"Roberson "
      "Daugherty\"}],\"greeting\":\"Hello, Sallie Chase! You have 7 unread "
      "messages.\",\"favoriteFruit\":\"apple\"},{\"_id\":\"614ada7c93b63ecad5f9ba5e\",\"index\":3,"
      "\"guid\":\"924b02fc-7c27-481a-9941-db3b9403dfe1\",\"isActive\":true,\"balance\":\"$1,633."
      "60\",\"picture\":\"http://placehold.it/"
      "32x32\",\"age\":29,\"eyeColor\":\"brown\",\"name\":\"Grace "
      "Mccall\",\"gender\":\"female\",\"company\":\"PIVITOL\",\"email\":\"gracemccall@pivitol."
      "com\",\"phone\":\"+1 (964) 541-2514\",\"address\":\"734 Schaefer Street, Topaz, Virginia, "
      "9137\",\"about\":\"Amet officia magna fugiat ut pariatur fugiat elit culpa voluptate elit "
      "do proident culpa minim. Commodo do minim reprehenderit ut voluptate ut velit id esse "
      "consequat. Labore ullamco deserunt irure eiusmod cillum tempor incididunt qui adipisicing "
      "nostrud pariatur enim aliquip. Excepteur nostrud commodo consectetur esse duis irure "
      "qui.\\r\\n\",\"registered\":\"2015-04-24T03:55:17 "
      "-02:00\",\"latitude\":58.801446,\"longitude\":-157.413865,\"tags\":[\"do\",\"ea\",\"eu\","
      "\"eu\",\"qui\",\"duis\",\"sint\"],\"friends\":[{\"id\":0,\"name\":\"Carrie "
      "Short\"},{\"id\":1,\"name\":\"Dickerson Barnes\"},{\"id\":2,\"name\":\"Rae "
      "Rios\"}],\"greeting\":\"Hello, Grace Mccall! You have 5 unread "
      "messages.\",\"favoriteFruit\":\"apple\"},{\"_id\":\"614ada7c9caf1353b0e22bbf\",\"index\":4,"
      "\"guid\":\"e5981ae1-90e4-41c4-9905-161522db700b\",\"isActive\":false,\"balance\":\"$3,660."
      "34\",\"picture\":\"http://placehold.it/"
      "32x32\",\"age\":31,\"eyeColor\":\"blue\",\"name\":\"Herring "
      "Powers\",\"gender\":\"male\",\"company\":\"PYRAMIA\",\"email\":\"herringpowers@pyramia."
      "com\",\"phone\":\"+1 (981) 541-2829\",\"address\":\"409 Furman Avenue, Waterloo, South "
      "Carolina, 380\",\"about\":\"In officia culpa aliqua culpa pariatur aliqua mollit ex. Velit "
      "est Lorem enim magna cillum sunt elit consectetur deserunt ea est consectetur fugiat "
      "mollit. Aute Lorem excepteur minim esse qui. Id Lorem in tempor et. Nisi aliquip laborum "
      "magna eu aute.\\r\\n\",\"registered\":\"2018-07-05T07:28:54 "
      "-02:00\",\"latitude\":51.497405,\"longitude\":-129.422711,\"tags\":[\"eiusmod\",\"et\","
      "\"nostrud\",\"reprehenderit\",\"Lorem\",\"cillum\",\"nulla\"],\"friends\":[{\"id\":0,"
      "\"name\":\"Tonia Keith\"},{\"id\":1,\"name\":\"Leanne Rice\"},{\"id\":2,\"name\":\"Craig "
      "Gregory\"}],\"greeting\":\"Hello, Herring Powers! You have 6 unread "
      "messages.\",\"favoriteFruit\":\"strawberry\"},{\"_id\":\"614ada7c53a3d6da77468f25\","
      "\"index\":5,\"guid\":\"abb2eec9-c4f0-4a0d-b20a-5c8e50fe88a1\",\"isActive\":true,"
      "\"balance\":\"$1,481.08\",\"picture\":\"http://placehold.it/"
      "32x32\",\"age\":31,\"eyeColor\":\"green\",\"name\":\"Lela "
      "Dillard\",\"gender\":\"female\",\"company\":\"CEMENTION\",\"email\":\"leladillard@"
      "cemention.com\",\"phone\":\"+1 (856) 456-3657\",\"address\":\"391 Diamond Street, Madaket, "
      "Ohio, 9337\",\"about\":\"Tempor dolor ullamco esse cillum excepteur. Excepteur aliqua non "
      "enim anim esse amet cupidatat non. Cillum excepteur occaecat cupidatat elit labore. "
      "Pariatur ut esse sint elit. Velit sint magna et commodo sit velit labore consectetur irure "
      "officia proident aliquip. Aliqua dolore ipsum voluptate veniam deserunt amet irure. Cillum "
      "consequat veniam proident Lorem in anim enim veniam ea "
      "nulla.\\r\\n\",\"registered\":\"2017-01-11T11:07:22 "
      "-01:00\",\"latitude\":86.349081,\"longitude\":-179.983754,\"tags\":[\"consequat\","
      "\"labore\",\"consectetur\",\"dolor\",\"laborum\",\"eiusmod\",\"in\"],\"friends\":[{\"id\":"
      "0,\"name\":\"Hancock Rivera\"},{\"id\":1,\"name\":\"Chasity "
      "Oneil\"},{\"id\":2,\"name\":\"Whitaker Barr\"}],\"greeting\":\"Hello, Lela Dillard! You "
      "have 3 unread messages.\",\"favoriteFruit\":\"strawberry\"}]";
  std::stringstream is(input);

  JsonFormatter json;
  std::unique_ptr<Value> value = json.deserialize(is);
  EXPECT_EQ(value->type(), eValueType::Array);

  std::stringstream out;
  json.serialize(out, *value);
  EXPECT_EQ(out.str(), input);
}

}  // namespace blender::io::serialize::json::testing

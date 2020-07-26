/* Apache License, Version 2.0 */

#include "BLI_float3.hh"
#include "FN_attributes_ref.hh"

#include "testing/testing.h"

namespace blender::fn::tests {

TEST(attributes_info, BuildEmpty)
{
  AttributesInfoBuilder info_builder;
  AttributesInfo info{info_builder};

  EXPECT_EQ(info.size(), 0);
}

TEST(attributes_info, AddSameNameTwice)
{
  AttributesInfoBuilder info_builder;
  info_builder.add<int>("A", 4);
  info_builder.add<int>("A", 5);
  AttributesInfo info{info_builder};
  EXPECT_EQ(info.size(), 1);
  EXPECT_TRUE(info.has_attribute("A", CPPType::get<int>()));
  EXPECT_FALSE(info.has_attribute("B", CPPType::get<int>()));
  EXPECT_FALSE(info.has_attribute("A", CPPType::get<float>()));
  EXPECT_EQ(info.default_of<int>("A"), 4);
  EXPECT_EQ(info.name_of(0), "A");
  EXPECT_EQ(info.index_range().start(), 0);
  EXPECT_EQ(info.index_range().one_after_last(), 1);
}

TEST(attributes_info, BuildWithDefaultString)
{
  AttributesInfoBuilder info_builder;
  info_builder.add("A", CPPType::get<std::string>());
  AttributesInfo info{info_builder};
  EXPECT_EQ(info.default_of<std::string>("A"), "");
}

TEST(attributes_info, BuildWithGivenDefault)
{
  AttributesInfoBuilder info_builder;
  info_builder.add<std::string>("A", "hello world");
  AttributesInfo info{info_builder};
  const void *default_value = info.default_of("A");
  EXPECT_EQ(*(const std::string *)default_value, "hello world");
  EXPECT_EQ(info.type_of("A"), CPPType::get<std::string>());
}

TEST(mutable_attributes_ref, ComplexTest)
{
  AttributesInfoBuilder info_builder;
  info_builder.add<float3>("Position", {0, 0, 10});
  info_builder.add<uint>("ID", 0);
  info_builder.add<float>("Size", 0.5f);
  info_builder.add<std::string>("Name", "<no name>");
  AttributesInfo info{info_builder};

  int amount = 5;
  Array<float3> positions(amount);
  Array<uint> ids(amount, 0);
  Array<float> sizes(amount);
  Array<std::string> names(amount);

  Array<void *> buffers = {positions.data(), ids.data(), sizes.data(), names.data()};
  MutableAttributesRef attributes{info, buffers, IndexRange(1, 3)};
  EXPECT_EQ(attributes.size(), 3);
  EXPECT_EQ(attributes.info().size(), 4);
  EXPECT_EQ(attributes.get("Position").data(), positions.data() + 1);
  EXPECT_EQ(attributes.get("ID").data(), ids.data() + 1);
  EXPECT_EQ(attributes.get("Size").data(), sizes.data() + 1);
  EXPECT_EQ(attributes.get("Name").data(), names.data() + 1);

  EXPECT_EQ(attributes.get("ID").size(), 3);
  EXPECT_EQ(attributes.get<uint>("ID").size(), 3);

  EXPECT_EQ(ids[2], 0);
  MutableSpan<uint> ids_span = attributes.get<uint>("ID");
  ids_span[1] = 42;
  EXPECT_EQ(ids[2], 42);

  EXPECT_FALSE(attributes.try_get<int>("not existant").has_value());
  EXPECT_FALSE(attributes.try_get<int>("Position").has_value());
  EXPECT_TRUE(attributes.try_get<float3>("Position").has_value());
  EXPECT_FALSE(attributes.try_get("not existant", CPPType::get<int>()).has_value());
  EXPECT_FALSE(attributes.try_get("Position", CPPType::get<int>()).has_value());
  EXPECT_TRUE(attributes.try_get("Position", CPPType::get<float3>()).has_value());

  MutableAttributesRef sliced = attributes.slice(IndexRange(1, 2));
  EXPECT_EQ(sliced.size(), 2);
  sliced.get<uint>("ID")[0] = 100;
  EXPECT_EQ(ids[2], 100);
}

}  // namespace blender::fn::tests

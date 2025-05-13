/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_storage.hh"

namespace blender::bke::tests {

TEST(attribute_storage, Empty)
{
  AttributeStorage storage;
  int count = 0;
  storage.foreach([&](const Attribute & /*attribute*/) { count++; });
  EXPECT_EQ(count, 0);
}

TEST(attribute_storage, Single)
{
  AttributeStorage storage;

  auto *sharing_info = new ImplicitSharedValue<Array<float>>(Span<float>{1.5f, 1.2f, 1.1f, 1.0f});
  Attribute::ArrayData data{};
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  data.data = sharing_info->data.data();
  data.size = 4;
  storage.add("foo", AttrDomain::Corner, AttrType::Float, std::move(data));

  EXPECT_TRUE(storage.lookup("foo"));
  EXPECT_EQ(storage.lookup("foo")->domain(), AttrDomain::Corner);
  EXPECT_EQ(storage.lookup("foo")->data_type(), AttrType::Float);
  {
    const auto &data = std::get<Attribute::ArrayData>(storage.lookup("foo")->data());
    EXPECT_EQ(data.data, sharing_info->data.data());
  }

  int count = 0;
  storage.foreach([&](const Attribute & /*attribute*/) { count++; });
  EXPECT_EQ(count, 1);
}

TEST(attribute_storage, GetForWrite)
{
  AttributeStorage storage;

  auto *sharing_info = new ImplicitSharedValue<Array<float>>(Span<float>{1.5f, 1.2f, 1.1f, 1.0f});
  Attribute::ArrayData data{};
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  data.data = sharing_info->data.data();
  data.size = 4;
  storage.add("foo", AttrDomain::Corner, AttrType::Float, std::move(data));
  {
    const auto &data = std::get<Attribute::ArrayData>(storage.lookup("foo")->data_for_write());
    EXPECT_EQ(data.data, sharing_info->data.data());
  }
  {
    sharing_info->add_user();
    const auto &data = std::get<Attribute::ArrayData>(storage.lookup("foo")->data_for_write());
    EXPECT_NE(data.data, sharing_info->data.data());
    const float *data_ptr = static_cast<const float *>(data.data);
    EXPECT_EQ(data_ptr[0], 1.5f);
    EXPECT_EQ(data_ptr[1], 1.2f);
    EXPECT_EQ(data_ptr[2], 1.1f);
    EXPECT_EQ(data_ptr[3], 1.0f);
    sharing_info->remove_user_and_delete_if_last();
  }
  {
    const auto &data = std::get<Attribute::ArrayData>(storage.lookup("foo")->data_for_write());
    const float *data_ptr = static_cast<const float *>(data.data);
    EXPECT_EQ(data_ptr[0], 1.5f);
    EXPECT_EQ(data_ptr[1], 1.2f);
    EXPECT_EQ(data_ptr[2], 1.1f);
    EXPECT_EQ(data_ptr[3], 1.0f);
  }
}

TEST(attribute_storage, MultipleShared)
{
  AttributeStorage storage;

  auto *sharing_info = new ImplicitSharedValue<Array<float>>(Span<float>{1.5f, 1.2f, 1.1f, 1.0f});
  Attribute::ArrayData data{};
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  data.data = sharing_info->data.data();
  data.size = 4;
  storage.add("we", AttrDomain::Corner, AttrType::Float, data);
  storage.add("need", AttrDomain::Point, AttrType::Float, data);
  storage.add("more", AttrDomain::Face, AttrType::Float, data);
  storage.add("data", AttrDomain::Edge, AttrType::Float, data);

  /* The same data is shared among 4 attributes (as well as the original `data`). */
  EXPECT_EQ(sharing_info->strong_users(), 5);
  storage.add("final!", AttrDomain::Edge, AttrType::Float, std::move(data));
  EXPECT_EQ(sharing_info->strong_users(), 5);

  {
    const auto &data = std::get<Attribute::ArrayData>(storage.lookup("more")->data_for_write());
    const float *data_ptr = static_cast<const float *>(data.data);
    EXPECT_EQ(data_ptr[0], 1.5f);
    EXPECT_EQ(data_ptr[1], 1.2f);
    EXPECT_EQ(data_ptr[2], 1.1f);
    EXPECT_EQ(data_ptr[3], 1.0f);
  }

  int count = 0;
  storage.foreach([&](const Attribute & /*attribute*/) { count++; });
  EXPECT_EQ(count, 5);
}

TEST(attribute_storage, CopyConstruct)
{
  AttributeStorage storage;

  auto *sharing_info = new ImplicitSharedValue<Array<float>>(Span<float>{1.5f, 1.2f, 1.1f, 1.0f});
  Attribute::ArrayData data{};
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  data.data = sharing_info->data.data();
  data.size = 4;
  storage.add("foo", AttrDomain::Corner, AttrType::Float, std::move(data));

  AttributeStorage copy{storage};

  EXPECT_TRUE(copy.lookup("foo"));
  EXPECT_EQ(copy.lookup("foo")->domain(), AttrDomain::Corner);
  EXPECT_EQ(copy.lookup("foo")->data_type(), AttrType::Float);
  {
    const auto &data = std::get<Attribute::ArrayData>(copy.lookup("foo")->data());
    /* The data is shared, so it should be the same as the original. */
    EXPECT_EQ(data.data, sharing_info->data.data());
  }
}

TEST(attribute_storage, MoveConstruct)
{
  AttributeStorage storage;

  auto *sharing_info = new ImplicitSharedValue<Array<float>>(Span<float>{1.5f, 1.2f, 1.1f, 1.0f});
  Attribute::ArrayData data{};
  data.sharing_info = ImplicitSharingPtr<>(sharing_info);
  data.data = sharing_info->data.data();
  data.size = 4;
  storage.add("foo", AttrDomain::Corner, AttrType::Float, std::move(data));

  AttributeStorage copy{std::move(storage)};

  EXPECT_TRUE(copy.lookup("foo"));
  EXPECT_EQ(copy.lookup("foo")->domain(), AttrDomain::Corner);
  EXPECT_EQ(copy.lookup("foo")->data_type(), AttrType::Float);
  {
    const auto &data = std::get<Attribute::ArrayData>(copy.lookup("foo")->data());
    /* The data is shared, so it should be the same as the original. */
    EXPECT_EQ(data.data, sharing_info->data.data());
  }
}

TEST(attribute_storage, UniqueNames)
{
  AttributeStorage storage;

  auto create_array_data = []() {
    auto *sharing_info = new ImplicitSharedValue<Array<float>>(
        Span<float>{1.5f, 1.2f, 1.1f, 1.0f});
    Attribute::ArrayData data{};
    data.sharing_info = ImplicitSharingPtr<>(sharing_info);
    data.data = sharing_info->data.data();
    data.size = 4;
    return data;
  };

  storage.add("foo", AttrDomain::Corner, AttrType::Float, create_array_data());
  storage.add("foo_2", AttrDomain::Face, AttrType::Float, create_array_data());
  storage.add("foo_3", AttrDomain::Point, AttrType::Float, create_array_data());
  storage.add(
      storage.unique_name_calc("foo"), AttrDomain::Edge, AttrType::Float, create_array_data());
  storage.add(
      storage.unique_name_calc("foo"), AttrDomain::Corner, AttrType::Float, create_array_data());
  storage.add(
      storage.unique_name_calc("foo_2"), AttrDomain::Point, AttrType::Float, create_array_data());

  int count = 0;
  storage.foreach([&](const Attribute & /*attribute*/) { count++; });
  EXPECT_EQ(count, 6);
}

}  // namespace blender::bke::tests

/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_idprop.hh"
#include "testing/testing.h"

namespace blender::bke::tests {

TEST(idproperties, CreateGroup)
{
  IDProperty *prop = idprop::create_group("test").release();
  IDP_FreeProperty(prop);
}

TEST(idproperties, AddToGroup)
{
  IDProperty *group = idprop::create_group("test").release();
  EXPECT_EQ(IDP_GetPropertyFromGroup(group, "a"), nullptr);
  EXPECT_TRUE(IDP_AddToGroup(group, idprop::create("a", 3.0f).release()));
  EXPECT_TRUE(IDP_AddToGroup(group, idprop::create("b", 5).release()));
  EXPECT_EQ(IDP_float_get(IDP_GetPropertyFromGroup(group, "a")), 3.0f);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group, "b")), 5);
  IDProperty *duplicate_prop = idprop::create("a", 10).release();
  EXPECT_FALSE(IDP_AddToGroup(group, duplicate_prop));
  EXPECT_EQ(IDP_float_get(IDP_GetPropertyFromGroup(group, "a")), 3.0f);
  EXPECT_EQ(IDP_GetPropertyFromGroup(group, "c"), nullptr);
  IDP_FreeProperty(duplicate_prop);
  IDP_FreeProperty(group);
}

TEST(idproperties, ReplaceInGroup)
{
  IDProperty *group = idprop::create_group("test").release();
  EXPECT_TRUE(IDP_AddToGroup(group, idprop::create("a", 3.0f).release()));
  EXPECT_EQ(IDP_float_get(IDP_GetPropertyFromGroup(group, "a")), 3.0f);
  IDP_ReplaceInGroup(group, idprop::create("a", 5.0f).release());
  EXPECT_EQ(IDP_float_get(IDP_GetPropertyFromGroup(group, "a")), 5.0f);
  IDP_ReplaceInGroup(group, idprop::create("b", 5).release());
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group, "b")), 5);
  IDP_FreeProperty(group);
}

TEST(idproperties, RemoveFromGroup)
{
  IDProperty *group = idprop::create_group("test").release();
  EXPECT_EQ(IDP_GetPropertyFromGroup(group, "a"), nullptr);
  IDProperty *prop_a = idprop::create("a", 3.0f).release();
  EXPECT_TRUE(IDP_AddToGroup(group, prop_a));
  EXPECT_EQ(IDP_float_get(IDP_GetPropertyFromGroup(group, "a")), 3.0f);
  IDP_RemoveFromGroup(group, prop_a);
  EXPECT_EQ(IDP_GetPropertyFromGroup(group, "a"), nullptr);
  IDP_FreeProperty(prop_a);
  IDP_FreeProperty(group);
}

TEST(idproperties, ReplaceGroupInGroup)
{
  IDProperty *group1 = idprop::create_group("test").release();
  IDP_AddToGroup(group1, idprop::create("a", 1).release());
  IDP_AddToGroup(group1, idprop::create("b", 2).release());
  IDProperty *group2 = idprop::create_group("test2").release();
  IDP_AddToGroup(group2, idprop::create("b", 3).release());
  IDP_AddToGroup(group2, idprop::create("c", 4).release());

  IDP_ReplaceGroupInGroup(group1, group2);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group1, "a")), 1);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group1, "b")), 3);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group1, "c")), 4);

  IDP_FreeProperty(group1);
  IDP_FreeProperty(group2);
}

TEST(idproperties, SyncGroupValues)
{
  IDProperty *group1 = idprop::create_group("test").release();
  IDProperty *group2 = idprop::create_group("test").release();
  IDP_AddToGroup(group1, idprop::create("a", 1).release());
  IDP_AddToGroup(group1, idprop::create("b", 2).release());
  IDP_AddToGroup(group1, idprop::create("x", 2).release());
  IDP_AddToGroup(group2, idprop::create("a", 3).release());
  IDP_AddToGroup(group2, idprop::create("c", 4).release());
  IDP_AddToGroup(group2, idprop::create("x", "value").release());

  IDP_SyncGroupValues(group1, group2);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group1, "a")), 3);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group1, "b")), 2);
  EXPECT_EQ(IDP_int_get(IDP_GetPropertyFromGroup(group1, "x")), 2);
  EXPECT_EQ(IDP_GetPropertyFromGroup(group1, "c"), nullptr);

  IDP_FreeProperty(group1);
  IDP_FreeProperty(group2);
}

TEST(idproperties, ReprGroup)
{
  auto repr_fn = [](IDProperty *prop) -> std::string {
    uint result_len;
    char *c_str = IDP_reprN(prop, &result_len);
    std::string result = std::string(c_str, result_len);
    MEM_freeN(c_str);
    return result;
  };

  IDProperty *group = idprop::create_group("test").release();

  EXPECT_EQ(repr_fn(group), "{}");

  IDP_AddToGroup(group, idprop::create("a", 1).release());
  IDP_AddToGroup(group, idprop::create("b", 0.5f).release());
  IDP_AddToGroup(group, idprop::create_bool("c", true).release());
  IDP_AddToGroup(group, idprop::create_bool("d", false).release());
  IDP_AddToGroup(group, idprop::create("e", "ABC (escape \" \\)").release());
  IDP_AddToGroup(group, idprop::create("f", Span<int32_t>({-1, 0, 1})).release());
  IDP_AddToGroup(group, idprop::create("g", Span<float>({-0.5f, 0.0f, 0.5f})).release());
  IDP_AddToGroup(group, idprop::create_group("h").release());

  EXPECT_EQ(repr_fn(group),
            "{"
            "\"a\": 1, "
            "\"b\": 0.5, "
            "\"c\": True, "
            "\"d\": False, "
            "\"e\": \"ABC (escape \\\" \\\\)\", "
            "\"f\": [-1, 0, 1], "
            "\"g\": [-0.5, 0, 0.5], "
            "\"h\": {}"
            "}");

  IDP_FreeProperty(group);
}

}  // namespace blender::bke::tests

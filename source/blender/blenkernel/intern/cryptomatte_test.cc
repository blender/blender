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
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 */
#include "testing/testing.h"

#include "BKE_cryptomatte.h"
#include "BKE_cryptomatte.hh"
#include "BKE_image.h"

#include "RE_pipeline.h"

#include "MEM_guardedalloc.h"

namespace blender::bke::cryptomatte::tests {

TEST(cryptomatte, meta_data_key)
{
  ASSERT_EQ("cryptomatte/c7dbf5e/key",
            BKE_cryptomatte_meta_data_key("ViewLayer.CryptoMaterial", "key"));
  ASSERT_EQ("cryptomatte/b990b65/𝓴𝓮𝔂",
            BKE_cryptomatte_meta_data_key("𝖚𝖓𝖎𝖈𝖔𝖉𝖊.CryptoMaterial", "𝓴𝓮𝔂"));
}

TEST(cryptomatte, extract_layer_name)
{
  ASSERT_EQ("ViewLayer.CryptoMaterial",
            BKE_cryptomatte_extract_layer_name("ViewLayer.CryptoMaterial00"));
  ASSERT_EQ("𝖚𝖓𝖎𝖈𝖔𝖉𝖊", BKE_cryptomatte_extract_layer_name("𝖚𝖓𝖎𝖈𝖔𝖉𝖊13"));
  ASSERT_EQ("NoTrailingSampleNumber",
            BKE_cryptomatte_extract_layer_name("NoTrailingSampleNumber"));
  ASSERT_EQ("W1thM1dd13Numb3rs", BKE_cryptomatte_extract_layer_name("W1thM1dd13Numb3rs09"));
  ASSERT_EQ("", BKE_cryptomatte_extract_layer_name("0123"));
  ASSERT_EQ("", BKE_cryptomatte_extract_layer_name(""));
}

TEST(cryptomatte, layer)
{
  blender::bke::cryptomatte::CryptomatteLayer layer;
  ASSERT_EQ("{}", layer.manifest());

  layer.add_hash("Object", 123);
  ASSERT_EQ("{\"Object\":\"0000007b\"}", layer.manifest());

  layer.add_hash("Object2", 123245678);
  ASSERT_EQ("{\"Object\":\"0000007b\",\"Object2\":\"0758946e\"}", layer.manifest());
}

TEST(cryptomatte, layer_quoted)
{
  blender::bke::cryptomatte::CryptomatteLayer layer;
  layer.add_hash("\"Object\"", 123);
  ASSERT_EQ("{\"\\\"Object\\\"\":\"0000007b\"}", layer.manifest());
}

static void test_cryptomatte_manifest(std::string expected, std::string manifest)
{
  EXPECT_EQ(expected,
            blender::bke::cryptomatte::CryptomatteLayer::read_from_manifest(manifest)->manifest());
}

TEST(cryptomatte, layer_from_manifest)
{
  test_cryptomatte_manifest("{}", "{}");
  test_cryptomatte_manifest(R"({"Object":"12345678"})", R"({"Object": "12345678"})");
  test_cryptomatte_manifest(R"({"Object":"12345678","Object2":"87654321"})",
                            R"({"Object":"12345678","Object2":"87654321"})");
  test_cryptomatte_manifest(R"({"Object":"12345678","Object2":"87654321"})",
                            R"(  {  "Object"  :  "12345678"  ,  "Object2"  :  "87654321"  }  )");
  test_cryptomatte_manifest(R"({"Object\"01\"":"12345678"})", R"({"Object\"01\"": "12345678"})");
  test_cryptomatte_manifest(
      R"({"Object\"01\"":"12345678","Object":"12345678","Object2":"87654321"})",
      R"({"Object\"01\"":"12345678","Object":"12345678", "Object2":"87654321"})");
}

TEST(cryptomatte, extract_layer_hash_from_metadata_key)
{
  EXPECT_EQ("eb4c67b",
            blender::bke::cryptomatte::CryptomatteStampDataCallbackData::extract_layer_hash(
                "cryptomatte/eb4c67b/conversion"));
  EXPECT_EQ("qwerty",
            blender::bke::cryptomatte::CryptomatteStampDataCallbackData::extract_layer_hash(
                "cryptomatte/qwerty/name"));
  /* Check if undefined behaviors are handled. */
  EXPECT_EQ("",
            blender::bke::cryptomatte::CryptomatteStampDataCallbackData::extract_layer_hash(
                "cryptomatte/name"));
  EXPECT_EQ("",
            blender::bke::cryptomatte::CryptomatteStampDataCallbackData::extract_layer_hash(
                "cryptomatte/"));
}

static void validate_cryptomatte_session_from_stamp_data(void *UNUSED(data),
                                                         const char *propname,
                                                         char *propvalue,
                                                         int UNUSED(len))
{
  blender::StringRefNull prop_name(propname);
  if (!prop_name.startswith("cryptomatte/")) {
    return;
  }

  if (prop_name == "cryptomatte/87f095e/name") {
    EXPECT_STREQ("viewlayername.layer1", propvalue);
  }
  else if (prop_name == "cryptomatte/87f095e/hash") {
    EXPECT_STREQ("MurmurHash3_32", propvalue);
  }
  else if (prop_name == "cryptomatte/87f095e/conversion") {
    EXPECT_STREQ("uint32_to_float32", propvalue);
  }
  else if (prop_name == "cryptomatte/87f095e/manifest") {
    EXPECT_STREQ(R"({"Object":"12345678"})", propvalue);
  }

  else if (prop_name == "cryptomatte/c42daa7/name") {
    EXPECT_STREQ("viewlayername.layer2", propvalue);
  }
  else if (prop_name == "cryptomatte/c42daa7/hash") {
    EXPECT_STREQ("MurmurHash3_32", propvalue);
  }
  else if (prop_name == "cryptomatte/c42daa7/conversion") {
    EXPECT_STREQ("uint32_to_float32", propvalue);
  }
  else if (prop_name == "cryptomatte/c42daa7/manifest") {
    EXPECT_STREQ(R"({"Object2":"87654321"})", propvalue);
  }

  else {
    EXPECT_EQ("Unhandled", std::string(propname) + ": " + propvalue);
  }
}

TEST(cryptomatte, session_from_stamp_data)
{
  /* Create CryptomatteSession from stamp data. */
  RenderResult *render_result = static_cast<RenderResult *>(
      MEM_callocN(sizeof(RenderResult), __func__));
  BKE_render_result_stamp_data(render_result, "cryptomatte/qwerty/name", "layer1");
  BKE_render_result_stamp_data(
      render_result, "cryptomatte/qwerty/manifest", R"({"Object":"12345678"})");
  BKE_render_result_stamp_data(render_result, "cryptomatte/uiop/name", "layer2");
  BKE_render_result_stamp_data(
      render_result, "cryptomatte/uiop/manifest", R"({"Object2":"87654321"})");
  CryptomatteSessionPtr session(BKE_cryptomatte_init_from_render_result(render_result));
  EXPECT_NE(session.get(), nullptr);
  RE_FreeRenderResult(render_result);

  /* Create StampData from CryptomatteSession. */
  ViewLayer view_layer;
  BLI_strncpy(view_layer.name, "viewlayername", sizeof(view_layer.name));
  RenderResult *render_result2 = static_cast<RenderResult *>(
      MEM_callocN(sizeof(RenderResult), __func__));
  BKE_cryptomatte_store_metadata(session.get(), render_result2, &view_layer);

  /* Validate StampData. */
  BKE_stamp_info_callback(
      nullptr, render_result2->stamp_data, validate_cryptomatte_session_from_stamp_data, false);

  RE_FreeRenderResult(render_result2);
}

}  // namespace blender::bke::cryptomatte::tests

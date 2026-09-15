/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BKE_file_handler.hh"
#include "BKE_gtest_base.hh"

#include "BLI_string.hh"

#include "testing/testing.h"

namespace blender::bke::tests {
#define MAX_FILE_HANDLERS_TEST_SIZE 8
static FileHandlerType *test_file_handlers[MAX_FILE_HANDLERS_TEST_SIZE];

static void file_handler_add_test(const int test_number,
                                  const char *idname,
                                  const char *label,
                                  const char *file_extensions_str,
                                  Vector<std::string> expected_file_extensions)
{
  EXPECT_LE(test_number, MAX_FILE_HANDLERS_TEST_SIZE);
  EXPECT_GE(test_number, 1);
  EXPECT_EQ(file_handlers().size(), test_number - 1);

  std::unique_ptr<FileHandlerType> file_handler = std::make_unique<FileHandlerType>();

  test_file_handlers[test_number - 1] = file_handler.get();

  STRNCPY(file_handler->idname, idname);
  STRNCPY(file_handler->file_extensions_str, file_extensions_str);
  STRNCPY(file_handler->label, label);

  file_handler_add(std::move(file_handler));
  EXPECT_EQ(file_handlers().size(), test_number);
  EXPECT_EQ(file_handlers()[test_number - 1].get(), test_file_handlers[test_number - 1]);
  EXPECT_EQ(file_handlers()[test_number - 1]->file_extensions, expected_file_extensions);
}

class FileHandlerTest : public BlenderGTestBase {};

TEST_F(FileHandlerTest, add)
{
  file_handler_add_test(1,
                        "Test_FH_blender1",
                        "File Handler Test 1",
                        ".blender;.blend;.ble",
                        {".blender", ".blend", ".ble"});
  file_handler_add_test(2, "Test_FH_blender2", "File Handler Test 2", ".ble", {".ble"});
  file_handler_add_test(3, "Test_FH_blender3", "File Handler Test 3", ";;.ble", {".ble"});
  file_handler_add_test(4, "Test_FH_blender4", "File Handler Test 4", ";.ble;", {".ble"});
  file_handler_add_test(5, "Test_FH_blender5", "File Handler Test 5", "d", {});
  file_handler_add_test(6, "Test_FH_blender6", "File Handler Test 6", ";;", {});
  file_handler_add_test(7, "Test_FH_blender7", "File Handler Test 7", ".", {});
  file_handler_add_test(8, "Test_FH_blender8", "File Handler Test 8", "", {});
}

TEST_F(FileHandlerTest, find)
{
  EXPECT_EQ(file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE);
  EXPECT_EQ(file_handler_find("Test_FH_blender1"), test_file_handlers[0]);
  EXPECT_EQ(file_handler_find("Test_FH_blender2"), test_file_handlers[1]);
  EXPECT_EQ(file_handler_find("Test_FH_blender3"), test_file_handlers[2]);
  EXPECT_EQ(file_handler_find("Test_FH_blender4"), test_file_handlers[3]);
  EXPECT_EQ(file_handler_find("Test_FH_blender5"), test_file_handlers[4]);
  EXPECT_EQ(file_handler_find("Test_FH_blender6"), test_file_handlers[5]);
  EXPECT_EQ(file_handler_find("Test_FH_blender7"), test_file_handlers[6]);
  EXPECT_EQ(file_handler_find("Test_FH_blender8"), test_file_handlers[7]);
  EXPECT_EQ(file_handler_find("Test_FH_blende"), nullptr);
  EXPECT_EQ(file_handler_find("TstFH_blen"), nullptr);
}

TEST_F(FileHandlerTest, remove)
{
  EXPECT_EQ(file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE);

  file_handler_remove(file_handler_find("Test_FH_blender2"));

  EXPECT_EQ(file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE - 1);
  EXPECT_EQ(file_handler_find("Test_FH_blender2"), nullptr);

  /** `FileHandlerType` pointer in `test_file_handlers[1]` is not longer valid. */
  EXPECT_EQ(file_handler_find("Test_FH_blender1"), test_file_handlers[0]);
  EXPECT_EQ(file_handler_find("Test_FH_blender3"), test_file_handlers[2]);
  EXPECT_EQ(file_handler_find("Test_FH_blender4"), test_file_handlers[3]);
  EXPECT_EQ(file_handler_find("Test_FH_blender5"), test_file_handlers[4]);
  EXPECT_EQ(file_handler_find("Test_FH_blender6"), test_file_handlers[5]);
  EXPECT_EQ(file_handler_find("Test_FH_blender7"), test_file_handlers[6]);
  EXPECT_EQ(file_handler_find("Test_FH_blender8"), test_file_handlers[7]);

  EXPECT_EQ(file_handlers()[0].get(), test_file_handlers[0]);
  EXPECT_EQ(file_handlers()[1].get(), test_file_handlers[2]);
  EXPECT_EQ(file_handlers()[2].get(), test_file_handlers[3]);
  EXPECT_EQ(file_handlers()[3].get(), test_file_handlers[4]);
  EXPECT_EQ(file_handlers()[4].get(), test_file_handlers[5]);
  EXPECT_EQ(file_handlers()[5].get(), test_file_handlers[6]);
  EXPECT_EQ(file_handlers()[6].get(), test_file_handlers[7]);

  file_handler_remove(file_handler_find("Test_FH_blender8"));

  EXPECT_EQ(file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE - 2);
  EXPECT_EQ(file_handler_find("Test_FH_blender8"), nullptr);

  /** `FileHandlerType` pointer in `test_file_handlers[7]` is not longer valid. */
  EXPECT_EQ(file_handler_find("Test_FH_blender1"), test_file_handlers[0]);
  EXPECT_EQ(file_handler_find("Test_FH_blender3"), test_file_handlers[2]);
  EXPECT_EQ(file_handler_find("Test_FH_blender4"), test_file_handlers[3]);
  EXPECT_EQ(file_handler_find("Test_FH_blender5"), test_file_handlers[4]);
  EXPECT_EQ(file_handler_find("Test_FH_blender6"), test_file_handlers[5]);
  EXPECT_EQ(file_handler_find("Test_FH_blender7"), test_file_handlers[6]);

  EXPECT_EQ(file_handlers()[0].get(), test_file_handlers[0]);
  EXPECT_EQ(file_handlers()[1].get(), test_file_handlers[2]);
  EXPECT_EQ(file_handlers()[2].get(), test_file_handlers[3]);
  EXPECT_EQ(file_handlers()[3].get(), test_file_handlers[4]);
  EXPECT_EQ(file_handlers()[4].get(), test_file_handlers[5]);
  EXPECT_EQ(file_handlers()[5].get(), test_file_handlers[6]);
}

static void expect_label_with_extensions(const char *label,
                                         Vector<std::string> file_extensions,
                                         const std::string &expected)
{
  FileHandlerType file_handler{};
  STRNCPY(file_handler.label, label);
  file_handler.file_extensions = std::move(file_extensions);
  EXPECT_EQ(file_handler.label_with_extensions(), expected);
}

TEST_F(FileHandlerTest, label_with_extensions)
{
  /* No extensions: label is returned unchanged. */
  expect_label_with_extensions("Wavefront OBJ", {}, "Wavefront OBJ");

  /* Single extension. */
  expect_label_with_extensions("Wavefront OBJ", {".obj"}, "Wavefront OBJ (.obj)");

  /* No common prefix: joined with '/'. */
  expect_label_with_extensions("Collada", {".dae", ".zae"}, "Collada (.dae/.zae)");

  /* Common prefix ".ab" has only 2 letters excluding the leading dot: below the 3-letter
   * threshold required to collapse. */
  expect_label_with_extensions("Prefix Too Short", {".ab", ".abc"}, "Prefix Too Short (.ab/.abc)");

  /* Common prefix ".ble" has enough letters to collapse, but ".blend" is longer than
   * `prefix.size() + 1`, so the extensions are not collapsible. */
  expect_label_with_extensions(
      "Extension Too Long", {".ble", ".blend"}, "Extension Too Long (.ble/.blend)");

  /* Common prefix has enough letters and no extension exceeds the length limit:
   * collapses to `<prefix>*`. */
  expect_label_with_extensions("Universal Scene Description",
                               {".usd", ".usda", ".usdc"},
                               "Universal Scene Description (.usd*)");

  /* Common prefix ".abc" has exactly 3 letters excluding the leading dot: the minimum
   * required to collapse. */
  expect_label_with_extensions(
      "Prefix Length Boundary", {".abc", ".abcd"}, "Prefix Length Boundary (.abc*)");

  /* ".foo" and ".fooz" form a collapsible group, ".bar" does not share a prefix with
   * either and forms its own group. */
  expect_label_with_extensions(
      "Multiple Groups", {".foo", ".fooz", ".bar"}, "Multiple Groups (.foo*/.bar)");
}
}  // namespace blender::bke::tests

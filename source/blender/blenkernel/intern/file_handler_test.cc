/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "BKE_file_handler.hh"
#include "testing/testing.h"

namespace blender::tests {
#define MAX_FILE_HANDLERS_TEST_SIZE 8
static FileHandlerType *file_handlers[MAX_FILE_HANDLERS_TEST_SIZE];

static void file_handler_add_test(const int test_number,
                                  const char *idname,
                                  const char *label,
                                  const char *file_extensions_str,
                                  blender::Vector<std::string> expected_file_extensions)
{
  EXPECT_LE(test_number, MAX_FILE_HANDLERS_TEST_SIZE);
  EXPECT_GE(test_number, 1);
  EXPECT_EQ(BKE_file_handlers().size(), test_number - 1);

  std::unique_ptr<FileHandlerType> file_handler = std::make_unique<FileHandlerType>();

  file_handlers[test_number - 1] = file_handler.get();

  strcpy(file_handler->idname, idname);
  strcpy(file_handler->file_extensions_str, file_extensions_str);
  strcpy(file_handler->label, label);

  BKE_file_handler_add(std::move(file_handler));
  EXPECT_EQ(BKE_file_handlers().size(), test_number);
  EXPECT_EQ(BKE_file_handlers()[test_number - 1].get(), file_handlers[test_number - 1]);
  EXPECT_EQ(BKE_file_handlers()[test_number - 1]->file_extensions, expected_file_extensions);
}

TEST(file_handler, add)
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

TEST(file_handler, find)
{
  EXPECT_EQ(BKE_file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender1"), file_handlers[0]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender2"), file_handlers[1]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender3"), file_handlers[2]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender4"), file_handlers[3]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender5"), file_handlers[4]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender6"), file_handlers[5]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender7"), file_handlers[6]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender8"), file_handlers[7]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blende"), nullptr);
  EXPECT_EQ(BKE_file_handler_find("TstFH_blen"), nullptr);
}

TEST(file_handler, remove)
{
  EXPECT_EQ(BKE_file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE);

  BKE_file_handler_remove(BKE_file_handler_find("Test_FH_blender2"));

  EXPECT_EQ(BKE_file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE - 1);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender2"), nullptr);

  /** `FileHandlerType` pointer in `file_handlers[1]` is not longer valid. */
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender1"), file_handlers[0]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender3"), file_handlers[2]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender4"), file_handlers[3]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender5"), file_handlers[4]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender6"), file_handlers[5]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender7"), file_handlers[6]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender8"), file_handlers[7]);

  EXPECT_EQ(BKE_file_handlers()[0].get(), file_handlers[0]);
  EXPECT_EQ(BKE_file_handlers()[1].get(), file_handlers[2]);
  EXPECT_EQ(BKE_file_handlers()[2].get(), file_handlers[3]);
  EXPECT_EQ(BKE_file_handlers()[3].get(), file_handlers[4]);
  EXPECT_EQ(BKE_file_handlers()[4].get(), file_handlers[5]);
  EXPECT_EQ(BKE_file_handlers()[5].get(), file_handlers[6]);
  EXPECT_EQ(BKE_file_handlers()[6].get(), file_handlers[7]);

  BKE_file_handler_remove(BKE_file_handler_find("Test_FH_blender8"));

  EXPECT_EQ(BKE_file_handlers().size(), MAX_FILE_HANDLERS_TEST_SIZE - 2);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender8"), nullptr);

  /** `FileHandlerType` pointer  in `file_handlers[7]` is not longer valid. */
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender1"), file_handlers[0]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender3"), file_handlers[2]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender4"), file_handlers[3]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender5"), file_handlers[4]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender6"), file_handlers[5]);
  EXPECT_EQ(BKE_file_handler_find("Test_FH_blender7"), file_handlers[6]);

  EXPECT_EQ(BKE_file_handlers()[0].get(), file_handlers[0]);
  EXPECT_EQ(BKE_file_handlers()[1].get(), file_handlers[2]);
  EXPECT_EQ(BKE_file_handlers()[2].get(), file_handlers[3]);
  EXPECT_EQ(BKE_file_handlers()[3].get(), file_handlers[4]);
  EXPECT_EQ(BKE_file_handlers()[4].get(), file_handlers[5]);
  EXPECT_EQ(BKE_file_handlers()[5].get(), file_handlers[6]);
}
}  // namespace blender::tests

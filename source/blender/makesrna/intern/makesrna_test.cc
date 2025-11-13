/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string_ref.hh"

#include "makesrna_utils.hh"

#include "testing/testing.h"

namespace blender::rna::tests {

TEST(makesrna_test, forward_struct_declarations_empty_set)
{
  std::ostringstream stream;
  rna_write_struct_forward_declarations(stream, {});
  EXPECT_EQ(stream.str(), blender::StringRef(""));
}

TEST(makesrna_test, forward_struct_declarations_unscoped_struct_set)
{
  std::ostringstream stream;
  blender::Vector<blender::StringRef> test_structs = {
      "bContext2",
      "PointerRNA2",
      "BContext",
      "POINTERRNA1",
      "pointerrna3",
      "Object",
      "Scene",
      "Addons",
      "bAddon",
  };
  rna_write_struct_forward_declarations(stream, std::move(test_structs));
  const char *expected_stream =
      "struct Addons;\n"
      "struct bAddon;\n"
      "struct BContext;\n"
      "struct bContext2;\n"
      "struct Object;\n"
      "struct POINTERRNA1;\n"
      "struct PointerRNA2;\n"
      "struct pointerrna3;\n"
      "struct Scene;\n";
  EXPECT_EQ(stream.str(), blender::StringRef(expected_stream));
}

TEST(makesrna_test, forward_struct_declarations_scoped_struct_set)
{
  std::ostringstream stream;
  blender::Vector<blender::StringRef> test_structs = {
      "bContext2",
      "PointerRNA2",
      "BContext",
      "POINTERRNA1",
      "pointerrna3",
      "Object",
      "blender::UI::FooStruct",
      "Scene",
      "Addons",
      "bAddon",
      "blender::Vector",
      "blender::Map",
      "blender::ui::Layout",
      "blender::ui::PieLayout",
  };
  rna_write_struct_forward_declarations(stream, test_structs);
  const char *expected_stream =
      "struct Addons;\n"
      "struct bAddon;\n"
      "struct BContext;\n"
      "struct bContext2;\n"
      "struct Object;\n"
      "struct POINTERRNA1;\n"
      "struct PointerRNA2;\n"
      "struct pointerrna3;\n"
      "struct Scene;\n"

      "namespace blender {\n"
      "struct Map;\n"
      "struct Vector;\n"
      "}; // namespace blender\n"

      "namespace blender::UI {\n"
      "struct FooStruct;\n"
      "}; // namespace blender::UI\n"

      "namespace blender::ui {\n"
      "struct Layout;\n"
      "struct PieLayout;\n"
      "}; // namespace blender::ui\n";

  EXPECT_EQ(stream.str(), expected_stream);
}
}  // namespace blender::rna::tests

/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BKE_path_templates.hh"

#include "testing/testing.h"

namespace blender::bke::tests {

using namespace blender::bke::path_templates;

static std::string error_to_string(const Error &error)
{
  const char *type;
  switch (error.type) {
    case ErrorType::UNESCAPED_CURLY_BRACE:
      type = "UNESCAPED_CURLY_BRACE";
      break;
    case ErrorType::VARIABLE_SYNTAX:
      type = "VARIABLE_SYNTAX";
      break;
    case ErrorType::FORMAT_SPECIFIER:
      type = "FORMAT_SPECIFIER";
      break;
    case ErrorType::UNKNOWN_VARIABLE:
      type = "UNKNOWN_VARIABLE";
      break;
  }

  std::string s;
  fmt::format_to(std::back_inserter(s),
                 "({}, ({}, {}))",
                 type,
                 error.byte_range.start(),
                 error.byte_range.size());

  return s;
}

static std::string errors_to_string(Span<Error> errors)
{
  std::string s;

  fmt::format_to(std::back_inserter(s), "[");
  bool is_first = true;
  for (const Error &error : errors) {
    if (is_first) {
      is_first = false;
    }
    else {
      fmt::format_to(std::back_inserter(s), ", ");
    }
    fmt::format_to(std::back_inserter(s), "{}", error_to_string(error));
  }
  fmt::format_to(std::back_inserter(s), "]");

  return s;
}

TEST(path_templates, VariableMap)
{
  VariableMap map;

  /* With in empty variable map, these should all return false / fail. */
  EXPECT_FALSE(map.contains("hello"));
  EXPECT_FALSE(map.remove("hello"));
  EXPECT_EQ(std::nullopt, map.get_string("hello"));
  EXPECT_EQ(std::nullopt, map.get_filepath("hello"));
  EXPECT_EQ(std::nullopt, map.get_integer("hello"));
  EXPECT_EQ(std::nullopt, map.get_float("hello"));

  /* Populate the map. */
  EXPECT_TRUE(map.add_string("hello", "What a wonderful world."));
  EXPECT_TRUE(map.add_filepath("where", "/my/path"));
  EXPECT_TRUE(map.add_integer("bye", 42));
  EXPECT_TRUE(map.add_float("what", 3.14159));

  /* Attempting to add variables with those names again should fail, since they
   * already exist now. */
  EXPECT_FALSE(map.add_string("hello", "Sup."));
  EXPECT_FALSE(map.add_string("where", "Sup."));
  EXPECT_FALSE(map.add_string("bye", "Sup."));
  EXPECT_FALSE(map.add_string("what", "Sup."));
  EXPECT_FALSE(map.add_filepath("hello", "/place"));
  EXPECT_FALSE(map.add_filepath("where", "/place"));
  EXPECT_FALSE(map.add_filepath("bye", "/place"));
  EXPECT_FALSE(map.add_filepath("what", "/place"));
  EXPECT_FALSE(map.add_integer("hello", 2));
  EXPECT_FALSE(map.add_integer("where", 2));
  EXPECT_FALSE(map.add_integer("bye", 2));
  EXPECT_FALSE(map.add_integer("what", 2));
  EXPECT_FALSE(map.add_float("hello", 2.71828));
  EXPECT_FALSE(map.add_float("where", 2.71828));
  EXPECT_FALSE(map.add_float("bye", 2.71828));
  EXPECT_FALSE(map.add_float("what", 2.71828));

  /* Confirm that the right variables exist. */
  EXPECT_TRUE(map.contains("hello"));
  EXPECT_TRUE(map.contains("where"));
  EXPECT_TRUE(map.contains("bye"));
  EXPECT_TRUE(map.contains("what"));
  EXPECT_FALSE(map.contains("not here"));

  /* Fetch the variables we added. */
  EXPECT_EQ("What a wonderful world.", map.get_string("hello"));
  EXPECT_EQ("/my/path", map.get_filepath("where"));
  EXPECT_EQ(42, map.get_integer("bye"));
  EXPECT_EQ(3.14159, map.get_float("what"));

  /* The same variables shouldn't exist for the other types, despite our attempt
   * to add them earlier. */
  EXPECT_EQ(std::nullopt, map.get_filepath("hello"));
  EXPECT_EQ(std::nullopt, map.get_integer("hello"));
  EXPECT_EQ(std::nullopt, map.get_float("hello"));
  EXPECT_EQ(std::nullopt, map.get_string("where"));
  EXPECT_EQ(std::nullopt, map.get_integer("where"));
  EXPECT_EQ(std::nullopt, map.get_float("where"));
  EXPECT_EQ(std::nullopt, map.get_string("bye"));
  EXPECT_EQ(std::nullopt, map.get_filepath("bye"));
  EXPECT_EQ(std::nullopt, map.get_float("bye"));
  EXPECT_EQ(std::nullopt, map.get_string("what"));
  EXPECT_EQ(std::nullopt, map.get_filepath("what"));
  EXPECT_EQ(std::nullopt, map.get_integer("what"));

  /* Remove the variables. */
  EXPECT_TRUE(map.remove("hello"));
  EXPECT_TRUE(map.remove("where"));
  EXPECT_TRUE(map.remove("bye"));
  EXPECT_TRUE(map.remove("what"));

  /* The variables shouldn't exist anymore. */
  EXPECT_FALSE(map.contains("hello"));
  EXPECT_FALSE(map.contains("where"));
  EXPECT_FALSE(map.contains("bye"));
  EXPECT_FALSE(map.contains("what"));
  EXPECT_EQ(std::nullopt, map.get_string("hello"));
  EXPECT_EQ(std::nullopt, map.get_filepath("where"));
  EXPECT_EQ(std::nullopt, map.get_integer("bye"));
  EXPECT_EQ(std::nullopt, map.get_float("what"));
  EXPECT_FALSE(map.remove("hello"));
  EXPECT_FALSE(map.remove("where"));
  EXPECT_FALSE(map.remove("bye"));
  EXPECT_FALSE(map.remove("what"));
}

TEST(path_templates, VariableMap_add_filename_only)
{
  VariableMap map;

  EXPECT_TRUE(map.add_filename_only("a", "/home/bob/project_joe/scene_3.blend", "fallback"));
  EXPECT_EQ("scene_3", map.get_filepath("a"));

  EXPECT_TRUE(map.add_filename_only("b", "/home/bob/project_joe/scene_3", "fallback"));
  EXPECT_EQ("scene_3", map.get_filepath("b"));

  EXPECT_TRUE(map.add_filename_only("c", "/home/bob/project_joe/scene.03.blend", "fallback"));
  EXPECT_EQ("scene.03", map.get_filepath("c"));

  EXPECT_TRUE(map.add_filename_only("d", "/home/bob/project_joe/.scene_3.blend", "fallback"));
  EXPECT_EQ(".scene_3", map.get_filepath("d"));

  EXPECT_TRUE(map.add_filename_only("e", "/home/bob/project_joe/.scene_3", "fallback"));
  EXPECT_EQ(".scene_3", map.get_filepath("e"));

  EXPECT_TRUE(map.add_filename_only("f", "scene_3.blend", "fallback"));
  EXPECT_EQ("scene_3", map.get_filepath("f"));

  EXPECT_TRUE(map.add_filename_only("g", "scene_3", "fallback"));
  EXPECT_EQ("scene_3", map.get_filepath("g"));

  /* No filename in path (ending slash means it's a directory). */
  EXPECT_TRUE(map.add_filename_only("h", "/home/bob/project_joe/", "fallback"));
  EXPECT_EQ("fallback", map.get_filepath("h"));

  /* Empty path. */
  EXPECT_TRUE(map.add_filename_only("i", "", "fallback"));
  EXPECT_EQ("fallback", map.get_filepath("i"));

  /* Attempt to add already-added variable. */
  EXPECT_FALSE(map.add_filename_only("i", "", "fallback"));
}

TEST(path_templates, VariableMap_add_path_up_to_file)
{
  VariableMap map;

  EXPECT_TRUE(map.add_path_up_to_file("a", "/home/bob/project_joe/scene_3.blend", "fallback"));
  EXPECT_EQ("/home/bob/project_joe/", map.get_filepath("a"));

  EXPECT_TRUE(map.add_path_up_to_file("b", "project_joe/scene_3.blend", "fallback"));
  EXPECT_EQ("project_joe/", map.get_filepath("b"));

  EXPECT_TRUE(map.add_path_up_to_file("c", "/scene_3.blend", "fallback"));
  EXPECT_EQ("/", map.get_filepath("c"));

  /* No filename in path (ending slash means it's a directory). */
  EXPECT_TRUE(map.add_path_up_to_file("e", "/home/bob/project_joe/", "fallback"));
  EXPECT_EQ("/home/bob/project_joe/", map.get_filepath("e"));

  EXPECT_TRUE(map.add_path_up_to_file("f", "/", "fallback"));
  EXPECT_EQ("/", map.get_filepath("f"));

  /* No leading path. */
  EXPECT_TRUE(map.add_path_up_to_file("d", "scene_3.blend", "fallback"));
  EXPECT_EQ("fallback", map.get_filepath("d"));

  /* Empty path. */
  EXPECT_TRUE(map.add_path_up_to_file("g", "", "fallback"));
  EXPECT_EQ("fallback", map.get_filepath("g"));

  /* Attempt to add already-added variable. */
  EXPECT_FALSE(map.add_filename_only("g", "", "fallback"));
}

struct PathTemplateTestCase {
  char path_in[FILE_MAX];
  char path_result[FILE_MAX];
  Vector<Error> expected_errors;
};

TEST(path_templates, validate_and_apply_template)
{
  VariableMap variables;
  {
    variables.add_string("hi", "hello");
    variables.add_string("bye", "goodbye");
    variables.add_string("empty", "");
    variables.add_string("sanitize", "./\\?*:|\"<>");
    variables.add_string("long", "This string is exactly 32 bytes_");
    variables.add_filepath("path", "C:\\and/or/../nor/");
    variables.add_integer("the_answer", 42);
    variables.add_integer("prime", 7);
    variables.add_integer("i_negative", -7);
    variables.add_float("pi", 3.14159265358979323846);
    variables.add_float("e", 2.71828182845904523536);
    variables.add_float("ntsc", 30.0 / 1.001);
    variables.add_float("two", 2.0);
    variables.add_float("f_negative", -3.14159265358979323846);
    variables.add_float("huge", 200000000000000000000000000000000.0);
    variables.add_float("tiny", 0.000000000000000000000000000000002);
  }

  const Vector<PathTemplateTestCase> test_cases = {
      /* Simple case, testing all variables. */
      {
          "{hi}_{bye}_{empty}_{sanitize}_{path}"
          "_{the_answer}_{prime}_{i_negative}_{pi}_{e}_{ntsc}_{two}"
          "_{f_negative}_{huge}_{tiny}",
          "hello_goodbye__.__________C:\\and/or/../nor/"
          "_42_7_-7_3.141592653589793_2.718281828459045_29.970029970029973_2.0"
          "_-3.141592653589793_2e+32_2e-33",
          {},
      },

      /* Integer formatting. */
      {
          "{the_answer:#}_{the_answer:##}_{the_answer:####}_{i_negative:####}",
          "42_42_0042_-007",
          {},
      },

      /* Integer formatting as float. */
      {
          "{the_answer:.###}_{the_answer:#.##}_{the_answer:###.##}_{i_negative:###.####}",
          "42.000_42.00_042.00_-07.0000",
          {},
      },

      /* Float formatting: specify fractional digits only. */
      {
          "{pi:.####}_{e:.###}_{ntsc:.########}_{two:.##}_{f_negative:.##}_{huge:.##}_{tiny:.##}",
          "3.1416_2.718_29.97002997_2.00_-3.14_200000000000000010732324408786944.00_0.00",
          {},
      },

      /* Float formatting: specify both integer and fractional digits. */
      {
          "{pi:##.####}_{e:####.###}_{ntsc:#.########}_{two:###.##}_{f_negative:###.##}_{huge:###."
          "##}_{tiny:###.##}",
          "03.1416_0002.718_29.97002997_002.00_-03.14_200000000000000010732324408786944.00_000.00",
          {},
      },

      /* Float formatting: format as integer. */
      {
          "{pi:##}_{e:####}_{ntsc:#}_{two:###}",
          "03_0003_30_002",
          {},
      },

      /* Escaping. "{{" and "}}" are the escape codes for literal "{" and "}". */
      {
          "{hi}_{{hi}}_{{{bye}}}_{bye}",
          "hello_{hi}_{goodbye}_goodbye",
          {},
      },

      /* Error: string variables do not support format specifiers. */
      {
          "{hi:##}_{bye:#}",
          "{hi:##}_{bye:#}",
          {
              {ErrorType::FORMAT_SPECIFIER, IndexRange(0, 7)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(8, 7)},
          },
      },

      /* Error: float formatting: specifying integer digits only (but still wanting
       * it printed as a float) is currently not supported. */
      {
          "{pi:##.}_{e:####.}_{ntsc:#.}_{two:###.}_{f_negative:###.}_{huge:###.}_{tiny:###.}",
          "{pi:##.}_{e:####.}_{ntsc:#.}_{two:###.}_{f_negative:###.}_{huge:###.}_{tiny:###.}",
          {
              {ErrorType::FORMAT_SPECIFIER, IndexRange(0, 8)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(9, 9)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(19, 9)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(29, 10)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(40, 17)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(58, 11)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(70, 11)},
          },
      },

      /* Error: missing variable. */
      {
          "{hi}_{missing}_{bye}",
          "{hi}_{missing}_{bye}",
          {
              {ErrorType::UNKNOWN_VARIABLE, IndexRange(5, 9)},
          },
      },

      /* Error: incomplete variable expression. */
      {
          "foo{hi",
          "foo{hi",
          {
              {ErrorType::VARIABLE_SYNTAX, IndexRange(3, 3)},
          },
      },

      /* Error: incomplete variable expression after complete one. */
      {
          "foo{bye}{hi",
          "foo{bye}{hi",
          {
              {ErrorType::VARIABLE_SYNTAX, IndexRange(8, 3)},
          },
      },

      /* Error: invalid format specifiers. */
      {
          "{prime:}_{prime:.}_{prime:#.#.#}_{prime:sup}_{prime::sup}_{prime}",
          "{prime:}_{prime:.}_{prime:#.#.#}_{prime:sup}_{prime::sup}_{prime}",
          {
              {ErrorType::FORMAT_SPECIFIER, IndexRange(0, 8)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(9, 9)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(19, 13)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(33, 11)},
              {ErrorType::FORMAT_SPECIFIER, IndexRange(45, 12)},
          },
      },

      /* Error: unclosed variable. */
      {
          "{hi_{hi}_{bye}",
          "{hi_{hi}_{bye}",
          {
              {ErrorType::VARIABLE_SYNTAX, IndexRange(0, 4)},
          },
      },

      /* Error: escaped braces inside variable. */
      {
          "{hi_{{hi}}_{bye}",
          "{hi_{{hi}}_{bye}",
          {
              {ErrorType::VARIABLE_SYNTAX, IndexRange(0, 4)},
          },
      },

      /* Test what happens when the path would expand to a string that's longer than
       * `FILE_MAX`.
       *
       * We don't care so much about any kind of "correctness" here, we just want to
       * ensure that it still results in a valid null-terminated string that fits in
       * `FILE_MAX` bytes.
       *
       * NOTE: this test will have to be updated if `FILE_MAX` is ever changed. */
      {
          "___{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}"
          "{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{long}{"
          "long}{long}",

          "___This string is exactly 32 bytes_This string is exactly 32 bytes_This string is "
          "exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This "
          "string is exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 "
          "bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This string is "
          "exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This "
          "string is exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 "
          "bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This string is "
          "exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This "
          "string is exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 "
          "bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This string is "
          "exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 bytes_This "
          "string is exactly 32 bytes_This string is exactly 32 bytes_This string is exactly 32 "
          "by",

          {},
      },
  };

  for (const PathTemplateTestCase &test_case : test_cases) {
    char path[FILE_MAX];
    STRNCPY(path, test_case.path_in);

    /* Do validation first, which shouldn't modify the path. */
    const Vector<Error> validation_errors = BKE_path_validate_template(path, variables);
    EXPECT_EQ(validation_errors, test_case.expected_errors)
        << "  Template errors: " << errors_to_string(validation_errors) << std::endl
        << "  Expected errors: " << errors_to_string(test_case.expected_errors) << std::endl
        << "  Note: test_case.path_in = " << test_case.path_in << std::endl;
    EXPECT_EQ(blender::StringRef(path), test_case.path_in)
        << "  Note: test_case.path_in = " << test_case.path_in << std::endl;

    /* Then do application, which should modify the path. */
    const Vector<Error> application_errors = BKE_path_apply_template(path, FILE_MAX, variables);
    EXPECT_EQ(application_errors, test_case.expected_errors)
        << "  Template errors: " << errors_to_string(application_errors) << std::endl
        << "  Expected errors: " << errors_to_string(test_case.expected_errors) << std::endl
        << "  Note: test_case.path_in = " << test_case.path_in << std::endl;
    EXPECT_EQ(blender::StringRef(path), test_case.path_result)
        << "  Note: test_case.path_in = " << test_case.path_in << std::endl;
  }
}

}  // namespace blender::bke::tests
